#include <string>
#include <algorithm>

#include "Log.h"
#include "View.h"
#include "Mixer.h"
#include "MixingGroup.h"
#include "tinyxml2Toolkit.h"
#include "SessionSource.h"
#include "SessionVisitor.h"
#include "SessionCreator.h"
#include "Settings.h"

#include "ActionManager.h"

#ifndef NDEBUG
#define ACTION_DEBUG
#endif

using namespace tinyxml2;

Action::Action(): step_(0), max_step_(0)
{
}

void Action::clear()
{
    // clean the history
    xmlDoc_.Clear();
    step_ = 0;
    max_step_ = 0;

    // start fresh
    store("Session start");
}

void Action::store(const std::string &label)
{
    // ignore if locked or if no label is given
    if (locked_ || label.empty())
        return;

    // incremental naming of history nodes
    step_++;
    std::string nodename = "H" + std::to_string(step_);

    // erase future
    for (uint e = step_; e <= max_step_; e++) {
        std::string name = "H" + std::to_string(e);
        XMLElement *node = xmlDoc_.FirstChildElement( name.c_str() );
        if ( node )
            xmlDoc_.DeleteChild(node);
    }
    max_step_ = step_;

    // create history node
    XMLElement *sessionNode = xmlDoc_.NewElement( nodename.c_str() );
    xmlDoc_.InsertEndChild(sessionNode);
    // label describes the action
    sessionNode->SetAttribute("label", label.c_str());
    // view indicates the view when this action occured
    sessionNode->SetAttribute("view", (int) Mixer::manager().view()->mode());

    // get session to operate on
    Session *se = Mixer::manager().session();

    // save all sources using source visitor
    SessionVisitor sv(&xmlDoc_, sessionNode);
    for (auto iter = se->begin(); iter != se->end(); iter++, sv.setRoot(sessionNode) )
        (*iter)->accept(sv);

    // debug
#ifdef ACTION_DEBUG
    Log::Info("Action stored %s '%s'", nodename.c_str(), label.c_str());
//        XMLSaveDoc(&xmlDoc_, "/home/bhbn/history.xml");
#endif
}

void Action::undo()
{
    // not possible to go to 1 -1 = 0
    if (step_ <= 1)
        return;

    // restore always changes step_ to step_ - 1
    restore( step_ - 1);
}

void Action::redo()
{
    // not possible to go to max_step_ + 1
    if (step_ >= max_step_)
        return;

    // restore always changes step_ to step_ + 1
    restore( step_ + 1);
}


void Action::stepTo(uint target)
{
    // get reasonable target
    uint t = CLAMP(target, 1, max_step_);

    // ignore t == step_
    if (t != step_)
        restore(t);
}

std::string Action::label(uint s) const
{
    std::string l = "";

    if (s > 0 && s <= max_step_) {
        std::string nodename = "H" + std::to_string(s);
        const XMLElement *sessionNode = xmlDoc_.FirstChildElement( nodename.c_str() );
        l = sessionNode->Attribute("label");
    }
    return l;
}

void Action::restore(uint target)
{
    // lock
    locked_ = true;

    // get history node of target step
    step_ = CLAMP(target, 1, max_step_);
    std::string nodename = "H" + std::to_string(step_);
    XMLElement *sessionNode = xmlDoc_.FirstChildElement( nodename.c_str() );

    // ask view to refresh, and switch to action view if user prefers
    int view = Settings::application.current_view ;
    if (Settings::application.action_history_follow_view)
        sessionNode->QueryIntAttribute("view", &view);
    Mixer::manager().setView( (View::Mode) view);

#ifdef ACTION_DEBUG
    Log::Info("Restore %s '%s' ", nodename.c_str(), sessionNode->Attribute("label"));
#endif

    //
    // compare source lists
    //

    // we operate on the current session
    Session *se = Mixer::manager().session();
    if (se == nullptr)
        return;

    // sessionsources contains list of ids of all sources currently in the session (before loading)
    SourceIdList session_sources = se->getIdList();
//    for( auto it = sessionsources.begin(); it != sessionsources.end(); it++)
//        Log::Info("sessionsources  id %s", std::to_string(*it).c_str());

    // load history status:
    // - if a source exists, its attributes are updated, and that's all
    // - if a source does not exists (in current session), it is created inside the session
    SessionLoader loader( se );
    loader.load( sessionNode );

    // loaded_sources contains map of xml ids of all sources treated by loader
    std::map< uint64_t, Source* > loaded_sources = loader.getSources();

    // remove intersect of both lists (sources were updated by SessionLoader)
    for( auto lsit = loaded_sources.begin(); lsit != loaded_sources.end(); ){
        auto ssit = std::find(session_sources.begin(), session_sources.end(), (*lsit).first);
        if ( ssit != session_sources.end() ) {
            lsit = loaded_sources.erase(lsit);
            session_sources.erase(ssit);
        }
        else
            lsit++;
    }

    // remaining ids in list sessionsources : to remove
    while ( !session_sources.empty() ){
        Source *s = Mixer::manager().findSource( session_sources.front() );
        if (s!=nullptr) {
#ifdef ACTION_DEBUG
            Log::Info("Delete   id %s\n", std::to_string(session_sources.front() ).c_str());
#endif
            // remove the source from the mixer
            Mixer::manager().detach( s );
            // delete source from session
            se->deleteSource( s );
        }
        session_sources.pop_front();
    }

    // remaining sources in list loaded_sources : to add
    for ( auto lsit = loaded_sources.begin(); lsit != loaded_sources.end(); lsit++)
    {
#ifdef ACTION_DEBUG
        Log::Info("Recreate id %s to %s\n", std::to_string((*lsit).first).c_str(), std::to_string((*lsit).second->id()).c_str());
#endif
        // attach created source
        Mixer::manager().attach( (*lsit).second );

    }

    //
    // compare mixing groups
    //

    // Get the list of mixing groups in the xml loader
    std::list< SourceList > loadergroups = loader.getMixingGroups();

    // clear all session groups
    auto group_iter = se->beginMixingGroup();
    while ( group_iter != se->endMixingGroup() )
        group_iter = se->deleteMixingGroup(group_iter);

    // apply all changes creating or modifying groups in the session
    // (after this, new groups are created and existing groups are adjusted)
    for (auto group_loader_it = loadergroups.begin(); group_loader_it != loadergroups.end(); group_loader_it++) {
        se->link( *group_loader_it, Mixer::manager().view(View::MIXING)->scene.fg() );
    }

    // free
    locked_ = false;

}

