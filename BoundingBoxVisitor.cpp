#include <glm/gtc/matrix_transform.hpp>

#include "BoundingBoxVisitor.h"

#include "Log.h"
#include "Primitives.h"
#include "Decorations.h"

BoundingBoxVisitor::BoundingBoxVisitor(bool force): force_(force)
{
    modelview_ = glm::identity<glm::mat4>();

}

void BoundingBoxVisitor::setModelview(glm::mat4 modelview)
{
    modelview_ = modelview;
}

GlmToolkit::AxisAlignedBoundingBox BoundingBoxVisitor::bbox()
{
    return bbox_;
}

void BoundingBoxVisitor::visit(Node &n)
{
    // use the transform modified during update    modelview_ *= n.transform_;
    glm::mat4 transform_local = GlmToolkit::transform(n.translation_, n.rotation_, n.scale_);
    modelview_ *= transform_local;
}

void BoundingBoxVisitor::visit(Group &n)
{
    if (!n.visible_)
        return;
    glm::mat4 mv = modelview_;
    for (NodeSet::iterator node = n.begin(); node != n.end(); node++) {
        if ( (*node)->visible_ || force_)
            (*node)->accept(*this);
        modelview_ = mv;
    }
}

void BoundingBoxVisitor::visit(Switch &n)
{
    if (!n.visible_ || n.numChildren() < 1)
        return;
    glm::mat4 mv = modelview_;
    if ( n.activeChild()->visible_ || force_)
        n.activeChild()->accept(*this);
    modelview_ = mv;
}

void BoundingBoxVisitor::visit(Primitive &n)
{

    bbox_.extend(n.bbox().transformed(modelview_));

//    Log::Info("visitor box (%f, %f)-(%f, %f)", bbox_.min().x, bbox_.min().y, bbox_.max().x, bbox_.max().y);
}

void BoundingBoxVisitor::visit(Scene &n)
{
    n.ws()->accept(*this);
}

