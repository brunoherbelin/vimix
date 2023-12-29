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
#include <sstream>
#include <iomanip>

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/vector_angle.hpp>

#include "ImGuiToolkit.h"

#include "Mixer.h"
#include "defines.h"
#include "Source.h"
#include "SourceCallback.h"
#include "Settings.h"
#include "Decorations.h"
#include "UserInterfaceManager.h"
#include "BoundingBoxVisitor.h"
#include "ActionManager.h"
#include "MixingGroup.h"
#include "MousePointer.h"

#include "MixingView.h"

// internal utility
float sin_quad_texture(float x, float y);
uint textureMixingQuadratic();



MixingView::MixingView() : View(MIXING), limbo_scale_(MIXING_MIN_THRESHOLD)
{
    scene.root()->scale_ = glm::vec3(MIXING_DEFAULT_SCALE, MIXING_DEFAULT_SCALE, 1.0f);
    scene.root()->translation_ = glm::vec3(0.5f, 0.0f, 0.0f);
    // read default settings
    if ( Settings::application.views[mode_].name.empty() )
        // no settings found: store application default
        saveSettings();
    else
        restoreSettings();
    Settings::application.views[mode_].name = "Mixing";

    // Mixing scene background
    limbo_ = new Mesh("mesh/disk.ply");
    limbo_->scale_ = glm::vec3(limbo_scale_, limbo_scale_, 1.f);
    limbo_->shader()->color = glm::vec4( COLOR_LIMBO_CIRCLE, 1.f );
    scene.bg()->attach(limbo_);

    // interactive limbo scaling slider
    limbo_slider_root_ = new Group;
    limbo_slider_root_->translation_ = glm::vec3(0.0f, limbo_scale_, 0.f);
    scene.bg()->attach(limbo_slider_root_);
    limbo_slider_ = new Disk();
    limbo_slider_->translation_ = glm::vec3(0.f, -0.01f, 0.f);
    limbo_slider_->scale_ = glm::vec3(0.1f, 0.1f, 1.f);
    limbo_slider_->color = glm::vec4( COLOR_LIMBO_CIRCLE, 1.f );
    limbo_slider_root_->attach(limbo_slider_);
    limbo_up_ = new Mesh("mesh/triangle_point.ply");
    limbo_up_->scale_ = glm::vec3(0.8f, 0.8f, 1.f);
    limbo_up_->shader()->color = glm::vec4( COLOR_CIRCLE_ARROW, 0.01f );
    limbo_slider_root_->attach(limbo_up_);
    limbo_down_ = new Mesh("mesh/triangle_point.ply");
    limbo_down_->translation_ = glm::vec3(0.f, -0.02f, 0.f);
    limbo_down_->scale_ = glm::vec3(0.8f, 0.8f, 1.f);
    limbo_down_->rotation_ = glm::vec3(0.f, 0.f, M_PI);
    limbo_down_->shader()->color = glm::vec4( COLOR_CIRCLE_ARROW, 0.01f );
    limbo_slider_root_->attach(limbo_down_);

    mixingCircle_ = new Mesh("mesh/disk.ply");
    mixingCircle_->shader()->color = glm::vec4( 1.f, 1.f, 1.f, 1.f );
    scene.bg()->attach(mixingCircle_);

    circle_ = new Mesh("mesh/circle.ply");
    circle_->shader()->color = glm::vec4( COLOR_CIRCLE, 1.0f );
    scene.bg()->attach(circle_);

    // Mixing scene foreground

    // interactive button
    button_white_ = new Disk();
    button_white_->scale_ = glm::vec3(0.035f, 0.035f, 1.f);
    button_white_->translation_ = glm::vec3(0.f, 1.f, 0.f);
    button_white_->color = glm::vec4( COLOR_CIRCLE, 1.0f );
    scene.fg()->attach(button_white_);
    // button color WHITE (non interactive; no picking detected on Mesh)
    Mesh *tmp = new Mesh("mesh/disk.ply");
    tmp->scale_ = glm::vec3(0.028f, 0.028f, 1.f);
    tmp->translation_ = glm::vec3(0.f, 1.f, 0.f);
    tmp->shader()->color = glm::vec4( 0.85f, 0.85f, 0.85f, 1.f );
    scene.fg()->attach(tmp);

    // interactive button
    button_black_ = new Disk();
    button_black_->scale_ = glm::vec3(0.035f, 0.035f, 1.f);
    button_black_->translation_ = glm::vec3(0.f, -1.f, 0.f);
    button_black_->color = glm::vec4( COLOR_CIRCLE, 1.0f );
    scene.fg()->attach(button_black_);
    // button color BLACK (non interactive; no picking detected on Mesh)
    tmp = new Mesh("mesh/disk.ply");
    tmp->scale_ = glm::vec3(0.028f, 0.028f, 1.f);
    tmp->translation_ = glm::vec3(0.f, -1.f, 0.f);
    tmp->shader()->color = glm::vec4( 0.1f, 0.1f, 0.1f, 1.f );
    scene.fg()->attach(tmp);

    // moving slider
    slider_root_ = new Group;
    scene.fg()->attach(slider_root_);
    // interactive slider
    slider_ = new Disk();
    slider_->scale_ = glm::vec3(0.08f, 0.08f, 1.f);
    slider_->translation_ = glm::vec3(0.0f, 1.0f, 0.f);
    slider_->color = glm::vec4( COLOR_CIRCLE, 0.9f );
    slider_root_->attach(slider_);
    // dark mask in front
    tmp = new Mesh("mesh/disk.ply");
    tmp->scale_ = glm::vec3(0.075f, 0.075f, 1.f);
    tmp->translation_ = glm::vec3(0.0f, 1.0f, 0.f);
    tmp->shader()->color = glm::vec4( COLOR_SLIDER_CIRCLE, 1.0f );
    slider_root_->attach(tmp);
    // show arrows indicating movement of slider
    slider_arrows_ = new Group;
    slider_arrows_->visible_ = false;
    slider_root_->attach(slider_arrows_);
    tmp = new Mesh("mesh/triangle_point.ply");
    tmp->translation_ = glm::vec3(-0.012f, 1.0f, 0.f);
    tmp->scale_ = glm::vec3(0.7f, 0.7f, 1.f);
    tmp->rotation_ = glm::vec3(0.f, 0.f, M_PI_2);
    tmp->shader()->color = glm::vec4( COLOR_CIRCLE_ARROW, 0.1f );
    slider_arrows_->attach(tmp);
    tmp = new Mesh("mesh/triangle_point.ply");
    tmp->translation_ = glm::vec3(0.012f, 1.0f, 0.f);
    tmp->scale_ = glm::vec3(0.7f, 0.7f, 1.f);
    tmp->rotation_ = glm::vec3(0.f, 0.f, -M_PI_2);
    tmp->shader()->color = glm::vec4( COLOR_CIRCLE_ARROW, 0.1f );
    slider_arrows_->attach(tmp);

    // replace grid with appropriate one
    if (grid) delete grid;
    grid = new MixingGrid(scene.root());
    grid->root()->visible_ = false;
}


