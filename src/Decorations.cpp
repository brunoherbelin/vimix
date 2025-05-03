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

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <glm/gtx/component_wise.hpp>

#include "Visitor.h"
#include "GlmToolkit.h"
#include "Decorations.h"

#include "imgui.h"
#include <glad/glad.h>

Frame::Frame(CornerType corner, BorderType border, ShadowType shadow) : Node(),
    right_(nullptr), left_(nullptr), top_(nullptr), shadow_(nullptr), square_(nullptr)
{
    static Mesh *shadows[3] = {nullptr};
    if (shadows[0] == nullptr)  {
        shadows[0] = new Mesh("mesh/glow.ply", "images/glow.dds");
        shadows[1] = new Mesh("mesh/shadow.ply", "images/shadow.dds");
        shadows[2] = new Mesh("mesh/shadow_perspective.ply", "images/shadow_perspective.dds");
    }
    static Mesh *frames[9] = {nullptr};
    if (frames[0] == nullptr)  {
        frames[0] = new Mesh("mesh/border_round.ply");
        frames[1] = new Mesh("mesh/border_round_left.ply");
        frames[2] = new Mesh("mesh/border_top.ply");
        frames[3] = new Mesh("mesh/border_large_round.ply");
        frames[4] = new Mesh("mesh/border_large_round_left.ply");
        frames[5] = new Mesh("mesh/border_large_top.ply");
        frames[6] = new Mesh("mesh/border_perspective_round.ply");
        frames[7] = new Mesh("mesh/border_perspective_round_left.ply");
        frames[8] = new Mesh("mesh/border_perspective_top.ply");
    }
    static LineSquare *sharpframethin = new LineSquare( 4.f );
    static LineSquare *sharpframelarge = new LineSquare( 6.f );

    // Round corners
    if (corner == ROUND){
        if (border == THIN) {
            right_ = frames[0];
            left_  = frames[1];
            top_   = frames[2];
        }
        else{
            right_ = frames[3];
            left_  = frames[4];
            top_   = frames[5];
        }
    }
    // Group corners
    else if (corner == GROUP){
        if (border == THIN) {
            right_ = frames[6];
            left_  = frames[7];
            top_   = frames[8];
        }
        else{
            right_ = frames[6];
            left_  = frames[7];
            top_   = frames[8];
        }
    }
    // Sharp corner
    else {
        if (border == LARGE)
            square_ = sharpframelarge;
        else
            square_ = sharpframethin;
    }

    switch (shadow) {
    default:
    case NONE:
        break;
    case GLOW:
        shadow_ = shadows[0];
        break;
    case DROP:
        shadow_ = shadows[1];
        break;
    case PERSPECTIVE:
        shadow_ = shadows[2];
        break;
    }

    color = glm::vec4( 1.f, 1.f, 1.f, 1.f);
}

Frame::~Frame()
{

}

void Frame::update( float dt )
{
    Node::update(dt);
    if(top_)
        top_->update(dt);
    if(right_)
        right_->update(dt);
    if(left_)
        left_->update(dt);
    if(shadow_)
        shadow_->update(dt);
    if(square_)
        square_->update(dt);
}

