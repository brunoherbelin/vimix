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
#include <glm/gtx/component_wise.hpp>

#include "ImGuiToolkit.h"

#include "Mixer.h"
#include "defines.h"
#include "Source.h"
#include "Settings.h"
#include "PickingVisitor.h"
#include "DrawVisitor.h"
#include "Decorations.h"
#include "UserInterfaceManager.h"
#include "BoundingBoxVisitor.h"
#include "ActionManager.h"
#include "MousePointer.h"

#include "GeometryView.h"


GeometryView::GeometryView() : View(GEOMETRY)
{
    scene.root()->scale_ = glm::vec3(GEOMETRY_DEFAULT_SCALE, GEOMETRY_DEFAULT_SCALE, 1.0f);
    // read default settings
    if ( Settings::application.views[mode_].name.empty() )
        // no settings found: store application default
        saveSettings();
    else
        restoreSettings();
    Settings::application.views[mode_].name = "Geometry";

    // Geometry Scene foreground
    output_surface_ = new Surface;
    output_surface_->visible_ = false;
    scene.fg()->attach(output_surface_);
    Frame *border = new Frame(Frame::SHARP, Frame::THIN, Frame::NONE);
    border->color = glm::vec4( COLOR_FRAME, 1.f );
    scene.fg()->attach(border);

    // User interface foreground
    //
    // point to show POSITION
    overlay_position_ = new Symbol(Symbol::SQUARE_POINT);
    overlay_position_->scale_ = glm::vec3(0.5f, 0.5f, 1.f);
    scene.fg()->attach(overlay_position_);
    overlay_position_->visible_ = false;
    // cross to show the axis for POSITION
    overlay_position_cross_ = new Symbol(Symbol::GRID);
    overlay_position_cross_->scale_ = glm::vec3(0.5f, 0.5f, 1.f);
    scene.fg()->attach(overlay_position_cross_);
    overlay_position_cross_->visible_ = false;
    // 'clock' : tic marks every 10 degrees for ROTATION
    // with dark background
    overlay_rotation_clock_ = new Group;
    overlay_rotation_clock_tic_ = new Symbol(Symbol::CLOCK);
    overlay_rotation_clock_->attach(overlay_rotation_clock_tic_);
    Symbol *s = new Symbol(Symbol::CIRCLE_POINT);
    s->color = glm::vec4(0.f, 0.f, 0.f, 0.1f);
    s->scale_ = glm::vec3(28.f, 28.f, 1.f);
    s->translation_.z = -0.1;
    overlay_rotation_clock_->attach(s);
    overlay_rotation_clock_->scale_ = glm::vec3(0.25f, 0.25f, 1.f);
    scene.fg()->attach(overlay_rotation_clock_);
    overlay_rotation_clock_->visible_ = false;
    // circle to show fixed-size  ROTATION
    overlay_rotation_clock_hand_ = new Symbol(Symbol::CLOCK_H);
    overlay_rotation_clock_hand_->scale_ = glm::vec3(0.25f, 0.25f, 1.f);
    scene.fg()->attach(overlay_rotation_clock_hand_);
    overlay_rotation_clock_hand_->visible_ = false;
    overlay_rotation_fix_ = new Symbol(Symbol::SQUARE);
    overlay_rotation_fix_->scale_ = glm::vec3(0.25f, 0.25f, 1.f);
    scene.fg()->attach(overlay_rotation_fix_);
    overlay_rotation_fix_->visible_ = false;
    // circle to show the center of ROTATION
    overlay_rotation_ = new Symbol(Symbol::CIRCLE);
    overlay_rotation_->scale_ = glm::vec3(0.25f, 0.25f, 1.f);
    scene.fg()->attach(overlay_rotation_);
    overlay_rotation_->visible_ = false;
    // 'grid' : tic marks every 0.1 step for SCALING
    // with dark background
    Group *g = new Group;
    s = new Symbol(Symbol::GRID);
    s->scale_ = glm::vec3(1.655f, 1.655f, 1.f);
    g->attach(s);
    s = new Symbol(Symbol::SQUARE_POINT);
    s->color = glm::vec4(0.f, 0.f, 0.f, 0.1f);
    s->scale_ = glm::vec3(17.f, 17.f, 1.f);
    s->translation_.z = -0.1;
    g->attach(s);
    overlay_scaling_grid_ = g;
    overlay_scaling_grid_->scale_ = glm::vec3(0.3f, 0.3f, 1.f);
    scene.fg()->attach(overlay_scaling_grid_);
    overlay_scaling_grid_->visible_ = false;
    // cross in the square for proportional SCALING
    overlay_scaling_cross_ = new Symbol(Symbol::CROSS);
    overlay_scaling_cross_->scale_ = glm::vec3(0.3f, 0.3f, 1.f);
    scene.fg()->attach(overlay_scaling_cross_);
    overlay_scaling_cross_->visible_ = false;
    // square to show the center of SCALING
    overlay_scaling_ = new Symbol(Symbol::SQUARE);
    overlay_scaling_->scale_ = glm::vec3(0.3f, 0.3f, 1.f);
    scene.fg()->attach(overlay_scaling_);
    overlay_scaling_->visible_ = false;

    border = new Frame(Frame::SHARP, Frame::THIN, Frame::NONE);
    border->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 0.2f );
    overlay_crop_ = border;
    scene.fg()->attach(overlay_crop_);
    overlay_crop_->visible_ = false;

    // will be init later
    overlay_selection_scale_ = nullptr;
    overlay_selection_rotate_ = nullptr;
    overlay_selection_stored_status_ = nullptr;
    overlay_selection_active_ = false;

    // replace grid with appropriate one
    translation_grid_ = new TranslationGrid(scene.root());
    translation_grid_->root()->visible_ = false;
    rotation_grid_ = new RotationGrid(scene.root());
    rotation_grid_->root()->visible_ = false;
    if (grid) delete grid;
    grid = translation_grid_;
}

void GeometryView::update(float dt)
{
    View::update(dt);

    // a more complete update is requested
    if (View::need_deep_update_ > 0) {

        // update rendering of render frame
        FrameBuffer *output = Mixer::manager().session()->frame();
        if (output){
            float aspect_ratio = output->aspectRatio();
            for (NodeSet::iterator node = scene.bg()->begin(); node != scene.bg()->end(); ++node) {
                (*node)->scale_.x = aspect_ratio;
            }
            for (NodeSet::iterator node = scene.fg()->begin(); node != scene.fg()->end(); ++node) {
                (*node)->scale_.x = aspect_ratio;
            }
            output_surface_->setTextureIndex( output->texture() );

            // set grid aspect ratio
            if (Settings::application.proportional_grid)
                translation_grid_->setAspectRatio( output->aspectRatio() );
            else
                translation_grid_->setAspectRatio( 1.f );
        }

        // prevent invalid scaling
        float s = CLAMP(scene.root()->scale_.x, GEOMETRY_MIN_SCALE, GEOMETRY_MAX_SCALE);
        scene.root()->scale_.x = s;
        scene.root()->scale_.y = s;

        // change grid color
        const ImVec4 c = ImGuiToolkit::HighlightColor();
        translation_grid_->setColor( glm::vec4(c.x, c.y, c.z, 0.3) );
        rotation_grid_->setColor( glm::vec4(c.x, c.y, c.z, 0.3) );
    }

    // the current view is the geometry view
    if (Mixer::manager().view() == this )
    {
        ImVec4 c = ImGuiToolkit::HighlightColor();
        updateSelectionOverlay(glm::vec4(c.x, c.y, c.z, c.w));

//        overlay_selection_icon_->visible_ = false;
    }
}

void GeometryView::resize ( int scale )
{
    float z = CLAMP(0.01f * (float) scale, 0.f, 1.f);
    z *= z;
    z *= GEOMETRY_MAX_SCALE - GEOMETRY_MIN_SCALE;
    z += GEOMETRY_MIN_SCALE;
    scene.root()->scale_.x = z;
    scene.root()->scale_.y = z;

    // Clamp translation to acceptable area
    glm::vec3 border(2.f * Mixer::manager().session()->frame()->aspectRatio(), 2.f, 0.f);
    scene.root()->translation_ = glm::clamp(scene.root()->translation_, -border, border);
}

int  GeometryView::size ()
{
    float z = (scene.root()->scale_.x - GEOMETRY_MIN_SCALE) / (GEOMETRY_MAX_SCALE - GEOMETRY_MIN_SCALE);
    return (int) ( sqrt(z) * 100.f);
}

