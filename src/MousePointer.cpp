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

#include <glm/geometric.hpp>
#include <glm/gtc/random.hpp> // for diskRand

#include "imgui.h"

#include "Metronome.h"
#include "View/View.h"
#include "Mixer.h"
#include "TabletInput.h"

//#include "RenderingManager.h"
#include "MousePointer.h"

#define IMVEC_IO(v) ImVec2( v.x / ImGui::GetIO().DisplayFramebufferScale.x, v.y / ImGui::GetIO().DisplayFramebufferScale.y)

std::vector< std::tuple<int, int, std::string, std::string> > Pointer::Modes = {
    { ICON_POINTER_DEFAULT, "Default", "Default" },
    { ICON_POINTER_GRID,    "Grid",    "Step" },
    { ICON_POINTER_LINEAR,  "Line",    "Speed" },
    { ICON_POINTER_SPRING,  "Spring",  "Mass" },
    { ICON_POINTER_WIGGLY,  "Wiggly",  "Radius" },
    { ICON_POINTER_BROWNIAN,  "Brownian",  "Radius" },
    { ICON_POINTER_METRONOME, "Metronome", "Speed" }
};

void PointerGrid::initiate(const glm::vec2&)
{
    Mixer::manager().view()->grid->setUnit( (Grid::Units) round( 4.f * strength_ ) );
    Mixer::manager().view()->grid->setActive(true);
}

void PointerGrid::update(const glm::vec2 &pos, float dt)
{
    Pointer::update(pos, dt);
    Mixer::manager().view()->grid->setUnit( (Grid::Units) round( 4.f * strength_ ) );
}

void PointerGrid::terminate()
{
    Mixer::manager().view()->grid->setActive(false);
}


#define POINTER_LINEAR_MIN_SPEED 40.f
#define POINTER_LINEAR_MAX_SPEED 800.f
#define POINTER_LINEAR_THICKNESS 4.f
#define POINTER_LINEAR_ARROW 40.f

void PointerLinear::update(const glm::vec2 &pos, float dt)
{
    current_ = pos;

    float speed = POINTER_LINEAR_MIN_SPEED + (POINTER_LINEAR_MAX_SPEED - POINTER_LINEAR_MIN_SPEED) * strength_;

    glm::vec2 delta = current_ - target_ ;

    if (glm::length(delta) > 10.f )
        target_ += glm::normalize(delta) * (speed * glm::max(dt,0.001f) );
}

void PointerLinear::draw()
{
    const ImU32 color = ImGui::GetColorU32(ImGuiCol_HeaderActive);
    const ImVec2 _end = IMVEC_IO(target_);

    // draw line
    ImGui::GetBackgroundDrawList()->AddLine(IMVEC_IO(current_), _end, color, POINTER_LINEAR_THICKNESS);
    ImGui::GetBackgroundDrawList()->AddCircleFilled(_end, 6.0, color);

    // direction vector
    glm::vec2 delta = current_ - target_;
    float l = glm::length(delta);
    delta = glm::normalize( delta );

    // draw dots regularly to show speed
    for (float p = 0.f; p < l; p += 200.f * (strength_ + 0.1f)) {
        glm::vec2 point = current_ - delta * p;
        ImGui::GetBackgroundDrawList()->AddCircleFilled(IMVEC_IO(point), 4.0, color);
    }

    // draw arrow head
    if ( l > POINTER_LINEAR_ARROW * 1.5f) {
        glm::vec2 ortho = glm::normalize( glm::vec2( glm::cross( glm::vec3(delta, 0.f), glm::vec3(0.f, 0.f, 1.f)) ));
        ortho *= POINTER_LINEAR_ARROW;
        delta *= POINTER_LINEAR_ARROW;
        const glm::vec2 pointA = current_ - delta + ortho * 0.5f;
        const glm::vec2 pointB = current_ - delta - ortho * 0.5f;
        ImGui::GetBackgroundDrawList()->AddTriangleFilled(IMVEC_IO(current_), IMVEC_IO(pointA), IMVEC_IO(pointB), color);
    }
}

#define POINTER_WIGGLY_MIN_RADIUS 3.f
#define POINTER_WIGGLY_MAX_RADIUS 400.f
#define POINTER_WIGGLY_SMOOTHING 10

void PointerWiggly::update(const glm::vec2 &pos, float)
{
    current_ = pos;
    radius_ = (POINTER_WIGGLY_MAX_RADIUS - POINTER_WIGGLY_MIN_RADIUS) * strength_;
    if (TabletInput::instance().hasPressure() && TabletInput::instance().isPressed()) {
        radius_ *= TabletInput::instance().getPressure();
    }
    radius_ += POINTER_WIGGLY_MIN_RADIUS;

    // change pos to a random point in a close radius
    glm::vec2 p = pos + glm::diskRand( radius_ );

    // smooth a little and apply
    const float emaexp = 2.0 / float( POINTER_WIGGLY_SMOOTHING + 1);
    target_ = emaexp * p + (1.f - emaexp) * target_;
}