void MixingView::draw()
{
    // set texture
    if (mixingCircle_->texture() == 0)
        mixingCircle_->setTexture(textureMixingQuadratic());

    // Display grid
    grid->root()->visible_ = (grid->active() && current_action_ongoing_);

    // temporarily force shaders to use opacity blending for rendering icons
    Shader::force_blending_opacity = true;
    // draw scene of this view
    View::draw();
    // restore state
    Shader::force_blending_opacity = false;

    // display popup menu
    if (show_context_menu_ == MENU_SELECTION) {
        ImGui::OpenPopup( "MixingSelectionContextMenu" );
        show_context_menu_ = MENU_NONE;
    }
    if (ImGui::BeginPopup("MixingSelectionContextMenu")) {

        // colored context menu
        ImGui::PushStyleColor(ImGuiCol_Text, ImGuiToolkit::HighlightColor());
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(COLOR_MENU_HOVERED, 0.5f));

        // special action of Mixing view: link or unlink
        SourceList selected = Mixer::selection().getCopy();
        if ( Mixer::manager().session()->canlink( selected )) {
            // the selected sources can be linked
            if (ImGui::Selectable( ICON_FA_LINK "  Link" )){
                // assemble a MixingGroup
                Mixer::manager().session()->link(selected, scene.fg() );
                Action::manager().store(std::string("Sources linked."));
                // clear selection and select one of the sources of the group
                Source *cur = Mixer::selection().front();
                Mixer::manager().unsetCurrentSource();
                Mixer::selection().clear();
                Mixer::manager().setCurrentSource( cur );
            }
        }
        else {
            // the selected sources cannot be linked: offer to unlink!
            if (ImGui::Selectable( ICON_FA_UNLINK "  Unlink" )){
                // dismantle MixingGroup(s)
                Mixer::manager().session()->unlink(selected);
                Action::manager().store(std::string("Sources unlinked."));
            }
        }

        ImGui::Separator();

        // manipulation of sources in Mixing view
        if (ImGui::Selectable( ICON_FA_CROSSHAIRS "  Center" )){
            glm::vec2 center = glm::vec2(0.f, 0.f);
            for (SourceList::iterator  it = Mixer::selection().begin(); it != Mixer::selection().end(); ++it) {
                // compute barycenter (1)
                center += glm::vec2((*it)->group(View::MIXING)->translation_);
            }
            // compute barycenter (2)
            center /= Mixer::selection().size();
            for (SourceList::iterator  it = Mixer::selection().begin(); it != Mixer::selection().end(); ++it) {
                (*it)->group(View::MIXING)->translation_ -= glm::vec3(center, 0.f);
                (*it)->touch();
            }
            Action::manager().store(std::string("Selection: Mixing Center"));
        }
        if (ImGui::Selectable( ICON_FA_HAYKAL "  Dispatch" )){
            glm::vec2 center = glm::vec2(0.f, 0.f);
            // distribute with equal angle
            float angle = 0.f;
            for (SourceList::iterator  it = Mixer::selection().begin(); it != Mixer::selection().end(); ++it) {

                glm::vec2 P = center + glm::rotate(glm::vec2(0.f, 1.2), angle);
                (*it)->group(View::MIXING)->translation_.x = P.x;
                (*it)->group(View::MIXING)->translation_.y = P.y;
                (*it)->touch();
                angle -= glm::two_pi<float>() / float(Mixer::selection().size());
            }
            Action::manager().store(std::string("Selection: Mixing Dispatch"));
        }
        if (ImGui::Selectable( ICON_FA_FAN "  Distribute" )){
            SourceList list;
            glm::vec2 center = glm::vec2(0.f, 0.f);
            for (SourceList::iterator  it = Mixer::selection().begin(); it != Mixer::selection().end(); ++it) {
                list.push_back(*it);
                // compute barycenter (1)
                center += glm::vec2((*it)->group(View::MIXING)->translation_);
            }
            // compute barycenter (2)
            center /= list.size();
            // sort the vector of sources in clockwise order around the center pos_
            list = mixing_sorted( list, center);
            // average distance
            float d = 0.f;
            for (SourceList::iterator it = list.begin(); it != list.end(); ++it) {
                d += glm::distance(glm::vec2((*it)->group(View::MIXING)->translation_), center);
            }
            d /= list.size();
            // distribute with equal angle
            float angle = 0.f;
            for (SourceList::iterator it = list.begin(); it != list.end(); ++it) {
                glm::vec2 P = center + glm::rotate(glm::vec2(0.f, d), angle);
                (*it)->group(View::MIXING)->translation_.x = P.x;
                (*it)->group(View::MIXING)->translation_.y = P.y;
                (*it)->touch();
                angle -= glm::two_pi<float>() / float(list.size());
            }
            Action::manager().store(std::string("Selection: Mixing Distribute in circle"));
        }
        if (ImGui::Selectable( ICON_FA_ALIGN_CENTER "   Align & Distribute" )){
            SourceList list;
            glm::vec2 center = glm::vec2(0.f, 0.f);
            for (SourceList::iterator  it = Mixer::selection().begin(); it != Mixer::selection().end(); ++it) {
                list.push_back(*it);
                // compute barycenter (1)
                center += glm::vec2((*it)->group(View::MIXING)->translation_);
            }
            // compute barycenter (2)
            center /= list.size();
            // distribute with equal angle
            float i = 0.f;
            float sign = -1.f;
            for (SourceList::iterator it = list.begin(); it != list.end(); ++it, sign*=-1.f) {
                (*it)->group(View::MIXING)->translation_.x = center.x;
                (*it)->group(View::MIXING)->translation_.y = center.y + sign * i;
                if (sign<0) i+=0.32f;
                (*it)->touch();
            }
            Action::manager().store(std::string("Selection: Mixing Align & Distribute"));
        }
        if (ImGui::Selectable( ICON_FA_CLOUD_SUN " Expand & hide" )){
            SourceList::iterator  it = Mixer::selection().begin();
            for (; it != Mixer::selection().end(); ++it) {
                (*it)->call( new SetAlpha(0.f) );
            }
            Action::manager().store(std::string("Selection: Mixing Expand & hide"));
        }
        if (ImGui::Selectable( ICON_FA_SUN "  Compress & show" )){
            SourceList::iterator  it = Mixer::selection().begin();
            for (; it != Mixer::selection().end(); ++it) {
                (*it)->call( new SetAlpha(0.999f) );
            }
            Action::manager().store(std::string("Selection: Mixing Compress & show"));
        }
        ImGui::PopStyleColor(2);
        ImGui::EndPopup();
    }
}

