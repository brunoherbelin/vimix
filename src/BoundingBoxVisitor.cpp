#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#include "View.h"
#include "Scene.h"
#include "Source.h"

#include "BoundingBoxVisitor.h"

BoundingBoxVisitor::BoundingBoxVisitor(bool force): force_(force), modelview_(glm::identity<glm::mat4>())
{

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
    for (NodeSet::iterator node = n.begin(); node != n.end(); ++node) {
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
}

void BoundingBoxVisitor::visit(Scene &n)
{
    n.ws()->accept(*this);
    if (force_){
        n.bg()->accept(*this);
        n.fg()->accept(*this);
    }
}

GlmToolkit::AxisAlignedBoundingBox BoundingBoxVisitor::AABB(SourceList l, View *view)
{
    // calculate bbox on selection
    BoundingBoxVisitor selection_visitor_bbox;
    for (auto it = l.begin(); it != l.end(); ++it) {
        // calculate bounding box of area covered by selection
        selection_visitor_bbox.setModelview( view->scene.ws()->transform_ );
        (*it)->group( view->mode() )->accept(selection_visitor_bbox);
    }

    return selection_visitor_bbox.bbox();
}

GlmToolkit::OrientedBoundingBox BoundingBoxVisitor::OBB(SourceList l, View *view)
{
    GlmToolkit::OrientedBoundingBox obb_;

    // try the orientation of each source in the list
    for (auto source_it = l.begin(); source_it != l.end(); ++source_it) {

        float angle = (*source_it)->group( view->mode() )->rotation_.z;
        glm::mat4 transform = view->scene.ws()->transform_;
        transform = glm::rotate(transform, -angle, glm::vec3(0.f, 0.f, 1.f) );

        // calculate bbox of the list in this orientation
        BoundingBoxVisitor selection_visitor_bbox;
        for (auto it = l.begin(); it != l.end(); ++it) {
            // calculate bounding box of area covered by sources' nodes
            selection_visitor_bbox.setModelview( transform );
            (*it)->group( view->mode() )->accept(selection_visitor_bbox);
        }

        // if not initialized or if new bbox is smaller than previous
        if ( obb_.aabb.isNull() || selection_visitor_bbox.bbox() < obb_.aabb) {
            // keep this bbox as candidate
            obb_.aabb = selection_visitor_bbox.bbox();
            obb_.orientation = glm::vec3(0.f, 0.f, angle);
        }

    }

    return obb_;
}

