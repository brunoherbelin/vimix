#include <algorithm>
#include <thread>

#include <tinyxml2.h>
#include "tinyxml2Toolkit.h"
using namespace tinyxml2;

#include "defines.h"
#include "Log.h"
#include "View.h"
#include "Primitives.h"
#include "Mesh.h"
#include "FrameBuffer.h"
#include "Settings.h"
#include "SystemToolkit.h"
#include "ImageProcessingShader.h"
#include "GarbageVisitor.h"
#include "SessionVisitor.h"
#include "SessionCreator.h"
#include "MediaPlayer.h"

#include "Mixer.h"

// static objects for multithreaded session loading
static std::atomic<bool> sessionLoadPending_ = false;
static std::atomic<bool> sessionLoadFinished_ = false;
static std::string sessionThreadFilename_ = "";

static void loadSession(const std::string& filename, Session *session)
{
     sessionLoadPending_ = true;
     sessionLoadFinished_ = false;
     sessionThreadFilename_ = "";

     // actual loading of xml file
     SessionCreator creator( session );

     if (!creator.load(filename)) {
         // error loading
         Log::Info("Failed to load Session file %s.", filename.c_str());
     }
     else {
         // loaded ok
         Log::Info("Session file %s loaded. %d source(s) created.", filename.c_str(), session->numSource());
     }

     sessionThreadFilename_ = filename;
     sessionLoadFinished_ = true;
     sessionLoadPending_ = false;
}

// static objects for multithreaded session saving
static std::atomic<bool> sessionSavePending_ = false;
static std::atomic<bool> sessionSaveFinished_ = false;

static void saveSession(const std::string& filename, Session *session)
{
    // reset
    sessionSavePending_ = true;
    sessionSaveFinished_ = false;
    sessionThreadFilename_ = "";

    // creation of XML doc
    XMLDocument xmlDoc;

    XMLElement *version = xmlDoc.NewElement(APP_NAME);
    version->SetAttribute("major", XML_VERSION_MAJOR);
    version->SetAttribute("minor", XML_VERSION_MINOR);
    xmlDoc.InsertEndChild(version);

    // 1. list of sources
    XMLElement *sessionNode = xmlDoc.NewElement("Session");
    xmlDoc.InsertEndChild(sessionNode);
    SourceList::iterator iter;
    for (iter = session->begin(); iter != session->end(); iter++)
    {
        SessionVisitor sv(&xmlDoc, sessionNode);
        // source visitor
        (*iter)->accept(sv);
    }

    // 2. config of views
    XMLElement *views = xmlDoc.NewElement("Views");
    xmlDoc.InsertEndChild(views);
    {
        XMLElement *mixing = xmlDoc.NewElement( "Mixing" );
        mixing->InsertEndChild( SessionVisitor::NodeToXML(*session->config(View::MIXING), &xmlDoc));
        views->InsertEndChild(mixing);

        XMLElement *geometry = xmlDoc.NewElement( "Geometry" );
        geometry->InsertEndChild( SessionVisitor::NodeToXML(*session->config(View::GEOMETRY), &xmlDoc));
        views->InsertEndChild(geometry);
    }

    // save file to disk
    XMLSaveDoc(&xmlDoc, filename);

    // all ok
    sessionThreadFilename_ = filename;
    sessionSaveFinished_ = true;
    sessionSavePending_ = false;
}

Mixer::Mixer() : session_(nullptr), back_session_(nullptr), current_view_(nullptr)
{
    // this initializes with a new empty session
    newSession();

    // this initializes with the current view
    setCurrentView( (View::Mode) Settings::application.current_view );
}


void Mixer::update()
{
    // swap front and back sessions when loading is finished
    if (sessionLoadFinished_) {
        swap();
        sessionFilename_ = sessionThreadFilename_;
        sessionLoadFinished_ = false;
    }
    if (sessionSaveFinished_) {
        sessionFilename_ = sessionThreadFilename_;
        sessionSaveFinished_ = false;
    }

    // compute dt
    if (update_time_ == GST_CLOCK_TIME_NONE)
        update_time_ = gst_util_get_timestamp ();
    gint64 current_time = gst_util_get_timestamp ();
    float dt = static_cast<float>( GST_TIME_AS_MSECONDS(current_time - update_time_) ) * 0.001f;
    update_time_ = current_time;

    // update session and associted sources
    session_->update(dt);

    // update views
    mixing_.update(dt);
    geometry_.update(dt);
    // TODO other views


}

