/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2026 Bruno Herbelin <bruno.herbelin@gmail.com>
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
#include <sstream>
#include <set>

#include <glm/gtc/matrix_transform.hpp>

#include "IconsFontAwesome5.h"
#include "defines.h"
#include "Metronome.h"
#include "Settings.h"
#include "Log.h"
#include "Mixer.h"
#include "MixingGroup.h"
#include "Session.h"
#include "ActionManager.h"
#include "RenderingManager.h"
#include "FrameBuffer.h"
#include "Source/Source.h"
#include "Scene/Decorations.h"
#include "Scene/Grid.h"
#include "Visitor/PickingVisitor.h"
#include "View/GeometryHandleManipulation.h"

#include "PathAutomaton.h"
#include "FieldAutomaton.h"
#include "AutomatonRig.h"

// tolerance to detect a change made by someone else than the rig
#define POSE_EPSILON 1e-6f

// When a link of a path automaton stops acting (unlink, disable, delete), its
// effect is transfered in the rest pose so that the source stays where it is,
// and links start from where the source is when enabled again.
// Set to 0 for sources to go back at their rest pose (previous behavior).
#define AUTOMATON_BAKE_LINKS 1

//
// AutomatonPose
//

void AutomatonPose::capture (const Node *n)
{
    translation = n->translation_;
    rotation = n->rotation_;
    scale = n->scale_;
    crop = n->crop_;
    data = n->data_;
}

void AutomatonPose::apply (Node *n) const
{
    n->translation_ = translation;
    n->rotation_ = rotation;
    n->scale_ = scale;
    n->crop_ = crop;
    n->data_ = data;
}

// channels of the node of a source that are manipulated in each view
// (the others are computed by the source itself)
static void copyChannels (View::Mode m, const AutomatonPose &from, Node *to)
{
    switch (m) {
    case View::MIXING:
        to->translation_.x = from.translation.x;
        to->translation_.y = from.translation.y;
        break;
    case View::GEOMETRY:
        to->translation_.x = from.translation.x;
        to->translation_.y = from.translation.y;
        to->rotation_.z = from.rotation.z;
        to->scale_.x = from.scale.x;
        to->scale_.y = from.scale.y;
        to->crop_ = from.crop;
        to->data_ = from.data;
        break;
    case View::LAYER:
        to->translation_.z = from.translation.z;
        break;
    default:
        break;
    }
}

template <typename T>
static bool transfer (const T &node, const T &written, T &base)
{
    const T d = node - written;
    bool changed = false;
    for (int i = 0; i < T::length(); ++i)
        changed |= std::abs(d[i]) > POSE_EPSILON;
    if (changed)
        base += d;
    return changed;
}

static bool transfer (float node, float written, float &base)
{
    const float d = node - written;
    if (std::abs(d) > POSE_EPSILON) {
        base += d;
        return true;
    }
    return false;
}

// transfer to base pose the changes made on the node since the pose was written
static void transferChannels (View::Mode m, const Node *node, const AutomatonPose &written, AutomatonPose &base)
{
    switch (m) {
    case View::MIXING:
        transfer(node->translation_.x, written.translation.x, base.translation.x);
        transfer(node->translation_.y, written.translation.y, base.translation.y);
        break;
    case View::GEOMETRY:
        transfer(node->translation_.x, written.translation.x, base.translation.x);
        transfer(node->translation_.y, written.translation.y, base.translation.y);
        transfer(node->rotation_.z, written.rotation.z, base.rotation.z);
        transfer(node->scale_.x, written.scale.x, base.scale.x);
        transfer(node->scale_.y, written.scale.y, base.scale.y);
        transfer(node->crop_, written.crop, base.crop);
        for (int c = 0; c < 4; ++c)
            transfer(node->data_[c], written.data[c], base.data[c]);
        break;
    case View::LAYER:
        transfer(node->translation_.z, written.translation.z, base.translation.z);
        break;
    default:
        break;
    }
}

//
// Manipulation of handles of sources, as done by the GeometryView
//
// The functions of GeometryHandleManipulation show and hide overlays and other handles:
// the rig gives them dummy overlays and handles (never drawn)
//
struct HandleDummies {
    Handles *handles[15];
    Group *node;
    Symbol *symbol;
    Grid *grid;
    Group *grid_parent;
    HandleDummies () {
        for (int i = 0; i < 15; ++i)
            handles[i] = new Handles( (Handles::Type) i );
        node = new Group;
        symbol = new Symbol(Symbol::EMPTY);
        grid_parent = new Group;
        grid = new Grid(grid_parent);
        grid->setActive(false);
    }
};

static HandleDummies &dummies ()
{
    static HandleDummies _dummies;
    return _dummies;
}