void MixingView::resize ( int scale )
{
    float z = CLAMP(0.01f * (float) scale, 0.f, 1.f);
    z *= z;
    z *= MIXING_MAX_SCALE - MIXING_MIN_SCALE;
    z += MIXING_MIN_SCALE;
    scene.root()->scale_.x = z;
    scene.root()->scale_.y = z;

    // Clamp translation to acceptable area
    const ImGuiIO& io = ImGui::GetIO();
    glm::vec2 res = glm::vec2(io.DisplaySize.x, io.DisplaySize.y);
    glm::vec3 border(2.3f * res.x/res.y, 2.3f, 0.f);
    scene.root()->translation_ = glm::clamp(scene.root()->translation_, -border, border);
}

int  MixingView::size ()
{
    float z = (scene.root()->scale_.x - MIXING_MIN_SCALE) / (MIXING_MAX_SCALE - MIXING_MIN_SCALE);
    return (int) ( sqrt(z) * 100.f);
}

void MixingView::centerSource(Source *s)
{
    // calculate screen area visible in the default view
    GlmToolkit::AxisAlignedBoundingBox view_box;
    glm::mat4 modelview = GlmToolkit::transform(scene.root()->translation_, scene.root()->rotation_, scene.root()->scale_);
    view_box.extend( Rendering::manager().unProject(glm::vec2(0.f, Rendering::manager().mainWindow().height()), modelview) );
    view_box.extend( Rendering::manager().unProject(glm::vec2(Rendering::manager().mainWindow().width(), 0.f), modelview) );

    // check if upper-left corner of source is in view box
    glm::vec3 pos_source = s->group(mode_)->translation_ + glm::vec3( -s->group(mode_)->scale_.x, s->group(mode_)->scale_.y, 0.f);
    if ( !view_box.contains(pos_source)) {
        // not visible so shift view
        glm::vec2 screenpoint = glm::vec2(500.f, 20.f) * Rendering::manager().mainWindow().dpiScale();
        glm::vec3 pos_to = Rendering::manager().unProject(screenpoint, scene.root()->transform_);
        glm::vec4 pos_delta = glm::vec4(pos_to.x, pos_to.y, 0.f, 0.f) - glm::vec4(pos_source.x, pos_source.y, 0.f, 0.f);
        pos_delta = scene.root()->transform_ * pos_delta;
        scene.root()->translation_ += glm::vec3(pos_delta);
    }
}

