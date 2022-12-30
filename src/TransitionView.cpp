/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2022 Bruno Herbelin <bruno.herbelin@gmail.com>
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

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/vector_angle.hpp>

#include "ImGuiToolkit.h"

#include <string>
#include <sstream>

#include "Mixer.h"
#include "defines.h"
#include "Settings.h"
#include "SessionSource.h"
#include "DrawVisitor.h"
#include "Decorations.h"
#include "UserInterfaceManager.h"
#include "Log.h"

#include "TransitionView.h"

#define POS_TARGET 0.4f


TransitionView::TransitionView() : View(TRANSITION), transition_source_(nullptr)
{
    // read default settings
    if ( Settings::application.views[mode_].name.empty() )
    {
        // no settings found: store application default
        scene.root()->scale_ = glm::vec3(TRANSITION_DEFAULT_SCALE, TRANSITION_DEFAULT_SCALE, 1.0f);
        scene.root()->translation_ = glm::vec3(1.5f, 0.f, 0.0f);
        saveSettings();
    }
    else
        restoreSettings();
    Settings::application.views[mode_].name = "Transition";

    // Geometry Scene background
    gradient_ = new Switch;
    gradient_->attach(new ImageSurface("images/gradient_0_cross_linear.png"));
    gradient_->attach(new ImageSurface("images/gradient_1_black_linear.png"));
    gradient_->attach(new ImageSurface("images/gradient_2_cross_quad.png"));
    gradient_->attach(new ImageSurface("images/gradient_3_black_quad.png"));
    gradient_->scale_ = glm::vec3(0.501f, 0.006f, 1.f);
    gradient_->translation_ = glm::vec3(-0.5f, -0.005f, -0.01f);
    scene.fg()->attach(gradient_);

    mark_1s_ = new Mesh("mesh/h_mark.ply");
    mark_1s_->translation_ = glm::vec3(-1.f, -0.01f, 0.0f);
    mark_1s_->shader()->color = glm::vec4( COLOR_TRANSITION_LINES, 0.9f );
    scene.fg()->attach(mark_1s_);

    mark_100ms_ = new Mesh("mesh/h_mark.ply");
    mark_100ms_->translation_ = glm::vec3(-1.f, -0.01f, 0.0f);
    mark_100ms_->scale_ = glm::vec3(0.5f, 0.5f, 0.0f);
    mark_100ms_->shader()->color = glm::vec4( COLOR_TRANSITION_LINES, 0.9f );
    scene.fg()->attach(mark_100ms_);

    // move the whole forground below the icons
    scene.fg()->translation_ = glm::vec3(0.f, -0.11f, 0.0f);

    output_surface_ = new Surface;
    scene.bg()->attach(output_surface_);

    Frame *border = new Frame(Frame::ROUND, Frame::THIN, Frame::GLOW);
    border->color = glm::vec4( COLOR_FRAME, 1.0f );
    scene.bg()->attach(border);

    scene.bg()->scale_ = glm::vec3(0.1f, 0.1f, 1.f);
    scene.bg()->translation_ = glm::vec3(POS_TARGET, 0.f, 0.0f);

}

void TransitionView::update(float dt)
{
    // update scene
    View::update(dt);

    // a more complete update is requested
    if  (View::need_deep_update_ > 0) {

        // update rendering of render frame
        FrameBuffer *output = Mixer::manager().session()->frame();
        if (output){
            float aspect_ratio = output->aspectRatio();
            for (NodeSet::iterator node = scene.bg()->begin(); node != scene.bg()->end(); ++node) {
                (*node)->scale_.x = aspect_ratio;
            }
            output_surface_->setTextureIndex( output->texture() );
        }

    }

    // Update transition source
    if ( transition_source_ != nullptr) {

        float d = transition_source_->group(View::TRANSITION)->translation_.x;

        // Transfer this movement to changes in mixing
        // cross fading
        if ( Settings::application.transition.cross_fade )
        {
            float f = 0.f;
            // change alpha of session:
            if (Settings::application.transition.profile == 0)
                // linear => identical coordinates in Mixing View
                f = d;
            else {
                // quadratic => square coordinates in Mixing View
                f = (d+1.f)*(d+1.f) -1.f;
            }
            transition_source_->group(View::MIXING)->translation_.x = CLAMP(f, -1.f, 0.f);
            transition_source_->group(View::MIXING)->translation_.y = 0.f;

            // no fading when cross fading
            Mixer::manager().session()->setFadingTarget( 0.f );
        }
        // fade to black
        else
        {
            // change alpha of session ; hidden before -0.5, visible after
            transition_source_->group(View::MIXING)->translation_.x = d < -0.5f ? -1.f : 0.f;
            transition_source_->group(View::MIXING)->translation_.y = 0.f;

            // fade to black at 50% : fade-out [-1.0 -0.5], fade-in [-0.5 0.0]
            float f = 0.f;
            if (Settings::application.transition.profile == 0)
                f = ABS(2.f * d + 1.f);  // linear
            else {
                f = ( 2.f * d + 1.f);  // quadratic
                f *= f;
            }
            Mixer::manager().session()->setFadingTarget( 1.f - f );
        }

        // request update
        transition_source_->touch();

        if (d > 0.2f) {
            Mixer::manager().setView(View::MIXING);
            WorkspaceWindow::restoreWorkspace();
        }

    }


}


