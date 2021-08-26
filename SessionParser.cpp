#include "SessionParser.h"

using namespace tinyxml2;

#include "SystemToolkit.h"
#include "tinyxml2Toolkit.h"


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