void MixingView::update(float dt)
{
    View::update(dt);

//    // always update the mixinggroups
//    for (auto g = groups_.begin(); g != groups_.end(); g++)
//        (*g)->update(dt);

    // a more complete update is requested
    // for mixing, this means restore position of the fading slider
    if (View::need_deep_update_ > 0) {

        //
        // Set limbo scale according to session
        //
        const float p = CLAMP(Mixer::manager().session()->activationThreshold(), MIXING_MIN_THRESHOLD, MIXING_MAX_THRESHOLD);
        limbo_slider_root_->translation_.y = p;
        limbo_->scale_ = glm::vec3(p, p, 1.f);

        //
        // prevent invalid scaling
        //
        float s = CLAMP(scene.root()->scale_.x, MIXING_MIN_SCALE, MIXING_MAX_SCALE);
        scene.root()->scale_.x = s;
        scene.root()->scale_.y = s;

        // change grid color
        const ImVec4 c = ImGuiToolkit::HighlightColor();
        grid->setColor( glm::vec4(c.x, c.y, c.z, 0.3) );
    }

    // the current view is the mixing view
    if (Mixer::manager().view() == this ){

        //
        // Set slider to match the actual fading of the session
        //
        float f = Mixer::manager().session()->fading();

        // reverse calculate angle from fading & move slider
        slider_root_->rotation_.z  = SIGN(-slider_root_->rotation_.z) * asin(f) * -2.f;

        // visual feedback on mixing circle
        f = 1.f - f;
        mixingCircle_->shader()->color = glm::vec4(f, f, f, 1.f);

        // update the selection overlay
        const ImVec4 c = ImGuiToolkit::HighlightColor();
        updateSelectionOverlay(glm::vec4(c.x, c.y, c.z, c.w));
    }

}


