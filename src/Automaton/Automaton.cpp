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
#include <cmath>

#include <glm/gtc/constants.hpp>

#include "IconsFontAwesome5.h"

#include "defines.h"
#include "Scene/Scene.h"
#include "Scene/Primitives.h"
#include "Scene/Decorations.h"
#include "Visitor/Visitor.h"
#include "Toolkit/BaseToolkit.h"

#include "AutomatonMath.h"
#include "PathAutomaton.h"
#include "FieldAutomaton.h"
#include "Automaton.h"

std::vector< std::pair<const char *, std::string> > Automaton::Types = {
    { ICON_FA_ARROWS_ALT_H, std::string("Linear") },
    { ICON_FA_CIRCLE_NOTCH, std::string("Circular") },
    { ICON_FA_BEZIER_CURVE, std::string("Free path") },
    { ICON_FA_HEARTBEAT,    std::string("Heart") },
    { ICON_FA_GRIP_LINES,   std::string("Rift") }
};

std::vector< std::string > Automaton::Shapes = { "Linear", "Smooth", "Pulse" };

Automaton *Automaton::create (Type t, View::Mode m, uint64_t id)
{
    if (!accepts(t, m))
        return nullptr;

    switch (t) {
    case AUTOMATON_LINEAR:
        return new LinearAutomaton(m, id);
    case AUTOMATON_CIRCULAR:
        return new CircularAutomaton(m, id);
    case AUTOMATON_PATH:
        return new FreePathAutomaton(m, id);
    case AUTOMATON_HEART:
        return new HeartAutomaton(m, id);
    case AUTOMATON_RIFT:
        return new RiftAutomaton(m, id);
    default:
        return nullptr;
    }
}

bool Automaton::accepts (Type t, View::Mode m)
{
    // path automatons in views where sources are moved in 2D or in depth
    if (t < AUTOMATON_HEART)
        return m == View::MIXING || m == View::GEOMETRY || m == View::LAYER;
    // field automatons only in 2D views
    if (t < AUTOMATON_INVALID)
        return m == View::MIXING || m == View::GEOMETRY;
    return false;
}

Automaton::Automaton (View::Mode m, uint64_t id) : id_(id), mode_(m), enabled_(true), paused_(false), resuming_(false), resume_at_(RESUME_UNSET), loop_(true), stop_at_(RESUME_UNSET), time_(0.0),
    period_(4.0), offset_(0.0), shape_(AutomatonMath::SHAPE_LINEAR), reverse_(false),
    progress_(0.0), uiscale_(1.f), parent_(nullptr), hover_(GRIP_NONE), highlight_(false)
{
    if (id_ == 0)
        id_ = BaseToolkit::uniqueId();

    // frame_ is attached in the foreground of views, which can transform it
    // (e.g. Geometry view scales the nodes of its foreground to the aspect ratio
    // of the output): root_ compensates for it (see animate)
    frame_ = new Group;
    root_ = new Group;
    frame_->attach(root_);

    // grips on top of everything
    grips_ = new Group;
    grips_->translation_.z = 0.05f;
    root_->attach(grips_);
    move_grip_ = new Symbol(Symbol::SQUARE);
    move_grip_->translation_.z = 0.005f;
    grips_->attach(move_grip_);
}

Automaton::~Automaton ()
{
    if (parent_)
        parent_->detach(frame_);
    // deletes all children nodes
    delete frame_;
}

void Automaton::attachTo (Group *parent)
{
    if (parent_ != nullptr)
        parent_->detach(frame_);

    parent_ = parent;

    if (parent_ != nullptr)
        parent_->attach(frame_);
}

void Automaton::setEnabled (bool on)
{
    enabled_ = on;
}

void Automaton::setPaused (bool on)
{
    stop_at_ = RESUME_UNSET;
    if (on) {
        // pausing while waiting to resume: stay still
        paused_ = true;
        resuming_ = false;
    }
    else if (paused_) {
        // resume at next possible beat (see animate)
        paused_ = false;
        setResuming(true);
    }
}

