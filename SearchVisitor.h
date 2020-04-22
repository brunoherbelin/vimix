#ifndef SEARCHVISITOR_H
#define SEARCHVISITOR_H

#include "Visitor.h"

class SearchVisitor: public Visitor
{
    Node *node_;
    int id_;
    bool found_;

public:
    SearchVisitor(Node *node);
    SearchVisitor(int id);
    inline bool found() const { return found_; }
    inline Node *node() const { return found_ ? node_ : nullptr; }

    // Elements of Scene
    void visit(Scene& n);
    void visit(Node& n);
    void visit(Group& n);
    void visit(Switch& n);

    // already a Node or a Group
    void visit(Animation&) {}
    void visit(Primitive&) {}
    void visit(Surface&) {}
    void visit(ImageSurface&) {}
    void visit(MediaSurface&) {}
    void visit(FrameBufferSurface&) {}
    void visit(LineStrip&) {}
    void visit(LineSquare&) {}
    void visit(LineCircle&) {}
    void visit(Mesh&) {}

    // not nodes
    void visit(MediaPlayer&) {}
    void visit(Shader&) {}
    void visit(ImageShader&) {}
};

#endif // SEARCHVISITOR_H
