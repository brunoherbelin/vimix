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
#include <iomanip>

#include "Mixer.h"
#include "defines.h"
#include "Source.h"
#include "SourceCallback.h"
#include "Settings.h"
#include "Decorations.h"
#include "UserInterfaceManager.h"
#include "BoundingBoxVisitor.h"
#include "ActionManager.h"

#include "LayerView.h"

LayerView::LayerView() : View(LAYER), aspect_ratio(1.f)
{
    scene.root()->scale_ = glm::vec3(LAYER_DEFAULT_SCALE, LAYER_DEFAULT_SCALE, 1.0f);
    scene.root()->translation_ = glm::vec3(2.2f, 1.2f, 0.0f);
    // read default settings
    if ( Settings::application.views[mode_].name.empty() )
        // no settings found: store application default
        saveSettings();
    else
        restoreSettings();
    Settings::application.views[mode_].name = "Layer";

    // Geometry Scene background
    frame_ = new Group;
    Surface *rect = new Surface;
    rect->shader()->color.a = 0.3f;
    frame_->attach(rect);

    Frame *border = new Frame(Frame::ROUND, Frame::THIN, Frame::PERSPECTIVE);
    border->color = glm::vec4( COLOR_FRAME, 0.95f );
    frame_->attach(border);
    scene.bg()->attach(frame_);

    persp_left_ = new Mesh("mesh/perspective_axis_left.ply");
    persp_left_->shader()->color = glm::vec4( COLOR_FRAME_LIGHT, 1.f );
    persp_left_->scale_.x = LAYER_PERSPECTIVE;
    persp_left_->translation_.z = -0.1f;
    persp_left_->translation_.x = -1.f;
    scene.bg()->attach(persp_left_);

    persp_right_ = new Mesh("mesh/perspective_axis_right.ply");
    persp_right_->shader()->color = glm::vec4( COLOR_FRAME_LIGHT, 1.f );
    persp_right_->scale_.x = LAYER_PERSPECTIVE;
    persp_right_->translation_.z = -0.1f;
    persp_right_->translation_.x = 1.f;
    scene.bg()->attach(persp_right_);

}


void LayerView::draw()
{
    View::draw();

    // initialize the verification of the selection
    static bool candidate_flatten_group = false;

    // display popup menu
    if (show_context_menu_ == MENU_SELECTION) {

        // initialize the verification of the selection
        candidate_flatten_group = true;
        // start loop on selection
        SourceList::iterator  it = Mixer::selection().begin();
        float depth_first = (*it)->depth();
        for (; it != Mixer::selection().end(); ++it) {
            // test if selection is contiguous in layer (i.e. not interrupted)
            SourceList::iterator inter = Mixer::manager().session()->find(depth_first, (*it)->depth());
            if ( inter != Mixer::manager().session()->end() && !Mixer::selection().contains(*inter)){
                // NOT a group: there is a source in the session that
                // - is between two selected sources (in depth)
                // - is not part of the selection
                candidate_flatten_group = false;
                break;
            }
        }

        ImGui::OpenPopup( "LayerSelectionContextMenu" );
        show_context_menu_ = MENU_NONE;
    }
    if (ImGui::BeginPopup("LayerSelectionContextMenu")) {

        // colored context menu
        ImGui::PushStyleColor(ImGuiCol_Text, ImGuiToolkit::HighlightColor());
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(COLOR_MENU_HOVERED, 0.5f));

        // special action of Mixing view
        if (candidate_flatten_group){
            if (ImGui::Selectable( ICON_FA_SIGN_IN_ALT "  Group" )) {
                Mixer::manager().groupSelection();
            }
        }
        else {
            ImGui::TextDisabled( ICON_FA_SIGN_IN_ALT "  Group" );
        }
        ImGui::Separator();

        // manipulation of sources in Mixing view
        if (ImGui::Selectable( ICON_FA_ALIGN_CENTER "   Distribute" )){
            SourceList dsl = depth_sorted(Mixer::selection().getCopy());
            SourceList::iterator  it = dsl.begin();
            float depth = (*it)->depth();
            float depth_inc   = (dsl.back()->depth() - depth) / static_cast<float>(Mixer::selection().size()-1);
            for (++it; it != dsl.end(); ++it) {
                depth += depth_inc;
                (*it)->call( new SetDepth(depth, 80.f) );
            }
            Action::manager().store(std::string("Selection: Layer Distribute"));
        }
        if (ImGui::Selectable( ICON_FA_RULER_HORIZONTAL "  Compress" )){
            SourceList dsl = depth_sorted(Mixer::selection().getCopy());
            SourceList::iterator  it = dsl.begin();
            float depth = (*it)->depth();
            for (++it; it != dsl.end(); ++it) {
                depth += LAYER_STEP;
                (*it)->call( new SetDepth(depth, 80.f) );
            }
            Action::manager().store(std::string("Selection: Layer Compress"));
        }
        if (ImGui::Selectable( ICON_FA_EXCHANGE_ALT "  Reverse order" )){
            SourceList dsl = depth_sorted(Mixer::selection().getCopy());
            SourceList::iterator  it = dsl.begin();
            SourceList::reverse_iterator  rit = dsl.rbegin();
            for (; it != dsl.end(); ++it, ++rit) {
                (*it)->call( new SetDepth((*rit)->depth(), 80.f) );
            }
            Action::manager().store(std::string("Selection: Layer Reverse order"));
        }

        ImGui::PopStyleColor(2);
        ImGui::EndPopup();
    }
}


