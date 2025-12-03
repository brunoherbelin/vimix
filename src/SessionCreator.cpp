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

#include <glib.h>
#include <glm/fwd.hpp>
#include <sstream>
#include <algorithm>

#include "Log.h"
#include "defines.h"
#include "Source/Source.h"
#include "Source/SourceCallback.h"
#include "Source/CloneSource.h"
#include "Filter/FrameBufferFilter.h"
#include "Filter/DelayFilter.h"
#include "Filter/ImageFilter.h"
#include "Source/MediaSource.h"
#include "Source/SessionSource.h"
#include "Source/StreamSource.h"
#include "Source/PatternSource.h"
#include "Source/DeviceSource.h"
#include "Source/ScreenCaptureSource.h"
#include "Source/NetworkSource.h"
#include "Source/SrtReceiverSource.h"
#include "Source/MultiFileSource.h"
#include "Source/TextSource.h"
#include "Source/RenderSource.h"
#include "Session.h"
#include "ImageShader.h"
#include "ImageProcessingShader.h"
#include "MediaPlayer.h"
#include "Toolkit/SystemToolkit.h"
#include "Visitor/SessionVisitor.h"
#include "Source/ShaderSource.h"

#include "Toolkit/tinyxml2Toolkit.h"
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
                uint s = header->UnsignedAttribute("size");
                ret.description = std::to_string( s ) + " source" + ( s > 1 ? "s" : "");
                uint t = header->UnsignedAttribute("total");
                if (t>s)
                    ret.description += " (" + std::to_string(t) + " in total)";
                ret.description += "\n";
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

SessionCreator::SessionCreator(uint level): SessionLoader(nullptr, level)
{

}

void SessionCreator::load(const std::string& filename)
{
    // avoid imbricated sessions at too many depth
    if (level_ > MAX_SESSION_LEVEL) {
        Log::Warning("Too many imbricated sessions detected! Interrupting loading at depth %d.\n", MAX_SESSION_LEVEL);
        return;
    }

    // Load XML document
    XMLError eResult = xmlDoc_.LoadFile(filename.c_str());
    if ( XMLResultError(eResult)){
        Log::Warning("%s could not be opened.\n%s", filename.c_str(), xmlDoc_.ErrorStr());
        return;
    }

    // check header
    XMLElement *header = xmlDoc_.FirstChildElement(APP_NAME);
    if (header == nullptr) {
        Log::Warning("%s is not a %s file.", filename.c_str(), APP_NAME);
        return;
    }

    // check version
    int version_major = -1, version_minor = -1;
    header->QueryIntAttribute("major", &version_major);
    header->QueryIntAttribute("minor", &version_minor);
    if (version_major > XML_VERSION_MAJOR || version_minor > XML_VERSION_MINOR){
        Log::Warning("%s is in a newer version of vimix session (v%d.%d instead of v%d.%d).\n"
                     "Loading might lead to different or incomplete configuration.\n"
                     "Save the session to avoid further warning.",
                     filename.c_str(), version_major, version_minor, XML_VERSION_MAJOR, XML_VERSION_MINOR);
//        return;
    }

    // check content
    XMLElement *sessionNode = xmlDoc_.FirstChildElement("Session");
    if (sessionNode == nullptr) {
        Log::Warning("%s is not a %s session file.", filename.c_str(), APP_NAME);
        return;
    }

    // read id of session
    uint64_t id__ = 0;
    sessionNode->QueryUnsigned64Attribute("id", &id__);

    // session file seems legit, create a session
    session_ = new Session(id__);

    // set filename and current path
    sessionFilePath_ = SystemToolkit::path_filename(filename);
    session_->setFilename( filename );

    // load views config (includes resolution of session rendering)
    loadConfig( xmlDoc_.FirstChildElement("Views") );

    // ready to read sources
    SessionLoader::load( sessionNode );

    // load input callbacks (LEGACY - to be removed in future versions)
    SessionLoader::loadInputCallbacks( xmlDoc_.FirstChildElement("InputCallbacks") );

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
    Log::Info("Session %s Opened '%s' (%d sources)", std::to_string(session_->id()).c_str(),
              filename.c_str(), session_->size());

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

            session_->snapshots()->access_.lock();
            session_->snapshots()->keys_.push_back(id);
            session_->snapshots()->xmlDoc_->InsertEndChild( N->DeepClone(session_->snapshots()->xmlDoc_) );
            session_->snapshots()->access_.unlock();
        }
    }
}


