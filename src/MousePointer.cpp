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

#include <glm/gtc/random.hpp> // for diskRand

#include "imgui.h"

#include "Metronome.h"
#include "MousePointer.h"

std::vector< std::tuple<int, int, std::string> > Pointer::Modes = {
    { ICON_POINTER_DEFAULT, std::string("Default") },
    { ICON_POINTER_LINEAR,  std::string("Linear") },
    { ICON_POINTER_SPRING,  std::string("Spring") },
    { ICON_POINTER_WIGGLY,  std::string("Wiggly") },
    { ICON_POINTER_METRONOME, std::string("Metronome") }
};

#define POINTER_LINEAR_MIN_SPEED 40.f
#define POINTER_LINEAR_MAX_SPEED 800.f
#define POINTER_LINEAR_THICKNESS 4.f
#define POINTER_LINEAR_ARROW 40.f

void PointerLinear::update(glm::vec2 pos, float dt)
{
    float speed = POINTER_LINEAR_MIN_SPEED + (POINTER_LINEAR_MAX_SPEED - POINTER_LINEAR_MIN_SPEED) * strength_;

    glm::vec2 delta = pos - pos_ ;
    if (glm::length(delta) > 10.f )
        pos_ += glm::normalize(delta) * (speed * dt);
}

void PointerLinear::draw()
{
    ImGuiIO& io = ImGui::GetIO();
    const glm::vec2 start = glm::vec2( io.MousePos.x, io.MousePos.y);
    const glm::vec2 end   = glm::vec2( pos_.x / io.DisplayFramebufferScale.x, pos_.y / io.DisplayFramebufferScale.y );
    const ImVec2 _end = ImVec2( end.x, end.y );

    // draw line
    ImGui::GetBackgroundDrawList()->AddLine(io.MousePos, _end, ImGui::GetColorU32(ImGuiCol_HeaderActive), POINTER_LINEAR_THICKNESS);
    ImGui::GetBackgroundDrawList()->AddCircleFilled(_end, 6.0, ImGui::GetColorU32(ImGuiCol_HeaderActive));

    // direction vector
    glm::vec2 delta = start - end;
    float l = glm::length(delta);
    delta = glm::normalize( delta );

    // draw dots regularly to show speed
    for (float p = 0.f; p < l; p += 200.f * (strength_ + 0.1f)) {
        glm::vec2 point = start - delta * p;
        ImGui::GetBackgroundDrawList()->AddCircleFilled(ImVec2(point.x,point.y), 4.0, ImGui::GetColorU32(ImGuiCol_HeaderActive));
    }

    // draw arrow head
    if ( l > POINTER_LINEAR_ARROW * 1.5f) {
        glm::vec2 ortho = glm::normalize( glm::vec2( glm::cross( glm::vec3(delta, 0.f), glm::vec3(0.f, 0.f, 1.f)) ));
        ortho *= POINTER_LINEAR_ARROW;
        delta *= POINTER_LINEAR_ARROW;
        const glm::vec2 pointA = start - delta + ortho * 0.5f;
        const glm::vec2 pointB = start - delta - ortho * 0.5f;

        ImGui::GetBackgroundDrawList()->AddTriangleFilled(io.MousePos, ImVec2(pointA.x, pointA.y),
                                                          ImVec2(pointB.x, pointB.y), ImGui::GetColorU32(ImGuiCol_HeaderActive));
    }
}

#define POINTER_WIGGLY_MIN_RADIUS 30.f
#define POINTER_WIGGLY_MAX_RADIUS 300.f
#define POINTER_WIGGLY_SMOOTHING 10

void PointerWiggly::update(glm::vec2 pos, float)
{
    float radius = POINTER_WIGGLY_MIN_RADIUS + (POINTER_WIGGLY_MAX_RADIUS - POINTER_WIGGLY_MIN_RADIUS) * strength_;

    // change pos to a random point in a close radius
    pos += glm::diskRand( radius );

    // smooth a little and apply
    const float emaexp = 2.0 / float( POINTER_WIGGLY_SMOOTHING + 1);
    pos_ = emaexp * pos + (1.f - emaexp) * pos_;
}