std::pair<Node *, glm::vec2> MixingView::pick(glm::vec2 P)
{
    // get picking from generic View
    std::pair<Node *, glm::vec2> pick = View::pick(P);

    // deal with internal interactive objects
    if ( pick.first == button_white_ || pick.first == button_black_ ) {

        // animate clic
        pick.first->update_callbacks_.push_back(new BounceScaleCallback(0.3f));

        // animated fading in session
        if (pick.first == button_white_)
            Mixer::manager().session()->setFadingTarget(0.f, 500.f);
        else
            Mixer::manager().session()->setFadingTarget(1.f, 500.f);

    }
    else if ( overlay_selection_icon_ != nullptr && pick.first == overlay_selection_icon_ ) {

        openContextMenu(MENU_SELECTION);
    }
    else {
        // get if a source was picked
        Source *s = Mixer::manager().findSource(pick.first);
        if (s != nullptr) {
            // pick on the lock icon; unlock source
            if ( UserInterface::manager().ctrlModifier() && pick.first == s->lock_) {
                lock(s, false);
                pick = { s->locker_, pick.second };
//                pick = { nullptr, glm::vec2(0.f) };
            }
            // pick on the open lock icon; lock source and cancel pick
            else if ( UserInterface::manager().ctrlModifier() && pick.first == s->unlock_ ) {
                lock(s, true);
                pick = { nullptr, glm::vec2(0.f) };
            }
            // pick a locked source ; cancel pick
            else if ( !UserInterface::manager().ctrlModifier() && s->locked() ) {
                pick = { nullptr, glm::vec2(0.f) };
            }
            // pick the symbol: ask to show editor
            else if ( pick.first == s->symbol_ ) {
                UserInterface::manager().showSourceEditor(s);
            }
            // pick on the mixing group rotation icon
            else if ( pick.first == s->rotation_mixingroup_ ) {
                if (UserInterface::manager().shiftModifier())
                    s->mixinggroup_->setAction( MixingGroup::ACTION_GRAB_ONE );
                else
                    s->mixinggroup_->setAction( MixingGroup::ACTION_ROTATE_ALL );
            }
            // pick source of a mixing group
            else if ( s->mixinggroup_ != nullptr ) {
                if (UserInterface::manager().ctrlModifier()) {
                    SourceList linked = s->mixinggroup_->getCopy();
                    linked.remove(s);
                    if (linked.size() > 0) {
                        // avoid de-selection of current source if in a mixing group
                        if (Mixer::selection().contains(s) && Mixer::selection().size() < 2)
                            Mixer::selection().clear();
                        // instead, select all sources linked
                        if (Mixer::selection().empty())
                            Mixer::selection().add(linked);
                        // the source will be re-selected in UserInterface::handleMouse()
                    }
                }
                else if (UserInterface::manager().shiftModifier())
                    s->mixinggroup_->setAction( MixingGroup::ACTION_GRAB_ONE );
                else
                    s->mixinggroup_->setAction( MixingGroup::ACTION_GRAB_ALL );
            }
        }
    }

    return pick;
}



