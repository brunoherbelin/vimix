#ifndef GARBAGEVISITOR_H
#define GARBAGEVISITOR_H

#include <list>

#include "Source.h"
#include "Visitor.h"


class GarbageVisitor : public Visitor {

    Group *current_;
    std::list<Node *> targets_;
    bool found_;

public:
    GarbageVisitor(Source *sourcetocollect);
    GarbageVisitor(Node *nodetocollect);
    ~GarbageVisitor();

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

#endif // GARBAGEVISITOR_H