void Frame::draw(glm::mat4 modelview, glm::mat4 projection)
{
    if ( !initialized() ) {
        if(right_ && !right_->initialized())
            right_->init();
        if(left_ && !left_->initialized())
            left_->init();
        if(top_ && !top_->initialized())
            top_->init();
        if(shadow_ && !shadow_->initialized())
            shadow_->init();
        if(square_ && !square_->initialized())
            square_->init();
        init();
    }

    if ( visible_ ) {

        glm::mat4 ctm = modelview * transform_;

        // sharp border (scaled)
        if(square_) {
            square_->setColor(color);
            square_->draw( ctm, projection);
        }

        // shadow (scaled)
        if(shadow_){
            shadow_->shader()->color.a = 0.98f;
            shadow_->draw( ctm, projection);
        }

        // round top (scaled)
        if(top_) {
            top_->shader()->color = color;
            top_->draw( ctm, projection);
        }

        // round sides
        if(right_) {

            right_->shader()->color = color;

            // get scale
            glm::vec4 scale = ctm * glm::vec4(1.f, 1.0f, 0.f, 0.f);

            // get rotation
            glm::vec3 rot(0.f);
            glm::vec4 vec = ctm * glm::vec4(1.f, 0.f, 0.f, 0.f);
            rot.z = glm::orientedAngle( glm::vec3(1.f, 0.f, 0.f), glm::normalize(glm::vec3(vec)), glm::vec3(0.f, 0.f, 1.f) );

            // right side
            vec = ctm * glm::vec4(1.f, 0.f, 0.f, 1.f);
            right_->draw( GlmToolkit::transform(vec, rot, glm::vec3(scale.y, scale.y, 1.f)), projection );

        }
        if(left_) {

            left_->shader()->color = color;

            // get scale
            glm::vec4 scale = ctm * glm::vec4(1.f, 1.0f, 0.f, 0.f);

            // get rotation
            glm::vec3 rot(0.f);
            glm::vec4 vec = ctm * glm::vec4(1.f, 0.f, 0.f, 0.f);
            rot.z = glm::orientedAngle( glm::vec3(1.f, 0.f, 0.f), glm::normalize(glm::vec3(vec)), glm::vec3(0.f, 0.f, 1.f) );

            // right side
            vec = ctm * glm::vec4(-1.f, 0.f, 0.f, 1.f);
            left_->draw( GlmToolkit::transform(vec, rot, glm::vec3(scale.y, scale.y, 1.f)), projection );
        }
    }
}


void Frame::accept(Visitor& v)
{
    Node::accept(v);
    v.visit(*this);
}

Handles::Handles(Type type) : Node(), type_(type)
{
    static Mesh *handle_rotation = new Mesh("mesh/border_handles_rotation.ply");
    static Mesh *handle_corner   = new Mesh("mesh/border_handles_overlay.ply");
    static Mesh *handle_scale    = new Mesh("mesh/border_handles_scale.ply");
    static Mesh *handle_crop     = new Mesh("mesh/border_handles_crop.ply");
    static Mesh *handle_shape    = new Mesh("mesh/border_handles_shape.ply");
    static Mesh *handle_menu     = new Mesh("mesh/border_handles_menu.ply");
    static Mesh *handle_lock     = new Mesh("mesh/border_handles_lock.ply");
    static Mesh *handle_unlock   = new Mesh("mesh/border_handles_lock_open.ply");
    static Mesh *handle_eyeslash = new Mesh("mesh/icon_eye_slash.ply");
    static Mesh *handle_shadow   = new Mesh("mesh/border_handles_shadow.ply", "images/soft_shadow.dds");
    static Mesh *handle_node     = new Mesh("mesh/border_handles_sharp.ply");
    static Mesh *handle_crop_h   = new Mesh("mesh/border_handles_arrows.ply");
    static Mesh *handle_rounding = new Mesh("mesh/border_handles_roundcorner.ply");

    color   = glm::vec4( 1.f, 1.f, 1.f, 1.f);
    corner_ = glm::vec2(0.f, 0.f);
    shadow_ = handle_shadow;

    if ( type_ == Handles::ROTATE ) {
        handle_ = handle_rotation;
    }
    else if ( type_ == Handles::SCALE ) {
        handle_ = handle_scale;
    }
    else if ( type_ == Handles::MENU ) {
        handle_ = handle_menu;
    }
    else if ( type_ == Handles::EDIT_CROP ) {
        handle_ = handle_crop;
    }
    else if ( type_ == Handles::EDIT_SHAPE ) {
        handle_ = handle_shape;
    }
    else if ( type_ == Handles::LOCKED ) {
        handle_ = handle_lock;
    }
    else if ( type_ == Handles::UNLOCKED ) {
        handle_ = handle_unlock;
    }
    else if ( type_ == Handles::EYESLASHED ) {
        handle_ = handle_eyeslash;
    }
    else if ( type_ >= Handles::NODE_LOWER_LEFT  && type_ <= Handles::NODE_UPPER_RIGHT ) {
        handle_ = handle_node;
    }
    else if ( type_ == Handles::CROP_H || type_ == Handles::CROP_V ) {
        handle_ = handle_crop_h;
    }
    else if ( type_ == Handles::ROUNDING ) {
        handle_ = handle_rounding;
    }
    else {
        handle_ = handle_corner;
    }

}

Handles::~Handles()
{
}

void Handles::update( float dt )
{
    Node::update(dt);
    handle_->update(dt);
}

