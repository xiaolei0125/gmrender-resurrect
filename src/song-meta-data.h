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
#include <unordered_map>
#include <sstream>

#include <pugixml/pugixml.hpp>
#include <assert.h>
#include <logging.h>

class TrackMetadata
{
  public:
    class Entry
    {
      public:
        Entry(TrackMetadata& parent, pugi::xml_node& root, const std::string& k) : metadata(parent)
        {
          this->node = root.append_child(k.c_str());
          this->node.append_child(pugi::node_pcdata).set_value("");
        }

        template<typename T>
        Entry& operator=(const T& other)
        {
          this->node.first_child().text().set(other);
          metadata.notify();
          return *this;
        }

        Entry& operator=(const std::string& other)
        {
          this->node.first_child().text().set(other.c_str());
          metadata.notify();
          return *this;
        }

        operator const std::string() const
        {
          return std::string(node.child_value());
        }

      private:
        TrackMetadata& metadata;
        pugi::xml_node node; // Just a pointer really
    };

    typedef enum Tag
    { 
      kTitle,
      kArtist,
      kAlbum,
      kGenre,
      kCreator
    } Tag;

    TrackMetadata(void) 
    {
      this->xml_document.reset();

      pugi::xml_node root = this->xml_document.append_child("DIDL-Lite");
      root.append_attribute("xmlns").set_value("urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/");
      root.append_attribute("xmlns:dc").set_value("http://purl.org/dc/elements/1.1/");
      root.append_attribute("xmlns:upnp").set_value("urn:schemas-upnp-org:metadata-1-0/upnp/");

      pugi::xml_node item = root.append_child("item");
      item.append_attribute("id").set_value("0");

      metadata.emplace(kTitle,   Entry(*this, item, "dc:title"));
      metadata.emplace(kArtist,  Entry(*this, item, "upnp:artist"));
      metadata.emplace(kAlbum,   Entry(*this, item, "upnp:album"));
      metadata.emplace(kGenre,   Entry(*this, item, "upnp:genre"));
      metadata.emplace(kCreator, Entry(*this, item, "upnp:creator"));
    }

    Entry& operator[](Tag tag)
    {
      assert(this->metadata.count(tag));
      
      return this->metadata.at(tag);
    }

    operator const std::string() const
    {
      char idString[20]  = {0};
      snprintf(idString, sizeof(idString), "gmr-%08x", this->id);
      this->xml_document.first_child().child("item").attribute("id").set_value(idString);

      std::ostringstream stream;
      this->xml_document.save(stream, "\t", pugi::format_default | pugi::format_no_declaration);

      return stream.str();
    }

    void notify()
    {
      id++;
    }

private:
    uint32_t id = 0;

    std::unordered_map<Tag, Entry> metadata;

    pugi::xml_document xml_document;
};

#endif  // _SONG_META_DATA_H
