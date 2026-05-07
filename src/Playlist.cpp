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
#include <set>

#include <tinyxml2.h>
#include "Log.h"
#include "Toolkit/tinyxml2Toolkit.h"
#include "Toolkit/SystemToolkit.h"
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
    filename_ = filename;

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

// Forward declaration
static void extractSessionPaths(const std::string &sessionFile,
                                 std::list<std::string> &list,
                                 std::set<std::string> &visited);

static void scanSessionNode(XMLElement *sessionNode,
                             const std::string &sessionFilePath,
                             std::list<std::string> &list,
                             std::set<std::string> &visited)
{
    for (XMLElement *sourceNode = sessionNode->FirstChildElement("Source");
         sourceNode != nullptr;
         sourceNode = sourceNode->NextSiblingElement("Source"))
    {
        const char *pType = sourceNode->Attribute("type");
        if (!pType) continue;
        const std::string type(pType);

        if (type == "MediaSource") {
            XMLElement *uriNode = sourceNode->FirstChildElement("uri");
            if (uriNode) {
                const char *text = uriNode->GetText();
                if (text) {
                    std::string path(text);
                    if (!SystemToolkit::file_exists(path)) {
                        const char *relative = nullptr;
                        if (uriNode->QueryStringAttribute("relative", &relative) == XML_SUCCESS && relative)
                            path = SystemToolkit::path_absolute_from_path(relative, sessionFilePath);
                    }
                    if (SystemToolkit::file_exists(path) &&
                        std::find(list.begin(), list.end(), path) == list.end())
                        list.push_back(path);
                }
            }
        }
        else if (type == "SessionSource") {
            XMLElement *pathNode = sourceNode->FirstChildElement("path");
            if (pathNode) {
                const char *text = pathNode->GetText();
                if (text) {
                    std::string path(text);
                    if (!SystemToolkit::file_exists(path)) {
                        const char *relative = nullptr;
                        if (pathNode->QueryStringAttribute("relative", &relative) == XML_SUCCESS && relative)
                            path = SystemToolkit::path_absolute_from_path(relative, sessionFilePath);
                    }
                    if (SystemToolkit::file_exists(path)) {
                        if (std::find(list.begin(), list.end(), path) == list.end())
                            list.push_back(path);
                        extractSessionPaths(path, list, visited);
                    }
                }
            }
        }
        else if (type == "GroupSource") {
            XMLElement *innerSession = sourceNode->FirstChildElement("Session");
            if (innerSession)
                scanSessionNode(innerSession, sessionFilePath, list, visited);
        }
        else if (type == "MultiFileSource") {
            XMLElement *seqNode = sourceNode->FirstChildElement("Sequence");
            if (seqNode) {
                const char *text = seqNode->GetText();
                if (text) {
                    std::string location(text);
                    std::string folder = SystemToolkit::path_filename(location);
                    if (!SystemToolkit::path_directory(folder).empty()) {
                        const char *codec = seqNode->Attribute("codec");
                        std::string pattern = codec ? std::string("*.") + codec : "*";
                        std::list<std::string> seqfiles =
                            SystemToolkit::list_directory(folder, {pattern});
                        for (auto &f : seqfiles) {
                            if (std::find(list.begin(), list.end(), f) == list.end())
                                list.push_back(f);
                        }
                    }
                }
            }
        }
        else if (type == "CloneSource" || type == "ShaderSource") {
            // Both source types embed a FilteringProgram whose external shader file
            // is stored as the 'filename' attribute of the <Filter> child element.
            XMLElement *filterNode = sourceNode->FirstChildElement("Filter");
            if (filterNode) {
                const char *filename = nullptr;
                if (filterNode->QueryStringAttribute("filename", &filename) == XML_SUCCESS
                    && filename != nullptr && filename[0] != '\0') {
                    std::string path(filename);
                    if (SystemToolkit::file_exists(path) &&
                        std::find(list.begin(), list.end(), path) == list.end())
                        list.push_back(path);
                }
            }
        }
    }
}

static void extractSessionPaths(const std::string &sessionFile,
                                 std::list<std::string> &list,
                                 std::set<std::string> &visited)
{
    if (visited.count(sessionFile)) return;
    if (!SystemToolkit::file_exists(sessionFile)) return;
    visited.insert(sessionFile);

    XMLDocument xmlDoc;
    if (xmlDoc.LoadFile(sessionFile.c_str()) != XML_SUCCESS) return;

    XMLElement *sessionNode = xmlDoc.FirstChildElement("Session");
    if (!sessionNode) return;

    scanSessionNode(sessionNode, SystemToolkit::path_filename(sessionFile), list, visited);
}

std::list<std::string> Playlist::paths() const {

    std::list<std::string> list;
    std::set<std::string> visited;

    for (auto it = path_.cbegin(); it != path_.cend(); ++it) {
        if (std::find(list.begin(), list.end(), *it) == list.end())
            list.push_back(*it);
        extractSessionPaths(*it, list, visited);
    }

    return list;
}