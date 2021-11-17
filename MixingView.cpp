/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2020-2021 Bruno Herbelin <bruno.herbelin@gmail.com>
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
#include "Settings.h"
#include "Decorations.h"
#include "UserInterfaceManager.h"
#include "BoundingBoxVisitor.h"
#include "ActionManager.h"
#include "MixingGroup.h"
#include "Log.h"

#include "MixingView.h"

// internal utility
float sin_quad_texture(float x, float y);
uint textureMixingQuadratic();



MixingView::MixingView() : View(MIXING), limbo_scale_(MIXING_LIMBO_SCALE)
{
    scene.root()->scale_ = glm::vec3(MIXING_DEFAULT_SCALE, MIXING_DEFAULT_SCALE, 1.0f);
    scene.root()->translation_ = glm::vec3(0.0f, 0.0f, 0.0f);
    // read default settings
    if ( Settings::application.views[mode_].name.empty() ) {
        // no settings found: store application default
        Settings::application.views[mode_].name = "Mixing";
        saveSettings();
    }
    else
        restoreSettings();

    // Mixing scene background
    Mesh *tmp = new Mesh("mesh/disk.ply");
    tmp->scale_ = glm::vec3(limbo_scale_, limbo_scale_, 1.f);
    tmp->shader()->color = glm::vec4( COLOR_LIMBO_CIRCLE, 0.6f );
    scene.bg()->attach(tmp);

    mixingCircle_ = new Mesh("mesh/disk.ply");
    mixingCircle_->shader()->color = glm::vec4( 1.f, 1.f, 1.f, 1.f );
    scene.bg()->attach(mixingCircle_);

    circle_ = new Mesh("mesh/circle.ply");
    circle_->shader()->color = glm::vec4( COLOR_CIRCLE, 1.0f );
    scene.bg()->attach(circle_);

    // Mixing scene foreground

    // button frame
    tmp = new Mesh("mesh/disk.ply");
    tmp->scale_ = glm::vec3(0.033f, 0.033f, 1.f);
    tmp->translation_ = glm::vec3(0.f, 1.f, 0.f);
    tmp->shader()->color = glm::vec4( COLOR_CIRCLE, 0.9f );
    scene.fg()->attach(tmp);
    // interactive button
    button_white_ = new Disk();
    button_white_->scale_ = glm::vec3(0.026f, 0.026f, 1.f);
    button_white_->translation_ = glm::vec3(0.f, 1.f, 0.f);
    button_white_->color = glm::vec4( 0.85f, 0.85f, 0.85f, 1.0f );
    scene.fg()->attach(button_white_);
    // button frame
    tmp = new Mesh("mesh/disk.ply");
    tmp->scale_ = glm::vec3(0.033f, 0.033f, 1.f);
    tmp->translation_ = glm::vec3(0.f, -1.f, 0.f);
    tmp->shader()->color = glm::vec4( COLOR_CIRCLE, 0.9f );
    scene.fg()->attach(tmp);
    // interactive button
    button_black_ = new Disk();
    button_black_->scale_ = glm::vec3(0.026f, 0.026f, 1.f);
    button_black_->translation_ = glm::vec3(0.f, -1.f, 0.f);
    button_black_->color = glm::vec4( 0.1f, 0.1f, 0.1f, 1.0f );
    scene.fg()->attach(button_black_);
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


    stashCircle_ = new Disk();
    stashCircle_->scale_ = glm::vec3(0.5f, 0.5f, 1.f);
    stashCircle_->translation_ = glm::vec3(2.f, -1.0f, 0.f);
    stashCircle_->color = glm::vec4( COLOR_STASH_CIRCLE, 0.6f );
//    scene.bg()->attach(stashCircle_);

}


