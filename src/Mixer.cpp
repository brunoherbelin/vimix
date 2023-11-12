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

#include "ImageShader.h"
#include "TextSource.h"
#include "defines.h"
#include "Settings.h"
#include "Log.h"
#include "View.h"
#include "BaseToolkit.h"
#include "SystemToolkit.h"
#include "SessionCreator.h"
#include "SessionVisitor.h"
#include "SessionSource.h"
#include "CloneSource.h"
#include "RenderSource.h"
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "PatternSource.h"
#include "DeviceSource.h"
#include "ScreenCaptureSource.h"
#include "MultiFileSource.h"
#include "StreamSource.h"
#include "NetworkSource.h"
#include "SrtReceiverSource.h"
#include "SourceCallback.h"

#include "ActionManager.h"
#include "MixingGroup.h"
#include "FrameGrabber.h"

#include "Mixer.h"

#define THREADED_LOADING
std::vector< std::future<std::string> > sessionSavers_;
std::vector< std::future<Session *> > sessionLoaders_;
std::vector< std::future<Session *> > sessionImporters_;
std::vector< SessionSource * > sessionSourceToImport_;
const std::chrono::milliseconds timeout_ = std::chrono::milliseconds(4);


Mixer::Mixer() : session_(nullptr), back_session_(nullptr), sessionSwapRequested_(false),
    current_view_(nullptr), busy_(false), dt_(16.f), dt__(16.f)
{
    // unsused initial empty session
    session_ = new Session;
    current_source_ = session_->end();
    current_source_index_ = -1;

    // auto load if Settings ask to
    if ( Settings::application.recentSessions.load_at_start &&
         Settings::application.recentSessions.front_is_valid &&
         Settings::application.recentSessions.filenames.size() > 0 &&
         Settings::application.fresh_start) {
        load( Settings::application.recentSessions.filenames.front() );
        // initialize with the current view
        setView( (View::Mode) Settings::application.current_view );
    }
    else {
        // initialize with a new empty session
        clear();
        setView( View::MIXING );
    }
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
            if (sessionImporters_.back().valid()) {
                // get the session loaded by this loader
                merge( sessionImporters_.back().get() );
                // FIXME: shouldn't we delete the imported session?
            }
            // done with this session loader
            sessionImporters_.pop_back();
        }
    }

    // if there is a session loader pending
    if (!sessionLoaders_.empty()) {
        // check status of loader: did it finish ?
        if (sessionLoaders_.back().wait_for(timeout_) == std::future_status::ready ) {
            // get the session loaded by this loader
            if (sessionLoaders_.back().valid()) {
                // get the session loaded by this loader
                set( sessionLoaders_.back().get() );
            }
            // done with this session loader
            sessionLoaders_.pop_back();
            busy_ = false;
        }
    }
