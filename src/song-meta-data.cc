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

// TODO: we're assuming that the namespaces are abbreviated with 'dc' and 'upnp'
// ... but if I understand that correctly, that doesn't need to be the case.

#include "song-meta-data.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sstream>

#include <pugixml/pugixml.hpp>

#include "xmldoc.h"
#include "xmlescape.h"


//TrackMetadata2::TrackMetadata2() {
//  
//  this->xml_document.reset();
//
//  pugi::xml_node root = this->xml_document.append_child("DIDL-Lite");
//  root.append_attribute("xmlns").set_value("urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/");
//  root.append_attribute("xmlns:dc").set_value("http://purl.org/dc/elements/1.1/");
//  root.append_attribute("xmlns:upnp").set_value("urn:schemas-upnp-org:metadata-1-0/upnp/");
//}

// // Allocates a new DIDL formatted XML and fill it with given data.
// // The input fields are expected to be already xml escaped.
// void TrackMetadaa2::Create(std::string& id, std::string& title,
//                            std::string& artist, std::string& album,
//                            std::string& genre, std::string& composer) {
  

//   this->xml_document.reset();

//   pugi::xml_node root = this->xml_document.append_child("DIDL-Lite");
//   root.append_attribute("xmlns").set_value("urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/");
//   root.append_attribute("xmlns:dc").set_value("http://purl.org/dc/elements/1.1/");
//   root.append_attribute("xmlns:upnp").set_value("urn:schemas-upnp-org:metadata-1-0/upnp/");

//   pugi::xml_node item = root.append_child("item");
//   item.append_attribute("id").set_value(id.c_str());

//   pugi::xml_node dcTitle = item.append_child("dc:title");
//   dcTitle.append_child(pugi::node_pcdata).set_value(title.c_str());

//   pugi::xml_node upnpArtist = item.append_child("upnp:artist");
//   upnpArtist.append_child(pugi::node_pcdata).set_value(artist.c_str());

//   pugi::xml_node upnpAlbum = item.append_child("upnp:album");
//   upnpAlbum.append_child(pugi::node_pcdata).set_value(album.c_str());

//   pugi::xml_node upnpGenre = item.append_child("upnp:genre");
//   upnpGenre.append_child(pugi::node_pcdata).set_value(genre.c_str());

//   pugi::xml_node upnpCreator = item.append_child("upnp:creator");
//   upnpCreator.append_child(pugi::node_pcdata).set_value(composer.c_str());

//   //std::ostringstream stream;
//   //this.save(stream, "\t", pugi::format_default | pugi::format_no_declaration);
// }


// TODO: actually use some XML library for this, but spending too much time
// with XML is not good for the brain :) Worst thing that came out of the 90ies.
char *SongMetaData_to_DIDL(const TrackMetadata *object,
                           const char *original_xml) {
  // Generating a unique ID in case the players cache the content by
  // the item-ID. Right now this is experimental and not known to make
  // any difference - it seems that players just don't display changes
  // in the input stream. Grmbl.
  static unsigned int xml_id = 42;
  char unique_id[4 + 8 + 1];
  snprintf(unique_id, sizeof(unique_id), "gmr-%08x", xml_id++);

  char *result;
  //char *title, *artist, *album, *genre, *composer;
  //title = object->title.length() ? xmlescape(object->title.c_str(), 0) : NULL;
  //artist = object->artist.length() ? xmlescape(object->artist.c_str(), 0) : NULL;
  //album = object->album.length() ? xmlescape(object->album.c_str(), 0) : NULL;
  //genre = object->genre.length() ? xmlescape(object->genre.c_str(), 0) : NULL;
  //composer = object->composer.length() ? xmlescape(object->composer.c_str(), 0) : NULL;
  
  if (original_xml == NULL || strlen(original_xml) == 0) {
 // ./  result = generate_DIDL(unique_id, title, artist, album, genre, composer);
  } else {

    int edits = 0;
    // Otherwise, surgically edit the original document to give
    // control points as close as possible what they sent themself.
    //result = strdup(original_xml);
    //result = replace_range(result, "<dc:title>", "</dc:title>", title, &edits);
    //result = replace_range(result, "<upnp:artist>", "</upnp:artist>", artist,
    //                       &edits);
    //result =
    //    replace_range(result, "<upnp:album>", "</upnp:album>", album, &edits);
    //result =
    //    replace_range(result, "<upnp:genre>", "</upnp:genre>", genre, &edits);
    //result = replace_range(result, "<upnp:creator>", "</upnp:creator>",
    //                       composer, &edits);
    if (edits) {
      // Only if we changed the content, we generate a new
      // unique id.
      // TODO(Tucker)
      //result = replace_range(result, " id=\"", "\"", unique_id, &edits);
    }
  }
  return result;
}
