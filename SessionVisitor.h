#ifndef XMLVISITOR_H
#define XMLVISITOR_H

#include "Visitor.h"
#include "tinyxml2Toolkit.h"

class SessionVisitor : public Visitor {

    tinyxml2::XMLDocument *xmlDoc_;
    tinyxml2::XMLElement *xmlCurrent_;
    tinyxml2::XMLElement *xmlRoot_;

public:
    SessionVisitor();
    void save(std::string filename);
    tinyxml2::XMLElement *root() {return xmlRoot_;}

    // Elements of Scene
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

    // Elements with attributes
    void visit(MediaPlayer& n) override;
    void visit(Shader& n) override;
    void visit(ImageShader& n) override;
    void visit(ImageProcessingShader& n) override;
};

#endif // XMLVISITOR_H
