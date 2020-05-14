#include "PickingVisitor.h"

#include "Log.h"
#include "Primitives.h"
#include "Mesh.h"

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

      modelview_ *= n.transform_;
//      modelview_ *= transform(n.translation_, n.rotation_, n.scale_);
//    Log::Info("Node %d", n.id());
//    Log::Info("%s", glm::to_string(modelview_).c_str());
}

void PickingVisitor::visit(Group &n)
{
    glm::mat4 mv = modelview_;
//Log::Info("Group %d", n.id());
    for (NodeSet::iterator node = n.begin(); node != n.end(); node++) {
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

void PickingVisitor::visit(Animation &n)
{

}

void PickingVisitor::visit(Primitive &n)
{

}

void PickingVisitor::visit(Surface &n)
{
//    Log::Info("Surface %d", n.id());
    // apply inverse transform to the point of interest
    glm::vec4 P = glm::inverse(modelview_) * glm::vec4( point_, 0.f, 1.f );

    // lower left corner
    glm::vec3 LL = glm::vec3( -1.f, -1.f, 0.f );
    // up right corner
    glm::vec3 UR = glm::vec3( 1.f, 1.f, 0.f );

    // if P is inside [LL UR] rectangle:
    if ( P.x > LL.x && P.x < UR.x && P.y > LL.y && P.y < UR.y )
        // add this surface to the nodes picked
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
