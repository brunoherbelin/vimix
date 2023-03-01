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

#include <string>
#include <sstream>
#include <iomanip>

#include "ImGuiToolkit.h"

#include "defines.h"
#include "Mixer.h"
#include "Source.h"
#include "Settings.h"
#include "Resource.h"
#include "PickingVisitor.h"
#include "DrawVisitor.h"
#include "Decorations.h"
#include "UserInterfaceManager.h"
#include "ActionManager.h"
#include "DialogToolkit.h"

#include "TextureView.h"

#define MASK_PAINT_ACTION_LABEL "Mask Paint"

TextureView::TextureView() : View(TEXTURE), edit_source_(nullptr), need_edit_update_(true)
{
    scene.root()->scale_ = glm::vec3(APPEARANCE_DEFAULT_SCALE, APPEARANCE_DEFAULT_SCALE, 1.0f);
    scene.root()->translation_ = glm::vec3(0.8f, 0.f, 0.0f);
    // read default settings
    if ( Settings::application.views[mode_].name.empty() )
        // no settings found: store application default
        saveSettings();
    else
        restoreSettings();
    Settings::application.views[mode_].name = "Texturing";

    //
    // Scene background
    //
    // global dark
    Surface *tmp = new Surface( new Shader);
    tmp->scale_ = glm::vec3(20.f, 20.f, 1.f);
    tmp->shader()->color = glm::vec4( 0.1f, 0.1f, 0.1f, 0.6f );
    scene.bg()->attach(tmp);
    // frame showing the source original shape
    background_surface_= new Surface( new Shader);
    background_surface_->scale_ = glm::vec3(20.f, 20.f, 1.f);
    background_surface_->shader()->color = glm::vec4( COLOR_BGROUND, 1.0f );
    scene.bg()->attach(background_surface_);
    background_frame_ = new Frame(Frame::SHARP, Frame::THIN, Frame::NONE);
    background_frame_->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 0.6f );
    scene.bg()->attach(background_frame_);
    // frame with checkerboard background to show cropped preview
    preview_checker_ = new ImageSurface("images/checker.dds");
    static glm::mat4 Tra = glm::scale(glm::translate(glm::identity<glm::mat4>(), glm::vec3( -32.f, -32.f, 0.f)), glm::vec3( 64.f, 64.f, 1.f));
    preview_checker_->shader()->iTransform = Tra;
    scene.bg()->attach(preview_checker_);
    preview_frame_ = new Frame(Frame::SHARP, Frame::THIN, Frame::GLOW);
    preview_frame_->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 1.f );
    scene.bg()->attach(preview_frame_);
    // marks on the frame to show scale
    show_scale_ = false;
    horizontal_mark_ = new Mesh("mesh/h_mark.ply");
    horizontal_mark_->translation_ = glm::vec3(0.f, -1.f, 0.0f);
    horizontal_mark_->scale_ = glm::vec3(2.5f, -2.5f, 0.0f);
    horizontal_mark_->rotation_.z = M_PI;
    horizontal_mark_->shader()->color = glm::vec4( COLOR_TRANSITION_LINES, 0.9f );
    scene.bg()->attach(horizontal_mark_);
    vertical_mark_  = new Mesh("mesh/h_mark.ply");
    vertical_mark_->translation_ = glm::vec3(-1.0f, 0.0f, 0.0f);
    vertical_mark_->scale_ = glm::vec3(2.5f, -2.5f, 0.0f);
    vertical_mark_->rotation_.z = M_PI_2;
    vertical_mark_->shader()->color = glm::vec4( COLOR_TRANSITION_LINES, 0.9f );
    scene.bg()->attach(vertical_mark_);

    //
    // surface to show the texture of the source
    //
    preview_shader_ = new ImageShader;
    preview_surface_ = new Surface(preview_shader_); // to attach source preview
    preview_surface_->translation_.z = 0.002f;
    scene.bg()->attach(preview_surface_);

    //
    // User interface foreground
    //
    // Mask manipulation
    mask_node_ = new Group;
    mask_square_ = new Frame(Frame::SHARP, Frame::LARGE, Frame::NONE);
    mask_square_->color = glm::vec4( COLOR_APPEARANCE_MASK, 1.f );
    mask_node_->attach(mask_square_);
    mask_circle_ = new Mesh("mesh/circle.ply");
    mask_circle_->shader()->color = glm::vec4( COLOR_APPEARANCE_MASK, 1.f );
    mask_node_->attach(mask_circle_);
    mask_horizontal_ = new Mesh("mesh/h_line.ply");
    mask_horizontal_->shader()->color = glm::vec4( COLOR_APPEARANCE_MASK, 1.f );
    mask_horizontal_->scale_.x = 1.0f;
    mask_horizontal_->scale_.y = 3.0f;
    mask_node_->attach(mask_horizontal_);
    mask_vertical_ = new Group;
    Mesh *line = new Mesh("mesh/h_line.ply");
    line->shader()->color = glm::vec4( COLOR_APPEARANCE_MASK, 1.f );
    line->scale_.x = 1.0f;
    line->scale_.y = 3.0f;
    line->rotation_.z = M_PI_2;
    mask_vertical_->attach(line);
    mask_node_->attach(mask_vertical_);
    scene.fg()->attach(mask_node_);

    // Source manipulation (texture coordinates)
    //
    // point to show POSITION
    overlay_position_ = new Symbol(Symbol::SQUARE_POINT);
    overlay_position_->color = glm::vec4( COLOR_APPEARANCE_SOURCE, 1.f );
    overlay_position_->scale_ = glm::vec3(0.5f, 0.5f, 1.f);
    scene.fg()->attach(overlay_position_);
    overlay_position_->visible_ = false;
    // cross to show the axis for POSITION
    overlay_position_cross_ = new Symbol(Symbol::GRID);
    overlay_position_cross_->color = glm::vec4( COLOR_APPEARANCE_SOURCE, 1.f );
    overlay_position_cross_->scale_ = glm::vec3(0.5f, 0.5f, 1.f);
    scene.fg()->attach(overlay_position_cross_);
    overlay_position_cross_->visible_ = false;
    // 'grid' : tic marks every 0.1 step for SCALING
    // with dark background
    Group *g = new Group;
    Symbol *s = new Symbol(Symbol::GRID);
    s->scale_ = glm::vec3(1.655f, 1.655f, 1.f);
    s->color = glm::vec4( COLOR_APPEARANCE_SOURCE, 1.f );
    g->attach(s);
    s = new Symbol(Symbol::SQUARE_POINT);
    s->color = glm::vec4(0.f, 0.f, 0.f, 0.2f);
    s->scale_ = glm::vec3(18.f, 18.f, 1.f);
    s->translation_.z = -0.1;
    g->attach(s);
    overlay_scaling_grid_ = g;
    overlay_scaling_grid_->scale_ = glm::vec3(0.3f, 0.3f, 1.f);
    scene.fg()->attach(overlay_scaling_grid_);
    overlay_scaling_grid_->visible_ = false;
    // cross in the square for proportional SCALING
    overlay_scaling_cross_ = new Symbol(Symbol::CROSS);
    overlay_scaling_cross_->color = glm::vec4( COLOR_APPEARANCE_SOURCE, 1.f );
    overlay_scaling_cross_->scale_ = glm::vec3(0.3f, 0.3f, 1.f);
    scene.fg()->attach(overlay_scaling_cross_);
    overlay_scaling_cross_->visible_ = false;
    // square to show the center of SCALING
    overlay_scaling_ = new Symbol(Symbol::SQUARE);
    overlay_scaling_->color = glm::vec4( COLOR_APPEARANCE_SOURCE, 1.f );
    overlay_scaling_->scale_ = glm::vec3(0.3f, 0.3f, 1.f);
    scene.fg()->attach(overlay_scaling_);
    overlay_scaling_->visible_ = false;
    // 'clock' : tic marks every 10 degrees for ROTATION
    // with dark background
    g = new Group;
    s = new Symbol(Symbol::CLOCK);
    s->color = glm::vec4( COLOR_APPEARANCE_SOURCE, 1.f );
    g->attach(s);
    s = new Symbol(Symbol::CIRCLE_POINT);
    s->color = glm::vec4(0.f, 0.f, 0.f, 0.25f);
    s->scale_ = glm::vec3(28.f, 28.f, 1.f);
    s->translation_.z = -0.1;
    g->attach(s);
    overlay_rotation_clock_ = g;
    overlay_rotation_clock_->scale_ = glm::vec3(0.25f, 0.25f, 1.f);
    scene.fg()->attach(overlay_rotation_clock_);
    overlay_rotation_clock_->visible_ = false;
    // circle to show fixed-size  ROTATION
    overlay_rotation_clock_hand_ = new Symbol(Symbol::CLOCK_H);
    overlay_rotation_clock_hand_->color = glm::vec4( COLOR_APPEARANCE_SOURCE, 1.f );
    overlay_rotation_clock_hand_->scale_ = glm::vec3(0.25f, 0.25f, 1.f);
    scene.fg()->attach(overlay_rotation_clock_hand_);
    overlay_rotation_clock_hand_->visible_ = false;
    overlay_rotation_fix_ = new Symbol(Symbol::SQUARE);
    overlay_rotation_fix_->color = glm::vec4( COLOR_APPEARANCE_SOURCE, 1.f );
    overlay_rotation_fix_->scale_ = glm::vec3(0.25f, 0.25f, 1.f);
    scene.fg()->attach(overlay_rotation_fix_);
    overlay_rotation_fix_->visible_ = false;
    // circle to show the center of ROTATION
    overlay_rotation_ = new Symbol(Symbol::CIRCLE);
    overlay_rotation_->color = glm::vec4( COLOR_APPEARANCE_SOURCE, 1.f );
    overlay_rotation_->scale_ = glm::vec3(0.25f, 0.25f, 1.f);
    scene.fg()->attach(overlay_rotation_);
    overlay_rotation_->visible_ = false;

    // Mask draw
    mask_cursor_paint_ = 1;
    mask_cursor_shape_ = 1;
    stored_mask_size_ = glm::vec3(0.f);
    mask_cursor_circle_ = new Mesh("mesh/icon_circle.ply");
    mask_cursor_circle_->scale_ = glm::vec3(0.2f, 0.2f, 1.f);
    mask_cursor_circle_->shader()->color = glm::vec4( COLOR_APPEARANCE_MASK, 0.8f );
    mask_cursor_circle_->visible_ = false;
    scene.fg()->attach(mask_cursor_circle_);
    mask_cursor_square_ = new Mesh("mesh/icon_square.ply");
    mask_cursor_square_->scale_ = glm::vec3(0.2f, 0.2f, 1.f);
    mask_cursor_square_->shader()->color = glm::vec4( COLOR_APPEARANCE_MASK, 0.8f );
    mask_cursor_square_->visible_ = false;
    scene.fg()->attach(mask_cursor_square_);
    mask_cursor_crop_ = new Mesh("mesh/icon_crop.ply");
    mask_cursor_crop_->scale_ = glm::vec3(1.4f, 1.4f, 1.f);
    mask_cursor_crop_->shader()->color = glm::vec4( COLOR_APPEARANCE_MASK, 0.9f );
    mask_cursor_crop_->visible_ = false;
    scene.fg()->attach(mask_cursor_crop_);

    stored_mask_size_ = glm::vec3(0.f);
    show_cursor_forced_ = false;
}

