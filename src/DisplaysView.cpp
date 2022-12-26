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
#include "PickingVisitor.h"
#include "DrawVisitor.h"
#include "Decorations.h"
#include "UserInterfaceManager.h"
#include "BoundingBoxVisitor.h"
#include "RenderingManager.h"

#include "DisplaysView.h"


DisplaysView::DisplaysView() : View(DISPLAYS)
{
    scene.root()->scale_ = glm::vec3(DISPLAYS_DEFAULT_SCALE, DISPLAYS_DEFAULT_SCALE, 1.0f);
    // read default settings
    if ( Settings::application.views[mode_].name.empty() )
        // no settings found: store application default
        saveSettings();
    else
        restoreSettings();
    Settings::application.views[mode_].name = "Displays";

    // Geometry Scene foreground
    output_surface_ = new Surface;
    output_surface_->visible_ = false;
    scene.fg()->attach(output_surface_);
//    Frame *border = new Frame(Frame::SHARP, Frame::THIN, Frame::NONE);
//    border->color = glm::vec4( COLOR_FRAME, 1.f );
//    scene.fg()->attach(border);

//    // User interface foreground
//    //
//    // point to show POSITION
//    overlay_position_ = new Symbol(Symbol::SQUARE_POINT);
//    overlay_position_->scale_ = glm::vec3(0.5f, 0.5f, 1.f);
//    scene.fg()->attach(overlay_position_);
//    overlay_position_->visible_ = false;
//    // cross to show the axis for POSITION
//    overlay_position_cross_ = new Symbol(Symbol::GRID);
//    overlay_position_cross_->scale_ = glm::vec3(0.5f, 0.5f, 1.f);
//    scene.fg()->attach(overlay_position_cross_);
//    overlay_position_cross_->visible_ = false;
//    // 'clock' : tic marks every 10 degrees for ROTATION
//    // with dark background
//    overlay_rotation_clock_ = new Group;
//    overlay_rotation_clock_tic_ = new Symbol(Symbol::CLOCK);
//    overlay_rotation_clock_->attach(overlay_rotation_clock_tic_);
//    Symbol *s = new Symbol(Symbol::CIRCLE_POINT);
//    s->color = glm::vec4(0.f, 0.f, 0.f, 0.1f);
//    s->scale_ = glm::vec3(28.f, 28.f, 1.f);
//    s->translation_.z = -0.1;
//    overlay_rotation_clock_->attach(s);
//    overlay_rotation_clock_->scale_ = glm::vec3(0.25f, 0.25f, 1.f);
//    scene.fg()->attach(overlay_rotation_clock_);
//    overlay_rotation_clock_->visible_ = false;
//    // circle to show fixed-size  ROTATION
//    overlay_rotation_clock_hand_ = new Symbol(Symbol::CLOCK_H);
//    overlay_rotation_clock_hand_->scale_ = glm::vec3(0.25f, 0.25f, 1.f);
//    scene.fg()->attach(overlay_rotation_clock_hand_);
//    overlay_rotation_clock_hand_->visible_ = false;
//    overlay_rotation_fix_ = new Symbol(Symbol::SQUARE);
//    overlay_rotation_fix_->scale_ = glm::vec3(0.25f, 0.25f, 1.f);
//    scene.fg()->attach(overlay_rotation_fix_);
//    overlay_rotation_fix_->visible_ = false;
//    // circle to show the center of ROTATION
//    overlay_rotation_ = new Symbol(Symbol::CIRCLE);
//    overlay_rotation_->scale_ = glm::vec3(0.25f, 0.25f, 1.f);
//    scene.fg()->attach(overlay_rotation_);
//    overlay_rotation_->visible_ = false;
//    // 'grid' : tic marks every 0.1 step for SCALING
//    // with dark background
//    Group *g = new Group;
//    s = new Symbol(Symbol::GRID);
//    s->scale_ = glm::vec3(1.655f, 1.655f, 1.f);
//    g->attach(s);
//    s = new Symbol(Symbol::SQUARE_POINT);
//    s->color = glm::vec4(0.f, 0.f, 0.f, 0.1f);
//    s->scale_ = glm::vec3(17.f, 17.f, 1.f);
//    s->translation_.z = -0.1;
//    g->attach(s);
//    overlay_scaling_grid_ = g;
//    overlay_scaling_grid_->scale_ = glm::vec3(0.3f, 0.3f, 1.f);
//    scene.fg()->attach(overlay_scaling_grid_);
//    overlay_scaling_grid_->visible_ = false;
//    // cross in the square for proportional SCALING
//    overlay_scaling_cross_ = new Symbol(Symbol::CROSS);
//    overlay_scaling_cross_->scale_ = glm::vec3(0.3f, 0.3f, 1.f);
//    scene.fg()->attach(overlay_scaling_cross_);
//    overlay_scaling_cross_->visible_ = false;
//    // square to show the center of SCALING
//    overlay_scaling_ = new Symbol(Symbol::SQUARE);
//    overlay_scaling_->scale_ = glm::vec3(0.3f, 0.3f, 1.f);
//    scene.fg()->attach(overlay_scaling_);
//    overlay_scaling_->visible_ = false;

//    border = new Frame(Frame::SHARP, Frame::THIN, Frame::NONE);
//    border->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 0.2f );
//    overlay_crop_ = border;
//    scene.fg()->attach(overlay_crop_);
//    overlay_crop_->visible_ = false;

    // will be init later
    overlay_selection_scale_ = nullptr;
    overlay_selection_rotate_ = nullptr;
    overlay_selection_stored_status_ = nullptr;
    overlay_selection_active_ = false;
}