void TransitionView::draw()
{
    // update the GUI depending on changes in settings
    gradient_->setActive( 2*Settings::application.transition.profile + (Settings::application.transition.cross_fade ? 0 : 1) );

    // draw scene of this view
    View::draw();

    // 100ms tic marks
    int n = static_cast<int>( Settings::application.transition.duration / 0.1f );
    glm::mat4 T = glm::translate(glm::identity<glm::mat4>(), glm::vec3( 1.f / n, 0.f, 0.f));
    DrawVisitor dv(mark_100ms_, Rendering::manager().Projection());
    dv.loop(n+1, T);
    scene.accept(dv);

    // 1s tic marks
    int N = static_cast<int>( Settings::application.transition.duration );
    T = glm::translate(glm::identity<glm::mat4>(), glm::vec3( 10.f / n, 0.f, 0.f));
    DrawVisitor dv2(mark_1s_, Rendering::manager().Projection());
    dv2.loop(N+1, T);
    scene.accept(dv2);

    // Display GUI if there is a transition source...
    if ( transition_source_ ) {

        // ..and a valid TRANSITION group
        const Group *tg = transition_source_->group(View::TRANSITION);
        if (tg) {

            // display interface below timeline
            glm::vec2 pos_window = Rendering::manager().project(glm::vec3(-1.2f, -0.10f, 0.f), scene.root()->transform_, false);
            ImGui::SetNextWindowPos(ImVec2(pos_window.x, pos_window.y), ImGuiCond_Always);
            if (ImGui::Begin("##Transition", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground
                             | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings
                             | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus))
            {
                const glm::vec2 pos_canl = Rendering::manager().project(glm::vec3(-1.0f, -0.15f, 0.f), scene.root()->transform_, false);
                const glm::vec2 pos_tran = Rendering::manager().project(glm::vec3(-0.5f, -0.15f, 0.f), scene.root()->transform_, false);
                const glm::vec2 pos_play = Rendering::manager().project(glm::vec3(0.f, -0.15f, 0.f), scene.root()->transform_, false);
                const glm::vec2 pos_open = Rendering::manager().project(glm::vec3(POS_TARGET, -0.15f, 0.f), scene.root()->transform_, false);

                // style grey
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.27f, 0.27f, 0.27f, 0.55f));
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.27f, 0.27f, 0.27f, 0.79f));
                ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.27f, 0.27f, 0.27f, 0.7f));
                ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.15f, 0.15f, 0.15f, 1.00f));
                ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.10f, 0.10f, 0.10f, 1.00f));
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.27f, 0.27f, 0.27f, 0.55f)); // 7 colors
                ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);

                // "Cancel" button at the begining of the timeline, only if transition not started
                if (tg->translation_.x < -1.f + EPSILON) {
                    ImGui::SetCursorScreenPos(ImVec2(pos_canl.x -80.f, pos_canl.y +2.f));
                    ImGui::SetNextItemWidth(160.f);
                    if ( ImGui::Button(ICON_FA_TIMES " Cancel") )
                        cancel();
                }

                // toggle transition mode
                if (!Settings::application.transition.cross_fade) {
                    // black background in icon 'transition to black'
                    ImGui::SetCursorScreenPos(ImVec2(pos_tran.x - 20.f, pos_tran.y +2.f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.f));
                    ImGuiToolkit::Icon(19,1);
                    ImGui::PopStyleColor();
                }
                ImGui::SetCursorScreenPos(ImVec2(pos_tran.x - 20.f, pos_tran.y +2.f));
                const char *tooltip[2] = {"Transition to black", "Cross fading"};
                ImGuiToolkit::IconToggle(0,2,0,8, &Settings::application.transition.cross_fade, tooltip );


                //  Duration slider (adjusted width)
                const float width = (pos_play.x - pos_canl.x) / 5.0;
                ImGui::SetCursorScreenPos(ImVec2(pos_play.x - width, pos_play.y +2.f));
                ImGui::SetNextItemWidth( width );
                ImGui::SliderFloat("##transitionduration", &Settings::application.transition.duration,
                                   TRANSITION_MIN_DURATION, TRANSITION_MAX_DURATION, "%.1f s");

                // Play-Pause button just at the end of the timeline
                ImGui::SetCursorScreenPos(ImVec2(pos_play.x + 10.f, pos_play.y +2.f));
                if ( ImGui::Button( tg->update_callbacks_.empty() ? ICON_FA_PLAY : ICON_FA_PAUSE) )
                    // toggle play but do not open
                    play(false);

                // "Open" button on the target frame
                ImGui::SetCursorScreenPos(ImVec2(pos_open.x -80.f, pos_open.y +2.f));
                ImGui::SetNextItemWidth(160.f);
                if ( ImGui::Button(ICON_FA_FILE_UPLOAD " Open") )
                    open();

                ImGui::PopFont();
                ImGui::PopStyleColor(7);  // 7 colors
                ImGui::End();
            }
        }
    }
}

