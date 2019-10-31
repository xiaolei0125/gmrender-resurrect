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
    GstreamerOutput() : OutputModule() {}
    
    result_t initalize(void);

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

    void next_stream(void);

  private:
    GstElement* player = nullptr;

    std::string uri;
    std::string next_uri;

    GstState get_player_state(void);
};

extern struct output_module gstreamer_output;

#endif /*  _OUTPUT_GSTREAMER_H */
