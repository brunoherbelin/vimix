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

#include "defines.h"
#include "Scene/Scene.h"
#include "Scene/Primitives.h"
#include "Scene/Mesh.h"

#include "AutomatonMath.h"
#include "FieldAutomaton.h"

using namespace AutomatonMath;

#define RING_SAMPLES 72

static std::vector<glm::vec2> circle (glm::vec2 c, float r)
{
    std::vector<glm::vec2> path;
    for (size_t i = 0; i < RING_SAMPLES; ++i)
        path.push_back( ellipse(c, glm::vec2(r), float(i) / float(RING_SAMPLES)) );
    return path;
}

//
// HeartAutomaton
//

HeartAutomaton::HeartAutomaton (View::Mode m, uint64_t id) : FieldAutomaton(m, id),
    radius_(0.f), max_ring_(nullptr)
{
    period_ = 1.0;
    shape_ = SHAPE_PULSE;

    disk_ = new Mesh("mesh/disk.ply");
    disk_->translation_.z = 0.005f;
    root_->attach(disk_);

    ring_ = replaceLine(root_, nullptr, circle(glm::vec2(0.f), 1.f), AUTOMATON_LINE_WIDTH_MOTION, AUTOMATON_ALPHA_MOTION, true);
    ring_->translation_.z = 0.03f;

    reset(glm::vec2(0.f), 0.5f);
}

void HeartAutomaton::reset (glm::vec2 c, float size)
{
    controls_ = { c, c + glm::vec2(0.3f * size, 0.f), c + glm::vec2(size, 0.f) };
    rebuild();
}

void HeartAutomaton::constrain (int)
{
    // the maximum radius is beyond the disk radius
    const float r0 = std::max(0.01f, glm::distance(controls_[0], controls_[1]));
    float r1 = glm::distance(controls_[0], controls_[2]);
    if (r1 < r0 + 0.01f) {
        glm::vec2 d = controls_[2] - controls_[0];
        d = glm::length(d) > 1e-5f ? glm::normalize(d) : glm::vec2(1.f, 0.f);
        controls_[2] = controls_[0] + d * (r0 + 0.01f);
    }
}

void HeartAutomaton::rebuild ()
{
    const float r0 = glm::distance(controls_[0], controls_[1]);
    const float r1 = glm::distance(controls_[0], controls_[2]);
    disk_->translation_ = glm::vec3(controls_[0], 0.005f);
    disk_->scale_ = glm::vec3(r0, r0, 1.f);
    max_ring_ = replaceLine(root_, max_ring_, circle(controls_[0], r1), AUTOMATON_LINE_WIDTH_GUIDE, AUTOMATON_ALPHA_GUIDE, true);
    max_ring_->translation_.z = 0.02f;
}

void HeartAutomaton::motion ()
{
    const float r0 = glm::distance(controls_[0], controls_[1]);
    const float r1 = glm::distance(controls_[0], controls_[2]);
    radius_ = r0 + (r1 - r0) * wave(float(progress_), shape_);
    setLine(ring_, circle(controls_[0], radius_));
    ring_->shader()->color = color(AUTOMATON_ALPHA_MOTION);
    max_ring_->shader()->color = color(AUTOMATON_ALPHA_GUIDE);
    disk_->shader()->color = color(AUTOMATON_ALPHA_FILL);
}

glm::vec2 HeartAutomaton::push (glm::vec2 p) const
{
    return pushRadial(p, controls_[0], radius_);
}

float HeartAutomaton::curve (float u) const
{
    return wave(u, shape_);
}

std::vector<float> HeartAutomaton::events () const
{
    return { wavePeak(shape_) };
}

//
// RiftAutomaton
//

RiftAutomaton::RiftAutomaton (View::Mode m, uint64_t id) : FieldAutomaton(m, id),
    halfgap_(0.2f), gap_(0.f), axis_(nullptr)
{
    period_ = 2.0;
    shape_ = SHAPE_SMOOTH;

    const std::vector<glm::vec2> p = { glm::vec2(0.f), glm::vec2(1.f, 0.f) };
    line_a_ = replaceLine(root_, nullptr, p, AUTOMATON_LINE_WIDTH_MOTION, AUTOMATON_ALPHA_MOTION);
    line_a_->translation_.z = 0.03f;
    line_b_ = replaceLine(root_, nullptr, p, AUTOMATON_LINE_WIDTH_MOTION, AUTOMATON_ALPHA_MOTION);
    line_b_->translation_.z = 0.03f;

    reset(glm::vec2(0.f), 0.5f);
}

void RiftAutomaton::reset (glm::vec2 c, float size)
{
    controls_ = { c, c + glm::vec2(size, 0.f), c + glm::vec2(0.f, 0.4f * size) };
    halfgap_ = 0.4f * size;
    rebuild();
}

glm::vec2 RiftAutomaton::direction () const
{
    const glm::vec2 d = controls_[1] - controls_[0];
    return glm::length(d) > 1e-5f ? glm::normalize(d) : glm::vec2(1.f, 0.f);
}

void RiftAutomaton::constrain (int grip)
{
    const glm::vec2 d = direction();
    const glm::vec2 n(-d.y, d.x);
    // the gap point is on the normal of the lines, at the maximum gap
    if (grip == 2 || grip == GRIP_NONE)
        halfgap_ = std::max(0.01f, glm::dot(controls_[2] - controls_[0], n));
    controls_[2] = controls_[0] + n * halfgap_;
    // the end point gives a minimum length
    if (glm::distance(controls_[0], controls_[1]) < 0.02f)
        controls_[1] = controls_[0] + d * 0.02f;
}

void RiftAutomaton::rebuild ()
{
    const glm::vec2 d = controls_[1] - controls_[0];
    axis_ = replaceLine(root_, axis_, { controls_[0] - d, controls_[1] }, AUTOMATON_LINE_WIDTH_GUIDE, AUTOMATON_ALPHA_GUIDE);
    axis_->translation_.z = 0.02f;
}

void RiftAutomaton::motion ()
{
    gap_ = halfgap_ * wave(float(progress_), shape_);
    const glm::vec2 d = controls_[1] - controls_[0];
    const glm::vec2 u = direction();
    const glm::vec2 n = glm::vec2(-u.y, u.x) * std::max(gap_, 1e-4f);
    setLine(line_a_, { controls_[0] - d + n, controls_[1] + n });
    setLine(line_b_, { controls_[0] - d - n, controls_[1] - n });
    line_a_->shader()->color = color(AUTOMATON_ALPHA_MOTION);
    line_b_->shader()->color = color(AUTOMATON_ALPHA_MOTION);
    axis_->shader()->color = color(AUTOMATON_ALPHA_GUIDE);
}

glm::vec2 RiftAutomaton::push (glm::vec2 p) const
{
    const glm::vec2 u = direction();
    return pushRift(p, controls_[0], std::atan2(u.y, u.x),
                    glm::distance(controls_[0], controls_[1]), gap_);
}

float RiftAutomaton::curve (float u) const
{
    return wave(u, shape_);
}

std::vector<float> RiftAutomaton::events () const
{
    return { wavePeak(shape_) };
}