bool TransitionView::canSelect(Source *s) {

    return ( s!=nullptr && s == transition_source_);
}

void TransitionView::attach(SessionFileSource *ts)
{
    // store source for later (detatch & interaction)
    transition_source_ = ts;

    if ( transition_source_ != nullptr) {
        // insert in scene
        Group *tg = transition_source_->group(View::TRANSITION);
        tg->visible_ = true;
        scene.ws()->attach(tg);

        // in fade to black transition, start transition from current fading value
        if ( !Settings::application.transition.cross_fade) {

            // reverse calculate x position to match actual fading of session
            float d = 0.f;
            if (Settings::application.transition.profile == 0)
                d = -1.f + 0.5f * Mixer::manager().session()->fading();  // linear
            else {
                d = -1.f - 0.5f * ( sqrt(1.f - Mixer::manager().session()->fading()) - 1.f);  // quadratic
            }

            transition_source_->group(View::TRANSITION)->translation_.x = d;
        }
    }
}

Session *TransitionView::detach()
{
    // by default, nothing to return
    Session *ret = nullptr;

    if ( transition_source_ != nullptr) {

        // get and detatch the group node from the view workspace
        Group *tg = transition_source_->group(View::TRANSITION);
        scene.ws()->detach( tg );


        // test if the icon of the transition source is "Ready"
        if ( tg->translation_.x > 0.f )
            // detatch the session and return it
            ret = transition_source_->detach();
        else
            // not detached: make current
            Mixer::manager().setCurrentSource(transition_source_);

        // done with transition
        transition_source_ = nullptr;
    }

    return ret;
}

void TransitionView::zoom (float factor)
{
    if (transition_source_ != nullptr) {
        float d = transition_source_->group(View::TRANSITION)->translation_.x;
        d += 0.1f * factor;
        transition_source_->group(View::TRANSITION)->translation_.x = CLAMP(d, -1.f, 0.f);
    }
}

std::pair<Node *, glm::vec2> TransitionView::pick(glm::vec2 P)
{
    std::pair<Node *, glm::vec2> pick = View::pick(P);

    if (transition_source_ != nullptr) {
        // start animation when clic on target
        if (pick.first == output_surface_)
            play(true);
        // otherwise cancel animation
        else
            transition_source_->group(View::TRANSITION)->clearCallbacks();
    }

    return pick;
}