void TextureView::update(float dt)
{
    View::update(dt);

    // a more complete update is requested (e.g. after switching to view)
    if  (View::need_deep_update_ > 0 || edit_source_ != Mixer::manager().currentSource()) {
        need_edit_update_ = true;
    }

}

void TextureView::resize ( int scale )
{
    float z = CLAMP(0.01f * (float) scale, 0.f, 1.f);
    z *= z;
    z *= APPEARANCE_MAX_SCALE - APPEARANCE_MIN_SCALE;
    z += APPEARANCE_MIN_SCALE;
    scene.root()->scale_.x = z;
    scene.root()->scale_.y = z;

    // Clamp translation to acceptable area
    glm::vec3 border(2.f * Mixer::manager().session()->frame()->aspectRatio(), 2.f, 0.f);
    scene.root()->translation_ = glm::clamp(scene.root()->translation_, -border, border);
}

int TextureView::size ()
{
    float z = (scene.root()->scale_.x - APPEARANCE_MIN_SCALE) / (APPEARANCE_MAX_SCALE - APPEARANCE_MIN_SCALE);
    return (int) ( sqrt(z) * 100.f);
}

void  TextureView::recenter ()
{
    // restore default view
    restoreSettings();
}

void TextureView::select(glm::vec2 A, glm::vec2 B)
{
    // unproject mouse coordinate into scene coordinates
    glm::vec3 scene_point_A = Rendering::manager().unProject(A);
    glm::vec3 scene_point_B = Rendering::manager().unProject(B);

    // picking visitor traverses the scene
    PickingVisitor pv(scene_point_A, scene_point_B, true); // here is the difference
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

bool TextureView::canSelect(Source *s) {

    return ( s!=nullptr && ( s == Mixer::manager().currentSource() || s == edit_source_ ));
}

View::Cursor TextureView::over (glm::vec2 pos)
{
    mask_cursor_circle_->visible_ = false;
    mask_cursor_square_->visible_ = false;
    mask_cursor_crop_->visible_   = false;

    if (edit_source_ != nullptr)
    {
        glm::vec3 scene_pos = Rendering::manager().unProject(pos, scene.root()->transform_);
        glm::vec2 P(scene_pos);
        glm::vec2 S(preview_surface_->scale_);
        mask_cursor_circle_->translation_ = glm::vec3(P, 0.f);
        mask_cursor_square_->translation_ = glm::vec3(P, 0.f);
        mask_cursor_crop_->translation_ = glm::vec3(P, 0.f);

        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantCaptureMouse || show_cursor_forced_)
        {
            // show paint brush cursor
            if (edit_source_->maskShader()->mode == MaskShader::PAINT) {
                if (mask_cursor_paint_ > 0) {
                    S += glm::vec2(Settings::application.brush.x);
                    if ( ABS(P.x) < S.x  && ABS(P.y) < S.y ) {
                        mask_cursor_circle_->visible_ = Settings::application.brush.z < 1.0;
                        mask_cursor_square_->visible_ = Settings::application.brush.z > 0.0;
                        edit_source_->maskShader()->option = mask_cursor_paint_;
                        if (mask_cursor_paint_ > 1) {
                            ImVec4 c = ImGuiToolkit::HighlightColor(false);
                            mask_cursor_circle_->shader()->color = glm::vec4(c.x, c.y, c.z, 0.8f );
                            mask_cursor_square_->shader()->color = glm::vec4(c.x, c.y, c.z, 0.8f );
                        } else {
                            ImVec4 c = ImGuiToolkit::HighlightColor();
                            mask_cursor_circle_->shader()->color = glm::vec4(c.x, c.y, c.z, 0.8f );
                            mask_cursor_square_->shader()->color = glm::vec4(c.x, c.y, c.z, 0.8f );
                        }
                    }
                    else {
                        edit_source_->maskShader()->option = 0;
                    }
                }
            }
            // show crop cursor
            else if (edit_source_->maskShader()->mode == MaskShader::SHAPE) {
                if (mask_cursor_shape_ > 0) {
                    mask_cursor_crop_->visible_   = true;
                    mask_cursor_crop_->scale_ = glm::vec3(1.4f /scene.root()->scale_.x, 1.4f / scene.root()->scale_.x, 1.f);
                }
            }
        }
    }

    return Cursor();
}