void GeometryView::draw()
{
    // work on the current source
    Source *s = Mixer::manager().currentSource();

    // hack to prevent source manipulation (scale and rotate)
    // when multiple sources are selected: temporarily change mode to 'selected'
    if (s != nullptr &&  Mixer::selection().size() > 1)
        s->setMode(Source::SELECTED);

    // Drawing of Geometry view is different as it renders
    // only sources in the current workspace
    std::vector<Node *> surfaces;
    std::vector<Node *> overlays;
    uint workspaces_counts_[Source::WORKSPACE_ANY+1] = {0};
    uint hidden_count_ = 0;
    for (auto source_iter = Mixer::manager().session()->begin();
         source_iter != Mixer::manager().session()->end(); ++source_iter) {
        // count if it is visible
        if (Settings::application.views[mode_].ignore_mix || (*source_iter)->visible()) {
            // if it is in the current workspace
            if (Settings::application.current_workspace == Source::WORKSPACE_ANY
                || (*source_iter)->workspace() == Settings::application.current_workspace) {
                // will draw its surface
                surfaces.push_back((*source_iter)->groups_[mode_]);
                // will draw its frame and locker icon
                overlays.push_back((*source_iter)->frames_[mode_]);
                overlays.push_back((*source_iter)->locker_);
            }
            // count number of sources per workspace
            workspaces_counts_[(*source_iter)->workspace()]++;
            workspaces_counts_[Source::WORKSPACE_ANY]++;
        }
        hidden_count_ += (*source_iter)->visible() ? 0 : 1;
    }

    // 0. prepare projection for draw visitors
    glm::mat4 projection = Rendering::manager().Projection();

    // 1. Draw surface of sources in the current workspace
    DrawVisitor draw_surfaces(surfaces, projection);
    scene.accept(draw_surfaces);

    // 2. Draw scene rendering on top (which includes rendering of all visible sources)
    DrawVisitor draw_rendering(output_surface_, projection, true);
    scene.accept(draw_rendering);

    // 3. Draw frames and icons of sources in the current workspace
    DrawVisitor draw_overlays(overlays, projection);
    scene.accept(draw_overlays);

    // 4. Draw control overlays of current source on top (if selected)
    if (s != nullptr &&
        (Settings::application.current_workspace == Source::WORKSPACE_ANY ||
                         s->workspace() == Settings::application.current_workspace) &&
        (Settings::application.views[mode_].ignore_mix || s->visible()))
    {
        DrawVisitor dv(s->overlays_[mode_], projection);
        scene.accept(dv);
        // Always restore current source after draw
        s->setMode(Source::CURRENT);
    }

    // 5. Finally, draw overlays of view
    DrawVisitor draw_foreground(scene.fg(), projection);
    scene.accept(draw_foreground);

    // 6. Display grid
    if (grid->active() && current_action_ongoing_) {
        DrawVisitor draw_grid(grid->root(), projection, true);
        scene.accept(draw_grid);
    }

    // display interface
    // Locate window at upper right corner
    glm::vec2 P = glm::vec2(-output_surface_->scale_.x - 0.02f, output_surface_->scale_.y + 0.01);
    P = Rendering::manager().project(glm::vec3(P, 0.f), scene.root()->transform_, false);
    // Set window position depending on icons size
    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
    ImGui::SetNextWindowPos(ImVec2(P.x, P.y - 1.5f * ImGui::GetFrameHeight() ), ImGuiCond_Always);
    if (ImGui::Begin("##GeometryViewOptions", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground
                     | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings
                     | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus ))
    {
        // style
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.14f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.24f, 0.24f, 0.24f, 0.46f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 0.56f));
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.14f, 0.14f, 0.14f, 0.9f));

        // toggle sources visibility flag
        std::string _label = std::to_string(hidden_count_) + " source" + (hidden_count_>1?"s ":" ")
                             + "outside mixing circle " + ICON_FA_MOON;
        ImGuiToolkit::ButtonIconToggle(12, 0, &Settings::application.views[mode_].ignore_mix, _label.c_str());

        // select layers visibility
        static std::vector< std::string > _tooltips = {
            {"Sources in Background layer"},
            {"Sources in Workspace layer"},
            {"Sources in Foreground layer"},
            {"Sources in all layers (total)"}
        };
        std::vector< std::tuple<int, int, std::string> > _workspaces = {
            {ICON_WORKSPACE_BACKGROUND, std::to_string( workspaces_counts_[Source::WORKSPACE_BACKGROUND] )},
            {ICON_WORKSPACE_CENTRAL,    std::to_string( workspaces_counts_[Source::WORKSPACE_CENTRAL] )},
            {ICON_WORKSPACE_FOREGROUND, std::to_string( workspaces_counts_[Source::WORKSPACE_FOREGROUND] )},
            {ICON_WORKSPACE,            std::to_string( workspaces_counts_[Source::WORKSPACE_ANY] )}
        };
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        ImGui::SetNextItemWidth( ImGui::GetTextLineHeightWithSpacing() * 2.6);
        if ( ImGuiToolkit::ComboIcon ("##WORKSPACE", &Settings::application.current_workspace, _workspaces, _tooltips) ){
            // need full update
            Mixer::manager().setView(mode_);
        }

        ImGui::PopStyleColor(6);
        ImGui::End();
    }
    ImGui::PopFont();

    // display popup menu source
    if (show_context_menu_ == MENU_SOURCE) {
        ImGui::OpenPopup( "GeometrySourceContextMenu" );
        show_context_menu_ = MENU_NONE;
    }
    if (ImGui::BeginPopup("GeometrySourceContextMenu")) {
        if (s != nullptr) {
            // CROP source manipulation mode
            if (s->manipulator_->active() > 0) {
                if (ImGui::MenuItem(ICON_FA_VECTOR_SQUARE "  Switch to Shape mode"))
                    s->manipulator_->setActive(0);
                ImGui::Separator();

                if (ImGui::MenuItem(ICON_FA_CROP_ALT "  Reset crop")) {
                    s->group(mode_)->crop_ = glm::vec4(-1.f, 1.f, 1.f, -1.f);
                    s->touch();
                    Action::manager().store(s->name() + std::string(": Reset crop"));
                }
                ImGui::Text(ICON_FA_ANGLE_LEFT);
                ImGui::SameLine(18);
                if (ImGui::MenuItem(ICON_FA_ANGLE_RIGHT "   Reset distortion")) {
                    s->group(mode_)->data_ = glm::zero<glm::mat4>();
                    s->touch();
                    Action::manager().store(s->name() + std::string(": Reset distortion"));
                }
            }
            // SHAPE source manipulation mode
            else {
                if (ImGui::MenuItem( ICON_FA_CROP_ALT "  Switch to Crop mode" ))
                    s->manipulator_->setActive(1);
                ImGui::Separator();

                if (ImGui::MenuItem( ICON_FA_VECTOR_SQUARE "  Reset shape" )){
                    s->group(mode_)->scale_ = glm::vec3(1.f);
                    s->group(mode_)->rotation_.z = 0;
                    s->touch();
                    Action::manager().store(s->name() + std::string(": Reset shape"));
                }
                if (ImGui::MenuItem( ICON_FA_CIRCLE_NOTCH "  Reset rotation" )){
                    s->group(mode_)->rotation_.z = 0;
                    s->touch();
                    Action::manager().store(s->name() + std::string(": Reset rotation"));
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem( ICON_FA_CROSSHAIRS "  Center" )){
                s->group(mode_)->translation_ = glm::vec3(0.f);
                s->touch();
                Action::manager().store(s->name() + std::string(": Center"));
            }
            if (ImGui::MenuItem( ICON_FA_EXPAND_ALT "  Restore aspect ratio" )){
                s->group(mode_)->scale_.x = s->group(mode_)->scale_.y;
                s->group(mode_)->scale_.x *= (s->group(mode_)->crop_[1] - s->group(mode_)->crop_[0]) /
                                             (s->group(mode_)->crop_[2] - s->group(mode_)->crop_[3]);
                s->touch();
                Action::manager().store(s->name() + std::string(": Restore aspect ratio"));
            }
            if (ImGui::MenuItem( ICON_FA_EXPAND "  Fit to output" )){
                s->group(mode_)->scale_ = glm::vec3(output_surface_->scale_.x/ s->frame()->aspectRatio(), 1.f, 1.f);
                s->group(mode_)->rotation_.z = 0;
                s->group(mode_)->translation_ = glm::vec3(0.f);
                s->touch();
                Action::manager().store(s->name() + std::string(": Geometry Fit output"));
            }
        }
        ImGui::EndPopup();
    }
    // display popup menu selection
    if (show_context_menu_ == MENU_SELECTION) {
        ImGui::OpenPopup( "GeometrySelectionContextMenu" );
        show_context_menu_ = MENU_NONE;
    }
    if (ImGui::BeginPopup("GeometrySelectionContextMenu")) {

        // colored context menu
        ImGui::PushStyleColor(ImGuiCol_Text, ImGuiToolkit::HighlightColor());
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(COLOR_MENU_HOVERED, 0.5f));

        // batch manipulation of sources in Geometry view
        if (ImGui::Selectable( ICON_FA_EXPAND "  Fit all" )){
            for (auto sit = Mixer::selection().begin(); sit != Mixer::selection().end(); ++sit){
                (*sit)->group(mode_)->scale_ = glm::vec3(output_surface_->scale_.x/ (*sit)->frame()->aspectRatio(), 1.f, 1.f);
                (*sit)->group(mode_)->rotation_.z = 0;
                (*sit)->group(mode_)->translation_ = glm::vec3(0.f);
                (*sit)->touch();
            }
            Action::manager().store(std::string("Selection: Fit all."));
        }
        if (ImGui::Selectable( ICON_FA_VECTOR_SQUARE "  Reset all" )){
            // apply to every sources in selection
            for (auto sit = Mixer::selection().begin(); sit != Mixer::selection().end(); ++sit){
                (*sit)->group(mode_)->scale_ = glm::vec3(1.f);
                (*sit)->group(mode_)->rotation_.z = 0;
                (*sit)->group(mode_)->crop_ = glm::vec4(-1.f, 1.f, 1.f, -1.f);
                (*sit)->group(mode_)->translation_ = glm::vec3(0.f);
                (*sit)->touch();
            }
            Action::manager().store(std::string("Selection: Reset all."));
        }
//        if (ImGui::Selectable( ICON_FA_TH "  Mosaic" )){ // TODO

//        }
        ImGui::Separator();
        // manipulation of selection
        if (ImGui::Selectable( ICON_FA_CROSSHAIRS "  Center" )){
            glm::mat4 T = glm::translate(glm::identity<glm::mat4>(), -overlay_selection_->translation_);
            initiate();
            applySelectionTransform(T);
            terminate();
            Action::manager().store(std::string("Selection: Center."));
        }
        if (ImGui::Selectable( ICON_FA_COMPASS "  Align" )){
            // apply to every sources in selection
            for (auto sit = Mixer::selection().begin(); sit != Mixer::selection().end(); ++sit){
                (*sit)->group(mode_)->rotation_.z = overlay_selection_->rotation_.z;
                (*sit)->touch();
            }
            Action::manager().store(std::string("Selection: Align."));
        }
        if (ImGui::Selectable( ICON_FA_OBJECT_GROUP "  Best Fit" )){
            glm::mat4 T = glm::translate(glm::identity<glm::mat4>(), -overlay_selection_->translation_);
            float factor = 1.f;
            float angle = -overlay_selection_->rotation_.z;
            if ( overlay_selection_->scale_.x < overlay_selection_->scale_.y) {
                factor *= output_surface_->scale_.x / overlay_selection_->scale_.y;
                angle += glm::pi<float>() / 2.f;
            }
            else {
                factor *= output_surface_->scale_.x / overlay_selection_->scale_.x;
            }
            glm::mat4 S = glm::scale(glm::identity<glm::mat4>(), glm::vec3(factor, factor, 1.f));
            glm::mat4 R = glm::rotate(glm::identity<glm::mat4>(), angle, glm::vec3(0.f, 0.f, 1.f) );
            glm::mat4 M = S * R * T;
            initiate();
            applySelectionTransform(M);
            terminate();
            Action::manager().store(std::string("Selection: Best Fit."));
        }
        if (ImGui::Selectable( ICON_FA_EXCHANGE_ALT "  Mirror" )){
            glm::mat4 T = glm::translate(glm::identity<glm::mat4>(), -overlay_selection_->translation_);
            glm::mat4 F = glm::scale(glm::identity<glm::mat4>(), glm::vec3(-1.f, 1.f, 1.f));
            glm::mat4 M = glm::inverse(T) * F * T;
            initiate();
            applySelectionTransform(M);
            terminate();
            Action::manager().store(std::string("Selection: Mirror."));
        }

        ImGui::PopStyleColor(2);
        ImGui::EndPopup();
    }
}