// handles that can be linked
static const std::vector<int> linkable_handles = {
    Handles::RESIZE, Handles::RESIZE_H, Handles::RESIZE_V, Handles::ROTATE, Handles::SCALE,
    Handles::NODE_LOWER_LEFT, Handles::NODE_UPPER_LEFT, Handles::NODE_LOWER_RIGHT, Handles::NODE_UPPER_RIGHT,
    Handles::CROP_H, Handles::CROP_V, Handles::ROUNDING
};

// apply the displacement 'offset' of the handle of the link, with the base pose as starting status
// returns position of the handle in scene
static glm::vec2 applyHandle (Source *s, const AutomatonPose &base, const AutomatonLink &l, glm::vec2 offset,
                              Group *target = nullptr)
{
    FrameBuffer *fb = s->frame();
    if (fb == nullptr || l.handle < 0 || l.handle > 14)
        return glm::vec2(0.f);

    // status at the start of the virtual grab
    Group stored;
    base.apply(&stored);
    stored.update(0);

    // same computations as GeometryView::grab
    const glm::mat4 source_scale = glm::scale(glm::identity<glm::mat4>(), glm::vec3(1.f / fb->aspectRatio(), 1.f, 1.f));
    const glm::mat4 scene_to_source_transform = source_scale * glm::inverse(stored.transform_);
    const glm::mat4 source_to_scene_transform = glm::inverse(scene_to_source_transform);
    const glm::mat4 source_to_corner_transform = glm::translate(glm::identity<glm::mat4>(), glm::vec3(l.corner, 0.f));
    const glm::mat4 scene_to_corner_transform = source_to_corner_transform * scene_to_source_transform;
    const glm::mat4 corner_to_scene_transform = glm::inverse(scene_to_corner_transform);

    // virtual mouse grab from the point clicked when linked, moved by offset
    const glm::vec3 scene_from = glm::vec3( source_to_scene_transform * glm::vec4(l.point, 0.f, 1.f) );
    const glm::vec3 scene_to = scene_from + glm::vec3(offset, 0.f);

    HandleDummies &d = dummies();
    std::ostringstream info;
    View::Cursor cursor;
    GeometryHandleManipulation::HandleGrabContext ctx = {
        target ? target : s->group(View::GEOMETRY), // targetNode
        &stored,                        // stored_status
        fb,                             // frame
        scene_from,                     // scene_from
        scene_to,                       // scene_to
        scene_to_source_transform,      // scene_to_target_transform
        source_to_scene_transform,      // target_to_scene_transform
        scene_to_corner_transform,      // scene_to_corner_transform
        corner_to_scene_transform,      // corner_to_scene_transform
        l.corner,                       // corner
        { d.handles[l.handle], l.pick },// pick
        d.grid,                         // grid
        info,                           // info
        cursor,                         // cursor
        d.handles,                      // handles
        d.node,                         // overlay_crop
        d.symbol,                       // overlay_scaling
        d.symbol,                       // overlay_scaling_cross
        d.symbol,                       // overlay_rotation
        d.symbol,                       // overlay_rotation_fix
        d.node,                         // overlay_rotation_clock_hand
        false                           // constrained
    };

    switch (l.handle) {
    case Handles::RESIZE:           GeometryHandleManipulation::handleResize(ctx); break;
    case Handles::RESIZE_H:         GeometryHandleManipulation::handleResizeH(ctx); break;
    case Handles::RESIZE_V:         GeometryHandleManipulation::handleResizeV(ctx); break;
    case Handles::ROTATE:           GeometryHandleManipulation::handleRotate(ctx); break;
    case Handles::SCALE:            GeometryHandleManipulation::handleScale(ctx); break;
    case Handles::NODE_LOWER_LEFT:  GeometryHandleManipulation::handleNodeLowerLeft(ctx); break;
    case Handles::NODE_UPPER_LEFT:  GeometryHandleManipulation::handleNodeUpperLeft(ctx); break;
    case Handles::NODE_LOWER_RIGHT: GeometryHandleManipulation::handleNodeLowerRight(ctx); break;
    case Handles::NODE_UPPER_RIGHT: GeometryHandleManipulation::handleNodeUpperRight(ctx); break;
    case Handles::CROP_H:           GeometryHandleManipulation::handleCropH(ctx); break;
    case Handles::CROP_V:           GeometryHandleManipulation::handleCropV(ctx); break;
    case Handles::ROUNDING:         GeometryHandleManipulation::handleRounding(ctx); break;
    default: break;
    }

    return glm::vec2(scene_to);
}

//
// AutomatonRig
//