void DisplaysView::update(float dt)
{
    View::update(dt);

    // a more complete update is requested
    if (View::need_deep_update_ > 0) {

        // update rendering of render frame
        FrameBuffer *output = Mixer::manager().session()->frame();
        if (output){
//            float aspect_ratio = output->aspectRatio();
//            for (NodeSet::iterator node = scene.bg()->begin(); node != scene.bg()->end(); ++node) {
//                (*node)->scale_.x = aspect_ratio;
//            }
//            for (NodeSet::iterator node = scene.fg()->begin(); node != scene.fg()->end(); ++node) {
//                (*node)->scale_.x = aspect_ratio;
//            }
//            output_surface_->setTextureIndex( output->texture() );
        }

        // prevent invalid scaling
        float s = CLAMP(scene.root()->scale_.x, DISPLAYS_MIN_SCALE, DISPLAYS_MAX_SCALE);
        scene.root()->scale_.x = s;
        scene.root()->scale_.y = s;
    }


}


// recenter is called also when RenderingManager detects a change of monitors
void  DisplaysView::recenter ()
{
    // clear background display of monitors
    scene.clearBackground();

    // fill scene background with the frames to show monitors
    std::map<std::string, glm::ivec4> _monitors = Rendering::manager().monitors();
    for (auto monitor_iter = _monitors.begin();
         monitor_iter != _monitors.end(); ++monitor_iter) {

        Group *m = new Group;
        glm::vec4 rect = DISPLAYS_UNIT * glm::vec4(monitor_iter->second);
        m->scale_ = glm::vec3( 0.5f * rect.p, 0.5f * rect.q, 1.f );
        m->translation_ = glm::vec3( rect.x + m->scale_.x, -rect.y - m->scale_.y, 0.f );

        Surface *surf = new Surface;
        m->attach(surf);

        Frame *frame = new Frame(Frame::SHARP, Frame::THIN, Frame::NONE);
        frame->color = glm::vec4( COLOR_MONITOR, 1.f );
        m->attach(frame);

        scene.bg()->attach( m );

//        g_printerr("- display %f,%f  %f,%f\n", rect.x, rect.y, rect.w, rect.z);
//        g_printerr("          %f,%f  %f,%f\n", m->translation_.x, m->translation_.y,
//                   m->scale_.x, m->scale_.y);
    }

    // calculate screen area required to see the entire scene
    BoundingBoxVisitor scene_visitor_bbox(true);
    scene.accept(scene_visitor_bbox);
    GlmToolkit::AxisAlignedBoundingBox scene_box = scene_visitor_bbox.bbox();

    // compute scaling to fit the scene box into the scene, modulo a margin ratio
    scene.root()->scale_.x = SCENE_UNIT / ( scene_box.scale().x * 1.3f);
    scene.root()->scale_.y = scene.root()->scale_.x;
    // compute translation to place at the center (considering scaling)
    scene.root()->translation_ = -scene.root()->scale_.x * scene_box.center();

}

