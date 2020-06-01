#ifndef DRAWVISITOR_H
#define DRAWVISITOR_H

#include <glm/glm.hpp>
#include "Visitor.h"

class DrawVisitor : public Visitor
{
    glm::mat4 modelview_;
    glm::mat4 projection_;
    Node *target_;
    bool done_;

public:
    DrawVisitor(Node *nodetodraw, glm::mat4 projection);

    void visit(Scene& n) override;
    void visit(Node& n) override;
    void visit(Group& n) override;
    void visit(Switch& n) override;
    void visit(Animation& n) override;
    void visit(Primitive& n) override;
    void visit(Surface& n) override;
    void visit(ImageSurface& n) override;
    void visit(MediaSurface& n) override;
    void visit(FrameBufferSurface& n) override;
    void visit(LineStrip& n) override;
    void visit(LineSquare&) override;
    void visit(LineCircle& n) override;
    void visit(Mesh& n) override;
    void visit(Frame& n) override;

    void visit(MediaPlayer& n) override;
    void visit(Shader& n) override;
    void visit(ImageShader& n) override;
    void visit(ImageProcessingShader& n) override;

    void visit (Source& s) override;
    void visit (MediaSource& s) override;
};

#endif // DRAWVISITOR_H