#endif

    // if there is a session saving pending
    if (!sessionSavers_.empty()) {
        // check status of saver: did it finish ?
        if (sessionSavers_.back().wait_for(timeout_) == std::future_status::ready ) {
            std::string filename;
            // did we get a filename in return?
            if (sessionSavers_.back().valid()) {
                filename = sessionSavers_.back().get();
            }
            // done with this session saver
            sessionSavers_.pop_back();
            if (filename.empty())
                Log::Warning("Failed to save Session.");
            // all ok
            else {
                // set session filename
                session_->setFilename(filename);
                // cosmetics saved ok
                Rendering::manager().setMainWindowTitle(SystemToolkit::filename(filename));
                Settings::application.recentSessions.push(filename);
                Log::Notify("Session '%s' saved.", filename.c_str());
            }
            busy_ = false;
        }
    }

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
            // inform new session filename
            if (session_->filename().empty()) {
                Rendering::manager().setMainWindowTitle(Settings::application.windows[0].name);
            } else {
                Rendering::manager().setMainWindowTitle(SystemToolkit::filename(session_->filename()));
                Settings::application.recentSessions.push(session_->filename());
            }
        }
    }

    // if there is a source candidate for this session
    if (candidate_sources_.size() > 0) {
        // the first element of the pair is the source to insert
        // NB: only make the last candidate the current source in Mixing view
        insertSource(candidate_sources_.front().first, candidate_sources_.size() > 1 ? View::INVALID : View::MIXING);

        // the second element of the pair is the source to be replaced, i.e. deleted if provided
        if (candidate_sources_.front().second != nullptr)
            deleteSource(candidate_sources_.front().second);

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
    FrameGrabbing::manager().grabFrame(session_->frame());

    // manage sources which failed update
    if (session_->ready()) {
        // go through all failed sources
        SourceListUnique _failedsources = session_->failedSources();
        for(auto it = _failedsources.begin(); it != _failedsources.end(); ++it)  {

            // intervention depends on the severity of the failure
            Source::Failure fail = (*it)->failed();

            // only deal with sources that are still attached to mixer
            if ( attached( *it ) ) {

                // RETRY: Attempt to repair failed sources
                // (can be automatically repaired without user intervention)
                if (fail == Source::FAIL_RETRY) {
                    if ( !recreateSource( *it ) ) {
                        Log::Warning("Source '%s' failed and could not be fixed.", (*it)->name().c_str());
                        // delete failed source if could not recreate it
                        deleteSource( *it );
                    }
                }
                // Detatch CRITICAL failed sources from the mixer
                // (not deleted in the session; user can replace it)
                else if (fail == Source::FAIL_CRITICAL) {
                    detachSource( *it );
                }
                // Delete FATAL failed sources from the mixer
                // (nothing can be done by the user)
                else {
                    Log::Warning("Source '%s' failed and was deleted.", (*it)->name().c_str());
                    deleteSource( *it );
                }
                // needs refresh after intervention
                ++View::need_deep_update_;
            }
            // Source failed, was detached (after FAIL_CRITICAL) and got back to state FAIL_NONE
            else if ( fail < Source::FAIL_CRITICAL ) {
                // This looks like we should try to recreate it
                if ( !recreateSource( *it ) ) {
                    Log::Warning("Source '%s' definitely failed and could not be fixed.", (*it)->name().c_str());
                    // delete failed source if could not recreate it
                    deleteSource( *it );
                }
            }
        }
    }

    // update views
    mixing_.update(dt_);
    geometry_.update(dt_);
    layer_.update(dt_);
    appearance_.update(dt_);
    transition_.update(dt_);
    displays_.update(dt_);

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
Source * Mixer::createSourceFile(const std::string &path, bool disable_hw_decoding)
{
    // ready to create a source
    Source *s = nullptr;

    // sanity check
    if ( SystemToolkit::file_exists( path ) ) {

        // test type of file by extension
        if ( SystemToolkit::has_extension(path, VIMIX_FILE_EXT ) )
        {
            // create a session source
            SessionFileSource *ss = new SessionFileSource;
            ss->load(path);
            s = ss;
        }
        else {
            // (try to) create media source by default
            MediaSource *ms = new MediaSource;
            ms->mediaplayer()->setSoftwareDecodingForced(disable_hw_decoding);
            ms->setPath(path);
            s = ms;
        }
        // propose a new name based on uri
        s->setName(SystemToolkit::base_filename(path));

        // remember as recent import
        Settings::application.recentImport.push(path);
    }
    else {
        Settings::application.recentImport.remove(path);
        Log::Notify("File '%s' does not exist.", path.c_str());
    }

    return s;
}

Source * Mixer::createSourceMultifile(const std::list<std::string> &list_files, uint fps)
{
    // ready to create a source
    Source *s = nullptr;

    if ( list_files.size() >0 ) {

        // validate the creation of a sequence from the list
        MultiFileSequence sequence(list_files);

        if ( sequence.valid() ) {

            // try to create a sequence
            MultiFileSource *mfs = new MultiFileSource;
            mfs->setSequence(sequence, fps);
            s = mfs;

            // propose a new name
            s->setName( SystemToolkit::base_filename( BaseToolkit::common_prefix(list_files) ) );
        }
        else {
            Log::Notify("Could not find a sequence of consecutively numbered files.");
        }
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
    std::string name = Pattern::get(pattern).label;
    name = name.substr(0, name.find(" "));
    s->setName(name);

    return s;
}

Source * Mixer::createSourceDevice(const std::string &namedevice)
{
    // ready to create a source
    DeviceSource *s = new DeviceSource;
    s->setDevice(namedevice);

    // propose a new name based on pattern name
    s->setName( namedevice.substr(0, namedevice.find(" ")) );

    return s;
}

Source * Mixer::createSourceScreen(const std::string &namewindow)
{
    // ready to create a source
    ScreenCaptureSource *s = new ScreenCaptureSource;
    s->setWindow(namewindow);

    // propose a new name based on pattern name
    s->setName( namewindow.substr(0, namewindow.find(" ")) );

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

Source * Mixer::createSourceSrt(const std::string &ip, const std::string &port)
{
    // ready to create a source
    SrtReceiverSource *s = new SrtReceiverSource;
    s->setConnection(ip, port);

    // propose a new name based on address
    s->setName(s->uri());

    return s;
}

Source *Mixer::createSourceText(const std::string &contents, glm::ivec2 res)
{
    // ready to create a source
    TextSource *s = new TextSource;
    s->setContents(contents, res);

    // propose a new name based on contents
    std::string basestring = BaseToolkit::transliterate(contents);
    basestring = BaseToolkit::splitted(basestring, '\n').front();
    if (SystemToolkit::file_exists(basestring))
        basestring = SystemToolkit::base_filename(basestring);
    s->setName( basestring );

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

Source * Mixer::createSourceClone(const std::string &namesource, bool copy_attributes)
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

        // if clone operation asks to copy attributes
        if (copy_attributes) {
            // place clone next to origin
            s->group(View::MIXING)->translation_ = (*origin)->group(View::MIXING)->translation_;
            s->group(View::LAYER)->translation_  = (*origin)->group(View::LAYER)->translation_ + LAYER_STEP;
            // copy geometry (overlap)
            s->group(View::GEOMETRY)->translation_ = (*origin)->group(View::GEOMETRY)->translation_;
            s->group(View::GEOMETRY)->scale_ = (*origin)->group(View::GEOMETRY)->scale_;
            s->group(View::GEOMETRY)->rotation_ = (*origin)->group(View::GEOMETRY)->rotation_;
        }
    }

    return s;
}

void Mixer::addSource(Source *s)
{
    if (s != nullptr)
        candidate_sources_.push_back( std::make_pair(s, nullptr) );
}

void delayed_notification( Source *source )
{
    bool done = false;

    while (!done) {
        // give it a bit of time and avoid CPU overload
        std::this_thread::sleep_for(std::chrono::milliseconds(30));

        // to be safe, make sure we have a valid source
        Session *session = Mixer::manager().session();
        SourceList::iterator sit = session->find(source);
        if ( sit != session->end() ) {
            // voila! the source is valid : notify user if it's ready
            if ( (*sit)->ready() ) {
                Log::Notify("%s source %s is ready.", (*sit)->info().c_str(), (*sit)->name().c_str());
                done = true;
            }
            // do not notify if failed
            else if ( (*sit)->failed() )
                done = true;
        }
        // end loop if invalid source
        else
            done = true;
    }
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
        attachSource(s);

        // new state in history manager
        Action::manager().store(s->name() + std::string(": source inserted"));

        // Log & notification
        Log::Info("Adding source '%s'...", s->name().c_str());
        std::thread(delayed_notification, s).detach();

        // if requested to show the source in a given view
        // (known to work for View::MIXING et TRANSITION: other views untested)
        if (m != View::INVALID) {

            // switch to this view to show source created
            setView(m);
            current_view_->update(dt_);
            current_view_->centerSource(s);

            // set this new source as current
            setCurrentSource( sit );
        }
    }
}


void Mixer::replaceSource(Source *previous, Source *s)
{
    if ( previous == nullptr || s == nullptr)
        return ;

    // remember name
    std::string previous_name = previous->name();

    // remove source Nodes of previous from all views
    detachSource(previous);

    // copy all transforms
    s->group(View::MIXING)->copyTransform( previous->group(View::MIXING) );
    s->group(View::GEOMETRY)->copyTransform( previous->group(View::GEOMETRY) );
    s->group(View::LAYER)->copyTransform( previous->group(View::LAYER) );
    s->group(View::TEXTURE)->copyTransform( previous->group(View::TEXTURE) );

    // apply image processing
    SessionLoader loader( session_ );
    loader.applyImageProcessing(*s, SessionVisitor::getClipboard(previous));
    s->setImageProcessingEnabled( previous->imageProcessingEnabled() );
    s->blendingShader()->blending = previous->blendingShader()->blending;

    // rename s
    renameSource(s, previous_name);

    // add source 's' and remove source 'previous'
    candidate_sources_.push_back( std::make_pair(s, previous) );
}


bool Mixer::recreateSource(Source *s)
{
    SessionLoader loader( session_ );
    Source *replacement = loader.recreateSource(s);

    if (replacement == nullptr)
        return false;

    // remove source Nodes from all views
    detachSource(s);

    // delete source
    session_->deleteSource(s);

    // add source
    session_->addSource(replacement);

    // add sources Nodes to all views
    attachSource(replacement);

    return true;
}

void Mixer::deleteSource(Source *s)
{
    if ( s != nullptr )
    {
        // keep name for log
        std::string name = s->name();

        // remove source Nodes from all views
        detachSource(s);

        // delete source
        session_->deleteSource(s);

        // log
        Log::Notify("Source '%s' deleted.", name.c_str());
    }

    // cancel transition source in TRANSITION view
    if ( current_view_ == &transition_ ) {
        // cancel attachment
        transition_.attach(nullptr);
        // revert to mixing view
        setView(View::MIXING);
    }
}


void Mixer::attachSource(Source *s)
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
        // attach to session
        session_->attachSource(s);
    }
}