void DisplaysView::resize ( int scale )
{
    float z = CLAMP(0.01f * (float) scale, 0.f, 1.f);
    z *= z;
    z *= DISPLAYS_MAX_SCALE - DISPLAYS_MIN_SCALE;
    z += DISPLAYS_MIN_SCALE;
    scene.root()->scale_.x = z;
    scene.root()->scale_.y = z;
}

int  DisplaysView::size ()
{
    float z = (scene.root()->scale_.x - DISPLAYS_MIN_SCALE) / (DISPLAYS_MAX_SCALE - DISPLAYS_MIN_SCALE);
    return (int) ( sqrt(z) * 100.f);
}

void DisplaysView::draw()
{
//    std::vector<Node *> surfaces;
//    std::vector<Node *> overlays;

//    g_printerr("DisplaysView::draw()\n");


    View::draw();

//    // 0. prepare projection for draw visitors
//    glm::mat4 projection = Rendering::manager().Projection();

////    // 1. Draw surface of sources in the current workspace
////    DrawVisitor draw_surfaces(surfaces, projection);
////    scene.accept(draw_surfaces);

////    for( auto it = surfaces.begin(); it!=surfaces.end(); ++it) {
////        delete *it;
////    }

////    // 2. Draw scene rendering on top (which includes rendering of all visible sources)
////    DrawVisitor draw_rendering(output_surface_, projection, true);
////    scene.accept(draw_rendering);

//    // 3. Draw frames and icons of sources in the current workspace
//    DrawVisitor draw_overlays(overlays, projection);
//    scene.accept(draw_overlays);

//    // 4. Draw control overlays of current source on top (if selectable)
//    if (s!=nullptr && canSelect(s)) {
//        s->setMode(Source::CURRENT);
//        DrawVisitor dv(s->overlays_[mode_], projection);
//        scene.accept(dv);
//    }

//    // 5. Finally, draw overlays of view
//    DrawVisitor draw_foreground(scene.fg(), projection);
//    scene.accept(draw_foreground);

//     display interface
    // Locate window at upper left corner
//    glm::vec2 P = glm::vec2(-output_surface_->scale_.x - 0.02f, output_surface_->scale_.y + 0.01 );
//    P = Rendering::manager().project(glm::vec3(P, 0.f), scene.root()->transform_, false);
//    // Set window position depending on icons size
//    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
//    ImGui::SetNextWindowPos(ImVec2(P.x, P.y - 1.5f * ImGui::GetFrameHeight() ), ImGuiCond_Always);
//    if (ImGui::Begin("##DisplaysViewOptions", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground
//                     | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings
//                     | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus ))
//    {
//        // style grey
//        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(COLOR_FRAME_LIGHT, 1.f));  // 1
//        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.14f, 0.14f, 0.14f, 0.9f));
//        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.36f, 0.36f, 0.36f, 0.9f));
//        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.36f, 0.36f, 0.36f, 0.5f));
//        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.14f, 0.00f));
//        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.14f, 0.14f, 0.14f, 0.46f));
//        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.14f, 0.14f, 0.00f));
//        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.14f, 0.14f, 0.14f, 0.46f)); // 8

//        static std::vector< std::pair<int, int> > icons_ws = { {10,16}, {11,16}, {12,16} };
//        static std::vector< std::string > labels_ws = { "Background", "Workspace", "Foreground" };
//        if ( ImGuiToolkit::ComboIcon (icons_ws, labels_ws, &Settings::application.current_workspace) ){
//             ++View::need_deep_update_;
//        }

//        ImGui::PopStyleColor(8);  // 14 colors
//        ImGui::End();
//    }
//    ImGui::PopFont();

//    // display popup menu
//    if (show_context_menu_ == MENU_SOURCE) {
//        ImGui::OpenPopup( "DisplaysOutputContextMenu" );
//        show_context_menu_ = MENU_NONE;
//    }
//    if (ImGui::BeginPopup("DisplaysOutputContextMenu")) {
////        if (s != nullptr) {
////            if (ImGui::Selectable( ICON_FA_EXPAND "   Fit" )){
////                s->group(mode_)->scale_ = glm::vec3(output_surface_->scale_.x/ s->frame()->aspectRatio(), 1.f, 1.f);
////                s->group(mode_)->rotation_.z = 0;
////                s->group(mode_)->translation_ = glm::vec3(0.f);
////                s->touch();
////                Action::manager().store(s->name() + std::string(": Geometry Fit"));
////            }
////            if (ImGui::Selectable( ICON_FA_VECTOR_SQUARE "  Reset" )){
////                s->group(mode_)->scale_ = glm::vec3(1.f);
////                s->group(mode_)->rotation_.z = 0;
////                s->group(mode_)->crop_ = glm::vec3(1.f);
////                s->group(mode_)->translation_ = glm::vec3(0.f);
////                s->touch();
////                Action::manager().store(s->name() + std::string(": Geometry Reset"));
////            }
////            if (ImGui::Selectable( ICON_FA_CROSSHAIRS "  Reset position" )){
////                s->group(mode_)->translation_ = glm::vec3(0.f);
////                s->touch();
////                Action::manager().store(s->name() + std::string(": Reset position"));
////            }
////            if (ImGui::Selectable( ICON_FA_COMPASS "  Reset rotation" )){
////                s->group(mode_)->rotation_.z = 0;
////                s->touch();
////                Action::manager().store(s->name() + std::string(": Reset rotation"));
////            }
////            if (ImGui::Selectable( ICON_FA_EXPAND_ALT "  Reset aspect ratio" )){
////                s->group(mode_)->scale_.x = s->group(mode_)->scale_.y;
////                s->group(mode_)->scale_.x *= s->group(mode_)->crop_.x / s->group(mode_)->crop_.y;
////                s->touch();
////                Action::manager().store(s->name() + std::string(": Reset aspect ratio"));
////            }
////        }
//        ImGui::EndPopup();
//    }

}


