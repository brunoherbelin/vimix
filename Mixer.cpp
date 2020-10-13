#include <algorithm>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <chrono>
#include <future>

//  GStreamer
#include <gst/gst.h>

#include <tinyxml2.h>
#include "tinyxml2Toolkit.h"
using namespace tinyxml2;

#include "defines.h"
#include "Settings.h"
#include "Log.h"
#include "View.h"
#include "SystemToolkit.h"
#include "SessionCreator.h"
#include "SessionVisitor.h"
#include "SessionSource.h"
#include "MediaSource.h"
#include "PatternSource.h"
#include "DeviceSource.h"
#include "StreamSource.h"
#include "ActionManager.h"

#include "Mixer.h"

#define THREADED_LOADING
static std::vector< std::future<Session *> > sessionLoaders_;
static std::vector< std::future<Session *> > sessionImporters_;
const std::chrono::milliseconds timeout_ = std::chrono::milliseconds(4);


// static multithreaded session saving
static void saveSession(const std::string& filename, Session *session)
{
    // lock access while saving
    session->lock();

    // creation of XML doc
    XMLDocument xmlDoc;

    XMLElement *version = xmlDoc.NewElement(APP_NAME);
    version->SetAttribute("major", XML_VERSION_MAJOR);
    version->SetAttribute("minor", XML_VERSION_MINOR);
    version->SetAttribute("size", session->numSource());
    version->SetAttribute("date", SystemToolkit::date_time_string().c_str());
    xmlDoc.InsertEndChild(version);

    // 1. list of sources
    XMLElement *sessionNode = xmlDoc.NewElement("Session");
    xmlDoc.InsertEndChild(sessionNode);
    SessionVisitor sv(&xmlDoc, sessionNode);
    for (auto iter = session->begin(); iter != session->end(); iter++, sv.setRoot(sessionNode) )
        // source visitor
        (*iter)->accept(sv);

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

        XMLElement *layer = xmlDoc.NewElement( "Layer" );
        layer->InsertEndChild( SessionVisitor::NodeToXML(*session->config(View::LAYER), &xmlDoc));
        views->InsertEndChild(layer);

        XMLElement *render = xmlDoc.NewElement( "Rendering" );
        render->InsertEndChild( SessionVisitor::NodeToXML(*session->config(View::RENDERING), &xmlDoc));
        views->InsertEndChild(render);
    }


    // save file to disk
    if ( XMLSaveDoc(&xmlDoc, filename) ) {
        // all ok
        // set session filename
        session->setFilename(filename);
        // cosmetics saved ok
        Rendering::manager().mainWindow().setTitle(filename);
        Settings::application.recentSessions.push(filename);
        Log::Notify("Session %s saved.", filename.c_str());

    }
    else {
        // error
        Log::Warning("Failed to save Session file %s.", filename.c_str());
    }

    session->unlock();
}

Mixer::Mixer() : session_(nullptr), back_session_(nullptr), current_view_(nullptr),
                 update_time_(GST_CLOCK_TIME_NONE), dt_(0.f)
{
    // unsused initial empty session
    session_ = new Session;
    current_source_ = session_->end();
    current_source_index_ = -1;

    // auto load if Settings ask to
    if ( Settings::application.recentSessions.load_at_start &&
         Settings::application.recentSessions.front_is_valid &&
         Settings::application.recentSessions.filenames.size() > 0 &&
         Settings::application.first_instance)
        load( Settings::application.recentSessions.filenames.front() );
    else
        // initializes with a new empty session
        clear();

    // this initializes with the current view
    setView( (View::Mode) Settings::application.current_view );
}

