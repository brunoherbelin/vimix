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
    if ( XMLResultError(eResult))
        return ret;

    XMLElement *header = doc.FirstChildElement(APP_NAME);
    if (header != nullptr && header->Attribute("date") != 0) {
        int s = header->IntAttribute("size");
        ret = std::to_string( s ) + " source" + ( s > 1 ? "s\n" : "\n");
        std::string date( header->Attribute("date") );
        ret += date.substr(6,2) + "/" + date.substr(4,2) + "/" + date.substr(0,4) + " ";
        ret += date.substr(8,2) + ":" + date.substr(10,2) + "\n";
    }

    return ret;
}

SessionCreator::SessionCreator(Session *session): Visitor(), session_(session)
{

}

SessionCreator::~SessionCreator()
{
}

bool SessionCreator::load(const std::string& filename)
{
    XMLError eResult = xmlDoc_.LoadFile(filename.c_str());
    if ( XMLResultError(eResult))
        return false;

    XMLElement *header = xmlDoc_.FirstChildElement(APP_NAME);
    if (header == nullptr) {
        Log::Warning("%s is not a %s session file.", filename.c_str(), APP_NAME);
        return false;
    }

    int version_major = -1, version_minor = -1;
    header->QueryIntAttribute("major", &version_major); // TODO incompatible if major is different?
    header->QueryIntAttribute("minor", &version_minor);
    if (version_major != XML_VERSION_MAJOR || version_minor != XML_VERSION_MINOR){
        Log::Warning("%s is in a different versions of session file. Loading might fail.", filename.c_str());
        return false;
    }

    // ok, ready to read sources
    loadSession( xmlDoc_.FirstChildElement("Session") );
    // excellent, session was created: load optionnal config
    if (session_){
        loadConfig( xmlDoc_.FirstChildElement("Views") );
    }

    return true;
}

void SessionCreator::loadSession(XMLElement *sessionNode)
{
    if (sessionNode != nullptr) {
        // create a session if not provided
        if (!session_)
            session_ = new Session;

        int counter = 0;
        XMLElement* sourceNode = sessionNode->FirstChildElement("Source");
        for( ; sourceNode ; sourceNode = sourceNode->NextSiblingElement())
        {
            xmlCurrent_ = sourceNode;
            counter++;

            const char *pType = xmlCurrent_->Attribute("type");
            if (!pType)
                continue;
            if ( std::string(pType) == "MediaSource") {
                MediaSource *new_media_source = new MediaSource;
                new_media_source->accept(*this);
                session_->addSource(new_media_source);
            }
            else if ( std::string(pType) == "SessionSource") {
                SessionSource *new_session_source = new SessionSource;
                new_session_source->accept(*this);
                session_->addSource(new_session_source);
            }
            else if ( std::string(pType) == "RenderSource") {
                RenderSource *new_render_source = new RenderSource(session_);
                new_render_source->accept(*this);
                session_->addSource(new_render_source);
            }
            else if ( std::string(pType) == "PatternSource") {
                PatternSource *new_pattern_source = new PatternSource;
                new_pattern_source->accept(*this);
                session_->addSource(new_pattern_source);
            }
            // TODO : create other types of source

        }

        // create clones after all sources to potentially clone have been created
        sourceNode = sessionNode->FirstChildElement("Source");
        for( ; sourceNode ; sourceNode = sourceNode->NextSiblingElement())
        {
            xmlCurrent_ = sourceNode;
            counter++;

            const char *pType = xmlCurrent_->Attribute("type");
            if (!pType)
                continue;

            if ( std::string(pType) == "CloneSource") {
                XMLElement* originNode = xmlCurrent_->FirstChildElement("origin");
                if (originNode) {
                    std::string sourcename = std::string ( originNode->GetText() );
                    SourceList::iterator origin = session_->find(sourcename);
                    if (origin != session_->end()) {
                        CloneSource *new_clone_source = (*origin)->clone();
                        new_clone_source->accept(*this);
                        session_->addSource(new_clone_source);
                    }
                }
            }
        }

    }
    else
        Log::Warning("Session seems empty.");
}

void SessionCreator::loadConfig(XMLElement *viewsNode)
{
    if (viewsNode != nullptr) {
        // ok, ready to read views
        SessionCreator::XMLToNode( viewsNode->FirstChildElement("Mixing"), *session_->config(View::MIXING));
        SessionCreator::XMLToNode( viewsNode->FirstChildElement("Geometry"), *session_->config(View::GEOMETRY));
        SessionCreator::XMLToNode( viewsNode->FirstChildElement("Layer"), *session_->config(View::LAYER));
        SessionCreator::XMLToNode( viewsNode->FirstChildElement("Rendering"), *session_->config(View::RENDERING));
    }
}

void SessionCreator::XMLToNode(tinyxml2::XMLElement *xml, Node &n)
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

void SessionCreator::visit(Node &n)
{
    XMLToNode(xmlCurrent_, n);
}

void SessionCreator::visit(MediaPlayer &n)
{
    XMLElement* mediaplayerNode = xmlCurrent_->FirstChildElement("MediaPlayer");
    if (mediaplayerNode) {
        // timeline
        XMLElement *timelineelement = mediaplayerNode->FirstChildElement("Timeline");
        if (timelineelement) {
            Timeline tl;
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
        // playing properties
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

void SessionCreator::visit(Shader &n)
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

void SessionCreator::visit(ImageShader &n)
{
    const char *pType = xmlCurrent_->Attribute("type");
    if ( std::string(pType) != "ImageShader" )
        return;

    XMLElement* uniforms = xmlCurrent_->FirstChildElement("uniforms");
    if (uniforms) {
        uniforms->QueryFloatAttribute("stipple", &n.stipple);
        uniforms->QueryUnsignedAttribute("mask", &n.mask);
    }
}

void SessionCreator::visit(ImageProcessingShader &n)
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

void SessionCreator::visit (Source& s)
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

void SessionCreator::visit (MediaSource& s)
{
    // set uri
    XMLElement* uriNode = xmlCurrent_->FirstChildElement("uri");
    if (uriNode) {
        std::string uri = std::string ( uriNode->GetText() );
        s.setPath(uri);
    }

    // set config media player
    s.mediaplayer()->accept(*this);
}

void SessionCreator::visit (SessionSource& s)
{
    // set uri
    XMLElement* pathNode = xmlCurrent_->FirstChildElement("path");
    if (pathNode) {
        std::string path = std::string ( pathNode->GetText() );
        s.load(path);
    }

}

void SessionCreator::visit (PatternSource& s)
{
    uint p = xmlCurrent_->UnsignedAttribute("pattern");

    glm::ivec2 resolution(800, 600);
    XMLElement* res = xmlCurrent_->FirstChildElement("resolution");
    if (res)
        tinyxml2::XMLElementToGLM( res->FirstChildElement("ivec2"), resolution);

    s.setPattern(p, resolution);
}



