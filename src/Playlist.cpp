/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2023 Bruno Herbelin <bruno.herbelin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
**/

#include <algorithm>

#include <tinyxml2.h>
#include "tinyxml2Toolkit.h"
using namespace tinyxml2;

#include "Playlist.h"

Playlist::Playlist()
{

}

Playlist& Playlist::operator = (const Playlist& b)
{
    if (this != &b) {
        path_.clear();
        for(auto it = b.path_.cbegin(); it != b.path_.cend(); ++it)
            path_.push_back(*it);
    }
    return *this;
}

void Playlist::clear()
{
    path_.clear();
}

void Playlist::load(const std::string &filename)
{
    // try to load playlist file
    XMLDocument xmlDoc;
    XMLError eResult = xmlDoc.LoadFile(filename.c_str());

    // do not warn if non existing file
    if (eResult == XML_ERROR_FILE_NOT_FOUND)
        return;
    // warn and return on other error
    else if (XMLResultError(eResult))
        return;

    // first element should be called by the application name
    XMLElement *pRoot = xmlDoc.FirstChildElement("vimixplaylist");
    if (pRoot == nullptr)
        return;

    // all good, can clear previous list
    filename_ = filename;
    path_.clear();

    // Then it should contain a list of path
    XMLElement* pathNode = pRoot->FirstChildElement("path");
    for( ; pathNode ; pathNode = pathNode->NextSiblingElement()) {
        const char *p = pathNode->GetText();
        if (p)
            add( std::string(p) );
    }
}

bool Playlist::save()
{
    if ( filename_.empty() )
        return false;

    saveAs(filename_);
    return true;
}

void Playlist::saveAs(const std::string &filename)
{
    XMLDocument xmlDoc;
    XMLDeclaration *pDec = xmlDoc.NewDeclaration();
    xmlDoc.InsertFirstChild(pDec);

    XMLElement *pRoot = xmlDoc.NewElement("vimixplaylist");
    xmlDoc.InsertEndChild(pRoot);

    for(auto it = path_.cbegin(); it != path_.cend(); ++it) {
        XMLElement *pathNode = xmlDoc.NewElement("path");
        XMLText *text = xmlDoc.NewText( it->c_str() );
        pathNode->InsertEndChild( text );
        pRoot->InsertEndChild(pathNode);
    }

    XMLError eResult = xmlDoc.SaveFile(filename.c_str());
    if ( !XMLResultError(eResult))
        filename_ = filename;
}

bool Playlist::add(const std::string &path)
{
    if (has(path))
        return false;
    path_.push_back(path);
    return true;
}

size_t Playlist::add(const std::list<std::string> &list)
{
    size_t before = path_.size();
    for (auto it = list.begin(); it != list.end(); ++it) {
        if (!has( *it ))
            path_.push_back( *it );
    }
    return path_.size()-before;
}

void Playlist::remove(const std::string &path)
{
    std::deque<std::string>::const_iterator it = std::find(path_.begin(), path_.end(), path);

    if (it != path_.end())
        path_.erase(it);
}

bool Playlist::has(const std::string &path) const
{
    std::deque<std::string>::const_iterator it = std::find(path_.begin(), path_.end(), path);

    return (it != path_.end());
}

void Playlist::remove(size_t index)
{
    if ( index < path_.size() ) {
        path_.erase (path_.begin()+index);
    }
}

void Playlist::move(size_t from_index, size_t to_index)
{
    if ( from_index < path_.size() && to_index < path_.size() && from_index != to_index ) {
        if ( from_index < to_index ) {
            path_.insert(path_.begin() + to_index + 1, path_[from_index]);
            path_.erase(path_.begin() + from_index);
        }
        else {
            path_.insert(path_.begin() + to_index, path_[from_index]);
            path_.erase(path_.begin() + from_index + 1);
        }
    }
}

