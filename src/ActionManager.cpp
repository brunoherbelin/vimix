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
#include <regex>
#include <mutex>
#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>

#include "Log.h"
#include "View/View.h"
#include "Mixer.h"
#include "Toolkit/tinyxml2Toolkit.h"
#include "Visitor/SessionVisitor.h"
#include "SessionCreator.h"
#include "Settings.h"
#include "Toolkit/BaseToolkit.h"
#include "Interpolator.h"
#include "Draft.h"
#include "Toolkit/SystemToolkit.h"

#include "ActionManager.h"

#ifndef NDEBUG
#define ACTION_DEBUG
#endif

#define HISTORY_NODE(i) (std::string("H") + std::to_string(i))

using namespace tinyxml2;


Action::Action(): history_step_(0), history_max_step_(0), history_min_step_(0),
    history_init_pending_(false), snapshot_id_(0), snapshot_node_(nullptr), restored_id_(0),
    animating_(false), duration_(0.f), progress_(0.f)
{
    interpolator_ = new Interpolator;
}

void Action::init(const std::string &label)
{
    // clean the history
    history_doc_.Clear();
    history_step_ = 0;
    history_min_step_ = 1;
    history_max_step_ = 1;

    // reset snapshot
    snapshot_id_ = 0;
    snapshot_node_ = nullptr;
    restored_id_ = 0;

    store(label, false);

    // sources are not initialized yet (e.g. media not playing)
    history_init_pending_ = true;
}

// must be called in a thread running in parrallel of the rendering
// (needs opengl update to get thumbnail)
void captureMixerSession(Session *se, std::string node,
    std::string label,
    bool thumbnail,
    tinyxml2::XMLDocument *doc = nullptr)
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

        // generate short label, if possible
        std::smatch match;
        const std::regex pattern("([^:]*):(.*)");
        // extract left and right part around colon in the label
        if (std::regex_search(label, match, pattern) && match.size() > 1 && match[1].str().size() > 1) {
            // take left part of ':'
            std::string left = match[1].str();
            // keep only the initials, i.e. front and back characters, to upper case
            std::string short_label;
            short_label.append( 1, static_cast<char>(std::toupper(static_cast<unsigned char>(left.front()))));
            short_label.append( 1, static_cast<char>(std::toupper(static_cast<unsigned char>(left.back()))));

            // separator
            short_label.append(":");

            // take the right part of ':'
            if (match.size() > 2)
                short_label.append(match[2].str());

            sessionNode->SetAttribute("shortlabel", short_label.c_str() );
        }

        if (thumbnail) {
            // get the thumbnail (requires one opengl update to render)
            FrameBufferImage *thumbnail = se->renderThumbnail();
            if (thumbnail) {
                XMLElement *imageelement = SessionVisitor::ImageToXML(thumbnail, _doc);
                if (imageelement)
                    sessionNode->InsertEndChild(imageelement);
                delete thumbnail;
            }
        }

        // save session attributes
        sessionNode->SetAttribute("activationThreshold", se->activationThreshold());

        // save all sources using source visitor
        SessionVisitor sv(_doc, sessionNode);
        for (auto iter = se->begin(); iter != se->end(); ++iter, sv.setRoot(sessionNode) )
            (*iter)->accept(sv);

        // save input callbacks
        XMLElement *inputsNode = SessionVisitor::saveInputCallbacks(_doc, se);
        if (inputsNode)
            sessionNode->InsertEndChild(inputsNode);

        if (doc == nullptr)
            se->snapshots()->access_.unlock();
    }
}

void Action::storeSession(Session *se, std::string label, bool thumbnail)
{
    if (se == nullptr)
        return;

    // lock for creation of first step
    Action::manager().history_access_.lock();

    // incremental naming of history nodes
    Action::manager().history_step_++;

    // erase future
    for (uint e = Action::manager().history_step_; e <= Action::manager().history_max_step_; e++) {
        XMLElement *node = Action::manager().history_doc_.FirstChildElement( HISTORY_NODE(e).c_str() );
        if ( node )
            Action::manager().history_doc_.DeleteChild(node);
    }
    Action::manager().history_max_step_ = Action::manager().history_step_;

    // Ensure a maximum amount of stored steps (even very big, just to ensure memory limit)
    if (Action::manager().history_max_step_ - Action::manager().history_min_step_  > MAX_COUNT_HISTORY) {
        XMLElement *node = Action::manager().history_doc_.FirstChildElement( HISTORY_NODE(Action::manager().history_min_step_+1).c_str() );
        if ( node )
            Action::manager().history_doc_.DeleteChild(node);
        Action::manager().history_min_step_++;
    }

    // capture current session
    captureMixerSession(se,
                        HISTORY_NODE(Action::manager().history_step_),
                        label,
                        thumbnail,
                        &Action::manager().history_doc_
                        );

    Action::manager().history_access_.unlock();

#ifdef ACTION_DEBUG
    Log::Info("Action stored %d '%s'", Action::manager().history_step_, label.c_str());
     //  XMLSaveDoc(&Action::manager().history_doc_, "/home/bh/history.xml");
#endif
}

