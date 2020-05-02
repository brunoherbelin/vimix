#ifndef SESSIONCREATOR_H
#define SESSIONCREATOR_H

#include "Visitor.h"
#include "tinyxml2Toolkit.h"

class Session;

class SessionCreator : public Visitor {

    tinyxml2::XMLElement *xmlCurrent_;
    Session *session_;

public:
    SessionCreator(tinyxml2::XMLElement *root);

    inline Session *session() const { return session_; }

    // Elements of Scene
    void visit(Node& n) override;

    void visit(Scene& n) override {}
    void visit(Group& n) override {}
    void visit(Switch& n) override {}
    void visit(Animation& n) override {}
    void visit(Primitive& n) override {}
    void visit(Surface& n) override {}
    void visit(ImageSurface& n) override {}
    void visit(MediaSurface& n) override {}
    void visit(FrameBufferSurface& n) override {}
    void visit(LineStrip& n) override {}
    void visit(LineSquare&) override {}
    void visit(LineCircle& n) override {}
    void visit(Mesh& n) override {}
    void visit(Frame& n) override {}

    // Elements with attributes
    void visit(MediaPlayer& n) override;
    void visit(Shader& n) override;
    void visit(ImageShader& n) override;
    void visit(ImageProcessingShader& n) override;

    void visit (Source& s) override;
    void visit (MediaSource& s) override;

    static void XMLToNode(tinyxml2::XMLElement *xml, Node &n);
};


#endif // SESSIONCREATOR_H