void SessionLoader::loadInputCallbacks(tinyxml2::XMLElement *inputsNode)
{
    if (inputsNode != nullptr && session_ != nullptr) {

        // read all 'Callback' nodes
        xmlCurrent_ = inputsNode->FirstChildElement("Callback");
        for ( ; xmlCurrent_ ; xmlCurrent_ = xmlCurrent_->NextSiblingElement()) {
            // what key triggers the callback ?
            uint input = 0;
            xmlCurrent_->QueryUnsignedAttribute("input", &input);
            if (input > 0) {
                Target target;
                uint64_t sid = 0;
                uint64_t bid = 0;
                if ( xmlCurrent_->QueryUnsigned64Attribute("id", &sid) != XML_NO_ATTRIBUTE ) {
                    // find the source with the given id
                    SourceList::iterator sit = session_->find(sid);
                    if (sit != session_->end()) {
                        // assign variant
                        target = *sit;
                    }
                }
                else if ( xmlCurrent_->QueryUnsigned64Attribute("batch", &bid) != XML_NO_ATTRIBUTE ) {
                    // assign variant
                    target = (size_t) bid;
                }

                // if could identify the target
                if (target.index() > 0) {
                    // what type is the callback ?
                    uint type = 0;
                    xmlCurrent_->QueryUnsignedAttribute("type", &type);
                    // instanciate the callback of that type
                    SourceCallback *loadedcallback = SourceCallback::create((SourceCallback::CallbackType)type);
                    // successfully created a callback of saved type
                    if (loadedcallback) {
                        // apply specific parameters
                        loadedcallback->accept(*this);
                        // assign to target
                        session_->assignInputCallback(input, target, loadedcallback);
                    }
                }
            }
        }

        // read array of synchronyzation mode for all inputs (CSV)
        xmlCurrent_ = inputsNode->FirstChildElement("Synchrony");
        if (xmlCurrent_) {
            const char *text = xmlCurrent_->GetText();
            if (text) {
                std::istringstream iss(text);
                std::string token;
                uint i = 0;
                while(std::getline(iss, token, ';')) {
                    if (token.compare("1") == 0)
                        session_->setInputSynchrony(i, Metronome::SYNC_BEAT);
                    else if (token.compare("2") == 0)
                        session_->setInputSynchrony(i, Metronome::SYNC_PHASE);
                    ++i;
                }
            }
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
            session_->addBatch( playgroup_sources );
        }
    }
}

