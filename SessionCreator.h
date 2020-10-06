#ifndef SESSIONCREATOR_H
#define SESSIONCREATOR_H

#include <list>

#include "Visitor.h"
#include <tinyxml2.h>

class Session;


class SessionLoader : public Visitor {

public:
    SessionLoader(Session *session);
    inline Session *session() const { return session_; }

    void load(tinyxml2::XMLElement *sessionNode);
    inline std::list<uint64_t> getIdList() const { return sources_id_; }

    // Elements of Scene
    void visit(Node& n) override;

    void visit(Scene& n) override {}
    void visit(Group& n) override {}
    void visit(Switch& n) override {}
    void visit(Primitive& n) override {}
    void visit(Surface& n) override {}
    void visit(ImageSurface& n) override {}
    void visit(MediaSurface& n) override {}
    void visit(FrameBufferSurface& n) override {}
    void visit(LineStrip& n) override {}
    void visit(LineSquare&) override {}
    void visit(LineCircle& n) override {}
    void visit(Mesh& n) override {}

    // Elements with attributes
    void visit(MediaPlayer& n) override;
    void visit(Shader& n) override;
    void visit(ImageShader& n) override;
    void visit(ImageProcessingShader& n) override;

    // Sources
    void visit (Source& s) override;
    void visit (MediaSource& s) override;
    void visit (SessionSource& s) override;
    void visit (PatternSource& s) override;
    void visit (DeviceSource& s) override;

protected:
    tinyxml2::XMLElement *xmlCurrent_;
    Session *session_;
    std::list<uint64_t> sources_id_;

    static void XMLToNode(tinyxml2::XMLElement *xml, Node &n);
};

class SessionCreator : public SessionLoader {

    tinyxml2::XMLDocument xmlDoc_;

    void loadConfig(tinyxml2::XMLElement *viewsNode);

public:
    SessionCreator();

    void load(const std::string& filename);

    static std::string info(const std::string& filename);
};

#endif // SESSIONCREATOR_H