std::pair<Node *, glm::vec2> TextureView::pick(glm::vec2 P)
{
    // prepare empty return value
    std::pair<Node *, glm::vec2> pick = { nullptr, glm::vec2(0.f) };

    // unproject mouse coordinate into scene coordinates
    glm::vec3 scene_point_ = Rendering::manager().unProject(P);

    // picking visitor traverses the scene (force to find source)
    PickingVisitor pv(scene_point_, true);
    scene.accept(pv);

    // picking visitor found nodes?
    if ( !pv.empty()) {
        // keep edit source active if it is clicked
        // AND if the cursor is not for drawing
        Source *current = edit_source_;
        if (current != nullptr) {

            // special case for drawing in the mask
            if ( current->maskShader()->mode == MaskShader::PAINT && mask_cursor_paint_ > 0) {
                pick = { mask_cursor_circle_, P };
                return pick;
            }
            // special case for cropping the mask shape
            else if ( current->maskShader()->mode == MaskShader::SHAPE && mask_cursor_shape_ > 0) {
                pick = { mask_cursor_crop_, P };
                return pick;
            }

            // find if the edit source was picked
            auto itp = pv.rbegin();
            for (; itp != pv.rend(); itp++){
                // test if source contains this node
                Source::hasNode is_in_source( (*itp).first );
                if ( is_in_source( current ) ){
                    // a node in the current source was clicked !
                    pick = *itp;
                    break;
                }
            }
            // not found: the edit source was not clicked
            if ( itp == pv.rend() ) {
                current = nullptr;
            }
            // picking on the menu handle
            else if ( !current->locked() && pick.first == current->handles_[mode_][Handles::MENU] ) {
                // show context menu
                openContextMenu(MENU_SOURCE);
            }
            // pick on the lock icon; unlock source
            else if ( UserInterface::manager().ctrlModifier() && pick.first == current->lock_ ) {
                lock(current, false);
                pick = { current->locker_,  pick.second };
            }
            // pick on the open lock icon; lock source and cancel pick
            else if ( UserInterface::manager().ctrlModifier() && pick.first == current->unlock_ ) {
                lock(current, true);
                current = nullptr;
            }
        }

        // not the edit source (or no edit source)
        if (current == nullptr) {
            // cancel pick
            pick = { nullptr, glm::vec2(0.f) };
        }

    }

    return pick;
}

void TextureView::adjustBackground()
{
    // by default consider edit source is null
    mask_node_->visible_ = false;
    float image_original_width = 1.f;
    glm::vec3 scale = glm::vec3(1.f);
    preview_surface_->setTextureIndex( Resource::getTextureTransparent() );

    // if its a valid index
    if (edit_source_ != nullptr) {
        // update rendering frame to match edit source AR
        image_original_width = edit_source_->frame()->aspectRatio();
        scale = edit_source_->mixingsurface_->scale_;
        preview_surface_->setTextureIndex( edit_source_->frame()->texture() );
        preview_shader_->mask_texture = edit_source_->blendingShader()->mask_texture;
        preview_surface_->scale_ = scale;
        // mask appearance
        mask_node_->visible_ = edit_source_->maskShader()->mode > MaskShader::PAINT && mask_cursor_shape_ > 0;

        int shape = edit_source_->maskShader()->shape;
        mask_circle_->visible_ = shape == MaskShader::ELLIPSE;
        mask_square_->visible_ = shape == MaskShader::OBLONG || shape == MaskShader::RECTANGLE;
        mask_horizontal_->visible_ = shape == MaskShader::HORIZONTAL;
        mask_vertical_->visible_ = shape == MaskShader::VERTICAL;

        // symetrical shapes
        if ( shape < MaskShader::HORIZONTAL){
            mask_node_->scale_ = scale * glm::vec3(edit_source_->maskShader()->size, 1.f);
            mask_node_->translation_ = glm::vec3(0.f);
        }
        // vertical
        else if ( shape > MaskShader::HORIZONTAL ) {
            mask_node_->scale_ = glm::vec3(1.f, scale.y, 1.f);
            mask_node_->translation_ = glm::vec3(edit_source_->maskShader()->size.x * scale.x, 0.f, 0.f);
        }
        // horizontal
        else {
            mask_node_->scale_ = glm::vec3(scale.x, 1.f, 1.f);
            mask_node_->translation_ = glm::vec3(0.f, edit_source_->maskShader()->size.y * scale.y, 0.f);
        }

    }

    // background scene
    background_surface_->scale_.x = image_original_width;
    background_surface_->scale_.y = 1.f;
    background_frame_->scale_.x = image_original_width;
    vertical_mark_->translation_.x = -image_original_width;
    preview_frame_->scale_ = scale;
    preview_checker_->scale_ = scale;
    glm::mat4 Ar  = glm::scale(glm::identity<glm::mat4>(), scale );
    static glm::mat4 Tra = glm::scale(glm::translate(glm::identity<glm::mat4>(), glm::vec3( -32.f, -32.f, 0.f)), glm::vec3( 64.f, 64.f, 1.f));
    preview_checker_->shader()->iTransform = Ar * Tra;

}

Source *TextureView::getEditOrCurrentSource()
{
    // cancel multiple selection
    if (Mixer::selection().size() > 1) {
        Source *s = Mixer::manager().currentSource();
        Mixer::manager().unsetCurrentSource();
        if ( s == nullptr )
            s = Mixer::selection().front();
        Mixer::selection().clear();
        Mixer::manager().setCurrentSource(s);
    }

    // get current source
    Source *_source = Mixer::manager().currentSource();

    // no current source?
    if (_source == nullptr) {
        // if something can be selected
        if ( !Mixer::manager().session()->empty()) {
            // return the edit source, if exists
            if (edit_source_ != nullptr)
                _source = Mixer::manager().findSource(edit_source_->id());
        }
    }

    if (_source != nullptr && _source->failed() ) {
        _source = nullptr;
    }

    return _source;
}