AutomatonRig::AutomatonRig (Session *session) : session_(session), selected_(0), beats_(0.0)
{
}

AutomatonRig::~AutomatonRig ()
{
    for (auto a : automatons_)
        delete a;
    automatons_.clear();
}

Automaton *AutomatonRig::add (Automaton::Type t, View *view)
{
    if (view == nullptr)
        return nullptr;

    Automaton *a = Automaton::create(t, view->mode());
    if (a == nullptr)
        return nullptr;

    // place in the center of the visible area, with a size relative to zoom
    const glm::vec2 screen( Rendering::manager().mainWindow().width() / 2,
                            Rendering::manager().mainWindow().height() / 2 );
    const glm::vec3 c = Rendering::manager().unProject(screen, view->scene.root()->transform_);
    const float zoom = std::max(view->scene.root()->scale_.x, 0.01f);
    a->reset(glm::vec2(c), 1.2f / zoom);

    // unique name by type
    int n = 1;
    for (auto b : automatons_)
        n += b->type() == t ? 1 : 0;
    a->setName( Automaton::Types[t].second + " " + std::to_string(n) );

    // in a ViewSpace shown
    // in the ViewSpace shown, or of the current source if several shown, or the first
    uint vs = 0;
    bool single = false;
    for (uint v = 0; v < MAX_VIEWSPACE && !single; ++v) {
        if (Mixer::manager().viewSpaceOnlyShown(v)) {
            vs = v;
            single = true;
        }
    }
    if (!single && Mixer::manager().currentSource() != nullptr)
        vs = Mixer::manager().currentSource()->viewSpace();
    a->setViewSpace( vs );

    add(a);
    return a;
}

void AutomatonRig::add (Automaton *a)
{
    if (a == nullptr)
        return;
    std::lock_guard<std::mutex> lock(access_);
    // sorted by view (after the others of the same view)
    automatons_.insert( std::upper_bound(automatons_.begin(), automatons_.end(), a,
                                         [](Automaton *x, Automaton *y) { return x->mode() < y->mode(); }), a);
    selected_ = a->id();
}

void AutomatonRig::remove (Automaton *a)
{
    std::lock_guard<std::mutex> lock(access_);
    auto it = std::find(automatons_.begin(), automatons_.end(), a);
    if (it != automatons_.end()) {
        if (grab_.bot == a) {
            endLinking();
            grab_ = Grab();
        }
        if (selected_ == a->id())
            selected_ = 0;
        // linked sources stay where they are
        if (PathAutomaton *pa = dynamic_cast<PathAutomaton *>(a)) {
            if (pa->active())
                for (auto &l : pa->links())
                    bake(pa, l);
        }
        automatons_.erase(it);
        // NB: sources not influenced anymore are placed at rest by next update
        delete a;
    }
}

void AutomatonRig::clear ()
{
    std::lock_guard<std::mutex> lock(access_);
    for (auto a : automatons_)
        delete a;
    automatons_.clear();
    poses_.clear();
    grab_ = Grab();
}

Automaton *AutomatonRig::find (uint64_t id) const
{
    auto it = std::find_if(automatons_.begin(), automatons_.end(), [id](Automaton *a) { return a->id() == id; });
    return it != automatons_.end() ? *it : nullptr;
}

Automaton *AutomatonRig::selected () const
{
    return selected_ > 0 ? find(selected_) : nullptr;
}

void AutomatonRig::unlink (PathAutomaton *a, const AutomatonLink &link)
{
    std::lock_guard<std::mutex> lock(access_);
    if (a->active()) {
        for (auto &l : a->links())
            if (l.sameElement(link))
                bake(a, l);
    }
    a->links().remove_if([&link](const AutomatonLink &l) { return l.sameElement(link); });
}

void AutomatonRig::setEnabled (Automaton *a, bool on)
{
    std::lock_guard<std::mutex> lock(access_);
    if (a == nullptr || a->enabled() == on)
        return;

    if (PathAutomaton *pa = dynamic_cast<PathAutomaton *>(a)) {
        for (auto &l : pa->links()) {
            // disabled: linked sources stay where they are
            // (if all disabled, the links do not act: nothing to keep)
            if (!on) {
                if (!Settings::application.automatons_disabled)
                    bake(pa, l);
            }
#if AUTOMATON_BAKE_LINKS
            // enabled: linked sources start moving from where they are
            else
                l.anchor = pa->mover();
#endif
        }
    }
    // NB: field automatons do not bake: sources pushed are given back when disabled
    a->setEnabled(on);
}

