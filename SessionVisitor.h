#ifndef XMLVISITOR_H
#define XMLVISITOR_H

#include "Visitor.h"
#include "tinyxml2Toolkit.h"

class SessionVisitor : public Visitor {

    bool recursive_;
    tinyxml2::XMLDocument *xmlDoc_;
    tinyxml2::XMLElement *xmlCurrent_;

public:
    SessionVisitor(tinyxml2::XMLDocument *doc = nullptr,
                   tinyxml2::XMLElement *root = nullptr,
                   bool recursive = false);

    inline tinyxml2::XMLDocument *doc() const { return xmlDoc_; }

    // Elements of Scene
    void visit(Scene& n) override;
    void visit(Node& n) override;
    void visit(Group& n) override;
    void visit(Switch& n) override;
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

    static tinyxml2::XMLElement *NodeToXML(Node &n, tinyxml2::XMLDocument *doc);
};

#endif // XMLVISITOR_H