SessionLoader::SessionLoader(Session *session, uint level): Visitor(),
    session_(session), xmlCurrent_(nullptr), level_(level)
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

    if (sessionNode != nullptr && session_ != nullptr)
    {
        //
        // session attributes
        //
        // read and set activation threshold
        float t = MIXING_MIN_THRESHOLD;
        sessionNode->QueryFloatAttribute("activationThreshold", &t);
        session_->setActivationThreshold(t);

        std::list<XMLElement*> cloneNodesToAdd;
        std::map<int, uint64_t> loaded_xml_ids;

        //
        // source lists
        //
        XMLElement* sourceNode = sessionNode->FirstChildElement("Source");
        for(int i = 0 ; sourceNode ; sourceNode = sourceNode->NextSiblingElement("Source"))
        {
            xmlCurrent_ = sourceNode;

            // source to load
            Source *load_source = nullptr;

            // read the xml id of this source element
            uint64_t id_xml_ = 0;
            xmlCurrent_->QueryUnsigned64Attribute("id", &id_xml_);

            // check if a source with the given id exists in the session
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
                else if ( std::string(pType) == "ScreenCaptureSource") {
                    load_source = new ScreenCaptureSource(id_xml_);
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
                else if ( std::string(pType) == "SrtReceiverSource") {
                    load_source = new SrtReceiverSource(id_xml_);
                }
                else if ( std::string(pType) == "TextSource") {
                    load_source = new TextSource(id_xml_);
                }
                else if ( std::string(pType) == "ShaderSource") {
                    load_source = new ShaderSource(id_xml_);
                }
                else if ( std::string(pType) == "CloneSource") {
                    cloneNodesToAdd.push_front(xmlCurrent_);
                    continue;
                }
                else {
                    Log::Info("Unknown source type '%s' ignored.", pType);
                    continue;
                }

                // and store id as loaded
                loaded_xml_ids[i++] = id_xml_;

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

        // take all node elements for Clones to add
        while ( !cloneNodesToAdd.empty() ) {

            // take out Clone's XML element
            xmlCurrent_ = cloneNodesToAdd.front();
            cloneNodesToAdd.pop_front();

            // check if a clone source with same id exists
            uint64_t id_xml_ = 0;
            xmlCurrent_->QueryUnsigned64Attribute("id", &id_xml_);
            SourceList::iterator sit = session_->find(id_xml_);

            // no source clone with this id exists
            if ( sit == session_->end() ) {

                // try to get clone from given origin
                XMLElement* originNode = xmlCurrent_->FirstChildElement("origin");
                if (originNode) {

                    // find origin by id
                    uint64_t id_origin_ = 0;
                    originNode->QueryUnsigned64Attribute("id", &id_origin_);
                    SourceList::iterator origin;
                    if (id_origin_ > 0)
                        origin = session_->find(id_origin_);
                    // legacy: find origin by name
                    else {
                        const char *text = originNode->GetText();
                        if (text)
                            origin = session_->find( std::string(text) );
                        else
                            origin = session_->end();
                    }

                    // found the orign source!
                    if (origin != session_->end()) {
                        // create a new source of type Clone
                        CloneSource *clone_source = (*origin)->clone(id_xml_);

                        // add clone source to session
                        session_->addSource(clone_source);

                        // apply config to clone source
                        clone_source->accept(*this);
                        clone_source->touch();

                        // remember
                        sources_id_[id_xml_] = clone_source;
                    }
                    // origin not found, retry later
                    else {
                        // sanity check: does the given id even exists in the loaded list?
                        auto id_found = std::find_if(
                                  loaded_xml_ids.begin(),
                                  loaded_xml_ids.end(),
                                  [id_origin_](const auto& mo) {return mo.second == id_origin_; });
                        // keep trying to insert the clone only if the id exists
                        if(id_found != loaded_xml_ids.end())
                            cloneNodesToAdd.push_back(xmlCurrent_);
                    }
                }
            }
        }

        //
        // create groups
        //
        std::list< SourceList > groups = getMixingGroups();
        for (auto group_it = groups.begin(); group_it != groups.end(); ++group_it)
             session_->link( *group_it );

        //
        // reorder according to index order as originally loaded
        //
        for ( auto id = loaded_xml_ids.begin(); id != loaded_xml_ids.end(); ++id)
            session_->move( session_->index( session_->find( id->second)), id->first );


        // load input callbacks
        loadInputCallbacks( sessionNode->FirstChildElement("InputCallbacks") );

    }
}


Source *SessionLoader::recreateSource(Source *s)
{
    if ( s == nullptr || session_ == nullptr )
        return nullptr;

    // get the xml description from this source, and exit if not wellformed
    tinyxml2::XMLDocument xmlDoc;
    std::string clip = SessionVisitor::getClipboard(s);
    //g_printerr("clibboard\n%s", clip.c_str());

    // find XML desc of source
    tinyxml2::XMLElement* sourceNode = SessionLoader::firstSourceElement(clip, xmlDoc);
    if ( sourceNode == nullptr )
        return nullptr;

    // create loader
    SessionLoader loader( session_ );

    // actually create the source with SessionLoader using xml description
    return loader.createSource(sourceNode, SessionLoader::REPLACE); // not clone
}

Source *SessionLoader::createSource(tinyxml2::XMLElement *sourceNode, Mode mode)
{
    xmlCurrent_ = sourceNode;

    // source to load
    Source *load_source = nullptr;

    uint64_t id__ = 0;
    xmlCurrent_->QueryUnsigned64Attribute("id", &id__);

    // for CLONE, find the source with the given id in the session
    SourceList::iterator sit = session_->end();
    if (mode == CLONE)
        sit = session_->find(id__);

    // no source with this id exists or Mode DUPLICATE
    if ( sit == session_->end() ) {
        // for DUPLICATE, a new id should be given
        if (mode == DUPLICATE)
            id__ = 0;

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
            else if ( std::string(pType) == "ScreenCaptureSource") {
                load_source = new ScreenCaptureSource(id__);
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
            else if ( std::string(pType) == "SrtReceiverSource") {
                load_source = new SrtReceiverSource(id__);
            }
            else if ( std::string(pType) == "TextSource") {
                load_source = new TextSource(id__);
            }
            else if ( std::string(pType) == "ShaderSource") {
                load_source = new ShaderSource(id__);
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
    }

    // apply config to source
    if (load_source) {
        load_source->accept(*this);
        if (mode != REPLACE)
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
        if (cropNode) {
            const XMLElement *vecNode = cropNode->FirstChildElement("vec4");
            if (vecNode)
                tinyxml2::XMLElementToGLM(vecNode, n.crop_);
            else {
                // backward compatibility, read vec3
                vecNode = cropNode->FirstChildElement("vec3");
                if (vecNode) {
                    glm::vec3 crop;
                    tinyxml2::XMLElementToGLM(vecNode, crop);
                    n.crop_ = glm::vec4(-crop.x, crop.x, crop.y, -crop.y);
                }
            }
        }
        const XMLElement *dataNode = node->FirstChildElement("data");
        if (dataNode)
            tinyxml2::XMLElementToGLM( dataNode->FirstChildElement("mat4"), n.data_);
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
        uint64_t id__ = 0;
        mediaplayerNode->QueryUnsigned64Attribute("id", &id__);

        // timeline
        XMLElement *timelineelement = mediaplayerNode->FirstChildElement("Timeline");
        if (timelineelement) {
            Timeline tl;

            TimeInterval interval_(n.timeline()->interval());
            if (interval_.is_valid())
                tl.setTiming( interval_, n.timeline()->step());
            else
            {
                uint64_t b = GST_CLOCK_TIME_NONE;
                uint64_t e = GST_CLOCK_TIME_NONE;
                uint64_t s = GST_CLOCK_TIME_NONE;
                timelineelement->QueryUnsigned64Attribute("begin", &b);
                timelineelement->QueryUnsigned64Attribute("end", &e);
                timelineelement->QueryUnsigned64Attribute("step", &s);
                interval_ = TimeInterval( (GstClockTime) b, (GstClockTime) e);
                if (interval_.is_valid())
                    tl.setTiming( interval_, (GstClockTime) s);
            }

            XMLElement *gapselement = timelineelement->FirstChildElement("Gaps");
            if (gapselement) {
                XMLElement* gap = gapselement->FirstChildElement("Interval");
                for( ; gap ; gap = gap->NextSiblingElement())
                {
                    uint64_t a = GST_CLOCK_TIME_NONE;
                    uint64_t b = GST_CLOCK_TIME_NONE;
                    gap->QueryUnsigned64Attribute("begin", &a);
                    gap->QueryUnsigned64Attribute("end", &b);
                    tl.addGap( (GstClockTime) a, (GstClockTime) b );
                }
            }
            XMLElement *fadingselement = timelineelement->FirstChildElement("Fading");
            if (fadingselement) {
                XMLElement* array = fadingselement->FirstChildElement("array");
                XMLElementDecodeArray(array, tl.fadingArray(), MAX_TIMELINE_ARRAY * sizeof(float));
                uint mode = 0;
                fadingselement->QueryUnsignedAttribute("mode", &mode);
                n.setTimelineFadingMode((MediaPlayer::FadingMode) mode);
            }
            XMLElement *flagselement = timelineelement->FirstChildElement("Flags");
            if (flagselement) {
                XMLElement* flag = flagselement->FirstChildElement("Interval");
                for( ; flag ; flag = flag->NextSiblingElement())
                {
                    uint64_t a = GST_CLOCK_TIME_NONE;
                    uint64_t b = GST_CLOCK_TIME_NONE;
                    int t = 0;
                    flag->QueryUnsigned64Attribute("begin", &a);
                    flag->QueryUnsigned64Attribute("end", &b);
                    flag->QueryIntAttribute("type", &t);
                    TimeInterval flag((GstClockTime) a, (GstClockTime) b);
                    flag.type = t;
                    tl.addFlag( flag );
                }
            }
            n.setTimeline(tl);
        }

        // change play rate: will be activated in SessionLoader::visit (MediaSource& s)
        double speed = 1.0;
        mediaplayerNode->QueryDoubleAttribute("speed", &speed);
        n.setRate(speed);

        // change video effect only if effect given is different
        const char *pFilter = mediaplayerNode->Attribute("video_effect");
        if (pFilter) {
            if (n.videoEffect().compare(pFilter) != 0) {
                n.setVideoEffect(pFilter);
            }
        }

        // change play status only if different id (e.g. new media player)
        if ( n.id() != id__ ) {

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

            /// obsolete
            // only read media player play attribute if the source has no play attribute (backward compatibility)
            if ( !xmlCurrent_->Attribute( "play" ) ) {
                bool play = true;
                mediaplayerNode->QueryBoolAttribute("play", &play);
                n.play(play);
            }
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
        uniforms->QueryIntAttribute("nbColors", &n.nbColors);
        uniforms->QueryIntAttribute("invert", &n.invert);
    }

    XMLElement* gamma = xmlCurrent_->FirstChildElement("gamma");
    if (gamma)
        tinyxml2::XMLElementToGLM( gamma->FirstChildElement("vec4"), n.gamma);
    XMLElement* levels = xmlCurrent_->FirstChildElement("levels");
    if (levels)
        tinyxml2::XMLElementToGLM( levels->FirstChildElement("vec4"), n.levels);
}

void SessionLoader::visit (Source& s)
{
    XMLElement* sourceNode = xmlCurrent_;
    const char *pName = sourceNode->Attribute("name");
    s.setName(pName);

    // schedule lock and play of the source (callbacks called after init)
    bool l = false;
    sourceNode->QueryBoolAttribute("locked", &l);
    s.call( new Lock(l) );
    bool p = true;
    sourceNode->QueryBoolAttribute("play", &p);
    s.call( new Play(p, session_) );

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
        // set id of source used as mask (if exists)
        uint64_t id__ = 0;
        xmlCurrent_->QueryUnsigned64Attribute("source", &id__);
        s.maskSource()->connect(id__, session_);
        s.touch(Source::SourceUpdate_Mask);
        // set the mask from jpeg (if exists)
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

    xmlCurrent_ = sourceNode->FirstChildElement("Audio");
    if (xmlCurrent_) {
        bool on = xmlCurrent_->BoolAttribute("enabled", false);
        s.setAudioEnabled(on);
        float volume = xmlCurrent_->FloatAttribute("volume", 1.f);
        s.setAudioVolumeFactor(Source::VOLUME_BASE, volume);
        int mix = xmlCurrent_->IntAttribute("volume_mix", 0);
        s.setAudioVolumeMix( Source::Volume_mult_parent | mix);
    }

    // restore current
    xmlCurrent_ = sourceNode;

}

void SessionLoader::visit (MediaSource& s)
{
    bool freshload = false;
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
                freshload = true;
            }
        }
        // ensures the source is initialized even if no valid path is given
        else
            s.setPath("");
    }

    // set config media player
    s.mediaplayer()->accept(*this);

    // read source play speed from media player SessionLoader::visit
    double r = s.mediaplayer()->playSpeed();
    // reset media player to normal speed
    s.mediaplayer()->setRate(1.0); 
    // add a callback to activate source play speed and rewind
    s.call( new PlaySpeed( r ) );
    if (freshload)
        s.call( new RePlay( ) );
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
            if ( !SystemToolkit::file_exists(path) ){
                const char * relative;
                if ( pathNode->QueryStringAttribute("relative", &relative) == XML_SUCCESS) {
                    std::string rel = SystemToolkit::path_absolute_from_path(std::string( relative ), sessionFilePath_);
                    Log::Info("File '%s' not found; Trying '%s' instead.", path.c_str(), rel.c_str());
                    path = rel;
                }
            }
            // load only if session source s is new
            if ( path != s.path() ) {
                // level of session imbrication
                uint l = level_;
                if ( path == session_->filename() && l < MAX_SESSION_LEVEL) {
                    // prevent recursive load by reaching maximum level of inclusion immediately
                    l += MAX_SESSION_LEVEL;
                    Log::Warning("Prevending recursive inclusion of Session '%s' into itself.", path.c_str());
                }
                else
                    // normal increment level of inclusion
                    ++l;
                // launch session loader at incremented level
                s.load(path, l);
            }
        }
    }

}