void AutomatonRig::updateTargets (Source *s, View::Mode m, bool handles)
{
    Node *node = s->group(m);

    // position of the source body
    glm::vec2 position(node->translation_);
    if (m == View::LAYER)
        position = glm::vec2(-node->translation_.z, -node->translation_.z / LAYER_PERSPECTIVE);

    // position of the points of handles on the source, from its current pose
    glm::mat4 source_to_scene(1.f);
    FrameBuffer *fb = s->frame();
    handles = handles && m == View::GEOMETRY && fb != nullptr;
    if (handles) {
        Group current;
        AutomatonPose pose;
        pose.capture(node);
        pose.apply(&current);
        current.update(0);
        const glm::mat4 source_scale = glm::scale(glm::identity<glm::mat4>(),
                                                  glm::vec3(1.f / fb->aspectRatio(), 1.f, 1.f));
        source_to_scene = glm::inverse(source_scale * glm::inverse(current.transform_));
    }

    for (auto a : automatons_) {
        if (a->mode() != m)
            continue;
        if (PathAutomaton *pa = dynamic_cast<PathAutomaton *>(a)) {
            for (auto &l : pa->links()) {
                if (l.source != s->id())
                    continue;
                if (l.handle < 0)
                    l.target = position;
                else if (handles)
                    l.target = glm::vec2( source_to_scene * glm::vec4(l.point, 0.f, 1.f) );
            }
        }
    }
}

bool AutomatonRig::rebase (Source *s, View::Mode m)
{
    bool linked = false;
    for (auto a : automatons_) {
        PathAutomaton *pa = dynamic_cast<PathAutomaton *>(a);
        if (pa == nullptr || !pa->active() || pa->mode() != m)
            continue;
        for (auto &l : pa->links()) {
            if (l.source == s->id()) {
                l.anchor = pa->mover();
                linked = true;
            }
        }
    }
    return linked;
}

void AutomatonRig::followViewSpaces ()
{
    std::lock_guard<std::mutex> lock(access_);

    for (auto a : automatons_) {
        PathAutomaton *pa = dynamic_cast<PathAutomaton *>(a);
        if (pa == nullptr || pa->links().empty())
            continue;

        // ViewSpaces of the sources linked (that exist)
        std::set<uint> spaces;
        for (uint64_t id : pa->linkedSources()) {
            auto s = session_->find(id);
            if (s != session_->end())
                spaces.insert( (*s)->viewSpace() );
        }
        if (spaces.empty() || (spaces.size() == 1 && *spaces.begin() == pa->viewSpace()))
            continue;

        // all linked sources moved together: follow them
        if (spaces.size() == 1) {
            pa->setViewSpace( *spaces.begin() );
            for (auto &l : pa->links())
                l.active = true;
            continue;
        }

        // some linked sources moved away: unlink them (they stay where they are)
        for (auto l = pa->links().begin(); l != pa->links().end(); ) {
            auto s = session_->find(l->source);
            if (s != session_->end() && (*s)->viewSpace() != pa->viewSpace()) {
                if (pa->active() && l->active)
                    bake(pa, *l);
                Log::Notify("%s unlinked from %s %s: source moved to workspace %d.",
                             pa->name().c_str(), (*s)->name().c_str(), linkLabel(*l).c_str(),
                             (*s)->viewSpace() + 1);
                l = pa->links().erase(l);
            }
            else
                ++l;
        }
    }
}

void AutomatonRig::bake (PathAutomaton *a, const AutomatonLink &l)
{
#if !AUTOMATON_BAKE_LINKS
    (void) a; (void) l;
    return;
#endif
    auto it = poses_.find( PoseKey(l.source, (int) a->mode()) );
    if (it == poses_.end())
        return;
    AutomatonPose &base = it->second.base;
    const glm::vec2 displacement = a->mover() - l.anchor;

    // body: displacement in the rest pose (as in compose)
    if (l.handle < 0) {
        if (a->mode() == View::LAYER)
            base.translation.z = CLAMP(base.translation.z - displacement.x, MIN_DEPTH, MAX_DEPTH);
        else {
            base.translation.x += displacement.x;
            base.translation.y += displacement.y;
        }
    }
    // handle: rest pose becomes the pose given by the handle
    else if (a->mode() == View::GEOMETRY) {
        auto s = session_->find(l.source);
        if (s == session_->end())
            return;
        Group pose;
        base.apply(&pose);
        applyHandle(*s, base, l, displacement, &pose);
        base.capture(&pose);
    }
}

void AutomatonRig::setPaused (bool on)
{
    std::lock_guard<std::mutex> lock(access_);
    for (auto a : automatons_)
        a->setPaused(on);
}

void AutomatonRig::restart ()
{
    std::lock_guard<std::mutex> lock(access_);
    const double b = Metronome::manager().beats();
    for (auto a : automatons_)
        a->restart(b);
}