std::pair<Node *, glm::vec2> DisplaysView::pick(glm::vec2 P)
{
    // prepare empty return value
    std::pair<Node *, glm::vec2> pick = { nullptr, glm::vec2(0.f) };

    // unproject mouse coordinate into scene coordinates
    glm::vec3 scene_point_ = Rendering::manager().unProject(P);

//    // picking visitor traverses the scene
//    PickingVisitor pv(scene_point_);
//    scene.accept(pv);

//    // picking visitor found nodes?
//    if ( !pv.empty() ) {
//        // keep current source active if it is clicked
//        Source *current = Mixer::manager().currentSource();
//        if (current != nullptr) {
//            if (current->workspace() != Settings::application.current_workspace){
//                current = nullptr;
//            }
//            else {
//                // find if the current source was picked
//                auto itp = pv.rbegin();
//                for (; itp != pv.rend(); ++itp){
//                    // test if source contains this node
//                    Source::hasNode is_in_source((*itp).first );
//                    if ( is_in_source( current ) ){
//                        // a node in the current source was clicked !
//                        pick = *itp;
//                        break;
//                    }
//                }
//                // not found: the current source was not clicked
//                if (itp == pv.rend()) {
//                    current = nullptr;
//                }
//                // picking on the menu handle: show context menu
//                else if ( pick.first == current->handles_[mode_][Handles::MENU] ) {
//                    openContextMenu(MENU_SOURCE);
//                }
//                // pick on the lock icon; unlock source
//                else if ( UserInterface::manager().ctrlModifier() && pick.first == current->lock_ ) {
//                    lock(current, false);
//                    pick = { current->locker_, pick.second };
////                    pick = { nullptr, glm::vec2(0.f) };
//                }
//                // pick on the open lock icon; lock source and cancel pick
//                else if ( UserInterface::manager().ctrlModifier() && pick.first == current->unlock_ ) {
//                    lock(current, true);
//                    pick = { nullptr, glm::vec2(0.f) };
//                }
//                // pick a locked source ; cancel pick
//                else if ( !UserInterface::manager().ctrlModifier() && current->locked() ) {
//                    pick = { nullptr, glm::vec2(0.f) };
//                }
//            }
//        }
//        // the clicked source changed (not the current source)
//        if (current == nullptr) {

//            if (UserInterface::manager().ctrlModifier()) {

//                // default to failed pick
//                pick = { nullptr, glm::vec2(0.f) };

//                // loop over all nodes picked to detect clic on locks
//                for (auto itp = pv.rbegin(); itp != pv.rend(); ++itp){
//                    // get if a source was picked
//                    Source *s = Mixer::manager().findSource((*itp).first);
//                    // lock icon of a source (not current) is picked : unlock
//                    if ( s!=nullptr && (*itp).first == s->lock_) {
//                        lock(s, false);
//                        pick = { s->locker_, (*itp).second };
//                        break;
//                    }
//                }
//            }
//            // no lock icon picked, find what else was picked
//            if ( pick.first == nullptr) {

//                // loop over all nodes picked
//                for (auto itp = pv.rbegin(); itp != pv.rend(); ++itp){
//                    // get if a source was picked
//                    Source *s = Mixer::manager().findSource((*itp).first);
//                    // accept picked sources in current workspaces
//                    if ( s!=nullptr && s->workspace() == Settings::application.current_workspace) {
//                        // a non-locked source is picked (anywhere)
//                        if ( !s->locked() ) {
//                            // not in an active selection? don't pick this one!
//                            if ( !UserInterface::manager().ctrlModifier() &&
//                                 Mixer::selection().size() > 1 &&
//                                 !Mixer::selection().contains(s))
//                                continue;
//                            // yeah, pick this one (NB: locker_ is just a node in Geometry that is detected)
//                            pick = { s->locker_,  (*itp).second };
//                            break;
//                        }

//                    }
//                    // not a source picked
//                    else {
//                        // picked on selection handles
//                        if ( (*itp).first == overlay_selection_scale_ || (*itp).first == overlay_selection_rotate_ ) {
//                            pick = (*itp);
//                            // initiate selection manipulation
//                            if (overlay_selection_stored_status_) {
//                                overlay_selection_stored_status_->copyTransform(overlay_selection_);
//                                overlay_selection_active_ = true;
//                            }
//                            break;
//                        }
//                        else if ( overlay_selection_icon_ != nullptr && (*itp).first == overlay_selection_icon_ ) {
//                            pick = (*itp);
//                            openContextMenu(MENU_SELECTION);
//                            break;
//                        }
//                    }
//                }
//            }
//        }
//    }

    return pick;
}