void Mixer::detachSource(Source *s)
{
    if ( s != nullptr )
    {
        // in case it was the current source...
        if ( s == currentSource() )
            unsetCurrentSource();
        // in case it was selected..
        selection().remove(s);
        // detach from views
        mixing_.scene.ws()->detach( s->group(View::MIXING) );
        geometry_.scene.ws()->detach( s->group(View::GEOMETRY) );
        layer_.scene.ws()->detach( s->group(View::LAYER) );
        appearance_.scene.ws()->detach( s->group(View::TEXTURE) );
        transition_.scene.ws()->detach( s->group(View::TRANSITION) );
        // dettach from session
        session_->detachSource(s);
    }
}

bool Mixer::attached (Source *s) const
{
    return ( s != nullptr && s->group(View::MIXING)->refcount_ > 0 );
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

        // detach from scene workspace
        detachSource(s);
    }
}

void Mixer::uncover(Source *s)
{
    SourceList::iterator it = std::find(stash_.begin(), stash_.end(), s);
    if ( it != stash_.end() ) {
        stash_.erase(it);

        attachSource(s);
        session_->addSource(s);
    }
}

void Mixer::deleteSelection()
{
    // number of sources in selection
    uint N = selection().size();
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
    // obvious cancel
    if (selection().empty())
        return;

    // work on non-empty selection of sources
    auto _selection = selection().getCopy();

    // create session group where to transfer sources into
    SessionGroupSource *sessiongroup = new SessionGroupSource;
    sessiongroup->setResolution( session_->frame()->resolution() );

    // prepare for new session group name
    std::string name;
    // prepare for depth to place the group source
    float d = _selection.front()->depth();

    // remember groups before emptying the session
    std::list<SourceList> allgroups = session_->getMixingGroups();
    std::list<SourceList> selectgroups;
    for (auto git = allgroups.begin(); git != allgroups.end(); ++git){
        selectgroups.push_back( intersect( *git, _selection));
    }

    // browse the selection
    for (auto sit = _selection.begin(); sit != _selection.end(); ++sit) {

        // import source into group
        if ( sessiongroup->import(*sit) ) {
            // find lower depth in _selection
            d = MIN( (*sit)->depth(), d);
            // generate name from intials of all sources
            name += (*sit)->initials();
            // detach & remove element from selection()
            detachSource (*sit);
            // remove source from session
            session_->removeSource(*sit);
        }
    }

    if (sessiongroup->session()->size() > 0) {
        // recreate groups in session group
        for (auto git = selectgroups.begin(); git != selectgroups.end(); ++git)
            sessiongroup->session()->link( *git );

        // set depth at given location
        sessiongroup->group(View::LAYER)->translation_.z = d;

        // set alpha to full opacity
        sessiongroup->group(View::MIXING)->translation_.x = 0.f;
        sessiongroup->group(View::MIXING)->translation_.y = 0.f;

        // Add source to Session
        session_->addSource(sessiongroup);

        // set name (avoid name duplicates)
        renameSource(sessiongroup, name);

        // Attach source to Mixer
        attachSource(sessiongroup);

        // needs to update !
        ++View::need_deep_update_;

        // avoid display issues
        current_view_->update(0.f);

        // store in action manager
        std::ostringstream info;
        info << sessiongroup->name() << " inserted: " << sessiongroup->session()->size() << " sources groupped.";
        Action::manager().store(info.str());

        Log::Notify("Added %s source '%s' (group of %d sources)", sessiongroup->info().c_str(), sessiongroup->name().c_str(), sessiongroup->session()->size());

        // give the hand to the user
        Mixer::manager().setCurrentSource(sessiongroup);

    }
    else {
        delete sessiongroup;
        Log::Info("Failed to group selection");
    }

}

