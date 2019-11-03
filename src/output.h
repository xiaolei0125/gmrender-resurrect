// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
/* output.h - Output module frontend
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

#ifndef _OUTPUT_H
#define _OUTPUT_H

#include <set>
#include <string>

#include <glib.h>
#include "song-meta-data.h"

namespace Output
{
  typedef enum output_state_t 
  {
    PlaybackStopped,
    StartedNextStream
  } output_state_t;

  // Callbacks types from output to higher levels
  typedef void (*playback_callback_t)(output_state_t);
  typedef void (*metadata_callback_t)(const track_metadata_t&);

  int init(const char *shortname, playback_callback_t play_callback, metadata_callback_t metadata_callback);
  int add_options(GOptionContext *ctx);
  void dump_modules(void);

  std::set<std::string> get_supported_media(void);

  int loop(void);

  void set_uri(const char *uri);
  void set_next_uri(const char *uri);

  int play(void);
  int stop(void);
  int pause(void);
  int get_position(gint64 *track_dur_nanos, gint64 *track_pos_nanos);
  int seek(gint64 position_nanos);

  int get_volume(float *v);
  int set_volume(float v);
  int get_mute(int *m);
  int set_mute(int m);
};

#endif /* _OUTPUT_H */
