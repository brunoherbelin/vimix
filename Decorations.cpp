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

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <glm/gtx/component_wise.hpp>

#include "Decorations.h"

#include "Visitor.h"
#include "BoundingBoxVisitor.h"
#include "ImageShader.h"
#include "GlmToolkit.h"
#include "Resource.h"
#include "Log.h"

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
    static Mesh *handle_menu     = new Mesh("mesh/border_handles_menu.ply");
    static Mesh *handle_lock     = new Mesh("mesh/border_handles_lock.ply");
    static Mesh *handle_unlock   = new Mesh("mesh/border_handles_lock_open.ply");
    static Mesh *handle_shadow   = new Mesh("mesh/border_handles_shadow.ply", "images/soft_shadow.dds");

    if ( type_ == Handles::ROTATE ) {
        handle_ = handle_rotation;
    }
    else if ( type_ == Handles::SCALE ) {
        handle_ = handle_scale;
    }
    else if ( type_ == Handles::MENU ) {
        handle_ = handle_menu;
    }
    else if ( type_ == Handles::CROP ) {
        handle_ = handle_crop;
    }
    else if ( type_ == Handles::LOCKED ) {
        handle_ = handle_lock;
    }
    else if ( type_ == Handles::UNLOCKED ) {
        handle_ = handle_unlock;
    }
    else {
        handle_ = handle_corner;
    }

    color   = glm::vec4( 1.f, 1.f, 1.f, 1.f);
    corner_ = glm::vec2(0.f, 0.f);
    shadow_ = handle_shadow;
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
        else if ( type_ == Handles::CROP ){
            // one icon in bottom right corner
            // 1. Fixed displacement by (0.12,0.12) along the rotation..
            ctm = GlmToolkit::transform(glm::vec4(0.f), rot, mirror);
            glm::vec4 pos = ctm * glm::vec4(mirror.x * 0.12f, mirror.x * 0.12f, 0.f, 1.f);
            // 2. ..from the bottom right corner (1,-1)
            vec = ( modelview * glm::vec4(-1.f, -1.f, 0.f, 1.f) ) + pos;
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
            // one icon in top left corner
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
        icons[PATTERN]  = new Mesh("mesh/icon_gear.ply");
        shadows[PATTERN]= shadow;
        icons[CAMERA]   = new Mesh("mesh/icon_camera.ply");
        shadows[CAMERA] = shadow;
        icons[CUBE]    = new Mesh("mesh/icon_cube.ply");
        shadows[CUBE]  = shadow;
        icons[SHARE]    = new Mesh("mesh/icon_share.ply");
        shadows[SHARE]  = shadow;
        icons[DOTS]     = new Mesh("mesh/icon_dots.ply");
        shadows[DOTS]   = nullptr;
        icons[BUSY]     = new Mesh("mesh/icon_circles.ply");
        shadows[BUSY]   = nullptr;
        icons[LOCK]     = new Mesh("mesh/icon_lock.ply");
        shadows[LOCK]   = shadow;
        icons[UNLOCK]   = new Mesh("mesh/icon_unlock.ply");
        shadows[UNLOCK] = shadow;
        icons[EYE]      = new Mesh("mesh/icon_eye.ply");
        shadows[EYE]    = shadow;
        icons[EYESLASH] = new Mesh("mesh/icon_eye_slash.ply");
        shadows[EYESLASH] = shadow;
        icons[VECTORSLASH] = new Mesh("mesh/icon_vector_square_slash.ply");
        shadows[VECTORSLASH] = shadow;
        icons[ARROWS]   = new Mesh("mesh/icon_rightarrow.ply");
        shadows[ARROWS] = shadow;
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
        icons[CROSS]    = new Mesh("mesh/icon_cross.ply");
        shadows[CROSS]  = nullptr;
        icons[GRID]     = new Mesh("mesh/icon_grid.ply");
        shadows[GRID]   = nullptr;
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

Disk::~Disk()
{

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



//FrameBuffer *textToFrameBuffer(const std::string &text, font_style type, uint h)
//{
//    FrameBuffer *fb = nullptr;

//    uint w = 0;
//    for (auto i = 0; i < text.size(); ++i) {
//        const ImFontGlyph* glyph = fontmap[type]->FindGlyph( text[i] );
//        w += h * (uint) ceil( (glyph->X1 - glyph->X0) /  (glyph->Y1 - glyph->Y0) );
//    }
//    if (w > 0) {

//        // create and fill a FrameBuffer with the ImGui Font texture (once)
//        static uint framebufferFont_ = 0;
//        static uint textureFont_ = 0;
//        static int widthFont_ =0;
//        static int heightFont_ =0;
//        if (framebufferFont_ == 0) {
//            glGenTextures(1, &textureFont_);
//            glBindTexture(GL_TEXTURE_2D, textureFont_);
//            unsigned char *pixels;
//            int bpp = 0;
//            ImGuiIO& io = ImGui::GetIO();
//            io.Fonts->GetTexDataAsRGBA32(&pixels, &widthFont_, &heightFont_, &bpp);
//            glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA, widthFont_, heightFont_);
//            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, widthFont_, heightFont_, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
//            glBindTexture(GL_TEXTURE_2D, 0);
//            glGenFramebuffers(1, &framebufferFont_);
//            glBindFramebuffer(GL_FRAMEBUFFER, framebufferFont_);
//            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureFont_, 0);
//        }

//        // create an empty FrameBuffer for the target glyphs
//        fb = new FrameBuffer(w, h, true);

////        uint textureGlyphs_ = 0;
////        glGenTextures(1, &textureGlyphs_);
////        glBindTexture(GL_TEXTURE_2D, textureGlyphs_);
////        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA, w, h);
////        glBindTexture(GL_TEXTURE_2D, 0);
////        uint framebufferGlyphs_ = 0;
////        glGenFramebuffers(1, &framebufferGlyphs_);
////        glBindFramebuffer(GL_FRAMEBUFFER, framebufferGlyphs_);
////        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureGlyphs_, 0);

////        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebufferGlyphs_);


//        // blit glyphs from framebufferFont_ to framebuffer target
//        glBindFramebuffer(GL_READ_FRAMEBUFFER, framebufferFont_);
//        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb->opengl_id() );

//        GLint write_x = 0;
//        for (auto i = 0; i < text.size(); ++i) {
//            const ImFontGlyph* glyph = fontmap[type]->FindGlyph( text[i] );
//            w = h * (uint) ceil( (glyph->X1 - glyph->X0) /  (glyph->Y1 - glyph->Y0) );

//            GLint read_x0 = (GLint) ( glyph->U0 * (float) widthFont_ );
//            GLint read_y0 = (GLint) ( glyph->V0 * (float) heightFont_ );
//            GLint read_x1 = (GLint) ( glyph->U1 * (float) widthFont_ );
//            GLint read_y1 = (GLint) ( glyph->V1 * (float) heightFont_ );
//            glBlitFramebuffer(read_x0, read_y0, read_x1, read_y1,
//                              write_x, 0, w, h, GL_COLOR_BUFFER_BIT, GL_LINEAR);

//            write_x += w;
//        }

//    }

//    return fb;
//}


