#include <algorithm>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <chrono>
#include <future>
#include <sstream>

//  GStreamer
#include <gst/gst.h>

#include <tinyxml2.h>
#include "tinyxml2Toolkit.h"

#include "defines.h"
#include "Settings.h"
#include "Log.h"
#include "View.h"
#include "ImageShader.h"
#include "BaseToolkit.h"
#include "SystemToolkit.h"
#include "SessionCreator.h"
#include "SessionVisitor.h"
#include "SessionSource.h"
#include "MediaSource.h"
#include "PatternSource.h"
#include "DeviceSource.h"
#include "StreamSource.h"
#include "NetworkSource.h"
#include "ActionManager.h"
#include "MixingGroup.h"
#include "Streamer.h"

#include "Mixer.h"

#define THREADED_LOADING
static std::vector< std::future<Session *> > sessionLoaders_;
static std::vector< std::future<Session *> > sessionImporters_;
static std::vector< SessionSource * > sessionSourceToImport_;
const std::chrono::milliseconds timeout_ = std::chrono::milliseconds(4);


// static multithreaded session saving
static void saveSession(const std::string& filename, Session *session)
{
    // lock access while saving
    session->lock();

    // save file to disk
    if ( SessionVisitor::saveSession(filename, session) ) {
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

Mixer::Mixer() : session_(nullptr), back_session_(nullptr), current_view_(nullptr), dt_(0.f), dt__(0.f)
{
    // unsused initial empty session
    session_ = new Session;
    current_source_ = session_->end();
    current_source_index_ = -1;

    // auto load if Settings ask to
    if ( Settings::application.recentSessions.load_at_start &&
         Settings::application.recentSessions.front_is_valid &&
         Settings::application.recentSessions.filenames.size() > 0 &&
         Settings::application.fresh_start)
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
            // FIXME: shouldn't we delete the imported session?
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

    // if there is a session source to import
    if (!sessionSourceToImport_.empty()) {
        // get the session source to be imported
        SessionSource *source = sessionSourceToImport_.back();
        // merge (&delete) the session inside this session source
        merge( source );
        // done with this session source
        sessionSourceToImport_.pop_back();
    }

    // if a change of session is requested
    if (sessionSwapRequested_) {
        sessionSwapRequested_ = false;
        // sanity check
        if ( back_session_ ) {
            // swap front and back sessions
            swap();
            ++View::need_deep_update_;
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
    static GTimer *timer = g_timer_new ();
    dt_ = g_timer_elapsed (timer, NULL) * 1000.0;
    g_timer_start(timer);

    // compute stabilized dt__
    dt__ = 0.05f * dt_ + 0.95f * dt__;

    // update session and associated sources
    session_->update(dt_);

    // grab frames to recorders & streamers
    FrameGrabbing::manager().grabFrame(session_->frame(), dt_);

    // delete sources which failed update (one by one)
    Source *failure = session()->failedSource();
    if (failure != nullptr) {
        // failed media: remove it from the list of imports
        MediaSource *failedMedia = dynamic_cast<MediaSource *>(failure);
        if (failedMedia != nullptr) {
            Settings::application.recentImport.remove( failedMedia->path() );
        }
        // failed Render loopback: replace it with one matching the current session
        RenderSource *failedRender = dynamic_cast<RenderSource *>(failure);
        if (failedRender != nullptr) {
            if ( recreateSource(failedRender) )
                failure = nullptr; // prevent delete (already done in recreateSource)
        }
        // delete the source
        deleteSource(failure);
    }

    // update views
    mixing_.update(dt_);
    geometry_.update(dt_);
    layer_.update(dt_);
    appearance_.update(dt_);
    transition_.update(dt_);

    // deep update was performed
    if  (View::need_deep_update_ > 0)
        --View::need_deep_update_;
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
            SessionFileSource *ss = new SessionFileSource;
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
    RenderSource *s = new RenderSource;
    s->setSession(session_);

    // propose a new name based on session name
    if ( !session_->filename().empty() )
        s->setName(SystemToolkit::base_filename(session_->filename()));
    else
        s->setName("Output");

    return s;
}

Source * Mixer::createSourceStream(const std::string &gstreamerpipeline)
{
    // ready to create a source
    GenericStreamSource *s = new GenericStreamSource;
    s->setDescription(gstreamerpipeline);

    // propose a new name based on pattern name
    s->setName( gstreamerpipeline.substr(0, gstreamerpipeline.find(" ")) );

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
    s->setName( namedevice.substr(0, namedevice.find(" ")) );

    return s;
}


Source * Mixer::createSourceNetwork(const std::string &nameconnection)
{
    // ready to create a source
    NetworkSource *s = new NetworkSource;
    s->setConnection(nameconnection);

    // propose a new name based on address
    s->setName(nameconnection);

    return s;
}


Source * Mixer::createSourceGroup()
{
    SessionGroupSource *s = new SessionGroupSource;
    s->setResolution( session_->frame()->resolution() );

    // propose a new name
    s->setName("Group");

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
    if (origin != session_->end())
        // create a source
        s = (*origin)->clone();

    return s;
}

void Mixer::addSource(Source *s)
{
    if (s != nullptr)
        candidate_sources_.push_back(s);
}

void Mixer::insertSource(Source *s, View::Mode m)
{
    if ( s != nullptr )
    {
        // avoid duplicate name
        renameSource(s);

        // Add source to Session (ignored if source already in)
        SourceList::iterator sit = session_->addSource(s);

        // set a default depth to the new source
        layer_.setDepth(s);

        // set a default alpha to the new source
        mixing_.setAlpha(s);

        // add sources Nodes to all views
        attach(s);

        // new state in history manager
        Action::manager().store(s->name() + std::string(" source inserted"));

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


bool Mixer::replaceSource(Source *from, Source *to)
{
    if ( from == nullptr || to == nullptr)
        return false;

    // rename
    renameSource(to, from->name());

    // remove source Nodes from all views
    detach(from);

    // copy all transforms
    to->group(View::MIXING)->copyTransform( from->group(View::MIXING) );
    to->group(View::GEOMETRY)->copyTransform( from->group(View::GEOMETRY) );
    to->group(View::LAYER)->copyTransform( from->group(View::LAYER) );
    to->group(View::MIXING)->copyTransform( from->group(View::MIXING) );

    // TODO copy all filters


    // add source Nodes to all views
    attach(to);

    // add source
    session_->addSource(to);

    // delete source
    session_->deleteSource(from);

    return true;
}


bool Mixer::recreateSource(Source *s)
{
    if ( s == nullptr )
        return false;

    // get the xml description from this source, and exit if not wellformed
    tinyxml2::XMLDocument xmlDoc;
    tinyxml2::XMLElement* sourceNode = SessionLoader::firstSourceElement(SessionVisitor::getClipboard(s), xmlDoc);
    if ( sourceNode == nullptr )
        return false;

    // actually create the source with SessionLoader using xml description
    SessionLoader loader( session_ );
    Source *replacement = loader.createSource(sourceNode, SessionLoader::DUPLICATE); // not clone
    if (replacement == nullptr)
        return false;

    // remove source Nodes from all views
    detach(s);

    // delete source
    session_->deleteSource(s);

    // add sources Nodes to all views
    attach(replacement);

    // add source
    session_->addSource(replacement);

    return true;
}

void Mixer::deleteSource(Source *s)
{
    if ( s != nullptr )
    {
        // keep name for log
        std::string name = s->name();

        // remove source Nodes from all views
        detach(s);

        // delete source
        session_->deleteSource(s);

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
        appearance_.scene.ws()->attach( s->group(View::TEXTURE) );
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
        mixing_.scene.ws()->detach( s->group(View::MIXING) );
        geometry_.scene.ws()->detach( s->group(View::GEOMETRY) );
        layer_.scene.ws()->detach( s->group(View::LAYER) );
        appearance_.scene.ws()->detach( s->group(View::TEXTURE) );
        transition_.scene.ws()->detach( s->group(View::TRANSITION) );
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

        mixing_.scene.bg()->detach( s->group(View::MIXING) );
        attach(s);
        session_->addSource(s);
    }
}


void Mixer::deselect(Source *s)
{
    if ( s != nullptr ) {
        if ( s == *current_source_)
            unsetCurrentSource();
        Mixer::selection().remove(s);
    }
}

void Mixer::deleteSelection()
{
    // number of sources in selection
    int N = selection().size();
    // ignore if selection empty
    if (N > 0) {

        // adapt Action::manager undo info depending on case
        std::ostringstream info;
        if (N > 1)
            info << N << " sources deleted";
        else
            info << selection().front()->name() << ": deleted";

        // get clones first : this way we store the history of deletion in the right order
        SourceList selection_clones_;
        for ( auto sit = selection().begin(); sit != selection().end(); sit++ ) {
            CloneSource *clone = dynamic_cast<CloneSource *>(*sit);
            if (clone)
                selection_clones_.push_back(clone);
        }
        // delete all clones
        while ( !selection_clones_.empty() ) {
            deleteSource( selection_clones_.front());// this also removes element from selection()
            selection_clones_.pop_front();
        }
        // empty the selection
        while ( !selection().empty() )
            deleteSource( selection().front() ); // this also removes element from selection()

        Action::manager().store(info.str());
    }
}

void Mixer::groupSelection()
{
    if (selection().empty())
        return;

    SessionGroupSource *sessiongroup = new SessionGroupSource;
    sessiongroup->setResolution( session_->frame()->resolution() );

    // prepare for new session group attributes
    std::string name;
    float d = selection().front()->depth();

    // empty the selection
    while ( !selection().empty() ) {

        Source *s = selection().front();
        d = MIN(s->depth(), d);

        // import source into group
        if ( sessiongroup->import(s) ) {
            name += s->initials();
            // detach & remove element from selection()
            detach (s);
            // remove source from session
            session_->removeSource(s);
        }
        else
            selection().pop_front();

    }

    // set depth at given location
    sessiongroup->group(View::LAYER)->translation_.z = d;

    // set alpha to full opacity
    sessiongroup->group(View::MIXING)->translation_.x = 0.f;
    sessiongroup->group(View::MIXING)->translation_.y = 0.f;

    // Add source to Session
    session_->addSource(sessiongroup);

    // Attach source to Mixer
    attach(sessiongroup);

    // rename and avoid name duplicates
    renameSource(sessiongroup, name);

    // store in action manager
    std::ostringstream info;
    info << sessiongroup->name() << " inserted: " << sessiongroup->session()->numSource() << " sources flatten.";
    Action::manager().store(info.str());

    // give the hand to the user
    Mixer::manager().setCurrentSource(sessiongroup);
    Log::Notify(info.str().c_str());
}

void Mixer::renameSource(Source *s, const std::string &newname)
{
    if ( s != nullptr )
    {
        // tentative new name
        std::string tentativename = s->name();

        std::list<std::string> others = session_->getNameList();
        others.remove(tentativename);

        // refuse to rename to an empty name
        if ( !newname.empty() )
            tentativename = newname;

        tentativename = BaseToolkit::uniqueName(tentativename, others);

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

        if (current_view_ == &mixing_)
            (*current_source_)->group(View::MIXING)->update_callbacks_.push_back(new BounceScaleCallback);
        else if (current_view_ == &layer_)
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

SourceList Mixer::findSources (float depth_from, float depth_to)
{
    SourceList found;
    SourceList dsl = session_->getDepthSortedList();
    SourceList::iterator  it = dsl.begin();
    for (; it != dsl.end(); ++it) {
        if ( (*it)->depth() > depth_to )
            break;
        if ( (*it)->depth() >= depth_from )
            found.push_back(*it);
    }
    return found;
}

void Mixer::setCurrentSource(uint64_t id)
{
    setCurrentSource( session_->find(id) );
}

void Mixer::setCurrentSource(Node *node)
{
    if (node!=nullptr)
        setCurrentSource( session_->find(node) );
}

void Mixer::setCurrentSource(std::string namesource)
{
    setCurrentSource( session_->find(namesource) );
}

void Mixer::setCurrentSource(Source *s)
{
    if (s!=nullptr)
        setCurrentSource( session_->find(s) );
}

void Mixer::setCurrentIndex(int index)
{
    setCurrentSource( session_->at(index) );
}

void Mixer::moveIndex (int current_index, int target_index)
{
    // remember ptr to current source
    Source *previous_current_source_ = currentSource();

    // change order
    session_->move(current_index, target_index);

    // restore current
    unsetCurrentSource();
    setCurrentSource(previous_current_source_);
}

void Mixer::setCurrentNext()
{
    if (session_->numSource() > 0) {

        SourceList::iterator it = current_source_;
        ++it;

        if (it == session_->end())  {
            it = session_->begin();
        }

        setCurrentSource( it );
    }
}

void Mixer::setCurrentPrevious()
{
    if (session_->numSource() > 0) {

        SourceList::iterator it = current_source_;

        if (it == session_->begin())  {
            it = session_->end();
        }

        --it;
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
    if ( current_source_ != session_->end() )
        return (*current_source_);
    else
        return nullptr;
}

// management of view
void Mixer::setView(View::Mode m)
{
    // special case when leaving transition view
    if ( current_view_ == &transition_ ) {
        // get the session detached from the transition view and set it as current session
        // NB: detatch() can return nullptr, which is then ignored.
        Session *se = transition_.detach();
        if ( se != nullptr )
            set ( se );
        else
            Log::Info("Transition interrupted: Session source added.");
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
    case View::TEXTURE:
        current_view_ = &appearance_;
        break;
    case View::MIXING:
    default:
        current_view_ = &mixing_;
        break;
    }

    // setttings
    Settings::application.current_view = (int) m;

    // selection might have to change
    for (auto sit = session_->begin(); sit != session_->end(); sit++) {
        if ( !current_view_->canSelect(*sit) )
            deselect( *sit );
    }

    // need to deeply update view to apply eventual changes
    ++View::need_deep_update_;
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
    case View::TEXTURE:
        return &appearance_;
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
    session_->config(View::TEXTURE)->copyTransform( appearance_.scene.root() );

    // launch a thread to save the session
    std::thread (saveSession, filename, session_).detach();
}

void Mixer::load(const std::string& filename)
{
    if (filename.empty())
        return;

#ifdef THREADED_LOADING
    // load only one at a time
    if (sessionLoaders_.empty()) {
        // Start async thread for loading the session
        // Will be obtained in the future in update()
        sessionLoaders_.emplace_back( std::async(std::launch::async, Session::load, filename, 0) );
    }
#else
    set( Session::load(filename) );
#endif
}

void Mixer::open(const std::string& filename, bool smooth)
{
    if (smooth)
    {
        Log::Info("\nStarting transition to session %s", filename.c_str());

        // create special SessionSource to be used for the smooth transition
        SessionFileSource *ts = new SessionFileSource;
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
        sessionImporters_.emplace_back( std::async(std::launch::async, Session::load, filename, 0) );
    }
#else
    merge( Session::load(filename) );
#endif
}

void Mixer::import(SessionSource *source)
{
    sessionSourceToImport_.push_back( source );
}


void Mixer::merge(Session *session)
{
    if ( session == nullptr ) {
        Log::Warning("Failed to import Session.");
        return;
    }

    // new state in history manager
    std::ostringstream info;
    info << session->numSource() << " sources imported from " << session->filename();
    Action::manager().store(info.str());

    // import every sources
    for ( Source *s = session->popSource(); s != nullptr; s = session->popSource()) {
        // avoid name duplicates
        renameSource(s);

        // Add source to Session
        session_->addSource(s);

        // Attach source to Mixer
        attach(s);
    }

    // import and attach session's mixing groups
    auto group_iter = session->beginMixingGroup();
    while ( group_iter != session->endMixingGroup() ){
        session_->link((*group_iter)->getCopy(), mixing_.scene.fg());
        group_iter = session->deleteMixingGroup(group_iter);
    }

    // needs to update !
    ++View::need_deep_update_;

    // avoid display issues
    current_view_->update(0.f);
}

void Mixer::merge(SessionSource *source)
{
    if ( source == nullptr ) {
        Log::Warning("Failed to import Session Source.");
        return;
    }

    // detach session from SessionSource (source will fail and be deleted later)
    Session *session = source->detach();

    // prepare Action manager info
    std::ostringstream info;
    info << source->name().c_str() << " source deleted, " << session->numSource() << " sources imported";

    // import sources of the session (if not empty)
    if ( !session->empty() ) {

        // where to put the sources imported in depth?
        float target_depth = source->depth();

        // get how much space we need from there
        SourceList dsl = session->getDepthSortedList();
        float  start_depth = dsl.front()->depth();
        float  end_depth = dsl.back()->depth();
        float  need_depth = MAX( end_depth - start_depth, LAYER_STEP);

        // make room if there is not enough space
        SourceList to_be_moved = findSources(target_depth, MAX_DEPTH);
        if (!to_be_moved.empty()){
            float next_depth = to_be_moved.front()->depth();
            if ( next_depth < target_depth + need_depth) {
                SourceList::iterator  it = to_be_moved.begin();
                for (; it != to_be_moved.end(); ++it) {
                    float scale_depth = (MAX_DEPTH-(*it)->depth()) / (MAX_DEPTH-next_depth);
                    (*it)->setDepth( (*it)->depth() + scale_depth );
                }
            }
        }

        // import every sources
        for ( Source *s = session->popSource(); s != nullptr; s = session->popSource()) {

            // avoid name duplicates
            renameSource(s);

            // scale alpha
            s->setAlpha( s->alpha() * source->alpha() );

            // set depth (proportional to depth of s, adjusted by needed space)
            s->setDepth( target_depth + ( (s->depth()-start_depth)/ need_depth) );

            // set location
            // a. transform of node to import
            Group *sNode = s->group(View::GEOMETRY);
            glm::mat4 sTransform  = GlmToolkit::transform(sNode->translation_, sNode->rotation_, sNode->scale_);
            // b. transform of session source
            Group *sourceNode = source->group(View::GEOMETRY);
            glm::mat4 sourceTransform  = GlmToolkit::transform(sourceNode->translation_, sourceNode->rotation_, sourceNode->scale_);
            // c. combined transform of source and session source
            sourceTransform *= sTransform;
            GlmToolkit::inverse_transform(sourceTransform, sNode->translation_, sNode->rotation_, sNode->scale_);

            // Add source to Session
            session_->addSource(s);

            // Attach source to Mixer
            attach(s);
        }

        // import and attach session's mixing groups
        auto group_iter = session->beginMixingGroup();
        while ( group_iter != session->endMixingGroup() ){
            session_->link((*group_iter)->getCopy(), mixing_.scene.fg());
            group_iter = session->deleteMixingGroup(group_iter);
        }

        // needs to update !
        ++View::need_deep_update_;

    }

    // imported source itself should be removed
    detach(source);
    session_->deleteSource(source);

    // new state in history manager
    Action::manager().store(info.str());

    // avoid display issues
    current_view_->update(0.f);

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

    // attach new session's nodes to views
    for (auto source_iter = session_->begin(); source_iter != session_->end(); source_iter++)
        attach(*source_iter);

    // optional copy of views config
    mixing_.scene.root()->copyTransform( session_->config(View::MIXING) );
    geometry_.scene.root()->copyTransform( session_->config(View::GEOMETRY) );
    layer_.scene.root()->copyTransform( session_->config(View::LAYER) );
    appearance_.scene.root()->copyTransform( session_->config(View::TEXTURE) );

    // attach new session's mixing group to mixingview
    for (auto group_iter = session_->beginMixingGroup(); group_iter != session_->endMixingGroup(); group_iter++)
        (*group_iter)->attachTo( mixing_.scene.fg() );

    // set resolution
    session_->setResolution( session_->config(View::RENDERING)->scale_ );

    // transfer fading
    session_->setFading( MAX(back_session_->fading(), session_->fading()), true );

    // no current source
    current_source_ = session_->end();
    current_source_index_ = -1;

    // delete back (former front session)
    garbage_.push_back(back_session_);
    back_session_ = nullptr;

    // reset History manager
    Action::manager().init();

    // notification
    Log::Notify("Session %s loaded. %d source(s) created.", session_->filename().c_str(), session_->numSource());
}

void Mixer::close(bool smooth)
{
    if (smooth)
    {
        // create empty SessionSource to be used for the smooth transition
        SessionFileSource *ts = new SessionFileSource;

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

    // need to deeply update view to apply eventual changes
    ++View::need_deep_update_;

    Log::Info("New session ready.");
}

void Mixer::set(Session *s)
{
    if ( s == nullptr )
        return;

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
    tinyxml2::XMLDocument xmlDoc;
    tinyxml2::XMLElement* sourceNode = SessionLoader::firstSourceElement(clipboard, xmlDoc);
    if (sourceNode) {

        SessionLoader loader( session_ );

        for( ; sourceNode ; sourceNode = sourceNode->NextSiblingElement())
        {
            Source *s = loader.createSource(sourceNode);
            if (s) {
                // Add source to Session
                session_->addSource(s);
                // Add source to Mixer
                addSource(s);
            }
        }
    }
}


void Mixer::restore(tinyxml2::XMLElement *sessionNode)
{
    //
    // source lists
    //

    // sessionsources contains list of ids of all sources currently in the session (before loading)
    SourceIdList session_sources = session_->getIdList();
//    for( auto it = sessionsources.begin(); it != sessionsources.end(); it++)
//        Log::Info("sessionsources  id %s", std::to_string(*it).c_str());

    // load history status:
    // - if a source exists, its attributes are updated, and that's all
    // - if a source does not exists (in current session), it is created inside the session
    SessionLoader loader( session_ );
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
            detach( s );
            // delete source from session
            session_->deleteSource( s );
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
        attach( (*lsit).second );
    }

    //
    // mixing groups
    //

    // Get the list of mixing groups in the xml loader
    std::list< SourceList > loadergroups = loader.getMixingGroups();

    // clear all session groups
    auto group_iter = session_->beginMixingGroup();
    while ( group_iter != session_->endMixingGroup() )
        group_iter = session_->deleteMixingGroup(group_iter);

    // apply all changes creating or modifying groups in the session
    // (after this, new groups are created and existing groups are adjusted)
    for (auto group_loader_it = loadergroups.begin(); group_loader_it != loadergroups.end(); group_loader_it++)
        session_->link( *group_loader_it, view(View::MIXING)->scene.fg() );


    ++View::need_deep_update_;
}

