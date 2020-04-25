#ifndef IMGUIVISITOR_H
#define IMGUIVISITOR_H

#include "Visitor.h"

class ImGuiVisitor: public Visitor
{
public:
    ImGuiVisitor();

    // Elements of Scene
    void visit(Scene& n);
    void visit(Node& n);
    void visit(Group& n);
    void visit(Switch& n);
    void visit(Animation& n);
    void visit(Primitive& n);
    void visit(Surface&) {}
    void visit(ImageSurface&) {}
    void visit(MediaSurface& n);
    void visit(FrameBufferSurface& n);
    void visit(LineStrip&) {}
    void visit(LineSquare&) {}
    void visit(LineCircle&) {}
    void visit(Mesh&) {}

    // Elements with attributes
    void visit(MediaPlayer& n) override;
    void visit(Shader& n) override;
    void visit(ImageShader& n) override;
    void visit(ImageProcessingShader& n) override;
};

#endif // IMGUIVISITOR_H
