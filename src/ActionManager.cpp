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

#include <string>
#include <thread>

#include "Log.h"
#include "View.h"
#include "Mixer.h"
#include "tinyxml2Toolkit.h"
#include "SessionVisitor.h"
#include "SessionCreator.h"
#include "Settings.h"
#include "BaseToolkit.h"
#include "Interpolator.h"
#include "SystemToolkit.h"

#include "ActionManager.h"

#ifndef NDEBUG
#define ACTION_DEBUG
#endif

#define HISTORY_NODE(i) std::to_string(i).insert(0,1,'H')

using namespace tinyxml2;


Action::Action(): history_step_(0), history_max_step_(0),
    snapshot_id_(0), snapshot_node_(nullptr), interpolator_(nullptr), interpolator_node_(nullptr)
{

}

void Action::init()
{
    // clean the history
    history_doc_.Clear();
    history_step_ = 0;
    history_max_step_ = 0;

    // reset snapshot
    snapshot_id_ = 0;
    snapshot_node_ = nullptr;

    store("Session start");
}

// must be called in a thread running in parrallel of the rendering
// (needs opengl update to get thumbnail)
void captureMixerSession(Session *se, std::string node, std::string label, tinyxml2::XMLDocument *doc = nullptr)
{
    if (se != nullptr) {

        tinyxml2::XMLDocument *_doc = doc;
        if (doc == nullptr) {
            _doc = se->snapshots()->xmlDoc_;
            se->snapshots()->access_.lock();
        }

        // create node
        XMLElement *sessionNode = _doc->NewElement( node.c_str() );
        _doc->InsertEndChild(sessionNode);
        // label describes the action
        sessionNode->SetAttribute("label", label.c_str() );
        // label describes the action
        sessionNode->SetAttribute("date", SystemToolkit::date_time_string().c_str() );
        // view indicates the view when this action occurred
        sessionNode->SetAttribute("view", (int) Mixer::manager().view()->mode());

        // get the thumbnail (requires one opengl update to render)
        FrameBufferImage *thumbnail = se->renderThumbnail();
        if (thumbnail) {
            XMLElement *imageelement = SessionVisitor::ImageToXML(thumbnail, _doc);
            if (imageelement)
                sessionNode->InsertEndChild(imageelement);
            delete thumbnail;
        }

        // save session attributes
        sessionNode->SetAttribute("activationThreshold", se->activationThreshold());

        // save all sources using source visitor
        SessionVisitor sv(_doc, sessionNode);
        for (auto iter = se->begin(); iter != se->end(); ++iter, sv.setRoot(sessionNode) )
            (*iter)->accept(sv);

        if (doc == nullptr)
            se->snapshots()->access_.unlock();
    }
}

void Action::storeSession(Session *se, std::string label)
{
    //
    if (!Action::manager().history_access_.try_lock())
        return;

    // incremental naming of history nodes
    Action::manager().history_step_++;

    // erase future
    for (uint e = Action::manager().history_step_; e <= Action::manager().history_max_step_; e++) {
        XMLElement *node = Action::manager().history_doc_.FirstChildElement( HISTORY_NODE(e).c_str() );
        if ( node )
            Action::manager().history_doc_.DeleteChild(node);
    }
    Action::manager().history_max_step_ = Action::manager().history_step_;

    // capture current session
    captureMixerSession(se,
                        HISTORY_NODE(Action::manager().history_step_),
                        label,
                        &Action::manager().history_doc_);

    Action::manager().history_access_.unlock();
}


void Action::store(const std::string &label)
{
    // TODO: set a maximum amount of stored steps? (even very big, just to ensure memory limit)

    // ignore if locked or if no label is given
    if (label.empty())
        return;

    // threaded capturing state of current session
    std::thread(Action::storeSession, Mixer::manager().session(), label).detach();

#ifdef ACTION_DEBUG
    Log::Info("Action stored %d '%s'", history_step_, label.c_str());
//        XMLSaveDoc(&history_doc_, "/home/bhbn/history.xml");
#endif
}

void Action::undo()
{
    // not possible to go to 1 -1 = 0
    if (history_step_ <= 1)
        return;

    // restore always changes step_ to step_ - 1
    restore( history_step_ - 1);
}

void Action::redo()
{
    // not possible to go to max_step_ + 1
    if (history_step_ >= history_max_step_)
        return;

    // restore always changes step_ to step_ + 1
    restore( history_step_ + 1);
}


void Action::stepTo(uint target)
{
    // get reasonable target
    uint t = CLAMP(target, 1, history_max_step_);

    // ignore t == step_
    if (t != history_step_)
        restore(t);
}

std::string Action::label(uint s) const
{
    std::string l = "";

    if (s > 0 && s <= history_max_step_) {
        const XMLElement *sessionNode = history_doc_.FirstChildElement( HISTORY_NODE(s).c_str());
        if  (sessionNode)
            l = sessionNode->Attribute("label");
    }
    return l;
}