bool AutomatonRig::allPaused () const
{
    for (auto a : automatons_)
        if (!a->paused())
            return false;
    return !automatons_.empty();
}

bool AutomatonRig::anyPaused () const
{
    for (auto a : automatons_)
        if (a->paused() || a->resuming())
            return true;
    return false;
}

void AutomatonRig::setRestPose (uint64_t source, View::Mode m, const AutomatonPose &base)
{
    RestPose &p = poses_[ PoseKey(source, (int) m) ];
    p.base = base;
    p.written_valid = false;
}

AutomatonRig::RestPose &AutomatonRig::acquire (Source *s, View::Mode m)
{
    PoseKey k(s->id(), (int) m);
    auto it = poses_.find(k);
    if (it == poses_.end()) {
        // a source newly influenced is at rest where it is
        RestPose p;
        p.base.capture( s->group(m) );
        it = poses_.emplace(k, p).first;
    }
    return it->second;
}

bool AutomatonRig::userGrabbing (Source *s, View::Mode m) const
{
    View *v = Mixer::manager().view(m);
    if (v == nullptr || Mixer::manager().view() != v || !v->initiated())
        return false;

    Source *current = Mixer::manager().currentSource();
    if (s == current || Mixer::selection().contains(s))
        return true;
    // mixing group follows the current source
    if (m == View::MIXING && current && current->mixingGroup() && current->mixingGroup()->contains(s))
        return true;

    return false;
}

void AutomatonRig::update ()
{
    std::lock_guard<std::mutex> lock(access_);

    beats_ = Metronome::manager().beats();

    // automatons are shown in views only if the session is edited
    const bool edited = Mixer::manager().session() == session_;

    // valid sources of the session by id
    std::map<uint64_t, Source *> sources;
    for (auto it = session_->begin(); it != session_->end(); ++it) {
        if ( !(*it)->failed() )
            sources[(*it)->id()] = *it;
    }

    // animate automatons
    for (auto a : automatons_) {
        View *v = Mixer::manager().view(a->mode());
        Group *fg = edited ? v->scene.fg() : nullptr;
        if (a->parent() != fg)
            a->attachTo(fg);
        a->setHighlight(edited && a->id() == selected_);
        a->setVisible(edited && Mixer::manager().viewSpaceShown(a->viewSpace()));
        // NB: during linking, only the ViewSpace of the automaton linking is shown
        a->setInteractive(Mixer::manager().viewSpaceOnlyShown(a->viewSpace()));
        a->animate(beats_, 1.f / std::max(v->scene.root()->scale_.x, 0.01f));
    }

    // links act only on sources in the ViewSpace of their automaton
    for (auto a : automatons_) {
        PathAutomaton *pa = dynamic_cast<PathAutomaton *>(a);
        if (pa == nullptr)
            continue;
        for (auto &l : pa->links()) {
            auto s = sources.find(l.source);
            const bool active = s != sources.end() && s->second->viewSpace() == a->viewSpace();
            if (pa->active() && l.active != active) {
                // leaving the ViewSpace: the source stays where it is
                if (!active)
                    bake(pa, l);
#if AUTOMATON_BAKE_LINKS
                // entering the ViewSpace: the source starts moving from where it is
                else
                    l.anchor = pa->mover();
#endif
            }
            l.active = active;
        }
    }

    // which sources are influenced by automatons
    for (auto &p : poses_) {
        p.second.influenced = false;
        p.second.source = nullptr;
    }
    for (auto a : automatons_) {
        if (!a->active())
            continue;
        const View::Mode m = a->mode();
        if (a->field()) {
            for (auto &s : sources) {
                if ( !s.second->locked() && s.second->viewSpace() == a->viewSpace()
                     && (m == View::MIXING || s.second->hasImage()) )
                    acquire(s.second, m).influenced = true;
            }
        }
        else if (PathAutomaton *pa = dynamic_cast<PathAutomaton *>(a)) {
            for (auto &l : pa->links()) {
                auto s = sources.find(l.source);
                if ( l.active && s != sources.end() && (m == View::MIXING || s->second->hasImage()) )
                    acquire(s->second, m).influenced = true;
            }
        }
    }

    // apply
    for (auto it = poses_.begin(); it != poses_.end(); ) {
        auto s = sources.find(it->first.first);
        // source does not exist anymore
        if (s == sources.end()) {
            it = poses_.erase(it);
            continue;
        }
        Source *src = s->second;
        const View::Mode m = (View::Mode) it->first.second;
        RestPose &p = it->second;
        Node *node = src->group(m);

        // let the user move the source
        const bool was_frozen = p.frozen;
        p.frozen = edited && userGrabbing(src, m);
        if (p.frozen) {
            // links follow the source moved by the user
            updateTargets(src, m, true);
            ++it;
            continue;
        }

#if AUTOMATON_BAKE_LINKS
        // released by the user: links of path automatons start again from where
        // the source was left, i.e. the rest pose is the pose of the source and
        // the links are anchored to the current position of their automaton
        if (was_frozen && rebase(src, m))
            p.base.capture(node);
        // the source was moved since last pose written: move its rest pose
        else
#else
        (void) was_frozen;
#endif
        if (p.written_valid)
            transferChannels(m, node, p.written, p.base);

        // not influenced anymore : back to rest
        if (!p.influenced) {
            copyChannels(m, p.base, node);
            if (m == View::LAYER)
                ++View::need_deep_update_;
            src->touch();
            it = poses_.erase(it);
            continue;
        }

        compose(src, m, p);
        p.source = src;
        ++it;
    }

    // draw links
    for (auto a : automatons_) {
        if (PathAutomaton *pa = dynamic_cast<PathAutomaton *>(a))
            pa->updateLinks();
    }
}