void Mixer::draw()
{
    // draw the current view in the window
    current_view_->draw();
}

// manangement of sources
void Mixer::createSourceMedia(std::string uri)
{
    // create source
    MediaSource *m = new MediaSource();
    m->setURI(uri);

    // propose a new name based on uri
    renameSource(m, SystemToolkit::base_filename(uri));

    // add to mixer
    insertSource(m);
}

void Mixer::insertSource(Source *s)
{
    // Add source to Session and set it as current
    setCurrentSource( session_->addSource(s) );

    // add sources Nodes to ALL views
    // Mixing Node
    mixing_.scene.fg()->attach(s->group(View::MIXING));
    // Geometry Node
    geometry_.scene.fg()->attach(s->group(View::GEOMETRY));

}
void Mixer::deleteSource(Source *s)
{
//    SessionVisitor visit;
//    geometry_.scene.accept(visit);
//    visit.doc()->SaveFile("./geombefore.xml");
//    SessionVisitor visitm;
//    mixing_.scene.accept(visitm);
//    visitm.doc()->SaveFile("./mixbefore.xml");

    unsetCurrentSource();

    // remove source Nodes from views
    mixing_.scene.fg()->detatch( s->group(View::MIXING) );
    geometry_.scene.fg()->detatch( s->group(View::GEOMETRY) );

    // delete source
    session_->deleteSource(s);

//    SessionVisitor visit2;
//    geometry_.scene.accept(visit2);
//    visit2.doc()->SaveFile("./geomafter.xml");
//    SessionVisitor visit2m;
//    mixing_.scene.accept(visit2m);
//    visit2m.doc()->SaveFile("./mixmafter.xml");
}

void Mixer::renameSource(Source *s, const std::string &newname)
{
    // tentative new name
    std::string tentativename = newname;

    // refuse to rename to an empty name
    if ( newname.empty() )
        tentativename = "source";

    // trivial case : same name as current
    if ( tentativename == s->name() )
        return;

    // search for a source of the name 'tentativename'
    std::string basename = tentativename;
    int count = 1;
    while ( std::find_if(session_->begin(), session_->end(), hasName(tentativename)) != session_->end() ) {
        tentativename = basename + std::to_string(++count);
    }

    // ok to rename
    s->setName(tentativename);
}

void Mixer::setCurrentSource(SourceList::iterator it)
{
    // nothing to do if already current
    if ( current_source_ == it )
        return;

    // change current
    if ( it != session_->end() ) {
        current_source_ = it;
        current_source_index_ = session_->index(it);
        (*current_source_)->setOverlayVisible(true);
    }
    // default
    else
        unsetCurrentSource();
}

void Mixer::setCurrentSource(Node *node)
{
    setCurrentSource( session_->find(node) );
}

void Mixer::setCurrentSource(std::string namesource)
{
    setCurrentSource( session_->find(namesource) );
}

void Mixer::setCurrentSource(Source *s)
{
    setCurrentSource( session_->find(s) );
}

void Mixer::setCurrentSource(int index)
{
    setCurrentSource( session_->find(index) );
}

void Mixer::unsetCurrentSource()
{
    // discard overlay for previously current source
    if ( current_source_ != session_->end() )
        (*current_source_)->setOverlayVisible(false);

    // deselect current source
    current_source_ = session_->end();
    current_source_index_ = -1;
}

int Mixer::indexCurrentSource()
{
    return current_source_index_;
}

Source *Mixer::currentSource()
{
    if ( current_source_ == session_->end() )
        return nullptr;
    else {
        return *(current_source_);
    }
}