void PointerWiggly::draw()
{
    const ImU32 color = ImGui::GetColorU32(ImGuiCol_HeaderActive);
    ImGui::GetBackgroundDrawList()->AddLine(IMVEC_IO(current_), IMVEC_IO(target_), color, 5.f);

    const float max = POINTER_WIGGLY_MIN_RADIUS + (POINTER_WIGGLY_MAX_RADIUS - POINTER_WIGGLY_MIN_RADIUS) * strength_;
    if (TabletInput::instance().hasPressure() && TabletInput::instance().isPressed())
        ImGui::GetBackgroundDrawList()->AddCircle(IMVEC_IO(current_), radius_ * 0.5f, color, 0);
    ImGui::GetBackgroundDrawList()->AddCircle(IMVEC_IO(current_), max * 0.5f, color, 0, 2.f + 4.f * strength_);
}

void PointerBrownian::update(const glm::vec2 &pos, float)
{
    current_ = pos;
    radius_ = (POINTER_WIGGLY_MAX_RADIUS - POINTER_WIGGLY_MIN_RADIUS) * strength_;
    radius_ += POINTER_WIGGLY_MIN_RADIUS;

    // Brownian motion: add small random displacement in 2D
    // Generate random step using gaussian distribution for each axis
    glm::vec2 random_step = glm::gaussRand(glm::vec2(0.0f), glm::vec2(1.f) );

    // Scale by radius and apply damping to keep motion bounded
    float factor = 0.3f;    
    if (TabletInput::instance().hasPressure() && TabletInput::instance().isPressed()) {
        factor *= TabletInput::instance().getPressure();
    }
    float damping = 0.92f;    
    brownian_offset_ = brownian_offset_ * damping + random_step * radius_ * factor;

    // Clamp offset to stay within maximum radius
    float offset_length = glm::length(brownian_offset_);
    if (offset_length > radius_) {
        brownian_offset_ = brownian_offset_ * (radius_ / offset_length);
    }

    glm::vec2 p = pos + brownian_offset_;

    // smooth a little and apply
    const float emaexp = 2.0 / float( POINTER_WIGGLY_SMOOTHING + 1);
    target_ = emaexp * p + (1.f - emaexp) * target_;
}

void PointerBrownian::draw()
{
    const ImU32 color = ImGui::GetColorU32(ImGuiCol_HeaderActive);
    ImGui::GetBackgroundDrawList()->AddLine(IMVEC_IO(current_), IMVEC_IO(target_), color, 5.f);

    const float max = POINTER_WIGGLY_MIN_RADIUS + (POINTER_WIGGLY_MAX_RADIUS - POINTER_WIGGLY_MIN_RADIUS) * strength_;
    if (TabletInput::instance().hasPressure() && TabletInput::instance().isPressed())
        ImGui::GetBackgroundDrawList()->AddCircle(IMVEC_IO(current_), radius_ * 0.8f, color, 0);
    ImGui::GetBackgroundDrawList()->AddCircle(IMVEC_IO(current_), max * 0.8f, color, 0, 2.f + 4.f * strength_);
}


#define POINTER_METRONOME_RADIUS 36.f

void PointerMetronome::initiate(const glm::vec2 &pos)
{
    Pointer::initiate(pos);
    beat_pos_ = pos;
}

void PointerMetronome::update(const glm::vec2 &pos, float dt)
{
    current_ = pos;
    // aim for the position at the cursor at each beat
    if ( Metronome::manager().timeToBeat() < std::chrono::milliseconds( (uint)ceil(dt * 1000.f) ))
        beat_pos_ = pos;

    // calculate min jump ratio for current fps and current tempo
    // and considering it takes 10 frames to reach the beat_pos,
    float ratio = 10.f / ((60.f / Metronome::manager().tempo()) / glm::max(dt,0.001f));

    // animate the target cursor position to reach beat_pos_
    glm::vec2 delta = target_ - beat_pos_;
    target_ -= delta * (ratio + strength_ * (1.f-ratio) );
}

void PointerMetronome::draw()
{
    const ImU32 color = ImGui::GetColorU32(ImGuiCol_HeaderActive);
    ImGui::GetBackgroundDrawList()->AddLine(IMVEC_IO(current_), IMVEC_IO(target_), color, 4.f);
    ImGui::GetBackgroundDrawList()->AddCircle(IMVEC_IO(current_), POINTER_METRONOME_RADIUS, color, 0, 3.f);
    ImGui::GetBackgroundDrawList()->AddCircleFilled(IMVEC_IO(target_), 6.0, color);

    double t = Metronome::manager().beats();
    t -= floor(t);
    ImGui::GetBackgroundDrawList()->AddCircleFilled(IMVEC_IO(current_), t * POINTER_METRONOME_RADIUS, color, 0);
}

#define POINTER_SPRING_MIN_MASS 6.f
#define POINTER_SPRING_MAX_MASS 60.f