void Mixer::update()
{
    // sort-of garbage collector : just wait for 1 iteration
    // before deleting the previous session: this way, the sources
    // had time to end properly
    if (garbage_.size()>0) {
        delete garbage_.back();
        garbage_.pop_back();
    }

#ifdef THREADED_LOADING
    // if there is a session importer pending
    if (!sessionImporters_.empty()) {
        // check status of loader: did it finish ?
        if (sessionImporters_.back().wait_for(timeout_) == std::future_status::ready ) {
            // get the session loaded by this loader
            merge( sessionImporters_.back().get() );
            // done with this session loader
            sessionImporters_.pop_back();
        }
    }

    // if there is a session loader pending
    if (!sessionLoaders_.empty()) {
        // check status of loader: did it finish ?
        if (sessionLoaders_.back().wait_for(timeout_) == std::future_status::ready ) {
            // get the session loaded by this loader
            set( sessionLoaders_.back().get() );
            // done with this session loader
            sessionLoaders_.pop_back();
        }
    }
#endif

    // if a change of session is requested
    if (sessionSwapRequested_) {
        sessionSwapRequested_ = false;
        // sanity check
        if ( back_session_ ) {
            // swap front and back sessions
            swap();
            // set session filename
            Rendering::manager().mainWindow().setTitle(session_->filename());
            Settings::application.recentSessions.push(session_->filename());
        }
    }

    // if there is a source candidate for this session
    if (candidate_sources_.size() > 0) {
        // NB: only make the last candidate the current source in Mixing view
        insertSource(candidate_sources_.front(), candidate_sources_.size() > 1 ? View::INVALID : View::MIXING);
        candidate_sources_.pop_front();
    }

    // compute dt
    if (update_time_ == GST_CLOCK_TIME_NONE)
        update_time_ = gst_util_get_timestamp ();
    guint64 current_time = gst_util_get_timestamp ();
    // dt is in milisecond, with fractional precision (from micro seconds)
    dt_ = static_cast<float>( GST_TIME_AS_USECONDS(current_time - update_time_) * 0.001f);
    update_time_ = current_time;

    // update session and associated sources
    session_->update(dt_);

    // delete sources which failed update (one by one)
    Source *failure = session()->failedSource();
    if (failure != nullptr) {
        MediaSource *failedFile = dynamic_cast<MediaSource *>(failure);
        if (failedFile != nullptr) {
            Settings::application.recentImport.remove( failedFile->path() );
        }
        deleteSource(failure, false);
    }

    // update views
    mixing_.update(dt_);
    geometry_.update(dt_);
    layer_.update(dt_);
    transition_.update(dt_);

    // deep updates shall be performed only 1 frame
    View::need_deep_update_ = false;
}

void Mixer::draw()
{
    // draw the current view in the window
    current_view_->draw();

}

// manangement of sources
Source * Mixer::createSourceFile(const std::string &path)
{
    // ready to create a source
    Source *s = nullptr;

    // sanity check
    if ( SystemToolkit::file_exists( path ) ) {

        // test type of file by extension
        std::string ext = SystemToolkit::extension_filename(path);
        if ( ext == "mix" )
        {
            // create a session source
            SessionSource *ss = new SessionSource();
            ss->load(path);
            s = ss;
        }
        else {
            // (try to) create media source by default
            MediaSource *ms = new MediaSource;
            ms->setPath(path);
            s = ms;
        }

        // remember in recent media
        Settings::application.recentImport.push(path);
        Settings::application.recentImport.path = SystemToolkit::path_filename(path);

        // propose a new name based on uri
        s->setName(SystemToolkit::base_filename(path));

    }
    else {
        Settings::application.recentImport.remove(path);
        Log::Notify("File %s does not exist.", path.c_str());
    }

    return s;
}

Source * Mixer::createSourceRender()
{
    // ready to create a source
    RenderSource *s = new RenderSource(session_);

    // propose a new name based on session name
    s->setName(SystemToolkit::base_filename(session_->filename()));

    return s;
}

Source * Mixer::createSourceStream(const std::string &gstreamerpipeline)
{
    // ready to create a source
    GenericStreamSource *s = new GenericStreamSource;
    s->setDescription(gstreamerpipeline);

    // propose a new name based on pattern name
    std::string name = gstreamerpipeline.substr(0, gstreamerpipeline.find(" "));
    s->setName(name);

    return s;
}

Source * Mixer::createSourcePattern(uint pattern, glm::ivec2 res)
{
    // ready to create a source
    PatternSource *s = new PatternSource;
    s->setPattern(pattern, res);

    // propose a new name based on pattern name
    std::string name = Pattern::pattern_types[pattern];
    name = name.substr(0, name.find(" "));
    s->setName(name);

    return s;
}

Source * Mixer::createSourceDevice(const std::string &namedevice)
{
    // ready to create a source
    Source *s = Device::manager().createSource(namedevice);

    // propose a new name based on pattern name
    std::string name = namedevice.substr(0, namedevice.find(" "));
    s->setName(name);

    return s;
}


Source * Mixer::createSourceClone(const std::string &namesource)
{
    // ready to create a source
    Source *s = nullptr;

    // origin to clone is either the given name or the current
    SourceList::iterator origin = session_->end();
    if ( !namesource.empty() )
        origin =session_->find(namesource);
    else if (current_source_ != session_->end())
        origin = current_source_;

    // have an origin, can clone it
    if (origin != session_->end()) {

        // create a source
        s = (*origin)->clone();

        // propose new name (this automatically increments name)
        renameSource(s, (*origin)->name());
    }

    return s;
}

