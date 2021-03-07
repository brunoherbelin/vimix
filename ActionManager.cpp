#include <string>
#include <algorithm>

#include "Log.h"
#include "View.h"
#include "Mixer.h"
#include "tinyxml2Toolkit.h"
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

void Action::store(const std::string &label, uint64_t id)
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
    // id indicates which object was modified
    sessionNode->SetAttribute("id", id);
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

    // what id was modified to get to this step ?
    // get history node of current step
    std::string nodename = "H" + std::to_string(step_);
    XMLElement *sessionNode = xmlDoc_.FirstChildElement( nodename.c_str() );
    uint64_t id = 0;
    sessionNode->QueryUnsigned64Attribute("id", &id);

    // restore always changes step_ to step_ - 1
    restore( step_ - 1, id);
}

void Action::redo()
{
    // not possible to go to max_step_ + 1
    if (step_ >= max_step_)
        return;

    // what id to modify to go to next step ?
    std::string nodename = "H" + std::to_string(step_ + 1);
    XMLElement *sessionNode = xmlDoc_.FirstChildElement( nodename.c_str() );
    uint64_t id = 0;
    sessionNode->QueryUnsigned64Attribute("id", &id);

    // restore always changes step_ to step_ + 1
    restore( step_ + 1, id);
}


void Action::stepTo(uint target)
{
    // get reasonable target
    uint t = CLAMP(target, 1, max_step_);

    // going backward
    if ( t < step_ ) {
        // go back one step at a time
        while (t < step_)
            undo();
    }
    // step forward
    else if ( t > step_ ) {
        // go forward one step at a time
        while (t > step_)
            redo();
    }
    // ignore t == step_
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

void Action::restore(uint target, uint64_t id)
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

    // we operate on the current session
    Session *se = Mixer::manager().session();
    if (se == nullptr)
        return;

    // sessionsources contains list of ids of all sources currently in the session
    SourceIdList sessionsources = se->getIdList();
//    for( auto it = sessionsources.begin(); it != sessionsources.end(); it++)
//        Log::Info("sessionsources  id %s", std::to_string(*it).c_str());

    // load history status:
    // - if a source exists, its attributes are updated, and that's all
    // - if a source does not exists (in session), it is created in the session
    SessionLoader loader( se );
    loader.load( sessionNode );

    // loadersources contains list of ids of all sources generated by loader
    SourceIdList loadersources = loader.getIdList();
//    for( auto it = loadersources.begin(); it != loadersources.end(); it++)
//        Log::Info("loadersources  id %s", std::to_string(*it).c_str());

    // remove intersect of both lists (sources were updated by SessionLoader)
    for( auto lsit = loadersources.begin(); lsit != loadersources.end(); ){
        auto ssit = std::find(sessionsources.begin(), sessionsources.end(), (*lsit));
        if ( ssit != sessionsources.end() ) {
            lsit = loadersources.erase(lsit);
            sessionsources.erase(ssit);
        }
        else
            lsit++;
    }
    // remaining ids in list sessionsources : to remove
    while ( !sessionsources.empty() ){
        Source *s = Mixer::manager().findSource( sessionsources.front() );
        if (s!=nullptr) {
#ifdef ACTION_DEBUG
            Log::Info("Delete   id %s", std::to_string(sessionsources.front() ).c_str());
#endif
            // remove the source from the mixer
            Mixer::manager().detach( s );
            // delete source from session
            se->deleteSource( s );
        }
        sessionsources.pop_front();
    }
    // remaining ids in list loadersources : to add
    while ( !loadersources.empty() ){
#ifdef ACTION_DEBUG
        Log::Info("Recreate id %s to %s", std::to_string(id).c_str(), std::to_string(loadersources.front()).c_str());
#endif
        // change the history to match the new id
        replaceSourceId(id, loadersources.front());
        // add the source to the mixer
        Mixer::manager().attach( Mixer::manager().findSource( loadersources.front() ) );
        loadersources.pop_front();
    }

    // free
    locked_ = false;

}


void Action::replaceSourceId(uint64_t previousid, uint64_t newid)
{
    // loop over every session history step
    XMLElement* historyNode = xmlDoc_.FirstChildElement("H1");
    for( ; historyNode ; historyNode = historyNode->NextSiblingElement())
    {
        // check if this history node references this id
        uint64_t id_history_ = 0;
        historyNode->QueryUnsigned64Attribute("id", &id_history_);
        if ( id_history_ == previousid )
            // change to new id
            historyNode->SetAttribute("id", newid);

        // loop over every source in session history
        XMLElement* sourceNode = historyNode->FirstChildElement("Source");
        for( ; sourceNode ; sourceNode = sourceNode->NextSiblingElement())
        {
            // check if this source node has this id
            uint64_t id_source_ = 0;
            sourceNode->QueryUnsigned64Attribute("id", &id_source_);
            if ( id_source_ == previousid )
                // change to new id
                sourceNode->SetAttribute("id", newid);
        }
    }

}
