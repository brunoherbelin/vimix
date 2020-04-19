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
    void visit(Scene& n);
    void visit(Node& n);
    void visit(Group& n);
    void visit(Switch& n);
    void visit(Animation& n);
    void visit(Primitive& n);
    void visit(ImageSurface& n);
    void visit(MediaSurface& n);
    void visit(FrameBufferSurface& n);
    void visit(LineStrip& n);
    void visit(LineSquare&);
    void visit(LineCircle& n);
    void visit(Mesh& n);

    // Elements with attributes
    void visit(MediaPlayer& n);
    void visit(Shader& n);
    void visit(ImageShader& n);
};

#endif // XMLVISITOR_H
