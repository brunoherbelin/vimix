/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2022 Bruno Herbelin <bruno.herbelin@gmail.com>
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

#include <sstream>

#include "Log.h"
#include "defines.h"
#include "Scene.h"
#include "Primitives.h"
#include "Mesh.h"
#include "Source.h"
#include "CloneSource.h"
#include "MediaSource.h"
#include "SessionSource.h"
#include "StreamSource.h"
#include "PatternSource.h"
#include "DeviceSource.h"
#include "NetworkSource.h"
#include "MultiFileSource.h"
#include "StreamSource.h"
#include "RenderSource.h"
#include "Session.h"
#include "ImageShader.h"
#include "ImageProcessingShader.h"
#include "MediaPlayer.h"
#include "SystemToolkit.h"

#include "tinyxml2Toolkit.h"
using namespace tinyxml2;

#include "SessionCreator.h"

SessionInformation SessionCreator::info(const std::string& filename)
{
    SessionInformation ret;

    // if the file exists
    if (SystemToolkit::file_exists(filename)) {
        // impose C locale
        setlocale(LC_ALL, "C");
        // try to load the file
        XMLDocument doc;
        XMLError eResult = doc.LoadFile(filename.c_str());
        // silently ignore on error
        if ( !XMLResultError(eResult, false)) {

            const XMLElement *header = doc.FirstChildElement(APP_NAME);
            if (header != nullptr) {
                int s = header->IntAttribute("size");
                ret.description = std::to_string( s ) + " source" + ( s > 1 ? "s\n" : "\n");
                const char *att_string = header->Attribute("resolution");
                if (att_string)
                    ret.description += std::string( att_string ) + "\n";
                att_string = header->Attribute("date");
                if (att_string) {
                    std::string date( att_string );
                    ret.description += date.substr(6,2) + "/" + date.substr(4,2) + "/" + date.substr(0,4) + " @ ";
                    ret.description += date.substr(8,2) + ":" + date.substr(10,2);
                }
            }
            const XMLElement *session = doc.FirstChildElement("Session");
            if (session != nullptr ) {
                const XMLElement *thumbnailelement = session->FirstChildElement("Thumbnail");
                // if there is a user defined thumbnail, get it
                if (thumbnailelement) {
                    ret.thumbnail = XMLToImage(thumbnailelement);
                    ret.user_thumbnail_ = true;
                }
                // otherwise get the default saved thumbnail in session
                else
                    ret.thumbnail = XMLToImage(session);
            }
        }
    }

    return ret;
}

SessionCreator::SessionCreator(int recursion): SessionLoader(nullptr, recursion)
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
    header->QueryIntAttribute("major", &version_major);
    header->QueryIntAttribute("minor", &version_minor);
    if (version_major != XML_VERSION_MAJOR || version_minor != XML_VERSION_MINOR){
        Log::Warning("%s session file is in version v%d.%d. but this vimix program expects v%d.%d.\n"
                     "Loading might fail or lead to different or incomplete configuration.\n"
                     "You can save this session again to avoid this warning.",
                     filename.c_str(), version_major, version_minor, XML_VERSION_MAJOR, XML_VERSION_MINOR);
//        return;
    }

    // session file seems legit, create a session
    session_ = new Session;

    // load views config (includes resolution of session rendering)
    loadConfig( xmlDoc_.FirstChildElement("Views") );

    // ready to read sources
    sessionFilePath_ = SystemToolkit::path_filename(filename);
    XMLElement *sessionNode = xmlDoc_.FirstChildElement("Session");
    SessionLoader::load( sessionNode );

    // create groups
    std::list< SourceList > groups = getMixingGroups();
    for (auto group_it = groups.begin(); group_it != groups.end(); ++group_it)
         session_->link( *group_it );

    // load snapshots
    loadSnapshots( xmlDoc_.FirstChildElement("Snapshots") );

    // load notes
    loadNotes( xmlDoc_.FirstChildElement("Notes") );

    // load playlists
    loadPlayGroups( xmlDoc_.FirstChildElement("PlayGroups") );

    // thumbnail
    const XMLElement *thumbnailelement = sessionNode->FirstChildElement("Thumbnail");
    // if there is a user-defined thumbnail, get it
    if (thumbnailelement) {
        FrameBufferImage *thumbnail = XMLToImage(thumbnailelement);
        if (thumbnail != nullptr)
            session_->setThumbnail( thumbnail );
    }

    // all good
    session_->setFilename(filename);
}


