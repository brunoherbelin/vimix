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


Frame::Frame(CornerType corner, BorderType border, ShadowType shadow) : Node(), side_(nullptr), top_(nullptr), shadow_(nullptr), square_(nullptr)
{
    static Mesh *shadows[3] = {nullptr};
    if (shadows[0] == nullptr)  {
        shadows[0] = new Mesh("mesh/glow.ply", "images/glow.dds");
        shadows[1] = new Mesh("mesh/shadow.ply", "images/shadow.dds");
        shadows[2] = new Mesh("mesh/shadow_perspective.ply", "images/shadow_perspective.dds");
    }
    static Mesh *frames[4] = {nullptr};
    if (frames[0] == nullptr)  {
        frames[0] = new Mesh("mesh/border_round.ply");
        frames[1] = new Mesh("mesh/border_top.ply");
        frames[2] = new Mesh("mesh/border_large_round.ply");
        frames[3] = new Mesh("mesh/border_large_top.ply");
    }
    static LineSquare *sharpframethin = new LineSquare( 3 );
    static LineSquare *sharpframelarge = new LineSquare( 5 );

    if (corner == SHARP) {
        if (border == LARGE)
            square_ = sharpframelarge;
        else
            square_ = sharpframethin;
    }
    else {
        // Round corners
        if (border == THIN) {
            side_  = frames[0];
            top_   = frames[1];
        }
        else{
            side_  = frames[2];
            top_   = frames[3];
        }
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
    if(side_)
        side_->update(dt);
    if(shadow_)
        shadow_->update(dt);
    if(square_)
        square_->update(dt);
}

void Frame::draw(glm::mat4 modelview, glm::mat4 projection)
{
    if ( !initialized() ) {
        if(side_  && !side_->initialized())
            side_->init();
        if(top_  && !top_->initialized())
            top_->init();
        if(shadow_ && !shadow_->initialized())
            shadow_->init();
        if(square_ && !square_->initialized())
            square_->init();
        init();
    }

    if ( visible_ ) {

        glm::mat4 ctm = modelview * transform_;

        // shadow (scaled)
        if(shadow_){
            shadow_->shader()->color.a = 0.98f;
            shadow_->draw( ctm, projection);
        }

        // top (scaled)
        if(top_) {
            top_->shader()->color = color;
            top_->draw( ctm, projection);
        }

        // top (scaled)
        if(square_) {
            square_->shader()->color = color;
            square_->draw( ctm, projection);
        }

        if(side_) {

            side_->shader()->color = color;

            // get scale
            glm::vec4 scale = ctm * glm::vec4(1.f, 1.0f, 0.f, 0.f);

            // get rotation
            glm::vec3 rot(0.f);
            glm::vec4 vec = ctm * glm::vec4(1.f, 0.f, 0.f, 0.f);
            rot.z = glm::orientedAngle( glm::vec3(1.f, 0.f, 0.f), glm::normalize(glm::vec3(vec)), glm::vec3(0.f, 0.f, 1.f) );

            if(side_) {

                // left side
                vec = ctm * glm::vec4(1.f, 0.f, 0.f, 1.f);
                side_->draw( GlmToolkit::transform(vec, rot, glm::vec3(scale.y, scale.y, 1.f)), projection );

                // right side
                vec = ctm * glm::vec4(-1.f, 0.f, 0.f, 1.f);
                side_->draw( GlmToolkit::transform(vec, rot, glm::vec3(-scale.y, scale.y, 1.f)), projection );

            }
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
    static Mesh *handle_active   = new Mesh("mesh/border_handles_overlay_filled.ply");

    if ( !initialized() ) {
        if(handle_ && !handle_->initialized())
            handle_->init();
        if(shadow_ && !shadow_->initialized())
            shadow_->init();
        init();
    }

    if ( visible_ ) {

        // set color
        handle_->shader()->color = color;
        handle_active->shader()->color = color;

        // extract rotation from modelview
        glm::mat4 ctm;
        glm::vec3 rot(0.f);
        glm::vec4 vec = modelview * glm::vec4(1.f, 0.f, 0.f, 0.f);
        rot.z = glm::orientedAngle( glm::vec3(1.f, 0.f, 0.f), glm::normalize(glm::vec3(vec)), glm::vec3(0.f, 0.f, 1.f) );

        // extract scaling and mirroring
        ctm = glm::rotate(glm::identity<glm::mat4>(), -rot.z, glm::vec3(0.f, 0.f, 1.f)) * modelview ;
        vec = ctm *  glm::vec4(1.f, 1.f, 0.f, 0.f);
        glm::vec4 mirror = glm::sign(vec);

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
            ctm = GlmToolkit::transform(vec, rot, glm::vec3(mirror.x, mirror.y, 1.f));
            // 3. draw
            shadow_->draw( ctm, projection );
            handle_->draw( ctm, projection );
        }
        else if ( type_ == Handles::CROP ){
            // one icon in bottom right corner
            // 1. Fixed displacement by (0.12,0.12) along the rotation..
            ctm = GlmToolkit::transform(glm::vec4(0.f), rot, mirror);
            glm::vec4 pos = ctm * glm::vec4(mirror.x * 0.12f, mirror.x * 0.12f, 0.f, 1.f);
            // 2. ..from the bottom right corner (1,1)
            vec = ( modelview * glm::vec4(-1.f, -1.f, 0.f, 1.f) ) + pos;
            ctm = GlmToolkit::transform(vec, rot, glm::vec3(mirror.x, mirror.y, 1.f));
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
        icons[VIDEO]    = new Mesh("mesh/icon_video.ply");
        shadows[VIDEO]  = shadow;
        icons[SESSION]  = new Mesh("mesh/icon_vimix.ply");
        shadows[SESSION]= shadow;
        icons[CLONE]    = new Mesh("mesh/icon_clone.ply");
        shadows[CLONE]  = shadow;
        icons[RENDER]   = new Mesh("mesh/icon_render.ply");
        shadows[RENDER] = shadow;
        icons[PATTERN]  = new Mesh("mesh/icon_gear.ply");
        shadows[PATTERN]= shadow;
        icons[CAMERA]   = new Mesh("mesh/icon_camera.ply");
        shadows[CAMERA] = shadow;
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
        icons[ARROWS]   = new Mesh("mesh/icon_rightarrow.ply");
        shadows[ARROWS] = shadow;
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


//SelectionBox::SelectionBox()
//{
////    color = glm::vec4( 1.f, 1.f, 1.f, 1.f);
//    color = glm::vec4( 1.f, 0.f, 0.f, 1.f);
//    square_ = new LineSquare( 3 );

//}

//void SelectionBox::draw (glm::mat4 modelview, glm::mat4 projection)
//{
//    if ( !initialized() ) {
//        square_->init();
//        init();
//    }

//    if (visible_) {

//        // use a visitor bounding box to calculate extend of all selected nodes
//        BoundingBoxVisitor vbox;

//        // visit every child of the selection
//        for (NodeSet::iterator node = children_.begin();
//             node != children_.end(); node++) {
//            // reset the transform before
//            vbox.setModelview(glm::identity<glm::mat4>());
//            (*node)->accept(vbox);
//        }

//        // get the bounding box
//        bbox_ = vbox.bbox();

////        Log::Info("                                       -------- visitor box (%f, %f)-(%f, %f)", bbox_.min().x, bbox_.min().y, bbox_.max().x, bbox_.max().y);

//        // set color
//        square_->shader()->color = color;

//        // compute transformation from bounding box
////        glm::mat4 ctm = modelview * GlmToolkit::transform(glm::vec3(0.f), glm::vec3(0.f), glm::vec3(1.f));
//        glm::mat4 ctm = modelview * GlmToolkit::transform(bbox_.center(), glm::vec3(0.f), bbox_.scale());

//        // draw bbox
////        square_->draw( modelview, projection);
//        square_->draw( ctm, projection);

//        // DEBUG
////        visible_=false;
//    }

//}


