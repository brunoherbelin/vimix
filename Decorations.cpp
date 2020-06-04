#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/vector_angle.hpp>

#include "Decorations.h"

#include "Visitor.h"
#include "ImageShader.h"
#include "GlmToolkit.h"


Frame::Frame(Type type) : Node(), type_(type), side_(nullptr), top_(nullptr), shadow_(nullptr), square_(nullptr)
{
    color = glm::vec4( 1.f, 1.f, 1.f, 1.f);
    switch (type) {
    case SHARP_LARGE:
        square_ = new LineSquare(color, 3 );
        shadow_ = new Mesh("mesh/glow.ply", "images/glow.dds");
        break;
    case SHARP_THIN:
        square_ = new LineSquare(color, 3 );
        break;
    case ROUND_LARGE:
        side_  = new Mesh("mesh/border_large_round.ply");
        top_   = new Mesh("mesh/border_large_top.ply");
        shadow_  = new Mesh("mesh/shadow.ply", "images/shadow.dds");
        break;
    default:
    case ROUND_THIN:
        side_  = new Mesh("mesh/border_round.ply");
        top_   = new Mesh("mesh/border_top.ply");
        shadow_  = new Mesh("mesh/shadow.ply", "images/shadow.dds");
        break;
    case ROUND_SHADOW:
        side_  = new Mesh("mesh/border_round.ply");
        top_   = new Mesh("mesh/border_top.ply");
        shadow_  = new Mesh("mesh/shadow_perspective.ply", "images/shadow_perspective.dds");
        break;
    }

}

Frame::~Frame()
{
    if(side_)  delete side_;
    if(top_)  delete top_;
    if(shadow_)  delete shadow_;
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
}

void Frame::draw(glm::mat4 modelview, glm::mat4 projection)
{
    if ( !initialized() ) {
        if(side_)  side_->init();
        if(top_)   top_->init();
        if(shadow_)  shadow_->init();
        init();
    }

    if ( visible_ ) {

        glm::mat4 ctm = modelview * transform_;

        // shadow (scaled)
        if(shadow_){
            shadow_->shader()->color.a = 0.8f;
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
    color   = glm::vec4( 1.f, 1.f, 0.f, 1.f);

    if ( type_ == ROTATE ) {
        handle_ = new Mesh("mesh/border_handles_rotation.ply");
    }
    else {
//        handle_ = new LineSquare(color, int ( 2.1f * Rendering::manager().DPIScale()) );
        handle_ = new Mesh("mesh/border_handles_overlay.ply");
    }

}

Handles::~Handles()
{
    if(handle_) delete handle_;
}

void Handles::update( float dt )
{
    Node::update(dt);
    handle_->update(dt);
}

void Handles::draw(glm::mat4 modelview, glm::mat4 projection)
{
    if ( !initialized() ) {
        if(handle_) handle_->init();
        init();
    }

    if ( visible_ ) {

        // set color
        handle_->shader()->color = color;

        glm::mat4 ctm;
        glm::vec3 rot(0.f);
        glm::vec4 vec = modelview * glm::vec4(1.f, 0.f, 0.f, 0.f);
        rot.z = glm::orientedAngle( glm::vec3(1.f, 0.f, 0.f), glm::normalize(glm::vec3(vec)), glm::vec3(0.f, 0.f, 1.f) );

        if ( type_ == RESIZE ) {

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
        }
        else if ( type_ == RESIZE_H ){
            // left and right
            vec = modelview * glm::vec4(1.f, 0.f, 0.f, 1.f);
            ctm = GlmToolkit::transform(vec, rot, glm::vec3(1.f));
            handle_->draw( ctm, projection );

            vec = modelview * glm::vec4(-1.f, 0.f, 0.f, 1.f);
            ctm = GlmToolkit::transform(vec, rot, glm::vec3(1.f));
            handle_->draw( ctm, projection );
        }
        else if ( type_ == RESIZE_V ){
            // top and bottom
            vec = modelview * glm::vec4(0.f, 1.f, 0.f, 1.f);
            ctm = GlmToolkit::transform(vec, rot, glm::vec3(1.f));
            handle_->draw( ctm, projection );

            vec = modelview * glm::vec4(0.f, -1.f, 0.f, 1.f);
            ctm = GlmToolkit::transform(vec, rot, glm::vec3(1.f));
            handle_->draw( ctm, projection );
        }
        else if ( type_ == ROTATE ){
            // one icon in top right corner
            // 1. Fixed displacement by (0.12,0.12) along the rotation..
            ctm = GlmToolkit::transform(glm::vec4(0.f), rot, glm::vec3(1.f));
            vec = ctm * glm::vec4(0.12f, 0.12f, 0.f, 1.f);
            // 2. ..from the top right corner (1,1)
            vec = ( modelview * glm::vec4(1.f, 1.f, 0.f, 1.f) ) + vec;
            ctm = GlmToolkit::transform(vec, rot, glm::vec3(1.f));
            handle_->draw( ctm, projection );
        }
    }
}


void Handles::accept(Visitor& v)
{
    Node::accept(v);
    v.visit(*this);
}


Icon::Icon(Type style) : Node()
{
    color   = glm::vec4( 1.f, 1.f, 1.f, 1.f);
    translation_ = glm::vec3(0.8f, 0.8f, 0.f);

    switch (style) {
    case IMAGE:
        icon_  = new Mesh("mesh/icon_image.ply");
        break;
    case VIDEO:
        icon_  = new Mesh("mesh/icon_video.ply");
        break;
    case SESSION:
        icon_  = new Mesh("mesh/icon_vimix.ply");
        break;
    case CLONE:
        icon_  = new Mesh("mesh/icon_clone.ply");
        break;
    case RENDER:
        icon_  = new Mesh("mesh/icon_render.ply");
        break;
    case EMPTY:
        icon_  = new Mesh("mesh/icon_empty.ply");
        break;
    default:
    case GENERIC:
        icon_  = new Mesh("mesh/point.ply");
        translation_ = glm::vec3(0.f, 0.f, 0.f);
        break;
    }

}

Icon::~Icon()
{
    if(icon_)  delete icon_;
}

void Icon::draw(glm::mat4 modelview, glm::mat4 projection)
{
    if ( !initialized() ) {
        if(icon_)  icon_->init();
        init();
    }

    if ( visible_ ) {

        if(icon_) {
            // set color
            icon_->shader()->color = color;

            glm::mat4 ctm = modelview * transform_;
            // correct for aspect ratio
            glm::vec4 vec = ctm * glm::vec4(1.f, 1.0f, 0.f, 0.f);
            ctm *= glm::scale(glm::identity<glm::mat4>(), glm::vec3( vec.y / vec.x, 1.f, 1.f));

            icon_->draw( ctm, projection);
        }
    }
}


void Icon::accept(Visitor& v)
{
    Node::accept(v);
    v.visit(*this);
}
