// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
/* output_gstreamer.c - Output module for GStreamer
 *
 * Copyright (C) 2005-2007   Ivo Clarysse
 *
 * Adapted to gstreamer-0.10 2006 David Siorpaes
 * Adapted to output to snapcast 2017 Daniel Jäcksch
 *
 * This file is part of GMediaRender.
 *
 * GMediaRender is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GMediaRender is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GMediaRender; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <gst/gst.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "logging.h"
#include "output_gstreamer.h"
#include "output_module.h"
#include "upnp_connmgr.h"

static double buffer_duration = 0.0; /* Buffer disbled by default, see #182 */

static void scan_caps(const GstCaps *caps) {
  guint i;

  g_return_if_fail(caps != NULL);

  if (gst_caps_is_any(caps)) {
    return;
  }
  if (gst_caps_is_empty(caps)) {
    return;
  }

  for (i = 0; i < gst_caps_get_size(caps); i++) {
    GstStructure *structure = gst_caps_get_structure(caps, i);
    const char *mime_type = gst_structure_get_name(structure);
    register_mime_type(mime_type);
  }
  // There seem to be all kinds of mime types out there that start with
  // "audio/" but are not explicitly supported by gstreamer. Let's just
  // tell the controller that we can handle everything "audio/*" and hope
  // for the best.
  register_mime_type("audio/*");
}

static void scan_pad_templates_info(GstElement *element,
                                    GstElementFactory *factory) {
  (void)factory;

  const GList *pads;
  GstPadTemplate *padtemplate;
  GstElementClass *element_class;

  element_class = GST_ELEMENT_GET_CLASS(element);

  if (!element_class->numpadtemplates) {
    return;
  }

  pads = element_class->padtemplates;
  while (pads) {
    padtemplate = (GstPadTemplate *)(pads->data);
    // GstPad *pad = (GstPad *) (pads->data);
    pads = g_list_next(pads);

    if ((padtemplate->direction == GST_PAD_SINK) &&
        ((padtemplate->presence == GST_PAD_ALWAYS) ||
         (padtemplate->presence == GST_PAD_SOMETIMES) ||
         (padtemplate->presence == GST_PAD_REQUEST)) &&
        (padtemplate->caps)) {
      scan_caps(padtemplate->caps);
    }
  }
}

static void scan_mime_list(void) {
  GstRegistry *registry = NULL;
  GList *plugins = NULL;

#if (GST_VERSION_MAJOR < 1)
  registry = gst_registry_get_default();
  plugins = gst_default_registry_get_plugin_list();
#else
  registry = gst_registry_get();
  plugins = gst_registry_get_plugin_list(registry);
#endif

  while (plugins) {
    GList *features;
    GstPlugin *plugin;

    plugin = (GstPlugin *)(plugins->data);
    plugins = g_list_next(plugins);

    features = gst_registry_get_feature_list_by_plugin(
        registry, gst_plugin_get_name(plugin));

    while (features) {
      GstPluginFeature *feature;

      feature = GST_PLUGIN_FEATURE(features->data);

      if (GST_IS_ELEMENT_FACTORY(feature)) {
        GstElementFactory *factory;
        GstElement *element;
        factory = GST_ELEMENT_FACTORY(feature);
        element = gst_element_factory_create(factory, NULL);
        if (element) {
          scan_pad_templates_info(element, factory);
        }
      }

      features = g_list_next(features);
    }
  }
}

static GstElement *player_ = NULL;
static char *gsuri_ = NULL;        // locally strdup()ed
static char *gs_next_uri_ = NULL;  // locally strdup()ed
static struct SongMetaData song_meta_;

static output_transition_cb_t play_trans_callback_ = NULL;
static output_update_meta_cb_t meta_update_callback_ = NULL;

struct track_time_info {
  gint64 duration;
  gint64 position;
};
static struct track_time_info last_known_time_ = {0, 0};