View::Cursor MixingView::grab (Source *s, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2> pick)
{
    View::Cursor ret = Cursor();
    ret.type = Cursor_ResizeAll;

    // unproject
    glm::vec3 gl_Position_from = Rendering::manager().unProject(from, scene.root()->transform_);
    glm::vec3 gl_Position_to   = Rendering::manager().unProject(to, scene.root()->transform_);

    // No source is given
    if (!s) {

        // snap to grid (polar)
        if (grid->active())
            gl_Position_to = grid->snap(gl_Position_to);

        // if interaction with slider
        if (pick.first == slider_) {

            // apply rotation to match angle with mouse cursor
            float angle = glm::orientedAngle( glm::normalize(glm::vec2(0.f, 1.0)), glm::normalize(glm::vec2(gl_Position_to)));

            // snap on 0 and PI angles
            if ( ABS_DIFF(angle, 0.f) < 0.05)
                angle = 0.f;
            else if ( ABS_DIFF(angle, M_PI) < 0.05)
                angle = M_PI;

            // animate slider (rotation angle on its parent)
            slider_root_->rotation_.z = angle;

            // calculate fading from angle
            float f = sin( ABS(angle) * 0.5f);
            Mixer::manager().session()->setFadingTarget(f);

            // cursor feedback
            slider_->color = glm::vec4( COLOR_CIRCLE_OVER, 0.9f );
            slider_arrows_->visible_ = true;
            std::ostringstream info;
            info << ICON_FA_ADJUST << " Output fading " << 100 - int(f * 100.0) << " %";
            return Cursor(Cursor_Hand, info.str() );
        }
        else if (pick.first == limbo_slider_) {

            // move slider scaling limbo area
            const float p = CLAMP(gl_Position_to.y, MIXING_MIN_THRESHOLD, MIXING_MAX_THRESHOLD);
            limbo_slider_root_->translation_.y = p;
            limbo_->scale_ = glm::vec3(p, p, 1.f);
            Mixer::manager().session()->setActivationThreshold(p);

            // color change of arrow indicators
            limbo_up_->shader()->color = glm::vec4( COLOR_CIRCLE_ARROW, p < MIXING_MAX_THRESHOLD ? 0.15f : 0.01f );
            limbo_down_->shader()->color = glm::vec4( COLOR_CIRCLE_ARROW, p > MIXING_MIN_THRESHOLD ? 0.15f : 0.01f );

            std::ostringstream info;
            info << ICON_FA_SNOWFLAKE " Deactivation limit";
            return Cursor(Cursor_Hand, info.str() );
        }

        // nothing to do
        return Cursor();
    }
    //
    // Interaction with source
    //
    // compute position after translation
    glm::vec3 newpos = s->stored_status_->translation_ + gl_Position_to - gl_Position_from;

    // snap to grid (polar)
    if (grid->active())
        newpos = grid->snap(newpos);

    // apply translation
    s->group(mode_)->translation_ = newpos;

    // manage mixing group
    if (s->mixinggroup_ != nullptr  ) {
        // inform mixing groups to follow the current source
        if (Source::isCurrent(s) && s->mixinggroup_->action() > MixingGroup::ACTION_UPDATE) {
            s->mixinggroup_->follow(s);
            // special cursor for rotation
            if (s->mixinggroup_->action() == MixingGroup::ACTION_ROTATE_ALL)
                ret.type = Cursor_Hand;
        }
        else
            s->mixingGroup()->setAction(MixingGroup::ACTION_NONE);
    }

    // request update
    s->touch();

    //
    // grab all others from selection
    //
    // compute effective translation of current source s
    newpos = s->group(mode_)->translation_ - s->stored_status_->translation_;
    // loop over selection
    for (auto it = Mixer::selection().begin(); it != Mixer::selection().end(); ++it) {
        if ( *it != s && !(*it)->locked() && ( s->mixinggroup_ == nullptr || !s->mixinggroup_->contains(*it)) ) {
            // translate and request update
            (*it)->group(mode_)->translation_ = (*it)->stored_status_->translation_ + newpos;
            (*it)->touch();
        }
    }

    std::ostringstream info;
    if (s->active()) {
        info << "Alpha " << std::fixed << std::setprecision(3) << s->blendingShader()->color.a << "  ";
        info << ( (s->blendingShader()->color.a > 0.f) ? ICON_FA_SUN : ICON_FA_MOON);
    }
    else
        info << "Inactive  " << ICON_FA_SNOWFLAKE;

    // store action in history
    current_action_ = s->name() + ": " + info.str();


    // update cursor
    ret.info = info.str();
    return ret;
}

void MixingView::terminate(bool force)
{
    View::terminate(force);

    // terminate all mixing group actions
    for (auto g = Mixer::manager().session()->beginMixingGroup();
         g != Mixer::manager().session()->endMixingGroup(); ++g)
        (*g)->setAction( MixingGroup::ACTION_FINISH );

}

View::Cursor MixingView::over (glm::vec2 pos)
{
    View::Cursor ret = Cursor();
    std::pair<Node *, glm::vec2> pick = View::pick(pos);

    //
    // deal with internal interactive objects
    //
    // mouse over opacity slider
    if ( pick.first == slider_ ) {
        slider_->color = glm::vec4( COLOR_CIRCLE_OVER, 0.9f );        
        slider_arrows_->visible_ = true;
        ret.type = Cursor_Hand;
    }
    else {
        slider_arrows_->visible_ = false;
        slider_->color = glm::vec4( COLOR_CIRCLE, 0.9f );
    }

    // mouse over limbo scale
    if ( pick.first == limbo_slider_ ) {
        limbo_up_->shader()->color = glm::vec4( COLOR_CIRCLE_ARROW, limbo_slider_root_->translation_.y < MIXING_MAX_THRESHOLD ? 0.1f : 0.01f );
        limbo_down_->shader()->color = glm::vec4( COLOR_CIRCLE_ARROW, limbo_slider_root_->translation_.y > MIXING_MIN_THRESHOLD ? 0.1f : 0.01f );
        ret.type = Cursor_Hand;
    }
    else {
        limbo_up_->shader()->color = glm::vec4( COLOR_CIRCLE_ARROW, 0.01f );
        limbo_down_->shader()->color = glm::vec4( COLOR_CIRCLE_ARROW, 0.01f );
    }

    // mouse over white and black buttons
    if ( pick.first == button_white_ )
        button_white_->color = glm::vec4( COLOR_CIRCLE_OVER, 1.f );
    else
        button_white_->color = glm::vec4( COLOR_CIRCLE, 1.f );

    if ( pick.first == button_black_ )
        button_black_->color = glm::vec4( COLOR_CIRCLE_OVER, 1.f );
    else
        button_black_->color = glm::vec4( COLOR_CIRCLE, 1.f );

    //
    // mouse over source
    //
    //    Source *s = Mixer::manager().findSource(pick.first);
    Source *s = Mixer::manager().currentSource();
    if (s != nullptr && s->ready()) {

        s->symbol_->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 1.f );
        const ImVec4 h = ImGuiToolkit::HighlightColor();

        // overlay symbol
        if ( pick.first == s->symbol_ )
            s->symbol_->color = glm::vec4( h.x, h.y, h.z, 1.f );

    }

    return ret;
}