FrameBufferImage *Action::thumbnail(uint s) const
{
    FrameBufferImage *img = nullptr;

    if (s > 0 && s <= history_max_step_) {
        const XMLElement *sessionNode = history_doc_.FirstChildElement( HISTORY_NODE(s).c_str());
        if  (sessionNode)
            img = SessionLoader::XMLToImage(sessionNode);
    }

    return img;
}

void Action::restore(uint target)
{
    history_access_.lock();

    // get history node of target step
    history_step_ = CLAMP(target, 1, history_max_step_);
    XMLElement *sessionNode = history_doc_.FirstChildElement( HISTORY_NODE(history_step_).c_str() );

    if (sessionNode) {

        // ask view to refresh, and switch to action view if user prefers
        int view = Settings::application.current_view ;
        if (Settings::application.action_history_follow_view)
            sessionNode->QueryIntAttribute("view", &view);
        Mixer::manager().setView( (View::Mode) view);

        // actually restore
        Mixer::manager().restore(sessionNode);
    }

    history_access_.unlock();
}


void Action::takeSnapshot(Session *se, const std::string &label, bool create_thread)
{
    if (se !=nullptr) {

        // create snapshot id
        u_int64_t id = BaseToolkit::uniqueId();

        // inform session of its new snapshot
        se->snapshots()->keys_.push_back(id);

        if (create_thread)
            // threaded capture state of current session
            std::thread(captureMixerSession, se, SNAPSHOT_NODE(id), label, nullptr).detach();
        else
            captureMixerSession(se, SNAPSHOT_NODE(id), label);

#ifdef ACTION_DEBUG
        Log::Info("Snapshot stored %d '%s'", id, label.c_str());
#endif
    }
}

void Action::snapshot(const std::string &label)
{
    // ensure label is unique
    std::string snap_label = BaseToolkit::uniqueName(label, labels());

    // take the snapshot on current session
    takeSnapshot(Mixer::manager().session(), snap_label, true);

}

void Action::open(uint64_t snapshotid)
{
    if ( snapshot_id_ != snapshotid )
    {
        // get snapshot node of target in current session
        Session *se = Mixer::manager().session();
        if (se) {
            se->snapshots()->access_.lock();
            snapshot_node_ = se->snapshots()->xmlDoc_->FirstChildElement(
                SNAPSHOT_NODE(snapshotid).c_str());
            se->snapshots()->access_.unlock();
        }

        if (snapshot_node_)
            snapshot_id_ = snapshotid;
        else
            snapshot_id_ = 0;

        interpolator_node_ = nullptr;
    }
}

void Action::replace(uint64_t snapshotid)
{
    if (snapshotid > 0)
        open(snapshotid);

    if (snapshot_node_) {
        // remember label
        std::string label = snapshot_node_->Attribute("label");

        // remove previous node
        Session *se = Mixer::manager().session();
        if (se) {

            se->snapshots()->access_.lock();
            se->snapshots()->xmlDoc_->DeleteChild( snapshot_node_ );
            se->snapshots()->access_.unlock();

            // threaded capture state of current session
            std::thread(captureMixerSession, se, SNAPSHOT_NODE(snapshot_id_), label, nullptr).detach();

#ifdef ACTION_DEBUG
            Log::Info("Snapshot replaced %d '%s'", snapshot_id_, label.c_str());
#endif

        }
    }
}

std::list<uint64_t> Action::snapshots() const
{
    return Mixer::manager().session()->snapshots()->keys_;
}

std::list<std::string> Action::labels() const
{
    std::list<std::string> names;

    tinyxml2::XMLDocument *doc = Mixer::manager().session()->snapshots()->xmlDoc_;
    for ( XMLElement *snap = doc->FirstChildElement(); snap ; snap = snap->NextSiblingElement() )
        names.push_back( snap->Attribute("label"));

    return names;
}

std::string Action::label(uint64_t snapshotid) const
{
    std::string label = "";

    // get snapshot node of target in current session
    Session *se = Mixer::manager().session();
    const XMLElement *snap = se->snapshots()->xmlDoc_->FirstChildElement( SNAPSHOT_NODE(snapshotid).c_str() );

    if (snap)
        label = snap->Attribute("label");

    return label;
}

std::string Action::date(uint64_t snapshotid) const
{
    std::string date = "";

    // get snapshot node of target in current session
    Session *se = Mixer::manager().session();
    const XMLElement *snap = se->snapshots()->xmlDoc_->FirstChildElement( SNAPSHOT_NODE(snapshotid).c_str() );

    if (snap){
        const char *d = snap->Attribute("date");
        if (d)
            date = std::string(d);
    }

    return date;
}

void Action::setLabel (uint64_t snapshotid, const std::string &label)
{
    open(snapshotid);

    if (snapshot_node_){
        // ensure unique snapshot label
        std::string snap_label = BaseToolkit::uniqueName(label, labels());
        // change attribute
        snapshot_node_->SetAttribute("label", snap_label.c_str());
    }
}