bool DisplaysView::canSelect(Source *s) {

    return ( s!=nullptr && View::canSelect(s) && s->ready() && s->active() && s->workspace() == Settings::application.current_workspace);
}


void DisplaysView::applySelectionTransform(glm::mat4 M)
{
//    for (auto sit = Mixer::selection().begin(); sit != Mixer::selection().end(); ++sit){
//        // recompute all from matrix transform
//        glm::mat4 transform = M * (*sit)->stored_status_->transform_;
//        glm::vec3 tra, rot, sca;
//        GlmToolkit::inverse_transform(transform, tra, rot, sca);
//        (*sit)->group(mode_)->translation_ = tra;
//        (*sit)->group(mode_)->scale_ = sca;
//        (*sit)->group(mode_)->rotation_ = rot;
//        // will have to be updated
//        (*sit)->touch();
//    }
}

View::Cursor DisplaysView::grab (Source *s, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2> pick)
{
    View::Cursor ret = Cursor();

    // grab coordinates in scene-View reference frame
    glm::vec3 scene_from = Rendering::manager().unProject(from, scene.root()->transform_);
    glm::vec3 scene_to   = Rendering::manager().unProject(to, scene.root()->transform_);
    glm::vec3 scene_translation = scene_to - scene_from;

    // No source is given
    if (!s) {

//        // possibly grabing the selection overlay handles
//        if (overlay_selection_ && overlay_selection_active_ ) {

//            // rotation center to selection position
//            glm::mat4 T = glm::translate(glm::identity<glm::mat4>(), overlay_selection_stored_status_->translation_);
//            glm::vec4 selection_from = glm::inverse(T) * glm::vec4( scene_from,  1.f );
//            glm::vec4 selection_to   = glm::inverse(T) * glm::vec4( scene_to,  1.f );

//            // calculate scaling of selection
//            float factor = glm::length( glm::vec2( selection_to ) ) / glm::length( glm::vec2( selection_from ) );
//            glm::mat4 S = glm::scale(glm::identity<glm::mat4>(), glm::vec3(factor, factor, 1.f));

//            // if interaction with selection SCALING handle
//            if (pick.first == overlay_selection_scale_) {

//                // show manipulation overlay
//                overlay_scaling_cross_->visible_ = true;
//                overlay_scaling_grid_->visible_ = false;
//                overlay_scaling_->visible_ = true;
//                overlay_scaling_->translation_.x = overlay_selection_stored_status_->translation_.x;
//                overlay_scaling_->translation_.y = overlay_selection_stored_status_->translation_.y;
//                overlay_scaling_->rotation_.z = overlay_selection_stored_status_->rotation_.z;
//                overlay_scaling_->update(0);
//                overlay_scaling_cross_->copyTransform(overlay_scaling_);
//                overlay_scaling_->color = overlay_selection_icon_->color;
//                overlay_scaling_cross_->color = overlay_selection_icon_->color;

//                // apply to selection overlay
//                glm::vec4 vec = S * glm::vec4( overlay_selection_stored_status_->scale_, 0.f );
//                overlay_selection_->scale_ = glm::vec3(vec);

//                // apply to selection sources
//                // NB: complete transform matrix (right to left) : move to center, scale and move back
//                glm::mat4 M = T * S * glm::inverse(T);
//                applySelectionTransform(M);

//                // store action in history
//                current_action_ = "Scale selection";
//                ret.type = Cursor_ResizeNWSE;
//            }
//            // if interaction with selection ROTATION handle
//            else if (pick.first == overlay_selection_rotate_) {

//                // show manipulation overlay
//                overlay_rotation_->visible_ = true;
//                overlay_rotation_->translation_.x = overlay_selection_stored_status_->translation_.x;
//                overlay_rotation_->translation_.y = overlay_selection_stored_status_->translation_.y;
//                overlay_rotation_->update(0);
//                overlay_rotation_->color = overlay_selection_icon_->color;
//                overlay_rotation_fix_->visible_ = false;
//                overlay_rotation_fix_->copyTransform(overlay_rotation_);
//                overlay_rotation_fix_->color = overlay_selection_icon_->color;

//                // cancel out scaling with SHIFT modifier key
//                if (UserInterface::manager().shiftModifier()) {
//                    overlay_rotation_fix_->visible_ = true;
//                    float scale_factor = glm::length( glm::vec2( overlay_selection_->scale_ ) ) / glm::length( glm::vec2( overlay_selection_stored_status_->scale_ ) );
//                    S = glm::scale(glm::identity<glm::mat4>(), glm::vec3(scale_factor, scale_factor, 1.f));
//                }

//                // compute rotation angle
//                float angle = glm::orientedAngle( glm::normalize(glm::vec2(selection_from)), glm::normalize(glm::vec2(selection_to)));

//                // apply to selection overlay
//                glm::vec4 vec = S * glm::vec4( overlay_selection_stored_status_->scale_, 0.f );
//                overlay_selection_->scale_ = glm::vec3(vec);
//                overlay_selection_->rotation_.z = overlay_selection_stored_status_->rotation_.z + angle;

//                // POST-CORRECTION ; discretized rotation with ALT
//                if (UserInterface::manager().altModifier()) {
//                    int degrees = int(  glm::degrees(overlay_selection_->rotation_.z) );
//                    degrees = (degrees / 10) * 10;
//                    overlay_selection_->rotation_.z = glm::radians( float(degrees) );
//                    angle = overlay_selection_->rotation_.z - overlay_selection_stored_status_->rotation_.z;
//                    overlay_rotation_clock_->visible_ = true;
//                    overlay_rotation_clock_->copyTransform(overlay_rotation_);
//                    overlay_rotation_clock_tic_->color = overlay_selection_icon_->color;
//                }

//                // apply to selection sources
//                // NB: complete transform matrix (right to left) : move to center, rotate, scale and move back
//                glm::mat4 R = glm::rotate(glm::identity<glm::mat4>(), angle, glm::vec3(0.f, 0.f, 1.f) );
//                glm::mat4 M = T * S * R * glm::inverse(T);
//                applySelectionTransform(M);

//                // store action in history
//                current_action_ = "Scale and rotate selection";
//                ret.type = Cursor_Hand;
//            }

//        }

        // update cursor
        return ret;
    }

//    Group *sourceNode = s->group(mode_); // groups_[View::GEOMETRY]

//    // make sure matrix transform of stored status is updated
//    s->stored_status_->update(0);
//    // grab coordinates in source-root reference frame
//    glm::vec4 source_from = glm::inverse(s->stored_status_->transform_) * glm::vec4( scene_from,  1.f );
//    glm::vec4 source_to   = glm::inverse(s->stored_status_->transform_) * glm::vec4( scene_to,  1.f );
//    glm::vec3 source_scaling     = glm::vec3(source_to) / glm::vec3(source_from);

    // which manipulation to perform?
    std::ostringstream info;

    // store action in history
    current_action_ = s->name() + ": " + info.str();

    // update cursor
    ret.info = info.str();
    return ret;
}