void SessionCreator::loadConfig(XMLElement *viewsNode)
{
    if (viewsNode != nullptr && session_ != nullptr) {
        // ok, ready to read views
        SessionLoader::XMLToNode( viewsNode->FirstChildElement("Mixing"), *session_->config(View::MIXING));
        SessionLoader::XMLToNode( viewsNode->FirstChildElement("Geometry"), *session_->config(View::GEOMETRY));
        SessionLoader::XMLToNode( viewsNode->FirstChildElement("Layer"), *session_->config(View::LAYER));
        SessionLoader::XMLToNode( viewsNode->FirstChildElement("Texture"), *session_->config(View::TEXTURE));
        SessionLoader::XMLToNode( viewsNode->FirstChildElement("Rendering"), *session_->config(View::RENDERING));
    }
}

void SessionCreator::loadSnapshots(XMLElement *snapshotsNode)
{
    if (snapshotsNode != nullptr && session_ != nullptr) {

        const XMLElement* N = snapshotsNode->FirstChildElement();
        for( ; N ; N = N->NextSiblingElement()) {

            char c;
            u_int64_t id = 0;
            std::istringstream nodename( N->Name() );
            nodename >> c >> id;

            session_->snapshots()->keys_.push_back(id);
            session_->snapshots()->xmlDoc_->InsertEndChild( N->DeepClone(session_->snapshots()->xmlDoc_) );
        }
    }
}

void SessionCreator::loadNotes(XMLElement *notesNode)
{
    if (notesNode != nullptr && session_ != nullptr) {

        XMLElement* note = notesNode->FirstChildElement("Note");
        for( ; note ; note = note->NextSiblingElement())
        {
            SessionNote N;

            note->QueryBoolAttribute("large", &N.large);
            note->QueryIntAttribute("stick", &N.stick);
            XMLElement *posNode = note->FirstChildElement("pos");
            if (posNode)  tinyxml2::XMLElementToGLM( posNode->FirstChildElement("vec2"), N.pos);
            XMLElement *sizeNode = note->FirstChildElement("size");
            if (sizeNode)  tinyxml2::XMLElementToGLM( sizeNode->FirstChildElement("vec2"), N.size);
            XMLElement* contentNode = note->FirstChildElement("text");
            if (contentNode && contentNode->GetText())
                N.text = std::string ( contentNode->GetText() );

            session_->addNote(N);
        }

    }
}

void SessionCreator::loadPlayGroups(tinyxml2::XMLElement *playgroupNode)
{
    if (playgroupNode != nullptr && session_ != nullptr) {

        XMLElement* playgroup = playgroupNode->FirstChildElement("PlayGroup");
        for( ; playgroup ; playgroup = playgroup->NextSiblingElement())
        {
            SourceIdList playgroup_sources;

            XMLElement* playgroupSourceNode = playgroup->FirstChildElement("source");
            for ( ; playgroupSourceNode ; playgroupSourceNode = playgroupSourceNode->NextSiblingElement()) {
                uint64_t id__ = 0;
                playgroupSourceNode->QueryUnsigned64Attribute("id", &id__);

                if (sources_id_.count(id__) > 0)
                    playgroup_sources.push_back( id__ );

            }
            session_->addPlayGroup( playgroup_sources );
        }
    }
}

SessionLoader::SessionLoader(): Visitor(),
    session_(nullptr), xmlCurrent_(nullptr), recursion_(0)
{
    // impose C locale
    setlocale(LC_ALL, "C");
}