void Action::refreshInit()
{
    if (!history_init_pending_)
        return;
    history_init_pending_ = false;

    // do not block if an action is being stored
    if (!history_access_.try_lock())
        return;

    // replace initial step only if no other action was stored since init
    if (history_step_ == 1 && history_max_step_ == 1) {
        XMLElement *node = history_doc_.FirstChildElement( HISTORY_NODE(1).c_str() );
        if (node) {
            std::string label = node->Attribute("label") ? node->Attribute("label") : "";
            history_doc_.DeleteChild(node);
            captureMixerSession(Mixer::manager().liveSession(), HISTORY_NODE(1), label, false, &history_doc_);
        }
    }

    history_access_.unlock();
}

void Action::store(const std::string &label, bool threaded)
{
    // ignore if no label is given
    if (label.empty())
        return;

    // ignore actions in DRAFT mode
    if (Draft::manager().active())
        return;

    // threaded capturing state of current session
    if (threaded)
        std::thread(Action::storeSession, Mixer::manager().liveSession(), label, true).detach();
    else
        Action::storeSession(Mixer::manager().liveSession(), label, false);
}

void Action::undo()
{
    if (Draft::manager().active()) {
        Log::Notify("Not available in Draft mode.");
        return;
    }
    // finish draft or snapshot animation
    Draft::manager().terminate();
    finish();
    // not possible to go to 1 -1 = 0
    if (history_step_ <= 1)
        return;

    // restore always changes step_ to step_ - 1
    restore( history_step_ - 1);
}

void Action::redo()
{
    if (Draft::manager().active()) {
        Log::Notify("Not available in Draft mode.");
        return;
    }
    // finish draft or snapshot animation
    Draft::manager().terminate();
    finish();
    // not possible to go to max_step_ + 1
    if (history_step_ >= history_max_step_)
        return;

    // restore always changes step_ to step_ + 1
    restore( history_step_ + 1);
}