void Handles::draw(glm::mat4 modelview, glm::mat4 projection)
{

    if ( !initialized() ) {
        if(handle_ && !handle_->initialized())
            handle_->init();
        if(shadow_ && !shadow_->initialized())
            shadow_->init();
        init();
    }

    if ( visible_ && handle_) {
        static Mesh *handle_active = new Mesh("mesh/border_handles_overlay_filled.ply");

        // set color
        handle_->shader()->color = color;
        handle_active->shader()->color = color;

        glm::mat4 ctm;
        glm::vec4 vec;
        glm::vec3 tra, rot, sca;

        // get rotation and mirroring from the modelview
        GlmToolkit::inverse_transform(modelview, tra, rot, sca);
        glm::vec3 mirror = glm::sign(sca);

        if ( type_ == Handles::RESIZE ) {

            // 4 corners
            vec = modelview * glm::vec4(1.f, -1.f, 0.f, 1.f);
            ctm = GlmToolkit::transform(vec, rot, glm::vec3(1.f));
            handle_->draw( ctm, projection );

            vec = modelview * glm::vec4(1.f, 1.f, 0.f, 1.f);
            ctm = GlmToolkit::transform(vec, rot, glm::vec3(1.f));
            handle_->draw( ctm, projection );

            vec = modelview * glm::vec4(-1.f, -1.f, 0.f, 1.f);
            ctm = GlmToolkit::transform(vec, rot, glm::vec3(1.f));
            handle_->draw( ctm, projection );

            vec = modelview * glm::vec4(-1.f, 1.f, 0.f, 1.f);
            ctm = GlmToolkit::transform(vec, rot, glm::vec3(1.f));
            handle_->draw( ctm, projection );

            if ( glm::length(corner_) > 0.f ) {
                vec = modelview * glm::vec4(corner_.x, corner_.y, 0.f, 1.f);
                ctm = GlmToolkit::transform(vec, rot, glm::vec3(1.f));
                handle_active->draw( ctm, projection );
            }
        }
        else if ( type_ == Handles::RESIZE_H ){
            // left and right
            vec = modelview * glm::vec4(1.f, 0.f, 0.f, 1.f);
            ctm = GlmToolkit::transform(vec, rot, glm::vec3(1.f));
            handle_->draw( ctm, projection );

            vec = modelview * glm::vec4(-1.f, 0.f, 0.f, 1.f);
            ctm = GlmToolkit::transform(vec, rot, glm::vec3(1.f));
            handle_->draw( ctm, projection );

            if ( glm::length(corner_) > 0.f ) {
                vec = modelview * glm::vec4(corner_.x, corner_.y, 0.f, 1.f);
                ctm = GlmToolkit::transform(vec, rot, glm::vec3(1.f));
                handle_active->draw( ctm, projection );
            }
        }
        else if ( type_ == Handles::RESIZE_V ){
            // top and bottom
            vec = modelview * glm::vec4(0.f, 1.f, 0.f, 1.f);
            ctm = GlmToolkit::transform(vec, rot, glm::vec3(1.f));
            handle_->draw( ctm, projection );

            vec = modelview * glm::vec4(0.f, -1.f, 0.f, 1.f);
            ctm = GlmToolkit::transform(vec, rot, glm::vec3(1.f));
            handle_->draw( ctm, projection );

            if ( glm::length(corner_) > 0.f ) {
                vec = modelview * glm::vec4(corner_.x, corner_.y, 0.f, 1.f);
                ctm = GlmToolkit::transform(vec, rot, glm::vec3(1.f));
                handle_active->draw( ctm, projection );
            }
        }
        else if ( type_ == Handles::ROTATE ){
            // one icon in top right corner
            // 1. Fixed displacement by (0.12,0.12) along the rotation..
            ctm = GlmToolkit::transform(glm::vec4(0.f), rot, mirror);
            glm::vec4 pos = ctm * glm::vec4( 0.12f, 0.12f, 0.f, 1.f);
            // 2. ..from the top right corner (1,1)
            vec = ( modelview * glm::vec4(1.f, 1.f, 0.f, 1.f) ) + pos;
            ctm = GlmToolkit::transform(vec, rot, glm::vec3(1.f));
            // 3. draw
            shadow_->draw( ctm, projection );
            handle_->draw( ctm, projection );
        }
        else if ( type_ == Handles::SCALE ){
            // one icon in bottom right corner
            // 1. Fixed displacement by (0.12,0.12) along the rotation..
            ctm = GlmToolkit::transform(glm::vec4(0.f), rot, mirror);
            glm::vec4 pos = ctm * glm::vec4(mirror.x * 0.12f, mirror.x * -0.12f, 0.f, 1.f);
            // 2. ..from the bottom right corner (1,1)
            vec = ( modelview * glm::vec4(1.f, -1.f, 0.f, 1.f) ) + pos;
            ctm = GlmToolkit::transform(vec, rot, mirror);
            // 3. draw
            shadow_->draw( ctm, projection );
            handle_->draw( ctm, projection );
        }
        else if ( type_ == Handles::EDIT_CROP || type_ == Handles::EDIT_SHAPE ){
            // one icon in bottom left corner
            // 1. Fixed displacement by (0.12,0.12) along the rotation..
            ctm = GlmToolkit::transform(glm::vec4(0.f), rot, mirror);
            glm::vec4 pos = ctm * glm::vec4(mirror.x * -0.13f, mirror.x * -0.12f, 0.f, 1.f);
            // 2. ..from the bottom left corner (-1,-1)
            vec = ( modelview * glm::vec4(-1.f, 1.f, 0.f, 1.f) ) + pos;
            ctm = GlmToolkit::transform(vec, rot, mirror);
            // 3. draw
            shadow_->draw( ctm, projection );
            handle_->draw( ctm, projection );
        }
        else if ( type_ == Handles::MENU ){
            // one icon in top left corner
            // 1. Fixed displacement by (-0.12,0.12) along the rotation..
            ctm = GlmToolkit::transform(glm::vec4(0.f), rot, mirror);
            glm::vec4 pos = ctm * glm::vec4( -0.12f, 0.12f, 0.f, 1.f);
            // 2. ..from the top right corner (1,1)
            vec = ( modelview * glm::vec4(-1.f, 1.f, 0.f, 1.f) ) + pos;
            ctm = GlmToolkit::transform(vec, rot, mirror);
            // 3. draw
            shadow_->draw( ctm, projection );
            handle_->draw( ctm, projection );
        }
        else if ( type_ == Handles::LOCKED || type_ == Handles::UNLOCKED ){
            // one icon in bottom right corner
            // 1. Fixed displacement by (-0.12,0.12) along the rotation..
            ctm = GlmToolkit::transform(glm::vec4(0.f), rot, mirror);
            glm::vec4 pos = ctm * glm::vec4( -0.12f, 0.12f, 0.f, 1.f);
            // 2. ..from the bottom right corner (1,-1)
            vec = ( modelview * glm::vec4(1.f, -1.f, 0.f, 1.f) ) + pos;
            ctm = GlmToolkit::transform(vec, rot, mirror);
            // 3. draw
            shadow_->draw( ctm, projection );
            handle_->draw( ctm, projection );
        }
        else if ( type_ == Handles::EYESLASHED ){
            // one icon in bottom right corner
            // 1. Fixed displacement by (0.12,0.12) along the rotation..
            ctm = GlmToolkit::transform(glm::vec4(0.f), rot, mirror);
            glm::vec4 pos = ctm * glm::vec4(mirror.x * 0.15f, mirror.x * 0.15f, 0.f, 1.f);
            // 2. ..from the bottom left corner (-1,-1)
            vec = ( modelview * glm::vec4(-1.f, -1.f, 0.f, 1.f) ) + pos;
            ctm = GlmToolkit::transform(vec, rot, mirror);
            // 3. draw
            shadow_->draw( ctm, projection );
            handle_->draw( ctm, projection );
        }
        else if ( type_ == Handles::NODE_LOWER_LEFT ) {
            // 1. Corner
            vec = modelview * glm::vec4(translation_.x - 1.f, translation_.y - 1.f, 0.f, 1.f) ;
            ctm = GlmToolkit::transform(vec, rot, glm::vec3(1.f));
            // 2. draw
            handle_->draw( ctm, projection );
        }
        else if ( type_ == Handles::NODE_UPPER_LEFT ) {
            // 1. Corner
            vec = modelview * glm::vec4(translation_.x - 1.f, translation_.y + 1.f, 0.f, 1.f) ;
            ctm = GlmToolkit::transform(vec, rot, glm::vec3(1.f));
            // 2. draw
            handle_->draw( ctm, projection );
        }
        else if ( type_ == Handles::NODE_LOWER_RIGHT ) {
            // 1. Corner
            vec = modelview * glm::vec4(translation_.x + 1.f, translation_.y - 1.f, 0.f, 1.f) ;
            ctm = GlmToolkit::transform(vec, rot, glm::vec3(1.f));
            // 2. draw
            handle_->draw( ctm, projection );
        }
        else if ( type_ == Handles::NODE_UPPER_RIGHT ){
            // 1. Corner
            vec = modelview * glm::vec4(translation_.x + 1.f, translation_.y + 1.f, 0.f, 1.f) ;
            ctm = GlmToolkit::transform(vec, rot, glm::vec3(1.f));
            // 2. draw
            handle_->draw( ctm, projection );
        }
        else if ( type_ == Handles::CROP_H ){
            // left and right
            vec = modelview * glm::vec4(1.f, 0.f, 0.f, 1.f);
            ctm = GlmToolkit::transform(vec, rot, glm::vec3(1.f));
            handle_->draw( ctm, projection );

            vec = modelview * glm::vec4(-1.f, 0.f, 0.f, 1.f);
            ctm = GlmToolkit::transform(vec, rot, glm::vec3(1.f));
            handle_->draw( ctm, projection );

        }
        else if ( type_ == Handles::CROP_V ){
            // top and bottom
            vec = modelview * glm::vec4(0.f, 1.f, 0.f, 1.f);
            ctm = GlmToolkit::transform(vec, rot, glm::vec3(1.f));
            ctm = glm::rotate(ctm, (float) M_PI_2, glm::vec3(0.f, 0.f, 1.f));
            handle_->draw( ctm, projection );

            vec = modelview * glm::vec4(0.f, -1.f, 0.f, 1.f);
            ctm = GlmToolkit::transform(vec, rot, glm::vec3(1.f));
            ctm = glm::rotate(ctm, (float) M_PI_2, glm::vec3(0.f, 0.f, 1.f));
            handle_->draw( ctm, projection );
        }
        else if ( type_ == Handles::ROUNDING ){
            // one icon in top right corner
            // 1. Fixed displacement by (0.0,0.12) along the rotation..
            ctm = GlmToolkit::transform(glm::vec4(0.f), rot, mirror);
            glm::vec4 pos = ctm * glm::vec4( 0.0f, 0.13f, 0.f, 1.f);
            // 2. ..from the top right corner (1,1)
            vec = ( modelview * glm::vec4(translation_.x +1.f, 1.f, 0.f, 1.f) ) + pos;
            ctm = GlmToolkit::transform(vec, rot, glm::vec3(1.f));
            // 3. draw
            shadow_->draw( ctm, projection );
            handle_->draw( ctm, projection );
        }
    }
}