void Automaton::setLoop (bool on)
{
    loop_ = on;
    stop_at_ = RESUME_UNSET;
}

void Automaton::setPeriod (double p)
{
    // a period is at least 1/16th of a beat
    period_ = std::max(p, 1.0 / 16.0);
    stop_at_ = RESUME_UNSET;
}

void Automaton::setOffset (double o)
{
    offset_ = o;
    stop_at_ = RESUME_UNSET;
}

void Automaton::setControls (const std::vector<glm::vec2> &c)
{
    if (c.size() < controls_.size() && type() != AUTOMATON_PATH)
        return;
    controls_ = c;
    constrain(GRIP_NONE);
    rebuild();
}

glm::vec4 Automaton::color (float alpha) const
{
    if (!enabled_)
        return glm::vec4(COLOR_AUTOMATON, alpha * AUTOMATON_ALPHA_DISABLED);
    return glm::vec4(COLOR_AUTOMATON, highlight_ ? std::min(1.f, alpha * AUTOMATON_ALPHA_HIGHLIGHT) : alpha);
}

glm::vec2 Automaton::center () const
{
    glm::vec2 c(0.f);
    for (auto &p : controls_)
        c += p;
    if (!controls_.empty())
        c /= float(controls_.size());
    return c;
}

void Automaton::animate (double beats, float uiscale)
{
    uiscale_ = uiscale;
    // cancel the transform of the frame by the view
    root_->scale_ = glm::vec3( 1.f / std::max(std::abs(frame_->scale_.x), 1e-3f),
                               1.f / std::max(std::abs(frame_->scale_.y), 1e-3f), 1.f );
    root_->translation_ = glm::vec3( -frame_->translation_.x * root_->scale_.x,
                                     -frame_->translation_.y * root_->scale_.y, 0.f );
    if (resuming_) {
        // first beat time from which the motion can continue where it stopped:
        // the delay is a whole number of beats (resume immediately if on the same beat)
        if (resume_at_ == RESUME_UNSET)
            resume_at_ = time_ + std::ceil(beats - time_ - 1e-6);
        if (beats >= resume_at_) {
            // shift the phase by the delay (modulo period to keep it small)
            double o = offset_ + (resume_at_ - time_);
            o -= period_ * std::floor(o / period_);
            // keep a negative offset for display if it was
            if (offset_ < 0.0 && o > 0.0)
                o -= period_;
            offset_ = o;
            resuming_ = false;
            resume_at_ = RESUME_UNSET;
        }
    }
    if (!paused_ && !resuming_) {
        // not looping: play until the end of the current cycle, then pause
        if (!loop_) {
            if (stop_at_ == RESUME_UNSET) {
                // end of the current cycle (a full cycle if at its very end)
                // NB: from beats and not time_, which is the time of pause when
                // just resumed (and the offset was shifted to resume)
                const double p = AutomatonMath::progress(beats, period_, offset_);
                stop_at_ = beats + (1.0 - p) * period_;
                if (stop_at_ - beats < 1e-3 * period_)
                    stop_at_ += period_;
            }
            if (beats >= stop_at_) {
                beats = stop_at_;
                paused_ = true;
                stop_at_ = RESUME_UNSET;
            }
        }
        time_ = beats;
    }
    progress_ = AutomatonMath::progress(time_, period_, offset_);
    motion();
    updateGrips();
}