void SessionLoader::visit (SessionGroupSource& s)
{
    float height = 0.f;
    xmlCurrent_->QueryFloatAttribute("height", &height);
    if (height > 0.f) {
        // load dimensions
        glm::vec3 scale(1.f), center(0.f);
        const XMLElement *scaleNode = xmlCurrent_->FirstChildElement("scale");
        if (scaleNode)
            tinyxml2::XMLElementToGLM( scaleNode->FirstChildElement("vec3"), scale);
        const XMLElement *centerNode = xmlCurrent_->FirstChildElement("center");
        if (centerNode)
            tinyxml2::XMLElementToGLM( centerNode->FirstChildElement("vec3"), center);
        s.setDimensions(scale, center, height);
    }
    else {
        // set resolution from host session
        s.setResolution( session_->config(View::RENDERING)->scale_ );        
    }

    // get the inside session
    XMLElement* sessionGroupNode = xmlCurrent_->FirstChildElement("Session");
    if (sessionGroupNode) {
        // only parse if newly created
        if (s.session()->empty()) {
            // load session inside group
            SessionLoader grouploader( s.session(), level_ + 1 );
            grouploader.load( sessionGroupNode );
        }
    }
}

void SessionLoader::visit (RenderSource& s)
{
    // set attributes
    int p = 0;
    xmlCurrent_->QueryIntAttribute("provenance", &p);
    s.setRenderingProvenance((RenderSource::RenderSourceProvenance)p);

    // set session
    s.setSession( session_ );
}