void Handles::accept(Visitor& v)
{
    Node::accept(v);
    v.visit(*this);
}


Symbol::Symbol(Type t, glm::vec3 pos) : Node(), type_(t)
{
    static Mesh *shadow= new Mesh("mesh/border_handles_shadow.ply", "images/soft_shadow.dds");
    static Mesh *shadows[(int)EMPTY+1] = {nullptr};
    static Mesh *icons[(int)EMPTY+1] = {nullptr};
    if (icons[0] == nullptr)  {
        icons[CIRCLE_POINT]   = new Mesh("mesh/point.ply");
        shadows[CIRCLE_POINT] = nullptr;
        icons[SQUARE_POINT]   = new Mesh("mesh/square_point.ply");
        shadows[SQUARE_POINT] = nullptr;
        icons[IMAGE]    = new Mesh("mesh/icon_image.ply");
        shadows[IMAGE]  = shadow;
        icons[SEQUENCE] = new Mesh("mesh/icon_sequence.ply");
        shadows[SEQUENCE]= shadow;
        icons[VIDEO]    = new Mesh("mesh/icon_video.ply");
        shadows[VIDEO]  = shadow;
        icons[SESSION]  = new Mesh("mesh/icon_vimix.ply");
        shadows[SESSION]= shadow;
        icons[CLONE]    = new Mesh("mesh/icon_clone.ply");
        shadows[CLONE]  = shadow;
        icons[RENDER]   = new Mesh("mesh/icon_render.ply");
        shadows[RENDER] = shadow;
        icons[GROUP]    = new Mesh("mesh/icon_group_vimix.ply");
        shadows[GROUP]  = shadow;
        icons[PATTERN]  = new Mesh("mesh/icon_pattern.ply");
        shadows[PATTERN]= shadow;
        icons[CODE]     = new Mesh("mesh/icon_code.ply");
        shadows[CODE]   = shadow;
        icons[CAMERA]   = new Mesh("mesh/icon_camera.ply");
        shadows[CAMERA] = shadow;
        icons[SCREEN]   = new Mesh("mesh/icon_cube.ply");
        shadows[SCREEN] = shadow;
        icons[SHARE]    = new Mesh("mesh/icon_share.ply");
        shadows[SHARE]  = shadow;
        icons[RECEIVE]  = new Mesh("mesh/icon_receive.ply");
        shadows[RECEIVE]= shadow;
        icons[TEXT]     = new Mesh("mesh/icon_text.ply");
        shadows[TEXT]   = shadow;
        icons[BUSY]     = new Mesh("mesh/icon_circles.ply");
        shadows[BUSY]   = nullptr;
        icons[LOCK]     = new Mesh("mesh/icon_lock.ply");
        shadows[LOCK]   = shadow;
        icons[UNLOCK]   = new Mesh("mesh/icon_unlock.ply");
        shadows[UNLOCK] = shadow;
        icons[TELEVISION] = new Mesh("mesh/icon_tv.ply");
        shadows[TELEVISION] = shadow;
        icons[ROTATION]   = new Mesh("mesh/border_handles_rotation.ply");
        shadows[ROTATION] = shadow;
        icons[CIRCLE]   = new Mesh("mesh/icon_circle.ply");
        shadows[CIRCLE] = nullptr;
        icons[CLOCK]    = new Mesh("mesh/icon_clock.ply");
        shadows[CLOCK]  = nullptr;
        icons[CLOCK_H]  = new Mesh("mesh/icon_clock_hand.ply");
        shadows[CLOCK_H]= nullptr;
        icons[SQUARE]   = new Mesh("mesh/icon_square.ply");
        shadows[SQUARE] = nullptr;
        icons[GRID]     = new Mesh("mesh/icon_grid.ply");
        shadows[GRID]   = nullptr;
        icons[CROSS]    = new Mesh("mesh/icon_cross.ply");
        shadows[CROSS]  = nullptr;
        icons[PLAY]     = new Mesh("mesh/icon_play.ply");
        shadows[PLAY]   = shadow;
        icons[FFWRD]    = new Mesh("mesh/icon_fastforward.ply");
        shadows[FFWRD]  = shadow;
        icons[BLEND_NORMAL]    = new Mesh("mesh/icon_blend_normal.ply");
        shadows[BLEND_NORMAL]  = nullptr;
        icons[BLEND_SCREEN]    = new Mesh("mesh/icon_blend_add.ply");
        shadows[BLEND_SCREEN]  = nullptr;
        icons[BLEND_SUBTRACT]  = new Mesh("mesh/icon_blend_subtract.ply");
        shadows[BLEND_SUBTRACT]= nullptr;
        icons[BLEND_MULT]  = new Mesh("mesh/icon_blend_mult.ply");
        shadows[BLEND_MULT]= nullptr;
        icons[BLEND_H_LIGHT]  = new Mesh("mesh/icon_blend_hard_light.ply");
        shadows[BLEND_H_LIGHT]= nullptr;
        icons[BLEND_S_LIGHT]  = new Mesh("mesh/icon_blend_soft_light.ply");
        shadows[BLEND_S_LIGHT]= nullptr;
        icons[BLEND_S_SUB]  = new Mesh("mesh/icon_blend_soft_sub.ply");
        shadows[BLEND_S_SUB]= nullptr;
        icons[BLEND_LIGHTEN]  = new Mesh("mesh/icon_blend_lighten.ply");
        shadows[BLEND_LIGHTEN]= nullptr;
        icons[EMPTY]    = new Mesh("mesh/icon_empty.ply");
        shadows[EMPTY]  = shadow;
    }

    symbol_ = icons[type_];
    shadow_ = shadows[type_];
    translation_ = pos;
    color = glm::vec4( 1.f, 1.f, 1.f, 1.f);
}

