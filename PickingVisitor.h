#ifndef PICKINGVISITOR_H
#define PICKINGVISITOR_H

#include <glm/glm.hpp>
#include "Visitor.h"

class PickingVisitor: public Visitor
{
    glm::vec2 point_;
    glm::mat4 modelview_;
    Node *node_;

public:
    PickingVisitor(glm::vec2 coordinates);
    Node *picked() { return node_; }

    // Elements of Scene
    void visit(Scene& n);
    void visit(Node& n);
    void visit(Group& n);
    void visit(Switch& n);
    void visit(Animation& n);
    void visit(Primitive& n);
    void visit(ImageSurface& n);
    void visit(MediaSurface& n);
    void visit(FrameBufferSurface& n);
    void visit(LineStrip& n);
    void visit(LineSquare&);
    void visit(LineCircle& n);
    void visit(Mesh& n);

    // not picking other Elements with attributes
    void visit(MediaPlayer&) {}
    void visit(Shader&) {}
    void visit(ImageShader&) {}
};

#endif // PICKINGVISITOR_H