#define MAX_DURATION 1000.f
#define MIN_SPEED_M 0.005f
#define MAX_SPEED_M 0.5f

void MixingView::arrow (glm::vec2 movement)
{
    static float _duration = 0.f;
    static glm::vec2 _from(0.f);
    static glm::vec2 _displacement(0.f);

    Source *current = Mixer::manager().currentSource();

    if (!current && !Mixer::selection().empty())
        Mixer::manager().setCurrentSource( Mixer::selection().back() );

    if (current) {

        if (current_action_ongoing_) {

            // TODO : precise movement ?

            // add movement to displacement
            _duration += dt_;
            const float speed = MIN_SPEED_M + (MAX_SPEED_M - MIN_SPEED_M) * glm::min(1.f,_duration / MAX_DURATION);
            _displacement += movement * dt_ * speed;

            // set coordinates of target
            glm::vec2 _to  = _from + _displacement;

            // update mouse pointer action
            MousePointer::manager().active()->update(_to, dt_ / 1000.f);

            if (current->mixinggroup_ != nullptr )
                current->mixinggroup_->setAction( MixingGroup::ACTION_GRAB_ALL );

            // simulate mouse grab
            grab(current, _from, MousePointer::manager().active()->target(),
                 std::make_pair(current->group(mode_), glm::vec2(0.f) ) );

            // draw mouse pointer effect
            MousePointer::manager().active()->draw();
        }
        else {

            if (UserInterface::manager().altModifier() || Settings::application.mouse_pointer_lock)
                MousePointer::manager().setActiveMode( (Pointer::Mode) Settings::application.mouse_pointer );
            else
                MousePointer::manager().setActiveMode( Pointer::POINTER_DEFAULT );

            // reset
            _duration = 0.f;
            _displacement = glm::vec2(0.f);

            // initiate view action and store status of source
            initiate();

            // get coordinates of source and set this as start of mouse position
            _from = glm::vec2( Rendering::manager().project(current->group(mode_)->translation_, scene.root()->transform_) );

            // Initiate mouse pointer action
            MousePointer::manager().active()->initiate(_from);
        }

    }
    else {
        terminate(true);

        // reset
        _duration = 0.f;
        _from = glm::vec2(0.f);
        _displacement = glm::vec2(0.f);
    }

}

void MixingView::setAlpha(Source *s)
{
    if (!s)
        return;

    // move the mixing node of the source
    Group *sourceNode = s->group(mode_);
    glm::vec2 mix_pos(sourceNode->translation_);

    for(NodeSet::iterator it = scene.ws()->begin(); it != scene.ws()->end(); ++it) {
        // avoid superposing icons: distribute equally
        if ( glm::distance(glm::vec2((*it)->translation_), mix_pos) < DELTA_ALPHA) {
            mix_pos += glm::vec2(-0.03f, 0.03f);
        }
    }

    sourceNode->translation_.x = mix_pos.x;
    sourceNode->translation_.y = mix_pos.y;

    // request update
    s->touch();
}


void MixingView::updateSelectionOverlay(glm::vec4 color)
{
    View::updateSelectionOverlay(color);

    if (overlay_selection_->visible_) {
        // calculate bbox on selection
        GlmToolkit::AxisAlignedBoundingBox selection_box = BoundingBoxVisitor::AABB(Mixer::selection().getCopy(), this);
        overlay_selection_->scale_ = selection_box.scale();
        overlay_selection_->translation_ = selection_box.center();

        // slightly extend the boundary of the selection
        overlay_selection_frame_->scale_ = glm::vec3(1.f) + glm::vec3(0.01f, 0.01f, 1.f) / overlay_selection_->scale_;
    }
}

#define CIRCLE_PIXELS 64
#define CIRCLE_PIXEL_RADIUS 1024.f
//#define CIRCLE_PIXELS 256
//#define CIRCLE_PIXEL_RADIUS 16384.0
//#define CIRCLE_PIXELS 1024
//#define CIRCLE_PIXEL_RADIUS 262144.0

float sin_quad_texture(float x, float y) {
//    return 0.5f + 0.5f * cos( M_PI * CLAMP( ( ( x * x ) + ( y * y ) ) / CIRCLE_PIXEL_RADIUS, 0.f, 1.f ) );
    float D = sqrt( ( x * x )/ CIRCLE_PIXEL_RADIUS + ( y * y )/ CIRCLE_PIXEL_RADIUS );
    return 0.5f + 0.5f * cos( M_PI * CLAMP( D * sqrt(D), 0.f, 1.f ) );
}

