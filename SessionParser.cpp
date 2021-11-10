/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2020-2021 Bruno Herbelin <bruno.herbelin@gmail.com>
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


#include "SystemToolkit.h"
#include "tinyxml2Toolkit.h"
using namespace tinyxml2;

#include "SessionParser.h"

SessionParser::SessionParser()
{

}

bool SessionParser::open(const std::string &filename)
{
    // if the file exists
    if (filename.empty() || !SystemToolkit::file_exists(filename))
        return false;

    // try to load the file
    xmlDoc_.Clear();
    XMLError eResult = xmlDoc_.LoadFile(filename.c_str());

    // error
    if ( XMLResultError(eResult, false) )
        return false;

    filename_ = filename;
    return true;
}

bool SessionParser::save()
{
    if (filename_.empty())
        return false;

    // save file to disk
    return ( XMLSaveDoc(&xmlDoc_, filename_) );
}

std::map< uint64_t, std::pair<std::string, bool> > SessionParser::pathList() const
{
    std::map< uint64_t, std::pair<std::string, bool> > paths;

    // fill path list
    const XMLElement *session = xmlDoc_.FirstChildElement("Session");
    if (session != nullptr ) {
        const XMLElement *sourceNode = session->FirstChildElement("Source");

        for( ; sourceNode ; sourceNode = sourceNode->NextSiblingElement())
        {
            // get id
            uint64_t sid = 0;
            sourceNode->QueryUnsigned64Attribute("id", &sid);

            // get path
            const XMLElement* pathNode = nullptr;

            pathNode = sourceNode->FirstChildElement("uri");
            if (!pathNode)
                pathNode = sourceNode->FirstChildElement("path");
            if (!pathNode)
                pathNode = sourceNode->FirstChildElement("Sequence");

            if (pathNode) {
                const char *text = pathNode->GetText();
                if (text) {
                    bool exists = SystemToolkit::file_exists(text);
                    paths[sid] = std::pair<std::string, bool>(std::string(text), exists);
                }
            }
        }
    }

    // return path list
    return paths;
}

void SessionParser::replacePath(uint64_t id, const std::string &path)
{
    XMLElement *session = xmlDoc_.FirstChildElement("Session");
    if (session != nullptr ) {
        XMLElement *sourceNode = session->FirstChildElement("Source");

        for( ; sourceNode ; sourceNode = sourceNode->NextSiblingElement())
        {
            // get id
            uint64_t sid = 0;
            sourceNode->QueryUnsigned64Attribute("id", &sid);

            if (sid == id) {

                // get path
                XMLElement* pathNode = nullptr;

                pathNode = sourceNode->FirstChildElement("uri");
                if (!pathNode)
                    pathNode = sourceNode->FirstChildElement("path");
                if (!pathNode)
                    pathNode = sourceNode->FirstChildElement("Sequence");

                if (pathNode) {
                    XMLText *text = xmlDoc_.NewText( path.c_str() );
                    pathNode->InsertEndChild( text );
                }
                break;
            }
        }
    }

}
