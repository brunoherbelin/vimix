#include "SessionCreator.h"

#include "Log.h"
#include "defines.h"
#include "Scene.h"
#include "Primitives.h"
#include "Mesh.h"
#include "Source.h"
#include "MediaSource.h"
#include "SessionSource.h"
#include "StreamSource.h"
#include "PatternSource.h"
#include "DeviceSource.h"
#include "NetworkSource.h"
#include "Session.h"
#include "ImageShader.h"
#include "ImageProcessingShader.h"
#include "MediaPlayer.h"

#include <tinyxml2.h>
#include "tinyxml2Toolkit.h"
using namespace tinyxml2;


std::string SessionCreator::info(const std::string& filename)
{
    std::string ret = "";

    XMLDocument doc;
    XMLError eResult = doc.LoadFile(filename.c_str());
    if ( XMLResultError(eResult)) {
        Log::Warning("%s could not be openned.", filename.c_str());
        return ret;
    }

    XMLElement *header = doc.FirstChildElement(APP_NAME);
    if (header != nullptr && header->Attribute("date") != 0) {
        int s = header->IntAttribute("size");
        ret = std::to_string( s ) + " source" + ( s > 1 ? "s\n" : "\n");
        const char *att_string = header->Attribute("resolution");
        if (att_string)
            ret += std::string( att_string ) + "\n";
        att_string = header->Attribute("date");
        if (att_string) {
            std::string date( att_string );
            ret += date.substr(6,2) + "/" + date.substr(4,2) + "/" + date.substr(0,4) + " @ ";
            ret += date.substr(8,2) + ":" + date.substr(10,2);
        }

    }

    return ret;
}

SessionCreator::SessionCreator(): SessionLoader(nullptr)
{

}

void SessionCreator::load(const std::string& filename)
{
    XMLError eResult = xmlDoc_.LoadFile(filename.c_str());
    if ( XMLResultError(eResult)){
        Log::Warning("%s could not be openned.", filename.c_str());
        return;
    }

    XMLElement *header = xmlDoc_.FirstChildElement(APP_NAME);
    if (header == nullptr) {
        Log::Warning("%s is not a %s session file.", filename.c_str(), APP_NAME);
        return;
    }

    int version_major = -1, version_minor = -1;
    header->QueryIntAttribute("major", &version_major); // TODO incompatible if major is different?
    header->QueryIntAttribute("minor", &version_minor);
    if (version_major != XML_VERSION_MAJOR || version_minor != XML_VERSION_MINOR){
        Log::Warning("%s is in a different versions of session file. Loading might fail.", filename.c_str());
        return;
    }

    // session file seems legit, create a session
    session_ = new Session;

    // ready to read sources
    SessionLoader::load( xmlDoc_.FirstChildElement("Session") );

    // load optionnal config
    loadConfig( xmlDoc_.FirstChildElement("Views") );

    // all good
    session_->setFilename(filename);
}


void SessionCreator::loadConfig(XMLElement *viewsNode)
{
    if (viewsNode != nullptr) {
        // ok, ready to read views
        SessionLoader::XMLToNode( viewsNode->FirstChildElement("Mixing"), *session_->config(View::MIXING));
        SessionLoader::XMLToNode( viewsNode->FirstChildElement("Geometry"), *session_->config(View::GEOMETRY));
        SessionLoader::XMLToNode( viewsNode->FirstChildElement("Layer"), *session_->config(View::LAYER));
        SessionLoader::XMLToNode( viewsNode->FirstChildElement("Rendering"), *session_->config(View::RENDERING));
    }
}

SessionLoader::SessionLoader(Session *session): Visitor(), session_(session)
{

}

//Source *SessionLoader::createSource(XMLElement *sourceNode)
//{
//    // source to load
//    Source *load_source = nullptr;

//    // check if a source with the given id exists in the session
//    uint64_t id__ = 0;
//    sourceNode->QueryUnsigned64Attribute("id", &id__);
//    SourceList::iterator sit = session_->find(id__);

//    // no source with this id exists
//    if ( sit == session_->end() ) {
//        // create a new source depending on type
//        const char *pType = sourceNode->Attribute("type");
//        if (!pType)
//            continue;
//        if ( std::string(pType) == "MediaSource") {
//            load_source = new MediaSource;
//        }
//        else if ( std::string(pType) == "SessionSource") {
//            load_source = new SessionSource;
//        }
//        else if ( std::string(pType) == "RenderSource") {
//            load_source = new RenderSource(session_);
//        }
//        else if ( std::string(pType) == "PatternSource") {
//            load_source = new PatternSource;
//        }
//        else if ( std::string(pType) == "DeviceSource") {
//            load_source = new DeviceSource;
//        }