void MixingView::draw()
{
    // set texture
    if (mixingCircle_->texture() == 0)
        mixingCircle_->setTexture(textureMixingQuadratic());

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
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.36f, 0.36f, 0.36f, 0.44f));

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
        if (ImGui::Selectable( ICON_FA_HAYKAL "  Distribute" )){
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
            Action::manager().store(std::string("Selection: Mixing Distribute"));
        }
        if (ImGui::Selectable( ICON_FA_CLOUD_SUN " Expand & hide" )){
            SourceList::iterator  it = Mixer::selection().begin();
            for (; it != Mixer::selection().end(); ++it) {
                (*it)->setAlpha(0.f);
            }
            Action::manager().store(std::string("Selection: Mixing Expand & hide"));
        }
        if (ImGui::Selectable( ICON_FA_SUN "  Compress & show" )){
            SourceList::iterator  it = Mixer::selection().begin();
            for (; it != Mixer::selection().end(); ++it) {
                (*it)->setAlpha(0.99f);
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
    glm::vec2 res = resolution();
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
        // Set slider to match the actual fading of the session
        //
        float f = Mixer::manager().session()->empty() ? 0.f : Mixer::manager().session()->fading();

        // reverse calculate angle from fading & move slider
        slider_root_->rotation_.z  = SIGN(slider_root_->rotation_.z) * asin(f) * 2.f;

        // visual feedback on mixing circle
        f = 1.f - f;
        mixingCircle_->shader()->color = glm::vec4(f, f, f, 1.f);

        // prevent invalid scaling
        float s = CLAMP(scene.root()->scale_.x, MIXING_MIN_SCALE, MIXING_MAX_SCALE);
        scene.root()->scale_.x = s;
        scene.root()->scale_.y = s;
    }

    // the current view is the mixing view
    if (Mixer::manager().view() == this )
    {
        //
        // Set session fading to match the slider angle (during animation)
        //

        // calculate fading from angle
        float f = sin( ABS(slider_root_->rotation_.z) * 0.5f);

        // apply fading
        if ( ABS_DIFF( f, Mixer::manager().session()->fading()) > EPSILON )
        {
            // apply fading to session
            Mixer::manager().session()->setFading(f);

            // visual feedback on mixing circle
            f = 1.f - f;
            mixingCircle_->shader()->color = glm::vec4(f, f, f, 1.f);
        }

        // update the selection overlay
        updateSelectionOverlay();
    }

}


std::pair<Node *, glm::vec2> MixingView::pick(glm::vec2 P)
{
    // get picking from generic View
    std::pair<Node *, glm::vec2> pick = View::pick(P);

    // deal with internal interactive objects
    if ( pick.first == button_white_ || pick.first == button_black_ ) {

        RotateToCallback *anim = nullptr;
        if (pick.first == button_white_)
            anim = new RotateToCallback(0.f, 500.f);
        else
            anim = new RotateToCallback(SIGN(slider_root_->rotation_.z) * M_PI, 500.f);

        // animate clic
        pick.first->update_callbacks_.push_back(new BounceScaleCallback(0.3f));

        // reset & start animation
        slider_root_->update_callbacks_.clear();
        slider_root_->update_callbacks_.push_back(anim);

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
//                pick = { s->locker_, pick.second };
                pick = { nullptr, glm::vec2(0.f) };
            }
            // pick on the open lock icon; lock source and cancel pick
            else if ( UserInterface::manager().ctrlModifier() && pick.first == s->unlock_ ) {
                lock(s, true);
                pick = { nullptr, glm::vec2(0.f) };
            }
            // pick a locked source ; cancel pick
            else if ( s->locked() ) {
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
                    if (Mixer::selection().empty())
                        Mixer::selection().add(linked);
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

            // cursor feedback
            slider_->color = glm::vec4( COLOR_CIRCLE_OVER, 0.9f );
            std::ostringstream info;
            info  << "Global opacity " << 100 - int(Mixer::manager().session()->fading() * 100.0) << " %";
            return Cursor(Cursor_Hand, info.str() );
        }

        // nothing to do
        return Cursor();
    }
    //
    // Interaction with source
    //
    // compute delta translation
    s->group(mode_)->translation_ = s->stored_status_->translation_ + gl_Position_to - gl_Position_from;

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


    std::ostringstream info;
    if (s->active()) {
        info << "Alpha " << std::fixed << std::setprecision(3) << s->blendingShader()->color.a << "  ";
        info << ( (s->blendingShader()->color.a > 0.f) ? ICON_FA_EYE : ICON_FA_EYE_SLASH);
    }
    else
        info << "Inactive  " << ICON_FA_SNOWFLAKE;

    // store action in history
    current_action_ = s->name() + ": " + info.str();

    // update cursor
    ret.info = info.str();
    return ret;
}

