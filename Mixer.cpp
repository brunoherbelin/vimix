#include <algorithm>

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
#include "SessionVisitor.h"
#include "SessionCreator.h"

#include "MediaPlayer.h"

#include "Mixer.h"

Mixer::Mixer() : session_(nullptr), current_view_(nullptr)
{
    // this initializes with a new empty session
    newSession();

    // this initializes with the current view
    setCurrentView( (View::Mode) Settings::application.current_view );
}


void Mixer::update()
{
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
    mixing_.scene.root()->addChild(s->group(View::MIXING));
    // Geometry Node
    geometry_.scene.root()->addChild(s->group(View::GEOMETRY));

}
void Mixer::deleteSource(Source *s)
{
    unsetCurrentSource();
    session_->deleteSource(s);
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
    unsetCurrentSource();
    if ( it != session_->end() ) {
        current_source_ = it;
        current_source_index_ = session_->index(it);
        (*current_source_)->setOverlayVisible(true);
    }
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


void Mixer::save(const std::string& filename)
{
    XMLDocument xmlDoc;

    XMLElement *version = xmlDoc.NewElement(APP_NAME);
    version->SetAttribute("major", XML_VERSION_MAJOR);
    version->SetAttribute("minor", XML_VERSION_MINOR);
    xmlDoc.InsertEndChild(version);

    // block: list of sources
    XMLElement *session = xmlDoc.NewElement("Session");
    xmlDoc.InsertEndChild(session);
    SourceList::iterator iter;
    for (iter = session_->begin(); iter != session_->end(); iter++)
    {
        SessionVisitor sv(&xmlDoc, session);
        (*iter)->accept(sv);
    }

    // block: config of views
    XMLElement *views = xmlDoc.NewElement("Views");
    xmlDoc.InsertEndChild(views);
    {
        XMLElement *mixing = xmlDoc.NewElement( "Mixing" );
        mixing->InsertEndChild( SessionVisitor::NodeToXML(*mixing_.scene.root(), &xmlDoc));
        views->InsertEndChild(mixing);

        XMLElement *geometry = xmlDoc.NewElement( "Geometry" );
        geometry->InsertEndChild( SessionVisitor::NodeToXML(*geometry_.scene.root(), &xmlDoc));
        views->InsertEndChild(geometry);
    }

    // save file
    XMLSaveDoc(&xmlDoc, filename);
}

bool Mixer::open(const std::string& filename)
{
    XMLDocument xmlDoc;
    XMLError eResult = xmlDoc.LoadFile(filename.c_str());
    if (XMLResultError(eResult))
        return false;

    XMLElement *version = xmlDoc.FirstChildElement(APP_NAME);
    if (version == nullptr) {
        Log::Warning("Not a %s session file: %s", APP_NAME, filename.c_str());
        return false;
    }
    int version_major = -1, version_minor = -1;
    version->QueryIntAttribute("major", &version_major); // TODO incompatible if major is different?
    version->QueryIntAttribute("minor", &version_minor);
    if (version_major != XML_VERSION_MAJOR || version_minor != XML_VERSION_MINOR){
        Log::Warning("%s is in an older versions of session file format. Data might be lost.", filename.c_str());
    }

    // ok, ready to read sources
    SessionCreator creator( xmlDoc.FirstChildElement("Session") );
    Session *new_session = creator.session();

    // if all good, change to new session
    if (new_session) {

        // validate new session
        newSession( new_session );

        // insert nodes of sources into views
        SourceList::iterator iter;
        for (iter = new_session->begin(); iter != new_session->end(); iter++)
        {
            mixing_.scene.root()->addChild( (*iter)->group(View::MIXING) );
            geometry_.scene.root()->addChild( (*iter)->group(View::GEOMETRY) );
        }

    }
    else {
        Log::Warning("Session file %s not loaded.", filename.c_str());
        return false;
    }

    // config of view settings (optionnal)
    XMLElement *views = xmlDoc.FirstChildElement("Views");
    if (views != nullptr) {
        // ok, ready to read views
        SessionCreator::XMLToNode( views->FirstChildElement("Mixing"), *mixing_.scene.root());
        SessionCreator::XMLToNode( views->FirstChildElement("Geometry"), *geometry_.scene.root());
    }

    Log::Info("Session file %s loaded. %d source(s) created.", filename.c_str(), session_->numSource());

    return true;
}

void Mixer::newSession(Session *newsession)
{
    // TODO : remove roots of scenes for views?
//    mixing_.scene.clear();
//    geometry_.scene.clear();

    // delete session : delete all sources
    if (session_)
        delete session_;

    // validate new session
    if (newsession)
        session_ = newsession;
    else
        session_ = new Session;

    // no current source
    current_source_ = session_->end();
    current_source_index_ = -1;

    // reset timer
    update_time_ = GST_CLOCK_TIME_NONE;

    // default views
    mixing_.restoreSettings();
    geometry_.restoreSettings();
}
