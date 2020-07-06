#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <glm/gtx/component_wise.hpp>

#include "Decorations.h"

#include "Visitor.h"
#include "BoundingBoxVisitor.h"
#include "ImageShader.h"
#include "GlmToolkit.h"
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

//    switch (type) {
//    case SHARP_LARGE:
//        square_ = sharpframe;
//        shadow_ = shadows[0];
//        break;
//    case SHARP_THIN:
//        square_ = sharpframe;
//        break;
//    case ROUND_LARGE:
//        side_  = frames[2];
//        top_   = frames[3];
//        shadow_ = shadows[1];
//        break;
//    default:
//    case ROUND_THIN:
//        side_  = frames[0];
//        top_   = frames[1];
//        shadow_ = shadows[1];
//        break;
//    case ROUND_THIN_PERSPECTIVE:
//        side_  = frames[0];
//        top_   = frames[1];
//        shadow_ = shadows[2];
//        break;
//    }

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
    static Mesh *handle_rotation_ = new Mesh("mesh/border_handles_rotation.ply");
    static Mesh *handle_corner    = new Mesh("mesh/border_handles_overlay.ply");

    color   = glm::vec4( 1.f, 1.f, 0.f, 1.f);
    if ( type_ == ROTATE ) {
        handle_ = handle_rotation_;
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
        init();
    }

    if ( visible_ ) {

        // set color
        handle_->shader()->color = color;

        glm::mat4 ctm;
        glm::vec3 rot(0.f);
        glm::vec4 vec = modelview * glm::vec4(1.f, 0.f, 0.f, 0.f);
        rot.z = glm::orientedAngle( glm::vec3(1.f, 0.f, 0.f), glm::normalize(glm::vec3(vec)), glm::vec3(0.f, 0.f, 1.f) );
        vec = modelview * glm::vec4(0.f, 1.f, 0.f, 1.f);
//        glm::vec3 scale( vec.x > 0.f ? 1.f : -1.f, vec.y > 0.f ? 1.f : -1.f, 1.f);
//        glm::vec3 scale(1.f, 1.f, 1.f);

//        Log::Info(" (0,1) becomes (%f, %f)", scale.x, scale.y);

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
            glm::vec4 pos = ctm * glm::vec4(0.12f, 0.12f, 0.f, 1.f);
//            Log::Info(" (0.12,0.12) becomes (%f, %f)", pos.x, pos.y);
            // 2. ..from the top right corner (1,1)
            vec = ( modelview * glm::vec4(1.f, 1.f, 0.f, 1.f) ) + pos;
            ctm = GlmToolkit::transform(vec, rot, glm::vec3(1.f));

// TODO fix problem with negative scale
//            glm::vec4 target = modelview * glm::vec4(1.2f, 1.2f, 0.f, 1.f);

//            vec = modelview * glm::vec4(1.f, 1.f, 0.f, 1.f);
//            glm::vec4 dv = target - vec;

//            Log::Info("dv  (%f, %f)", dv.x, dv.y);
//            float m = dv.x < dv.y ? dv.x : dv.y;
//            Log::Info("min  %f", m);

//            ctm = GlmToolkit::transform( glm::vec3(target), rot, glm::vec3(1.f));

            handle_->draw( ctm, projection );
        }
    }
}


void Handles::accept(Visitor& v)
{
    Node::accept(v);
    v.visit(*this);
}


Symbol::Symbol(Type style, glm::vec3 pos) : Node()
{
    static Mesh *icons[7] = {nullptr};
    if (icons[0] == nullptr)  {
        icons[IMAGE]   = new Mesh("mesh/icon_image.ply");
        icons[VIDEO]   = new Mesh("mesh/icon_video.ply");
        icons[SESSION] = new Mesh("mesh/icon_vimix.ply");
        icons[CLONE]   = new Mesh("mesh/icon_clone.ply");
        icons[RENDER]  = new Mesh("mesh/icon_render.ply");
        icons[EMPTY]   = new Mesh("mesh/icon_empty.ply");
        icons[GENERIC] = new Mesh("mesh/point.ply");
    }

    symbol_ = icons[style];
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
        init();
    }

    if ( visible_ ) {

        if(symbol_) {
            // set color
            symbol_->shader()->color = color;

            glm::mat4 ctm = modelview * transform_;
            // correct for aspect ratio
            glm::vec4 vec = ctm * glm::vec4(1.f, 1.0f, 0.f, 0.f);
            ctm *= glm::scale(glm::identity<glm::mat4>(), glm::vec3( vec.y / vec.x, 1.f, 1.f));

            symbol_->draw( ctm, projection);
        }
    }
}


void Symbol::accept(Visitor& v)
{
    Node::accept(v);
    v.visit(*this);
}


SelectionBox::SelectionBox()
{
//    color = glm::vec4( 1.f, 1.f, 1.f, 1.f);
    color = glm::vec4( 1.f, 0.f, 0.f, 1.f);
    square_ = new LineSquare( 3 );

}

void SelectionBox::draw (glm::mat4 modelview, glm::mat4 projection)
{
    if ( !initialized() ) {
        square_->init();
        init();
    }

    if (visible_) {

        // use a visitor bounding box to calculate extend of all selected nodes
        BoundingBoxVisitor vbox;

        // visit every child of the selection
        for (NodeSet::iterator node = children_.begin();
             node != children_.end(); node++) {
            // reset the transform before
            vbox.setModelview(glm::identity<glm::mat4>());
            (*node)->accept(vbox);
        }

        // get the bounding box
        bbox_ = vbox.bbox();

//        Log::Info("                                       -------- visitor box (%f, %f)-(%f, %f)", bbox_.min().x, bbox_.min().y, bbox_.max().x, bbox_.max().y);

        // set color
        square_->shader()->color = color;

        // compute transformation from bounding box
//        glm::mat4 ctm = modelview * GlmToolkit::transform(glm::vec3(0.f), glm::vec3(0.f), glm::vec3(1.f));
        glm::mat4 ctm = modelview * GlmToolkit::transform(bbox_.center(), glm::vec3(0.f), bbox_.scale());

        // draw bbox
//        square_->draw( modelview, projection);
        square_->draw( ctm, projection);

        // DEBUG
//        visible_=false;
    }

}