void PointerSpring::initiate(const glm::vec2 &pos)
{
    Pointer::initiate(pos);
    velocity_ = glm::vec2(0.f);
}

void PointerSpring::update(const glm::vec2 &pos, float dt)
{
    current_ = pos;

    // percentage of loss of energy at every update
    const float viscousness = 0.7;
    // force applied on the mass, as percent of the Maximum mass
    const float stiffness = 0.8;
    // damping : opposite direction of force, non proportional to mass
    const float damping = 60.0;
    // mass as a percentage of min to max
    mass_ = (POINTER_SPRING_MAX_MASS - POINTER_SPRING_MIN_MASS) * strength_;
    if (TabletInput::instance().hasPressure() && TabletInput::instance().isPressed()) {
        mass_ *= 1.f - TabletInput::instance().getPressure();
    }
    mass_ += POINTER_SPRING_MIN_MASS;

    // compute delta betwen initial and current position
    glm::vec2 delta = pos - target_;
    if ( glm::length(delta) > 0.0001f ) {
        // apply force on velocity : spring stiffness / mass
        velocity_ += delta * ( (POINTER_SPRING_MAX_MASS * stiffness) / mass_ );
        // apply damping dynamics
        velocity_ -= damping * glm::max(dt,0.001f) * glm::normalize(delta);
        // compute new position : add velocity x time
        target_ += glm::max(dt,0.001f) * velocity_;
        // diminish velocity by viscousness of substrate
        // (loss of energy between updates)
        velocity_ *= viscousness;
    }
}

void PointerSpring::draw()
{
    const ImU32 color = ImGui::GetColorU32(ImGuiCol_HeaderActive);
    const glm::vec2 delta = target_ - current_;

    glm::vec2 ortho = glm::normalize( glm::vec2( glm::cross( glm::vec3(delta, 0.f), glm::vec3(0.f, 0.f, 1.f)) ));
    ortho *= 0.05f * glm::length( velocity_ );

    // draw a wave with 3 bezier
    glm::vec2 _third = current_ + delta * 1.f / 9.f + ortho;
    glm::vec2 _twothird = current_ + delta * 2.f / 9.f - ortho;
    glm::vec2 _end = current_ + delta * 3.f / 9.f;
    ImGui::GetBackgroundDrawList()->AddBezierCurve(IMVEC_IO(current_), IMVEC_IO(_third), IMVEC_IO(_twothird), IMVEC_IO(_end), color, 5.f);

    current_ = _end;
    _third = current_ + delta  * 1.f / 9.f + ortho;
    _twothird = current_ + delta * 2.f / 9.f - ortho;
    _end = current_ + delta * 3.f / 9.f;
    ImGui::GetBackgroundDrawList()->AddBezierCurve(IMVEC_IO(current_), IMVEC_IO(_third), IMVEC_IO(_twothird), IMVEC_IO(_end), color, 5.f);

    current_ = _end;
    _third = current_ + delta  * 1.f / 9.f + ortho;
    _twothird = current_ + delta * 2.f / 9.f - ortho;
    _end = target_;
    ImGui::GetBackgroundDrawList()->AddBezierCurve(IMVEC_IO(current_), IMVEC_IO(_third), IMVEC_IO(_twothird), IMVEC_IO(_end), color, 5.f);

    // represent the weight with a filled circle
    const float max = POINTER_SPRING_MIN_MASS + (POINTER_SPRING_MAX_MASS - POINTER_SPRING_MIN_MASS) * strength_;
    if (TabletInput::instance().hasPressure())
        ImGui::GetBackgroundDrawList()->AddCircle(IMVEC_IO(_end), max, color, 0);
    ImGui::GetBackgroundDrawList()->AddCircleFilled(IMVEC_IO(_end), mass_, color, 0);
}


MousePointer::MousePointer() : mode_(Pointer::POINTER_DEFAULT)
{
    pointer_[Pointer::POINTER_DEFAULT] = new Pointer;
    pointer_[Pointer::POINTER_GRID]    = new PointerGrid;
    pointer_[Pointer::POINTER_LINEAR]  = new PointerLinear;
    pointer_[Pointer::POINTER_SPRING]  = new PointerSpring;
    pointer_[Pointer::POINTER_WIGGLY]  = new PointerWiggly;
    pointer_[Pointer::POINTER_BROWNIAN]  = new PointerBrownian;
    pointer_[Pointer::POINTER_METRONOME] = new PointerMetronome;
}

MousePointer::~MousePointer()
{
    delete pointer_[Pointer::POINTER_DEFAULT];
    delete pointer_[Pointer::POINTER_GRID];
    delete pointer_[Pointer::POINTER_LINEAR];
    delete pointer_[Pointer::POINTER_SPRING];
    delete pointer_[Pointer::POINTER_WIGGLY];
    delete pointer_[Pointer::POINTER_BROWNIAN];
    delete pointer_[Pointer::POINTER_METRONOME];
}
