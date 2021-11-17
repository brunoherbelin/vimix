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

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <string>
#include <sstream>
#include <regex>

#include "ImGuiToolkit.h"

#include "defines.h"
#include "Settings.h"
#include "Source.h"
#include "PickingVisitor.h"
#include "Decorations.h"
#include "Mixer.h"
#include "UserInterfaceManager.h"
#include "BoundingBoxVisitor.h"
#include "ActionManager.h"
#include "Log.h"

#include "View.h"

uint View::need_deep_update_ = 1;

View::View(Mode m) : mode_(m), dt_(16.f)
{
    show_context_menu_ = MENU_NONE;

    overlay_selection_ = nullptr;
    overlay_selection_frame_ = nullptr;
    overlay_selection_icon_ = nullptr;
}

void View::restoreSettings()
{
    scene.root()->scale_ = Settings::application.views[mode_].default_scale;
    scene.root()->translation_ = Settings::application.views[mode_].default_translation;
}

void View::saveSettings()
{
    Settings::application.views[mode_].default_scale = scene.root()->scale_;
    Settings::application.views[mode_].default_translation = scene.root()->translation_;
}

void View::draw()
{
    // draw scene of this view
    scene.root()->draw(glm::identity<glm::mat4>(), Rendering::manager().Projection());
}

void View::update(float dt)
{
    dt_ = dt;

    // recursive update from root of scene
    scene.update( dt_ );

    // a more complete update is requested
    if (View::need_deep_update_ > 0) {
        // reorder sources
        scene.ws()->sort();
    }
}


View::Cursor View::drag (glm::vec2 from, glm::vec2 to)
{
    static glm::vec3 start_translation = glm::vec3(0.f);
    static glm::vec2 start_position = glm::vec2(0.f);

    if ( start_position != from ) {
        start_position = from;
        start_translation = scene.root()->translation_;
    }

    // unproject
    glm::vec3 gl_Position_from = Rendering::manager().unProject(from);
    glm::vec3 gl_Position_to = Rendering::manager().unProject(to);

    // compute delta translation
    scene.root()->translation_ = start_translation + gl_Position_to - gl_Position_from;

    // apply and clamp
    zoom(0.f);

    return Cursor(Cursor_ResizeAll);
}

std::pair<Node *, glm::vec2> View::pick(glm::vec2 P)
{
    // prepare empty return value
    std::pair<Node *, glm::vec2> pick = { nullptr, glm::vec2(0.f) };

    // unproject mouse coordinate into scene coordinates
    glm::vec3 scene_point_ = Rendering::manager().unProject(P);

    // picking visitor traverses the scene
    PickingVisitor pv(scene_point_);
    scene.accept(pv);

    // picking visitor found nodes?
    if ( !pv.empty()) {
        // select top-most Node picked
        pick = pv.back();
    }

    return pick;
}

void View::initiate()
{
    current_action_ = "";
    for (auto sit = Mixer::manager().session()->begin();
         sit != Mixer::manager().session()->end(); ++sit)
        (*sit)->store(mode_);
}

void View::terminate()
{
    std::regex r("\\n");
    current_action_ = std::regex_replace(current_action_, r, " ");
    Action::manager().store(current_action_);
    current_action_ = "";
}

void View::zoom( float factor )
{
    resize( size() + int(factor * 2.f));
}