void Mixer::addSource(Source *s)
{
    if (s != nullptr) {
        candidate_sources_.push_back(s);
    }
}

void Mixer::insertSource(Source *s, View::Mode m)
{
    if ( s != nullptr )
    {
        // avoid duplicate name
        renameSource(s, s->name());

        // Add source to Session (ignored if source already in)
        SourceList::iterator sit = session_->addSource(s);

        // set a default depth to the new source
        layer_.setDepth(s);

        // set a default alpha to the new source
        mixing_.setAlpha(s);

        // add sources Nodes to all views
        attach(s);

        // new state in history manager
        Action::manager().store(s->name() + std::string(" inserted"), s->id());

        // if requested to show the source in a given view
        // (known to work for View::MIXING et TRANSITION: other views untested)
        if (m != View::INVALID) {

            // switch to this view to show source created
            setView(m);
            current_view_->update(0.f);
            current_view_->centerSource(s);

            // set this new source as current
            setCurrentSource( sit );
        }
    }
}

void Mixer::deleteSource(Source *s, bool withundo)
{
    if ( s != nullptr )
    {
        // keep name for log
        std::string name = s->name();
        uint64_t id = s->id();

        // remove source Nodes from all views
        detach(s);

        // delete source
        session_->deleteSource(s);

        // store new state in history manager
        if (withundo)
            Action::manager().store(name + std::string(" deleted"), id);

        // log
        Log::Notify("Source %s deleted.", name.c_str());
    }

    // cancel transition source in TRANSITION view
    if ( current_view_ == &transition_ ) {
        // cancel attachment
        transition_.attach(nullptr);
        // revert to mixing view
        setView(View::MIXING);
    }
}


void Mixer::attach(Source *s)
{
    if ( s != nullptr )
    {
        // force update
        s->touch();
        // attach to views
        mixing_.scene.ws()->attach( s->group(View::MIXING) );
        geometry_.scene.ws()->attach( s->group(View::GEOMETRY) );
        layer_.scene.ws()->attach( s->group(View::LAYER) );
    }
}

void Mixer::detach(Source *s)
{
    if ( s != nullptr )
    {
        // in case it was the current source...
        unsetCurrentSource();
        // in case it was selected..
        selection().remove(s);
        // detach from views
        mixing_.scene.ws()->detatch( s->group(View::MIXING) );
        geometry_.scene.ws()->detatch( s->group(View::GEOMETRY) );
        layer_.scene.ws()->detatch( s->group(View::LAYER) );
        transition_.scene.ws()->detatch( s->group(View::TRANSITION) );
    }
}



bool Mixer::concealed(Source *s)
{
    SourceList::iterator it = std::find(stash_.begin(), stash_.end(), s);
    return it != stash_.end();
}

void Mixer::conceal(Source *s)
{
    if ( !concealed(s) ) {
        // in case it was the current source...
        unsetCurrentSource();

        // in case it was selected..
        selection().remove(s);

        // store to stash
        stash_.push_front(s);

        // remove from session
        session_->removeSource(s);

        // detach from scene workspace, and put only in mixing background
        detach(s);
        mixing_.scene.bg()->attach( s->group(View::MIXING) );
    }
}

void Mixer::uncover(Source *s)
{
    SourceList::iterator it = std::find(stash_.begin(), stash_.end(), s);
    if ( it != stash_.end() ) {
        stash_.erase(it);

        mixing_.scene.bg()->detatch( s->group(View::MIXING) );
        attach(s);
        session_->addSource(s);
    }
}

void Mixer::deleteSelection()
{
    // get clones first : this way we store the history of deletion in the right order
    SourceList selection_clones_;
    for ( auto sit = selection().begin(); sit != selection().end(); sit++ ) {
        CloneSource *clone = dynamic_cast<CloneSource *>(*sit);
        if (clone)
            selection_clones_.push_back(clone);
    }
    // delete all clones
    while ( !selection_clones_.empty() ) {
        deleteSource( selection_clones_.front());
        selection_clones_.pop_front();
    }
    // empty the selection
    while ( !selection().empty() )
        deleteSource( selection().front()); // this also remove element from selection()

}

