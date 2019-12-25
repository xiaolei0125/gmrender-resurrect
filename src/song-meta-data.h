// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
/* song-meta-data - Object holding meta data for a song.
 *
 * Copyright (C) 2012 Henner Zeller
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

#ifndef _SONG_META_DATA_H
#define _SONG_META_DATA_H

#include <string>
#include <map>
#include <unordered_map>
#include <sstream>

#include <pugixml/pugixml.hpp>
#include <assert.h>
#include <logging.h>

class TrackMetadata
{
  public:
    typedef enum Tag
    { 
      kTitle = 0, // Title must be first 
      kArtist,
      kAlbum,
      kGenre,
      kCreator,
      //kClass, // TODO Required but not sure how to detect from stream
      // Not yet implemented
      //kDate,
      //kTrackNumber,
      //kBitrate,
    } Tag;

    TrackMetadata(void) 
    {
      tags.emplace(kTitle,   "dc:title");
      tags.emplace(kArtist,  "upnp:artist");
      tags.emplace(kAlbum,   "upnp:album");
      tags.emplace(kGenre,   "upnp:genre");
      tags.emplace(kCreator, "dc:creator");
      //tags.emplace(kClass,   "upnp:class");
      //tags.emplace(kDate,    "dc:date");
      //tags.emplace(kTrackNumber,    "upnp:originalTrackNumber");
    }

    const std::string& operator[](Tag tag) const
    {
      assert(this->tags.count(tag));

      return this->tags.at(tag).value;
    }

    std::string& operator[](Tag tag)
    {
      assert(this->tags.count(tag));

      // Assume if someone grabs a non-const tag it'll be updated
      this->notify();

      return this->tags.at(tag).value;
    }

    std::string& operator[](const char* name)
    {
      static std::string invalid_tag;

      if (name_tag_map.count(name))
        return (*this)[name_tag_map.at(name)];

      Log_warn("Metadata", "Unsupported tag name '%s'", name);
      return invalid_tag;
    }
    
    std::string ToXml(const std::string& xml = "") const;

  private:
    class Entry
    {
      public:
        Entry(const std::string& k) : key(k) {}

        const std::string key;
        std::string value;
    };

    uint32_t id = 0;
    std::map<Tag, Entry> tags;

    void notify()
    {
      id++;
    }
  
    std::unordered_map<std::string, Tag> name_tag_map =
    {
      {"artist",      kArtist},
      {"title",       kTitle},
      {"album",       kAlbum},
      {"composer",    kCreator},
      {"genre",       kGenre},
      // To yet implemented
      //{"date",        kDate},
      //{"datetime",    kDate},
      //{"tracknumber", kTrackNumber},
      //{"bitrate",     kBitrate},
    };

  void CreateXmlRoot(pugi::xml_document& xml_document) const;
};

#endif  // _SONG_META_DATA_H
