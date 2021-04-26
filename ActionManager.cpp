#include <string>
#include <algorithm>

#include "Log.h"
#include "View.h"
#include "Mixer.h"
#include "MixingGroup.h"
#include "tinyxml2Toolkit.h"
#include "ImageProcessingShader.h"
#include "SessionVisitor.h"
#include "SessionCreator.h"
#include "Settings.h"
#include "BaseToolkit.h"
#include "Interpolator.h"

#include "ActionManager.h"

#ifndef NDEBUG
#define ACTION_DEBUG
#endif

#define HISTORY_NODE(i) std::to_string(i).insert(0,1,'H')
#define SNAPSHOT_NODE(i) std::to_string(i).insert(0,1,'S')

using namespace tinyxml2;

void captureMixerSession(tinyxml2::XMLDocument *doc, std::string node, std::string label)
{
    // get session to operate on
    Session *se = Mixer::manager().session();
    se->lock();

    // create node
    XMLElement *sessionNode = doc->NewElement( node.c_str() );
    doc->InsertEndChild(sessionNode);
    // label describes the action
    sessionNode->SetAttribute("label", label.c_str() );
    // view indicates the view when this action occured
    sessionNode->SetAttribute("view", (int) Mixer::manager().view()->mode());

    // get the thumbnail (requires one opengl update to render)
    FrameBufferImage *thumbnail = se->thumbnail();
    XMLElement *imageelement = SessionVisitor::ImageToXML(thumbnail, doc);
    if (imageelement)
        sessionNode->InsertEndChild(imageelement);
    delete thumbnail;

    // save all sources using source visitor
    SessionVisitor sv(doc, sessionNode);
    for (auto iter = se->begin(); iter != se->end(); ++iter, sv.setRoot(sessionNode) )
        (*iter)->accept(sv);

    se->unlock();
}


Action::Action(): history_step_(0), history_max_step_(0), locked_(false),
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

void Action::store(const std::string &label)
{
    // ignore if locked or if no label is given
    if (locked_ || label.empty())
        return;

    // incremental naming of history nodes
    history_step_++;

    // erase future
    for (uint e = history_step_; e <= history_max_step_; e++) {
        XMLElement *node = history_doc_.FirstChildElement( HISTORY_NODE(e).c_str() );
        if ( node )
            history_doc_.DeleteChild(node);
    }
    history_max_step_ = history_step_;

    // threaded capturing state of current session
    std::thread(captureMixerSession, &history_doc_, HISTORY_NODE(history_step_), label).detach();

#ifdef ACTION_DEBUG
    Log::Info("Action stored %d '%s'", history_step_, label.c_str());
        XMLSaveDoc(&history_doc_, "/home/bhbn/history.xml");
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
    // lock
    locked_ = true;

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

    // free
    locked_ = false;
}



void Action::snapshot(const std::string &label)
{
    // ignore if locked
    if (locked_)
        return;

    std::string snap_label = BaseToolkit::uniqueName(label, labels());

    // create snapshot id
    u_int64_t id = BaseToolkit::uniqueId();

    // get session to operate on
    Session *se = Mixer::manager().session();
    se->snapshots()->keys_.push_back(id);

    // threaded capture state of current session
    std::thread(captureMixerSession, se->snapshots()->xmlDoc_, SNAPSHOT_NODE(id), snap_label).detach();

#ifdef ACTION_DEBUG
    Log::Info("Snapshot stored %d '%s'", id, snap_label.c_str());
#endif
}

void Action::open(uint64_t snapshotid)
{
    if ( snapshot_id_ != snapshotid )
    {
        // get snapshot node of target in current session
        Session *se = Mixer::manager().session();
        snapshot_node_ = se->snapshots()->xmlDoc_->FirstChildElement( SNAPSHOT_NODE(snapshotid).c_str() );

        if (snapshot_node_)
            snapshot_id_ = snapshotid;
        else
            snapshot_id_ = 0;

        interpolator_node_ = nullptr;
    }
}

void Action::replace(uint64_t snapshotid)
{
    // ignore if locked or if no label is given
    if (locked_)
        return;

    if (snapshotid > 0)
        open(snapshotid);

    if (snapshot_node_) {
        // remember label
        std::string label = snapshot_node_->Attribute("label");

        // remove previous node
        Session *se = Mixer::manager().session();
        se->snapshots()->xmlDoc_->DeleteChild( snapshot_node_ );

        // threaded capture state of current session
        std::thread(captureMixerSession, se->snapshots()->xmlDoc_, SNAPSHOT_NODE(snapshot_id_), label).detach();

#ifdef ACTION_DEBUG
        Log::Info("Snapshot replaced %d '%s'", snapshot_id_, label.c_str());
#endif
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

void Action::setLabel (uint64_t snapshotid, const std::string &label)
{
    open(snapshotid);

    if (snapshot_node_)
        snapshot_node_->SetAttribute("label", label.c_str());
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

void Action::remove(uint64_t snapshotid)
{
    if (snapshotid > 0)
        open(snapshotid);

    if (snapshot_node_) {
        // remove
        Session *se = Mixer::manager().session();
        se->snapshots()->xmlDoc_->DeleteChild( snapshot_node_ );
        se->snapshots()->keys_.remove( snapshot_id_ );
    }

    snapshot_node_ = nullptr;
    snapshot_id_ = 0;
}

void Action::restore(uint64_t snapshotid)
{
    // lock
    locked_ = true;

    if (snapshotid > 0)
        open(snapshotid);

    if (snapshot_node_)
        // actually restore
        Mixer::manager().restore(snapshot_node_);

    // free
    locked_ = false;

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

