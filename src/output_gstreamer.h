// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
/* output_gstreamer.h - Definitions for GStreamer output module
 *
 * Copyright (C) 2005-2007   Ivo Clarysse
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

#ifndef _OUTPUT_GSTREAMER_H
#define _OUTPUT_GSTREAMER_H

#include <gst/gst.h>

#include "output_module.h"

class GstreamerOutput : public OutputModule
{
  public:
    GstreamerOutput(output_transition_cb_t play = nullptr, output_update_meta_cb_t meta = nullptr) : OutputModule("gst", "GStreamer multimedia framework", play, meta){}
    
    result_t initalize(void);

    std::vector<GOptionGroup*> get_options(void);

    void set_uri(const std::string &uri);
    void set_next_uri(const std::string &uri);

    result_t play(void);
    result_t stop(void);
    result_t pause(void);
    result_t seek(int64_t position_ns);

    result_t get_position(track_state_t* position);
    result_t get_volume(float* volume);
    result_t set_volume(float volume);
    result_t get_mute(bool* mute);
    result_t set_mute(bool mute);

  private:
    GstElement* player = nullptr;

    std::string uri;
    std::string next_uri;

    struct options_t
    {
      char* audio_sink = nullptr;
      char* audio_device = nullptr;
      char* audio_pipe = nullptr;
      char* video_sink = nullptr;
      double initial_db = 0.0;
      double buffer_duration = 0.0; // Buffer disbled by default, see #182
    } options;

    GstState get_player_state(void);
    void next_stream(void);
    bool bus_callback(GstMessage* message);
};

extern struct output_module gstreamer_output;

#endif /*  _OUTPUT_GSTREAMER_H */