/**
  @brief  Initialize the output module

  @param  none
  @retval result_t
*/
OutputModule::result_t GstreamerOutput::initalize(void)
{
  // TODO Tucker
  //SongMetaData_init(&song_meta_);
  //scan_mime_list();

  // TODO Tucker feed in as arguments, constructor?
  if (audio_sink != NULL && audio_pipe != NULL) 
  {
    Log_error("gstreamer", "--gstout-audosink and --gstout-audiopipe are mutually exclusive.");
    return OutputModule::Error;
  }

#if (GST_VERSION_MAJOR < 1)
  const char player_element_name[] = "playbin2";
#else
  const char player_element_name[] = "playbin";
#endif

  this->player = gst_element_factory_make(player_element_name, "play");
  if (this->player == NULL)
    return OutputModule::Error; // TODO Tucker assert?

  // Configure buffering if enabled
  if (buffer_duration > 0) 
  {
    int64_t buffer_duration_ns = round(buffer_duration * 1.0e9);
    Log_info("gstreamer", "Setting buffer duration to %ldms", buffer_duration_ns / 1000000);
    g_object_set(G_OBJECT(this->player), "buffer-duration", (gint64) buffer_duration_ns, NULL);
  } 
  else 
  {
    Log_info("gstreamer", "Buffering disabled (--gstout-buffer-duration)");
  }

  GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(this->player));
  gst_bus_add_watch(bus, my_bus_callback, NULL); // TODO Tucker rename callback for bus
  gst_object_unref(bus);

  GstElement* sink = NULL;
  if (audio_sink)
  {
    Log_info("gstreamer", "Setting audio sink to %s; device=%s\n", audio_sink, audio_device ? audio_device : "");
   
    sink = gst_element_factory_make(audio_sink, "sink");
    if (sink == NULL)
      Log_error("gstreamer", "Couldn't create sink '%s'", audio_sink);
  }
  else if (audio_pipe) 
  {
    Log_info("gstreamer", "Setting audio sink-pipeline to %s\n", audio_pipe);
   
    sink = gst_parse_bin_from_description(audio_pipe, TRUE, NULL);
    if (sink == NULL)
      Log_error("gstreamer", "Could not create pipeline.");
  }

  if (sink != NULL)
  {
    // Add the audio device if it exists
    if (audio_device != NULL) {
      g_object_set(G_OBJECT(sink), "device", audio_device, NULL);

    g_object_set(G_OBJECT(this->player), "audio-sink", sink, NULL);
  }

  if (videosink != NULL)
  {
    Log_info("gstreamer", "Setting video sink to %s", videosink);

    GstElement* vsink = gst_element_factory_make(videosink, "sink");
    if (vsink == NULL)
      Log_error("gstreamer", "Couldn't create sink '%s'", videosink);
    else
      g_object_set(G_OBJECT(this->player), "video-sink", vsink, NULL);
  }

  if (gst_element_set_state(this->player, GST_STATE_READY) == GST_STATE_CHANGE_FAILURE) 
    Log_error("gstreamer", "Error: pipeline doesn't become ready.");

  g_signal_connect(G_OBJECT(this->players), "about-to-finish", G_CALLBACK([](GstElement* o, gpointer d) =>
  {  
    ((GstreamerOutput*) d)->next_stream();;
  }), this);
  
  output_gstreamer_set_mute(0);

  if (initial_db < 0)
    output_gstreamer_set_volume(exp(initial_db / 20 * log(10)));

  return 0;
}


/**
  @brief  Sets the next stream for playback. Triggered by the "about-to-finish" signal

  @param  none
  @retval void
*/
void GstreamerOutput::next_stream(void)
{
  Log_info("gstreamer", "about-to-finish cb: setting uri %s", this->next_uri.c_str());

  // Swap contents of next URI into current URI
  this->uri.swap(this->next_uri);

  // Cear next URI so we don't repeat
  this->next_uri.clear();

  if (this->uri.length() > 0)
  {
    g_object_set(G_OBJECT(this->player), "uri", this->uri.c_str(), NULL);

    // TODO Tucker callback
    //if (play_trans_callback_) {
    //  // TODO(hzeller): can we figure out when we _actually_
    //  // start playing this ? there are probably a couple
    //  // of seconds between now and actual start.
    //  play_trans_callback_(PLAY_STARTED_NEXT_STREAM);
    //}
  }
}

/**
  @brief  Get the current state of the Gstreamer player

  @param  none
  @retval GstState
*/
void GstreamerOutput::get_player_state(void)
{
  GstState state = GST_STATE_PLAYING; // TODO Tucker, default to playing?
  gst_element_get_state(this->player, &state, NULL, 0);
  
  return state;
}

