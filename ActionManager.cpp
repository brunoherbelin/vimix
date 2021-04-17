#include <string>
#include <algorithm>
#include <sstream>

#include "Log.h"
#include "View.h"
#include "Mixer.h"
#include "MixingGroup.h"
#include "tinyxml2Toolkit.h"
#include "SessionSource.h"
#include "SessionVisitor.h"
#include "SessionCreator.h"
#include "Settings.h"
#include "GlmToolkit.h"

#include "ActionManager.h"

#ifndef NDEBUG
#define ACTION_DEBUG
#endif

#define HISTORY_NODE(i) std::to_string(i).insert(0, "H").c_str()
#define SNAPSHOT_NODE(i) std::to_string(i).insert(0, "S").c_str()

using namespace tinyxml2;

Action::Action(): history_step_(0), history_max_step_(0)
{
}

void Action::init(const std::string &xml)
{
    // clean the history
    history_doc_.Clear();
    history_step_ = 0;
    history_max_step_ = 0;
    // start fresh
    store("Session start");

    // clean the snapshots
    snapshots_doc_.Clear();
    snapshots_.clear();

    if ( !xml.empty() ) {
        if ( XMLResultError( snapshots_doc_.Parse( xml.c_str() ) ))
            Log::Info("Failed to load snapshots");
        else
        {
            const XMLElement* N = snapshots_doc_.RootElement();
            for( ; N ; N=N->NextSiblingElement()) {

                char c;
                u_int64_t id = 0;
                std::istringstream nodename( N->Name() );
                nodename >> c >> id;
                snapshots_.push_back(id);
            }
        }
    }

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
        XMLElement *node = history_doc_.FirstChildElement( HISTORY_NODE(e) );
        if ( node )
            history_doc_.DeleteChild(node);
    }
    history_max_step_ = history_step_;

    // create history node
    XMLElement *sessionNode = history_doc_.NewElement( HISTORY_NODE(history_step_) );
    history_doc_.InsertEndChild(sessionNode);
    // label describes the action
    sessionNode->SetAttribute("label", label.c_str());
    // view indicates the view when this action occured
    sessionNode->SetAttribute("view", (int) Mixer::manager().view()->mode());

    // get session to operate on
    Session *se = Mixer::manager().session();

    // save all sources using source visitor
    SessionVisitor sv(&history_doc_, sessionNode);
    for (auto iter = se->begin(); iter != se->end(); iter++, sv.setRoot(sessionNode) )
        (*iter)->accept(sv);

    // debug
#ifdef ACTION_DEBUG
    Log::Info("Action stored %d '%s'", history_step_, label.c_str());
//        XMLSaveDoc(&xmlDoc_, "/home/bhbn/history.xml");
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
        const XMLElement *sessionNode = history_doc_.FirstChildElement( HISTORY_NODE(s));
        l = sessionNode->Attribute("label");
    }
    return l;
}

void Action::restore(uint target)
{
    // lock
    locked_ = true;

    // get history node of target step
    history_step_ = CLAMP(target, 1, history_max_step_);
    XMLElement *sessionNode = history_doc_.FirstChildElement( HISTORY_NODE(history_step_) );

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
    // ignore if locked or if no label is given
    if (locked_ || label.empty())
        return;

    // create snapshot id
    u_int64_t id = GlmToolkit::uniqueId();
    snapshots_.push_back(id);

    // create snapshot node
    XMLElement *sessionNode = snapshots_doc_.NewElement( SNAPSHOT_NODE(id) );
    snapshots_doc_.InsertEndChild(sessionNode);

    // label describes the snapshot
    sessionNode->SetAttribute("label", label.c_str());

    // get session to operate on
    Session *se = Mixer::manager().session();

    // save all sources using source visitor
    SessionVisitor sv(&snapshots_doc_, sessionNode);
    for (auto iter = se->begin(); iter != se->end(); iter++, sv.setRoot(sessionNode) )
        (*iter)->accept(sv);

    // TODO: copy action history instead?

    // debug
#ifdef ACTION_DEBUG
    Log::Info("Snapshot stored %d '%s'", id, label.c_str());
//        XMLSaveDoc(&xmlDoc_, "/home/bhbn/history.xml");
#endif
}

const char *Action::snapshotsDescription()
{
    // get compact string
    XMLPrinter xmlPrint;
    snapshots_doc_.Print( &xmlPrint );

    return xmlPrint.CStr();
}


std::string Action::label(uint64_t snapshotid) const
{
    std::string l = "";
    const XMLElement *sessionNode = snapshots_doc_.FirstChildElement( SNAPSHOT_NODE(snapshotid) );

    if (sessionNode) {
        l = sessionNode->Attribute("label");
    }
    return l;
}

void Action::setLabel (uint64_t snapshotid, const std::string &label)
{
    // get history node of target
    XMLElement *sessionNode = snapshots_doc_.FirstChildElement( SNAPSHOT_NODE(snapshotid) );

    if (sessionNode) {
        sessionNode->SetAttribute("label", label.c_str());
    }
}

void Action::remove(uint64_t snapshotid)
{
    // get history node of target
    XMLElement *sessionNode = snapshots_doc_.FirstChildElement( SNAPSHOT_NODE(snapshotid) );

    if (sessionNode) {
        snapshots_doc_.DeleteChild( sessionNode );
        snapshots_.remove(snapshotid);
    }
}

void Action::restore(uint64_t snapshotid)
{
    // lock
    locked_ = true;

    // get history node of target
    XMLElement *sessionNode = snapshots_doc_.FirstChildElement( SNAPSHOT_NODE(snapshotid) );

    if (sessionNode) {
        // actually restore
        Mixer::manager().restore(sessionNode);
    }

    // free
    locked_ = false;
}

