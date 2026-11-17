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

#include <glm/gtc/constants.hpp>

#include "defines.h"
#include "Scene/Scene.h"
#include "Scene/Primitives.h"
#include "Scene/Decorations.h"
#include "Visitor/Visitor.h"

#include "PathAutomaton.h"

using namespace AutomatonMath;

bool AutomatonLink::sameElement (const AutomatonLink &other) const
{
    if (source != other.source || handle != other.handle)
        return false;
    // handles with several corners: same corner
    return handle < 0 || glm::distance(corner, other.corner) < 0.5f;
}

//
// PathAutomaton
//

PathAutomaton::PathAutomaton (View::Mode m, uint64_t id) : Automaton(m, id),
    mover_(0.f), path_node_(nullptr), linking_line_(nullptr)
{
    links_node_ = new Group;
    links_node_->translation_.z = 0.01f;
    root_->attach(links_node_);

    // circle as mover
    mover_node_ = new Symbol(Symbol::CIRCLE_POINT);
    mover_node_->translation_.z = 0.04f;
    root_->attach(mover_node_);

    linking_line_ = replaceLine(links_node_, nullptr, { glm::vec2(0.f), glm::vec2(0.1f, 0.f) },
                                AUTOMATON_LINE_WIDTH, AUTOMATON_ALPHA_LINKING);
    linking_line_->visible_ = false;
}

int PathAutomaton::pick (glm::vec2 p) const
{
    // the moving part has priority
    if (glm::distance(p, mover_) < AUTOMATON_GRIP_PICK * AUTOMATON_MOVER_PICK_SCALE * uiscale_)
        return GRIP_MOVER;
    return Automaton::pick(p);
}

void PathAutomaton::setMover (glm::vec2 p)
{
    mover_ = p;
    const float s = AUTOMATON_MOVER_SIZE / AUTOMATON_MESH_POINT_SIZE * uiscale_
                    * (hover_ == GRIP_MOVER ? AUTOMATON_MOVER_HOVER_SCALE : 1.f);
    mover_node_->translation_ = glm::vec3(p, 0.04f);
    mover_node_->scale_ = glm::vec3(s, s, 1.f);
    mover_node_->color = color(AUTOMATON_ALPHA_MOVER);
    if (path_node_)
        path_node_->shader()->color = color(AUTOMATON_ALPHA_PATH);

    // links follow the mover
    updateLinks();
}

void PathAutomaton::setPath (const std::vector<glm::vec2> &path, bool loop)
{
    path_node_ = replaceLine(root_, path_node_, path, AUTOMATON_LINE_WIDTH, AUTOMATON_ALPHA_PATH, loop);
    path_node_->translation_.z = 0.02f;
}

bool PathAutomaton::toggleLink (const AutomatonLink &link)
{
    auto it = std::find_if(links_.begin(), links_.end(),
                           [&link](const AutomatonLink &l) { return l.sameElement(link); });
    if (it != links_.end()) {
        links_.erase(it);
        return false;
    }
    links_.push_back(link);
    return true;
}

std::set<uint64_t> PathAutomaton::linkedSources () const
{
    std::set<uint64_t> ids;
    for (auto &l : links_)
        ids.insert(l.source);
    return ids;
}

void PathAutomaton::removeLinks (uint64_t source)
{
    links_.remove_if([source](const AutomatonLink &l) { return l.source == source; });
}

void PathAutomaton::updateLinks ()
{
    // as many lines as links
    while (link_lines_.size() < links_.size())
        link_lines_.push_back( replaceLine(links_node_, nullptr, { mover_, mover_ + glm::vec2(0.1f, 0.f) },
                                           AUTOMATON_LINE_WIDTH_LINK, AUTOMATON_ALPHA_LINK) );
    while (link_lines_.size() > links_.size()) {
        links_node_->detach(link_lines_.back());
        delete link_lines_.back();
        link_lines_.pop_back();
    }
    // as many points at the end of lines
    while (link_points_.size() < links_.size()) {
        link_points_.push_back( new Symbol(Symbol::CIRCLE_POINT) );
        links_node_->attach(link_points_.back());
    }
    while (link_points_.size() > links_.size()) {
        links_node_->detach(link_points_.back());
        delete link_points_.back();
        link_points_.pop_back();
    }

    const float s = AUTOMATON_LINK_POINT_SIZE / AUTOMATON_MESH_POINT_SIZE * uiscale_;
    size_t i = 0;
    for (auto l = links_.begin(); l != links_.end(); ++l, ++i) {
        // links not acting are not drawn
        link_lines_[i]->visible_ = l->active;
        link_points_[i]->visible_ = l->active;
        setLine(link_lines_[i], { mover_, l->target });
        link_lines_[i]->shader()->color = color(AUTOMATON_ALPHA_LINK);
        // point on the element of the source linked
        link_points_[i]->translation_ = glm::vec3(l->target, 0.01f);
        link_points_[i]->scale_ = glm::vec3(s, s, 1.f);
        link_points_[i]->color = color(AUTOMATON_ALPHA_LINK_POINT);
    }
}

void PathAutomaton::showLinking (bool on, glm::vec2 to)
{
    linking_line_->visible_ = on;
    if (on) {
        setLine(linking_line_, { mover_, to });
        linking_line_->shader()->color = glm::vec4(COLOR_AUTOMATON, AUTOMATON_ALPHA_LINKING);
    }
}

void PathAutomaton::accept (Visitor &v)
{
    Automaton::accept(v);
    v.visit(*this);
}

//
// LinearAutomaton
//