void PointerWiggly::draw()
{
    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 _end = ImVec2( pos_.x / io.DisplayFramebufferScale.x, pos_.y / io.DisplayFramebufferScale.y );
    ImGui::GetBackgroundDrawList()->AddLine(io.MousePos, _end, ImGui::GetColorU32(ImGuiCol_HeaderActive), 5.f);

    const float radius = POINTER_WIGGLY_MIN_RADIUS + (POINTER_WIGGLY_MAX_RADIUS - POINTER_WIGGLY_MIN_RADIUS) * strength_;
    ImGui::GetBackgroundDrawList()->AddCircle(io.MousePos, radius * 0.5f,
                                              ImGui::GetColorU32(ImGuiCol_HeaderActive), 0, 2.f + 4.f * strength_);
}

#define POINTER_METRONOME_RADIUS 30.f

void PointerMetronome::update(glm::vec2 pos, float dt)
{
    if ( Metronome::manager().timeToBeat() < std::chrono::milliseconds( (uint)floor(dt * 1000.f) )) {
        pos_ = pos;
    }
}

void PointerMetronome::draw()
{
    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 end = ImVec2(pos_.x / io.DisplayFramebufferScale.x, pos_.y / io.DisplayFramebufferScale.y);
    ImGui::GetBackgroundDrawList()->AddLine(io.MousePos, end, ImGui::GetColorU32(ImGuiCol_HeaderActive), 4.f);
    ImGui::GetBackgroundDrawList()->AddCircle(io.MousePos, POINTER_METRONOME_RADIUS, ImGui::GetColorU32(ImGuiCol_HeaderActive), 0, 3.f);
    ImGui::GetBackgroundDrawList()->AddCircleFilled(end, 6.0, ImGui::GetColorU32(ImGuiCol_HeaderActive));

    double t = Metronome::manager().phase();
    t -= floor(t);
    ImGui::GetBackgroundDrawList()->AddCircleFilled(io.MousePos, t * POINTER_METRONOME_RADIUS, ImGui::GetColorU32(ImGuiCol_HeaderActive), 0);
}

#define POINTER_SPRING_MIN_MASS 6.f
#define POINTER_SPRING_MAX_MASS 40.f

void PointerSpring::initiate(glm::vec2 pos)
{
    Pointer::initiate(pos);
    velocity_ = glm::vec2(0.f);
}

void PointerSpring::update(glm::vec2 pos, float dt)
{
    // percentage of loss of energy at every update
    const float viscousness = 0.75;
    // force applied on the mass, as percent of the Maximum mass
    const float stiffness = 0.8;
    // damping : opposite direction of force, non proportional to mass
    const float damping = 60.0;
    // mass as a percentage of min to max
    const float mass = POINTER_SPRING_MAX_MASS - (POINTER_SPRING_MAX_MASS - POINTER_SPRING_MIN_MASS) * strength_;

    // compute delta betwen initial and current position
    glm::vec2 delta = pos - pos_;
    // apply force on velocity : spring stiffness / mass
    velocity_ += delta * ( (POINTER_SPRING_MAX_MASS * stiffness) / mass );
    // apply damping dynamics
    velocity_ -= damping * dt * glm::normalize(delta);
    // compute new position : add velocity x time
    pos_ += dt * velocity_;
    // diminish velocity by viscousness of substrate
    // (loss of energy between updates)
    velocity_ *= viscousness;
}