void DisplaysView::terminate(bool force)
{
    View::terminate(force);

//    // hide all view overlays
//    overlay_position_->visible_       = false;
//    overlay_position_cross_->visible_ = false;
//    overlay_rotation_clock_->visible_ = false;
//    overlay_rotation_clock_hand_->visible_ = false;
//    overlay_rotation_fix_->visible_   = false;
//    overlay_rotation_->visible_       = false;
//    overlay_scaling_grid_->visible_   = false;
//    overlay_scaling_cross_->visible_  = false;
//    overlay_scaling_->visible_        = false;
//    overlay_crop_->visible_           = false;

//    // restore possible color change after selection operation
//    overlay_rotation_->color = glm::vec4(1.f, 1.f, 1.f, 0.8f);
//    overlay_rotation_fix_->color = glm::vec4(1.f, 1.f, 1.f, 0.8f);
//    overlay_rotation_clock_tic_->color = glm::vec4(1.f, 1.f, 1.f, 0.8f);
//    overlay_scaling_->color = glm::vec4(1.f, 1.f, 1.f, 0.8f);
//    overlay_scaling_cross_->color =  glm::vec4(1.f, 1.f, 1.f, 0.8f);

    // restore of all handles overlays
//    glm::vec2 c(0.f, 0.f);
//    for (auto sit = Mixer::manager().session()->begin();
//         sit != Mixer::manager().session()->end(); ++sit){

//        (*sit)->handles_[mode_][Handles::RESIZE]->overlayActiveCorner(c);
//        (*sit)->handles_[mode_][Handles::RESIZE_H]->overlayActiveCorner(c);
//        (*sit)->handles_[mode_][Handles::RESIZE_V]->overlayActiveCorner(c);
//        (*sit)->handles_[mode_][Handles::RESIZE]->visible_ = true;
//        (*sit)->handles_[mode_][Handles::RESIZE_H]->visible_ = true;
//        (*sit)->handles_[mode_][Handles::RESIZE_V]->visible_ = true;
//        (*sit)->handles_[mode_][Handles::SCALE]->visible_ = true;
//        (*sit)->handles_[mode_][Handles::ROTATE]->visible_ = true;
//        (*sit)->handles_[mode_][Handles::CROP]->visible_ = true;
//        (*sit)->handles_[mode_][Handles::MENU]->visible_ = true;
//    }

    overlay_selection_active_ = false;
}