void AutomatonRig::compose (Source *s, View::Mode m, RestPose &p)
{
    Node *node = s->group(m);
    const float previous_depth = node->translation_.z;

    // start from rest
    copyChannels(m, p.base, node);

    // links of path automatons: handles first, then translation of the source
    glm::vec2 offset(0.f);
    for (auto a : automatons_) {
        if (!a->active() || a->mode() != m)
            continue;
        PathAutomaton *pa = dynamic_cast<PathAutomaton *>(a);
        if (pa == nullptr)
            continue;
        for (auto &l : pa->links()) {
            if (l.source != s->id() || !l.active)
                continue;
            const glm::vec2 displacement = pa->mover() - l.anchor;
            if (l.handle < 0)
                offset += displacement;
            else if (m == View::GEOMETRY)
                l.target = applyHandle(s, p.base, l, displacement);
        }
    }
    if (m == View::LAYER)
        node->translation_.z = CLAMP(p.base.translation.z - offset.x, MIN_DEPTH, MAX_DEPTH);
    else {
        node->translation_.x += offset.x;
        node->translation_.y += offset.y;
    }

    // fields push sources away
    if (m != View::LAYER && !s->locked()) {
        for (auto a : automatons_) {
            if (!a->active() || a->mode() != m || a->viewSpace() != s->viewSpace())
                continue;
            if (FieldAutomaton *fa = dynamic_cast<FieldAutomaton *>(a)) {
                const glm::vec2 q = fa->push( glm::vec2(node->translation_) );
                node->translation_.x = q.x;
                node->translation_.y = q.y;
            }
        }
    }

    // position of the source for drawing links (handles were given by applyHandle)
    updateTargets(s, m, false);

    // change of depth requires reordering of views
    if (m == View::LAYER && std::abs(previous_depth - node->translation_.z) > POSE_EPSILON)
        ++View::need_deep_update_;

    // request update of the source
    s->touch();
}

void AutomatonRig::commit ()
{
    std::lock_guard<std::mutex> lock(access_);

    // remember the pose after update of the sources
    for (auto &p : poses_) {
        if (p.second.source && !p.second.frozen) {
            p.second.written.capture( p.second.source->group( (View::Mode) p.first.second ) );
            p.second.written_valid = true;
        }
        p.second.source = nullptr;
    }
}

//
// Interaction
//

static glm::vec2 scenePoint (View *view, glm::vec2 screen)
{
    return glm::vec2( Rendering::manager().unProject(screen, view->scene.root()->transform_) );
}

bool AutomatonRig::pick (View *view, glm::vec2 screen)
{
    grab_ = Grab();
    if (view == nullptr)
        return false;

    const glm::vec2 p = scenePoint(view, screen);

    // the selected automaton has priority, then the last ones (drawn on top)
    std::list<Automaton *> candidates(automatons_.rbegin(), automatons_.rend());
    Automaton *sel = selected();
    if (sel) {
        candidates.remove(sel);
        candidates.push_front(sel);
    }

    for (auto a : candidates) {
        if (a->mode() != view->mode() || a->parent() == nullptr || !a->visible() || !a->interactive())
            continue;
        const int g = a->pick(p);
        if (g != Automaton::GRIP_NONE) {
            grab_.bot = a;
            grab_.grip = g;
            grab_.from = p;
            grab_.stored = a->controls();
            selected_ = a->id();
            return true;
        }
    }
    return false;
}