uint textureMixingQuadratic()
{
    static GLuint texid = 0;
    if (texid == 0) {
        // generate the texture with alpha exactly as computed for sources
        GLubyte matrix[CIRCLE_PIXELS*CIRCLE_PIXELS * 4];
        int l = -CIRCLE_PIXELS / 2 + 1;

        for (int i = 0; i < CIRCLE_PIXELS ; ++i) {
            int c = -CIRCLE_PIXELS / 2 + 1;
            for (int j = 0; j < CIRCLE_PIXELS ; ++j) {
                // distance to the center
                GLfloat distance = sin_quad_texture( (float) c , (float) l  );
//                distance = 1.f - (GLfloat) ((c * c) + (l * l)) / CIRCLE_PIXEL_RADIUS;  // quadratic
//                distance = 1.f - (GLfloat) sqrt( (GLfloat) ((c * c) + (l * l))) / (GLfloat) sqrt(CIRCLE_PIXEL_RADIUS); // linear

                // transparency
                GLfloat alpha = 255.f * CLAMP( distance , 0.f, 1.f);
                GLubyte A = static_cast<GLubyte>(alpha);

                // luminance adjustment
                GLfloat luminance = 255.f * CLAMP( 0.2f + 0.75f * distance, 0.f, 1.f);
                GLubyte L = static_cast<GLubyte>(luminance);

                // fill pixel RGBA
                matrix[ i * CIRCLE_PIXELS * 4 + j * 4 + 0 ] = L;
                matrix[ i * CIRCLE_PIXELS * 4 + j * 4 + 1 ] = L;
                matrix[ i * CIRCLE_PIXELS * 4 + j * 4 + 2 ] = L;
                matrix[ i * CIRCLE_PIXELS * 4 + j * 4 + 3 ] = A;

                ++c;
            }
            ++l;
        }
        // setup texture
        glGenTextures(1, &texid);
        glBindTexture(GL_TEXTURE_2D, texid);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, CIRCLE_PIXELS, CIRCLE_PIXELS);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, CIRCLE_PIXELS, CIRCLE_PIXELS, GL_BGRA, GL_UNSIGNED_BYTE, matrix);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    return texid;
}


MixingGrid::MixingGrid(Group *parent) : Grid(parent, Grid::GRID_POLAR)
{
    root_ = new Group;
    root_->visible_ = false;
    parent_->attach(root_);

    polar_grids_ = new Switch;    
    polar_grids_->attach(new LineCircleGrid(Grid::polar_units_[UNIT_PRECISE], 50,
                                            Grid::ortho_units_[UNIT_PRECISE], 0.5f));
    polar_grids_->attach(new LineCircleGrid(Grid::polar_units_[UNIT_SMALL], 30,
                                            Grid::ortho_units_[UNIT_SMALL], 0.5f));
    polar_grids_->attach(new LineCircleGrid(Grid::polar_units_[UNIT_DEFAULT], 15,
                                            Grid::ortho_units_[UNIT_DEFAULT], 0.5f));
    polar_grids_->attach(new LineCircleGrid(Grid::polar_units_[UNIT_LARGE],  6,
                                            Grid::ortho_units_[UNIT_LARGE], 0.5f));
    polar_grids_->attach(new LineCircleGrid(Grid::polar_units_[UNIT_ONE],  3,
                                            Grid::ortho_units_[UNIT_ONE], 0.5f));
    root_->attach(polar_grids_);

    // not visible at init
    setColor( glm::vec4(0.f) );
}

Group *MixingGrid::root ()
{
    //adjust the grid to the unit scale
    polar_grids_->setActive(unit_);

    // return the node to draw
    return root_;
}




//    // trying to enter stash
//    if ( glm::distance( glm::vec2(s->group(mode_)->translation_), glm::vec2(stashCircle_->translation_)) < stashCircle_->scale_.x) {

//        // refuse to put an active source in stash
//        if (s->active())
//            s->group(mode_)->translation_ = s->stored_status_->translation_;
//        else {
//            Mixer::manager().conceal(s);
//            s->group(mode_)->scale_ = glm::vec3(MIXING_ICON_SCALE) - glm::vec3(0.1f, 0.1f, 0.f);
//        }
//    }
//    else if ( Mixer::manager().concealed(s) ) {
//        Mixer::manager().uncover(s);
//        s->group(mode_)->scale_ = glm::vec3(MIXING_ICON_SCALE);
//    }

