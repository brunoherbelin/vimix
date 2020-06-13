#ifndef BOUNDINGBOXVISITOR_H
#define BOUNDINGBOXVISITOR_H

#include "GlmToolkit.h"
#include "Visitor.h"


class BoundingBoxVisitor: public Visitor
{
    glm::mat4 modelview_;
    GlmToolkit::AxisAlignedBoundingBox bbox_;

public:

    BoundingBoxVisitor();

    void setModelview(glm::mat4 modelview);
    GlmToolkit::AxisAlignedBoundingBox bbox();

    // Elements of Scene
    void visit(Scene& n) override;
    void visit(Node& n) override;
    void visit(Group& n) override;
    void visit(Switch& n) override;
    void visit(Primitive& n) override;
};

#endif // BOUNDINGBOXVISITOR_H