void PointerSpring::draw()
{
    ImGuiIO& io = ImGui::GetIO();
    glm::vec2 _start = glm::vec2( io.MousePos.x * io.DisplayFramebufferScale.x, io.MousePos.y * io.DisplayFramebufferScale.y );

    const glm::vec2 delta = pos_ - _start;
    glm::vec2 ortho = glm::normalize( glm::vec2( glm::cross( glm::vec3(delta, 0.f), glm::vec3(0.f, 0.f, 1.f)) ));
    ortho *= 0.05f * glm::length( velocity_ );

    // draw a wave with 3 bezier
    glm::vec2 _third = _start + delta * 1.f / 9.f + ortho;
    glm::vec2 _twothird = _start + delta * 2.f / 9.f - ortho;
    glm::vec2 _end = _start + delta * 3.f / 9.f;

    ImVec2 start = ImVec2(_start.x / io.DisplayFramebufferScale.x, _start.y / io.DisplayFramebufferScale.y);
    ImVec2 third = ImVec2(_third.x / io.DisplayFramebufferScale.x, _third.y / io.DisplayFramebufferScale.y);
    ImVec2 twothird = ImVec2(_twothird.x / io.DisplayFramebufferScale.x, _twothird.y / io.DisplayFramebufferScale.y);
    ImVec2 end = ImVec2(_end.x / io.DisplayFramebufferScale.x, _end.y / io.DisplayFramebufferScale.y);
    ImGui::GetBackgroundDrawList()->AddBezierCurve(start, third, twothird, end,
                                                   ImGui::GetColorU32(ImGuiCol_HeaderActive), 5.f);
    _start = _end;
    _third = _start + delta  * 1.f / 9.f + ortho;
    _twothird = _start + delta * 2.f / 9.f - ortho;
    _end = _start + delta * 3.f / 9.f;

    start = ImVec2(_start.x / io.DisplayFramebufferScale.x, _start.y / io.DisplayFramebufferScale.y);
    third = ImVec2(_third.x / io.DisplayFramebufferScale.x, _third.y / io.DisplayFramebufferScale.y);
    twothird = ImVec2(_twothird.x / io.DisplayFramebufferScale.x, _twothird.y / io.DisplayFramebufferScale.y);
    end = ImVec2(_end.x / io.DisplayFramebufferScale.x, _end.y / io.DisplayFramebufferScale.y);
    ImGui::GetBackgroundDrawList()->AddBezierCurve(start, third, twothird, end,
                                                   ImGui::GetColorU32(ImGuiCol_HeaderActive), 5.f);
    _start = _end;
    _third = _start + delta  * 1.f / 9.f + ortho;
    _twothird = _start + delta * 2.f / 9.f - ortho;

    start = ImVec2(_start.x / io.DisplayFramebufferScale.x, _start.y / io.DisplayFramebufferScale.y);
    third = ImVec2(_third.x / io.DisplayFramebufferScale.x, _third.y / io.DisplayFramebufferScale.y);
    twothird = ImVec2(_twothird.x / io.DisplayFramebufferScale.x, _twothird.y / io.DisplayFramebufferScale.y);
    end = ImVec2(pos_.x / io.DisplayFramebufferScale.x, pos_.y / io.DisplayFramebufferScale.y);
    ImGui::GetBackgroundDrawList()->AddBezierCurve(start, third, twothird, end,
                                                   ImGui::GetColorU32(ImGuiCol_HeaderActive), 5.f);

    // represent the weight with a filled circle
    float mass = POINTER_SPRING_MAX_MASS - (POINTER_SPRING_MAX_MASS - POINTER_SPRING_MIN_MASS) * strength_;
    ImGui::GetBackgroundDrawList()->AddCircleFilled(end, mass, ImGui::GetColorU32(ImGuiCol_HeaderActive));
}


MousePointer::MousePointer() : mode_(Pointer::POINTER_DEFAULT), active_(nullptr)
{
    active_ = new Pointer;
}

void MousePointer::setActiveMode(Pointer::Mode m)
{
    if (mode_ != m) {

        mode_ = m;

        if (active_)
            delete active_;

        switch (mode_) {
        case Pointer::POINTER_SPRING:
            active_ = new PointerSpring;
            break;
        case Pointer::POINTER_METRONOME:
            active_ = new PointerMetronome;
            break;
        case Pointer::POINTER_LINEAR:
            active_ = new PointerLinear;
            break;
        case Pointer::POINTER_WIGGLY:
            active_ = new PointerWiggly;
            break;
        default:
        case Pointer::POINTER_DEFAULT:
            active_ = new Pointer;
            break;
        }
    }
}