void TextureView::draw()
{
    // edit view needs to be updated (source changed)
    if ( need_edit_update_ )
    {
        need_edit_update_ = false;

        // now, follow the change of current source
        // & remember source to edit
        edit_source_ = getEditOrCurrentSource();

        // update background and frame to match editsource
        adjustBackground();
    }

    // draw marks in axis
    if (edit_source_ != nullptr && show_scale_){
        if (edit_source_->maskShader()->shape != MaskShader::HORIZONTAL){
            DrawVisitor dv(horizontal_mark_, Rendering::manager().Projection());
            glm::vec3 dT = glm::vec3( -0.2f * edit_source_->mixingsurface_->scale_.x, 0.f, 0.f);
            glm::mat4 T = glm::translate(glm::identity<glm::mat4>(), dT);
            dv.loop(6, T);
            scene.accept(dv);
            dT = glm::vec3( +0.2f * edit_source_->mixingsurface_->scale_.x, 0.f, 0.f);
            T = glm::translate(glm::identity<glm::mat4>(), dT);
            dv.loop(6, T);
            scene.accept(dv);
        }
        if (edit_source_->maskShader()->shape != MaskShader::VERTICAL){
            DrawVisitor dv(vertical_mark_, Rendering::manager().Projection());
            glm::vec3 dT = glm::vec3( 0.f, -0.2f * edit_source_->mixingsurface_->scale_.y, 0.f);
            glm::mat4 T = glm::translate(glm::identity<glm::mat4>(), dT);
            dv.loop(6, T);
            scene.accept(dv);
            dT = glm::vec3( 0.f, +0.2f * edit_source_->mixingsurface_->scale_.y, 0.f);
            T = glm::translate(glm::identity<glm::mat4>(), dT);
            dv.loop(6, T);
            scene.accept(dv);
        }
    }

    // draw general view
    Shader::force_blending_opacity = true;
    View::draw();
    Shader::force_blending_opacity = false;

    // if source active
    if (edit_source_ != nullptr){

        // force to redraw the frame of the edit source (even if source is not visible)
        DrawVisitor dv(edit_source_->groups_[mode_], Rendering::manager().Projection(), true);
        scene.accept(dv);

        // display interface
        // Locate window at upper left corner
        glm::vec2 P = glm::vec2(-background_frame_->scale_.x - 0.02f, background_frame_->scale_.y + 0.01 );
        P = Rendering::manager().project(glm::vec3(P, 0.f), scene.root()->transform_, false);
        // Set window position depending on icons size
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ImGui::SetNextWindowPos(ImVec2(P.x, P.y - 1.5f * ImGui::GetFrameHeight() ), ImGuiCond_Always);
        if (ImGui::Begin("##AppearanceMaskOptions", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground
                         | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings
                         | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus ))
        {

            // style grey
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.0f));  // 1
            ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.14f, 0.14f, 0.14f, 0.9f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.14f, 0.f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.24f, 0.24f, 0.24f, 0.46f));
            ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.85f, 0.85f, 0.85f, 0.86f));
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.95f, 0.95f, 0.95f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.24f, 0.24f, 0.46f));
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.36f, 0.36f, 0.36f, 0.9f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.36f, 0.36f, 0.36f, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.88f, 0.88f, 0.88f, 0.73f));
            ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.83f, 0.83f, 0.84f, 0.78f));
            ImGui::PushStyleColor(ImGuiCol_TabHovered, ImVec4(0.53f, 0.53f, 0.53f, 0.60f));
            ImGui::PushStyleColor(ImGuiCol_TabActive, ImVec4(0.40f, 0.40f, 0.40f, 1.00f));   // 14 colors

            int mode = edit_source_->maskShader()->mode;
            ImGui::SetNextItemWidth( ImGui::GetTextLineHeight() * 2.6);
            if ( ImGui::Combo("##Mask", &mode, MaskShader::mask_names, IM_ARRAYSIZE(MaskShader::mask_names) ) ) {
                edit_source_->maskShader()->mode = mode;
                if (mode == MaskShader::NONE)
                    Mixer::manager().setCurrentSource(edit_source_);
                else if (mode == MaskShader::PAINT)
                    edit_source_->storeMask();
                edit_source_->touch();
                need_edit_update_ = true;
                // store action history
                std::ostringstream oss;
                oss << edit_source_->name() << ": Mask " << (mode>1?"Shape":(mode>0?"Paint":"None"));
                Action::manager().store(oss.str());
            }

            // GUI for drawing mask
            if (edit_source_->maskShader()->mode == MaskShader::PAINT) {

                ImGui::SameLine();
                ImGuiToolkit::HelpToolTip( ICON_FA_EDIT "\tMask paint \n\n"
                                          ICON_FA_MOUSE_POINTER "\t  Edit texture\n"
                                          ICON_FA_PAINT_BRUSH "\tBrush\n"
                                          ICON_FA_ERASER "\tEraser\n\n"
                                          ICON_FA_CIRCLE "\tBrush shape\n"
                                          ICON_FA_DOT_CIRCLE  "\tBrush size\n"
                                          ICON_FA_FEATHER_ALT "\tBrush Pressure\n\n"
                                          ICON_FA_MAGIC "\tEffects\n"
                                          ICON_FA_FOLDER_OPEN "\tOpen image");

                // select cursor
                static bool on = true;
                ImGui::SameLine(0, 60);
                on = mask_cursor_paint_ == 0;
                if (ImGuiToolkit::ButtonToggle(ICON_FA_MOUSE_POINTER, &on)) {
                    Mixer::manager().setCurrentSource(edit_source_);
                    mask_cursor_paint_ = 0;
                }

                ImGui::SameLine();
                on = mask_cursor_paint_ == 1;
                if (ImGuiToolkit::ButtonToggle(ICON_FA_PAINT_BRUSH, &on)) {
                    Mixer::manager().unsetCurrentSource();
                    mask_cursor_paint_ = 1;
                }

                ImGui::SameLine();
                on = mask_cursor_paint_ == 2;
                if (ImGuiToolkit::ButtonToggle(ICON_FA_ERASER, &on)) {
                    Mixer::manager().unsetCurrentSource();
                    mask_cursor_paint_ = 2;
                }

                if (mask_cursor_paint_ > 0) {

                    ImGui::SameLine(0, 50);
                    ImGui::SetNextItemWidth( ImGui::GetTextLineHeight() * 2.6);
                    const char* items[] = { ICON_FA_CIRCLE, ICON_FA_SQUARE };
                    static int item = 0;
                    item = (int) round(Settings::application.brush.z);
                    if(ImGui::Combo("##BrushShape", &item, items, IM_ARRAYSIZE(items))) {
                        Settings::application.brush.z = float(item);
                    }

                    ImGui::SameLine();
                    show_cursor_forced_ = false;
                    if (ImGui::Button(ICON_FA_DOT_CIRCLE ICON_FA_SORT_DOWN ))
                        ImGui::OpenPopup("brush_size_popup");
                    if (ImGui::BeginPopup("brush_size_popup", ImGuiWindowFlags_NoMove))
                    {
                        int pixel_size_min = int(0.05 * edit_source_->frame()->height() );
                        int pixel_size_max = int(2.0 * edit_source_->frame()->height() );
                        int pixel_size = int(Settings::application.brush.x * edit_source_->frame()->height() );
                        show_cursor_forced_ = true;
                        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_DEFAULT);
                        ImGuiToolkit::Indication("Large  ", 16, 1, ICON_FA_ARROW_RIGHT);
                        if (ImGui::VSliderInt("##BrushSize", ImVec2(30,260), &pixel_size, pixel_size_min, pixel_size_max, "") ){
                            Settings::application.brush.x = CLAMP(float(pixel_size) / edit_source_->frame()->height(), BRUSH_MIN_SIZE, BRUSH_MAX_SIZE);
                        }
                        if (ImGui::IsItemHovered() || ImGui::IsItemActive() )  {
                            ImGui::BeginTooltip();
                            ImGui::Text("%d px", pixel_size);
                            ImGui::EndTooltip();
                        }
                        ImGuiToolkit::Indication("Small  ", 15, 1, ICON_FA_ARROW_LEFT);
                        ImGui::PopFont();
                        ImGui::EndPopup();
                    }
                    // make sure the visual brush is up to date
                    glm::vec2 s = glm::vec2(Settings::application.brush.x);
                    mask_cursor_circle_->scale_ = glm::vec3(s * 1.16f, 1.f);
                    mask_cursor_square_->scale_ = glm::vec3(s * 1.75f, 1.f);

                    ImGui::SameLine();
                    if (ImGui::Button(ICON_FA_FEATHER_ALT ICON_FA_SORT_DOWN ))
                        ImGui::OpenPopup("brush_pressure_popup");
                    if (ImGui::BeginPopup("brush_pressure_popup", ImGuiWindowFlags_NoMove))
                    {
                        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_DEFAULT);
                        ImGuiToolkit::Indication("Light  ", ICON_FA_FEATHER_ALT, ICON_FA_ARROW_UP);
                        ImGui::VSliderFloat("##BrushPressure", ImVec2(30,260), &Settings::application.brush.y, BRUSH_MAX_PRESS, BRUSH_MIN_PRESS, "", 0.3f);
                        if (ImGui::IsItemHovered() || ImGui::IsItemActive() )  {
                            ImGui::BeginTooltip();
                            ImGui::Text("%.1f%%", Settings::application.brush.y * 100.0);
                            ImGui::EndTooltip();
                        }
                        ImGuiToolkit::Indication("Heavy  ", ICON_FA_WEIGHT_HANGING, ICON_FA_ARROW_DOWN);
                        ImGui::PopFont();
                        ImGui::EndPopup();
                    }

                    // store mask if changed, reset after applied
                    if (edit_source_->maskShader()->effect > 0)
                        edit_source_->storeMask();
                    edit_source_->maskShader()->effect = 0;

                    // menu for effects
                    ImGui::SameLine(0, 60);
                    if (ImGui::Button(ICON_FA_MAGIC ICON_FA_SORT_DOWN ))
                        ImGui::OpenPopup( "brush_menu_popup" );
                    if (ImGui::BeginPopup( "brush_menu_popup" ))
                    {
                        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_DEFAULT);
                        std::ostringstream oss;
                        oss << edit_source_->name();
                        int e = 0;
                        if (ImGui::Selectable( ICON_FA_BACKSPACE "\tClear")) {
                            e = 1;
                            oss << ": Clear " << MASK_PAINT_ACTION_LABEL;
                        }
                        if (ImGui::Selectable( ICON_FA_ADJUST "\tInvert")) {
                            e = 2;
                            oss << ": Invert " << MASK_PAINT_ACTION_LABEL;
                        }
                        if (ImGui::Selectable( ICON_FA_WAVE_SQUARE "\tEdge")) {
                            e = 3;
                            oss << ": Edge " << MASK_PAINT_ACTION_LABEL;
                        }
                        if (e>0) {
                            edit_source_->maskShader()->effect = e;
                            edit_source_->maskShader()->cursor = glm::vec4(100.0, 100.0, 0.f, 0.f);
                            edit_source_->touch();
                            Action::manager().store(oss.str());
                        }
                        ImGui::PopFont();
                        ImGui::EndPopup();
                    }

                    static DialogToolkit::OpenImageDialog maskdialog("Select Image");

                    ImGui::SameLine();
                    if (ImGui::Button(ICON_FA_FOLDER_OPEN))
                        maskdialog.open();
                    if (maskdialog.closed() && !maskdialog.path().empty())
                    {
                        FrameBufferImage *img = new FrameBufferImage(maskdialog.path());
                        if (edit_source_->maskbuffer_->fill( img )) {
                            // apply mask filled
                            edit_source_->storeMask();
                            // store history
                            std::ostringstream oss;
                            oss << edit_source_->name() << ": Mask fill with " << maskdialog.path();
                            Action::manager().store(oss.str());
                        }
                    }

                }
                // disabled info
                else {
                    ImGui::SameLine(0, 60);
                    ImGui::TextDisabled( "Paint mask" );
                }

            }
            // GUI for all other masks
            else if (edit_source_->maskShader()->mode == MaskShader::SHAPE) {

                ImGui::SameLine();
                ImGuiToolkit::HelpToolTip( ICON_FA_SHAPES "\tMask shape\n\n"
                                          ICON_FA_MOUSE_POINTER "\t  Edit texture\n"
                                          ICON_FA_CROP_ALT "\tCrop & Edit shape\n\n"
                                          ICON_FA_CARET_DOWN "  \tShape of the mask\n"
                                          ICON_FA_RADIATION_ALT "\tShape blur");

                // select cursor
                static bool on = true;
                ImGui::SameLine(0, 60);
                on = mask_cursor_shape_ == 0;
                if (ImGuiToolkit::ButtonToggle(ICON_FA_MOUSE_POINTER, &on)) {
                    Mixer::manager().setCurrentSource(edit_source_);
                    need_edit_update_ = true;
                    mask_cursor_shape_ = 0;
                }

                ImGui::SameLine();
                on = mask_cursor_shape_ == 1;
                if (ImGuiToolkit::ButtonToggle(ICON_FA_CROP_ALT, &on)) {
                    Mixer::manager().unsetCurrentSource();
                    need_edit_update_ = true;
                    mask_cursor_shape_ = 1;
                }

                int shape = edit_source_->maskShader()->shape;
                int blur_percent = int(edit_source_->maskShader()->blur * 100.f);

                if (mask_cursor_shape_ > 0) {

                    ImGui::SameLine(0, 50);
                    ImGui::SetNextItemWidth( ImGui::GetTextLineHeight() * 6.5f);
                    if ( ImGui::Combo("##MaskShape", &shape, MaskShader::mask_shapes, IM_ARRAYSIZE(MaskShader::mask_shapes) ) ) {
                        edit_source_->maskShader()->shape = shape;
                        edit_source_->touch();
                        need_edit_update_ = true;
                        // store action history
                        std::ostringstream oss;
                        oss << edit_source_->name() << ": Mask Shape " << MaskShader::mask_shapes[shape];
                        Action::manager().store(oss.str());
                    }

                    ImGui::SameLine(0, 20);
                    if (ImGui::Button(ICON_FA_RADIATION_ALT ICON_FA_SORT_DOWN ))
                        ImGui::OpenPopup("shape_smooth_popup");
                    if (ImGui::BeginPopup("shape_smooth_popup", ImGuiWindowFlags_NoMove))
                    {
                        static bool smoothchanged = false;
                        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_DEFAULT);
                        ImGuiToolkit::Indication("Blur  ", 7, 16, ICON_FA_ARROW_UP);
                        if (ImGui::VSliderInt("##shapeblur", ImVec2(30,260), &blur_percent, 0, 100, "") ){
                            edit_source_->maskShader()->blur = float(blur_percent) / 100.f;
                            edit_source_->touch();
                            need_edit_update_ = true;
                            smoothchanged = true;
                        }
                        else if (smoothchanged && ImGui::IsMouseReleased(ImGuiMouseButton_Left)){
                            // store action history
                            std::ostringstream oss;
                            oss << edit_source_->name() << ": Mask Shape Blur " << blur_percent << "%";
                            Action::manager().store(oss.str());
                            smoothchanged = false;
                        }
                        if (ImGui::IsItemHovered() || ImGui::IsItemActive() )  {
                            ImGui::BeginTooltip();
                            ImGui::Text("%.d%%", blur_percent);
                            ImGui::EndTooltip();
                        }
                        ImGuiToolkit::Indication("Sharp ", 8, 16, ICON_FA_ARROW_DOWN);
                        ImGui::PopFont();
                        ImGui::EndPopup();
                    }

                }
                // disabled info
                else {
                    ImGui::SameLine(0, 60);
                    ImGui::TextDisabled( "%s", MaskShader::mask_shapes[shape] );
                    ImGui::SameLine();
                    ImGui::TextDisabled( "mask");
                }
            }
            else {// mode == MaskShader::NONE
                ImGui::SameLine();
                ImGuiToolkit::HelpToolTip( ICON_FA_EXPAND "\tNo mask\n\n"
                                          ICON_FA_MOUSE_POINTER "\t  Edit texture\n");
                // always active mouse pointer
                ImGui::SameLine(0, 60);
                bool on = true;
                ImGuiToolkit::ButtonToggle(ICON_FA_MOUSE_POINTER, &on);
                ImGui::SameLine(0, 60);
                ImGui::TextDisabled( "No mask" );
            }

            ImGui::PopStyleColor(14);  // 14 colors
            ImGui::End();
        }
        ImGui::PopFont();

    }

    // display popup menu
    if (show_context_menu_ == MENU_SOURCE) {
        ImGui::OpenPopup( "AppearanceSourceContextMenu" );
        show_context_menu_ = MENU_NONE;
    }
    if (ImGui::BeginPopup("AppearanceSourceContextMenu")) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(COLOR_APPEARANCE_SOURCE, 1.f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(COLOR_MENU_HOVERED, 0.5f));
        Source *s = Mixer::manager().currentSource();
        if (s != nullptr) {

            if (s->textureMirrored()) {
                if (ImGui::Selectable( ICON_FA_BORDER_NONE "  Repeat " )){
                    s->setTextureMirrored(false);
                    Action::manager().store(s->name() + std::string(": Texture Repeat"));
                }
            } else {
                if (ImGui::Selectable( ICON_FA_BORDER_NONE "  Mirror " )){
                    s->setTextureMirrored(true);
                    Action::manager().store(s->name() + std::string(": Texture Mirror"));
                }
            }
            ImGui::Separator();

            if (ImGui::Selectable( ICON_FA_VECTOR_SQUARE "  Reset" )){
                s->group(mode_)->scale_ = glm::vec3(1.f);
                s->group(mode_)->rotation_.z = 0;
                s->group(mode_)->crop_ = glm::vec3(1.f);
                s->group(mode_)->translation_ = glm::vec3(0.f);
                s->touch();
                Action::manager().store(s->name() + std::string(": Texture Reset"));
            }
            if (ImGui::Selectable( ICON_FA_CROSSHAIRS "  Reset position" )){
                s->group(mode_)->translation_ = glm::vec3(0.f);
                s->touch();
                Action::manager().store(s->name() + std::string(": Texture Reset position"));
            }
            if (ImGui::Selectable( ICON_FA_COMPASS "  Reset rotation" )){
                s->group(mode_)->rotation_.z = 0;
                s->touch();
                Action::manager().store(s->name() + std::string(": Texture Reset rotation"));
            }
            if (ImGui::Selectable( ICON_FA_EXPAND_ALT "  Reset aspect ratio" )){
                s->group(mode_)->scale_.x = s->group(mode_)->scale_.y;
                s->group(mode_)->scale_.x *= s->group(mode_)->crop_.x / s->group(mode_)->crop_.y;
                s->touch();
                Action::manager().store(s->name() + std::string(": Texture Reset aspect ratio"));
            }
        }
        ImGui::PopStyleColor(2);
        ImGui::EndPopup();
    }

    show_scale_ = false;
}