Symbol::~Symbol()
{

}

void Symbol::draw(glm::mat4 modelview, glm::mat4 projection)
{
    if ( !initialized() ) {
        if(symbol_ && !symbol_->initialized())
            symbol_->init();
        if(shadow_ && !shadow_->initialized())
            shadow_->init();
        init();
    }

    if ( visible_ && symbol_) {

        // set color
        symbol_->shader()->color = color;

        // rebuild a matrix with rotation (see handles) and translation from modelview + translation_
        // and define scale to be 1, 1
        glm::mat4 ctm;
        glm::vec3 rot(0.f);
        glm::vec4 vec = modelview * glm::vec4(1.f, 0.f, 0.f, 0.f);
        rot.z = glm::orientedAngle( glm::vec3(1.f, 0.f, 0.f), glm::normalize(glm::vec3(vec)), glm::vec3(0.f, 0.f, 1.f) );
        // extract scaling
        ctm = glm::rotate(glm::identity<glm::mat4>(), -rot.z, glm::vec3(0.f, 0.f, 1.f)) * modelview ;
        vec = ctm * glm::vec4(1.f, 1.f, 0.f, 0.f);
        glm::vec3 sca = glm::vec3(vec.y , vec.y, 1.f) * glm::vec3(scale_.y, scale_.y, 1.f);
        // extract translation
        glm::vec3 tran = glm::vec3(modelview[3][0], modelview[3][1], modelview[3][2]) ;
        tran += translation_ * glm::vec3(vec);
        // apply local rotation
        rot.z += rotation_.z;
        // generate matrix
        ctm = GlmToolkit::transform(tran, rot, sca);

        if (shadow_)
            shadow_->draw( ctm, projection );
        symbol_->draw( ctm, projection);
    }
}