/**
  @brief  Set the URI of the Gstreamer playback module

  @param  uri URI to set
  @retval none
*/
void GstreamerOutput::set_uri(const std::string &uri)
{
  Log_info("gstreamer", "Set uri to '%s'", uri.c_str());

  this->uri = uri;

  // TODO Tucker Not sure how I feel about this callback. Is it really the 
  // job of the output to provide meta data updates?
  //meta_update_callback_ = meta_cb;
  //SongMetaData_clear(&song_meta_);
}

/**
  @brief  Set the next URI of the Gstreamer playback module

  @param  uri URI to set
  @retval none
*/
void GstreamerOutput::set_next_uri(const std::string &uri)
{
  Log_info("gstreamer", "Set next uri to '%s'", uri.c_str());

  this->next_uri = uri;
}

/**
  @brief  Start playback

  @param  none
  @retval result_t
*/
OutputModule::result_t GstreamerOutput::play(void)
{
  // TODO Tucker callbacks
  //play_trans_callback_ = callback;

  if (get_current_player_state() != GST_STATE_PAUSED) 
  {
    if (gst_element_set_state(this->player, GST_STATE_READY) == GST_STATE_CHANGE_FAILURE) 
      Log_error("gstreamer", "setting play state failed (1)"); // Error, but continue; can't get worse :)

    g_object_set(G_OBJECT(this->player), "uri", this->uri.c_str(), NULL);
  }

  if (gst_element_set_state(this->player, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) 
  {
    Log_error("gstreamer", "setting play state failed (2)");
    return OutputModule::Error
  }

  return OutputModule::Success;
}

/**
  @brief  Stop playback

  @param  none
  @retval result_t
*/
OutputModule::result_t GstreamerOutput::stop(void)
{
  if (gst_element_set_state(this->player, GST_STATE_READY) == GST_STATE_CHANGE_FAILURE)
    return OutputModule::Error;
  
  return OutputModule::Success;
}

/**
  @brief  Pause playback

  @param  none
  @retval result_t
*/
OutputModule::result_t GstreamerOutput::pause(void)
{
  if (gst_element_set_state(this->player, GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE)
    return OutputModule::Error;
  
  return OutputModule::Success;
}

/**
  @brief  Seek player to supplied time position

  @param  position_ns Seek position in nanoseconds
  @retval result_t
*/
OutputModule::result_t GstreamerOutput::seek(int64_t position_ns)
{
  if (gst_element_seek(this->player, 1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, (gint64) position_ns, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE))
    return OutputModule::Success;
  
  return OutputModule::Error;
}

/**
  @brief  Get the duration and position of the current track

  @param  track track_state_t to populate with duration and position information
  @retval result_t
*/
OutputModule::result_t GstreamerOutput::get_position(track_state_t* track)
{
  // TODO Tucker
  //*track_duration = last_known_time_.duration;
  //*track_pos = last_known_time_.position;

  if (this->get_player_state() != GST_STATE_PLAYING)
    return OutputModule::Success;
    
#if (GST_VERSION_MAJOR < 1)
  GstFormat fmt = GST_FORMAT_TIME;
  GstFormat* query_type = &fmt;
#else
  GstFormat query_type = GST_FORMAT_TIME;
#endif

  OutputModule::result_t result = OutputModule::Success;
  if (!gst_element_query_duration(this->player, query_type, (gint64*) &position->duration_ns))
  {
    Log_error("gstreamer", "Failed to get track duration.");
    result = OutputModule::Error;
  }
  
  if (!gst_element_query_position(this->player, query_type, (gint64*) &position->position_ns)) 
  {
    Log_error("gstreamer", "Failed to get track pos");
    result = OutputModule::Error;
  }
  
  // playbin2 does not allow to query while paused. Remember in case
  // we're asked then (it actually returns something, but it is bogus).
  // TODO Tucker
  //last_known_time_.duration = *track_duration;
  //last_known_time_.position = *track_pos;

  return result;
}

/**
  @brief  Get the volume of the Gstreamer output

  @param  volume Current volume (0.0 - 1.0)
  @retval result_t
*/
OutputModule::result_t GstreamerOutput::get_volume(float* volume)
{
  double vol = 0;
  g_object_get(this->player, "volume", &vol, NULL);

  *volume = (float) vol;

  Log_info("gstreamer", "Query volume fraction: %f", vol);

  result OutputModule::Success;
}

/**
  @brief  Set the volume of the Gstreamer output

  @param  volume Desired volume (0.0 - 1.0)
  @retval result_t
*/
OutputModule::result_t GstreamerOutput::set_volume(float* volume)
{
  Log_info("gstreamer", "Set volume fraction to %f", value);
  
  g_object_set(this->player, "volume", (double) volume, NULL);

  result OutputModule::Success;
}

/**
  @brief  Get the mute state of the Gstreamer output

  @param  bool Mute state
  @retval result_t
*/
OutputModule::result_t GstreamerOutput::get_mute(bool* mute)
{
  gboolean val = false;
  g_object_get(this->player, "mute", &val, NULL);

  *mute = (bool) val;

  result OutputModule::Success;
}

/**
  @brief  Set the mute on the Gstreamer output

  @param  mute Mute state
  @retval result_t
*/
OutputModule::result_t GstreamerOutput::set_mute(bool mute)
{
  g_object_set(this->player, "mute", (gboolean) mute, NULL);

  result OutputModule::Success;
}

// This is crazy. I want C++ :)
struct MetaModify {
  struct SongMetaData *meta;
  int any_change;
};

static void MetaModify_add_tag(const GstTagList *list, const gchar *tag,
                               gpointer user_data) {
  struct MetaModify *data = (struct MetaModify *)user_data;
  const char **destination = NULL;
  if (strcmp(tag, GST_TAG_TITLE) == 0) {
    destination = &data->meta->title;
  } else if (strcmp(tag, GST_TAG_ARTIST) == 0) {
    destination = &data->meta->artist;
  } else if (strcmp(tag, GST_TAG_ALBUM) == 0) {
    destination = &data->meta->album;
  } else if (strcmp(tag, GST_TAG_GENRE) == 0) {
    destination = &data->meta->genre;
  } else if (strcmp(tag, GST_TAG_COMPOSER) == 0) {
    destination = &data->meta->composer;
  }
  if (destination != NULL) {
    char *replace = NULL;
    gst_tag_list_get_string(list, tag, &replace);
    if (replace != NULL &&
        (*destination == NULL || strcmp(replace, *destination) != 0)) {
      free((char *)*destination);
      *destination = replace;
      data->any_change++;
    } else {
      free(replace);
    }
  }
}

static gboolean my_bus_callback(GstBus *bus, GstMessage *msg, gpointer data) {
  (void)bus;
  (void)data;

  GstMessageType msgType;
  const GstObject *msgSrc;
  const gchar *msgSrcName;

  msgType = GST_MESSAGE_TYPE(msg);
  msgSrc = GST_MESSAGE_SRC(msg);
  msgSrcName = GST_OBJECT_NAME(msgSrc);

  switch (msgType) {
    case GST_MESSAGE_EOS:
      Log_info("gstreamer", "%s: End-of-stream", msgSrcName);
      if (gs_next_uri_ != NULL) {
        // If playbin does not support gapless (old
        // versions didn't), this will trigger.
        free(gsuri_);
        gsuri_ = gs_next_uri_;
        gs_next_uri_ = NULL;
        gst_element_set_state(player_, GST_STATE_READY);
        g_object_set(G_OBJECT(player_), "uri", gsuri_, NULL);
        gst_element_set_state(player_, GST_STATE_PLAYING);
        if (play_trans_callback_) {
          play_trans_callback_(PLAY_STARTED_NEXT_STREAM);
        }
      } else if (play_trans_callback_) {
        play_trans_callback_(PLAY_STOPPED);
      }
      break;

    case GST_MESSAGE_ERROR: {
      gchar *debug;
      GError *err;

      gst_message_parse_error(msg, &err, &debug);

      Log_error("gstreamer", "%s: Error: %s (Debug: %s)", msgSrcName,
                err->message, debug);
      g_error_free(err);
      g_free(debug);

      break;
    }
    case GST_MESSAGE_STATE_CHANGED: {
      GstState oldstate, newstate, pending;
      gst_message_parse_state_changed(msg, &oldstate, &newstate, &pending);
      /*
      g_print("GStreamer: %s: State change: '%s' -> '%s', "
              "PENDING: '%s'\n", msgSrcName,
              gststate_get_name(oldstate),
              gststate_get_name(newstate),
              gststate_get_name(pending));
      */
      break;
    }

    case GST_MESSAGE_TAG: {
      GstTagList *tags = NULL;

      if (meta_update_callback_ != NULL) {
        gst_message_parse_tag(msg, &tags);
        /*g_print("GStreamer: Got tags from element %s\n",
                GST_OBJECT_NAME (msg->src));
        */
        struct MetaModify modify;
        modify.meta = &song_meta_;
        modify.any_change = 0;
        gst_tag_list_foreach(tags, &MetaModify_add_tag, &modify);
        gst_tag_list_free(tags);
        if (modify.any_change) {
          meta_update_callback_(&song_meta_);
        }
      }
      break;
    }

    case GST_MESSAGE_BUFFERING: {
      if (buffer_duration <= 0.0) break; /* nothing to buffer */

      gint percent = 0;
      gst_message_parse_buffering(msg, &percent);

      /* Pause playback until buffering is complete. */
      if (percent < 100)
        gst_element_set_state(player_, GST_STATE_PAUSED);
      else
        gst_element_set_state(player_, GST_STATE_PLAYING);
      break;
    }
    default:
      /*
      g_print("GStreamer: %s: unhandled message type %d (%s)\n",
              msgSrcName, msgType, gst_message_type_get_name(msgType));
      */
      break;
  }

  return TRUE;
}

static gchar *audio_sink = NULL;
static gchar *audio_device = NULL;
static gchar *audio_pipe = NULL;
static gchar *videosink = NULL;
static double initial_db = 0.0;

/* Options specific to output_gstreamer */
static GOptionEntry option_entries[] = {
    {"gstout-audiosink", 0, 0, G_OPTION_ARG_STRING, &audio_sink,
     "GStreamer audio sink to use "
     "(autoaudiosink, alsasink, osssink, esdsink, ...)",
     NULL},
    {"gstout-audiodevice", 0, 0, G_OPTION_ARG_STRING, &audio_device,
     "GStreamer device for the given audiosink. ", NULL},
    {"gstout-audiopipe", 0, 0, G_OPTION_ARG_STRING, &audio_pipe,
     "GStreamer audio sink to pipeline"
     "(gst-launch format) useful for further output format conversion.",
     NULL},
    {"gstout-videosink", 0, 0, G_OPTION_ARG_STRING, &videosink,
     "GStreamer video sink to use "
     "(autovideosink, xvimagesink, ximagesink, ...)",
     NULL},
    {"gstout-buffer-duration", 0, 0, G_OPTION_ARG_DOUBLE, &buffer_duration,
     "The size of the buffer in seconds. Set to zero to disable buffering.",
     NULL},
    {"gstout-initial-volume-db", 0, 0, G_OPTION_ARG_DOUBLE, &initial_db,
     "GStreamer initial volume in decibel (e.g. 0.0 = max; -6 = 1/2 max) ",
     NULL},
    {NULL}};

static int output_gstreamer_add_options(GOptionContext *ctx) {
  GOptionGroup *option_group;
  option_group =
      g_option_group_new("gstout", "GStreamer Output Options",
                         "Show GStreamer Output Options", NULL, NULL);
  g_option_group_add_entries(option_group, option_entries);

  g_option_context_add_group(ctx, option_group);

  g_option_context_add_group(ctx, gst_init_get_option_group());
  return 0;
}

static void prepare_next_stream(GstElement *obj, gpointer userdata) {
  (void)obj;
  
  GstreamerOutput* output = (GstreamerOutput*) userdata;

  Log_info("gstreamer", "about-to-finish cb: setting uri %s", output->next_uri.c_str());
  
  output->uri = output->next_uri;

  output->next_uri.clear();

  if (output->uri.length() > 0)
  {
    g_object_set(G_OBJECT(output->player), "uri", ouptut->uri, NULL);
    
    // TODO Tucker callback
    //if (play_trans_callback_) {
    //  // TODO(hzeller): can we figure out when we _actually_
    //  // start playing this ? there are probably a couple
    //  // of seconds between now and actual start.
    //  play_trans_callback_(PLAY_STARTED_NEXT_STREAM);
    //}
  }
}

static int output_gstreamer_init(void) {
  GstBus *bus;

  SongMetaData_init(&song_meta_);
  scan_mime_list();

#if (GST_VERSION_MAJOR < 1)
  const char player_element_name[] = "playbin2";
#else
  const char player_element_name[] = "playbin";
#endif

  player_ = gst_element_factory_make(player_element_name, "play");
  assert(player_ != NULL);

  /* set buffer size */
  if (buffer_duration > 0) {
    gint64 buffer_duration_ns = round(buffer_duration * 1.0e9);
    Log_info("gstreamer", "Setting buffer duration to %ldms",
             buffer_duration_ns / 1000000);
    g_object_set(G_OBJECT(player_), "buffer-duration", buffer_duration_ns,
                 NULL);
  } else {
    Log_info("gstreamer", "Buffering disabled (--gstout-buffer-duration)");
  }

  bus = gst_pipeline_get_bus(GST_PIPELINE(player_));
  gst_bus_add_watch(bus, my_bus_callback, NULL);
  gst_object_unref(bus);

  if (audio_sink != NULL && audio_pipe != NULL) {
    Log_error(
        "gstreamer",
        "--gstout-audosink and --gstout-audiopipe are mutually exclusive.");
    return 1;
  }

  if (audio_sink != NULL) {
    GstElement *sink = NULL;
    Log_info("gstreamer", "Setting audio sink to %s; device=%s\n", audio_sink,
             audio_device ? audio_device : "");
    sink = gst_element_factory_make(audio_sink, "sink");
    if (sink == NULL) {
      Log_error("gstreamer", "Couldn't create sink '%s'", audio_sink);
    } else {
      if (audio_device != NULL) {
        g_object_set(G_OBJECT(sink), "device", audio_device, NULL);
      }
      g_object_set(G_OBJECT(player_), "audio-sink", sink, NULL);
    }
  }
  if (audio_pipe != NULL) {
    GstElement *sink = NULL;
    Log_info("gstreamer", "Setting audio sink-pipeline to %s\n", audio_pipe);
    sink = gst_parse_bin_from_description(audio_pipe, TRUE, NULL);

    if (sink == NULL) {
      Log_error("gstreamer", "Could not create pipeline.");
    } else {
      g_object_set(G_OBJECT(player_), "audio-sink", sink, NULL);
    }
  }
  if (videosink != NULL) {
    GstElement *sink = NULL;
    Log_info("gstreamer", "Setting video sink to %s", videosink);
    sink = gst_element_factory_make(videosink, "sink");
    g_object_set(G_OBJECT(player_), "video-sink", sink, NULL);
  }

  if (gst_element_set_state(player_, GST_STATE_READY) ==
      GST_STATE_CHANGE_FAILURE) {
    Log_error("gstreamer", "Error: pipeline doesn't become ready.");
  }

  g_signal_connect(G_OBJECT(player_), "about-to-finish",
                   G_CALLBACK(prepare_next_stream), NULL);
  output_gstreamer_set_mute(0);
  if (initial_db < 0) {
    output_gstreamer_set_volume(exp(initial_db / 20 * log(10)));
  }

  return 0;
}

struct output_module gstreamer_output = {
    .shortname = "gst",
    .description = "GStreamer multimedia framework",
    .add_options = output_gstreamer_add_options,

    .init = output_gstreamer_init,
    .set_uri = output_gstreamer_set_uri,
    .set_next_uri = output_gstreamer_set_next_uri,
    .play = output_gstreamer_play,
    .stop = output_gstreamer_stop,
    .pause = output_gstreamer_pause,
    .seek = output_gstreamer_seek,

    .get_position = output_gstreamer_get_position,
    .get_volume = output_gstreamer_get_volume,
    .set_volume = output_gstreamer_set_volume,
    .get_mute = output_gstreamer_get_mute,
    .set_mute = output_gstreamer_set_mute,
};
