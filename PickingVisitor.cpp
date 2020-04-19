#include "PickingVisitor.h"

#include "Log.h"
#include "Primitives.h"
#include "Mesh.h"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>


PickingVisitor::PickingVisitor(glm::vec2 coordinates) : Visitor(), point_(coordinates), node_(nullptr)
{

}

void PickingVisitor::visit(Node &n)
{

}

void PickingVisitor::visit(Group &n)
{
    for (NodeSet::iterator node = n.begin(); node != n.end(); node++) {
        (*node)->accept(*this);
    }
}

void PickingVisitor::visit(Switch &n)
{
    (*n.activeChild())->accept(*this);
}

void PickingVisitor::visit(Animation &n)
{

}

void PickingVisitor::visit(Primitive &n)
{

}

void PickingVisitor::visit(ImageSurface &n)
{

}

void PickingVisitor::visit(FrameBufferSurface &n)
{

}

void PickingVisitor::visit(MediaSurface &n)
{

}

void PickingVisitor::visit(LineStrip &n)
{

}

void PickingVisitor::visit(LineSquare &)
{

}

void PickingVisitor::visit(LineCircle &n)
{

}

void PickingVisitor::visit(Mesh &n)
{

}

void PickingVisitor::visit(Scene &n)
{
    n.root()->accept(*this);
}
