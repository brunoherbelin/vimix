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
    void visit(MediaSurface& n) override;
    void visit(FrameBufferSurface& n) override;

    // Elements with attributes
    void visit(MediaPlayer& n) override;
    void visit(Shader& n) override;
    void visit(ImageShader& n) override;
    void visit(ImageProcessingShader& n) override;
    void visit (Source& s) override;
    void visit (MediaSource& s) override;
    void visit (SessionSource& s) override;
    void visit (RenderSource& s) override;
    void visit (CloneSource& s) override;
    void visit (PatternSource& s) override;
    void visit (DeviceSource& s) override;
    void visit (NetworkSource& s) override;
};

#endif // IMGUIVISITOR_H