//        // skip failed (including clones)
//        if (!load_source)
//            continue;

//        // add source to session
//        session_->addSource(load_source);
//    }
//    // get reference to the existing source
//    else
//        load_source = *sit;

//    return load_source;
//}

void SessionLoader::load(XMLElement *sessionNode)
{
    sources_id_.clear();

    if (sessionNode != nullptr && session_ != nullptr) {

        XMLElement* sourceNode = sessionNode->FirstChildElement("Source");
        for( ; sourceNode ; sourceNode = sourceNode->NextSiblingElement())
        {
            xmlCurrent_ = sourceNode;

            // source to load
            Source *load_source = nullptr;

            // check if a source with the given id exists in the session
            uint64_t id__ = 0;
            xmlCurrent_->QueryUnsigned64Attribute("id", &id__);
            SourceList::iterator sit = session_->find(id__);

            // no source with this id exists
            if ( sit == session_->end() ) {
                // create a new source depending on type
                const char *pType = xmlCurrent_->Attribute("type");
                if (!pType)
                    continue;
                if ( std::string(pType) == "MediaSource") {
                    load_source = new MediaSource;
                }
                else if ( std::string(pType) == "SessionSource") {
                    load_source = new SessionSource;
                }
                else if ( std::string(pType) == "RenderSource") {
                    load_source = new RenderSource(session_);
                }
                else if ( std::string(pType) == "PatternSource") {
                    load_source = new PatternSource;
                }
                else if ( std::string(pType) == "DeviceSource") {
                    load_source = new DeviceSource;
                }
                else if ( std::string(pType) == "NetworkSource") {
                    load_source = new NetworkSource;
                }

                // skip failed (including clones)
                if (!load_source)
                    continue;

                // add source to session
                session_->addSource(load_source);
            }
            // get reference to the existing source
            else
                load_source = *sit;

            // apply config to source
            load_source->accept(*this);
            load_source->touch();
            // remember
            sources_id_.push_back( load_source->id() );
        }

        // create clones after all sources, to be able to clone a source created above
        sourceNode = sessionNode->FirstChildElement("Source");
        for( ; sourceNode ; sourceNode = sourceNode->NextSiblingElement())
        {
            xmlCurrent_ = sourceNode;

            // verify type of node
            const char *pType = xmlCurrent_->Attribute("type");
            if ( pType && std::string(pType) == "CloneSource") {

                // check if a source with same id exists
                uint64_t id__ = 0;
                xmlCurrent_->QueryUnsigned64Attribute("id", &id__);
                SourceList::iterator sit = session_->find(id__);

                // no source clone with this id exists
                if ( sit == session_->end() ) {

                    // clone from given origin
                    XMLElement* originNode = xmlCurrent_->FirstChildElement("origin");
                    if (originNode) {
                        std::string sourcename = std::string ( originNode->GetText() );
                        SourceList::iterator origin = session_->find(sourcename);
                        // found the orign source
                        if (origin != session_->end()) {
                            // create a new source of type Clone
                            Source *clone_source = (*origin)->clone();
                            // add source to session
                            session_->addSource(clone_source);
                            // apply config to source
                            clone_source->accept(*this);
                            // remember
                            sources_id_.push_back( clone_source->id() );
                        }
                    }
                }
            }
        }
        // make sure no duplicate
        sources_id_.unique();
    }

}

Source *SessionLoader::cloneOrCreateSource(tinyxml2::XMLElement *sourceNode)
{
    xmlCurrent_ = sourceNode;

    // source to load
    Source *load_source = nullptr;
    bool is_clone = false;

    // check if a source with the given id exists in the session
    uint64_t id__ = 0;
    xmlCurrent_->QueryUnsigned64Attribute("id", &id__);
    SourceList::iterator sit = session_->find(id__);

    // no source with this id exists
    if ( sit == session_->end() ) {
        // create a new source depending on type
        const char *pType = xmlCurrent_->Attribute("type");
        if (pType) {
            if ( std::string(pType) == "MediaSource") {
                load_source = new MediaSource;
            }
            else if ( std::string(pType) == "SessionSource") {
                load_source = new SessionSource;
            }
            else if ( std::string(pType) == "RenderSource") {
                load_source = new RenderSource(session_);
            }
            else if ( std::string(pType) == "PatternSource") {
                load_source = new PatternSource;
            }
            else if ( std::string(pType) == "DeviceSource") {
                load_source = new DeviceSource;
            }
            else if ( std::string(pType) == "CloneSource") {
                // clone from given origin
                XMLElement* originNode = xmlCurrent_->FirstChildElement("origin");
                if (originNode) {
                    std::string sourcename = std::string ( originNode->GetText() );
                    SourceList::iterator origin = session_->find(sourcename);
                    // found the orign source
                    if (origin != session_->end())
                        load_source = (*origin)->clone();
                }
            }
        }
    }
    // clone existing source
    else {
        load_source = (*sit)->clone();
        is_clone = true;
    }

    // apply config to source
    if (load_source) {
        load_source->accept(*this);
        // reset mixing (force to place in mixing scene)
        load_source->group(View::MIXING)->translation_ = glm::vec3(DEFAULT_MIXING_TRANSLATION, 0.f);
        // increment depth for clones (avoid supperposition)
        if (is_clone)
            load_source->group(View::LAYER)->translation_.z += 0.2f;
    }

    return load_source;
}