void SessionLoader::visit(Stream &n)
{
    XMLElement* streamNode = xmlCurrent_->FirstChildElement("Stream");

    if (streamNode) {
        bool rewind_on_disabled = false;
        streamNode->QueryBoolAttribute("rewind_on_disabled", &rewind_on_disabled);
        n.setRewindOnDisabled(rewind_on_disabled);
    }
}

void SessionLoader::visit (StreamSource& s)
{
    // set config stream
    if (s.stream() != nullptr)
        s.stream()->accept(*this);
}

void SessionLoader::visit (PatternSource& s)
{
    uint t = xmlCurrent_->UnsignedAttribute("pattern");

    glm::ivec2 resolution(800, 600);
    XMLElement* res = xmlCurrent_->FirstChildElement("resolution");
    if (res)
        tinyxml2::XMLElementToGLM( res->FirstChildElement("ivec2"), resolution);

    // change only if different pattern
    if ( s.pattern() && t != s.pattern()->type() )
        s.setPattern(t, resolution);
}

void SessionLoader::visit(TextSource &s)
{
    bool value_changed = false;
    std::string text;
    glm::ivec2 resolution(800, 600);

    XMLElement *_contents = xmlCurrent_->FirstChildElement("contents");
    if ( s.contents() && _contents) {
        // content can be an array of encoded text (to support strings with Pango styles <i>, <b>, etc.)
        unsigned int len = 0;
        const XMLElement *array = _contents->FirstChildElement("array");
        if (array) {
            array->QueryUnsignedAttribute("len", &len);
            // ok, we got a size of data to load
            if (len > 0) {
                GString *data = g_string_new_len("", len);
                if (XMLElementDecodeArray(array, data->str, data->len)) {
                    text = std::string(data->str);
                }
                g_string_free(data, FALSE);
            }
        }
        // content can also just be raw text
        else {
            const char *data = _contents->GetText();
            if (data)
                text = std::string(data);
        }

        value_changed |= s.contents()->text().compare(text) != 0;

        // attributes of content
        const char *font;
        if (_contents->QueryStringAttribute("font-desc", &font) == XML_SUCCESS && font) {
            if (s.contents()->fontDescriptor().compare(font) != 0) {
                s.contents()->setFontDescriptor(font);
            }
        }
        uint var = s.contents()->horizontalAlignment();
        _contents->QueryUnsignedAttribute("halignment", &var);
        if (s.contents()->horizontalAlignment() != var)
            s.contents()->setHorizontalAlignment(var);
        var = s.contents()->verticalAlignment();
        _contents->QueryUnsignedAttribute("valignment", &var);
        if (s.contents()->verticalAlignment() != var)
            s.contents()->setVerticalAlignment(var);
        var = s.contents()->color();
        _contents->QueryUnsignedAttribute("color", &var);
        if (s.contents()->color() != var)
            s.contents()->setColor(var);
        var = s.contents()->outline();
        _contents->QueryUnsignedAttribute("outline", &var);
        if (s.contents()->outline() != var)
            s.contents()->setOutline(var);
        var = s.contents()->outlineColor();
        _contents->QueryUnsignedAttribute("outline-color", &var);
        if (s.contents()->outlineColor() != var)
            s.contents()->setOutlineColor(var);
        float x = 0.f;
        _contents->QueryFloatAttribute("x", &x);
        if (s.contents()->horizontalPadding() != x)
            s.contents()->setHorizontalPadding(x);
        float y = 0.f;
        _contents->QueryFloatAttribute("y", &y);
        if (s.contents()->verticalPadding() != y)
            s.contents()->setVerticalPadding(y);
    }

    XMLElement* res = xmlCurrent_->FirstChildElement("resolution");
    if (res) {
        tinyxml2::XMLElementToGLM( res->FirstChildElement("ivec2"), resolution);
        value_changed |= resolution.x != (int) s.stream()->width() || resolution.y != (int) s.stream()->height();
    }

    // change only if different
    if ( s.contents() && ( value_changed || s.contents()->text().empty() ) ) {
        s.setContents( text, resolution );
    }
}