void Mixer::groupAll(bool only_active)
{
    // obvious cancel
    if (session_->empty())
        return;

    // create session group where to transfer sources into
    SessionGroupSource *sessiongroup = new SessionGroupSource;
    sessiongroup->setResolution( session_->frame()->resolution() );

    // remember groups before emptying the session
    std::list<SourceList> allgroups = session_->getMixingGroups();

    // loop over sources from the current mixer session
    for(auto it = session_->begin(); it != session_->end(); ) {

        // if request for only active, take only active source
        // and if could import the source in the new session group
        if ( ( !only_active || (*it)->active() ) && sessiongroup->import( *it ) ) {
            // detatch the source from mixer
            detachSource( *it );
            // take out source from session and go to next (does not delete the source)
            it = session_->removeSource( *it );
        }
        // otherwise just iterate (leave source in mixer)
        else
            ++it;
    }

    // successful creation of a session group
    if (sessiongroup->session()->size() > 0) {

        // if a clone was left orphan in the mixer current session, take in in the group
        for(auto it = session_->begin(); it != session_->end(); ) {
            CloneSource *cs = dynamic_cast<CloneSource*>(*it);
            if ( cs != nullptr && session_->find( cs->origin() ) == session_->end() && sessiongroup->import( *it ) ) {
                // detatch the source from mixer
                detachSource( *it );
                // take out source from session and go to next (does not delete the source)
                it = session_->removeSource( *it );
            }
            // otherwise just iterate (leave source in mixer)
            else
                ++it;
        }

        // recreate groups in session group
        for (auto git = allgroups.begin(); git != allgroups.end(); ++git)
            sessiongroup->session()->link( *git );

        // set default depth in workspace for the session-group source
        sessiongroup->group(View::LAYER)->translation_.z = LAYER_BACKGROUND + LAYER_STEP;

        // set alpha to full opacity so that rendering is identical after swap
        sessiongroup->group(View::MIXING)->translation_.x = 0.f;
        sessiongroup->group(View::MIXING)->translation_.y = 0.f;

        // name the session-group source (avoid name duplicates)
        renameSource(sessiongroup, SystemToolkit::base_filename(session_->filename()));

        // Add the session-group source in the mixer
        // NB: sessiongroup will be updated and inserted to Mixing view on next frame
        addSource(sessiongroup);

    }
    else {
        delete sessiongroup;
        Log::Info("Failed to group all sources");
    }
}

