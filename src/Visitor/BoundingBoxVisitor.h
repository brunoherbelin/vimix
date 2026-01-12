#ifndef BOUNDINGBOXVISITOR_H
#define BOUNDINGBOXVISITOR_H

#include "Toolkit/GlmToolkit.h"
#include "Visitor.h"
#include "Source/SourceList.h"
#include "View/View.h"

class BoundingBoxVisitor: public Visitor
{
    bool force_;
    glm::mat4 modelview_;
    GlmToolkit::AxisAlignedBoundingBox bbox_;


public:
    BoundingBoxVisitor(bool force = false);

    void setModelview(glm::mat4 modelview);
    GlmToolkit::AxisAlignedBoundingBox bbox();

    // Elements of Scene
    void visit(Scene& n) override;
    void visit(Node& n) override;
    void visit(Group& n) override;
    void visit(Switch& n) override;
    void visit(Primitive& n) override;

    static GlmToolkit::AxisAlignedBoundingBox AABB(SourceList l, Group *root, View::Mode mode);
    static GlmToolkit::OrientedBoundingBox OBB(SourceList l, Group *root, View::Mode mode);
};

#endif // BOUNDINGBOXVISITOR_H
