#include "SessionCreator.h"

#include "Log.h"
#include "defines.h"
#include "Scene.h"
#include "Primitives.h"
#include "Mesh.h"
#include "Source.h"
#include "Session.h"
#include "ImageShader.h"
#include "ImageProcessingShader.h"
#include "MediaPlayer.h"

#include <tinyxml2.h>
using namespace tinyxml2;


SessionCreator::SessionCreator(Session *session): Visitor(), session_(session)
{
    xmlDoc_ = new XMLDocument;

}

bool SessionCreator::load(const std::string& filename)
{
    XMLError eResult = xmlDoc_->LoadFile(filename.c_str());
    if ( XMLResultError(eResult))
        return false;

    XMLElement *version = xmlDoc_->FirstChildElement(APP_NAME);
    if (version == nullptr) {
        Log::Warning("%s is not a %s session file.", filename.c_str(), APP_NAME);
        return false;
    }

    int version_major = -1, version_minor = -1;
    version->QueryIntAttribute("major", &version_major); // TODO incompatible if major is different?
    version->QueryIntAttribute("minor", &version_minor);
    if (version_major != XML_VERSION_MAJOR || version_minor != XML_VERSION_MINOR){
        Log::Warning("%s is in a different versions of session file. Loading might fail.", filename.c_str());
        return false;
    }

    // ok, ready to read sources
    loadSession( xmlDoc_->FirstChildElement("Session") );
    // excellent, session was created: load optionnal config
    if (session_){
        loadConfig( xmlDoc_->FirstChildElement("Views") );
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
            if ( std::string(pType) == "MediaSource") {
                MediaSource *new_media_source = new MediaSource();
                new_media_source->accept(*this);
                session_->addSource(new_media_source);
            }
            // TODO : create other types of source

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
        tinyxml2::XMLElementToGLM( scaleNode->FirstChildElement("vec3"), n.scale_);
        XMLElement *translationNode = node->FirstChildElement("translation");
        tinyxml2::XMLElementToGLM( translationNode->FirstChildElement("vec3"), n.translation_);
        XMLElement *rotationNode = node->FirstChildElement("rotation");
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
    tinyxml2::XMLElementToGLM( color->FirstChildElement("vec4"), n.color);

    XMLElement* blending = xmlCurrent_->FirstChildElement("blending");
    if (blending) {
        int blend = 0;
        blending->QueryIntAttribute("loop", &blend);
        n.blending = (Shader::BlendMode) blend;
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
    tinyxml2::XMLElementToGLM( gamma->FirstChildElement("vec4"), n.gamma);
    XMLElement* levels = xmlCurrent_->FirstChildElement("levels");
    tinyxml2::XMLElementToGLM( levels->FirstChildElement("vec4"), n.levels);
    XMLElement* chromakey = xmlCurrent_->FirstChildElement("chromakey");
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

    xmlCurrent_ = sourceNode->FirstChildElement("Blending");
    s.blendingShader()->accept(*this);

    xmlCurrent_ = sourceNode->FirstChildElement("ImageProcessing");
    s.processingShader()->accept(*this);

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




