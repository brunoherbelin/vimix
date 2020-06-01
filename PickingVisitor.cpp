#include "PickingVisitor.h"

#include "Log.h"
#include "Primitives.h"
#include "Decorations.h"

#include "GlmToolkit.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>


PickingVisitor::PickingVisitor(glm::vec2 coordinates) : Visitor(), point_(coordinates)
{
    modelview_ = glm::mat4(1.f);
}

void PickingVisitor::visit(Node &n)
{
    // use the transform modified during update
    modelview_ *= n.transform_;

//      modelview_ *= transform(n.translation_, n.rotation_, n.scale_);
//    Log::Info("Node %d", n.id());
//    Log::Info("%s", glm::to_string(modelview_).c_str());
}

void PickingVisitor::visit(Group &n)
{
    glm::mat4 mv = modelview_;
    for (NodeSet::iterator node = n.begin(); node != n.end(); node++) {
        if ( (*node)->visible_ )
            (*node)->accept(*this);
        modelview_ = mv;
    }
}

void PickingVisitor::visit(Switch &n)
{
    glm::mat4 mv = modelview_;
    (*n.activeChild())->accept(*this);
    modelview_ = mv;
}

void PickingVisitor::visit(Primitive &n)
{
    // TODO: generic visitor for primitive with random shape

}

void PickingVisitor::visit(Surface &n)
{
    if (!n.visible_)
        return;

    // apply inverse transform to the point of interest
    glm::vec4 P = glm::inverse(modelview_) * glm::vec4( point_, 0.f, 1.f );

    // test bounding box: it is an exact fit for a resctangular surface
    if ( n.bbox().contains( glm::vec3(P)) )
        // add this surface to the nodes picked
        nodes_.push_back( std::pair(&n, glm::vec2(P)) );
}

void PickingVisitor::visit(Frame &n)
{
//    if (n.border())
//        n.border()->accept(*this);
}

void PickingVisitor::visit(Handles &n)
{
    if (!n.visible_)
        return;

    // apply inverse transform to the point of interest
    glm::vec4 P = glm::inverse(modelview_) * glm::vec4( point_, 0.f, 1.f );

    bool picked = false;
    // test by distance to the corners (more tolerant than bbox)
    if ( n.type() == Handles::RESIZE ) {
        // 4 corners
        picked = ( glm::length(glm::vec2(+1.f, +1.f)-glm::vec2(P)) < 0.05f ||
                   glm::length(glm::vec2(+1.f, -1.f)- glm::vec2(P)) < 0.05f ||
                   glm::length(glm::vec2(-1.f, +1.f)- glm::vec2(P)) < 0.05f ||
                   glm::length(glm::vec2(-1.f, -1.f)- glm::vec2(P)) < 0.05f  );
    }
    else if ( n.type() == Handles::RESIZE_H ){
        // left & right
        picked = ( glm::length(glm::vec2(+1.f, 0.f)- glm::vec2(P)) < 0.05f ||
                   glm::length(glm::vec2(-1.f, 0.f)- glm::vec2(P)) < 0.05f );
    }
    else if ( n.type() == Handles::RESIZE_V ){
        // top & bottom
        picked = ( glm::length(glm::vec2(0.f, +1.f)- glm::vec2(P)) < 0.05f ||
                   glm::length(glm::vec2(0.f, -1.f)- glm::vec2(P)) < 0.05f );
    }
    else if ( n.type() == Handles::ROTATE ){
        picked = glm::length(glm::vec2(+1.1f, +1.1f)- glm::vec2(P)) < 0.1f;
    }

    if ( picked )
        // add this to the nodes picked
        nodes_.push_back( std::pair(&n, glm::vec2(P)) );

}

void PickingVisitor::visit(LineSquare &)
{
    // apply inverse transform to the point of interest
    glm::vec4 P = glm::inverse(modelview_) * glm::vec4( point_, 0.f, 1.f );

    // lower left corner
    glm::vec3 LL = glm::vec3( -1.f, -1.f, 0.f );
    // up right corner
    glm::vec3 UR = glm::vec3( 1.f, 1.f, 0.f );

    // if P is over a line [LL UR] rectangle:
    // TODO
}

void PickingVisitor::visit(LineCircle &n)
{
    // apply inverse transform to the point of interest
    glm::vec4 P = glm::inverse(modelview_) * glm::vec4( point_, 0.f, 1.f );

    float r = glm::length( glm::vec2(P) );
    if ( r < 1.02 && r > 0.98)
        nodes_.push_back( std::pair(&n, glm::vec2(P)) );
}

void PickingVisitor::visit(Scene &n)
{
    // TODO : maybe pick only foreground ?
    n.root()->accept(*this);
}
