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

    Source *cloneOrCreateSource(tinyxml2::XMLElement *sourceNode);

    // Elements of Scene
    void visit (Node& n) override;

    void visit (Scene&) override {}
    void visit (Group&) override {}
    void visit (Switch&) override {}
    void visit (Primitive&) override {}
    void visit (Surface&) override {}
    void visit (ImageSurface&) override {}
    void visit (MediaSurface&) override {}
    void visit (FrameBufferSurface&) override {}
    void visit (LineStrip&) override {}
    void visit (LineSquare&) override {}
    void visit (LineCircle&) override {}
    void visit (Mesh&) override {}

    // Elements with attributes
    void visit (MediaPlayer& n) override;
    void visit (Shader& n) override;
    void visit (ImageShader& n) override;
    void visit (MaskShader& n) override;
    void visit (ImageProcessingShader& n) override;

    // Sources
    void visit (Source& s) override;
    void visit (MediaSource& s) override;
    void visit (SessionSource& s) override;
    void visit (PatternSource& s) override;
    void visit (DeviceSource& s) override;
    void visit (NetworkSource& s) override;

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