void Mixer::ungroupAll()
{
    for (auto source_iter = session_->begin(); source_iter != session_->end(); source_iter++)
    {
        SessionGroupSource *ss = dynamic_cast< SessionGroupSource * >(*source_iter);

        if ( ss != nullptr )
            import(ss);

    }
}

void Mixer::groupSession()
{
    // new session group containing current session
    SessionGroupSource *sessiongroup = new SessionGroupSource;
    sessiongroup->setSession(session_);

    // set alpha to full opacity so that rendering is identical after swap
    sessiongroup->group(View::MIXING)->translation_.x = 0.f;
    sessiongroup->group(View::MIXING)->translation_.y = 0.f;

    // set default depth in workspace
    sessiongroup->group(View::LAYER)->translation_.z = LAYER_BACKGROUND + LAYER_STEP;

    // propose a name to the session group
    sessiongroup->setName( SystemToolkit::base_filename(session_->filename()));

    // create a session containing the sessiongroup
    Session *futuresession = new Session;
    futuresession->addSource( sessiongroup );

    // set identical filename to future session
    futuresession->setFilename( session_->filename() );

    // set and swap to futuresession (will be done at next update)
    set(futuresession);

    // detatch current session_'s nodes from views
    for (auto source_iter = session_->begin(); source_iter != session_->end(); source_iter++)
        detachSource(*source_iter);

    // detatch session_'s mixing group
    for (auto group_iter = session_->beginMixingGroup(); group_iter != session_->endMixingGroup(); group_iter++)
        (*group_iter)->attachTo(nullptr);

    // prevent deletion of session_ (now embedded into session group)
    session_ = new Session;

    Log::Notify("Switched to session '%s'", sessiongroup->name().c_str());
}

