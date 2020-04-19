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
//    Log::Info("Node %d", n.id());
//    Log::Info("%s", glm::to_string(modelview_).c_str());
}

void PickingVisitor::visit(Group &n)
{
//    Log::Info("Group %d", n.id());
    glm::mat4 mv = modelview_;

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
    // test if point is inside rectangle

    // lower left corner
    glm::vec4 LL = modelview_ * glm::vec4( -1.f, -1.f, 0.f, 0.f );
    // up right corner
    glm::vec4 UR = modelview_ * glm::vec4( 1.f, 1.f, 0.f, 0.f );

//    Log::Info("Surface %d", n.id());
//    Log::Info("LL %s", glm::to_string(LL).c_str());
//    Log::Info("UR %s", glm::to_string(UR).c_str());

    if ( point_.x > LL.x && point_.x < UR.x && point_.y > LL.y && point_.y < UR.y )
        nodes_.push_back(&n);
}

void PickingVisitor::visit(LineSquare &)
{

}

void PickingVisitor::visit(LineCircle &n)
{

}

void PickingVisitor::visit(Scene &n)
{
    n.root()->accept(*this);
}