void DisplaysView::arrow (glm::vec2 movement)
{
    static float accumulator = 0.f;
    accumulator += dt_;

    glm::vec3 gl_Position_from = Rendering::manager().unProject(glm::vec2(0.f), scene.root()->transform_);
    glm::vec3 gl_Position_to   = Rendering::manager().unProject(movement, scene.root()->transform_);
    glm::vec3 gl_delta = gl_Position_to - gl_Position_from;

//    bool first = true;
//    glm::vec3 delta_translation(0.f);
//    for (auto it = Mixer::selection().begin(); it != Mixer::selection().end(); ++it) {

//        // individual move with SHIFT
//        if ( !Source::isCurrent(*it) && UserInterface::manager().shiftModifier() )
//            continue;

//        Group *sourceNode = (*it)->group(mode_);
//        glm::vec3 dest_translation(0.f);

//        if (first) {
//            // dest starts at current
//            dest_translation = sourceNode->translation_;

//            // + ALT : discrete displacement
//            if (UserInterface::manager().altModifier()) {
//                if (accumulator > 100.f) {
//                    dest_translation += glm::sign(gl_delta) * 0.11f;
//                    dest_translation.x = ROUND(dest_translation.x, 10.f);
//                    dest_translation.y = ROUND(dest_translation.y, 10.f);
//                    accumulator = 0.f;
//                }
//                else
//                    break;
//            }
//            else {
//                // normal case: dest += delta
//                dest_translation += gl_delta * ARROWS_MOVEMENT_FACTOR * dt_;
//                accumulator = 0.f;
//            }

//            // store action in history
//            std::ostringstream info;
//            info << "Position " << std::fixed << std::setprecision(3) << sourceNode->translation_.x;
//            info << ", "  << sourceNode->translation_.y ;
//            current_action_ = (*it)->name() + ": " + info.str();

//            // delta for others to follow
//            delta_translation = dest_translation - sourceNode->translation_;
//        }
//        else {
//            // dest = current + delta from first
//            dest_translation = sourceNode->translation_ + delta_translation;
//        }

//        // apply & request update
//        sourceNode->translation_ = dest_translation;
//        (*it)->touch();

//        first = false;
//    }
}

