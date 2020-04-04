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
    void visit(Primitive& n) override;
    void visit(ImageSurface& n) override;
    void visit(MediaSurface& n) override;
    void visit(LineStrip& n) override;
    void visit(LineSquare& n) override;
    void visit(LineCircle& n) override;

    // Elements with attributes
    void visit(MediaPlayer& n) override;
    void visit(Shader& n) override;
    void visit(ImageShader& n) override;
};

#endif // IMGUIVISITOR_H
