// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
/* output.c - Output module frontend
 *
 * Copyright (C) 2007 Ivo Clarysse,  (C) 2012 Henner Zeller
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <algorithm>

#include <glib.h>

#include "logging.h"
#include "output_module.h"
#ifdef HAVE_GST
#include "output_gstreamer.h"
#endif
#include "output.h"

#define TAG "output"

typedef struct output_entry_t
{
  std::string shortname;
  std::string description;
  OutputModule* (*create)();
  OutputModule::Options& options;
} output_entry_t;

static std::vector<output_entry_t> modules = 
{
#ifdef HAVE_GST
  {"gst", "GStreamer multimedia framework", GstreamerOutput::create, GstreamerOutput::Options::get()}
#else
// this will be a runtime error, but there is not much point in waiting till then.
#error "No output configured. You need to ./configure --with-gstreamer"
#endif
};

static OutputModule* output_module = NULL;

void output_dump_modules(void) {
  
  if (modules.size() == 0)
  {
    printf("No outputs available.\n");
    return;
  }

  printf("Available outputs:\n"); 
  for (auto& module : modules)
    printf("\t%s - %s%s\n", module.shortname.c_str(), module.description.c_str(), (&module == &modules.front()) ? " (default)" : "");
}

int output_init(const char* shortname) {

  if (modules.size() == 0)
  {
    Log_error(TAG, "No outputs available.");
    return -1;
  }

  // Default to first entry if no name provided
  std::string name(shortname ? shortname : modules.front().shortname);

  // Locate module by shortname
  auto it = std::find_if(modules.begin(), modules.end(), [name](output_entry_t& entry)
  {
    return entry.shortname == name;
  });

  if (it == modules.end())
  {
    Log_error(TAG, "No such output: '%s'", name.c_str());
    return -1;
  }

  const output_entry_t& entry = *it;

  Log_info(TAG, "Using output: %s (%s)", entry.shortname.c_str(), entry.description.c_str());

  output_module = entry.create();

  assert(output_module != NULL);
  
  output_module->initalize(entry.options);

  // Free the modules list
  modules.clear();

  return 0;
}

static GMainLoop *main_loop_ = NULL;
static void exit_loop_sighandler(int sig) {
  if (main_loop_) {
    // TODO(hzeller): revisit - this is not safe to do.
    g_main_loop_quit(main_loop_);
  }
}

int output_loop() {
  /* Create a main loop that runs the default GLib main context */
  main_loop_ = g_main_loop_new(NULL, FALSE);

  signal(SIGINT, &exit_loop_sighandler);
  signal(SIGTERM, &exit_loop_sighandler);

  g_main_loop_run(main_loop_);

  return 0;
}

int output_add_options(GOptionContext *ctx) {
  
  for (const auto& module : modules)
  {
    for (auto option : module.options.get_option_groups())
      g_option_context_add_group(ctx, option);
  }
  
  return 0;
}

void output_set_uri(const char *uri) {
  if (output_module){
    output_module->set_uri(uri);
  }
}
void output_set_next_uri(const char *uri) {
  if (output_module) {
    output_module->set_next_uri(uri);
  }
}

int output_play() {
  if (output_module) {
    return output_module->play();
  }
  return -1;
}

int output_pause(void) {
  if (output_module) {
    return output_module->pause();
  }
  return -1;
}

int output_stop(void) {
  if (output_module) {
    return output_module->stop();
  }
  return -1;
}

int output_seek(gint64 position_nanos) {
  if (output_module) {
    return output_module->seek(position_nanos);
  }
  return -1;
}

int output_get_position(gint64 *track_dur, gint64 *track_pos) {
  if (output_module) {
    OutputModule::track_state_t state;
    if (output_module->get_position(&state))
    {
      *track_dur = state.duration_ns;
      *track_pos = state.position_ns;
    }

  }
  return -1;
}

int output_get_volume(float *value) {
  if (output_module) {
    return output_module->get_volume(value);
  }
  return -1;
}
int output_set_volume(float value) {
  if (output_module) {
    return output_module->set_volume(value);
  }
  return -1;
}
int output_get_mute(int *value) {
  if (output_module) {
    return output_module->get_mute((bool*) value);
  }
  return -1;
}
int output_set_mute(int value) {
  if (output_module) {
    return output_module->set_mute(value);
  }
  return -1;
}