void GeometryView::adaptGridToSource(Source *s, Node *picked)
{
    // Reset by default
    rotation_grid_->root()->translation_ = glm::vec3(0.f);
    rotation_grid_->root()->scale_ = glm::vec3(1.f);
    translation_grid_->root()->translation_ = glm::vec3(0.f);
    translation_grid_->root()->rotation_.z  = 0.f;

    if (s != nullptr && picked != nullptr) {
        if (picked == s->handles_[mode_][Handles::ROTATE]) {
            // shift grid at center of source
            rotation_grid_->root()->translation_ = s->group(mode_)->translation_;
            rotation_grid_->root()->scale_.x = glm::length(
                        glm::vec2(s->frame()->aspectRatio() * s->group(mode_)->scale_.x,
                                  s->group(mode_)->scale_.y) );
            rotation_grid_->root()->scale_.y = rotation_grid_->root()->scale_.x;
            // Swap grid to rotation grid
            rotation_grid_->setActive( grid->active() );
            translation_grid_->setActive( false );
            grid = rotation_grid_;
            return;
        }
        else if ( picked == s->handles_[mode_][Handles::RESIZE] ||
                  picked == s->handles_[mode_][Handles::RESIZE_V] ||
                  picked == s->handles_[mode_][Handles::RESIZE_H] ){
            translation_grid_->root()->translation_ = glm::vec3(0.f);
            translation_grid_->root()->rotation_.z = s->group(mode_)->rotation_.z;
            // Swap grid to translation grid
            translation_grid_->setActive( grid->active() );
            rotation_grid_->setActive( false );
            grid = translation_grid_;
        }
        else if ( picked == s->handles_[mode_][Handles::SCALE] ||
                  picked == s->handles_[mode_][Handles::NODE_LOWER_LEFT] ||
                  picked == s->handles_[mode_][Handles::NODE_UPPER_LEFT] ||
                  picked == s->handles_[mode_][Handles::NODE_LOWER_RIGHT] ||
                  picked == s->handles_[mode_][Handles::NODE_UPPER_RIGHT] ||
                  picked == s->handles_[mode_][Handles::CROP_V] ||
                  picked == s->handles_[mode_][Handles::CROP_H] ){
            translation_grid_->root()->translation_ = s->group(mode_)->translation_;
            translation_grid_->root()->rotation_.z = s->group(mode_)->rotation_.z;
            // Swap grid to translation grid
            translation_grid_->setActive( grid->active() );
            rotation_grid_->setActive( false );
            grid = translation_grid_;
        }
    }
    else {
        // Default:
        // Grid in scene global coordinate
        translation_grid_->setActive( grid->active()  );
        rotation_grid_->setActive( false );
        grid = translation_grid_;
    }
}

std::pair<Node *, glm::vec2> GeometryView::pick(glm::vec2 P)
{
    // prepare empty return value
    std::pair<Node *, glm::vec2> pick = { nullptr, glm::vec2(0.f) };

    // unproject mouse coordinate into scene coordinates
    glm::vec3 scene_point_ = Rendering::manager().unProject(P);

    // picking visitor traverses the scene
    PickingVisitor pv(scene_point_, false);
    scene.accept(pv);

    // picking visitor found nodes?
    if ( !pv.empty() ) {
        // keep current source active if it is clicked
        Source *current = Mixer::manager().currentSource();
        if (current != nullptr) {
            if ((Settings::application.current_workspace < Source::WORKSPACE_ANY &&
                 current->workspace() != Settings::application.current_workspace) ||
                (!Settings::application.views[mode_].ignore_mix && !current->visible()) )
            {
                current = nullptr;
            }
            else {
                // find if the current source was picked
                auto itp = pv.rbegin();
                for (; itp != pv.rend(); ++itp){
                    // test if source contains this node
                    Source::hasNode is_in_source((*itp).first );
                    if ( is_in_source( current ) ){
                        // a node in the current source was clicked !
                        pick = *itp;
                        // adapt grid to prepare grab action
                        adaptGridToSource(current, pick.first);
                        break;
                    }
                }
                // not found: the current source was not clicked
                // OR the selection contains multiple sources and actions on single source are disabled
                if (itp == pv.rend() || Mixer::selection().size() > 1) {
                    current = nullptr;
                    pick = { nullptr, glm::vec2(0.f) };
                }
                // picking on the menu handle: show context menu
                else if ( pick.first == current->handles_[mode_][Handles::MENU] ) {
                    openContextMenu(MENU_SOURCE);
                }
                // picking on the crop handle : switch to shape manipulation mode
                else if (pick.first == current->handles_[mode_][Handles::EDIT_CROP]) {
                    current->manipulator_->setActive(0);
                    pick = { current->handles_[mode_][Handles::EDIT_SHAPE], glm::vec2(0.f) };
                }
                // picking on the shape handle : switch to crop manipulation mode
                else if (pick.first == current->handles_[mode_][Handles::EDIT_SHAPE]) {
                    current->manipulator_->setActive(1);
                    pick = { current->handles_[mode_][Handles::EDIT_CROP], glm::vec2(0.f) };
                }
                // pick on the lock icon; unlock source
                else if ( UserInterface::manager().ctrlModifier() && pick.first == current->lock_ ) {
                    lock(current, false);
                    pick = { current->locker_, pick.second };
//                    pick = { nullptr, glm::vec2(0.f) };
                }
                // pick on the open lock icon; lock source and cancel pick
                else if ( UserInterface::manager().ctrlModifier() && pick.first == current->unlock_ ) {
                    lock(current, true);
                    pick = { nullptr, glm::vec2(0.f) };
                }
                // pick a locked source ; cancel pick
                else if ( !UserInterface::manager().ctrlModifier() && current->locked() ) {
                    pick = { nullptr, glm::vec2(0.f) };
                }
            }
        }
        // the clicked source changed (not the current source)
        if (current == nullptr) {

            if (UserInterface::manager().ctrlModifier()) {

                // default to failed pick
                pick = { nullptr, glm::vec2(0.f) };

                // loop over all nodes picked to detect clic on locks
                for (auto itp = pv.rbegin(); itp != pv.rend(); ++itp){
                    // get if a source was picked
                    Source *s = Mixer::manager().findSource((*itp).first);
                    // lock icon of a source (not current) is picked : unlock
                    if ( s != nullptr && s->locked() && (*itp).first == s->lock_) {
                        lock(s, false);
                        pick = { s->locker_, (*itp).second };
                        break;
                    }
                }
            }
            // no lock icon picked, find what else was picked
            if ( pick.first == nullptr) {

                // loop over all nodes picked
                for (auto itp = pv.rbegin(); itp != pv.rend(); ++itp){
                    // get if a source was picked
                    Source *s = Mixer::manager().findSource((*itp).first);
                    // accept picked sources in current workspaces
                    if ( s!=nullptr &&
                        (Settings::application.current_workspace == Source::WORKSPACE_ANY ||
                        s->workspace() == Settings::application.current_workspace) &&
                        (Settings::application.views[mode_].ignore_mix || s->visible()) )
                    {
                        if ( !UserInterface::manager().ctrlModifier() ) {
                            // source is locked; can't move
                            if ( s->locked() )
                                continue;
                            // a non-locked source is picked (anywhere)
                            // not in an active selection? don't pick this one!
                            if ( Mixer::selection().size() > 1 && !Mixer::selection().contains(s) )
                                continue;
                        }
                        // yeah, pick this one
                        pick = { s->group(mode_),  (*itp).second };
                        break;
                    }
                    // not a source picked
                    else {
                        // picked on selection handles
                        if ( (*itp).first == overlay_selection_scale_ || (*itp).first == overlay_selection_rotate_ ) {
                            pick = (*itp);
                            // initiate selection manipulation
                            if (overlay_selection_stored_status_) {
                                overlay_selection_stored_status_->copyTransform(overlay_selection_);
                                overlay_selection_active_ = true;
                            }
                            break;
                        }
                        else if ( overlay_selection_icon_ != nullptr && (*itp).first == overlay_selection_icon_ ) {
                            pick = (*itp);
                            openContextMenu(MENU_SELECTION);
                            break;
                        }
                    }
                }
            }
        }
    }

    return pick;
}