void Mixer::renameSource(Source *s, const std::string &newname)
{
    if ( s != nullptr )
    {
        // tentative new name
        std::string tentativename = s->name();

        // try the given new name if valid
        if ( !newname.empty() )
            tentativename = newname;

        tentativename = BaseToolkit::uniqueName(tentativename, session_->getNameList(s->id()));

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
    if ( it != session_->end() /*&& (*it)->mode() > Source::UNINITIALIZED */) {
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

SourceList Mixer::validate (const SourceList &list)
{
    SourceList sl;
    for( auto sit = list.begin(); sit != list.end(); ++sit) {
        SourceList::iterator it = session_->find( *sit );
        if (it != session_->end())
            sl.push_back(*sit);
    }
    return sl;
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

Source *Mixer::sourceAtIndex (int index)
{
    SourceList::iterator s = session_->at(index);
    if (s!=session_->end())
        return *s;
    return nullptr;
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
    if (session_->size() > 0) {

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
    if (session_->size() > 0) {

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

int Mixer::indexCurrentSource() const
{
    return current_source_index_;
}

int  Mixer::numSource() const
{
    return (int) session_->size();
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
            Log::Info("Transition interrupted.");
    }

    switch (m) {
    case View::DISPLAYS:
        current_view_ = &displays_;
        break;
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
    for (auto sit = session_->begin(); sit != session_->end(); ++sit) {
        Source *s = *sit;
        if ( s != nullptr && !current_view_->canSelect( s ) ) {
            if ( s == *current_source_ )
                unsetCurrentSource();
            Mixer::selection().remove( s );
        }
    }

    // need to deeply update view to apply eventual changes
    ++View::need_deep_update_;
}

View *Mixer::view(View::Mode m)
{
    switch (m) {
    case View::DISPLAYS:
        return &displays_;
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

void Mixer::save(bool with_version)
{
    if (!session_->filename().empty())
        saveas(session_->filename(), with_version);
}

void Mixer::saveas(const std::string& filename, bool with_version)
{
    // optional copy of views config
    session_->config(View::MIXING)->copyTransform( mixing_.scene.root() );
    session_->config(View::GEOMETRY)->copyTransform( geometry_.scene.root() );
    session_->config(View::LAYER)->copyTransform( layer_.scene.root() );
    session_->config(View::TEXTURE)->copyTransform( appearance_.scene.root() );

    // save only one at a time
    if (sessionSavers_.empty()) {
        // will be busy for saving
        busy_ = true;
        // prepare argument for saving a version (if requested)
        std::string versionname;
        if (with_version)
            versionname = SystemToolkit::date_time_string();
        // launch a thread to save the session
        // Will be captured in the future in update()
        sessionSavers_.emplace_back( std::async(std::launch::async, Session::save, filename, session_, versionname) );
    }
}

void Mixer::load(const std::string& filename)
{
    if (filename.empty())
        return;

#ifdef THREADED_LOADING
    // load only one at a time
    if (sessionLoaders_.empty()) {
        busy_ = true;
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
        // create special SessionSource to be used for the smooth transition
        SessionFileSource *ts = new SessionFileSource;

        // open filename if specified
        if (!filename.empty())
        {
            Log::Info("\nStarting transition to session %s", filename.c_str());
            ts->load(filename);
            // propose a new name based on uri
            ts->setName(SystemToolkit::base_filename(filename));
        }

        // attach the SessionSource to the transition view
        transition_.attach(ts);

        // insert source and switch to transition view
        insertSource(ts, View::TRANSITION);

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

    // remember groups before emptying the session
    std::list<SourceList> allgroups = session->getMixingGroups();

    // import every sources
    std::ostringstream info;
    info << session->size() << " sources imported from:" << session->filename();
    for ( Source *s = session->popSource(); s != nullptr; s = session->popSource()) {
        // avoid name duplicates
        renameSource(s);

        // Add source to Session
        session_->addSource(s);

        // Attach source to Mixer
        attachSource(s);
    }

    // recreate groups in current session_
    for (auto git = allgroups.begin(); git != allgroups.end(); ++git)
        session_->link( *git, mixing_.scene.fg() );

    // needs to update !
    ++View::need_deep_update_;

    // avoid display issues
    current_view_->update(0.f);

    // new state in history manager
    Action::manager().store(info.str());

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
    info << source->name().c_str() << " expanded:" << session->size() << " sources imported";

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
                    (*it)->call( new SetDepth( (*it)->depth() + scale_depth ) );
                }
            }
        }

        // remember groups before emptying the session
        std::list<SourceList> allgroups = session->getMixingGroups();

        // import every sources
        for ( Source *s = session->popSource(); s != nullptr; s = session->popSource()) {

            // avoid name duplicates
            renameSource(s);

            // scale alpha
            s->call( new SetAlpha(s->alpha() * source->alpha()));

            // set depth (proportional to depth of s, adjusted by needed space)
            s->call( new SetDepth( target_depth + ( (s->depth()-start_depth)/ need_depth) ) );

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
            attachSource(s);
        }

        // recreate groups in current session_
        for (auto git = allgroups.begin(); git != allgroups.end(); ++git)
            session_->link( *git, mixing_.scene.fg() );

        // needs to update !
        ++View::need_deep_update_;

    }

    // imported source itself should be removed
    detachSource(source);
    session_->deleteSource(source);

    // avoid display issues
    current_view_->update(0.f);

    // new state in history manager
    Action::manager().store(info.str());
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
            detachSource(*source_iter);
        // detatch session's mixing group
        for (auto group_iter = session_->beginMixingGroup(); group_iter != session_->endMixingGroup(); group_iter++)
            (*group_iter)->attachTo(nullptr);
    }

    // swap back and front
    Session *tmp = session_;
    session_ = back_session_;
    back_session_ = tmp;

    // attach new session's nodes to views
    for (auto source_iter = session_->begin(); source_iter != session_->end(); source_iter++)
        attachSource(*source_iter);

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

    // no current source
    current_source_ = session_->end();
    current_source_index_ = -1;

    if (back_session_) {
        // transfer fading
        session_->setFadingTarget( MAX(back_session_->fadingTarget(), session_->fadingTarget()));

        // delete back (former front session)
        garbage_.push_back(back_session_);
        back_session_ = nullptr;
    }

    // reset History manager
    Action::manager().init();

    // notification
    uint N = session_->size();
    std::string numsource = ( N>0 ? std::to_string(N) : "no" ) + " source" + (N>1 ? "s" : "");
    Log::Notify("Session '%s' loaded with %s.", session_->filename().c_str(), numsource.c_str());
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

    Settings::application.recentSessions.front_is_valid = false;
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



void Mixer::setResolution(glm::vec3 res)
{
    if (session_) {
        session_->setResolution(res);
        ++View::need_deep_update_;
        std::ostringstream info;
        info << "Session resolution changed to " << res.x << "x" << res.y;
        Log::Info("%s", info.str().c_str());
    }
}

void Mixer::paste(const std::string& clipboard)
{
    tinyxml2::XMLDocument xmlDoc;
    tinyxml2::XMLElement* sourceNode = SessionLoader::firstSourceElement(clipboard, xmlDoc);
    if (sourceNode) {

        SessionLoader loader( session_ );

        for( ; sourceNode ; sourceNode = sourceNode->NextSiblingElement())
        {
            Source *s = loader.createSource(sourceNode, SessionLoader::DUPLICATE);
            if (s) {
//                // avoid name duplicates
//                renameSource(s);
//                // Add source to Session
//                session_->addSource(s);
//                // Add source to Mixer
//                attach(s);

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
            detachSource( s );
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
        attachSource( (*lsit).second );
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