View::Cursor AutomatonRig::drag (View *view, glm::vec2 screen)
{
    if (grab_.bot == nullptr || view == nullptr || grab_.bot->mode() != view->mode())
        return View::Cursor();

    const glm::vec2 p = scenePoint(view, screen);

    // drag from the moving part: create a link
    if (grab_.grip == Automaton::GRIP_MOVER) {
        PathAutomaton *pa = dynamic_cast<PathAutomaton *>(grab_.bot);
        if (pa == nullptr)
            return View::Cursor();
        pa->showLinking(true, p);
        beginLinking();

        AutomatonLink l;
        std::string label;
        if (pickTarget(view, screen, pa->viewSpace(), l, label)) {
            const bool exists = std::any_of(pa->links().begin(), pa->links().end(),
                                            [&l](const AutomatonLink &k) { return k.sameElement(l); });
            return View::Cursor(View::Cursor_Hand, std::string(exists ? ICON_FA_UNLINK " Unlink " : ICON_FA_LINK " Link ") + label);
        }
        return View::Cursor(View::Cursor_Hand, ICON_FA_LINK " Release on a source");
    }

    // buttons are clicked, not dragged
    if (grab_.grip == Automaton::GRIP_ADD_KEY || grab_.grip == Automaton::GRIP_REMOVE_KEY)
        return View::Cursor(View::Cursor_Hand);

    // drag a grip: change geometry
    grab_.bot->grab(grab_.grip, grab_.stored, p - grab_.from);
    grab_.moved = true;
    return View::Cursor(View::Cursor_ResizeAll, std::string(ICON_FA_ATOM " ") + grab_.bot->name());
}

void AutomatonRig::beginLinking ()
{
    if (grab_.linking || grab_.bot == nullptr)
        return;
    // show only the sources that can be linked (in the ViewSpace of the automaton)
    grab_.linking = true;
    grab_.viewspaces = session_->viewSpaces();
    session_->setViewSpaces( 1u << grab_.bot->viewSpace() );
}

void AutomatonRig::endLinking ()
{
    // show the ViewSpaces as before
    if (grab_.linking)
        session_->setViewSpaces( grab_.viewspaces );
    grab_.linking = false;
}

void AutomatonRig::release (View *view, glm::vec2 screen)
{
    std::string label;

    if (grab_.bot && view && grab_.grip == Automaton::GRIP_MOVER) {
        PathAutomaton *pa = dynamic_cast<PathAutomaton *>(grab_.bot);
        if (pa) {
            pa->showLinking(false);
            AutomatonLink l;
            std::string target;
            if (pickTarget(view, screen, pa->viewSpace(), l, target)) {
                l.anchor = pa->mover();
                bool added = false;
                {
                    std::lock_guard<std::mutex> lock(access_);
                    // unlinked source stays where it is
                    if (pa->active()) {
                        for (auto &e : pa->links())
                            if (e.sameElement(l))
                                bake(pa, e);
                    }
                    added = pa->toggleLink(l);
                }
                label = pa->name() + ": " + (added ? "Link " : "Unlink ") + target;
            }
        }
    }
    else if (grab_.bot && view && (grab_.grip == Automaton::GRIP_ADD_KEY || grab_.grip == Automaton::GRIP_REMOVE_KEY)) {
        // click on a button: released over the same button
        FreePathAutomaton *fp = dynamic_cast<FreePathAutomaton *>(grab_.bot);
        if (fp && fp->pick(scenePoint(view, screen)) == grab_.grip) {
            const bool add = grab_.grip == Automaton::GRIP_ADD_KEY;
            {
                std::lock_guard<std::mutex> lock(access_);
                if (add)
                    fp->addKey();
                else
                    fp->removeKey();
            }
            label = fp->name() + (add ? ": Add key" : ": Remove key");
        }
    }
    else if (grab_.bot && grab_.moved)
        label = grab_.bot->name() + ": Edit";

    endLinking();
    grab_ = Grab();

    // store in history (NB: not under lock; history saves the rig)
    if (!label.empty())
        Action::manager().store(label);
}

void AutomatonRig::doubleclic ()
{
    // (buttons can be clicked repeatedly)
    if (grab_.bot == nullptr || grab_.grip == Automaton::GRIP_ADD_KEY || grab_.grip == Automaton::GRIP_REMOVE_KEY)
        return;
    Automaton *a = grab_.bot;
    {
        std::lock_guard<std::mutex> lock(access_);
        a->setPaused( !a->paused() );
    }
    Action::manager().store(a->name() + (a->paused() ? ": Pause" : ": Play"));
}

void AutomatonRig::cancel ()
{
    if (grab_.bot && grab_.grip == Automaton::GRIP_MOVER) {
        if (PathAutomaton *pa = dynamic_cast<PathAutomaton *>(grab_.bot))
            pa->showLinking(false);
    }
    else if (grab_.bot && grab_.moved)
        grab_.bot->setControls(grab_.stored);
    endLinking();
    grab_ = Grab();
}

