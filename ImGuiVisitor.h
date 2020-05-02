#ifndef IMGUIVISITOR_H
#define IMGUIVISITOR_H

#include "Visitor.h"

class ImGuiVisitor: public Visitor
{
public:
    ImGuiVisitor();

    // Elements of Scene
    void visit(Scene& n) override;
    void visit(Node& n) override;
    void visit(Group& n) override;
    void visit(Switch& n) override;
    void visit(Animation& n) override;
    void visit(Primitive& n) override;
    void visit(Surface&) override {}
    void visit(ImageSurface&) override {}
    void visit(MediaSurface& n) override;
    void visit(FrameBufferSurface& n) override;
    void visit(LineStrip&) override {}
    void visit(LineSquare&) override {}
    void visit(LineCircle&) override {}
    void visit(Mesh&) override {}
    void visit(Frame&) override {}

    // Elements with attributes
    void visit(MediaPlayer& n) override;
    void visit(Shader& n) override;
    void visit(ImageShader& n) override;
    void visit(ImageProcessingShader& n) override;
};

#endif // IMGUIVISITOR_H