void Symbol::accept(Visitor& v)
{
    Node::accept(v);
    v.visit(*this);
}

Mesh *Disk::disk_ = nullptr;

Disk::Disk() : Node()
{
    if (Disk::disk_ == nullptr)
        Disk::disk_ = new Mesh("mesh/disk.ply");

    color = glm::vec4( 1.f, 1.f, 1.f, 1.f);
}

void Disk::draw(glm::mat4 modelview, glm::mat4 projection)
{
    if ( !initialized() ) {
        if (!Disk::disk_->initialized())
            Disk::disk_->init();
        init();
    }

    if ( visible_ ) {

        // set color
        Disk::disk_->shader()->color = color;

        glm::mat4 ctm = modelview * transform_;
        Disk::disk_->draw( ctm, projection);

    }
}

void Disk::accept(Visitor& v)
{
    Node::accept(v);
    v.visit(*this);
}

Surface *Character::font_ = nullptr;

Character::Character(int imgui_font_index) : Node(), character_(' '), font_index_(imgui_font_index), baseline_(0.f), shape_(1.f, 1.f)
{
    if (Character::font_ == nullptr)
        Character::font_ = new Surface;

    uvTransform_ = glm::identity<glm::mat4>();
    color = glm::vec4( 1.f, 1.f, 1.f, 1.f);
}