View::Cursor TextureView::grab (Source *s, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2> pick)
{
    std::ostringstream info;
    View::Cursor ret = Cursor();

    // grab coordinates in scene-View reference frame
    glm::vec3 scene_from = Rendering::manager().unProject(from, scene.root()->transform_);
    glm::vec3 scene_to   = Rendering::manager().unProject(to, scene.root()->transform_);
    glm::vec3 scene_translation = scene_to - scene_from;

    // Not grabbing a source
    if (!s) {
        // work on the edited source
        if ( edit_source_ != nullptr ) {

            info << edit_source_->name() << ": ";

            if ( pick.first == mask_cursor_circle_ ) {

                // inform shader of a cursor action : coordinates and crop scaling
                edit_source_->maskShader()->cursor = glm::vec4(scene_to.x, scene_to.y,
                                                            edit_source_->mixingsurface_->scale_.x, edit_source_->mixingsurface_->scale_.y);
                edit_source_->touch();
                // action label
                info << MASK_PAINT_ACTION_LABEL;
                // cursor indication - no info, just cursor
                ret.type = Cursor_Hand;
                // store action in history
                current_action_ = info.str();
            }
            else if ( pick.first == mask_cursor_crop_ ) {

                // special case for horizontal and vertical Shapes
                bool hv = edit_source_->maskShader()->shape > MaskShader::RECTANGLE;
                // match edit source AR
                glm::vec3 val = edit_source_->mixingsurface_->scale_;
                // use cursor translation to scale by quadrant
                val = glm::sign( hv ? glm::vec3(1.f) : scene_from) * glm::vec3(scene_translation / val);
                // relative change of stored mask size
                val += stored_mask_size_;
                // apply discrete scale with ALT modifier
                if (UserInterface::manager().altModifier()) {
                    val.x = ROUND(val.x, 5.f);
                    val.y = ROUND(val.y, 5.f);
                    show_scale_ = true;
                }
                // Clamp | val | < 2.0
                val = glm::sign(val) * glm::min( glm::abs(val), glm::vec3(2.f));
                // clamp values for correct effect
                if (edit_source_->maskShader()->shape == MaskShader::HORIZONTAL)
                    edit_source_->maskShader()->size.y = val.y;
                else if (edit_source_->maskShader()->shape == MaskShader::VERTICAL)
                    edit_source_->maskShader()->size.x = val.x;
                else
                    edit_source_->maskShader()->size = glm::max(glm::abs(glm::vec2(val)), glm::vec2(0.2));
//                edit_source_->maskShader()->size = glm::max( glm::min( glm::vec2(val), glm::vec2(2.f)), glm::vec2(hv?-2.f:0.2f));
                edit_source_->touch();
                // update
                need_edit_update_ = true;
                // action label
                info << "Texture Mask " << std::fixed << std::setprecision(3) << edit_source_->maskShader()->size.x;
                info << " x "  << edit_source_->maskShader()->size.y;
                // cursor indication - no info, just cursor
                ret.type = Cursor_Hand;
                // store action in history
                current_action_ = info.str();
            }

        }
        return ret;
    }

    // do not operate on locked source
    if (s->locked())
        return ret;

    Group *sourceNode = s->group(mode_); // groups_[View::APPEARANCE]

    // make sure matrix transform of stored status is updated
    s->stored_status_->update(0);

    // grab coordinates in source-root reference frame
    glm::vec4 source_from = glm::inverse(s->stored_status_->transform_) * glm::vec4( scene_from,  1.f );
    glm::vec4 source_to   = glm::inverse(s->stored_status_->transform_) * glm::vec4( scene_to,  1.f );
    glm::vec3 source_scaling     = glm::vec3(source_to) / glm::vec3(source_from);

    // which manipulation to perform?
    if (pick.first)  {
        // which corner was picked ?
        glm::vec2 corner = glm::round(pick.second);

        // transform from source center to corner
        glm::mat4 T = GlmToolkit::transform(glm::vec3(corner.x, corner.y, 0.f), glm::vec3(0.f, 0.f, 0.f),
                                            glm::vec3(1.f / s->frame()->aspectRatio(), 1.f, 1.f));

        // transformation from scene to corner:
        glm::mat4 scene_to_corner_transform = T * glm::inverse(s->stored_status_->transform_);
        glm::mat4 corner_to_scene_transform = glm::inverse(scene_to_corner_transform);

        // compute cursor movement in corner reference frame
        glm::vec4 corner_from = scene_to_corner_transform * glm::vec4( scene_from,  1.f );
        glm::vec4 corner_to   = scene_to_corner_transform * glm::vec4( scene_to,  1.f );
        // operation of scaling in corner reference frame
        glm::vec3 corner_scaling = glm::vec3(corner_to) / glm::vec3(corner_from);

        // convert source position in corner reference frame
        glm::vec4 center = scene_to_corner_transform * glm::vec4( s->stored_status_->translation_, 1.f);

        // picking on the resizing handles in the corners
        if ( pick.first == s->handles_[mode_][Handles::RESIZE] ) {

            // hide all other grips
            s->handles_[mode_][Handles::SCALE]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_H]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_V]->visible_ = false;
            s->handles_[mode_][Handles::ROTATE]->visible_ = false;
            s->handles_[mode_][Handles::MENU]->visible_ = false;

            // inform on which corner should be overlayed (opposite)
            s->handles_[mode_][Handles::RESIZE]->overlayActiveCorner(-corner);

            // RESIZE CORNER
            // proportional SCALING with SHIFT
            if (UserInterface::manager().shiftModifier()) {
                // calculate proportional scaling factor
                float factor = glm::length( glm::vec2( corner_to ) ) / glm::length( glm::vec2( corner_from ) );
                // scale node
                sourceNode->scale_ = s->stored_status_->scale_ * glm::vec3(factor, factor, 1.f);
                // discretized scaling with ALT
                if (UserInterface::manager().altModifier()) {
                    sourceNode->scale_.x = ROUND(sourceNode->scale_.x, 10.f);
                    factor = sourceNode->scale_.x / s->stored_status_->scale_.x;
                    sourceNode->scale_.y = s->stored_status_->scale_.y * factor;
                }
                // update corner scaling to apply to center coordinates
                corner_scaling = sourceNode->scale_ / s->stored_status_->scale_;
            }
            // non-proportional CORNER RESIZE  (normal case)
            else {
                // scale node
                sourceNode->scale_ = s->stored_status_->scale_ * corner_scaling;
                // discretized scaling with ALT
                if (UserInterface::manager().altModifier()) {
                    sourceNode->scale_.x = ROUND(sourceNode->scale_.x, 10.f);
                    sourceNode->scale_.y = ROUND(sourceNode->scale_.y, 10.f);
                    corner_scaling = sourceNode->scale_ / s->stored_status_->scale_;
                }
            }
            // transform source center (in corner reference frame)
            center = glm::scale(glm::identity<glm::mat4>(), corner_scaling) * center;
            // convert center back into scene reference frame
            center = corner_to_scene_transform * center;
            // apply to node
            sourceNode->translation_ = glm::vec3(center);
            // show cursor depending on diagonal (corner picked)
            T = glm::rotate(glm::identity<glm::mat4>(), s->stored_status_->rotation_.z, glm::vec3(0.f, 0.f, 1.f));
            T = glm::scale(T, s->stored_status_->scale_);
            corner = T * glm::vec4( corner, 0.f, 0.f );
            ret.type = corner.x * corner.y > 0.f ? Cursor_ResizeNESW : Cursor_ResizeNWSE;
            info << "Texture scale " << std::fixed << std::setprecision(3) << sourceNode->scale_.x;
            info << " x "  << sourceNode->scale_.y;

        }
        // picking on the BORDER RESIZING handles left or right
        else if ( pick.first == s->handles_[mode_][Handles::RESIZE_H] ) {

            // hide all other grips
            s->handles_[mode_][Handles::RESIZE]->visible_ = false;
            s->handles_[mode_][Handles::SCALE]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_V]->visible_ = false;
            s->handles_[mode_][Handles::ROTATE]->visible_ = false;
            s->handles_[mode_][Handles::MENU]->visible_ = false;

            // inform on which corner should be overlayed (opposite)
            s->handles_[mode_][Handles::RESIZE_H]->overlayActiveCorner(-corner);

            // SHIFT: HORIZONTAL SCALE to restore source aspect ratio
            if (UserInterface::manager().shiftModifier()) {
                sourceNode->scale_.x = ABS(sourceNode->scale_.y) * SIGN(sourceNode->scale_.x);
                corner_scaling = sourceNode->scale_ / s->stored_status_->scale_;
            }
            // HORIZONTAL RESIZE (normal case)
            else {
                // x scale only
                corner_scaling = glm::vec3(corner_scaling.x, 1.f, 1.f);
                // scale node
                sourceNode->scale_ = s->stored_status_->scale_ * corner_scaling;
                // POST-CORRECTION ; discretized scaling with ALT
                if (UserInterface::manager().altModifier()) {
                    sourceNode->scale_.x = ROUND(sourceNode->scale_.x, 10.f);
                    corner_scaling = sourceNode->scale_ / s->stored_status_->scale_;
                }
            }
            // transform source center (in corner reference frame)
            center = glm::scale(glm::identity<glm::mat4>(), corner_scaling) * center;
            // convert center back into scene reference frame
            center = corner_to_scene_transform * center;
            // apply to node
            sourceNode->translation_ = glm::vec3(center);
            // show cursor depending on angle
            float c = tan(sourceNode->rotation_.z);
            ret.type = ABS(c) > 1.f ? Cursor_ResizeNS : Cursor_ResizeEW;
            info << "Texture Scale " << std::fixed << std::setprecision(3) << sourceNode->scale_.x;
            info << " x "  << sourceNode->scale_.y;
        }
        // picking on the BORDER RESIZING handles top or bottom
        else if ( pick.first == s->handles_[mode_][Handles::RESIZE_V] ) {

            // hide all other grips
            s->handles_[mode_][Handles::RESIZE]->visible_ = false;
            s->handles_[mode_][Handles::SCALE]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_H]->visible_ = false;
            s->handles_[mode_][Handles::ROTATE]->visible_ = false;
            s->handles_[mode_][Handles::MENU]->visible_ = false;

            // inform on which corner should be overlayed (opposite)
            s->handles_[mode_][Handles::RESIZE_V]->overlayActiveCorner(-corner);

            // SHIFT: VERTICAL SCALE to restore source aspect ratio
            if (UserInterface::manager().shiftModifier()) {
                sourceNode->scale_.y = ABS(sourceNode->scale_.x) * SIGN(sourceNode->scale_.y);
                corner_scaling = sourceNode->scale_ / s->stored_status_->scale_;
            }
            // VERTICAL RESIZE (normal case)
            else {
                // y scale only
                corner_scaling = glm::vec3(1.f, corner_scaling.y, 1.f);
                // scale node
                sourceNode->scale_ = s->stored_status_->scale_ * corner_scaling;
                // POST-CORRECTION ; discretized scaling with ALT
                if (UserInterface::manager().altModifier()) {
                    sourceNode->scale_.y = ROUND(sourceNode->scale_.y, 10.f);
                    corner_scaling = sourceNode->scale_ / s->stored_status_->scale_;
                }
            }
            // transform source center (in corner reference frame)
            center = glm::scale(glm::identity<glm::mat4>(), corner_scaling) * center;
            // convert center back into scene reference frame
            center = corner_to_scene_transform * center;
            // apply to node
            sourceNode->translation_ = glm::vec3(center);
            // show cursor depending on angle
            float c = tan(sourceNode->rotation_.z);
            ret.type = ABS(c) > 1.f ? Cursor_ResizeEW : Cursor_ResizeNS;
            info << "Texture Scale " << std::fixed << std::setprecision(3) << sourceNode->scale_.x;
            info << " x "  << sourceNode->scale_.y;
        }
        // picking on the CENTRER SCALING handle
        else if ( pick.first == s->handles_[mode_][Handles::SCALE] ) {

            // hide all other grips
            s->handles_[mode_][Handles::RESIZE]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_H]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_V]->visible_ = false;
            s->handles_[mode_][Handles::ROTATE]->visible_ = false;
            s->handles_[mode_][Handles::MENU]->visible_ = false;

            // prepare overlay
            overlay_scaling_cross_->visible_ = false;
            overlay_scaling_grid_->visible_ = false;
            overlay_scaling_->visible_ = true;
            overlay_scaling_->translation_.x = s->stored_status_->translation_.x;
            overlay_scaling_->translation_.y = s->stored_status_->translation_.y;
            overlay_scaling_->rotation_.z = s->stored_status_->rotation_.z;
            overlay_scaling_->update(0);

            // PROPORTIONAL ONLY
            if (UserInterface::manager().shiftModifier()) {
                float factor = glm::length( glm::vec2( source_to ) ) / glm::length( glm::vec2( source_from ) );
                source_scaling = glm::vec3(factor, factor, 1.f);
                overlay_scaling_cross_->visible_ = true;
                overlay_scaling_cross_->copyTransform(overlay_scaling_);
            }
            // apply center scaling
            sourceNode->scale_ = s->stored_status_->scale_ * source_scaling;
            // POST-CORRECTION ; discretized scaling with ALT
            if (UserInterface::manager().altModifier()) {
                sourceNode->scale_.x = ROUND(sourceNode->scale_.x, 10.f);
                sourceNode->scale_.y = ROUND(sourceNode->scale_.y, 10.f);
                overlay_scaling_grid_->visible_ = true;
                overlay_scaling_grid_->copyTransform(overlay_scaling_);
            }
            // show cursor depending on diagonal
            corner = glm::sign(sourceNode->scale_);
            ret.type = (corner.x * corner.y) > 0.f ? Cursor_ResizeNWSE : Cursor_ResizeNESW;
            info << "Texture Scale " << std::fixed << std::setprecision(3) << sourceNode->scale_.x;
            info << " x "  << sourceNode->scale_.y;
        }
        // picking on the rotating handle
        else if ( pick.first == s->handles_[mode_][Handles::ROTATE] ) {

            // hide all other grips
            s->handles_[mode_][Handles::RESIZE]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_H]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_V]->visible_ = false;
            s->handles_[mode_][Handles::SCALE]->visible_ = false;
            s->handles_[mode_][Handles::MENU]->visible_ = false;
            // ROTATION on CENTER
            overlay_rotation_->visible_ = true;
            overlay_rotation_->translation_.x = s->stored_status_->translation_.x;
            overlay_rotation_->translation_.y = s->stored_status_->translation_.y;
            overlay_rotation_->update(0);
            overlay_rotation_fix_->visible_ = true;
            overlay_rotation_fix_->copyTransform(overlay_rotation_);
            overlay_rotation_clock_->visible_ = false;

            // rotation center to center of source (disregarding scale)
            glm::mat4 M = glm::translate(glm::identity<glm::mat4>(), s->stored_status_->translation_);
            source_from = glm::inverse(M) * glm::vec4( scene_from,  1.f );
            source_to   = glm::inverse(M) * glm::vec4( scene_to,  1.f );
            // compute rotation angle
            float angle = glm::orientedAngle( glm::normalize(glm::vec2(source_from)), glm::normalize(glm::vec2(source_to)));
            // apply rotation on Z axis
            sourceNode->rotation_ = s->stored_status_->rotation_ + glm::vec3(0.f, 0.f, angle);

            // POST-CORRECTION ; discretized rotation with ALT
            int degrees = int(  glm::degrees(sourceNode->rotation_.z) );
            if (UserInterface::manager().altModifier()) {
                degrees = (degrees / 10) * 10;
                sourceNode->rotation_.z = glm::radians( float(degrees) );
                overlay_rotation_clock_->visible_ = true;
                overlay_rotation_clock_->copyTransform(overlay_rotation_);
                info << "Texture Angle " << degrees << UNICODE_DEGREE;
            }
            else
                info << "Texture Angle " << std::fixed << std::setprecision(1) << glm::degrees(sourceNode->rotation_.z) << UNICODE_DEGREE;

            overlay_rotation_clock_hand_->visible_ = true;
            overlay_rotation_clock_hand_->translation_.x = s->stored_status_->translation_.x;
            overlay_rotation_clock_hand_->translation_.y = s->stored_status_->translation_.y;
            overlay_rotation_clock_hand_->rotation_.z = sourceNode->rotation_.z;
            overlay_rotation_clock_hand_->update(0);

            // show cursor for rotation
            ret.type = Cursor_Hand;
            // + SHIFT = no scaling /  NORMAL = with scaling
            if (!UserInterface::manager().shiftModifier()) {
                // compute scaling to match cursor
                float factor = glm::length( glm::vec2( source_to ) ) / glm::length( glm::vec2( source_from ) );
                source_scaling = glm::vec3(factor, factor, 1.f);
                // apply center scaling
                sourceNode->scale_ = s->stored_status_->scale_ * source_scaling;
                info << std::endl << "          Scale " << std::fixed << std::setprecision(3) << sourceNode->scale_.x;
                info << " x "  << sourceNode->scale_.y ;
                overlay_rotation_fix_->visible_ = false;
            }
        }
        // picking anywhere but on a handle: user wants to move the source
        else {
            ret.type = Cursor_ResizeAll;
            sourceNode->translation_ = s->stored_status_->translation_ + scene_translation;
            // discretized translation with ALT
            if (UserInterface::manager().altModifier()) {
                sourceNode->translation_.x = ROUND(sourceNode->translation_.x, 10.f);
                sourceNode->translation_.y = ROUND(sourceNode->translation_.y, 10.f);                
                // Show grid overlay for POSITION
                overlay_position_cross_->visible_ = true;
                overlay_position_cross_->translation_.x = sourceNode->translation_.x;
                overlay_position_cross_->translation_.y = sourceNode->translation_.y;
                overlay_position_cross_->update(0);
            }
            // Show center overlay for POSITION
            overlay_position_->visible_ = true;
            overlay_position_->translation_.x = sourceNode->translation_.x;
            overlay_position_->translation_.y = sourceNode->translation_.y;
            overlay_position_->update(0);
            // Show move cursor
            info << "Texture Shift " << std::fixed << std::setprecision(3) << sourceNode->translation_.x;
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


void TextureView::initiate()
{
    // View default initiation of action
    View::initiate();

    if ( edit_source_ != nullptr ) {
        stored_mask_size_ = glm::vec3(edit_source_->maskShader()->size, 0.0);

        // apply mask settings
        edit_source_->maskShader()->brush = Settings::application.brush;
    }
    else
        stored_mask_size_ = glm::vec3(0.f);
}

void TextureView::terminate(bool force)
{
    // special case for texture paint: store image on mouse release (end of action PAINT)
    if ( edit_source_ != nullptr && current_action_.find(MASK_PAINT_ACTION_LABEL) != std::string::npos ) {
        edit_source_->storeMask();
        edit_source_->maskShader()->cursor = glm::vec4(100.0, 100.0, 0.f, 0.f);
    }

    // View default termination of action
    View::terminate(force);

    // hide all overlays
    overlay_position_->visible_       = false;
    overlay_position_cross_->visible_ = false;
    overlay_scaling_grid_->visible_   = false;
    overlay_scaling_cross_->visible_  = false;
    overlay_scaling_->visible_        = false;
    overlay_rotation_clock_->visible_ = false;
    overlay_rotation_clock_hand_->visible_ = false;
    overlay_rotation_fix_->visible_   = false;
    overlay_rotation_->visible_       = false;

    // cancel of all handles overlays
    const glm::vec2 c(0.f, 0.f);
    for (auto sit = Mixer::manager().session()->begin();
         sit != Mixer::manager().session()->end(); sit++){

        (*sit)->handles_[mode_][Handles::RESIZE]->overlayActiveCorner(c);
        (*sit)->handles_[mode_][Handles::RESIZE_H]->overlayActiveCorner(c);
        (*sit)->handles_[mode_][Handles::RESIZE_V]->overlayActiveCorner(c);
        (*sit)->handles_[mode_][Handles::RESIZE]->visible_ = true;
        (*sit)->handles_[mode_][Handles::RESIZE_H]->visible_ = true;
        (*sit)->handles_[mode_][Handles::RESIZE_V]->visible_ = true;
        (*sit)->handles_[mode_][Handles::SCALE]->visible_ = true;
        (*sit)->handles_[mode_][Handles::ROTATE]->visible_ = true;
        (*sit)->handles_[mode_][Handles::MENU]->visible_ = true;
    }

}

void TextureView::arrow (glm::vec2 movement)
{
    Source *s = Mixer::manager().currentSource();
    if (s) {
        static float accumulator = 0.f;
        accumulator += dt_;

        glm::vec3 gl_Position_from = Rendering::manager().unProject(glm::vec2(0.f), scene.root()->transform_);
        glm::vec3 gl_Position_to   = Rendering::manager().unProject(movement, scene.root()->transform_);
        glm::vec3 gl_delta = gl_Position_to - gl_Position_from;

        Group *sourceNode = s->group(mode_);
        glm::vec3 alt_move_ = sourceNode->translation_;
        if (UserInterface::manager().altModifier()) {            
            if (accumulator > 100.f)
            {
                alt_move_ += glm::sign(gl_delta) * 0.1f;
                sourceNode->translation_.x = ROUND(alt_move_.x, 10.f);
                sourceNode->translation_.y = ROUND(alt_move_.y, 10.f);
                accumulator = 0.f;
            }
        }
        else {
            sourceNode->translation_ += gl_delta * ARROWS_MOVEMENT_FACTOR * dt_;
            accumulator = 0.f;
            alt_move_ = sourceNode->translation_;
        }

        // store action in history
        std::ostringstream info;
        info << "Texture Shift " << std::fixed << std::setprecision(3) << sourceNode->translation_.x;
        info << ", "  << sourceNode->translation_.y ;
        current_action_ = s->name() + ": " + info.str();

        // request update
        s->touch();
    }
    else if (edit_source_) {
        if (edit_source_->maskShader()->mode == MaskShader::PAINT) {
            if (mask_cursor_paint_ > 0) {
                glm::vec2 b = 0.02f * movement;
                Settings::application.brush.x = CLAMP(Settings::application.brush.x+b.x, BRUSH_MIN_SIZE, BRUSH_MAX_SIZE);
                Settings::application.brush.y = CLAMP(Settings::application.brush.y+b.y, BRUSH_MIN_PRESS, BRUSH_MAX_PRESS);
            }
        }
        else if (edit_source_->maskShader()->mode == MaskShader::SHAPE) {
            if (mask_cursor_shape_ > 0) {
                float b = -0.02f * movement.y;
                edit_source_->maskShader()->blur = CLAMP(edit_source_->maskShader()->blur+b, SHAPE_MIN_BLUR, SHAPE_MAX_BLUR);
                edit_source_->touch();
            }
        }
    }
}