void DisplaysView::updateSelectionOverlay()
{
    View::updateSelectionOverlay();

//    // create first
//    if (overlay_selection_scale_ == nullptr) {

//        overlay_selection_stored_status_ = new Group;
//        overlay_selection_scale_ = new Handles(Handles::SCALE);
//        overlay_selection_->attach(overlay_selection_scale_);
//        overlay_selection_rotate_ = new Handles(Handles::ROTATE);
//        overlay_selection_->attach(overlay_selection_rotate_);
//    }

//    if (overlay_selection_->visible_) {

//        if ( !overlay_selection_active_) {

//            // calculate ORIENTED bbox on selection
//            GlmToolkit::OrientedBoundingBox selection_box = BoundingBoxVisitor::OBB(Mixer::selection().getCopy(), this);

//            // apply transform
//            overlay_selection_->rotation_ = selection_box.orientation;
//            overlay_selection_->scale_ = selection_box.aabb.scale();
//            glm::mat4 rot = glm::rotate(glm::identity<glm::mat4>(), selection_box.orientation.z, glm::vec3(0.f, 0.f, 1.f) );
//            glm::vec4 vec = rot * glm::vec4(selection_box.aabb.center(), 1.f);
//            overlay_selection_->translation_ = glm::vec3(vec);
//        }

//        // cosmetics
//        overlay_selection_scale_->color = overlay_selection_icon_->color;
//        overlay_selection_rotate_->color = overlay_selection_icon_->color;
//    }
}