// management of view
void Mixer::setCurrentView(View::Mode m)
{
    switch (m) {
    case View::GEOMETRY:
        current_view_ = &geometry_;
        break;
    case View::MIXING:
    default:
        current_view_ = &mixing_;
        break;
    }

    Settings::application.current_view = (int) m;
}

View *Mixer::getView(View::Mode m)
{
    switch (m) {
    case View::GEOMETRY:
        return &geometry_;
    case View::MIXING:
        return &mixing_;
    default:
        return nullptr;
    }
}

View *Mixer::currentView()
{
    return current_view_;
}

void Mixer::save()
{
    if (!sessionFilename_.empty())
        saveas(sessionFilename_);
}

void Mixer::saveas(const std::string& filename)
{
    // optional copy of views config
    session_->config(View::MIXING)->copyTransform( mixing_.scene.root() );
    session_->config(View::GEOMETRY)->copyTransform( geometry_.scene.root() );

    // launch a thread to save the session
    std::thread (saveSession, filename, session_).detach();

//    XMLDocument xmlDoc;

//    XMLElement *version = xmlDoc.NewElement(APP_NAME);
//    version->SetAttribute("major", XML_VERSION_MAJOR);
//    version->SetAttribute("minor", XML_VERSION_MINOR);
//    xmlDoc.InsertEndChild(version);

//    // block: list of sources
//    XMLElement *session = xmlDoc.NewElement("Session");
//    xmlDoc.InsertEndChild(session);
//    SourceList::iterator iter;
//    for (iter = session_->begin(); iter != session_->end(); iter++)
//    {
//        SessionVisitor sv(&xmlDoc, session);
//        // source visitor
//        (*iter)->accept(sv);
//    }

//    // block: config of views
//    XMLElement *views = xmlDoc.NewElement("Views");
//    xmlDoc.InsertEndChild(views);
//    {
//        XMLElement *mixing = xmlDoc.NewElement( "Mixing" );
//        mixing->InsertEndChild( SessionVisitor::NodeToXML(*mixing_.scene.root(), &xmlDoc));
//        views->InsertEndChild(mixing);

//        XMLElement *geometry = xmlDoc.NewElement( "Geometry" );
//        geometry->InsertEndChild( SessionVisitor::NodeToXML(*geometry_.scene.root(), &xmlDoc));
//        views->InsertEndChild(geometry);
//    }

//    // save file
//    XMLSaveDoc(&xmlDoc, filename);
}

void Mixer::open(const std::string& filename)
{
    if (back_session_)
        delete back_session_;

    // create empty session
    back_session_ = new Session;

    // launch a thread to load the session into back_session
    std::thread (loadSession, filename, back_session_).detach();

}


void Mixer::swap()
{
    if (!back_session_)
        return;

    if (session_) {
        // detatch current session's nodes from views
        for (auto source_iter = session_->begin(); source_iter != session_->end(); source_iter++)
        {
            mixing_.scene.fg()->detatch( (*source_iter)->group(View::MIXING) );
            geometry_.scene.fg()->detatch( (*source_iter)->group(View::GEOMETRY) );
        }
    }

    // swap back and front
    Session *tmp = session_;
    session_ = back_session_;
    back_session_ = tmp;

    // attach new session's nodes to views
    for (auto source_iter = session_->begin(); source_iter != session_->end(); source_iter++)
    {
        mixing_.scene.fg()->attach( (*source_iter)->group(View::MIXING) );
        geometry_.scene.fg()->attach( (*source_iter)->group(View::GEOMETRY) );
    }

    // optional copy of views config
    mixing_.scene.root()->copyTransform( session_->config(View::MIXING) );
    geometry_.scene.root()->copyTransform( session_->config(View::GEOMETRY) );

    // no current source
    current_source_ = session_->end();
    current_source_index_ = -1;

    // reset timer
    update_time_ = GST_CLOCK_TIME_NONE;
}


void Mixer::newSession()
{
    // delete previous back session if needed
    if (back_session_)
        delete back_session_;

    // create empty session
    back_session_ = new Session;

    // swap current with empty
    swap();

    // default view config
    mixing_.restoreSettings();
    geometry_.restoreSettings();

    sessionFilename_ = "newsession.vmx";
}
