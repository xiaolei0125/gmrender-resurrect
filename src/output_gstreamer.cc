// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
/* output_gstreamer.c - Output module for GStreamer
 *
 * Copyright (C) 2005-2007   Ivo Clarysse
 * Copyright (C) 2019        Tucker Kern
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

  if (this->options.audio_sink != NULL && this->options.audio_pipe != NULL) 
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
    return OutputModule::Error;

  // Configure buffering if enabled
  if (this->options.buffer_duration > 0) 
  {
    int64_t buffer_duration_ns = round(this->options.buffer_duration * 1.0e9);
    Log_info("gstreamer", "Setting buffer duration to %ldms", buffer_duration_ns / 1000000);
    g_object_set(G_OBJECT(this->player), "buffer-duration", (gint64) buffer_duration_ns, NULL);
  } 
  else 
  {
    Log_info("gstreamer", "Buffering disabled (--gstout-buffer-duration)");
  }

  GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(this->player));
  
  gst_bus_add_watch(bus, [](GstBus* b, GstMessage* m, gpointer d) -> gboolean
  {
    return ((GstreamerOutput*) d)->bus_callback(m);
  }, this);

  gst_object_unref(bus);

  GstElement* sink = NULL;
  if (this->options.audio_sink)
  {
    Log_info("gstreamer", "Setting audio sink to %s; device=%s\n", this->options.audio_sink, this->options.audio_device ? this->options.audio_device : "");
   
    sink = gst_element_factory_make(this->options.audio_sink, "sink");
    if (sink == NULL)
      Log_error("gstreamer", "Couldn't create sink '%s'", this->options.audio_sink);
  }
  else if (this->options.audio_pipe) 
  {
    Log_info("gstreamer", "Setting audio sink-pipeline to %s\n", this->options.audio_pipe);
   
    sink = gst_parse_bin_from_description(this->options.audio_pipe, TRUE, NULL);
    if (sink == NULL)
      Log_error("gstreamer", "Could not create pipeline.");
  }

  if (sink != NULL)
  {
    // Add the audio device if it exists
    if (this->options.audio_device != NULL)
      g_object_set(G_OBJECT(sink), "device", this->options.audio_device, NULL);

    g_object_set(G_OBJECT(this->player), "audio-sink", sink, NULL);
  }

  if (this->options.video_sink != NULL)
  {
    Log_info("gstreamer", "Setting video sink to %s", this->options.video_sink);

    GstElement* vsink = gst_element_factory_make(this->options.video_sink, "sink");
    if (vsink == NULL)
      Log_error("gstreamer", "Couldn't create sink '%s'", this->options.video_sink);
    else
      g_object_set(G_OBJECT(this->player), "video-sink", vsink, NULL);
  }

  if (gst_element_set_state(this->player, GST_STATE_READY) == GST_STATE_CHANGE_FAILURE) 
    Log_error("gstreamer", "Error: pipeline doesn't become ready.");

  // Typedef a function pointer of the about-to-finish callback
  typedef void (*CallbackType)(GstElement*, gpointer);
  g_signal_connect(G_OBJECT(this->player), "about-to-finish", G_CALLBACK((CallbackType) [](GstElement* o, gpointer d) -> void
  {  
    ((GstreamerOutput*) d)->next_stream();
  }), this);
  
  this->set_mute(false);

  if (this->options.initial_db < 0)
    this->set_volume(exp(this->options.initial_db / 20 * log(10)));

  return OutputModule::Success;
}

/**
  @brief  Return the command line options assocaited with the output module

  @param  void
  @retval std::vector<GOptionGroup*>
*/
std::vector<GOptionGroup*> GstreamerOutput::get_options()
{
  std::vector<GOptionGroup*> optionGroups;

  GOptionGroup* option_group = g_option_group_new("gstout", "GStreamer Output Options",
                                                  "Show GStreamer Output Options", NULL, NULL);

  GOptionEntry option_entries[] = 
  {
    {"gstout-audiosink", 0, 0, G_OPTION_ARG_STRING, &this->options.audio_sink,
     "GStreamer audio sink to use (autoaudiosink, alsasink, osssink, esdsink, ...)", NULL},

    {"gstout-audiodevice", 0, 0, G_OPTION_ARG_STRING, &this->options.audio_device, 
     "GStreamer device for the given audiosink. ", NULL},

    {"gstout-audiopipe", 0, 0, G_OPTION_ARG_STRING, &this->options.audio_pipe,
     "GStreamer audio sink to pipeline (gst-launch format) useful for further output format conversion.", NULL},

    {"gstout-videosink", 0, 0, G_OPTION_ARG_STRING, &this->options.video_sink,
     "GStreamer video sink to use (autovideosink, xvimagesink, ximagesink, ...)", NULL},
  
    {"gstout-buffer-duration", 0, 0, G_OPTION_ARG_DOUBLE, &this->options.buffer_duration,
     "The size of the buffer in seconds. Set to zero to disable buffering.", NULL},

    {"gstout-initial-volume-db", 0, 0, G_OPTION_ARG_DOUBLE, &this->options.initial_db,
     "GStreamer initial volume in decibel (e.g. 0.0 = max; -6 = 1/2 max) ", NULL},
    {NULL}
    };

  g_option_group_add_entries(option_group, option_entries);

  optionGroups.push_back(option_group);
  optionGroups.push_back(gst_init_get_option_group());

  return optionGroups;
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

  if (this->get_player_state() != GST_STATE_PAUSED) 
  {
    if (gst_element_set_state(this->player, GST_STATE_READY) == GST_STATE_CHANGE_FAILURE) 
      Log_error("gstreamer", "setting play state failed (1)"); // Error, but continue; can't get worse :)

    g_object_set(G_OBJECT(this->player), "uri", this->uri.c_str(), NULL);
  }

  if (gst_element_set_state(this->player, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) 
  {
    Log_error("gstreamer", "setting play state failed (2)");
    return OutputModule::Error;
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
#if (GST_VERSION_MAJOR < 1)
  static track_state_t last_state;

  // playbin2 does not allow to query while paused, return last known state
  *track = last_state;
  if (this->get_player_state() != GST_STATE_PLAYING)
    return OutputModule::Success;
    
  GstFormat fmt = GST_FORMAT_TIME;
  GstFormat* query_type = &fmt;
#else
  GstFormat query_type = GST_FORMAT_TIME;
#endif

  OutputModule::result_t result = OutputModule::Success;
  if (!gst_element_query_duration(this->player, query_type, (gint64*) &track->duration_ns))
  {
    Log_error("gstreamer", "Failed to get track duration.");
    result = OutputModule::Error;
  }
  
  if (!gst_element_query_position(this->player, query_type, (gint64*) &track->position_ns)) 
  {
    Log_error("gstreamer", "Failed to get track pos");
    result = OutputModule::Error;
  }
  
#if (GST_VERSION_MAJOR < 1)
  // Update last known state
  last_state = *track
#endif

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

  return OutputModule::Success;
}

/**
  @brief  Set the volume of the Gstreamer output

  @param  volume Desired volume (0.0 - 1.0)
  @retval result_t
*/
OutputModule::result_t GstreamerOutput::set_volume(float volume)
{
  Log_info("gstreamer", "Set volume fraction to %f", volume);
  
  g_object_set(this->player, "volume", (double) volume, NULL);

  return OutputModule::Success;
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

  return OutputModule::Success;
}

/**
  @brief  Set the mute on the Gstreamer output

  @param  mute Mute state
  @retval result_t
*/
OutputModule::result_t GstreamerOutput::set_mute(bool mute)
{
  g_object_set(this->player, "mute", (gboolean) mute, NULL);

  return OutputModule::Success;
}

/**
  @brief  Get the current state of the Gstreamer player

  @param  none
  @retval GstState
*/
GstState GstreamerOutput::get_player_state(void)
{
  GstState state = GST_STATE_PLAYING; // TODO Tucker, default to playing?
  gst_element_get_state(this->player, &state, NULL, 0);
  
  return state;
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

    // TODO(hzeller): can we figure out when we _actually_ start playing this? 
    // There are probably a couple of seconds between now and actual start.
    this->notify_playback_update(PLAY_STARTED_NEXT_STREAM);
    }
  }

/**
  @brief  Handle message from the Gstreamer bus

  @param  message GstMessage to process
  @retval bool - Message handled
*/
bool GstreamerOutput::bus_callback(GstMessage* message)
{
  switch (message->type) 
  {
    case GST_MESSAGE_EOS:
    {
      Log_info("gstreamer", "%s: End-of-stream", message->src->name);

      if (this->next_uri.length() > 0)
      {
        // If playbin does not support gapless (old versions didn't), this will trigger.
        
        // Swap contents of next URI into current URI
        this->uri.swap(this->next_uri);

        // Cear next URI so we don't repeat
        this->next_uri.clear();

        gst_element_set_state(this->player, GST_STATE_READY);

        g_object_set(G_OBJECT(this->player), "uri", this->uri.c_str(), NULL);

        gst_element_set_state(this->player, GST_STATE_PLAYING);

        this->notify_playback_update(PLAY_STARTED_NEXT_STREAM);
      }
      else
        this->notify_playback_update(PLAY_STOPPED);

      break;
    }

    case GST_MESSAGE_ERROR: 
    {
      GError* err = NULL;
      gchar* debug = NULL;
      gst_message_parse_error(message, &err, &debug);

      Log_error("gstreamer", "%s: Error: %s (Debug: %s)", message->src->name, err->message, debug);

      g_error_free(err);
      g_free(debug);

      break;
    }

    case GST_MESSAGE_STATE_CHANGED: 
    {
#ifdef DEBUG_STATE_CHANGE
      GstState oldstate, newstate, pending;
      gst_message_parse_state_changed(message, &oldstate, &newstate, &pending);

      Log_info("gstreamer", "Source: %s: State change: '%s' -> '%s', "
              "PENDING: '%s'\n", message->src->name,
              gststate_get_name(oldstate),
              gststate_get_name(newstate),
              gststate_get_name(pending));
#endif
      break;
    }

    case GST_MESSAGE_TAG:
    {
      // Nothing to do
      if (this->metadata_callback == NULL)
        break;
     
      GstTagList* tag_list = NULL;
      gst_message_parse_tag(message, &tag_list);

      auto attemptTagUpdate = [tag_list](std::string& tag, const char* tag_name) -> bool
      {
        // Attempt to fetch the tag
        gchar* value = NULL;
        if (gst_tag_list_get_string(tag_list, tag_name, &value))
          Log_info("gstreamer", "Got tag (%s) value (%s)", tag_name, value);

        // Copy into a string
        std::string new_tag(value);
        
        // Free the tag buffer
        g_free(&value);

        if (tag.compare(new_tag) == 0)
          return false; // Identical tags
        
        tag.swap(new_tag);
        
        return true;
      };

      bool notify = false;

      notify |= attemptTagUpdate(this->metadata.title, GST_TAG_TITLE);
      notify |= attemptTagUpdate(this->metadata.artist, GST_TAG_ARTIST);
      notify |= attemptTagUpdate(this->metadata.album, GST_TAG_ALBUM);
      notify |= attemptTagUpdate(this->metadata.genre, GST_TAG_GENRE);
      notify |= attemptTagUpdate(this->metadata.composer, GST_TAG_COMPOSER);
      
      if (notify)
        this->notify_metadata_change(this->metadata);

      gst_tag_list_free(tag_list);

      break;
    }

    case GST_MESSAGE_BUFFERING: 
    {
      if (this->options.buffer_duration <= 0.0) 
        break; // Buffering disabled

      gint percent = 0;
      gst_message_parse_buffering(message, &percent);

      /* Pause playback until buffering is complete. */
      if (percent < 100)
        gst_element_set_state(this->player, GST_STATE_PAUSED);
      else
        gst_element_set_state(this->player, GST_STATE_PLAYING);

      break;
    }
    
    default:
      break;
  }

  return true;
}

struct output_module gstreamer_output = {
    .shortname = "gst",
    .description = "GStreamer multimedia framework",
};