void LayerView::update(float dt)
{
    View::update(dt);

    // a more complete update is requested
    if (View::need_deep_update_ > 0) {

        // update rendering of render frame
        FrameBuffer *output = Mixer::manager().session()->frame();
        if (output){
            // correct with aspect ratio
            aspect_ratio = output->aspectRatio();
            frame_->scale_.x = aspect_ratio;
            persp_left_->translation_.x = -aspect_ratio;
            persp_right_->translation_.x = aspect_ratio + 0.06;
        }

        // prevent invalid scaling
        float s = CLAMP(scene.root()->scale_.x, LAYER_MIN_SCALE, LAYER_MAX_SCALE);
        scene.root()->scale_.x = s;
        scene.root()->scale_.y = s;
    }

    if (Mixer::manager().view() == this )
    {
        // update the selection overlay
        ImVec4 c = ImGuiToolkit::HighlightColor();
        updateSelectionOverlay(glm::vec4(c.x, c.y, c.z, c.w));
    }

}

bool LayerView::canSelect(Source *s) {

    return ( View::canSelect(s) && s->active() );
}

void LayerView::resize ( int scale )
{
    float z = CLAMP(0.01f * (float) scale, 0.f, 1.f);
    z *= z;
    z *= LAYER_MAX_SCALE - LAYER_MIN_SCALE;
    z += LAYER_MIN_SCALE;
    scene.root()->scale_.x = z;
    scene.root()->scale_.y = z;

    // Clamp translation to acceptable area
    glm::vec3 border(2.f, 1.f, 0.f);
    scene.root()->translation_ = glm::clamp(scene.root()->translation_, -border, border * 2.f);
}

int  LayerView::size ()
{
    float z = (scene.root()->scale_.x - LAYER_MIN_SCALE) / (LAYER_MAX_SCALE - LAYER_MIN_SCALE);
    return (int) ( sqrt(z) * 100.f);
}


std::pair<Node *, glm::vec2> LayerView::pick(glm::vec2 P)
{
    // get picking from generic View
    std::pair<Node *, glm::vec2> pick = View::pick(P);

    // deal with internal interactive objects
    if ( overlay_selection_icon_ != nullptr && pick.first == overlay_selection_icon_ ) {

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
            // pick a locked source; cancel pick
            else if ( !UserInterface::manager().ctrlModifier() && s->locked() ) {
                pick = { nullptr, glm::vec2(0.f) };
            }
            // pick the symbol: ask to show editor
            else if ( pick.first == s->symbol_ ) {
                UserInterface::manager().showSourceEditor(s);
            }
        }
        else
            pick = { nullptr, glm::vec2(0.f) };
    }

    return pick;
}


