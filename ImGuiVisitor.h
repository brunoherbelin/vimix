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
    void visit(MediaPlayer& n);
    void visit(Shader& n);
    void visit(ImageShader& n);
};

#endif // IMGUIVISITOR_H
