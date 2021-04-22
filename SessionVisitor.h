#ifndef XMLVISITOR_H
#define XMLVISITOR_H

#include "Visitor.h"
#include "tinyxml2Toolkit.h"
#include "SourceList.h"

class Session;
class FrameBufferImage;

class SessionVisitor : public Visitor {

    bool recursive_;
    tinyxml2::XMLDocument *xmlDoc_;
    tinyxml2::XMLElement *xmlCurrent_;

public:
    SessionVisitor(tinyxml2::XMLDocument *doc = nullptr,
                   tinyxml2::XMLElement *root = nullptr,
                   bool recursive = false);

    inline void setRoot(tinyxml2::XMLElement *root) { xmlCurrent_ = root; }

    static bool saveSession(const std::string& filename, Session *session);

    static std::string getClipboard(SourceList list);
    static std::string getClipboard(Source *s);
    static std::string getClipboard(ImageProcessingShader *s);

    // Elements of Scene
    void visit(Scene& n) override;
    void visit(Node& n) override;
    void visit(Group& n) override;
    void visit(Switch& n) override;
    void visit(Primitive& n) override;
    void visit(Surface&) override;
    void visit(ImageSurface& n) override;
    void visit(MediaSurface& n) override;
    void visit(FrameBufferSurface&) override;
    void visit(LineStrip& n) override;
    void visit(LineSquare&) override;
    void visit(Mesh& n) override;
    void visit(Frame& n) override;

    // Elements with attributes
    void visit(MediaPlayer& n) override;
    void visit(Shader& n) override;
    void visit(ImageShader& n) override;
    void visit(MaskShader& n) override;
    void visit(ImageProcessingShader& n) override;

    // Sources
    void visit (Source& s) override;
    void visit (MediaSource& s) override;
    void visit (SessionFileSource& s) override;
    void visit (SessionGroupSource& s) override;
    void visit (RenderSource&) override;
    void visit (CloneSource& s) override;
    void visit (PatternSource& s) override;
    void visit (DeviceSource& s) override;
    void visit (NetworkSource& s) override;
    void visit (MixingGroup& s) override;

protected:
    static tinyxml2::XMLElement *NodeToXML(Node &n, tinyxml2::XMLDocument *doc);
    static tinyxml2::XMLElement *ImageToXML(FrameBufferImage *img, tinyxml2::XMLDocument *doc);
};

#endif // XMLVISITOR_H
