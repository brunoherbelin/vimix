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

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#define GLM_ENABLE_EXPERIMENTAL
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
#include "MousePointer.h"

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
    scene_brush_pos = glm::vec3(0.f);

    // replace grid with appropriate one
    translation_grid_ = new TranslationGrid(scene.root());
    translation_grid_->root()->visible_ = false;
    rotation_grid_ = new RotationGrid(scene.root());
    rotation_grid_->root()->visible_ = false;
    if (grid) delete grid;
    grid = translation_grid_;
}

void TextureView::update(float dt)
{
    View::update(dt);

    // a more complete update is requested (e.g. after switching to view)
    if  (View::need_deep_update_ > 0 ||
        (Mixer::manager().currentSource() != nullptr && edit_source_ != Mixer::manager().currentSource() )) {
        // request update
        need_edit_update_ = true;
        // change grid color
        const ImVec4 c = ImGuiToolkit::HighlightColor();
        translation_grid_->setColor( glm::vec4(c.x, c.y, c.z, 0.3) );
        rotation_grid_->setColor( glm::vec4(c.x, c.y, c.z, 0.3) );
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

    if (edit_source_ != nullptr && edit_source_->ready())
    {
        // coordinates of mouse in scene reference frame
        glm::vec3 scene_pos = Rendering::manager().unProject(pos, scene.root()->transform_);

        // if the mouse button is down, use the grabbing coordinates instead
        if (current_action_ongoing_)
            scene_pos = scene_brush_pos;

        // put the cursor at the coordinates of the mouse / cursor
        mask_cursor_circle_->translation_ = glm::vec3(glm::vec2(scene_pos), 0.f);
        mask_cursor_square_->translation_ = glm::vec3(glm::vec2(scene_pos), 0.f);
        mask_cursor_crop_->translation_ = glm::vec3(glm::vec2(scene_pos), 0.f);

        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantCaptureMouse || show_cursor_forced_)
        {
            // show paint brush cursor
            if (edit_source_->maskShader()->mode == MaskShader::PAINT) {
                if (mask_cursor_paint_ > 0) {
                    glm::vec2 S(preview_surface_->scale_);
                    S += glm::vec2(Settings::application.brush.x);
                    if ( ABS(scene_pos.x) < S.x  && ABS(scene_pos.y) < S.y ) {
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

void TextureView::adaptGridToSource(Source *s, Node *picked)
{
    // remember active state
    bool _active = grid->active();

    // Reset
    grid = translation_grid_;
    rotation_grid_->setActive( false );
    translation_grid_->setActive( false );
    rotation_grid_->root()->translation_ = glm::vec3(0.f);
    rotation_grid_->root()->scale_ = glm::vec3(1.f);
    translation_grid_->root()->translation_ = glm::vec3(0.f);
    translation_grid_->root()->rotation_ = glm::vec3(0.f);

    if (s != nullptr) {

        if ( picked == s->handles_[mode_][Handles::ROTATE] ) {
            // shift grid at center of source
            rotation_grid_->root()->translation_ = s->group(mode_)->translation_;
            rotation_grid_->root()->scale_.x = glm::length(
                        glm::vec2(s->frame()->aspectRatio() * s->group(mode_)->scale_.x,
                                  s->group(mode_)->scale_.y) );
            rotation_grid_->root()->scale_.y = rotation_grid_->root()->scale_.x;
            // Swap grid to rotation grid
            grid = rotation_grid_;
        }
        else if ( picked == s->handles_[mode_][Handles::RESIZE] ||
                  picked == s->handles_[mode_][Handles::RESIZE_V] ||
                  picked == s->handles_[mode_][Handles::RESIZE_H] ){
            translation_grid_->root()->translation_ = glm::vec3(0.f);
            translation_grid_->root()->rotation_.z = s->group(mode_)->rotation_.z;
        }
        else if ( picked == s->handles_[mode_][Handles::SCALE] ){
            translation_grid_->root()->translation_ = s->group(mode_)->translation_;
            translation_grid_->root()->rotation_.z = s->group(mode_)->rotation_.z;
        }

        // set grid aspect ratio
        if (Settings::application.proportional_grid)
            translation_grid_->setAspectRatio( s->frame()->aspectRatio() );
        else
            translation_grid_->setAspectRatio( 1.f );
    }

    // restore active state
    grid->setActive( _active );

    // only the active source is visible
    rotation_grid_->root()->visible_ = rotation_grid_->active();
    translation_grid_->root()->visible_ = translation_grid_->active();
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
                // adapt grid to prepare grab action
                adaptGridToSource(current);
                return pick;
            }
            // special case for cropping the mask shape
            else if ( current->maskShader()->mode == MaskShader::SHAPE && mask_cursor_shape_ > 0) {
                pick = { mask_cursor_crop_, P };
                // adapt grid to prepare grab action
                adaptGridToSource(current);
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
                    // adapt grid to prepare grab action
                    adaptGridToSource(current, pick.first);
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

bool TextureView::adjustBackground()
{
    bool ret = false;

    // by default consider edit source is null
    mask_node_->visible_ = false;
    float image_original_width = 1.f;
    scale_crop_ = glm::vec3(1.f);
    shift_crop_ = glm::vec3(0.f);
    preview_surface_->setTextureIndex( Resource::getTextureTransparent() );

    // if its a valid index
    if (edit_source_ != nullptr) {
        if (edit_source_->ready()) {
            // update rendering frame to match edit source AR
            image_original_width = edit_source_->frame()->aspectRatio();
            scale_crop_.x = (edit_source_->group(View::GEOMETRY)->crop_[1]
                             - edit_source_->group(View::GEOMETRY)->crop_[0])
                            * 0.5f;
            scale_crop_.y = (edit_source_->group(View::GEOMETRY)->crop_[2]
                             - edit_source_->group(View::GEOMETRY)->crop_[3])
                            * 0.5f;
            shift_crop_.x = edit_source_->group(View::GEOMETRY)->crop_[1] - scale_crop_.x;
            shift_crop_.y = edit_source_->group(View::GEOMETRY)->crop_[3] + scale_crop_.y;
            scale_crop_.x *= edit_source_->frame()->aspectRatio();
            shift_crop_.x *= edit_source_->frame()->aspectRatio();

            preview_surface_->setTextureIndex(edit_source_->frame()->texture());
            preview_shader_->secondary_texture = edit_source_->blendingShader()->secondary_texture;
            preview_surface_->scale_ = scale_crop_;
            preview_surface_->translation_ = shift_crop_;

            // mask appearance
            mask_node_->visible_ = edit_source_->maskShader()->mode == MaskShader::SHAPE
                                   && mask_cursor_shape_ > 0;

            int shape = edit_source_->maskShader()->shape;
            mask_circle_->visible_ = shape == MaskShader::ELLIPSE;
            mask_square_->visible_ = shape == MaskShader::OBLONG || shape == MaskShader::RECTANGLE;
            mask_horizontal_->visible_ = shape == MaskShader::HORIZONTAL;
            mask_vertical_->visible_ = shape == MaskShader::VERTICAL;

            // symetrical shapes
            if (shape < MaskShader::HORIZONTAL) {
                mask_node_->scale_ = scale_crop_ * glm::vec3(edit_source_->maskShader()->size, 1.f);
                mask_node_->translation_ = glm::vec3(0.f);
            }
            // vertical
            else if (shape > MaskShader::HORIZONTAL) {
                mask_node_->scale_ = glm::vec3(1.f, scale_crop_.y, 1.f);
                mask_node_->translation_ = glm::vec3(edit_source_->maskShader()->size.x
                                                         * scale_crop_.x,
                                                     0.f,
                                                     0.f);
            }
            // horizontal
            else {
                mask_node_->scale_ = glm::vec3(scale_crop_.x, 1.f, 1.f);
                mask_node_->translation_ = glm::vec3(0.f,
                                                     edit_source_->maskShader()->size.y
                                                         * scale_crop_.y,
                                                     0.f);
            }

            mask_node_->translation_ += shift_crop_;
        }
        else
            ret = true;
    }

    // background scene
    background_surface_->scale_.x = image_original_width;
    background_surface_->scale_.y = 1.f;
    background_frame_->scale_.x = image_original_width;
    vertical_mark_->translation_.x = -image_original_width;
    preview_frame_->scale_ = scale_crop_;
    preview_frame_->translation_ = shift_crop_;
    preview_checker_->scale_ = scale_crop_;
    preview_checker_->translation_ = shift_crop_;
    glm::mat4 Ar  = glm::scale(glm::identity<glm::mat4>(), scale_crop_ );
    static glm::mat4 Tra = glm::scale(glm::translate(glm::identity<glm::mat4>(), glm::vec3( -32.f, -32.f, 0.f)), glm::vec3( 64.f, 64.f, 1.f));
    preview_checker_->shader()->iTransform = Ar * Tra;

    return ret;
}

Source *TextureView::getEditOrCurrentSource()
{
    // get current source
    Source *_source = Mixer::manager().currentSource();

    // no current source but possible source
    if (_source == nullptr && Mixer::manager().numSource() > 0) {
        // then get the source active in user interface left panel
        _source = UserInterface::manager().sourceInPanel();
        // no source in panel neither?
        if (_source == nullptr && edit_source_ != nullptr) {
            // then keep the current edit source ( if still available )
            _source = Mixer::manager().findSource( edit_source_->id() );
        }
        // ensures the source is selected
        Mixer::selection().set(_source);
    }

    // do not work on a failed source
    if (_source != nullptr && _source->failed() ) {
        _source = nullptr;
    }

    return _source;
}

void TextureView::draw()
{
    // edit view needs to be updated (source changed)
    if (need_edit_update_) {
        // now, follow the change of current source
        // & remember source to edit
        edit_source_ = getEditOrCurrentSource();

        // update background and frame to match editsource
        // & return true if still needs to edit update
        need_edit_update_ = adjustBackground();
    }

    // Display grid
    grid->root()->visible_ = (grid->active() && current_action_ongoing_);

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
        glm::vec2 P(-background_frame_->scale_.x, background_frame_->scale_.y + 0.01f);
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
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.22f, 0.22f, 0.22f, 0.99f));
            ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.85f, 0.85f, 0.85f, 0.86f));
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.95f, 0.95f, 0.95f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.22f, 0.22f, 0.99f));

            int maskmode = edit_source_->maskShader()->mode;
            ImGui::SetNextItemWidth( ImGui::GetTextLineHeightWithSpacing() * 2.6);

            if (ImGui::BeginCombo("##Mask", MaskShader::mask_icons[maskmode])) {

                for (int m = MaskShader::NONE; m <= MaskShader::SOURCE; ++m){
                    if (ImGui::Selectable( MaskShader::mask_icons[m] )) {
                        // on change of mode
                        if (maskmode != m) {
                            // cancel previous source mask
                            if (maskmode == MaskShader::SOURCE) {
                                // store source image as mask before painting
                                if (edit_source_->maskSource()->connected() && m == MaskShader::PAINT)
                                    edit_source_->setMask( edit_source_->maskSource()->source()->frame()->image() );
                                // cancel source mask
                                edit_source_->maskSource()->disconnect();
                            }
                            // store current mask before painting
                            else if (m == MaskShader::PAINT)
                                edit_source_->storeMask();
                            // set new mode
                            maskmode = m;
                            edit_source_->maskShader()->mode = maskmode;
                            // force update source and view
                            edit_source_->touch(Source::SourceUpdate_Mask);
                            need_edit_update_ = true;
                            // store action history
                            std::ostringstream oss;
                            oss << edit_source_->name() << ": " << MaskShader::mask_names[maskmode];
                            Action::manager().store(oss.str());
                            // force take control of source for NONE and SOURCE modes
                            if (maskmode == MaskShader::NONE || maskmode == MaskShader::SOURCE)
                                Mixer::manager().setCurrentSource(edit_source_);
                        }
                    }
                    if (ImGui::IsItemHovered())
                        ImGuiToolkit::ToolTip(MaskShader::mask_names[m]);
                }
                ImGui::EndCombo();
            }

            // GUI for selecting source mask
            if (maskmode == MaskShader::SOURCE) {

                ImGui::SameLine(0, 60);
                bool on = true;
                ImGuiToolkit::ButtonToggle(ICON_FA_MOUSE_POINTER, &on, "Edit texture");

                // List of sources
                ImGui::SameLine(0, 60);
                std::string label = "Select source";
                Source *ref_source = nullptr;
                if (edit_source_->maskSource()->connected()) {
                    ref_source = edit_source_->maskSource()->source();
                    if (ref_source != nullptr)
                        label = std::string("Source ") + ref_source->initials() + " - " + ref_source->name();
                }
                if (ImGui::BeginCombo("##SourceMask", label.c_str())) {
                    SourceList::iterator iter;
                    for (iter = Mixer::manager().session()->begin(); iter != Mixer::manager().session()->end(); ++iter)
                    {
                        label = std::string("Source ") + (*iter)->initials() + " - " + (*iter)->name();
                        if (ImGui::Selectable( label.c_str(), *iter == ref_source )) {
                            edit_source_->maskSource()->connect( *iter );
                            edit_source_->touch(Source::SourceUpdate_Mask);
                            need_edit_update_ = true;
                        }
                    }
                    ImGui::EndCombo();
                }
                if (ImGui::IsItemHovered())
                    ImGuiToolkit::ToolTip("Source used as mask");
                // reset button
                if (ref_source){
                    ImGui::SameLine();
                    if (ImGui::Button(ICON_FA_BACKSPACE)){
                        edit_source_->maskSource()->disconnect();
                        edit_source_->touch(Source::SourceUpdate_Mask);
                        need_edit_update_ = true;
                    }
                    if (ImGui::IsItemHovered())
                        ImGuiToolkit::ToolTip("Reset");
                }

            }
            // GUI for drawing mask
            else if (maskmode == MaskShader::PAINT) {

                // select cursor
                static bool on = true;
                ImGui::SameLine(0, 60);
                on = mask_cursor_paint_ == 0;
                if (ImGuiToolkit::ButtonToggle(ICON_FA_MOUSE_POINTER, &on, "Edit texture")) {
                    Mixer::manager().setCurrentSource(edit_source_);
                    mask_cursor_paint_ = 0;
                }

                ImGui::SameLine();
                on = mask_cursor_paint_ == 1;
                if (ImGuiToolkit::ButtonToggle(ICON_FA_PAINT_BRUSH, &on, "Brush")) {
                    Mixer::manager().unsetCurrentSource();
                    mask_cursor_paint_ = 1;
                }

                ImGui::SameLine();
                on = mask_cursor_paint_ == 2;
                if (ImGuiToolkit::ButtonToggle(ICON_FA_ERASER, &on, "Eraser")) {
                    Mixer::manager().unsetCurrentSource();
                    mask_cursor_paint_ = 2;
                }

                if (mask_cursor_paint_ > 0) {

                    ImGui::SameLine(0, 50);
                    if (ImGui::Button(ICON_FA_PEN ICON_FA_SORT_DOWN ))
                        ImGui::OpenPopup("brush_shape_popup");
                    if (ImGui::IsItemHovered())
                        ImGuiToolkit::ToolTip("Shape");
                    if (ImGui::BeginPopup( "brush_shape_popup" ))
                    {
                        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_DEFAULT);
                        if (ImGui::Selectable( ICON_FA_CIRCLE "  Circle"))
                            Settings::application.brush.z = 0.f;
                        if (ImGui::Selectable( ICON_FA_SQUARE "   Square"))
                            Settings::application.brush.z = 1.f;
                        ImGui::PopFont();
                        ImGui::EndPopup();
                    }

                    ImGui::SameLine();
                    show_cursor_forced_ = false;
                    if (ImGui::Button(ICON_FA_DOT_CIRCLE ICON_FA_SORT_DOWN ))
                        ImGui::OpenPopup("brush_size_popup");
                    if (ImGui::IsItemHovered())
                        ImGuiToolkit::ToolTip("Size");
                    if (ImGui::BeginPopup("brush_size_popup", ImGuiWindowFlags_NoMove))
                    {
                        int pixel_size_min = int(0.05 * edit_source_->frame()->height() );
                        int pixel_size_max = int(2.0 * edit_source_->frame()->height() );
                        int pixel_size = int(Settings::application.brush.x * edit_source_->frame()->height() );
                        show_cursor_forced_ = true;
                        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_DEFAULT);
                        ImGuiToolkit::Indication("Large  ", 16, 1);
                        if (ImGui::VSliderInt("##BrushSize", ImVec2(30,260), &pixel_size, pixel_size_min, pixel_size_max, "") ){
                            Settings::application.brush.x = CLAMP(float(pixel_size) / edit_source_->frame()->height(), BRUSH_MIN_SIZE, BRUSH_MAX_SIZE);
                        }
                        if (ImGui::IsItemHovered() || ImGui::IsItemActive() )  {
                            ImGui::BeginTooltip();
                            ImGui::Text("%d px", pixel_size);
                            ImGui::EndTooltip();
                        }
                        ImGuiToolkit::Indication("Small  ", 15, 1);
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
                    if (ImGui::IsItemHovered())
                        ImGuiToolkit::ToolTip("Pressure");
                    if (ImGui::BeginPopup("brush_pressure_popup", ImGuiWindowFlags_NoMove))
                    {
                        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_DEFAULT);
                        ImGuiToolkit::Indication("Light  ", ICON_FA_FEATHER_ALT);
                        ImGui::VSliderFloat("##BrushPressure", ImVec2(30,260), &Settings::application.brush.y, BRUSH_MAX_PRESS, BRUSH_MIN_PRESS, "", 0.3f);
                        if (ImGui::IsItemHovered() || ImGui::IsItemActive() )  {
                            ImGui::BeginTooltip();
                            ImGui::Text("%.1f%%", Settings::application.brush.y * 100.0);
                            ImGui::EndTooltip();
                        }
                        ImGuiToolkit::Indication("Heavy  ", ICON_FA_WEIGHT_HANGING);
                        ImGui::PopFont();
                        ImGui::EndPopup();
                    }

                    // store mask if changed, reset after applied
                    if (edit_source_->maskShader()->effect > 0)
                        edit_source_->storeMask();
                    edit_source_->maskShader()->effect = 0;

                    // menu for effects
                    ImGui::SameLine(0, 60);
                    if (ImGui::Button(ICON_FA_PAINT_ROLLER ICON_FA_SORT_DOWN ))
                        ImGui::OpenPopup( "brush_menu_popup" );
                    if (ImGui::IsItemHovered())
                        ImGuiToolkit::ToolTip("Operations");
                    if (ImGui::BeginPopup( "brush_menu_popup" ))
                    {
                        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_DEFAULT);
                        std::ostringstream oss;
                        oss << edit_source_->name();
                        int e = 0;
                        if (ImGui::Selectable( ICON_FA_BACKSPACE "  Clear")) {
                            e = 1;
                            oss << ": Clear " << MASK_PAINT_ACTION_LABEL;
                        }
                        if (ImGui::Selectable( ICON_FA_THEATER_MASKS "   Invert")) {
                            e = 2;
                            oss << ": Invert " << MASK_PAINT_ACTION_LABEL;
                        }
                        if (ImGui::Selectable( ICON_FA_WAVE_SQUARE "  Edge")) {
                            e = 3;
                            oss << ": Edge " << MASK_PAINT_ACTION_LABEL;
                        }
                        if (e>0) {
                            edit_source_->maskShader()->effect = e;
                            edit_source_->maskShader()->cursor = glm::vec4(100.0, 100.0, 0.f, 0.f);
                            edit_source_->touch(Source::SourceUpdate_Mask);
                            Action::manager().store(oss.str());
                        }
                        ImGui::PopFont();
                        ImGui::EndPopup();
                    }

                    static DialogToolkit::OpenFileDialog maskdialog("Select Image",
                                                                    IMAGES_FILES_TYPE,
                                                                    IMAGES_FILES_PATTERN );

                    ImGui::SameLine();
                    if (ImGui::Button(ICON_FA_FOLDER_OPEN))
                        maskdialog.open();
                    if (ImGui::IsItemHovered())
                        ImGuiToolkit::ToolTip("Open image");
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
            // GUI for shape masks
            else if (maskmode == MaskShader::SHAPE) {

                // select cursor
                static bool on = true;
                ImGui::SameLine(0, 60);
                on = mask_cursor_shape_ == 0;
                if (ImGuiToolkit::ButtonToggle(ICON_FA_MOUSE_POINTER, &on, "Edit texture")) {
                    Mixer::manager().setCurrentSource(edit_source_);
                    need_edit_update_ = true;
                    mask_cursor_shape_ = 0;
                }

                ImGui::SameLine();
                on = mask_cursor_shape_ == 1;
                if (ImGuiToolkit::ButtonToggle(ICON_FA_CROP_ALT, &on, "Edit shape")) {
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
                        edit_source_->touch(Source::SourceUpdate_Mask);
                        need_edit_update_ = true;
                        // store action history
                        std::ostringstream oss;
                        oss << edit_source_->name() << ": Mask Shape " << MaskShader::mask_shapes[shape];
                        Action::manager().store(oss.str());
                    }
                    if (ImGui::IsItemHovered())
                        ImGuiToolkit::ToolTip("Select shape");

                    ImGui::SameLine(0, 20);

                    char buf[128];
                    snprintf(buf, 128, "%d%%" ICON_FA_SORT_DOWN, blur_percent);
                    if (ImGui::Button(buf))
                        ImGui::OpenPopup("shape_smooth_popup");
                    if (ImGui::IsItemHovered())
                        ImGuiToolkit::ToolTip("Blur");
                    if (ImGui::BeginPopup("shape_smooth_popup", ImGuiWindowFlags_NoMove))
                    {
                        static bool smoothchanged = false;
                        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_DEFAULT);
                        ImGuiToolkit::Indication("Blurry ", 7, 16);
                        if (ImGui::VSliderInt("##shapeblur", ImVec2(30,260), &blur_percent, 0, 100, "") ){
                            edit_source_->maskShader()->blur = float(blur_percent) / 100.f;
                            edit_source_->touch(Source::SourceUpdate_Mask);
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
                            ImGui::Text("%.d%% blur", blur_percent);
                            ImGui::EndTooltip();
                        }
                        ImGuiToolkit::Indication("Sharp ", 8, 16);
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
            else {// maskmode == MaskShader::NONE
                // always active mouse pointer
                ImGui::SameLine(0, 60);
                bool on = true;
                ImGuiToolkit::ButtonToggle(ICON_FA_MOUSE_POINTER, &on, "Edit texture");
                ImGui::SameLine(0, 60);
                ImGui::TextDisabled( "No mask" );
            }

            ImGui::PopStyleColor(8);  // colors
            ImGui::End();
        }
        ImGui::PopFont();

    }

    // display popup menu
    if (show_context_menu_ == MENU_SOURCE) {
        ImGui::OpenPopup( "AppearanceSourceContextMenu" );
        show_context_menu_ = MENU_NONE;
    }
    if (ImGui::BeginPopup("AppearanceSourceContextMenu", ImGuiWindowFlags_NoMove)) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(COLOR_APPEARANCE_SOURCE, 1.f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(COLOR_MENU_HOVERED, 0.5f));
        Source *s = Mixer::manager().currentSource();
        if (s != nullptr) {

            if (s->textureMirrored()) {
                if (ImGui::Selectable( ICON_FA_TH_LARGE "  Repeat " )){
                    s->setTextureMirrored(false);
                    Action::manager().store(s->name() + std::string(": Texture Repeat"));
                }
            } else {
                if (ImGui::Selectable( ICON_FA_TH_LARGE "  Mirror " )){
                    s->setTextureMirrored(true);
                    Action::manager().store(s->name() + std::string(": Texture Mirror"));
                }
            }
            ImGui::Separator();

            if (ImGui::Selectable( ICON_FA_VECTOR_SQUARE "  Reset" )){
                s->group(mode_)->scale_ = glm::vec3(1.f);
                s->group(mode_)->rotation_.z = 0;
                s->group(mode_)->translation_ = glm::vec3(0.f);
                s->touch();
                Action::manager().store(s->name() + std::string(": Texture Reset"));
            }
            if (ImGui::Selectable( ICON_FA_CROSSHAIRS "  Reset position" )){
                s->group(mode_)->translation_ = glm::vec3(0.f);
                s->touch();
                Action::manager().store(s->name() + std::string(": Texture Reset position"));
            }
            if (ImGui::Selectable( ICON_FA_CIRCLE_NOTCH "  Reset rotation" )){
                s->group(mode_)->rotation_.z = 0;
                s->touch();
                Action::manager().store(s->name() + std::string(": Texture Reset rotation"));
            }
            if (ImGui::Selectable( ICON_FA_EXPAND_ALT "  Reset aspect ratio" )){
                s->group(mode_)->scale_.x = s->group(mode_)->scale_.y;
                s->group(mode_)->scale_.x *= (s->group(mode_)->crop_[1] - s->group(mode_)->crop_[0]) /
                                             (s->group(mode_)->crop_[2] - s->group(mode_)->crop_[3]);
                s->touch();
                Action::manager().store(s->name() + std::string(": Texture Reset aspect ratio"));
            }
        }
        ImGui::PopStyleColor(2);
        ImGui::EndPopup();
    }

}