LinearAutomaton::LinearAutomaton (View::Mode m, uint64_t id) : PathAutomaton(m, id)
{
    period_ = 2.0;
    reset(glm::vec2(0.f), 0.5f);
}

void LinearAutomaton::reset (glm::vec2 c, float size)
{
    controls_ = { c - glm::vec2(size, 0.f), c + glm::vec2(size, 0.f) };
    rebuild();
}

glm::vec2 LinearAutomaton::center () const
{
    return 0.5f * (controls_[0] + controls_[1]);
}

void LinearAutomaton::rebuild ()
{
    setPath(controls_);
}

void LinearAutomaton::motion ()
{
    const float u = wave(float(progress_), shape_);
    setMover( reverse_ ? glm::mix(controls_[1], controls_[0], u) : glm::mix(controls_[0], controls_[1], u) );
}

float LinearAutomaton::curve (float u) const
{
    return wave(u, shape_);
}

std::vector<float> LinearAutomaton::events () const
{
    // bounce
    return { wavePeak(shape_) };
}

//
// CircularAutomaton
//

CircularAutomaton::CircularAutomaton (View::Mode m, uint64_t id) : PathAutomaton(m, id)
{
    period_ = 4.0;
    reset(glm::vec2(0.f), 0.5f);
}

void CircularAutomaton::reset (glm::vec2 c, float size)
{
    controls_ = { c, c + glm::vec2(size, 0.f), c + glm::vec2(0.f, size * 0.7f) };
    rebuild();
}

void CircularAutomaton::constrain (int)
{
    // radius points on the axis of the ellipse, with a minimum radius
    const float m = 0.01f;
    controls_[1].y = controls_[0].y;
    controls_[1].x = controls_[0].x + std::max(m, std::abs(controls_[1].x - controls_[0].x));
    controls_[2].x = controls_[0].x;
    controls_[2].y = controls_[0].y + std::max(m, std::abs(controls_[2].y - controls_[0].y));
}

glm::vec2 CircularAutomaton::radius () const
{
    return glm::vec2(controls_[1].x - controls_[0].x, controls_[2].y - controls_[0].y);
}

void CircularAutomaton::rebuild ()
{
    std::vector<glm::vec2> path;
    const size_t N = 72;
    for (size_t i = 0; i < N; ++i)
        path.push_back( ellipse(controls_[0], radius(), float(i) / float(N)) );
    setPath(path, true);
}

void CircularAutomaton::motion ()
{
    // starts at the top (like a clock), turns clockwise unless reversed
    const float t = ramp(float(progress_), shape_);
    const float turn = reverse_ ? 0.25f + t : 0.25f - t;
    setMover( ellipse(controls_[0], radius(), turn) );
}

float CircularAutomaton::curve (float u) const
{
    return ramp(u, shape_);
}

std::vector<float> CircularAutomaton::events () const
{
    // quarters of turn
    std::vector<float> e;
    for (int q = 1; q < 4; ++q) {
        const float target = float(q) * 0.25f;
        float a = 0.f, b = 1.f;
        for (int i = 0; i < 24; ++i) {
            const float m = 0.5f * (a + b);
            if (ramp(m, shape_) < target)
                a = m;
            else
                b = m;
        }
        e.push_back(0.5f * (a + b));
    }
    return e;
}

//
// FreePathAutomaton
//

FreePathAutomaton::FreePathAutomaton (View::Mode m, uint64_t id) : PathAutomaton(m, id)
{
    period_ = 4.0;
    shape_ = SHAPE_SMOOTH;
    reset(glm::vec2(0.f), 0.5f);
}

void FreePathAutomaton::reset (glm::vec2 c, float size)
{
    controls_ = { c + glm::vec2(-size, -0.3f * size), c + glm::vec2(-0.35f * size, 0.5f * size),
                  c + glm::vec2(0.35f * size, -0.5f * size), c + glm::vec2(size, 0.3f * size) };
    rebuild();
}

void FreePathAutomaton::addKey ()
{
    const size_t n = controls_.size();
    glm::vec2 last = controls_[n - 1];
    glm::vec2 dir = last - controls_[n - 2];
    if (glm::length(dir) < 1e-3f)
        dir = glm::vec2(0.2f, 0.f);
    controls_.push_back( last + 0.5f * dir );
    rebuild();
}

void FreePathAutomaton::removeKey ()
{
    if (controls_.size() > 2) {
        controls_.pop_back();
        rebuild();
    }
}

void FreePathAutomaton::rebuild ()
{
    std::vector<size_t> idx;
    std::vector<glm::vec2> path = smoothPath(controls_, AUTOMATON_PATH_SAMPLES, &idx);
    arc_.set(path);
    key_fractions_.clear();
    for (size_t i : idx)
        key_fractions_.push_back( arc_.fraction(i) );
    setPath(path);
}

void FreePathAutomaton::motion ()
{
    const float u = wave(float(progress_), shape_);
    setMover( arc_.at(reverse_ ? 1.f - u : u) );
}

float FreePathAutomaton::curve (float u) const
{
    return wave(u, shape_);
}

std::vector<float> FreePathAutomaton::events () const
{
    // passing on intermediate key points, forward and backward, and bounce at the end
    std::vector<float> e;
    for (size_t k = 1; k + 1 < key_fractions_.size(); ++k) {
        const float f = reverse_ ? 1.f - key_fractions_[k] : key_fractions_[k];
        e.push_back( waveInverse(f, true) );
        e.push_back( waveInverse(f, false) );
    }
    e.push_back( wavePeak(shape_) );
    std::sort(e.begin(), e.end());
    return e;
}