void SessionLoader::XMLToNode(tinyxml2::XMLElement *xml, Node &n)
{
    if (xml != nullptr){
        XMLElement *node = xml->FirstChildElement("Node");
        if ( !node || std::string(node->Name()).find("Node") == std::string::npos )
            return;

        XMLElement *scaleNode = node->FirstChildElement("scale");
        if (scaleNode)
            tinyxml2::XMLElementToGLM( scaleNode->FirstChildElement("vec3"), n.scale_);
        XMLElement *translationNode = node->FirstChildElement("translation");
        if (translationNode)
            tinyxml2::XMLElementToGLM( translationNode->FirstChildElement("vec3"), n.translation_);
        XMLElement *rotationNode = node->FirstChildElement("rotation");
        if (rotationNode)
            tinyxml2::XMLElementToGLM( rotationNode->FirstChildElement("vec3"), n.rotation_);
    }
}

void SessionLoader::visit(Node &n)
{
    XMLToNode(xmlCurrent_, n);
}

void SessionLoader::visit(MediaPlayer &n)
{
    XMLElement* mediaplayerNode = xmlCurrent_->FirstChildElement("MediaPlayer");
    uint64_t id__ = -1;
    mediaplayerNode->QueryUnsigned64Attribute("id", &id__);

    if (mediaplayerNode) {
        // timeline
        XMLElement *timelineelement = mediaplayerNode->FirstChildElement("Timeline");
        if (timelineelement) {
            Timeline tl;
            tl.setTiming( n.timeline()->interval(), n.timeline()->step());
            XMLElement *gapselement = timelineelement->FirstChildElement("Gaps");
            if (gapselement) {
                XMLElement* gap = gapselement->FirstChildElement("Interval");
                for( ; gap ; gap = gap->NextSiblingElement())
                {
                    GstClockTime a = GST_CLOCK_TIME_NONE;
                    GstClockTime b = GST_CLOCK_TIME_NONE;
                    gap->QueryUnsigned64Attribute("begin", &a);
                    gap->QueryUnsigned64Attribute("end", &b);
                    tl.addGap( a, b );
                }
            }
            XMLElement *fadingselement = timelineelement->FirstChildElement("Fading");
            if (fadingselement) {
                XMLElement* array = fadingselement->FirstChildElement("array");
                XMLElementDecodeArray(array, tl.fadingArray(), MAX_TIMELINE_ARRAY * sizeof(float));
            }
            n.setTimeline(tl);
        }

        // change play status only if different id (e.g. new media player)
        if ( n.id() != id__ ) {

            double speed = 1.0;
            mediaplayerNode->QueryDoubleAttribute("speed", &speed);
            n.setPlaySpeed(speed);

            int loop = 1;
            mediaplayerNode->QueryIntAttribute("loop", &loop);
            n.setLoop( (MediaPlayer::LoopMode) loop);

            bool play = true;
            mediaplayerNode->QueryBoolAttribute("play", &play);
            n.play(play);
        }
    }
}

void SessionLoader::visit(Shader &n)
{
    XMLElement* color = xmlCurrent_->FirstChildElement("color");
    if ( color ) {
        tinyxml2::XMLElementToGLM( color->FirstChildElement("vec4"), n.color);
        XMLElement* blending = xmlCurrent_->FirstChildElement("blending");
        if (blending) {
            int blend = 0;
            blending->QueryIntAttribute("mode", &blend);
            n.blending = (Shader::BlendMode) blend;
        }
    }
}

void SessionLoader::visit(ImageShader &n)
{
    const char *pType = xmlCurrent_->Attribute("type");
    if ( std::string(pType) != "ImageShader" )
        return;

    XMLElement* uniforms = xmlCurrent_->FirstChildElement("uniforms");
    if (uniforms) {
        uniforms->QueryFloatAttribute("stipple", &n.stipple);
        uniforms->QueryUnsignedAttribute("mask", &n.mask);
    }

    XMLElement* uvtex = xmlCurrent_->FirstChildElement("uv");
    if (uvtex)
        tinyxml2::XMLElementToGLM( uvtex->FirstChildElement("vec4"), n.uv);
}