void Character::draw(glm::mat4 modelview, glm::mat4 projection)
{
    if ( !initialized() ) {

        if (!Character::font_->initialized()) {

            uint tex = (uint)(intptr_t)ImGui::GetIO().Fonts->TexID;
            if ( tex > 0) {
                Character::font_->init();
                font_->setTextureIndex(tex);
            }
        }

        if (Character::font_->initialized()) {
            init();
            setChar(character_);
        }
    }

    if ( visible_ ) {

        // set color
        Character::font_->shader()->color = color;

        // modify the shader iTransform to select UVs of the desired glyph
        Character::font_->shader()->iTransform = uvTransform_;

        // rebuild a matrix with rotation (see handles) and translation from modelview + translation_
        // and define scale to be 1, 1
        glm::mat4 ctm;
        glm::vec3 rot(0.f);
        glm::vec4 vec = modelview * glm::vec4(1.f, 0.f, 0.f, 0.f);
        rot.z = glm::orientedAngle( glm::vec3(1.f, 0.f, 0.f), glm::normalize(glm::vec3(vec)), glm::vec3(0.f, 0.f, 1.f) );
        // extract scaling
        ctm = glm::rotate(glm::identity<glm::mat4>(), -rot.z, glm::vec3(0.f, 0.f, 1.f)) * modelview ;
        vec = ctm * glm::vec4(1.f, 1.f, 0.f, 0.f);
        glm::vec2 sca = glm::vec2(vec.y) * glm::vec2(scale_.y) * shape_;
        // extract translation
        glm::vec3 tran = glm::vec3(modelview[3][0], modelview[3][1], modelview[3][2]) ;
        tran += (translation_  + glm::vec3(0.f, baseline_ * scale_.y, 0.f) ) * glm::vec3(vec);
        // apply local rotation
        rot.z += rotation_.z;
        // generate matrix
        ctm = GlmToolkit::transform(tran, rot, glm::vec3(sca, 1.f));

        Character::font_->draw( ctm, projection);
    }
}