float LayerView::setDepth(Source *s, float d)
{
    if (!s)
        return -1.f;

    // move the layer node of the source
    Group *sourceNode = s->group(mode_);

    float depth = d < 0.f ? sourceNode->translation_.z : d;

    // negative or no  depth given; find the front most depth
    if ( depth < 0.f ) {
        // default to place visible in front of background
        depth = LAYER_BACKGROUND + LAYER_STEP;

        // find the front-most souce in the workspace (behind FOREGROUND)
        for (NodeSet::iterator node = scene.ws()->begin(); node != scene.ws()->end(); ++node) {
            // place in front of previous sources
            depth = MAX(depth, (*node)->translation_.z + LAYER_STEP);

            // in case node is already at max depth
            if ((*node)->translation_.z + DELTA_DEPTH > MAX_DEPTH )
                (*node)->translation_.z -= DELTA_DEPTH;
        }
    }

    // change depth
    sourceNode->translation_.z = CLAMP( depth, MIN_DEPTH, MAX_DEPTH);

    // request reordering of scene at next update
    ++View::need_deep_update_;

    // request update of source
    s->touch();

    return sourceNode->translation_.z;
}

View::Cursor LayerView::grab (Source *s, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2>)
{
    if (!s)
        return Cursor();

    // unproject
    glm::vec3 gl_Position_from = Rendering::manager().unProject(from, scene.root()->transform_);
    glm::vec3 gl_Position_to   = Rendering::manager().unProject(to, scene.root()->transform_);

    // compute delta translation
    glm::vec3 dest_translation = s->stored_status_->translation_ + gl_Position_to - gl_Position_from;

    // discretized translation with ALT
    if (UserInterface::manager().altModifier())
        dest_translation.x = ROUND(dest_translation.x, 5.f);

    // apply change
    float d = setDepth( s,  MAX( -dest_translation.x, 0.f) );

    // store action in history
    std::ostringstream info;
    info << "Depth " << std::fixed << std::setprecision(2) << d << "  ";
    current_action_ = s->name() + ": " + info.str();

    if ( d > LAYER_FOREGROUND )
        info << "\n   (Foreground)";
    else if ( d < LAYER_BACKGROUND )
        info << "\n   (Background)";

    return Cursor(Cursor_ResizeNESW, info.str() );
}

void LayerView::arrow (glm::vec2 movement)
{
    static float accumulator = 0.f;
    accumulator += dt_;

    glm::vec3 gl_Position_from = Rendering::manager().unProject(glm::vec2(0.f), scene.root()->transform_);
    glm::vec3 gl_Position_to   = Rendering::manager().unProject(glm::vec2(movement.x-movement.y, 0.f), scene.root()->transform_);
    glm::vec3 gl_delta = gl_Position_to - gl_Position_from;

    bool first = true;
    glm::vec3 delta_translation(0.f);
    for (auto it = Mixer::selection().begin(); it != Mixer::selection().end(); it++) {

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
                    dest_translation += glm::sign(gl_delta) * 0.21f;
                    dest_translation.x = ROUND(dest_translation.x, 10.f);
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
            info << "Depth " << std::fixed << std::setprecision(2) << (*it)->depth() << "  ";
            current_action_ = (*it)->name() + ": " + info.str();

            // delta for others to follow
            delta_translation = dest_translation - sourceNode->translation_;
        }
        else {
            // dest = current + delta from first
            dest_translation = sourceNode->translation_ + delta_translation;
        }

        // apply & request update
        setDepth( *it,  MAX( -dest_translation.x, 0.f) );

        first = false;
    }
}



void LayerView::updateSelectionOverlay(glm::vec4 color)
{
    View::updateSelectionOverlay(color);

    if (overlay_selection_->visible_) {
        // calculate bbox on selection
        GlmToolkit::AxisAlignedBoundingBox selection_box = BoundingBoxVisitor::AABB(Mixer::selection().getCopy(), this);
        overlay_selection_->scale_ = selection_box.scale();
        overlay_selection_->translation_ = selection_box.center();

        // slightly extend the boundary of the selection
        overlay_selection_frame_->scale_ = glm::vec3(1.f) + glm::vec3(0.07f, 0.07f, 1.f) / overlay_selection_->scale_;
    }
}
