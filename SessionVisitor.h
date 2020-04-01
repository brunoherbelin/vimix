#ifndef XMLVISITOR_H
#define XMLVISITOR_H

#include "Visitor.h"
#include "tinyxml2Toolkit.h"

class SessionVisitor : public Visitor {

    std::string filename_;
    tinyxml2::XMLDocument *xmlDoc_;
    tinyxml2::XMLElement *xmlCurrent_;

public:
    SessionVisitor(std::string filename);

    // Elements of Scene
    void visit(Node& n) override;
    void visit(Group& n) override;
    void visit(Switch& n) override;
    void visit(Primitive& n) override;
    void visit(Scene& n) override;
    void visit(TexturedRectangle& n) override;
    void visit(MediaRectangle& n) override;
    void visit(LineStrip& n) override;

    // Elements with attributes
    void visit(MediaPlayer& n) override;
    void visit(Shader& n) override;
    void visit(ImageShader& n) override;
};

#endif // XMLVISITOR_H