void Mixer::renameSource(Source *s, const std::string &newname)
{
    if ( s != nullptr )
    {
        // tentative new name
        std::string tentativename = newname;

        // refuse to rename to an empty name
        if ( newname.empty() )
            tentativename = "source";

        // search for a source of the name 'tentativename'
        std::string basename = tentativename;
        int count = 1;
        for( auto it = session_->begin(); it != session_->end(); it++){
            if ( s->id() != (*it)->id() && (*it)->name() == tentativename )
                tentativename = basename + std::to_string( ++count );
        }

        // ok to rename
        s->setName(tentativename);
    }
}

void Mixer::setCurrentSource(SourceList::iterator it)
{
    // nothing to do if already current
    if ( current_source_ == it )
        return;

    // clear current (even if 'it' is invalid)
    unsetCurrentSource();

    // change current if 'it' is valid
    if ( it != session_->end() ) {
        current_source_ = it;
        current_source_index_ = session_->index(current_source_);

        // set selection for this only source if not already part of a selection
        if (!selection().contains(*current_source_))
            selection().set(*current_source_);

        // show status as current
        (*current_source_)->setMode(Source::CURRENT);

        (*current_source_)->group(View::MIXING)->update_callbacks_.push_back(new BounceScaleCallback);
        (*current_source_)->group(View::LAYER)->update_callbacks_.push_back(new BounceScaleCallback);
    }

}

Source * Mixer::findSource (Node *node)
{
    SourceList::iterator it = session_->find(node);
    if (it != session_->end())
        return *it;
    return nullptr;
}


Source * Mixer::findSource (std::string namesource)
{
    SourceList::iterator it = session_->find(namesource);
    if (it != session_->end())
        return *it;
    return nullptr;
}

Source * Mixer::findSource (uint64_t id)
{
    SourceList::iterator it = session_->find(id);
    if (it != session_->end())
        return *it;
    return nullptr;
}


