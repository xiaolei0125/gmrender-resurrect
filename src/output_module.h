// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
/* output_module.h - Output module interface definition
 *
 * Copyright (C) 2007   Ivo Clarysse
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

#ifndef _OUTPUT_MODULE_H
#define _OUTPUT_MODULE_H

#include <string>
#include <vector>

#include "output.h"

class OutputModule
{
  public:
    typedef struct track_state_t {int64_t duration_ns; int64_t position_ns;} track_state_t;
  
    const std::string shortname;
    const std::string description;

    typedef enum result_t
    {
      Success = 0,
      Error = -1
    } result_t;

    OutputModule(const char* name, const char* desc) : shortname(name), description(desc) {}

    virtual result_t initalize(void) = 0;

    virtual std::vector<GOptionGroup*> get_options(void) = 0;
    virtual void set_uri(const std::string &uri) = 0;
    virtual void set_next_uri(const std::string &uri) = 0;

    virtual result_t play(void) = 0;
    virtual result_t stop(void) = 0;
    virtual result_t pause(void) = 0;
    virtual result_t seek(int64_t position_ns) = 0;

    virtual result_t get_position(track_state_t* position) = 0;
    virtual result_t get_volume(float* volume) = 0;
    virtual result_t set_volume(float volume) = 0;
    virtual result_t get_mute(bool* mute) = 0;
    virtual result_t set_mute(bool mute) = 0;

};


struct output_module {
  const char *shortname;
  const char *description;
  int (*add_options)(GOptionContext *ctx);

  // Commands.
  int (*init)(void);
  void (*set_uri)(const char *uri, output_update_meta_cb_t meta_info);
  void (*set_next_uri)(const char *uri);
  int (*play)(output_transition_cb_t transition_callback);
  int (*stop)(void);
  int (*pause)(void);
  int (*seek)(gint64 position_nanos);

  // parameters
  int (*get_position)(gint64 *track_duration, gint64 *track_pos);
  int (*get_volume)(float *);
  int (*set_volume)(float);
  int (*get_mute)(int *);
  int (*set_mute)(int);
};

#endif