void MixingView::terminate()
{
    View::terminate();

    // terminate all mixing group actions
    for (auto g = Mixer::manager().session()->beginMixingGroup();
         g != Mixer::manager().session()->endMixingGroup(); ++g)
        (*g)->setAction( MixingGroup::ACTION_FINISH );

}

View::Cursor MixingView::over (glm::vec2 pos)
{
    View::Cursor ret = Cursor();
    std::pair<Node *, glm::vec2> pick = View::pick(pos);

    // deal with internal interactive objects
    if ( pick.first == slider_ ) {
        slider_->color = glm::vec4( COLOR_CIRCLE_OVER, 0.9f );
        ret.type = Cursor_Hand;
    }
    else
        slider_->color = glm::vec4( COLOR_CIRCLE, 0.9f );


    return ret;
}

void MixingView::arrow (glm::vec2 movement)
{
    static float accumulator = 0.f;
    accumulator += dt_;

    glm::vec3 gl_Position_from = Rendering::manager().unProject(glm::vec2(0.f), scene.root()->transform_);
    glm::vec3 gl_Position_to   = Rendering::manager().unProject(movement, scene.root()->transform_);
    glm::vec3 gl_delta = gl_Position_to - gl_Position_from;

    bool first = true;
    glm::vec3 delta_translation(0.f);
    for (auto it = Mixer::selection().begin(); it != Mixer::selection().end(); ++it) {

        // individual move with SHIFT
        if ( !Source::isCurrent(*it) && UserInterface::manager().shiftModifier() )
            continue;

        Group *sourceNode = (*it)->group(mode_);
        glm::vec3 dest_translation(0.f);

        if (first) {
            // dest starts at current
            dest_translation = sourceNode->translation_;

            // + ALT : discrete displacement
            if (UserInterface::manager().altModifier()) {
                if (accumulator > 100.f) {
                    dest_translation += glm::sign(gl_delta) * 0.1f;
                    dest_translation.x = ROUND(dest_translation.x, 10.f);
                    dest_translation.y = ROUND(dest_translation.y, 10.f);
                    accumulator = 0.f;
                }
                else
                    break;
            }
            else {
                // normal case: dest += delta
                dest_translation += gl_delta * ARROWS_MOVEMENT_FACTOR * dt_;
                accumulator = 0.f;
            }

            // store action in history
            std::ostringstream info;
            if ((*it)->active()) {
                info << "Alpha " << std::fixed << std::setprecision(3) << (*it)->blendingShader()->color.a << "  ";
                info << ( ((*it)->blendingShader()->color.a > 0.f) ? ICON_FA_EYE : ICON_FA_EYE_SLASH);
            }
            else
                info << "Inactive  " << ICON_FA_SNOWFLAKE;
            current_action_ = (*it)->name() + ": " + info.str();

            // delta for others to follow
            delta_translation = dest_translation - sourceNode->translation_;
        }
        else {
            // dest = current + delta from first
            dest_translation = sourceNode->translation_ + delta_translation;
        }

        // apply & request update
        sourceNode->translation_ = dest_translation;
        (*it)->touch();

        first = false;
    }

}

void MixingView::setAlpha(Source *s)
{
    if (!s)
        return;

    // move the layer node of the source
    Group *sourceNode = s->group(mode_);
    glm::vec2 mix_pos = glm::vec2(DEFAULT_MIXING_TRANSLATION);

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


void MixingView::updateSelectionOverlay()
{
    View::updateSelectionOverlay();

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
