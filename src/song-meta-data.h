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
    class IEntry
    {
      public:
        virtual IEntry& operator=(const std::string& other) = 0;
        virtual operator const std::string&() const = 0;
    };

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

    // "Object" required elements/attributes
    // ID, parentID, title, class, restricted

    // Res tag must have protocolInfo

    TrackMetadata(void) 
    {
      tags.emplace(kTitle,   Entry("dc:title", this));
      tags.emplace(kArtist,  Entry("upnp:artist", this));
      tags.emplace(kAlbum,   Entry("upnp:album", this));
      tags.emplace(kGenre,   Entry("upnp:genre", this));
      tags.emplace(kCreator, Entry("dc:creator", this));
      //tags.emplace(kClass,   "upnp:class");
      //tags.emplace(kDate,    "dc:date");
      //tags.emplace(kTrackNumber,    "upnp:originalTrackNumber");
    }

    const std::string& operator[](Tag tag) const
    {
      assert(this->tags.count(tag));

      return this->tags.at(tag);
    }

    IEntry& operator[](Tag tag)
    {
      assert(this->tags.count(tag));

      return this->tags.at(tag);
    }

    IEntry& operator[](const char* name)
    {
      static Entry invalid_entry("null", this);

      if (name_tag_map.count(name))
        return (*this)[name_tag_map.at(name)];

      Log_warn("Metadata", "Unsupported tag name '%s'", name);
      return invalid_entry;
    }
    
    std::string ToXml(const std::string& xml = "") const;

  private:
  class Entry : public IEntry
  {
    public:
      Entry(const std::string& k, TrackMetadata* const p) : parent(*p), key(k) {}

      Entry& operator=(const std::string& other)
      {          
        if (value.compare(other) == 0) return *this;  // Identical tags

        value.assign(other);
        parent.notify();
        return *this;
      }

      operator const std::string&() const
      {
        return value;
      }

      TrackMetadata& parent;
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
      // Not yet implemented
      //{"date",        kDate},
      //{"datetime",    kDate},
      //{"tracknumber", kTrackNumber},
      //{"bitrate",     kBitrate},
    };

  void CreateXmlRoot(pugi::xml_document& xml_document) const;
};

#endif  // _SONG_META_DATA_H