void View::recenter()
{
    // restore default view
    restoreSettings();

    // nothing else if scene is empty
    if (scene.ws()->numChildren() < 1)
        return;

    // calculate screen area visible in the default view
    GlmToolkit::AxisAlignedBoundingBox view_box;
    glm::mat4 modelview = GlmToolkit::transform(scene.root()->translation_, scene.root()->rotation_, scene.root()->scale_);
    view_box.extend( Rendering::manager().unProject(glm::vec2(0.f, Rendering::manager().mainWindow().height()), modelview) );
    view_box.extend( Rendering::manager().unProject(glm::vec2(Rendering::manager().mainWindow().width(), 0.f), modelview) );

    // calculate screen area required to see the entire scene
    BoundingBoxVisitor scene_visitor_bbox;
    scene.accept(scene_visitor_bbox);
    GlmToolkit::AxisAlignedBoundingBox scene_box = scene_visitor_bbox.bbox();

    // if the default view does not contains the entire scene
    // we shall adjust the view to fit the scene
    if ( !view_box.contains(scene_box)) {

        // drag view to move towards scene_box center (while remaining in limits of the view)
        glm::vec2 from = Rendering::manager().project(-view_box.center(), modelview);
        glm::vec2 to = Rendering::manager().project(-scene_box.center(), modelview);
        drag(from, to);

        // recalculate the view bounding box
        GlmToolkit::AxisAlignedBoundingBox updated_view_box;
        modelview = GlmToolkit::transform(scene.root()->translation_, scene.root()->rotation_, scene.root()->scale_);
        updated_view_box.extend( Rendering::manager().unProject(glm::vec2(0.f, Rendering::manager().mainWindow().height()), modelview) );
        updated_view_box.extend( Rendering::manager().unProject(glm::vec2(Rendering::manager().mainWindow().width(), 0.f), modelview) );

        // if the updated (translated) view does not contains the entire scene
        // we shall scale the view to fit the scene
        if ( !updated_view_box.contains(scene_box)) {

            glm::vec3 view_extend = updated_view_box.max() - updated_view_box.min();
            updated_view_box.extend(scene_box);
            glm::vec3 scene_extend = scene_box.max() - scene_box.min();
            glm::vec3 scale = view_extend  / scene_extend  ;
            float z = scene.root()->scale_.x;
            z = CLAMP( z * MIN(scale.x, scale.y), MIXING_MIN_SCALE, MIXING_MAX_SCALE);
            scene.root()->scale_.x = z;
            scene.root()->scale_.y = z;

        }
    }
}

void View::selectAll()
{
    Mixer::selection().clear();
    for(auto sit = Mixer::manager().session()->begin();
        sit != Mixer::manager().session()->end(); ++sit) {
        if (canSelect(*sit))
            Mixer::selection().add(*sit);
    }
    // special case of one single source in selection : make current after release
    if (Mixer::selection().size() == 1)
        Mixer::manager().setCurrentSource( Mixer::selection().front() );
}

void View::select(glm::vec2 A, glm::vec2 B)
{
    // unproject mouse coordinate into scene coordinates
    glm::vec3 scene_point_A = Rendering::manager().unProject(A);
    glm::vec3 scene_point_B = Rendering::manager().unProject(B);

    // picking visitor traverses the scene
    PickingVisitor pv(scene_point_A, scene_point_B);
    scene.accept(pv);

    // picking visitor found nodes in the area?
    if ( !pv.empty()) {

        // create a list of source matching the list of picked nodes
        SourceList selection;
        // loop over the nodes and add all sources found.
         for(std::vector< std::pair<Node *, glm::vec2> >::const_reverse_iterator p = pv.rbegin(); p != pv.rend(); ++p){
            Source *s = Mixer::manager().findSource( p->first );
            if (canSelect(s))
                selection.push_back( s );
        }
        // set the selection with list of picked (overlaped) sources
        Mixer::selection().set(selection);
    }
    else
        // reset selection
        Mixer::selection().clear();
}

bool View::canSelect(Source *s) {

    return ( s!=nullptr && !s->locked() );
}

void View::updateSelectionOverlay()
{
    // create first
    if (overlay_selection_ == nullptr) {
        overlay_selection_ = new Group;
        overlay_selection_icon_ = new Handles(Handles::MENU);
        overlay_selection_->attach(overlay_selection_icon_);
        overlay_selection_frame_ = new Frame(Frame::SHARP, Frame::LARGE, Frame::NONE);
        overlay_selection_->attach(overlay_selection_frame_);
        scene.fg()->attach(overlay_selection_);
    }

    // no overlay by default
    overlay_selection_->visible_ = false;

    // potential selection if more than 1 source selected
    if (Mixer::selection().size() > 1) {
        // show group overlay
        overlay_selection_->visible_ = true;
        ImVec4 c = ImGuiToolkit::HighlightColor();
        overlay_selection_frame_->color = glm::vec4(c.x, c.y, c.z, c.w * 0.8f);
        overlay_selection_icon_->color = glm::vec4(c.x, c.y, c.z, c.w);
    }
    // no selection: reset drawing selection overlay
    else
        overlay_selection_->scale_ = glm::vec3(0.f, 0.f, 1.f);
}


void View::lock(Source *s, bool on)
{
    s->setLocked(on);
    if (on)
        Action::manager().store(s->name() + std::string(": lock."));
    else
        Action::manager().store(s->name() + std::string(": unlock."));
}


glm::vec2 View::resolution() const
{
    const ImGuiIO& io = ImGui::GetIO();
    return glm::vec2(io.DisplaySize.x, io.DisplaySize.y);
}