bool GeometryView::canSelect(Source *s) {

    return ( s!=nullptr && View::canSelect(s) && s->ready() &&
            (Settings::application.views[mode_].ignore_mix || s->visible()) &&
            (Settings::application.current_workspace == Source::WORKSPACE_ANY || s->workspace() == Settings::application.current_workspace) );
}


void GeometryView::applySelectionTransform(glm::mat4 M)
{
    for (auto sit = Mixer::selection().begin(); sit != Mixer::selection().end(); ++sit){
        if ( (*sit)->locked() )
            continue;
        // recompute all from matrix transform
        glm::mat4 transform = M * (*sit)->stored_status_->transform_;
        glm::vec3 tra, rot, sca;
        GlmToolkit::inverse_transform(transform, tra, rot, sca);
        (*sit)->group(mode_)->translation_ = tra;
        (*sit)->group(mode_)->scale_ = sca;
        (*sit)->group(mode_)->rotation_ = rot;
        // will have to be updated
        (*sit)->touch();
    }
}

View::Cursor GeometryView::grab (Source *s, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2> pick)
{
    View::Cursor ret = Cursor();

    // grab coordinates in scene-View reference frame
    glm::vec3 scene_from = Rendering::manager().unProject(from, scene.root()->transform_);
    glm::vec3 scene_to   = Rendering::manager().unProject(to, scene.root()->transform_);

    // No source is given
    if (!s) {

        // possibly grabing the selection overlay handles
        if (overlay_selection_ && overlay_selection_active_ ) {

            // rotation center to selection position
            glm::mat4 T = glm::translate(glm::identity<glm::mat4>(), overlay_selection_stored_status_->translation_);
            glm::vec4 selection_from = glm::inverse(T) * glm::vec4( scene_from,  1.f );
            glm::vec4 selection_to   = glm::inverse(T) * glm::vec4( scene_to,  1.f );

            // calculate scaling of selection
            float factor = glm::length( glm::vec2( selection_to ) ) / glm::length( glm::vec2( selection_from ) );
            glm::mat4 S = glm::scale(glm::identity<glm::mat4>(), glm::vec3(factor, factor, 1.f));

            // if interaction with selection SCALING handle
            if (pick.first == overlay_selection_scale_) {

                // show manipulation overlay
                overlay_scaling_cross_->visible_ = true;
                overlay_scaling_grid_->visible_ = false;
                overlay_scaling_->visible_ = true;
                overlay_scaling_->translation_.x = overlay_selection_stored_status_->translation_.x;
                overlay_scaling_->translation_.y = overlay_selection_stored_status_->translation_.y;
                overlay_scaling_->rotation_.z = overlay_selection_stored_status_->rotation_.z;
                overlay_scaling_->update(0);
                overlay_scaling_cross_->copyTransform(overlay_scaling_);
                overlay_scaling_->color = overlay_selection_icon_->color;
                overlay_scaling_cross_->color = overlay_selection_icon_->color;

                //
                // Manipulate the scaling handle in the SCENE coordinates to apply grid snap
                //
                if ( grid->active() ) {
                    glm::vec3 handle = glm::vec3(1.f, -1.f, 0.f);
                    // Compute handle coordinates into SCENE reference frame
                    handle = overlay_selection_stored_status_->transform_ * glm::vec4( handle, 1.f );
                    // move the handle we hold by the mouse translation (in scene reference frame)
                    handle = glm::translate(glm::identity<glm::mat4>(), scene_to - scene_from) * glm::vec4( handle, 1.f );
                    // snap handle coordinates to grid (if active)
                    handle = grid->snap(handle);
                    // Compute handle coordinates back in SOURCE reference frame
                    handle = glm::inverse(overlay_selection_stored_status_->transform_) * glm::vec4( handle,  1.f );
                    // The scaling factor is computed by dividing new handle coordinates with the ones before transform
                    glm::vec3 handle_scaling = glm::vec3(handle) / glm::vec3(1.f, -1.f, 1.f);
                    S = glm::scale(glm::identity<glm::mat4>(), handle_scaling);
                }

                // apply to selection overlay
                glm::vec4 vec = S * glm::vec4( overlay_selection_stored_status_->scale_, 0.f );
                overlay_selection_->scale_ = glm::vec3(vec);

                // apply to selection sources
                // NB: complete transform matrix (right to left) : move to center, scale and move back
                glm::mat4 M = T * S * glm::inverse(T);
                applySelectionTransform(M);

                // store action in history
                current_action_ = "Scale selection";
                ret.type = Cursor_ResizeNWSE;
            }
            // if interaction with selection ROTATION handle
            else if (pick.first == overlay_selection_rotate_) {

                // show manipulation overlay
                overlay_rotation_->visible_ = true;
                overlay_rotation_->translation_.x = overlay_selection_stored_status_->translation_.x;
                overlay_rotation_->translation_.y = overlay_selection_stored_status_->translation_.y;
                overlay_rotation_->update(0);
                overlay_rotation_->color = overlay_selection_icon_->color;
                overlay_rotation_fix_->visible_ = false;
                overlay_rotation_fix_->copyTransform(overlay_rotation_);
                overlay_rotation_fix_->color = overlay_selection_icon_->color;

                // Swap grid to rotation, shifted at center of source
                rotation_grid_->setActive( grid->active() );
                translation_grid_->setActive( false );
                grid = rotation_grid_;
                grid->root()->translation_ = overlay_selection_stored_status_->translation_;

                // prepare variables
                const float diagonal = glm::length( glm::vec2(overlay_selection_stored_status_->scale_));
                glm::vec2 handle_polar = glm::vec2( diagonal, 0.f);

                // compute rotation angle
                float angle = glm::orientedAngle( glm::normalize(glm::vec2(selection_from)), glm::normalize(glm::vec2(selection_to)));
                handle_polar.y = overlay_selection_stored_status_->rotation_.z + angle;

                // compute scaling of diagonal to reach new coordinates
                handle_polar.x *= factor;

                // snap polar coordiantes (diagonal lenght, angle)
                if ( grid->active() ) {
                    handle_polar = glm::round( handle_polar / grid->step() ) * grid->step();
                    // prevent null size
                    handle_polar.x = glm::max( grid->step().x,  handle_polar.x );
                }

                // cancel scaling with SHIFT modifier key
                if (UserInterface::manager().shiftModifier()) {
                    overlay_rotation_fix_->visible_ = true;
                    handle_polar.x = 1.f;
                }
                else
                    handle_polar.x /= diagonal ;

                S = glm::scale(glm::identity<glm::mat4>(), glm::vec3(handle_polar.x, handle_polar.x, 1.f));

                // apply to selection overlay
                glm::vec4 vec = S * glm::vec4( overlay_selection_stored_status_->scale_, 0.f );
                overlay_selection_->scale_ = glm::vec3(vec);
                overlay_selection_->rotation_.z = handle_polar.y;

                // apply to selection sources
                // NB: complete transform matrix (right to left) : move to center, rotate, scale and move back
                angle = handle_polar.y - overlay_selection_stored_status_->rotation_.z;
                glm::mat4 R = glm::rotate(glm::identity<glm::mat4>(), angle, glm::vec3(0.f, 0.f, 1.f) );
                glm::mat4 M = T * S * R * glm::inverse(T);
                applySelectionTransform(M);

                // store action in history
                current_action_ = "Scale and rotate selection";
                ret.type = Cursor_Hand;
            }

        }

        // update cursor
        return ret;
    }

    Group *sourceNode = s->group(mode_); // groups_[View::GEOMETRY]

    // make sure matrix transform of stored status is updated
    s->stored_status_->update(0);

    // grab coordinates in source-root reference frame
    const glm::mat4 source_scale = glm::scale(glm::identity<glm::mat4>(),
                                              glm::vec3(1.f / s->frame()->aspectRatio(), 1.f, 1.f));
    const glm::mat4 scene_to_source_transform = source_scale * glm::inverse(s->stored_status_->transform_);
    const glm::mat4 source_to_scene_transform = glm::inverse(scene_to_source_transform);

    // which manipulation to perform?
    std::ostringstream info;
    if (pick.first)  {

        // which corner was picked ?
        glm::vec2 corner = glm::round(pick.second);

        // keep transform from source center to opposite corner
        const glm::mat4 source_to_corner_transform = glm::translate(glm::identity<glm::mat4>(), glm::vec3(corner, 0.f));

        // transformation from scene to corner:
        const glm::mat4 scene_to_corner_transform = source_to_corner_transform * scene_to_source_transform;
        const glm::mat4 corner_to_scene_transform = glm::inverse(scene_to_corner_transform);

        // picking on a Node
        if (pick.first == s->handles_[mode_][Handles::NODE_LOWER_LEFT]) {
            // hide other grips
            s->handles_[mode_][Handles::CROP_H]->visible_ = false;
            s->handles_[mode_][Handles::CROP_V]->visible_ = false;
            s->handles_[mode_][Handles::ROUNDING]->visible_ = false;
            s->handles_[mode_][Handles::MENU]->visible_ = false;
            s->handles_[mode_][Handles::EDIT_CROP]->visible_ = false;
            // get stored status
            glm::vec3 node_pos = glm::vec3(s->stored_status_->data_[0].x,
                                           s->stored_status_->data_[0].y,
                                           0.f);
            // Compute target coordinates of manipulated handle into SCENE reference frame
            node_pos = source_to_scene_transform * glm::vec4(node_pos, 1.f);
            // apply translation of target in SCENE
            node_pos = glm::translate(glm::identity<glm::mat4>(), scene_to - scene_from)  * glm::vec4(node_pos, 1.f);
            // snap handle coordinates to grid (if active)
            if ( grid->active() )
                node_pos = grid->snap(node_pos);
            // Compute handle coordinates back in SOURCE reference frame
            node_pos = scene_to_source_transform * glm::vec4(node_pos, 1.f);
            // Diagonal SCALING with SHIFT
            if (UserInterface::manager().shiftModifier())
                node_pos.y = node_pos.x;
            // apply to source Node and to handles
            sourceNode->data_[0].x = CLAMP( node_pos.x, 0.f, 0.96f );
            sourceNode->data_[0].y = CLAMP( node_pos.y, 0.f, 0.96f );
        }
        else if (pick.first == s->handles_[mode_][Handles::NODE_UPPER_LEFT]) {
            // hide other grips
            s->handles_[mode_][Handles::CROP_H]->visible_ = false;
            s->handles_[mode_][Handles::CROP_V]->visible_ = false;
            s->handles_[mode_][Handles::ROUNDING]->visible_ = false;
            s->handles_[mode_][Handles::MENU]->visible_ = false;
            s->handles_[mode_][Handles::EDIT_CROP]->visible_ = false;
            // get stored status
            glm::vec3 node_pos = glm::vec3(s->stored_status_->data_[1].x,
                                           s->stored_status_->data_[1].y,
                                           0.f);
            // Compute target coordinates of manipulated handle into SCENE reference frame
            node_pos = source_to_scene_transform * glm::vec4(node_pos, 1.f);
            // apply translation of target in SCENE
            node_pos = glm::translate(glm::identity<glm::mat4>(), scene_to - scene_from)  * glm::vec4(node_pos, 1.f);
            // snap handle coordinates to grid (if active)
            if ( grid->active() )
                node_pos = grid->snap(node_pos);
            // Compute handle coordinates back in SOURCE reference frame
            node_pos = scene_to_source_transform * glm::vec4(node_pos, 1.f);
            // Diagonal SCALING with SHIFT
            if (UserInterface::manager().shiftModifier())
                node_pos.y = node_pos.x;
            // apply to source Node and to handles
            sourceNode->data_[1].x = CLAMP( node_pos.x, 0.f, 0.96f );
            sourceNode->data_[1].y = CLAMP( node_pos.y, -0.96f, 0.f );
        }
        else if ( pick.first == s->handles_[mode_][Handles::NODE_LOWER_RIGHT] ) {
            // hide other grips
            s->handles_[mode_][Handles::CROP_H]->visible_ = false;
            s->handles_[mode_][Handles::CROP_V]->visible_ = false;
            s->handles_[mode_][Handles::ROUNDING]->visible_ = false;
            s->handles_[mode_][Handles::MENU]->visible_ = false;
            s->handles_[mode_][Handles::EDIT_CROP]->visible_ = false;
            // get stored status
            glm::vec3 node_pos = glm::vec3(s->stored_status_->data_[2].x,
                                           s->stored_status_->data_[2].y,
                                           0.f);
            // Compute target coordinates of manipulated handle into SCENE reference frame
            node_pos = source_to_scene_transform * glm::vec4(node_pos, 1.f);
            // apply translation of target in SCENE
            node_pos = glm::translate(glm::identity<glm::mat4>(), scene_to - scene_from)  * glm::vec4(node_pos, 1.f);
            // snap handle coordinates to grid (if active)
            if ( grid->active() )
                node_pos = grid->snap(node_pos);
            // Compute handle coordinates back in SOURCE reference frame
            node_pos = scene_to_source_transform * glm::vec4(node_pos, 1.f);            
            // Diagonal SCALING with SHIFT
            if (UserInterface::manager().shiftModifier())
                node_pos.y = node_pos.x;
            // apply to source Node and to handles
            sourceNode->data_[2].x = CLAMP( node_pos.x, -0.96f, 0.f );
            sourceNode->data_[2].y = CLAMP( node_pos.y, 0.f, 0.96f );
        }
        else if (pick.first == s->handles_[mode_][Handles::NODE_UPPER_RIGHT]) {
            // hide other grips
            s->handles_[mode_][Handles::CROP_H]->visible_ = false;
            s->handles_[mode_][Handles::CROP_V]->visible_ = false;
            s->handles_[mode_][Handles::ROUNDING]->visible_ = false;
            s->handles_[mode_][Handles::MENU]->visible_ = false;
            s->handles_[mode_][Handles::EDIT_CROP]->visible_ = false;

            // get stored status
            glm::vec3 node_pos = glm::vec3(s->stored_status_->data_[3].x,
                                           s->stored_status_->data_[3].y,
                                           0.f);
            // Compute target coordinates of manipulated handle into SCENE reference frame
            node_pos = source_to_scene_transform * glm::vec4(node_pos, 1.f);
            // apply translation of target in SCENE
            node_pos = glm::translate(glm::identity<glm::mat4>(), scene_to - scene_from)  * glm::vec4(node_pos, 1.f);
            // snap handle coordinates to grid (if active)
            if ( grid->active() )
                node_pos = grid->snap(node_pos);
            // Compute handle coordinates back in SOURCE reference frame
            node_pos = scene_to_source_transform * glm::vec4(node_pos, 1.f);
            // Diagonal SCALING with SHIFT
            if (UserInterface::manager().shiftModifier())
                node_pos.y = node_pos.x;
            // apply to source Node and to handles
            sourceNode->data_[3].x = CLAMP( node_pos.x, -0.96f, 0.f );
            sourceNode->data_[3].y = CLAMP( node_pos.y, -0.96f, 0.f );
        }
        // horizontal CROP
        else if (pick.first == s->handles_[mode_][Handles::CROP_H]) {
            // hide all other grips
            s->handles_[mode_][Handles::NODE_LOWER_RIGHT]->visible_ = false;
            s->handles_[mode_][Handles::NODE_LOWER_LEFT]->visible_ = false;
            s->handles_[mode_][Handles::NODE_UPPER_RIGHT]->visible_ = false;
            s->handles_[mode_][Handles::NODE_UPPER_LEFT]->visible_ = false;
            s->handles_[mode_][Handles::ROUNDING]->visible_ = false;
            s->handles_[mode_][Handles::CROP_V]->visible_ = false;
            s->handles_[mode_][Handles::MENU]->visible_ = false;
            s->handles_[mode_][Handles::EDIT_CROP]->visible_ = false;

            // prepare overlay frame showing full image size
            glm::vec3 _c_s = glm::vec3(s->stored_status_->crop_[0]
                                                       - s->stored_status_->crop_[1],
                                                   s->stored_status_->crop_[3]
                                                       - s->stored_status_->crop_[2],
                                                   2.f)
                                         * 0.5f;
            overlay_crop_->scale_ = s->stored_status_->scale_ / _c_s;
            overlay_crop_->scale_.x *= s->frame()->aspectRatio();
            overlay_crop_->rotation_.z = s->stored_status_->rotation_.z;
            overlay_crop_->translation_ = s->stored_status_->translation_;
            glm::vec3 _t((s->stored_status_->crop_[1] + _c_s.x) * overlay_crop_->scale_.x,
                         -s->stored_status_->crop_[3] + _c_s.y, 0.f);
            _t = glm::rotate(glm::identity<glm::mat4>(),
                             overlay_crop_->rotation_.z,
                             glm::vec3(0.f, 0.f, 1.f))
                 * glm::vec4(_t, 1.f);
            overlay_crop_->translation_ += _t;
            overlay_crop_->translation_.z = 0.f;
            overlay_crop_->update(0);
            overlay_crop_->visible_ = true;
            
            //
            // Manipulate the handle in the SCENE coordinates to apply grid snap
            //
            glm::vec4 handle = corner_to_scene_transform * glm::vec4(corner * 2.f, 0.f, 1.f );
            // move the corner we hold by the mouse translation (in scene reference frame)
            handle = glm::translate(glm::identity<glm::mat4>(), scene_to - scene_from) * handle;
            // snap handle coordinates to grid (if active)
            if ( grid->active() )
                handle = grid->snap(handle);

            // Compute coordinates coordinates back in CORNER reference frame
            handle = scene_to_corner_transform * handle;
            // The scaling factor is computed by dividing new handle coordinates with the ones before transform
            glm::vec2 handle_scaling = glm::vec2(handle.x, 1.f) / glm::vec2(corner.x * 2.f, 1.f);

            // Apply transform to the CROP
            if (corner.x > 0.f) {
                // RIGHT SIDE ; clamp between 0.1 and 1
                sourceNode->crop_[1] = CLAMP(s->stored_status_->crop_[0]
                                                 + (s->stored_status_->crop_[1]
                                                    - s->stored_status_->crop_[0])
                                                       * handle_scaling.x,
                                             0.1f,
                                             1.f);
            } else {
                // LEFT SIDE : clamp between -1 and -0.1
                sourceNode->crop_[0] = CLAMP(s->stored_status_->crop_[1]
                                                 - (s->stored_status_->crop_[1]
                                                    - s->stored_status_->crop_[0])
                                                       * handle_scaling.x,
                                             -1.f,
                                             -0.1f);
            }

            // get back the horizontal scaling after clamping of cropped coordinates
            handle_scaling.x = (sourceNode->crop_[1] - sourceNode->crop_[0])
                               / (s->stored_status_->crop_[1] - s->stored_status_->crop_[0]);

            //
            // Adjust scale and translation
            //
            // The center of the source in CORNER reference frame
            glm::vec4 corner_center = glm::vec4(corner, 0.f, 1.f);
            // scale center of source in CORNER reference frame
            corner_center = glm::scale(glm::identity<glm::mat4>(), glm::vec3(handle_scaling, 1.f))
                            * corner_center;
            // convert center back into scene reference frame
            corner_center = corner_to_scene_transform * corner_center;

            // Apply translation to the source
            sourceNode->translation_ = glm::vec3(corner_center);
            sourceNode->scale_ = s->stored_status_->scale_ * glm::vec3(handle_scaling, 1.f);

            // show cursor depending on angle
            float c = tan(sourceNode->rotation_.z);
            ret.type = ABS(c) > 1.f ? Cursor_ResizeNS : Cursor_ResizeEW;
            info << "Crop H " << std::fixed << std::setprecision(3) << sourceNode->crop_[0];
            info << " x " << sourceNode->crop_[1];
        }
        // Vertical CROP
        else if (pick.first == s->handles_[mode_][Handles::CROP_V]) {
            // hide all other grips
            s->handles_[mode_][Handles::NODE_LOWER_RIGHT]->visible_ = false;
            s->handles_[mode_][Handles::NODE_LOWER_LEFT]->visible_ = false;
            s->handles_[mode_][Handles::NODE_UPPER_RIGHT]->visible_ = false;
            s->handles_[mode_][Handles::NODE_UPPER_LEFT]->visible_ = false;
            s->handles_[mode_][Handles::ROUNDING]->visible_ = false;
            s->handles_[mode_][Handles::CROP_H]->visible_ = false;
            s->handles_[mode_][Handles::MENU]->visible_ = false;
            s->handles_[mode_][Handles::EDIT_CROP]->visible_ = false;

            // prepare overlay frame showing full image size
            glm::vec3 _c_s = glm::vec3(s->stored_status_->crop_[0]
                                           - s->stored_status_->crop_[1],
                                       s->stored_status_->crop_[3]
                                           - s->stored_status_->crop_[2],
                                       2.f)
                             * 0.5f;
            overlay_crop_->scale_ = s->stored_status_->scale_ / _c_s;
            overlay_crop_->scale_.x *= s->frame()->aspectRatio();
            overlay_crop_->rotation_.z = s->stored_status_->rotation_.z;
            overlay_crop_->translation_ = s->stored_status_->translation_;
            glm::vec3 _t((s->stored_status_->crop_[1] + _c_s.x) * overlay_crop_->scale_.x,
                         -s->stored_status_->crop_[3] + _c_s.y, 0.f);
            _t = glm::rotate(glm::identity<glm::mat4>(),
                             overlay_crop_->rotation_.z,
                             glm::vec3(0.f, 0.f, 1.f))
                 * glm::vec4(_t, 1.f);
            overlay_crop_->translation_ += _t;
            overlay_crop_->translation_.z = 0.f;
            overlay_crop_->update(0);
            overlay_crop_->visible_ = true;

            //
            // Manipulate the handle in the SCENE coordinates to apply grid snap
            //
            glm::vec4 handle = corner_to_scene_transform * glm::vec4(corner * 2.f, 0.f, 1.f );
            // move the corner we hold by the mouse translation (in scene reference frame)
            handle = glm::translate(glm::identity<glm::mat4>(), scene_to - scene_from) * handle;
            // snap handle coordinates to grid (if active)
            if ( grid->active() )
                handle = grid->snap(handle);

            // Compute coordinates coordinates back in CORNER reference frame
            handle = scene_to_corner_transform * handle;
            // The scaling factor is computed by dividing new handle coordinates with the ones before transform
            glm::vec2 handle_scaling = glm::vec2(1.f, handle.y) / glm::vec2(1.f, corner.y * 2.f);

            // Apply transform to the CROP
            if (corner.y > 0.f) {
                // TOP SIDE ; clamp between 0.1 and 1
                sourceNode->crop_[2] = CLAMP(s->stored_status_->crop_[3]
                                                 + (s->stored_status_->crop_[2]
                                                    - s->stored_status_->crop_[3])
                                                       * handle_scaling.y,
                                             0.1f,
                                             1.f);
            } else {
                // BOTTON SIDE : clamp between -1 and -0.1
                sourceNode->crop_[3] = CLAMP(s->stored_status_->crop_[2]
                                                 - (s->stored_status_->crop_[2]
                                                    - s->stored_status_->crop_[3])
                                                       * handle_scaling.y,
                                             -1.f,
                                             -0.1f);
            }

            // get back the horizontal scaling after clamping of cropped coordinates
            handle_scaling.y = (sourceNode->crop_[2] - sourceNode->crop_[3])
                               / (s->stored_status_->crop_[2] - s->stored_status_->crop_[3]);

            //
            // Adjust scale and translation
            //
            // The center of the source in CORNER reference frame
            glm::vec4 corner_center = glm::vec4(corner, 0.f, 1.f);
            // scale center of source in CORNER reference frame
            corner_center = glm::scale(glm::identity<glm::mat4>(), glm::vec3(handle_scaling, 1.f))
                            * corner_center;
            // convert center back into scene reference frame
            corner_center = corner_to_scene_transform * corner_center;

            // Apply translation to the source
            sourceNode->translation_ = glm::vec3(corner_center);
            sourceNode->scale_ = s->stored_status_->scale_ * glm::vec3(handle_scaling, 1.f);

            // show cursor depending on angle
            float c = tan(sourceNode->rotation_.z);
            ret.type = ABS(c) > 1.f ? Cursor_ResizeEW : Cursor_ResizeNS;
            info << "Crop V " << std::fixed << std::setprecision(3) << sourceNode->crop_[2];
            info << " x "  << sourceNode->crop_[3];
        }
        // pick the corner rounding handle
        else if (pick.first == s->handles_[mode_][Handles::ROUNDING]) {
            // hide other grips
            s->handles_[mode_][Handles::CROP_H]->visible_ = false;
            s->handles_[mode_][Handles::CROP_V]->visible_ = false;
            s->handles_[mode_][Handles::MENU]->visible_ = false;
            s->handles_[mode_][Handles::EDIT_CROP]->visible_ = false;
            // get stored status
//            glm::vec3 node_pos = glm::vec3( -s->stored_status_->data_[0].z, 0.f, 0.f);
            glm::vec3 node_pos = glm::vec3( -s->stored_status_->data_[0].w, 0.f, 0.f);
            // Compute target coordinates of manipulated handle into SCENE reference frame
            node_pos = source_to_scene_transform * glm::vec4(node_pos, 1.f);
            // apply translation of target in SCENE
            node_pos = glm::translate(glm::identity<glm::mat4>(), scene_to - scene_from) * glm::vec4(node_pos, 1.f);
            // Compute handle coordinates back in SOURCE reference frame
            node_pos = scene_to_source_transform * glm::vec4(node_pos, 1.f);

            // apply to source Node and to handles
//            sourceNode->data_[0].z = - CLAMP( node_pos.x, -1.f, 0.f );
            sourceNode->data_[0].w = - CLAMP( node_pos.x, -1.f, 0.f );
        }
        // picking on the resizing handles in the corners RESIZE CORNER
        else if ( pick.first == s->handles_[mode_][Handles::RESIZE] ) {
            // hide  other grips
            s->handles_[mode_][Handles::SCALE]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_H]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_V]->visible_ = false;
            s->handles_[mode_][Handles::ROTATE]->visible_ = false;
            s->handles_[mode_][Handles::EDIT_SHAPE]->visible_ = false;
            s->handles_[mode_][Handles::MENU]->visible_ = false;
            // inform on which corner should be overlayed (opposite)
            s->handles_[mode_][Handles::RESIZE]->overlayActiveCorner(-corner);

            //
            // Manipulate the handle in the SCENE coordinates to apply grid snap
            //
            glm::vec4 handle = corner_to_scene_transform * glm::vec4(corner * 2.f, 0.f, 1.f );
            // move the corner we hold by the mouse translation (in scene reference frame)
            handle = glm::translate(glm::identity<glm::mat4>(), scene_to - scene_from) * handle;
            // snap handle coordinates to grid (if active)
            if ( grid->active() )
                handle = grid->snap(handle);
            // Compute handleNODE_UPPER_RIGHT coordinates back in CORNER reference frame
            handle = scene_to_corner_transform * handle;
            // The scaling factor is computed by dividing new handle coordinates with the ones before transform
            glm::vec2 corner_scaling = glm::vec2(handle) / glm::vec2(corner * 2.f);

            // proportional SCALING with SHIFT
            if (UserInterface::manager().shiftModifier()) {
                corner_scaling = glm::vec2(glm::compMax(corner_scaling));
            }

            // Apply scaling to the source
            sourceNode->scale_ = s->stored_status_->scale_ * glm::vec3(corner_scaling, 1.f);

            //
            // Adjust translation
            //
            // The center of the source in CORNER reference frame
            glm::vec4 corner_center = glm::vec4( corner, 0.f, 1.f);
            // scale center of source in CORNER reference frame
            corner_center = glm::scale(glm::identity<glm::mat4>(), glm::vec3(corner_scaling, 1.f)) * corner_center;
            // convert center back into scene reference frame
            corner_center = corner_to_scene_transform * corner_center;
            // Apply scaling to the source
            sourceNode->translation_ = glm::vec3(corner_center);

            // show cursor depending on diagonal (corner picked)
            glm::mat4 T = glm::rotate(glm::identity<glm::mat4>(), s->stored_status_->rotation_.z, glm::vec3(0.f, 0.f, 1.f));
            T = glm::scale(T, s->stored_status_->scale_);
            corner = T * glm::vec4( corner, 0.f, 0.f );
            ret.type = corner.x * corner.y > 0.f ? Cursor_ResizeNESW : Cursor_ResizeNWSE;
            info << "Size " << std::fixed << std::setprecision(3) << sourceNode->scale_.x;
            info << " x "  << sourceNode->scale_.y;
        }
        // picking on the BORDER RESIZING handles left or right
        else if ( pick.first == s->handles_[mode_][Handles::RESIZE_H] ) {

            // hide all other grips
            s->handles_[mode_][Handles::RESIZE]->visible_ = false;
            s->handles_[mode_][Handles::SCALE]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_V]->visible_ = false;
            s->handles_[mode_][Handles::ROTATE]->visible_ = false;
            s->handles_[mode_][Handles::EDIT_SHAPE]->visible_ = false;
            s->handles_[mode_][Handles::MENU]->visible_ = false;
            // inform on which corner should be overlayed (opposite)
            s->handles_[mode_][Handles::RESIZE_H]->overlayActiveCorner(-corner);

            //
            // Manipulate the handle in the SCENE coordinates to apply grid snap
            //
            glm::vec4 handle = corner_to_scene_transform * glm::vec4(corner * 2.f, 0.f, 1.f );
            // move the corner we hold by the mouse translation (in scene reference frame)
            handle = glm::translate(glm::identity<glm::mat4>(), scene_to - scene_from) * handle;
            // snap handle coordinates to grid (if active)
            if ( grid->active() )
                handle = grid->snap(handle);
            // Compute coordinates coordinates back in CORNER reference frame
            handle = scene_to_corner_transform * handle;
            // The scaling factor is computed by dividing new handle coordinates with the ones before transform
            glm::vec2 corner_scaling =  glm::vec2(handle.x, 1.f) / glm::vec2(corner.x * 2.f, 1.f);

            // Apply scaling to the source
            sourceNode->scale_ = s->stored_status_->scale_ * glm::vec3(corner_scaling, 1.f);

            // SHIFT: restore previous aspect ratio
            if (UserInterface::manager().shiftModifier()) {
                float ar = s->stored_status_->scale_.y / s->stored_status_->scale_.x;
                sourceNode->scale_.y = ar * sourceNode->scale_.x;
            }

            //
            // Adjust translation
            //
            // The center of the source in CORNER reference frame
            glm::vec4 corner_center = glm::vec4( corner, 0.f, 1.f);
            // scale center of source in CORNER reference frame
            corner_center = glm::scale(glm::identity<glm::mat4>(), glm::vec3(corner_scaling, 1.f)) * corner_center;
            // convert center back into scene reference frame
            corner_center = corner_to_scene_transform * corner_center;
            // Apply scaling to the source
            sourceNode->translation_ = glm::vec3(corner_center);

            // show cursor depending on angle
            float c = tan(sourceNode->rotation_.z);
            ret.type = ABS(c) > 1.f ? Cursor_ResizeNS : Cursor_ResizeEW;
            info << "Size " << std::fixed << std::setprecision(3) << sourceNode->scale_.x;
            info << " x "  << sourceNode->scale_.y;
        }
        // picking on the BORDER RESIZING handles top or bottom
        else if ( pick.first == s->handles_[mode_][Handles::RESIZE_V] ) {

            // hide all other grips
            s->handles_[mode_][Handles::RESIZE]->visible_ = false;
            s->handles_[mode_][Handles::SCALE]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_H]->visible_ = false;
            s->handles_[mode_][Handles::ROTATE]->visible_ = false;
            s->handles_[mode_][Handles::EDIT_SHAPE]->visible_ = false;
            s->handles_[mode_][Handles::MENU]->visible_ = false;
            // inform on which corner should be overlayed (opposite)
            s->handles_[mode_][Handles::RESIZE_V]->overlayActiveCorner(-corner);

            //
            // Manipulate the handle in the SCENE coordinates to apply grid snap
            //
            glm::vec4 handle = corner_to_scene_transform * glm::vec4(corner * 2.f, 0.f, 1.f );
            // move the corner we hold by the mouse translation (in scene reference frame)
            handle = glm::translate(glm::identity<glm::mat4>(), scene_to - scene_from) * handle;
            // snap handle coordinates to grid (if active)
            if ( grid->active() )
                handle = grid->snap(handle);
            // Compute coordinates coordinates back in CORNER reference frame
            handle = scene_to_corner_transform * handle;
            // The scaling factor is computed by dividing new handle coordinates with the ones before transform
            glm::vec2 corner_scaling =  glm::vec2(1.f, handle.y) / glm::vec2(1.f, corner.y * 2.f);

            // Apply scaling to the source
            sourceNode->scale_ = s->stored_status_->scale_ * glm::vec3(corner_scaling, 1.f);

            // SHIFT: restore previous aspect ratio
            if (UserInterface::manager().shiftModifier()) {
                float ar = s->stored_status_->scale_.x / s->stored_status_->scale_.y;
                sourceNode->scale_.x = ar * sourceNode->scale_.y;
            }

            //
            // Adjust translation
            //
            // The center of the source in CORNER reference frame
            glm::vec4 corner_center = glm::vec4( corner, 0.f, 1.f);
            // scale center of source in CORNER reference frame
            corner_center = glm::scale(glm::identity<glm::mat4>(), glm::vec3(corner_scaling, 1.f)) * corner_center;
            // convert center back into scene reference frame
            corner_center = corner_to_scene_transform * corner_center;
            // Apply scaling to the source
            sourceNode->translation_ = glm::vec3(corner_center);

            // show cursor depending on angle
            float c = tan(sourceNode->rotation_.z);
            ret.type = ABS(c) > 1.f ? Cursor_ResizeEW : Cursor_ResizeNS;
            info << "Size " << std::fixed << std::setprecision(3) << sourceNode->scale_.x;
            info << " x "  << sourceNode->scale_.y;
        }
        // picking on the CENTRER SCALING handle
        else if ( pick.first == s->handles_[mode_][Handles::SCALE] ) {

            // hide all other grips
            s->handles_[mode_][Handles::RESIZE]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_H]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_V]->visible_ = false;
            s->handles_[mode_][Handles::ROTATE]->visible_ = false;
            s->handles_[mode_][Handles::EDIT_SHAPE]->visible_ = false;
            s->handles_[mode_][Handles::MENU]->visible_ = false;
            // prepare overlay
            overlay_scaling_cross_->visible_ = false;
            overlay_scaling_grid_->visible_ = false;
            overlay_scaling_->visible_ = true;
            overlay_scaling_->translation_.x = s->stored_status_->translation_.x;
            overlay_scaling_->translation_.y = s->stored_status_->translation_.y;
            overlay_scaling_->rotation_.z = s->stored_status_->rotation_.z;
            overlay_scaling_->update(0);

            //
            // Manipulate the scaling handle in the SCENE coordinates to apply grid snap
            //
            glm::vec3 handle = glm::vec3( glm::round(pick.second), 0.f);
            // Compute handle coordinates into SCENE reference frame
            handle = source_to_scene_transform * glm::vec4( handle, 1.f );
            // move the handle we hold by the mouse translation (in scene reference frame)
            handle = glm::translate(glm::identity<glm::mat4>(), scene_to - scene_from) * glm::vec4( handle, 1.f );
            // snap handle coordinates to grid (if active)
            if ( grid->active() )
                handle = grid->snap(handle);
            // Compute handle coordinates back in SOURCE reference frame
            handle = scene_to_source_transform * glm::vec4( handle,  1.f );
            // The scaling factor is computed by dividing new handle coordinates with the ones before transform
            glm::vec2 handle_scaling = glm::vec2(handle) / glm::round(pick.second);

            // proportional SCALING with SHIFT
            if (UserInterface::manager().shiftModifier()) {
                handle_scaling = glm::vec2(glm::compMax(handle_scaling));
                overlay_scaling_cross_->visible_ = true;
                overlay_scaling_cross_->copyTransform(overlay_scaling_);
            }
            // Apply scaling to the source
            sourceNode->scale_ = s->stored_status_->scale_ * glm::vec3(handle_scaling, 1.f);

            // show cursor depending on diagonal
            corner = glm::sign(sourceNode->scale_);
            ret.type = (corner.x * corner.y) > 0.f ? Cursor_ResizeNWSE : Cursor_ResizeNESW;
            info << "Size " << std::fixed << std::setprecision(3) << sourceNode->scale_.x;
            info << " x "  << sourceNode->scale_.y;
        }
        // picking on the rotating handle
        else if ( pick.first == s->handles_[mode_][Handles::ROTATE] ) {

            // hide all other grips
            s->handles_[mode_][Handles::RESIZE]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_H]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_V]->visible_ = false;
            s->handles_[mode_][Handles::SCALE]->visible_ = false;
            s->handles_[mode_][Handles::EDIT_SHAPE]->visible_ = false;
            s->handles_[mode_][Handles::MENU]->visible_ = false;
            // ROTATION on CENTER
            overlay_rotation_->visible_ = true;
            overlay_rotation_->translation_.x = s->stored_status_->translation_.x;
            overlay_rotation_->translation_.y = s->stored_status_->translation_.y;
            overlay_rotation_->update(0);
            overlay_rotation_fix_->visible_ = false;
            overlay_rotation_fix_->copyTransform(overlay_rotation_);
            overlay_rotation_clock_->visible_ = false;
            overlay_rotation_clock_hand_->visible_ = true;
            overlay_rotation_clock_hand_->translation_.x = s->stored_status_->translation_.x;
            overlay_rotation_clock_hand_->translation_.y = s->stored_status_->translation_.y;

            // prepare variables
            const float diagonal = glm::length( glm::vec2(s->frame()->aspectRatio() * s->stored_status_->scale_.x, s->stored_status_->scale_.y));
            glm::vec2 handle_polar = glm::vec2(diagonal, 0.f);

            // rotation center to center of source (disregarding scale)
            glm::mat4 M = glm::translate(glm::identity<glm::mat4>(), s->stored_status_->translation_);
            glm::vec3 source_from = glm::inverse(M) * glm::vec4( scene_from,  1.f );
            glm::vec3 source_to   = glm::inverse(M) * glm::vec4( scene_to,  1.f );

            // compute rotation angle on Z axis
            float angle = glm::orientedAngle( glm::normalize(glm::vec2(source_from)), glm::normalize(glm::vec2(source_to)));
            handle_polar.y = s->stored_status_->rotation_.z + angle;
            info << "Angle " << std::fixed << std::setprecision(1) << glm::degrees(sourceNode->rotation_.z) << UNICODE_DEGREE;

            // compute scaling of diagonal to reach new coordinates
            handle_polar.x *= glm::length( glm::vec2(source_to) ) / glm::length( glm::vec2(source_from) );

            // snap polar coordiantes (diagonal lenght, angle)
            if ( grid->active() ) {
                handle_polar = glm::round( handle_polar / grid->step() ) * grid->step();
                // prevent null size
                handle_polar.x = glm::max( grid->step().x,  handle_polar.x );
            }

            // Cancel scaling diagonal with SHIFT
            if (UserInterface::manager().shiftModifier()) {
                handle_polar.x = diagonal;
                overlay_rotation_fix_->visible_ = true;
            } else {
                info << std::endl << "   Size " << std::fixed << std::setprecision(3) << sourceNode->scale_.x;
                info << " x "  << sourceNode->scale_.y ;
            }

            // apply after snap
            sourceNode->rotation_ = glm::vec3(0.f, 0.f, handle_polar.y);
            handle_polar.x /= diagonal ;
            sourceNode->scale_ = s->stored_status_->scale_ * glm::vec3(handle_polar.x, handle_polar.x, 1.f);

            // update overlay
            overlay_rotation_clock_hand_->rotation_.z = sourceNode->rotation_.z;
            overlay_rotation_clock_hand_->update(0);

            // show cursor for rotation
            ret.type = Cursor_Hand;
        }
        // picking anywhere but on a handle: user wants to move the source
        else {

            // Default is to grab the center (0,0) of the source
            glm::vec3 handle(0.f);
            glm::vec3 offset(0.f);

            // Snap corner with SHIFT
            if (UserInterface::manager().shiftModifier()) {
                // get corner closest representative of the quadrant of the picking point
                handle = glm::vec3( glm::sign(pick.second), 0.f);
                // remember the offset for adjustment of translation to this corner
                offset = source_to_scene_transform * glm::vec4(handle, 0.f);
            }

            // Compute target coordinates of manipulated handle into SCENE reference frame
            glm::vec3 source_target = source_to_scene_transform * glm::vec4(handle, 1.f);

            // apply translation of target in SCENE
            source_target = glm::translate(glm::identity<glm::mat4>(), scene_to - scene_from) * glm::vec4(source_target, 1.f);

            // snap coordinates to grid (if active)
            if ( grid->active() )
                // snap coordinate in scene
                source_target = grid->snap(source_target);

            // Apply translation to the source
            sourceNode->translation_ = source_target - offset;

            //
            // grab all others in selection
            //
            // compute effective translation of current source s
            source_target = sourceNode->translation_ - s->stored_status_->translation_;
            // loop over selection
            for (auto it = Mixer::selection().begin(); it != Mixer::selection().end(); ++it) {
                if ( *it != s && !(*it)->locked() ) {
                    // translate and request update
                    (*it)->group(mode_)->translation_ = (*it)->stored_status_->translation_ + source_target;
                    (*it)->touch();
                }
            }

            // Show center overlay for POSITION
            overlay_position_->visible_ = true;
            overlay_position_->translation_.x = sourceNode->translation_.x;
            overlay_position_->translation_.y = sourceNode->translation_.y;
            overlay_position_->update(0);

            // Show move cursor
            ret.type = Cursor_ResizeAll;
            info << "Position " << std::fixed << std::setprecision(3) << sourceNode->translation_.x;
            info << ", "  << sourceNode->translation_.y ;
        }
    }

    // request update
    s->touch();

    // store action in history
    current_action_ = s->name() + ": " + info.str();

    // update cursor
    ret.info = info.str();
    return ret;
}

