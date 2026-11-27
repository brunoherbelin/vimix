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

#include <cmath>
#include <algorithm>

#include "AutomatonMath.h"

namespace AutomatonMath {

static const float PI_F = 3.14159265358979323846f;
static const double MIN_PERIOD = 1.0 / 64.0;

double cycle (double beats, double period, double offset)
{
    const double p = std::max(period, MIN_PERIOD);
    return std::floor( (beats - offset) / p );
}

double progress (double beats, double period, double offset)
{
    const double p = std::max(period, MIN_PERIOD);
    const double c = (beats - offset) / p;
    double u = c - std::floor(c);
    // guard against rounding to 1.0
    if (u >= 1.0 || u < 0.0)
        u = 0.0;
    return u;
}

float ramp (float u, int shape)
{
    u = glm::clamp(u, 0.f, 1.f);
    switch (shape) {
    case SHAPE_SMOOTH:
        return 0.5f - 0.5f * std::cos(PI_F * u);
    case SHAPE_PULSE:
        return 1.f - (1.f - u) * (1.f - u) * (1.f - u);
    default:
        return u;
    }
}

float wavePeak (int shape)
{
    return shape == SHAPE_PULSE ? AUTOMATON_PULSE_ATTACK : 0.5f;
}

float wave (float u, int shape)
{
    u = glm::clamp(u, 0.f, 1.f);
    switch (shape) {
    case SHAPE_SMOOTH:
        return 0.5f - 0.5f * std::cos(2.f * PI_F * u);
    case SHAPE_PULSE:
        if (u < AUTOMATON_PULSE_ATTACK)
            return std::sin(0.5f * PI_F * u / AUTOMATON_PULSE_ATTACK);
        return 0.5f + 0.5f * std::cos(PI_F * (u - AUTOMATON_PULSE_ATTACK) / (1.f - AUTOMATON_PULSE_ATTACK));
    default:
        return u < 0.5f ? 2.f * u : 2.f * (1.f - u);
    }
}

glm::vec2 ellipse (glm::vec2 center, glm::vec2 radius, float turn)
{
    const float a = 2.f * PI_F * turn;
    return center + glm::vec2(radius.x * std::cos(a), radius.y * std::sin(a));
}

glm::vec2 ellipse (glm::vec2 center, glm::vec2 axis_x, glm::vec2 axis_y, float turn)
{
    const float a = 2.f * PI_F * turn;
    return center + axis_x * std::cos(a) + axis_y * std::sin(a);
}

// centripetal Catmull-Rom segment between P1 and P2
static glm::vec2 catmullRom (glm::vec2 P0, glm::vec2 P1, glm::vec2 P2, glm::vec2 P3, float t)
{
    auto knot = [](float ti, glm::vec2 a, glm::vec2 b) {
        // alpha 0.5 : centripetal (no loop, no cusp)
        return ti + std::sqrt( std::max(glm::length(b - a), 1e-6f) );
    };
    const float t0 = 0.f;
    const float t1 = knot(t0, P0, P1);
    const float t2 = knot(t1, P1, P2);
    const float t3 = knot(t2, P2, P3);
    const float tt = t1 + t * (t2 - t1);

    const glm::vec2 A1 = (t1 - tt) / (t1 - t0) * P0 + (tt - t0) / (t1 - t0) * P1;
    const glm::vec2 A2 = (t2 - tt) / (t2 - t1) * P1 + (tt - t1) / (t2 - t1) * P2;
    const glm::vec2 A3 = (t3 - tt) / (t3 - t2) * P2 + (tt - t2) / (t3 - t2) * P3;
    const glm::vec2 B1 = (t2 - tt) / (t2 - t0) * A1 + (tt - t0) / (t2 - t0) * A2;
    const glm::vec2 B2 = (t3 - tt) / (t3 - t1) * A2 + (tt - t1) / (t3 - t1) * A3;
    return (t2 - tt) / (t2 - t1) * B1 + (tt - t1) / (t2 - t1) * B2;
}

std::vector<glm::vec2> smoothPath (const std::vector<glm::vec2> &keys, size_t samples,
                                   std::vector<size_t> *key_indices)
{
    std::vector<glm::vec2> path;
    if (key_indices)
        key_indices->clear();
    if (keys.empty() || samples < 2)
        return path;

    const size_t K = keys.size();
    path.resize(samples, keys.front());

    if (K < 2) {
        if (key_indices)
            key_indices->push_back(0);
        return path;
    }

    // every segment between keys gets (about) the same number of samples
    const size_t S = K - 1;
    for (size_t i = 0; i < samples; ++i) {
        // global parameter in [0 S]
        float g = float(i) * float(S) / float(samples - 1);
        size_t s = std::min( size_t(g), S - 1 );
        float t = g - float(s);
        // control points, with extrapolation at extremities
        const glm::vec2 P1 = keys[s];
        const glm::vec2 P2 = keys[s + 1];
        const glm::vec2 P0 = s > 0 ? keys[s - 1] : P1 - (P2 - P1);
        const glm::vec2 P3 = s + 2 < K ? keys[s + 2] : P2 + (P2 - P1);
        path[i] = catmullRom(P0, P1, P2, P3, t);
    }
    // exact extremities
    path.front() = keys.front();
    path.back() = keys.back();

    if (key_indices) {
        for (size_t k = 0; k < K; ++k)
            key_indices->push_back( size_t( std::lround( float(k) * float(samples - 1) / float(S) ) ) );
    }

    return path;
}

void ArcPath::set (const std::vector<glm::vec2> &points)
{
    points_ = points;
    cumulated_.assign(points_.size(), 0.f);
    length_ = 0.f;
    for (size_t i = 1; i < points_.size(); ++i) {
        length_ += glm::length(points_[i] - points_[i-1]);
        cumulated_[i] = length_;
    }
}

glm::vec2 ArcPath::at (float t) const
{
    if (points_.empty())
        return glm::vec2(0.f);
    if (points_.size() < 2 || length_ < 1e-9f)
        return points_.front();

    const float d = glm::clamp(t, 0.f, 1.f) * length_;
    // first point with cumulated length above d
    auto it = std::upper_bound(cumulated_.begin(), cumulated_.end(), d);
    if (it == cumulated_.end())
        return points_.back();
    const size_t i = std::max<size_t>(1, size_t(it - cumulated_.begin()));
    const float seg = cumulated_[i] - cumulated_[i-1];
    const float f = seg > 1e-9f ? (d - cumulated_[i-1]) / seg : 0.f;
    return glm::mix(points_[i-1], points_[i], f);
}

float ArcPath::fraction (size_t index) const
{
    if (index >= cumulated_.size() || length_ < 1e-9f)
        return 0.f;
    return cumulated_[index] / length_;
}

glm::vec2 pushRadial (glm::vec2 p, glm::vec2 center, float radius)
{
    if (radius <= 0.f)
        return p;
    const glm::vec2 v = p - center;
    const float d = glm::length(v);
    if (d >= radius)
        return p;
    if (d < 1e-6f)
        return center + glm::vec2(0.f, radius);
    return center + v * (radius / d);
}

glm::vec2 pushRift (glm::vec2 p, glm::vec2 center, float angle, float halflength, float halfgap)
{
    if (halfgap <= 0.f)
        return p;
    const glm::vec2 dir(std::cos(angle), std::sin(angle));
    const glm::vec2 normal(-dir.y, dir.x);
    const glm::vec2 v = p - center;
    const float along = glm::dot(v, dir);
    // outside of the segment : not affected
    if (std::abs(along) > halflength)
        return p;
    const float side = glm::dot(v, normal);
    if (std::abs(side) >= halfgap)
        return p;
    // place on the border, on its side
    const float target = side < 0.f ? -halfgap : halfgap;
    return p + normal * (target - side);
}

}