void SessionLoader::visit (DeviceSource& s)
{
    std::string devname = std::string ( xmlCurrent_->Attribute("device") );

    // change only if different device
    if ( devname != s.device() )
        s.setDevice(devname);
}


void SessionLoader::visit (ScreenCaptureSource& s)
{
    std::string winname = std::string ( xmlCurrent_->Attribute("window") );

    // change only if different window
    if ( winname != s.window() )
        s.setWindow(winname);
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

void SessionLoader::visit (SrtReceiverSource& s)
{
    XMLElement* ip = xmlCurrent_->FirstChildElement("ip");
    XMLElement* port = xmlCurrent_->FirstChildElement("port");
    if (ip && port) {
        const char * textip = ip->GetText();
        const char * textport = port->GetText();
        if (textip && textport)
            s.setConnection(textip, textport);
    }
}

void SessionLoader::visit (FrameBufferFilter&)
{

}

void SessionLoader::visit (DelayFilter& f)
{
    double d = 0.0;
    xmlCurrent_->QueryDoubleAttribute("delay", &d);
    f.setDelay(d);
}

void SessionLoader::visit (ResampleFilter& f)
{
    int m = 0;
    xmlCurrent_->QueryIntAttribute("factor", &m);
    f.setFactor(m);
}

std::map< std::string, float > get_parameters_(XMLElement* parameters)
{
    std::map< std::string, float > filter_params;
    if (parameters) {
        XMLElement* param = parameters->FirstChildElement("uniform");
        for( ; param ; param = param->NextSiblingElement())
        {
            float val = 0.f;
            param->QueryFloatAttribute("value", &val);
            const char * name;
            if ( param->QueryStringAttribute("name", &name) == XML_SUCCESS && name != NULL)
                filter_params[name] = val;
        }
    }
    return filter_params;
}

void SessionLoader::visit (BlurFilter& f)
{
    // blur specific
    int m = 0;
    xmlCurrent_->QueryIntAttribute("method", &m);
    f.setMethod(m);
    // image filter parameters
    f.setProgramParameters( get_parameters_(xmlCurrent_->FirstChildElement("parameters")) );
}

void SessionLoader::visit (SharpenFilter& f)
{
    int m = 0;
    xmlCurrent_->QueryIntAttribute("method", &m);
    f.setMethod(m);
    // image filter parameters
    f.setProgramParameters( get_parameters_(xmlCurrent_->FirstChildElement("parameters")) );
}

void SessionLoader::visit (SmoothFilter& f)
{
    int m = 0;
    xmlCurrent_->QueryIntAttribute("method", &m);
    f.setMethod(m);
    // image filter parameters
    f.setProgramParameters( get_parameters_(xmlCurrent_->FirstChildElement("parameters")) );
}

void SessionLoader::visit (EdgeFilter& f)
{
    int m = 0;
    xmlCurrent_->QueryIntAttribute("method", &m);
    f.setMethod(m);
    // image filter parameters
    f.setProgramParameters( get_parameters_(xmlCurrent_->FirstChildElement("parameters")) );
}

void SessionLoader::visit (AlphaFilter& f)
{
    int m = 0;
    xmlCurrent_->QueryIntAttribute("operation", &m);
    f.setOperation(m);
    // image filter parameters
    f.setProgramParameters( get_parameters_(xmlCurrent_->FirstChildElement("parameters")) );
}

std::map< std::string, uint64_t > get_textures_(XMLElement* textures)
{
    std::map< std::string, uint64_t > filter_params;
    if (textures) {
        XMLElement* param = textures->FirstChildElement("sampler2D");
        for( ; param ; param = param->NextSiblingElement())
        {
            uint64_t val = 0.f;
            param->QueryUnsigned64Attribute("id", &val);
            const char * name;
            if ( param->QueryStringAttribute("name", &name) == XML_SUCCESS && name != NULL)
                filter_params[name] = val;
        }
    }
    return filter_params;
}

void SessionLoader::visit (ImageFilter& f)
{
    std::pair< std::string, std::string > filter_codes;

    const char *name = NULL;
    std::string filter_name;
    if (xmlCurrent_->QueryStringAttribute("name", &name) == XML_SUCCESS && name != NULL)
        filter_name = std::string(name);

    const char *filename = NULL;
    std::string filter_filename;
    if (xmlCurrent_->QueryStringAttribute("filename", &filename) == XML_SUCCESS && filename != NULL)
        filter_filename = std::string(filename);

    // image filter code
    XMLElement* firstpass  = xmlCurrent_->FirstChildElement("firstpass");
    if (firstpass) {
        const char * code = firstpass->GetText();
        if (code)
            filter_codes.first = std::string(code);
    }
    XMLElement* secondpass = xmlCurrent_->FirstChildElement("secondpass");
    if (secondpass) {
        const char * code = secondpass->GetText();
        if (code)
            filter_codes.second = std::string(code);
    }

    // image filter parameters
    std::map< std::string, float > filter_params = get_parameters_(xmlCurrent_->FirstChildElement("parameters"));

   // image filter textures
    std::map< std::string, uint64_t > filter_textures = get_textures_(xmlCurrent_->FirstChildElement("textures"));

    // set image filter program and parameters
    f.setProgram( FilteringProgram(filter_name, filter_codes.first, filter_codes.second,
                                  filter_params, filter_filename, filter_textures) );

    // set global iMouse
    XMLElement* imouse = xmlCurrent_->FirstChildElement("iMouse");
    if (imouse)
        tinyxml2::XMLElementToGLM( imouse->FirstChildElement("vec4"), FilteringProgram::iMouse);

}

void SessionLoader::visit(ShaderSource &s)
{
    XMLElement *res = xmlCurrent_->FirstChildElement("resolution");
    if (res) {
        glm::vec3 resolution(64.f, 64.f, 0.f);
        tinyxml2::XMLElementToGLM(res->FirstChildElement("vec3"), resolution);
        s.setResolution(resolution);
    }

    // shader code
    xmlCurrent_ = xmlCurrent_->FirstChildElement("Filter");
    if (xmlCurrent_) {
        // set config filter
        s.filter()->accept(*this);
    }

}

void SessionLoader::visit (CloneSource& s)
{
    // configuration of filter in clone
    xmlCurrent_ = xmlCurrent_->FirstChildElement("Filter");
    if (xmlCurrent_) {
        // get type of filter and create
        int t = 0;
        xmlCurrent_->QueryIntAttribute("type", &t);
        s.setFilter( FrameBufferFilter::Type(t) );

        // set config filter
        s.filter()->accept(*this);
    }
}

void SessionLoader::visit (SourceCallback &)
{
}

void SessionLoader::visit (ValueSourceCallback &c)
{
    float v = 0.f;
    xmlCurrent_->QueryFloatAttribute("value", &v);
    c.setValue(v);

    float d = 0.f;
    xmlCurrent_->QueryFloatAttribute("duration", &d);
    c.setDuration(d);

    bool b = false;
    xmlCurrent_->QueryBoolAttribute("bidirectional", &b);
    c.setBidirectional(b);
}

void SessionLoader::visit (Play &c)
{
    bool p = true;
    xmlCurrent_->QueryBoolAttribute("play", &p);
    c.setValue(p);

    bool b = false;
    xmlCurrent_->QueryBoolAttribute("bidirectional", &b);
    c.setBidirectional(b);
}

void SessionLoader::visit (PlayFastForward &c)
{
    float d = 0.f;
    xmlCurrent_->QueryFloatAttribute("step", &d);
    c.setValue(d);

    d = 0.f;
    xmlCurrent_->QueryFloatAttribute("duration", &d);
    c.setDuration(d);
}

void SessionLoader::visit (Seek &c)
{
    uint64_t v = 0.f;
    xmlCurrent_->QueryUnsigned64Attribute("value", &v);
    c.setValue(v);

    bool b = false;
    xmlCurrent_->QueryBoolAttribute("bidirectional", &b);
    c.setBidirectional(b);
}
void SessionLoader::visit (Flag &c)
{
    int v = -1;
    xmlCurrent_->QueryIntAttribute("value", &v);
    c.setValue(v);
}

void SessionLoader::visit (SetAlpha &c)
{
    float a = 0.f;
    xmlCurrent_->QueryFloatAttribute("alpha", &a);
    c.setValue(a);

    float d = 0.f;
    xmlCurrent_->QueryFloatAttribute("duration", &d);
    c.setDuration(d);

    bool b = false;
    xmlCurrent_->QueryBoolAttribute("bidirectional", &b);
    c.setBidirectional(b);
}

void SessionLoader::visit (SetDepth &c)
{
    float d = 0.f;
    xmlCurrent_->QueryFloatAttribute("depth", &d);
    c.setValue(d);

    d = 0.f;
    xmlCurrent_->QueryFloatAttribute("duration", &d);
    c.setDuration(d);

    bool b = false;
    xmlCurrent_->QueryBoolAttribute("bidirectional", &b);
    c.setBidirectional(b);
}

void SessionLoader::visit (SetGeometry &c)
{
    float d = 0.f;
    xmlCurrent_->QueryFloatAttribute("duration", &d);
    c.setDuration(d);

    bool b = false;
    xmlCurrent_->QueryBoolAttribute("bidirectional", &b);
    c.setBidirectional(b);

    XMLElement* current = xmlCurrent_;

    xmlCurrent_ = xmlCurrent_->FirstChildElement("Geometry");
    if (xmlCurrent_) {
        Group tmp;
        tmp.accept(*this);
        c.setTarget(&tmp);
    }

    xmlCurrent_ = current;
}

void SessionLoader::visit (SetGamma &c)
{
    float d = 0.f;
    xmlCurrent_->QueryFloatAttribute("duration", &d);
    c.setDuration(d);

    bool b = false;
    xmlCurrent_->QueryBoolAttribute("bidirectional", &b);
    c.setBidirectional(b);

    XMLElement* gamma = xmlCurrent_->FirstChildElement("gamma");
    if (gamma) {
        glm::vec4 v;
        tinyxml2::XMLElementToGLM( gamma->FirstChildElement("vec4"), v);
        c.setValue(v);
    }
}

void SessionLoader::visit (Loom &c)
{
    float d = 0.f;
    xmlCurrent_->QueryFloatAttribute("delta", &d);
    c.setValue(d);

    d = 0.f;
    xmlCurrent_->QueryFloatAttribute("duration", &d);
    c.setDuration(d);
}

void SessionLoader::visit (Grab &c)
{
    float dx = 0.f, dy = 0.f;
    xmlCurrent_->QueryFloatAttribute("delta.x", &dx);
    xmlCurrent_->QueryFloatAttribute("delta.y", &dy);
    c.setValue( glm::vec2(dx, dy) );

    float d = 0.f;
    xmlCurrent_->QueryFloatAttribute("duration", &d);
    c.setDuration(d);
}

void SessionLoader::visit (Resize &c)
{
    float dx = 0.f, dy = 0.f;
    xmlCurrent_->QueryFloatAttribute("delta.x", &dx);
    xmlCurrent_->QueryFloatAttribute("delta.y", &dy);
    c.setValue( glm::vec2(dx, dy) );

    float d = 0.f;
    xmlCurrent_->QueryFloatAttribute("duration", &d);
    c.setDuration(d);
}

void SessionLoader::visit (Turn &c)
{
    float d = 0.f;
    xmlCurrent_->QueryFloatAttribute("delta", &d);
    c.setValue(d);

    d = 0.f;
    xmlCurrent_->QueryFloatAttribute("duration", &d);
    c.setDuration(d);
}