void SessionLoader::visit(ImageProcessingShader &n)
{
    const char *pType = xmlCurrent_->Attribute("type");
    if ( std::string(pType) != "ImageProcessingShader" )
        return;

    XMLElement* uniforms = xmlCurrent_->FirstChildElement("uniforms");
    if (uniforms) {
        uniforms->QueryFloatAttribute("brightness", &n.brightness);
        uniforms->QueryFloatAttribute("contrast", &n.contrast);
        uniforms->QueryFloatAttribute("saturation", &n.saturation);
        uniforms->QueryFloatAttribute("hueshift", &n.hueshift);
        uniforms->QueryFloatAttribute("threshold", &n.threshold);
        uniforms->QueryFloatAttribute("lumakey", &n.lumakey);
        uniforms->QueryIntAttribute("nbColors", &n.nbColors);
        uniforms->QueryIntAttribute("invert", &n.invert);
        uniforms->QueryFloatAttribute("chromadelta", &n.chromadelta);
        uniforms->QueryIntAttribute("filter", &n.filterid);
    }

    XMLElement* gamma = xmlCurrent_->FirstChildElement("gamma");
    if (gamma)
        tinyxml2::XMLElementToGLM( gamma->FirstChildElement("vec4"), n.gamma);
    XMLElement* levels = xmlCurrent_->FirstChildElement("levels");
    if (levels)
        tinyxml2::XMLElementToGLM( levels->FirstChildElement("vec4"), n.levels);
    XMLElement* chromakey = xmlCurrent_->FirstChildElement("chromakey");
    if (chromakey)
        tinyxml2::XMLElementToGLM( chromakey->FirstChildElement("vec4"), n.chromakey);
}

void SessionLoader::visit (Source& s)
{
    XMLElement* sourceNode = xmlCurrent_;
    const char *pName = sourceNode->Attribute("name");
    s.setName(pName);

    xmlCurrent_ = sourceNode->FirstChildElement("Mixing");
    s.groupNode(View::MIXING)->accept(*this);

    xmlCurrent_ = sourceNode->FirstChildElement("Geometry");
    s.groupNode(View::GEOMETRY)->accept(*this);

    xmlCurrent_ = sourceNode->FirstChildElement("Layer");
    s.groupNode(View::LAYER)->accept(*this);

    xmlCurrent_ = sourceNode->FirstChildElement("Blending");
    s.blendingShader()->accept(*this);

    xmlCurrent_ = sourceNode->FirstChildElement("ImageProcessing");
    bool on = xmlCurrent_->BoolAttribute("enabled", true);
    s.processingShader()->accept(*this);
    s.setImageProcessingEnabled(on);

    // restore current
    xmlCurrent_ = sourceNode;
}

void SessionLoader::visit (MediaSource& s)
{
    // set uri
    XMLElement* uriNode = xmlCurrent_->FirstChildElement("uri");
    if (uriNode) {
        std::string uri = std::string ( uriNode->GetText() );
        // load only new files
        if ( uri != s.path() )
            s.setPath(uri);
    }

    // set config media player
    s.mediaplayer()->accept(*this);
}

void SessionLoader::visit (SessionSource& s)
{
    // set uri
    XMLElement* pathNode = xmlCurrent_->FirstChildElement("path");
    if (pathNode) {
        std::string path = std::string ( pathNode->GetText() );
        // load only new files
        if ( path != s.path() )
            s.load(path);
    }

}

void SessionLoader::visit (PatternSource& s)
{
    uint t = xmlCurrent_->UnsignedAttribute("pattern");

    glm::ivec2 resolution(800, 600);
    XMLElement* res = xmlCurrent_->FirstChildElement("resolution");
    if (res)
        tinyxml2::XMLElementToGLM( res->FirstChildElement("ivec2"), resolution);

    // change only if different pattern
    if ( t != s.pattern()->type() )
        s.setPattern(t, resolution);
}

void SessionLoader::visit (DeviceSource& s)
{
    std::string devname = std::string ( xmlCurrent_->Attribute("device") );

    // change only if different device
    if ( devname != s.device() )
        s.setDevice(devname);
}


void SessionLoader::visit (NetworkSource& s)
{
    std::string connect = std::string ( xmlCurrent_->Attribute("connection") );

    // change only if different device
    if ( connect != s.connection() )
        s.setConnection(connect);
}