void Mixer::setCurrentSource(uint64_t id)
{
    setCurrentSource( session_->find(id) );
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

void Mixer::setCurrentIndex(int index)
{
    setCurrentSource( session_->at(index) );
}

void Mixer::setCurrentNext()
{
    if (session_->numSource() > 0) {

        SourceList::iterator it = current_source_;
        it++;

        if (it == session_->end())  {
            it = session_->begin();
        }

        setCurrentSource( it );
    }
}

void Mixer::unsetCurrentSource()
{
    // discard overlay for previously current source
    if ( current_source_ != session_->end() ) {

        // current source is part of a selection, just change status
        if (selection().size() > 1) {
            (*current_source_)->setMode(Source::SELECTED);
        }
        // current source is the only selected source, unselect too
        else
        {
            // remove from selection
            selection().remove( *current_source_ );
        }

        // deselect current source
        current_source_ = session_->end();
        current_source_index_ = -1;
    }
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
void Mixer::setView(View::Mode m)
{
    // special case when leaving transition view
    if ( current_view_ == &transition_ ) {
        // get the session detached from the transition view and set it as current session
        // NB: detatch() can return nullptr, which is then ignored.
        set ( transition_.detach() );
    }

    switch (m) {
    case View::TRANSITION:
        current_view_ = &transition_;
        break;
    case View::GEOMETRY:
        current_view_ = &geometry_;
        break;
    case View::LAYER:
        current_view_ = &layer_;
        break;
    case View::MIXING:
    default:
        current_view_ = &mixing_;
        break;
    }

    // need to deeply update view to apply eventual changes
    View::need_deep_update_ = true;

    Settings::application.current_view = (int) m;
}

View *Mixer::view(View::Mode m)
{
    switch (m) {
    case View::TRANSITION:
        return &transition_;
    case View::GEOMETRY:
        return &geometry_;
    case View::LAYER:
        return &layer_;
    case View::MIXING:
        return &mixing_;
    default:
        return current_view_;
    }
}

void Mixer::save()
{
    if (!session_->filename().empty())
        saveas(session_->filename());
}

void Mixer::saveas(const std::string& filename)
{
    // optional copy of views config
    session_->config(View::MIXING)->copyTransform( mixing_.scene.root() );
    session_->config(View::GEOMETRY)->copyTransform( geometry_.scene.root() );
    session_->config(View::LAYER)->copyTransform( layer_.scene.root() );

    // launch a thread to save the session
    std::thread (saveSession, filename, session_).detach();
}

void Mixer::load(const std::string& filename)
{

#ifdef THREADED_LOADING
    // load only one at a time
    if (sessionLoaders_.empty()) {
        // Start async thread for loading the session
        // Will be obtained in the future in update()
        sessionLoaders_.emplace_back( std::async(std::launch::async, Session::load, filename) );
    }
#else
    set( Session::load(filename) );
#endif
}

void Mixer::open(const std::string& filename)
{
    if (Settings::application.smooth_transition)
    {
        Log::Info("\nStarting transition to session %s", filename.c_str());

        // create special SessionSource to be used for the smooth transition
        SessionSource *ts = new SessionSource();
        // open filename if specified
        if (!filename.empty())
            ts->load(filename);
        // propose a new name based on uri
        renameSource(ts, SystemToolkit::base_filename(filename));

        // insert source and switch to transition view
        insertSource(ts, View::TRANSITION);

        // attach the SessionSource to the transition view
        transition_.attach(ts);
    }
    else
        load(filename);
}

void Mixer::import(const std::string& filename)
{
#ifdef THREADED_LOADING
    // import only one at a time
    if (sessionImporters_.empty()) {
        // Start async thread for loading the session
        // Will be obtained in the future in update()
        sessionImporters_.emplace_back( std::async(std::launch::async, Session::load, filename) );
    }
#else
    merge( Session::load(filename) );
#endif
}

void Mixer::merge(Session *session)
{
    if ( session == nullptr ) {
        Log::Warning("Failed to import Session.");
        return;
    }

    // import every sources
    for ( Source *s = session->popSource(); s != nullptr; s = session->popSource()) {
        renameSource(s, s->name());
        insertSource(s);
    }
}

void Mixer::swap()
{
    if (!back_session_)
        return;

    if (session_) {
        // clear selection
        selection().clear();
        // detatch current session's nodes from views
        for (auto source_iter = session_->begin(); source_iter != session_->end(); source_iter++)
            detach(*source_iter);
    }

    // swap back and front
    Session *tmp = session_;
    session_ = back_session_;
    back_session_ = tmp;

    // swap recorders
    back_session_->transferRecorders(session_);

    // attach new session's nodes to views
    for (auto source_iter = session_->begin(); source_iter != session_->end(); source_iter++)
        attach(*source_iter);

    // optional copy of views config
    mixing_.scene.root()->copyTransform( session_->config(View::MIXING) );
    geometry_.scene.root()->copyTransform( session_->config(View::GEOMETRY) );
    layer_.scene.root()->copyTransform( session_->config(View::LAYER) );

    // set resolution
    session_->setResolution( session_->config(View::RENDERING)->scale_ );

    // transfer fading
    session_->setFading( MAX(back_session_->fading(), session_->fading()), true );

    // request complete update for views
    View::need_deep_update_ = true;

    // no current source
    current_source_ = session_->end();
    current_source_index_ = -1;

    // reset timer
    update_time_ = GST_CLOCK_TIME_NONE;

    // delete back (former front session)
    garbage_.push_back(back_session_);
    back_session_ = nullptr;

    // reset History manager
    Action::manager().clear();

    // notification
    Log::Notify("Session %s loaded. %d source(s) created.", session_->filename().c_str(), session_->numSource());
}

void Mixer::close()
{
    if (Settings::application.smooth_transition)
    {
        // create empty SessionSource to be used for the smooth transition
        SessionSource *ts = new SessionSource();
        ts->load();

        // insert source and switch to transition view
        insertSource(ts, View::TRANSITION);

        // attach the SessionSource to the transition view
        transition_.attach(ts);
    }
    else
        clear();
}

void Mixer::clear()
{
    // delete previous back session if needed
    if (back_session_)
        garbage_.push_back(back_session_);

    // create empty session
    back_session_ = new Session;

    // swap current with empty
    sessionSwapRequested_ = true;

    Log::Info("New session ready.");
}

void Mixer::set(Session *s)
{
    if ( s == nullptr ) {
        Log::Warning("Session loading cancelled.");
        return;
    }

    // delete previous back session if needed
    if (back_session_)
        garbage_.push_back(back_session_);

    // set to new given session
    back_session_ = s;

    // swap current with given session
    sessionSwapRequested_ = true;
}

void Mixer::paste(const std::string& clipboard)
{
    if (clipboard.empty())
        return;

    tinyxml2::XMLDocument xmlDoc;
    tinyxml2::XMLError eResult = xmlDoc.Parse(clipboard.c_str());
    if ( XMLResultError(eResult))
        return;

    tinyxml2::XMLElement *root = xmlDoc.FirstChildElement(APP_NAME);
    if ( root == nullptr )
        return;

    SessionLoader loader( session_ );

    XMLElement* sourceNode = root->FirstChildElement("Source");
    for( ; sourceNode ; sourceNode = sourceNode->NextSiblingElement())
    {
        addSource(loader.cloneOrCreateSource(sourceNode));
    }

}