View::Cursor TextureView::grab (Source *s, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2> pick)
{
    std::ostringstream info;
    View::Cursor ret = Cursor();

    // grab coordinates in scene-View reference frame
    glm::vec3 scene_from = Rendering::manager().unProject(from, scene.root()->transform_);
    glm::vec3 scene_to   = Rendering::manager().unProject(to, scene.root()->transform_);

    // Not grabbing a source
    if (!s) {
        // work on the edited source
        if ( edit_source_ != nullptr ) {
            info << edit_source_->name() << ": ";

            // set brush coordinates (used in mouse over)
            scene_brush_pos = scene_to;

            if ( pick.first == mask_cursor_circle_ ) {
                // snap prush coordinates if grid is active
                if (grid->active())
                    scene_brush_pos = grid->snap(scene_brush_pos);
                // inform shader of a cursor action : coordinates and crop scaling
                edit_source_->maskShader()->cursor = glm::vec4(scene_brush_pos.x - shift_crop_.x,
                                                               scene_brush_pos.y - shift_crop_.y,
                                                               edit_source_->mixingsurface_->scale_.x,
                                                               edit_source_->mixingsurface_->scale_.y);
                edit_source_->touch(Source::SourceUpdate_Mask);
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
                glm::vec3 scene_translation = scene_to - scene_from;
                val = glm::sign( hv ? glm::vec3(1.f) : scene_from) * glm::vec3(scene_translation / val);
                // relative change of stored mask size
                val += stored_mask_size_;
                // snap to grid following its aspect ratio
                if (grid->active()) {
                    val.x *= grid->aspectRatio();
                    val = grid->snap(val);
                    val.x *= 1.f / grid->aspectRatio();
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
                // update
                edit_source_->touch(Source::SourceUpdate_Mask);
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

    Group *sourceNode = s->group(mode_);

    // make sure matrix transform of stored status is updated
    s->stored_status_->update(0);

    // grab coordinates in source-root reference frame
    const glm::mat4 scene_to_source_transform = glm::inverse(s->stored_status_->transform_);
    const glm::mat4 source_to_scene_transform = s->stored_status_->transform_;

    // which manipulation to perform?
    if (pick.first)  {
        // which corner was picked ?
        glm::vec2 corner = glm::round(pick.second);

        // transform from source center to corner
        glm::mat4 source_to_corner_transform = GlmToolkit::transform(glm::vec3(corner, 0.f), glm::vec3(0.f, 0.f, 0.f),
                                            glm::vec3(1.f / s->frame()->aspectRatio(), 1.f, 1.f));

        // transformation from scene to corner:
        glm::mat4 scene_to_corner_transform = source_to_corner_transform * scene_to_source_transform;
        glm::mat4 corner_to_scene_transform = glm::inverse(scene_to_corner_transform);

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
            glm::vec2 corner_scaling = glm::vec2(handle) / glm::vec2(corner * 2.f);

            // proportional SCALING with SHIFT
            if (UserInterface::manager().shiftModifier())
                corner_scaling = glm::vec2(glm::compMax(corner_scaling));

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

            // Transform scene to reference frame at the center, but scaled to match aspect ratio
            const glm::mat4 center_scale = glm::scale(glm::identity<glm::mat4>(),
                                                      glm::vec3(1.f / s->frame()->aspectRatio(), 1.f, 1.f));

            glm::mat4 scene_to_center_transform = center_scale * scene_to_source_transform;
            glm::mat4 center_to_scene_transform = glm::inverse(scene_to_center_transform);

            //
            // Manipulate the scaling handle in the SCENE coordinates to apply grid snap
            //
            glm::vec3 handle = glm::vec3( glm::round(pick.second), 0.f);
            // Compute handle coordinates into SCENE reference frame
            handle = center_to_scene_transform * glm::vec4( handle, 1.f );
            // move the handle we hold by the mouse translation (in scene reference frame)
            handle = glm::translate(glm::identity<glm::mat4>(), scene_to - scene_from) * glm::vec4( handle, 1.f );
            // snap handle coordinates to grid (if active)
            if ( grid->active() )
                handle = grid->snap(handle);
            // Compute handle coordinates back in SOURCE CENTER reference frame
            handle = scene_to_center_transform * glm::vec4( handle,  1.f );
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
            overlay_rotation_fix_->visible_ = false;
            overlay_rotation_fix_->copyTransform(overlay_rotation_);
            overlay_rotation_clock_->visible_ = false;
            overlay_rotation_clock_hand_->visible_ = true;
            overlay_rotation_clock_hand_->translation_.x = s->stored_status_->translation_.x;
            overlay_rotation_clock_hand_->translation_.y = s->stored_status_->translation_.y;

            // prepare variables
            const float diagonal = glm::length( glm::vec2(s->frame()->aspectRatio() * s->stored_status_->scale_.x, s->stored_status_->scale_.y));
            glm::vec2 handle_polar = glm::vec2( diagonal, 0.f);

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
            source_target.z = 0.f;

            // apply translation of target in SCENE
            source_target = glm::translate(glm::identity<glm::mat4>(), scene_to - scene_from) * glm::vec4(source_target, 1.f);

            // snap coordinates to grid (if active)
            if ( grid->active() )
                source_target = grid->snap(source_target);

            // Apply translation to the source
            sourceNode->translation_ = source_target - offset;

            // Show center overlay for POSITION
            overlay_position_->visible_ = true;
            overlay_position_->translation_.x = sourceNode->translation_.x;
            overlay_position_->translation_.y = sourceNode->translation_.y;
            overlay_position_->update(0);

            // Show move cursor
            ret.type = Cursor_ResizeAll;
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

    // reset grid
    adaptGridToSource();
}

#define MAX_DURATION 1000.f
#define MIN_SPEED_A 0.005f
#define MAX_SPEED_A 0.5f

void TextureView::arrow (glm::vec2 movement)
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

            // initiate view action and store status of source
            initiate();

            // get coordinates of source and set this as start of mouse position
            _from = glm::vec2( Rendering::manager().project(current->group(mode_)->translation_, scene.root()->transform_) );
            _displacement = glm::vec2(0.f);

            // Initiate mouse pointer action
            MousePointer::manager().active()->initiate(_from);
        }

    }
    else {

        terminate(true);

        _from = glm::vec2(0.f);
        _displacement = glm::vec2(0.f);

    }

//    Source *s = Mixer::manager().currentSource();
//    if (s) {
//        static float accumulator = 0.f;
//        accumulator += dt_;

//        glm::vec3 gl_Position_from = Rendering::manager().unProject(glm::vec2(0.f), scene.root()->transform_);
//        glm::vec3 gl_Position_to   = Rendering::manager().unProject(movement, scene.root()->transform_);
//        glm::vec3 gl_delta = gl_Position_to - gl_Position_from;

//        Group *sourceNode = s->group(mode_);
//        glm::vec3 alt_move_ = sourceNode->translation_;
//        if (UserInterface::manager().altModifier()) {
//            if (accumulator > 100.f)
//            {
//                // precise movement with SHIFT
//                if ( UserInterface::manager().shiftModifier() ) {
//                    alt_move_ += glm::sign(gl_delta) * 0.0011f;
//                    sourceNode->translation_.x = ROUND(alt_move_.x, 1000.f);
//                    sourceNode->translation_.y = ROUND(alt_move_.y, 1000.f);
//                }
//                else {
//                    alt_move_ += glm::sign(gl_delta) * 0.11f;
//                    sourceNode->translation_.x = ROUND(alt_move_.x, 10.f);
//                    sourceNode->translation_.y = ROUND(alt_move_.y, 10.f);
//                }
//                accumulator = 0.f;
//            }
//        }
//        else {
//            sourceNode->translation_ += gl_delta * ARROWS_MOVEMENT_FACTOR * dt_;
//            accumulator = 0.f;
//            alt_move_ = sourceNode->translation_;
//        }

//        // store action in history
//        std::ostringstream info;
//        info << "Texture Shift " << std::fixed << std::setprecision(3) << sourceNode->translation_.x;
//        info << ", "  << sourceNode->translation_.y ;
//        current_action_ = s->name() + ": " + info.str();

//        // request update
//        s->touch();
//    }
//    else if (edit_source_) {
//        if (edit_source_->maskShader()->mode == MaskShader::PAINT) {
//            if (mask_cursor_paint_ > 0) {
//                glm::vec2 b = 0.02f * movement;
//                Settings::application.brush.x = CLAMP(Settings::application.brush.x+b.x, BRUSH_MIN_SIZE, BRUSH_MAX_SIZE);
//                Settings::application.brush.y = CLAMP(Settings::application.brush.y+b.y, BRUSH_MIN_PRESS, BRUSH_MAX_PRESS);
//            }
//        }
//        else if (edit_source_->maskShader()->mode == MaskShader::SHAPE) {
//            if (mask_cursor_shape_ > 0) {
//                float b = -0.02f * movement.y;
//                edit_source_->maskShader()->blur = CLAMP(edit_source_->maskShader()->blur+b, SHAPE_MIN_BLUR, SHAPE_MAX_BLUR);
//                edit_source_->touch(Source::SourceUpdate_Mask);
//            }
//        }
//    }
}