void Automaton::updateGrips ()
{
    // as many grips as controls
    while (grip_nodes_.size() < controls_.size()) {
        // diamond: square turned by 45 degrees (like handles to change shape in Geometry view)
        Symbol *d = new Symbol(Symbol::SQUARE_POINT);
        d->rotation_.z = glm::quarter_pi<float>();
        grips_->attach(d);
        grip_nodes_.push_back(d);
    }
    while (grip_nodes_.size() > controls_.size()) {
        grips_->detach(grip_nodes_.back());
        delete grip_nodes_.back();
        grip_nodes_.pop_back();
    }

    const float s = AUTOMATON_GRIP_SIZE / AUTOMATON_MESH_POINT_SIZE * uiscale_;
    for (size_t i = 0; i < grip_nodes_.size(); ++i) {
        Symbol *d = grip_nodes_[i];
        d->visible_ = gripShown(i);
        d->translation_ = glm::vec3(controls_[i], 0.f);
        d->scale_ = glm::vec3(s, s, 1.f) * (hover_ == int(i) ? AUTOMATON_GRIP_HOVER_SCALE : 1.f);
        d->color = color(hover_ == int(i) ? AUTOMATON_ALPHA_HOVER : AUTOMATON_ALPHA_GRIP);
    }

    const float c = AUTOMATON_MOVER_SIZE / AUTOMATON_MESH_CROSS_SIZE * uiscale_ * (hover_ == GRIP_MOVE ? AUTOMATON_GRIP_HOVER_SCALE : 1.f);
    move_grip_->translation_ = glm::vec3(center(), 0.005f);
    move_grip_->scale_ = glm::vec3(c, c, 1.f);
    move_grip_->color = color(hover_ == GRIP_MOVE ? AUTOMATON_ALPHA_HOVER : AUTOMATON_ALPHA_GRIP_MOVE);
}

int Automaton::pick (glm::vec2 p) const
{
    const float r = AUTOMATON_GRIP_PICK * uiscale_;
    // control points first
    for (size_t i = 0; i < controls_.size(); ++i) {
        if (gripShown(i) && glm::distance(p, controls_[i]) < r)
            return int(i);
    }
    // move all
    if (glm::distance(p, center()) < r * 1.2f)
        return GRIP_MOVE;
    return GRIP_NONE;
}

void Automaton::grab (int grip, const std::vector<glm::vec2> &stored, glm::vec2 delta)
{
    if (stored.size() != controls_.size())
        return;

    if (grip == GRIP_MOVE) {
        for (size_t i = 0; i < controls_.size(); ++i)
            controls_[i] = stored[i] + delta;
    }
    else if (grip >= 0 && grip < int(controls_.size())) {
        controls_ = stored;
        controls_[grip] = stored[grip] + delta;
    }
    else
        return;

    constrain(grip);
    rebuild();
}

void Automaton::accept (Visitor &v)
{
    v.visit(*this);
}

float Automaton::waveInverse (float value, bool rising) const
{
    // bisection on the monotonic part of the wave
    const float peak = AutomatonMath::wavePeak(shape_);
    float a = rising ? 0.f : peak;
    float b = rising ? peak : 1.f;
    for (int i = 0; i < 24; ++i) {
        const float m = 0.5f * (a + b);
        const float w = AutomatonMath::wave(m, shape_);
        if ( (w < value) == rising )
            a = m;
        else
            b = m;
    }
    return 0.5f * (a + b);
}

LineStrip *Automaton::replaceLine (Group *parent, LineStrip *line, const std::vector<glm::vec2> &path,
                                   float width, float alpha, bool loop)
{
    glm::vec4 c(COLOR_AUTOMATON, alpha);
    if (line) {
        c = line->shader()->color;
        parent->detach(line);
        delete line;
    }
    // prevent degenerated paths
    std::vector<glm::vec2> p = path;
    if (p.size() < 2)
        p.resize(2, p.empty() ? glm::vec2(0.f) : p.front());
    for (size_t i = 1; i < p.size(); ++i) {
        if (glm::distance(p[i], p[i-1]) < 1e-5f)
            p[i] += glm::vec2(1e-4f, 1e-4f);
    }
    LineStrip *l = loop ? new LineLoop(p, width) : new LineStrip(p, width);
    l->shader()->color = c;
    parent->attach(l);
    return l;
}

void Automaton::setLine (LineStrip *line, const std::vector<glm::vec2> &path)
{
    if (!line)
        return;
    std::vector<glm::vec2> p = path;
    for (size_t i = 1; i < p.size(); ++i) {
        if (glm::distance(p[i], p[i-1]) < 1e-5f)
            p[i] += glm::vec2(1e-4f, 1e-4f);
    }
    line->changePath(p);
}