void GeometryView::terminate(bool force)
{
    View::terminate(force);

    // hide all view overlays
    overlay_position_->visible_       = false;
    overlay_position_cross_->visible_ = false;
    overlay_rotation_clock_->visible_ = false;
    overlay_rotation_clock_hand_->visible_ = false;
    overlay_rotation_fix_->visible_   = false;
    overlay_rotation_->visible_       = false;
    overlay_scaling_grid_->visible_   = false;
    overlay_scaling_cross_->visible_  = false;
    overlay_scaling_->visible_        = false;
    overlay_crop_->visible_           = false;

    // restore possible color change after selection operation
    overlay_rotation_->color = glm::vec4(1.f, 1.f, 1.f, 0.8f);
    overlay_rotation_fix_->color = glm::vec4(1.f, 1.f, 1.f, 0.8f);
    overlay_rotation_clock_tic_->color = glm::vec4(1.f, 1.f, 1.f, 0.8f);
    overlay_scaling_->color = glm::vec4(1.f, 1.f, 1.f, 0.8f);
    overlay_scaling_cross_->color =  glm::vec4(1.f, 1.f, 1.f, 0.8f);

    // restore of all handles overlays
    glm::vec2 c(0.f, 0.f);
    for (auto sit = Mixer::manager().session()->begin();
         sit != Mixer::manager().session()->end(); ++sit){
        (*sit)->handles_[mode_][Handles::RESIZE]->overlayActiveCorner(c);
        (*sit)->handles_[mode_][Handles::RESIZE_H]->overlayActiveCorner(c);
        (*sit)->handles_[mode_][Handles::RESIZE_V]->overlayActiveCorner(c);
        for (auto h = 0; h < 15; ++h)
            (*sit)->handles_[mode_][h]->visible_ = true;
    }

    overlay_selection_active_ = false;

    // reset grid
    adaptGridToSource();
}