void Action::stepTo(uint target)
{
    if (Draft::manager().active()) {
        Log::Notify("Not available in Draft mode.");
        return;
    }
    // finish draft or snapshot animation
    Draft::manager().terminate();
    finish();
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

std::string Action::shortlabel(uint s) const
{
    std::string l = "";

    if (s > 0 && s <= history_max_step_) {
        const XMLElement *sessionNode = history_doc_.FirstChildElement( HISTORY_NODE(s).c_str());
        if  (sessionNode) {
            if (sessionNode->Attribute("shortlabel"))
                l = sessionNode->Attribute("shortlabel");
            else
                l = sessionNode->Attribute("label");
        }
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
    if (Draft::manager().active()) {
        Log::Notify("Not available in Draft mode.");
        return;
    }
    // finish draft or snapshot animation
    Draft::manager().terminate();
    finish();
    history_access_.lock();

    // get history node of target step
    history_step_ = CLAMP(target, 1, history_max_step_);
    XMLElement *sessionNode = history_doc_.FirstChildElement( HISTORY_NODE(history_step_).c_str() );

    if (sessionNode) {

        // ask view to refresh, and switch to action view if user prefers
        int view = Settings::application.current_view ;
        if (Settings::application.action_history_follow_view)
            sessionNode->QueryIntAttribute("view", &view);
        // never enter a view switching between sessions
        if (View::isSwitchingView(view))
            view = Settings::application.current_view;
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
            std::thread(captureMixerSession, se, SNAPSHOT_NODE(id), label, true, nullptr).detach();
        else
            captureMixerSession(se, SNAPSHOT_NODE(id), label, false);

#ifdef ACTION_DEBUG
        Log::Info("Snapshot stored %d '%s'", id, label.c_str());
#endif
    }
}

void Action::snapshot(const std::string &label)
{
    if (Draft::manager().active()) {
        Log::Notify("Not available in Draft mode.");
        return;
    }
    // finish draft or snapshot animation
    Draft::manager().terminate();
    finish();
    // ensure label is unique
    std::string snap_label = BaseToolkit::uniqueName(label, labels());

    // take the snapshot on current session
    takeSnapshot(Mixer::manager().liveSession(), snap_label, true);

}

void Action::open(uint64_t snapshotid)
{
    if ( snapshot_id_ != snapshotid )
    {
        // get snapshot node of target in current session
        Session *se = Mixer::manager().liveSession();
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
    }
}

void Action::replace(uint64_t snapshotid)
{
    if (Draft::manager().active()) {
        Log::Notify("Not available in Draft mode.");
        return;
    }
    // finish draft or snapshot animation
    Draft::manager().terminate();
    finish();
    if (snapshotid > 0)
        open(snapshotid);

    if (snapshot_node_) {
        // remember label
        std::string label = snapshot_node_->Attribute("label");

        // remove previous node
        Session *se = Mixer::manager().liveSession();
        if (se) {

            se->snapshots()->access_.lock();
            se->snapshots()->xmlDoc_->DeleteChild( snapshot_node_ );
            se->snapshots()->access_.unlock();

            // threaded capture state of current session
            std::thread(captureMixerSession, se, SNAPSHOT_NODE(snapshot_id_), label, true, nullptr).detach();

#ifdef ACTION_DEBUG
            Log::Info("Snapshot replaced %d '%s'", snapshot_id_, label.c_str());
#endif

        }
    }
}

bool Action::identical(const XMLElement *a, const XMLElement *b)
{
    if (a == nullptr || b == nullptr)
        return a == b;

    if (std::strcmp(a->Name(), b->Name()) != 0)
        return false;

    // same attributes, in the same order
    const XMLAttribute *x = a->FirstAttribute();
    const XMLAttribute *y = b->FirstAttribute();
    for (; x && y; x = x->Next(), y = y->Next()) {
        if (std::strcmp(x->Name(), y->Name()) != 0 || std::strcmp(x->Value(), y->Value()) != 0)
            return false;
    }
    if (x || y)
        return false;

    // same children, in the same order
    const XMLNode *ca = a->FirstChild();
    const XMLNode *cb = b->FirstChild();
    for (; ca && cb; ca = ca->NextSibling(), cb = cb->NextSibling()) {
        const XMLElement *ea = ca->ToElement();
        const XMLElement *eb = cb->ToElement();
        if (ea && eb) {
            if (!identical(ea, eb))
                return false;
        }
        // text, comments
        else if (!ca->ShallowEqual(cb))
            return false;
    }

    return ca == nullptr && cb == nullptr;
}

// Remove the information that does not influence rendering:
// - the id of the elements inside a source (nodes, shaders, players, etc.), generated
//   each time the source is created
// - the name and play state of a source
static void removeNonRendering(XMLElement *element, bool inside_source)
{
    if (inside_source)
        element->DeleteAttribute("id");

    if (std::strcmp(element->Name(), "Source") == 0) {
        element->DeleteAttribute("name");
        element->DeleteAttribute("play");
        inside_source = true;
    }
    for (XMLElement *child = element->FirstChildElement(); child; child = child->NextSiblingElement())
        removeNonRendering(child, inside_source);
}

// Search, in the given snapshots (from the oldest), the ones identical to a previous one.
static std::list<uint64_t> searchIdentical(std::shared_ptr<tinyxml2::XMLDocument> doc,
                                           std::vector< std::pair<uint64_t, const XMLElement *> > snapshots)
{
    std::list<uint64_t> found;
    std::vector<const XMLElement *> kept;

    for (auto s = snapshots.begin(); s != snapshots.end(); ++s) {
        bool duplicate = false;
        for (auto k = kept.begin(); k != kept.end() && !duplicate; ++k)
            duplicate = Action::identical(s->second, *k);
        if (duplicate)
            found.push_back(s->first);
        else
            kept.push_back(s->second);
    }

    return found;
}

void Action::removeIdentical()
{
    if (search_.valid() || Mixer::manager().liveSession() == nullptr)
        return;

    // Copy of the snapshots to search in without blocking or being disturbed.
    // The copy has no thumbnail and no information that does not influence rendering
    // (label, date, view, ids of nodes, shaders and players, name and play state of sources).
    std::shared_ptr<tinyxml2::XMLDocument> doc = std::make_shared<tinyxml2::XMLDocument>();
    std::vector< std::pair<uint64_t, const XMLElement *> > snapshots;

    Session *se = Mixer::manager().liveSession();
    se->snapshots()->access_.lock();
    for (auto id = se->snapshots()->keys_.begin(); id != se->snapshots()->keys_.end(); ++id) {
        // (snapshot being captured by a thread does not exist yet)
        const XMLElement *node = se->snapshots()->xmlDoc_->FirstChildElement( SNAPSHOT_NODE(*id).c_str() );
        if (node == nullptr)
            continue;
        XMLElement *copy = doc->NewElement("Snapshot");
        for (const XMLAttribute *a = node->FirstAttribute(); a; a = a->Next()) {
            const std::string name(a->Name());
            if (name != "label" && name != "shortlabel" && name != "date" && name != "view")
                copy->SetAttribute(a->Name(), a->Value());
        }
        for (const XMLNode *c = node->FirstChild(); c; c = c->NextSibling()) {
            const XMLElement *e = c->ToElement();
            if (e == nullptr || std::strcmp(e->Name(), "Image") != 0)
                copy->InsertEndChild( c->DeepClone(doc.get()) );
        }
        removeNonRendering(copy, false);
        doc->InsertEndChild(copy);
        snapshots.push_back( std::make_pair(*id, copy) );
    }
    se->snapshots()->access_.unlock();

    // search in a thread (snapshots removed in update)
    search_ = std::async(std::launch::async, searchIdentical, doc, std::move(snapshots));
}

std::list<uint64_t> Action::snapshots() const
{
    return Mixer::manager().liveSession()->snapshots()->keys_;
}

std::list<std::string> Action::labels() const
{
    std::list<std::string> names;

    tinyxml2::XMLDocument *doc = Mixer::manager().liveSession()->snapshots()->xmlDoc_;
    for ( XMLElement *snap = doc->FirstChildElement(); snap ; snap = snap->NextSiblingElement() )
        names.push_back( snap->Attribute("label"));

    return names;
}

std::string Action::label(uint64_t snapshotid) const
{
    std::string label = "";

    // get snapshot node of target in current session
    Session *se = Mixer::manager().liveSession();
    const XMLElement *snap = se->snapshots()->xmlDoc_->FirstChildElement( SNAPSHOT_NODE(snapshotid).c_str() );

    if (snap)
        label = snap->Attribute("label");

    return label;
}

std::chrono::system_clock::time_point Action::date(uint64_t snapshotid) const
{
    std::chrono::system_clock::time_point date;

    // get snapshot node of target in current session
    Session *se = Mixer::manager().liveSession();
    const XMLElement *snap = se->snapshots()->xmlDoc_->FirstChildElement( SNAPSHOT_NODE(snapshotid).c_str() );

    if (snap){
        const char *d = snap->Attribute("date");
        if (d)
            SystemToolkit::date_time_parse(std::string(d), date);
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
    Session *se = Mixer::manager().liveSession();
    const XMLElement *snap = se->snapshots()->xmlDoc_->FirstChildElement( SNAPSHOT_NODE(snapshotid).c_str() );

    if (snap){
        img = SessionLoader::XMLToImage(snap);
    }

    return img;
}

void Action::clearSnapshots()
{
    Session *se = Mixer::manager().liveSession();
    while (!se->snapshots()->keys_.empty())
        remove(se->snapshots()->keys_.front());
}

void Action::remove(uint64_t snapshotid)
{
    if (snapshotid > 0)
        open(snapshotid);

    if (snapshot_node_) {
        // remove
        Session *se = Mixer::manager().liveSession();
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
    if (Draft::manager().active()) {
        Log::Notify("Not available in Draft mode.");
        return;
    }
    // finish draft or snapshot animation
    Draft::manager().terminate();
    finish();
    if (snapshotid > 0)
        open(snapshotid);

    if (snapshot_node_) {
        // actually restore
        Mixer::manager().restore(snapshot_node_);
        restored_id_ = snapshot_id_;
    }

    store("Snapshot " + label(snapshot_id_));
}

// position of the source moved on the border of the mixing circle (alpha 0)
static SourceCore faded(const Source *s)
{
    SourceCore core;
    SourceCoreField::copy(core, *s, SourceCoreField::bit(SourceCoreField::MIXING_POSITION));
    glm::vec3 &pos = core.group(View::MIXING)->translation_;
    const glm::vec2 p(pos);
    const float d = glm::length(p);
    const glm::vec2 dir = d > EPSILON ? p / d : glm::vec2(1.f, 0.f);
    const glm::vec2 out = dir * MAX(d, 1.f);
    pos.x = out.x;
    pos.y = out.y;
    return core;
}

void Action::restore(uint64_t snapshotid, float duration)
{
    // no animation
    if (duration < 1.f || Mixer::manager().editingBundle()) {
        restore(snapshotid);
        return;
    }

    if (Draft::manager().active()) {
        Log::Notify("Not available in Draft mode.");
        return;
    }
    // finish draft or snapshot animation
    Draft::manager().terminate();
    finish();
    if (snapshotid > 0)
        open(snapshotid);

    if (!snapshot_node_)
        return;

    Session *se = Mixer::manager().liveSession();

    // state of sources before restore
    std::map<uint64_t, SourceCore> from;
    std::map<uint64_t, Source::Parameters> from_parameters;
    std::map<uint64_t, bool> from_processing;
    for (auto it = se->begin(); it != se->end(); ++it) {
        SourceCoreField::copy(from[(*it)->id()], **it);
        from_parameters[(*it)->id()] = (*it)->parameters();
        from_processing[(*it)->id()] = (*it)->imageProcessingEnabled();
    }

    // restore all at once (keeping the sources to remove)
    removed_.clear();
    Mixer::manager().restore(snapshot_node_, &removed_);
    restored_id_ = snapshot_id_;

    // animate sources from previous to restored state
    interpolator_->clear();
    for (auto it = se->begin(); it != se->end(); ++it) {
        Source *s = *it;
        const bool remove = std::find(removed_.begin(), removed_.end(), s) != removed_.end();
        auto f = from.find(s->id());

        // source not in snapshot: fade out
        if (remove)
            interpolator_->add(s, faded(s), SourceCoreField::bit(SourceCoreField::MIXING_POSITION));
        // source created by snapshot: fade in
        else if (f == from.end()) {
            SourceCore target;
            SourceCoreField::copy(target, *s, SourceCoreField::bit(SourceCoreField::MIXING_POSITION));
            SourceCoreField::copy(*s, faded(s), SourceCoreField::bit(SourceCoreField::MIXING_POSITION));
            s->touch();
            interpolator_->add(s, target, SourceCoreField::bit(SourceCoreField::MIXING_POSITION));
        }
        // source existing before: from previous to restored state
        else {
            SourceCore target;
            SourceCoreField::copy(target, *s);
            SourceCoreField::Mask mask = SourceCoreField::diff(target, f->second);
            Source::Parameters parameters = SourceCoreField::diff( s->parameters(), from_parameters[s->id()] );
            // image processing switched in snapshot
            SourceInterpolator::ProcessingSwitch processing = SourceInterpolator::PROCESSING_KEEP;
            if ( s->imageProcessingEnabled() != from_processing[s->id()] )
                processing = s->imageProcessingEnabled() ? SourceInterpolator::PROCESSING_ENABLE
                                                         : SourceInterpolator::PROCESSING_DISABLE;

            if ( mask != 0 || !parameters.empty() || processing != SourceInterpolator::PROCESSING_KEEP ) {
                // back to previous state
                SourceCoreField::copy(*s, f->second);
                s->setParameters( from_parameters[s->id()] );
                s->setImageProcessingEnabled( from_processing[s->id()] );
                s->touch();
                interpolator_->add(s, target, mask, parameters, processing);
            }
        }
    }

    // start the animation (or finish if nothing to animate)
    animating_ = true;
    duration_ = duration;
    progress_ = 0.f;
    if (interpolator_->empty())
        finish();
    else
        interpolator_->apply(0.f);
}

float Action::progress() const
{
    if (animating_ && duration_ > 0.f)
        return CLAMP(progress_ / duration_, 0.f, 1.f);
    return 0.f;
}

void Action::update(float dt)
{
    // end of search of identical snapshots: remove them
    if (search_.valid() && search_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        const std::list<uint64_t> found = search_.get();
        for (auto id = found.begin(); id != found.end(); ++id)
            remove(*id);
        if (found.empty())
            Log::Notify("No identical version found.");
        else
            Log::Notify("%d identical version%s removed.", static_cast<int>(found.size()), found.size() > 1 ? "s" : "");
    }

    if (!animating_)
        return;

    progress_ += dt;

    if (progress_ < duration_)
        interpolator_->apply(progress_ / duration_);
    else
        finish();
}

void Action::finish(bool store_history)
{
    if (!animating_)
        return;
    animating_ = false;

    interpolator_->apply(1.f);
    interpolator_->clear();

    // delete sources faded out (as in Mixer::restore)
    for (auto it = removed_.begin(); it != removed_.end(); ++it) {
        Mixer::manager().detachSource( *it );
        Mixer::manager().liveSession()->deleteSource( *it );
    }
    removed_.clear();
    ++View::need_deep_update_;

    if (store_history)
        store("Snapshot " + label(restored_id_));
}

void Action::forget(Source *s)
{
    interpolator_->remove(s);
    removed_.remove(s);
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