FrameBufferImage *Action::thumbnail(uint64_t snapshotid) const
{
    FrameBufferImage *img = nullptr;

    // get snapshot node of target in current session
    Session *se = Mixer::manager().session();
    const XMLElement *snap = se->snapshots()->xmlDoc_->FirstChildElement( SNAPSHOT_NODE(snapshotid).c_str() );

    if (snap){
        img = SessionLoader::XMLToImage(snap);
    }

    return img;
}

void Action::clearSnapshots()
{
    Session *se = Mixer::manager().session();
    while (!se->snapshots()->keys_.empty())
        remove(se->snapshots()->keys_.front());
}

void Action::remove(uint64_t snapshotid)
{
    if (snapshotid > 0)
        open(snapshotid);

    if (snapshot_node_) {
        // remove
        Session *se = Mixer::manager().session();
        se->snapshots()->access_.lock();
        se->snapshots()->xmlDoc_->DeleteChild( snapshot_node_ );
        se->snapshots()->keys_.remove( snapshot_id_ );
        se->snapshots()->access_.unlock();
    }

    snapshot_node_ = nullptr;
    snapshot_id_ = 0;
}

void Action::restore(uint64_t snapshotid)
{
    if (snapshotid > 0)
        open(snapshotid);

    if (snapshot_node_)
        // actually restore
        Mixer::manager().restore(snapshot_node_);

    store("Snapshot " + label(snapshot_id_));
}

float Action::interpolation()
{
    float ret = 0.f;
    if ( interpolator_node_ == snapshot_node_  && interpolator_)
        ret = interpolator_->current();

    return ret;
}

void Action::interpolate(float val, uint64_t snapshotid)
{
    if (snapshotid > 0)
        open(snapshotid);

    if (snapshot_node_) {

        if ( interpolator_node_ != snapshot_node_ ) {

            // change interpolator
            if (interpolator_)
                delete interpolator_;

            // create new interpolator
            interpolator_ = new Interpolator;

            // current session
            Session *se = Mixer::manager().session();

            XMLElement* N = snapshot_node_->FirstChildElement("Source");
            for( ; N ; N = N->NextSiblingElement()) {

                // check if a source with the given id exists in the session
                uint64_t id_xml_ = 0;
                N->QueryUnsigned64Attribute("id", &id_xml_);
                SourceList::iterator sit = se->find(id_xml_);

                // a source with this id exists
                if ( sit != se->end() ) {
                    // read target in the snapshot xml
                    SourceCore target;
                    SessionLoader::XMLToSourcecore(N, target);

                    // add an interpolator for this source
                    interpolator_->add(*sit, target);
                }
            }

            // operate interpolation on opened snapshot
            interpolator_node_ = snapshot_node_;
        }

        if (interpolator_) {
//            Log::Info("Action::interpolate %f", val);
            interpolator_->apply( val );
        }
    }

}



// static multithreaded version saving
static void saveSnapshot(const std::string& filename, tinyxml2::XMLElement *snapshot_node)
{
    if (!snapshot_node){
        Log::Warning("Invalid version.", filename.c_str());
        return;
    }
    const char *l = snapshot_node->Attribute("label");
    if (!l) {
        Log::Warning("Invalid version.", filename.c_str());
        return;
    }

    // load the file: is it a session?
    tinyxml2::XMLDocument xmlDoc;
    XMLError eResult = xmlDoc.LoadFile(filename.c_str());
    if ( XMLResultError(eResult)){
        Log::Warning("%s could not be opened for re-export.", filename.c_str());
        return;
    }
    XMLElement *header = xmlDoc.FirstChildElement(APP_NAME);
    if (header == nullptr) {
        Log::Warning("%s is not a %s session file.", filename.c_str(), APP_NAME);
        return;
    }

    // remove all snapshots
    XMLElement* snapshotNode = xmlDoc.FirstChildElement("Snapshots");
    xmlDoc.DeleteChild(snapshotNode);

    // swap "Session" node with version_node
    XMLElement *sessionNode = xmlDoc.FirstChildElement("Session");
    xmlDoc.DeleteChild(sessionNode);
    sessionNode = snapshot_node->DeepClone(&xmlDoc)->ToElement();
    sessionNode->SetName("Session");
    xmlDoc.InsertEndChild(sessionNode);

    // we got a session, set a new export filename
    std::string newfilename = filename;
    newfilename.insert(filename.size()-4, "_" + std::string(l));

    // save new file to disk
    if ( XMLSaveDoc(&xmlDoc, newfilename) )
        Log::Notify("Version exported to %s.", newfilename.c_str());
    else
        // error
        Log::Warning("Failed to export Session file %s.", newfilename.c_str());
}

void Action::saveas(const std::string& filename, uint64_t snapshotid)
{
    if (filename.empty())
        return;

    if (snapshotid > 0)
        open(snapshotid);

    if (snapshot_node_) {
        // launch a thread to save the session
        std::thread (saveSnapshot, filename, snapshot_node_).detach();
    }
}