void TransitionView::open()
{
    if (transition_source_ != nullptr) {
        // quick jump over to target (and open)
        transition_source_->group(View::TRANSITION)->clearCallbacks();
        MoveToCallback *anim = new MoveToCallback(glm::vec3(POS_TARGET, 0.0, 0.0), 180.f);
        transition_source_->group(View::TRANSITION)->update_callbacks_.push_back(anim);
    }
}

void TransitionView::cancel()
{
    if (transition_source_ != nullptr) {
        // cancel any animation
        transition_source_->group(View::TRANSITION)->clearCallbacks();
        // get and detatch the group node from the view workspace
        scene.ws()->detach( transition_source_->group(View::TRANSITION) );

        // done with transition source
        Mixer::manager().deleteSource(transition_source_);
        transition_source_ = nullptr;
        // quit view
        Mixer::manager().setView(View::MIXING);
    }
}

void TransitionView::play(bool open)
{
    if (transition_source_ != nullptr) {

        // toggle play/stop with button
        if (!transition_source_->group(View::TRANSITION)->update_callbacks_.empty() && !open) {
            // just cancel previous animation
            transition_source_->group(View::TRANSITION)->clearCallbacks();
            return;
        }
        // else cancel previous animation and start new one
        transition_source_->group(View::TRANSITION)->clearCallbacks();

        // if want to open session after play, target  movement till end position, otherwise stop at 0
        float target_x = open ? POS_TARGET : 0.f;

        // calculate how far to reach target
        float time = CLAMP(- transition_source_->group(View::TRANSITION)->translation_.x, 0.f, 1.f);
        // extra distance to reach transition if want to open
        time += open ? 0.2f : 0.f;
        // calculate remaining time on the total duration, in ms
        time *= Settings::application.transition.duration  * 1000.f;

        // if remaining time is more than 50ms
        if (time > 50.f) {
            // start animation
            MoveToCallback *anim = new MoveToCallback(glm::vec3(target_x, 0.0, 0.0), time);
            transition_source_->group(View::TRANSITION)->update_callbacks_.push_back(anim);
        }
        // otherwise finish animation
        else
            transition_source_->group(View::TRANSITION)->translation_.x = target_x;
    }
}

View::Cursor TransitionView::grab (Source *s, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2>)
{
    if (!s)
        return Cursor();

    // unproject
    glm::vec3 gl_Position_from = Rendering::manager().unProject(from, scene.root()->transform_);
    glm::vec3 gl_Position_to   = Rendering::manager().unProject(to, scene.root()->transform_);

    // compute delta translation
    float d = s->stored_status_->translation_.x + gl_Position_to.x - gl_Position_from.x;
    std::ostringstream info;
    if (d > 0.2) {
        s->group(mode_)->translation_.x = 0.4;
        info << "Open session";
    }
    else {
        s->group(mode_)->translation_.x = CLAMP(d, -1.f, 0.f);
        info << "Transition " <<  int( 100.f * (1.f + s->group(View::TRANSITION)->translation_.x)) << "%";
    }

    return Cursor(Cursor_ResizeEW, info.str() );
}

bool TransitionView::doubleclic (glm::vec2 )
{
    Mixer::manager().setView(View::MIXING);
    return true;
}

void TransitionView::arrow (glm::vec2 movement)
{
    Source *s = Mixer::manager().currentSource();
    if (s) {

        glm::vec3 gl_Position_from = Rendering::manager().unProject(glm::vec2(0.f), scene.root()->transform_);
        glm::vec3 gl_Position_to   = Rendering::manager().unProject(movement, scene.root()->transform_);
        glm::vec3 gl_delta = gl_Position_to - gl_Position_from;

        float d = s->group(mode_)->translation_.x + gl_delta.x * ARROWS_MOVEMENT_FACTOR * dt_;
        s->group(mode_)->translation_.x = CLAMP(d, -1.f, 0.f);

        // request update
        s->touch();
    }
}

View::Cursor TransitionView::drag (glm::vec2 from, glm::vec2 to)
{
    Cursor ret = View::drag(from, to);

    // Clamp translation to acceptable area
    scene.root()->translation_ = glm::clamp(scene.root()->translation_, glm::vec3(1.f, -1.7f, 0.f), glm::vec3(2.f, 1.7f, 0.f));

    return ret;
}
