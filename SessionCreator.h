#ifndef SESSIONCREATOR_H
#define SESSIONCREATOR_H

#include <map>
#include <tinyxml2.h>

#include "Visitor.h"
#include "SourceList.h"

class Session;


class SessionLoader : public Visitor {

public:

    SessionLoader(Session *session, int recursion = 0);
    inline Session *session() const { return session_; }

    void load(tinyxml2::XMLElement *sessionNode);
    std::map< uint64_t, Source* > getSources() const;
    std::list< SourceList > getMixingGroups() const;

    Source *createSource(tinyxml2::XMLElement *sourceNode, bool clone_duplicates = true);

    // Elements of Scene
    void visit (Node& n) override;
    void visit (Scene&) override {}
    void visit (Group&) override {}
    void visit (Switch&) override {}
    void visit (Primitive&) override {}

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
    void visit (RenderSource& s) override;
    void visit (PatternSource& s) override;
    void visit (DeviceSource& s) override;
    void visit (NetworkSource& s) override;

protected:
    // result created session
    Session *session_;
    // parsing current xml
    tinyxml2::XMLElement *xmlCurrent_;
    // level of loading recursion
    int recursion_;
    // map of correspondance from xml source id (key) to new source pointer (value)
    std::map< uint64_t, Source* > sources_id_;
    // list of groups (lists of xml source id)
    std::list< SourceIdList > groups_sources_id_;

    static void XMLToNode(tinyxml2::XMLElement *xml, Node &n);
};

class SessionCreator : public SessionLoader {

    tinyxml2::XMLDocument xmlDoc_;

    void loadConfig(tinyxml2::XMLElement *viewsNode);

public:
    SessionCreator(int recursion = 0);

    void load(const std::string& filename);

    static std::string info(const std::string& filename);
};

#endif // SESSIONCREATOR_H
