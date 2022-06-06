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
    std::string sessionFilePath_;

    static void saveConfig(tinyxml2::XMLDocument *doc, Session *session);
    static void saveSnapshots(tinyxml2::XMLDocument *doc, Session *session);
    static void saveNotes(tinyxml2::XMLDocument *doc, Session *session);
    static void savePlayGroups(tinyxml2::XMLDocument *doc, Session *session);
    static void saveInputCallbacks(tinyxml2::XMLDocument *doc, Session *session);

public:
    SessionVisitor(tinyxml2::XMLDocument *doc = nullptr,
                   tinyxml2::XMLElement *root = nullptr,
                   bool recursive = false);

    inline void setRoot(tinyxml2::XMLElement *root) { xmlCurrent_ = root; }

    static bool saveSession(const std::string& filename, Session *session);

    static std::string getClipboard(const SourceList &list);
    static std::string getClipboard(Source * const s);
    static std::string getClipboard(ImageProcessingShader * const s);

    // Elements of Scene
    void visit (Scene& n) override;
    void visit (Node& n) override;
    void visit (Group& n) override;
    void visit (Switch& n) override;
    void visit (Primitive& n) override;
    void visit (Surface&) override;
    void visit (ImageSurface& n) override;
    void visit (FrameBufferSurface&) override;
    void visit (LineStrip& n) override;
    void visit (LineSquare&) override;
    void visit (Mesh& n) override;
    void visit (Frame& n) override;

    // Elements with attributes
    void visit (MediaPlayer& n) override;
    void visit (Shader& n) override;
    void visit (ImageShader& n) override;
    void visit (MaskShader& n) override;
    void visit (ImageProcessingShader& n) override;

    // Sources
    void visit (Source& s) override;
    void visit (MediaSource& s) override;
    void visit (SessionFileSource& s) override;
    void visit (SessionGroupSource& s) override;
    void visit (RenderSource&) override;
    void visit (PatternSource& s) override;
    void visit (DeviceSource& s) override;
    void visit (NetworkSource& s) override;
    void visit (MixingGroup& s) override;
    void visit (MultiFileSource& s) override;
    void visit (GenericStreamSource& s) override;
    void visit (SrtReceiverSource& s) override;

    void visit (CloneSource& s) override;
    void visit (FrameBufferFilter&) override;
    void visit (DelayFilter&) override;
    void visit (ResampleFilter&) override;
    void visit (BlurFilter&) override;
    void visit (SharpenFilter&) override;
    void visit (EdgeFilter&) override;
    void visit (AlphaFilter&) override;
    void visit (ImageFilter&) override;

    // callbacks
    void visit (SourceCallback&) override;
    void visit (SetAlpha&) override;
    void visit (SetDepth&) override;
    void visit (SetGeometry&) override;
    void visit (Loom&) override;
    void visit (Grab&) override;
    void visit (Resize&) override;
    void visit (Turn&) override;
    void visit (Play&) override;

    static tinyxml2::XMLElement *NodeToXML(const Node &n, tinyxml2::XMLDocument *doc);
    static tinyxml2::XMLElement *ImageToXML(const FrameBufferImage *img, tinyxml2::XMLDocument *doc);
};

#endif // XMLVISITOR_H
