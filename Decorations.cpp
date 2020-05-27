
#include <glad/glad.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/vector_angle.hpp>


#include "Resource.h"
#include "Visitor.h"
#include "ImageShader.h"
#include "Log.h"
#include "GlmToolkit.h"
#include "Decorations.h"


Frame::Frame(Type type) : Node(), type_(type)
{
    color   = glm::vec4( 1.f, 1.f, 1.f, 1.f);
    switch (type) {
    case SHARP_LARGE:
        border_  = nullptr; //new Mesh("mesh/border_large_sharp.ply");
        shadow_  = new Mesh("mesh/shadow.ply", "images/shadow.png");
        break;
    case SHARP_THIN:
        border_  = new Mesh("mesh/border_sharp.ply");
        shadow_  = nullptr;
        break;
    case ROUND_LARGE:
        border_  = new Mesh("mesh/border_large_round.ply");
        shadow_  = new Mesh("mesh/shadow.ply", "images/shadow.png");
        break;
    default:
    case ROUND_THIN:
        border_  = new Mesh("mesh/border_round.ply");
        shadow_  = new Mesh("mesh/shadow.ply", "images/shadow.png");
        break;
    case ROUND_SHADOW:
        border_  = new Mesh("mesh/border_round.ply");
        shadow_  = new Mesh("mesh/shadow_perspective.ply", "images/shadow_perspective.png");
        break;
    }
}

Frame::~Frame()
{
    if(border_)  delete border_;
    if(shadow_)  delete shadow_;
}

void Frame::draw(glm::mat4 modelview, glm::mat4 projection)
{
    if ( !initialized() ) {
        if(border_)  border_->init();
        if(shadow_)  shadow_->init();
        init();
    }

    if ( visible_ ) {

        // enable antialiasing
        glEnable(GL_MULTISAMPLE_ARB);

        // shadow
        if(shadow_)
            shadow_->draw( modelview * transform_, projection);

        if(border_) {
            // right side
            float ar = scale_.x / scale_.y;
            glm::vec3 s(1.f, 1.f, 1.f);
            //        glm::vec3 s(scale_.y, scale_.y, 1.0);
            glm::vec3 t(translation_.x - 1.0 + ar, translation_.y, translation_.z);
            glm::mat4 ctm = modelview * GlmToolkit::transform(t, rotation_, s);

            if(border_) {
                // right side
                border_->shader()->color = color;
                border_->draw( ctm, projection );
                // left side
                t.x = -t.x;
                s.x = -s.x;
                ctm = modelview * GlmToolkit::transform(t, rotation_, s);
                border_->draw( ctm, projection );
            }
        }
        // enable antialiasing
        glDisable(GL_MULTISAMPLE_ARB);
    }
}


void Frame::accept(Visitor& v)
{
    Node::accept(v);
    v.visit(*this);
}

Handles::Handles(Type type) : Node(), type_(type)
{
    color   = glm::vec4( 1.f, 1.f, 1.f, 1.f);

    if ( type_ == ROTATE ) {
        handle_ = new Mesh("mesh/border_handles_rotation.ply");
    }
    else {
//        handle_ = new LineSquare(color, int ( 2.1f * Rendering::manager().DPIScale()) );
        handle_ = new Mesh("mesh/border_handles_overlay.ply");
//        handle_->scale_ = glm::vec3( 0.05f, 0.05f, 1.f);
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
        // enable antialiasing
        glEnable(GL_MULTISAMPLE_ARB);

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
            // only once in upper top right corner
            glm::vec4 pos = modelview * glm::vec4(1.0f, 1.0f, 0.f, 1.f);
            ctm = GlmToolkit::transform(glm::vec3(pos) + glm::vec3(0.12f, 0.12f, 0.f), glm::vec3(0.f), glm::vec3(1.f));
            handle_->draw( ctm, projection );
        }

        glDisable(GL_MULTISAMPLE_ARB);
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
    default:
    case GENERIC:
        icon_  = new Mesh("mesh/icon_empty.ply");
        break;
    }

    translation_ = glm::vec3(0.8f, 0.8f, 0.f);
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

        // enable antialiasing
        glEnable(GL_MULTISAMPLE_ARB);

        if(icon_) {
            // set color
            icon_->shader()->color = color;

            glm::mat4 ctm = modelview * transform_;
            // correct for aspect ratio
            glm::vec4 vec = ctm * glm::vec4(1.f, 1.0f, 0.f, 0.f);
            ctm *= glm::scale(glm::identity<glm::mat4>(), glm::vec3( vec.y / vec.x, 1.f, 1.f));

            icon_->draw( ctm, projection);
        }

        // enable antialiasing
        glDisable(GL_MULTISAMPLE_ARB);
    }
}


void Icon::accept(Visitor& v)
{
    Node::accept(v);
    v.visit(*this);
}