View::Cursor AutomatonRig::over (View *view, glm::vec2 screen)
{
    View::Cursor ret;
    if (view == nullptr || grab_.bot != nullptr)
        return ret;

    const glm::vec2 p = scenePoint(view, screen);
    bool found = false;
    for (auto it = automatons_.rbegin(); it != automatons_.rend(); ++it) {
        Automaton *a = *it;
        int g = Automaton::GRIP_NONE;
        if (!found && a->mode() == view->mode() && a->parent() != nullptr && a->visible() && a->interactive())
            g = a->pick(p);
        a->hover(g);
        if (g == Automaton::GRIP_MOVER || g == Automaton::GRIP_MOVE
            || g == Automaton::GRIP_ADD_KEY || g == Automaton::GRIP_REMOVE_KEY) {
            found = true;
            ret.type = View::Cursor_Hand;
            ret.info = std::string(ICON_FA_ATOM " ");
            if (g == Automaton::GRIP_MOVER)
                ret.info += "Drag to a source to link";
            else if (g == Automaton::GRIP_ADD_KEY)
                ret.info += "Add key point";
            else if (g == Automaton::GRIP_REMOVE_KEY)
                ret.info += "Remove key point";
            else {
                ret.info += a->name();
                ret.info += a->paused() ? "\n   (double clic to play)" : "\n   (double clic to pause)";
            }
            // else
            //
        }
    }
    return ret;
}

bool AutomatonRig::pickTarget (View *view, glm::vec2 screen, uint viewspace, AutomatonLink &link, std::string &label) const
{
    const View::Mode m = view->mode();

    // pick in the scene of the view (without the side effects of View::pick)
    PickingVisitor pv( Rendering::manager().unProject(screen) );
    view->scene.accept(pv);

    for (auto it = pv.rbegin(); it != pv.rend(); ++it) {
        Source *s = Mixer::manager().findSource(it->first);
        if (s == nullptr || (m != View::MIXING && !s->hasImage()) || s->viewSpace() != viewspace)
            continue;

        link = AutomatonLink();
        link.source = s->id();

        // handles of the source (visible only on current source)
        if (m == View::GEOMETRY) {
            for (int t : linkable_handles) {
                if (it->first == s->handle(View::GEOMETRY, t)) {
                    link.handle = t;
                    link.pick = it->second;
                    link.corner = glm::round(it->second);
                    break;
                }
            }
            // the point clicked, in source coordinates
            FrameBuffer *fb = s->frame();
            if (fb) {
                const Group *node = s->group(View::GEOMETRY);
                const glm::mat4 T = GlmToolkit::transform(node->translation_, node->rotation_, node->scale_);
                const glm::mat4 scene_to_source = glm::scale(glm::identity<glm::mat4>(),
                                                             glm::vec3(1.f / fb->aspectRatio(), 1.f, 1.f))
                                                  * glm::inverse(T);
                link.point = glm::vec2( scene_to_source * glm::vec4(scenePoint(view, screen), 0.f, 1.f) );
            }
        }

        label = s->name() + " " + linkLabel(link);
        return true;
    }
    return false;
}

std::string AutomatonRig::linkLabel (const AutomatonLink &link)
{
    auto corner = [&link]() {
        std::string c = link.corner.y > 0.f ? "top" : "bottom";
        c += link.corner.x > 0.f ? "-right" : "-left";
        return c;
    };
    auto side = [&link](bool horizontal) {
        if (horizontal)
            return std::string(link.corner.x > 0.f ? "right" : "left");
        return std::string(link.corner.y > 0.f ? "top" : "bottom");
    };

    switch (link.handle) {
    case -1:                        return "position";
    case Handles::RESIZE:           return "resize " + corner();
    case Handles::RESIZE_H:         return "resize " + side(true);
    case Handles::RESIZE_V:         return "resize " + side(false);
    case Handles::ROTATE:           return "rotation";
    case Handles::SCALE:            return "scale";
    case Handles::NODE_LOWER_LEFT:  return "shape bottom-left";
    case Handles::NODE_UPPER_LEFT:  return "shape top-left";
    case Handles::NODE_LOWER_RIGHT: return "shape bottom-right";
    case Handles::NODE_UPPER_RIGHT: return "shape top-right";
    case Handles::CROP_H:           return "crop " + side(true);
    case Handles::CROP_V:           return "crop " + side(false);
    case Handles::ROUNDING:         return "corner rounding";
    default:                        return "?";
    }
}