#define MAX_DURATION 1000.f
#define MIN_SPEED_A 0.005f
#define MAX_SPEED_A 0.5f

void GeometryView::arrow (glm::vec2 movement)
{
    static float _duration = 0.f;
    static glm::vec2 _from(0.f);
    static glm::vec2 _displacement(0.f);

    Source *current = Mixer::manager().currentSource();

    if (!current && !Mixer::selection().empty())
        Mixer::manager().setCurrentSource( Mixer::selection().back() );

    if (current) {

        if (current_action_ongoing_) {

            // add movement to displacement
            _duration += dt_;
            const float speed = MIN_SPEED_A + (MAX_SPEED_A - MIN_SPEED_A) * glm::min(1.f,_duration / MAX_DURATION);
            _displacement += movement * dt_ * speed;

            // set coordinates of target
            glm::vec2 _to  = _from + _displacement;

            // update mouse pointer action
            MousePointer::manager().active()->update(_to, dt_ / 1000.f);

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

void GeometryView::updateSelectionOverlay(glm::vec4 color)
{
    View::updateSelectionOverlay(color);

    // create first
    if (overlay_selection_scale_ == nullptr) {

        overlay_selection_stored_status_ = new Group;
        overlay_selection_scale_ = new Handles(Handles::SCALE);
        overlay_selection_->attach(overlay_selection_scale_);
        overlay_selection_rotate_ = new Handles(Handles::ROTATE);
        overlay_selection_->attach(overlay_selection_rotate_);
    }

    if (overlay_selection_->visible_) {

        if ( !overlay_selection_active_) {

            // calculate ORIENTED bbox on selection
            GlmToolkit::OrientedBoundingBox selection_box = BoundingBoxVisitor::OBB(Mixer::selection().getCopy(), this);

            // apply transform
            overlay_selection_->rotation_ = selection_box.orientation;
            overlay_selection_->scale_ = selection_box.aabb.scale();
            glm::mat4 rot = glm::rotate(glm::identity<glm::mat4>(), selection_box.orientation.z, glm::vec3(0.f, 0.f, 1.f) );
            glm::vec4 vec = rot * glm::vec4(selection_box.aabb.center(), 1.f);
            overlay_selection_->translation_ = glm::vec3(vec);
        }

        // cosmetics
        overlay_selection_scale_->color = overlay_selection_icon_->color;
        overlay_selection_rotate_->color = overlay_selection_icon_->color;
    }
}