void Character::accept(Visitor& v)
{
    Node::accept(v);
    v.visit(*this);
}

void Character::setChar(char c)
{
    character_ = c;

    if (initialized()) {
        // get from imgui the UVs of the given char
        const ImGuiIO& io = ImGui::GetIO();
        ImFont* myfont = io.Fonts->Fonts[0];
        if (font_index_ > 0 && font_index_ < io.Fonts->Fonts.size() )
            myfont = io.Fonts->Fonts[font_index_];
        const ImFontGlyph* glyph = myfont->FindGlyph(character_);

        if (glyph) {
            // create a texture UV transform to get the UV coordinates of the glyph
            const glm::vec3 uv_t = glm::vec3( glyph->U0, glyph->V0, 0.f);
            const glm::vec3 uv_s = glm::vec3( glyph->U1 - glyph->U0, glyph->V1 - glyph->V0, 1.f);
            const glm::vec3 uv_r = glm::vec3(0.f, 0.f, 0.f);
            uvTransform_ = GlmToolkit::transform(uv_t, uv_r, uv_s);

            // remember glyph shape
            shape_ = glm::vec2(glyph->X1 - glyph->X0, glyph->Y1 - glyph->Y0) / myfont->FontSize;
            baseline_ = -glyph->Y0 / myfont->FontSize;
        }
    }
}

Mesh *DotLine::dot_ = nullptr;
Mesh *DotLine::arrow_ = nullptr;

DotLine::DotLine() : Node()
{
    if (DotLine::dot_ == nullptr)
        DotLine::dot_ = new Mesh("mesh/point.ply");
    if (DotLine::arrow_ == nullptr)
        DotLine::arrow_ = new Mesh("mesh/triangle_point.ply");

    spacing = 0.3f;
    target = glm::vec3( 0.f, 0.f, 0.f);
    color  = glm::vec4( 1.f, 1.f, 1.f, 1.f);
}

void DotLine::draw(glm::mat4 modelview, glm::mat4 projection)
{
    if ( !initialized() ) {
        if (!DotLine::dot_->initialized())
            DotLine::dot_->init();
        if (!DotLine::arrow_->initialized())
            DotLine::arrow_->init();
        init();
    }

    if ( visible_ ) {

        // set color
        DotLine::dot_->shader()->color = color;
        DotLine::arrow_->shader()->color = color;

        glm::mat4 ctm = modelview;
        float angle = glm::orientedAngle( glm::normalize(glm::vec2(0.f,-1.f)), glm::normalize(glm::vec2(target)));
        glm::mat4 R = glm::rotate(glm::identity<glm::mat4>(), angle, glm::vec3(0.f, 0.f, 1.f) );
        R *= glm::scale(glm::identity<glm::mat4>(), glm::vec3(1.0f, 1.5f, 1.f));

        // draw start point
        DotLine::dot_->draw( ctm, projection);

        // draw equally spaced intermediate points
        glm::vec3 inc = target;
        glm::vec3 space = spacing * glm::normalize(target);

        while ( glm::length(inc) > spacing ) {
            inc -= space;
            ctm *= glm::translate(glm::identity<glm::mat4>(), space);
            DotLine::arrow_->draw( ctm * R, projection);
        }

        // draw target point
        ctm = modelview * glm::translate(glm::identity<glm::mat4>(), target);
        DotLine::dot_->draw( ctm, projection);
    }
}

void DotLine::accept(Visitor& v)
{
    Node::accept(v);
    v.visit(*this);
}