SessionLoader::SessionLoader(Session *session, int recursion): Visitor(),
    session_(session), xmlCurrent_(nullptr), recursion_(recursion)
{
    // impose C locale
    setlocale(LC_ALL, "C");
}


std::map< uint64_t, Source* > SessionLoader::getSources() const
{
    return sources_id_;
}

// groups_sources_id_ is parsed in XML and contains list of groups of ids
// Here we return the list of groups of newly created sources
// based on correspondance map sources_id_
// NB: importantly the list is cleared from duplicates
std::list< SourceList > SessionLoader::getMixingGroups() const
{
    std::list< SourceList > groups_new_sources_id;

    // perform conversion from xml id to new id
    for (auto git = groups_sources_id_.begin(); git != groups_sources_id_.end(); ++git)
    {
        SourceList new_sources;
        for (auto sit = (*git).begin(); sit != (*git).end(); ++sit ) {
            if (sources_id_.count(*sit) > 0)
                new_sources.push_back( sources_id_.at(*sit) );
        }
        new_sources.sort();
        groups_new_sources_id.push_back( new_sources );
    }

    // remove duplicates
    groups_new_sources_id.unique();

    return groups_new_sources_id;
}

void SessionLoader::load(XMLElement *sessionNode)
{
    sources_id_.clear();

    if (recursion_ > MAX_SESSION_LEVEL) {
        Log::Warning("Recursive or imbricated sessions detected! Interrupting loading after %d iterations.\n", MAX_SESSION_LEVEL);
        return;
    }

    if (sessionNode != nullptr && session_ != nullptr)
    {
        //
        // session attributes
        //
        float t = MIXING_MIN_THRESHOLD;
        sessionNode->QueryFloatAttribute("activationThreshold", &t);
        session_->setActivationThreshold(t);

        //
        // source lists
        //
        XMLElement* sourceNode = sessionNode->FirstChildElement("Source");
        for( ; sourceNode ; sourceNode = sourceNode->NextSiblingElement())
        {
            xmlCurrent_ = sourceNode;

            // source to load
            Source *load_source = nullptr;

            // check if a source with the given id exists in the session
            uint64_t id_xml_ = 0;
            xmlCurrent_->QueryUnsigned64Attribute("id", &id_xml_);
            SourceList::iterator sit = session_->find(id_xml_);

            // no source with this id exists
            if ( sit == session_->end() ) {
                // create a new source depending on type
                const char *pType = xmlCurrent_->Attribute("type");
                if (!pType)
                    continue;
                if ( std::string(pType) == "MediaSource") {
                    load_source = new MediaSource(id_xml_);
                }
                else if ( std::string(pType) == "SessionSource") {
                    load_source = new SessionFileSource(id_xml_);
                }
                else if ( std::string(pType) == "GroupSource") {
                    load_source = new SessionGroupSource(id_xml_);
                }
                else if ( std::string(pType) == "RenderSource") {
                    load_source = new RenderSource(id_xml_);
                }
                else if ( std::string(pType) == "PatternSource") {
                    load_source = new PatternSource(id_xml_);
                }
                else if ( std::string(pType) == "DeviceSource") {
                    load_source = new DeviceSource(id_xml_);
                }
                else if ( std::string(pType) == "NetworkSource") {
                    load_source = new NetworkSource(id_xml_);
                }
                else if ( std::string(pType) == "MultiFileSource") {
                    load_source = new MultiFileSource(id_xml_);
                }
                else if ( std::string(pType) == "GenericStreamSource") {
                    load_source = new GenericStreamSource(id_xml_);
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
            sources_id_[id_xml_] = load_source;
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
                uint64_t id_xml_ = 0;
                xmlCurrent_->QueryUnsigned64Attribute("id", &id_xml_);
                SourceList::iterator sit = session_->find(id_xml_);

                // no source clone with this id exists
                if ( sit == session_->end() ) {

                    // clone from given origin
                    XMLElement* originNode = xmlCurrent_->FirstChildElement("origin");
                    if (originNode) {
                        uint64_t id_origin_ = 0;
                        originNode->QueryUnsigned64Attribute("id", &id_origin_);
                        SourceList::iterator origin;
                        if (id_origin_ > 0)
                            origin = session_->find(id_origin_);
                        else {
                            const char *text = originNode->GetText();
                            if (text)
                                origin = session_->find( std::string(text) );
                            else
                                origin = session_->end();
                        }
                        // found the orign source
                        if (origin != session_->end()) {
                            // create a new source of type Clone
                            CloneSource *clone_source = (*origin)->clone(id_xml_);

                            // add source to session
                            session_->addSource(clone_source);

                            // apply config to source
                            clone_source->accept(*this);
                            clone_source->touch();

                            // remember
                            sources_id_[id_xml_] = clone_source;
                        }
                    }
                }
            }
        }

        // loop over SourceLinks and resolve them
        // NB: this could become the mechanism for clone sources too

    }
}

Source *SessionLoader::createSource(tinyxml2::XMLElement *sourceNode, Mode mode)
{
    xmlCurrent_ = sourceNode;

    // source to load
    Source *load_source = nullptr;
    bool is_clone = false;

    uint64_t id__ = 0;
    xmlCurrent_->QueryUnsigned64Attribute("id", &id__);

    // check if a source with the given id exists in the session
    SourceList::iterator sit = session_->end();
    if (mode == CLONE) {
        sit = session_->find(id__);
    }

    // no source with this id exists or Mode DUPLICATE
    if ( sit == session_->end() ) {
        // create a new source depending on type
        const char *pType = xmlCurrent_->Attribute("type");
        if (pType) {
            if ( std::string(pType) == "MediaSource") {
                load_source = new MediaSource(id__);
            }
            else if ( std::string(pType) == "SessionSource") {
                load_source = new SessionFileSource(id__);
            }
            else if ( std::string(pType) == "GroupSource") {
                load_source = new SessionGroupSource(id__);
            }
            else if ( std::string(pType) == "RenderSource") {
                load_source = new RenderSource(id__);
            }
            else if ( std::string(pType) == "PatternSource") {
                load_source = new PatternSource(id__);
            }
            else if ( std::string(pType) == "DeviceSource") {
                load_source = new DeviceSource(id__);
            }
            else if ( std::string(pType) == "NetworkSource") {
                load_source = new NetworkSource(id__);
            }
            else if ( std::string(pType) == "MultiFileSource") {
                load_source = new MultiFileSource(id__);
            }
            else if ( std::string(pType) == "GenericStreamSource") {
                load_source = new GenericStreamSource(id__);
            }
            else if ( std::string(pType) == "CloneSource") {
                // clone from given origin
                XMLElement* originNode = xmlCurrent_->FirstChildElement("origin");
                if (originNode) {
                    uint64_t id_origin_ = 0;
                    originNode->QueryUnsigned64Attribute("id", &id_origin_);
                    SourceList::iterator origin;
                    if (id_origin_ > 0)
                        origin = session_->find(id_origin_);
                    else
                        origin = session_->find( std::string ( originNode->GetText() ) );
                    // found the orign source
                    if (origin != session_->end())
                        load_source = (*origin)->clone(id__);
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
        // increment depth for clones (avoid supperposition)
        if (is_clone)
            load_source->group(View::LAYER)->translation_.z += 0.2f;
    }

    return load_source;
}


bool SessionLoader::isClipboard(const std::string &clipboard)
{
    if (clipboard.size() > 6 && clipboard.substr(0, 6) == "<" APP_NAME )
        return true;

    return false;
}

tinyxml2::XMLElement* SessionLoader::firstSourceElement(const std::string &clipboard, XMLDocument &xmlDoc)
{
    tinyxml2::XMLElement* sourceNode = nullptr;

    if ( !isClipboard(clipboard) )
        return sourceNode;

    // header
    tinyxml2::XMLError eResult = xmlDoc.Parse(clipboard.c_str());
    if ( XMLResultError(eResult))
        return sourceNode;

    tinyxml2::XMLElement *root = xmlDoc.FirstChildElement(APP_NAME);
    if ( root == nullptr )
        return sourceNode;

    // find node
    sourceNode = root->FirstChildElement("Source");
    return sourceNode;
}

void SessionLoader::applyImageProcessing(const Source &s, const std::string &clipboard)
{
    if ( !isClipboard(clipboard) )
        return;

    // header
    tinyxml2::XMLDocument xmlDoc;
    tinyxml2::XMLError eResult = xmlDoc.Parse(clipboard.c_str());
    if ( XMLResultError(eResult))
        return;

    tinyxml2::XMLElement *root = xmlDoc.FirstChildElement(APP_NAME);
    if ( root == nullptr )
        return;

    // find node
    tinyxml2::XMLElement* imgprocNode = nullptr;
    tinyxml2::XMLElement* sourceNode = root->FirstChildElement("Source");
    if (sourceNode == nullptr)
        imgprocNode = root->FirstChildElement("ImageProcessing");
    else
        imgprocNode = sourceNode->FirstChildElement("ImageProcessing");

    if (imgprocNode == nullptr)
        return;

    // create session visitor and browse
    SessionLoader loader;
    loader.xmlCurrent_ = imgprocNode;
    s.processingShader()->accept(loader);
}

//void SessionLoader::applyMask(const Source &s, std::string clipboard)
//{
//    if ( !isClipboard(clipboard) )
//        return;

//    // header
//    tinyxml2::XMLDocument xmlDoc;
//    tinyxml2::XMLError eResult = xmlDoc.Parse(clipboard.c_str());
//    if ( XMLResultError(eResult))
//        return;

//    tinyxml2::XMLElement *root = xmlDoc.FirstChildElement(APP_NAME);
//    if ( root == nullptr )
//        return;

//    // find node
//    tinyxml2::XMLElement* naskNode = nullptr;
//    tinyxml2::XMLElement* sourceNode = root->FirstChildElement("Source");
//    if (sourceNode == nullptr)
//        naskNode = root->FirstChildElement("Mask");
//    else
//        naskNode = sourceNode->FirstChildElement("ImageProcessing");

//    if (naskNode == nullptr)
//        return;

//    // create session visitor and browse
//    SessionLoader loader;
//    loader.xmlCurrent_ = naskNode;
////    s.processingShader()->accept(loader);
//}

void SessionLoader::XMLToNode(const tinyxml2::XMLElement *xml, Node &n)
{
    if (xml != nullptr){
        const XMLElement *node = xml->FirstChildElement("Node");
        if ( !node || std::string(node->Name()).find("Node") == std::string::npos )
            return;

        const XMLElement *scaleNode = node->FirstChildElement("scale");
        if (scaleNode)
            tinyxml2::XMLElementToGLM( scaleNode->FirstChildElement("vec3"), n.scale_);
        const XMLElement *translationNode = node->FirstChildElement("translation");
        if (translationNode)
            tinyxml2::XMLElementToGLM( translationNode->FirstChildElement("vec3"), n.translation_);
        const XMLElement *rotationNode = node->FirstChildElement("rotation");
        if (rotationNode)
            tinyxml2::XMLElementToGLM( rotationNode->FirstChildElement("vec3"), n.rotation_);
        const XMLElement *cropNode = node->FirstChildElement("crop");
        if (cropNode)
            tinyxml2::XMLElementToGLM( cropNode->FirstChildElement("vec3"), n.crop_);
    }
}

void SessionLoader::XMLToSourcecore(tinyxml2::XMLElement *xml, SourceCore &s)
{
    SessionLoader::XMLToNode(xml->FirstChildElement("Mixing"),  *s.group(View::MIXING) );
    SessionLoader::XMLToNode(xml->FirstChildElement("Geometry"),*s.group(View::GEOMETRY) );
    SessionLoader::XMLToNode(xml->FirstChildElement("Layer"),   *s.group(View::LAYER) );
    SessionLoader::XMLToNode(xml->FirstChildElement("Texture"), *s.group(View::TEXTURE) );

    SessionLoader v(nullptr);
    v.xmlCurrent_ = xml->FirstChildElement("ImageProcessing");
    if (v.xmlCurrent_)
        s.processingShader()->accept(v);

}

FrameBufferImage *SessionLoader::XMLToImage(const XMLElement *xml)
{
    FrameBufferImage *i = nullptr;

    if (xml != nullptr){
        // if there is an Image mask stored
        const XMLElement* imageNode = xml->FirstChildElement("Image");
        if (imageNode) {
            // get theoretical image size
            int w = 0, h = 0;
            imageNode->QueryIntAttribute("width", &w);
            imageNode->QueryIntAttribute("height", &h);
            // if there is an internal array of data
            const XMLElement* array = imageNode->FirstChildElement("array");
            if (array) {
                // create a temporary jpeg with size of the array
                FrameBufferImage::jpegBuffer jpgimg;
                array->QueryUnsignedAttribute("len", &jpgimg.len);
                // ok, we got a size of data to load
                if (jpgimg.len>0) {
                    // allocate jpeg buffer
                    jpgimg.buffer = (unsigned char*) malloc(jpgimg.len);
                    // actual decoding of array
                    if (XMLElementDecodeArray(array, jpgimg.buffer, jpgimg.len) ) {
                        // create and set the image from jpeg
                        i = new FrameBufferImage(jpgimg);
                        // failed if wrong size
                        if ( (w>0 && h>0) && (i->width != w || i->height != h) ) {
                            delete i;
                            i = nullptr;
                        }
                    }
                    // free temporary buffer
                    if (jpgimg.buffer)
                        free(jpgimg.buffer);
                }
            }
        }
    }

    return i;
}

void SessionLoader::visit(Node &n)
{
    XMLToNode(xmlCurrent_, n);
}

void SessionLoader::visit(MediaPlayer &n)
{
    XMLElement* mediaplayerNode = xmlCurrent_->FirstChildElement("MediaPlayer");

    if (mediaplayerNode) {
        uint64_t id__ = -1;
        mediaplayerNode->QueryUnsigned64Attribute("id", &id__);

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

            bool gpudisable = false;
            mediaplayerNode->QueryBoolAttribute("software_decoding", &gpudisable);
            n.setSoftwareDecodingForced(gpudisable);

            bool rewind_on_disabled = false;
            mediaplayerNode->QueryBoolAttribute("rewind_on_disabled", &rewind_on_disabled);
            n.setRewindOnDisabled(rewind_on_disabled);

            int sync_to_metronome = 0;
            mediaplayerNode->QueryIntAttribute("sync_to_metronome", &sync_to_metronome);
            n.setSyncToMetronome( (Metronome::Synchronicity) sync_to_metronome);

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
    }
}

void SessionLoader::visit(MaskShader &n)
{
    const char *pType = xmlCurrent_->Attribute("type");
    if ( std::string(pType) != "MaskShader" )
        return;

    xmlCurrent_->QueryUnsignedAttribute("mode", &n.mode);
    xmlCurrent_->QueryUnsignedAttribute("shape", &n.shape);

    XMLElement* uniforms = xmlCurrent_->FirstChildElement("uniforms");
    if (uniforms) {
        uniforms->QueryFloatAttribute("blur", &n.blur);
        uniforms->QueryIntAttribute("option", &n.option);
        XMLElement* size = uniforms->FirstChildElement("size");
        if (size)
            tinyxml2::XMLElementToGLM( size->FirstChildElement("vec2"), n.size);
    }
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
    bool l = false;
    sourceNode->QueryBoolAttribute("locked", &l);
    s.setLocked(l);

    xmlCurrent_ = sourceNode->FirstChildElement("Mixing");
    if (xmlCurrent_) s.groupNode(View::MIXING)->accept(*this);

    xmlCurrent_ = sourceNode->FirstChildElement("Geometry");
    if (xmlCurrent_) s.groupNode(View::GEOMETRY)->accept(*this);

    xmlCurrent_ = sourceNode->FirstChildElement("Layer");
    if (xmlCurrent_) s.groupNode(View::LAYER)->accept(*this);

    xmlCurrent_ = sourceNode->FirstChildElement("Texture");
    if (xmlCurrent_) {
        s.groupNode(View::TEXTURE)->accept(*this);
        bool m = true;
        xmlCurrent_->QueryBoolAttribute("mirrored", &m);
        s.setTextureMirrored(m);
    }

    xmlCurrent_ = sourceNode->FirstChildElement("Blending");
    if (xmlCurrent_)
        s.blendingShader()->accept(*this);

    xmlCurrent_ = sourceNode->FirstChildElement("Mask");
    if (xmlCurrent_)  {
        // read the mask shader attributes
        s.maskShader()->accept(*this);
        // set the mask from jpeg
        s.setMask( SessionLoader::XMLToImage(xmlCurrent_) );
    }

    xmlCurrent_ = sourceNode->FirstChildElement("ImageProcessing");
    if (xmlCurrent_) {
        bool on = xmlCurrent_->BoolAttribute("enabled", true);
        uint64_t id__ = 0;
        xmlCurrent_->QueryUnsigned64Attribute("follow", &id__);
        s.processingShader()->accept(*this);
        s.setImageProcessingEnabled(on);
        s.processingshader_link_.connect(id__, session_);
    }

    xmlCurrent_ = sourceNode->FirstChildElement("MixingGroup");
    if (xmlCurrent_) {
        SourceIdList idlist;
        XMLElement* mixingSourceNode = xmlCurrent_->FirstChildElement("source");
        for ( ; mixingSourceNode ; mixingSourceNode = mixingSourceNode->NextSiblingElement()) {
            uint64_t id__ = 0;
            mixingSourceNode->QueryUnsigned64Attribute("id", &id__);
            idlist.push_back(id__);
        }
        groups_sources_id_.push_back(idlist);
    }

    // restore current
    xmlCurrent_ = sourceNode;
}

void SessionLoader::visit (MediaSource& s)
{
    // set uri
    XMLElement* pathNode = xmlCurrent_->FirstChildElement("uri"); // TODO change to "path" but keep backward compatibility
    if (pathNode) {
        const char * text = pathNode->GetText();
        if (text) {
            std::string path(text);
            // load only new files
            if ( path.compare(s.path()) != 0 ) {
                if ( !SystemToolkit::file_exists(path)){
                    const char * relative;
                    if ( pathNode->QueryStringAttribute("relative", &relative) == XML_SUCCESS) {
                        std::string rel = SystemToolkit::path_absolute_from_path(std::string( relative ), sessionFilePath_);
                        Log::Info("File %s not found; Trying %s instead.", path.c_str(), rel.c_str());
                        path = rel;
                    }
                }
                s.setPath(path);
            }
        }
        // ensures the source is initialized even if no valid path is given
        else
            s.setPath("");
    }

    // set config media player
    s.mediaplayer()->accept(*this);
}

void SessionLoader::visit (SessionFileSource& s)
{
    // set fading
    float f = 0.f;
    xmlCurrent_->QueryFloatAttribute("fading", &f);
    s.session()->setFadingTarget(f);
    // set uri
    XMLElement* pathNode = xmlCurrent_->FirstChildElement("path");
    if (pathNode) {
        const char * text = pathNode->GetText();
        if (text) {
            std::string path(text);
            // load only new files
            if ( path != s.path() ) {
                if ( !SystemToolkit::file_exists(path)){
                    const char * relative;
                    if ( pathNode->QueryStringAttribute("relative", &relative) == XML_SUCCESS) {
                        std::string rel = SystemToolkit::path_absolute_from_path(std::string( relative ), sessionFilePath_);
                        Log::Info("File %s not found; Trying %s instead.", path.c_str(), rel.c_str());
                        path = rel;
                    }
                }
                s.load(path, recursion_ + 1);
            }
        }
    }

}

void SessionLoader::visit (SessionGroupSource& s)
{
    // set resolution from host session
    s.setResolution( session_->config(View::RENDERING)->scale_ );

    // get the inside session
    XMLElement* sessionGroupNode = xmlCurrent_->FirstChildElement("Session");
    if (sessionGroupNode) {
        // only parse if newly created
        if (s.session()->empty()) {
            // load session inside group
            SessionLoader grouploader( s.session(), recursion_ + 1 );
            grouploader.load( sessionGroupNode );
        }
    }
}

void SessionLoader::visit (RenderSource& s)
{
    // set attributes
    int mode = 0;
    xmlCurrent_->QueryIntAttribute("renderMode", &mode);
    s.setRenderMode((RenderSource::RenderSourceMode)mode);

    // set session
    s.setSession( session_ );
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


void SessionLoader::visit (MultiFileSource& s)
{
    XMLElement* seq = xmlCurrent_->FirstChildElement("Sequence");

    if (seq) {

        MultiFileSequence sequence;

        const char *text = seq->GetText();
        if (text) {
            sequence.location = std::string (text);

            // fix path if absolute path is not found
            std::string folder = SystemToolkit::path_filename(sequence.location);
            std::string dir = SystemToolkit::path_directory(folder);
            if ( dir.empty() ){
                const char * relative;
                if ( seq->QueryStringAttribute("relative", &relative) == XML_SUCCESS) {
                    std::string rel = SystemToolkit::path_absolute_from_path(std::string(relative), sessionFilePath_);
                    Log::Info("Folder %s not found; Trying %s instead.", folder.c_str(), rel.c_str());
                    sequence.location = rel;
                }
            }

            // set sequence parameters
            seq->QueryIntAttribute("min", &sequence.min);
            seq->QueryIntAttribute("max", &sequence.max);
            seq->QueryUnsignedAttribute("width", &sequence.width);
            seq->QueryUnsignedAttribute("height", &sequence.height);
            const char *codec = seq->Attribute("codec");
            if (codec)
                sequence.codec = std::string(codec);

            uint fps = 0;
            seq->QueryUnsignedAttribute("fps", &fps);

            // different sequence
            if ( sequence != s.sequence() ) {
                s.setSequence( sequence, fps);
            }
            // same sequence, different framerate
            else if ( fps != s.framerate() )  {
                s.setFramerate( fps );
            }

            int begin = -1;
            seq->QueryIntAttribute("begin", &begin);
            int end = INT_MAX;
            seq->QueryIntAttribute("end", &end);
            if ( begin != s.begin() || end != s.end() )
                s.setRange(begin, end);

            bool loop = true;
            seq->QueryBoolAttribute("loop", &loop);
            if ( loop != s.loop() )
                s.setLoop(loop);
        }
    }

}

void SessionLoader::visit (GenericStreamSource& s)
{
    XMLElement* desc = xmlCurrent_->FirstChildElement("Description");

    if (desc) {
        const char * text = desc->GetText();
        if (text)
            s.setDescription(text);
    }
}


void SessionLoader::visit (CloneSource& s)
{
    // set attributes
    int imagemode = 0;
    xmlCurrent_->QueryIntAttribute("imageMode", &imagemode);
    s.setImageMode((CloneSource::CloneImageMode)imagemode);

    double delay = 0.0;
    xmlCurrent_->QueryDoubleAttribute("delay", &delay);
    s.setDelay(delay);
}

