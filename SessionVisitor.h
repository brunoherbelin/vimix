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

    void visit(Node& n) override;
    void visit(Primitive& n) override;
    void visit(Group& n) override;
    void visit(TexturedRectangle& n) override;
    void visit(MediaRectangle& n) override;
    void visit(LineStrip& n) override;
    void visit(Scene& n) override;

};

#endif // XMLVISITOR_H
