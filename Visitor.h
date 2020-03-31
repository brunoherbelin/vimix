#ifndef VISITOR_H
#define VISITOR_H

#include <string>

namespace tinyxml2 {
class XMLDocument;
class XMLElement;
}

// Forward declare different kind of Node
class Node;
class Primitive;
class Group;
class Scene;
class TexturedRectangle;
class MediaRectangle;
class LineStrip;

// Declares the interface for the visitors
class Visitor {

public:
    // Declare overloads for each kind of Node to visit
    virtual void visit(Node& n) = 0;
    virtual void visit(Primitive& n) = 0;
    virtual void visit(Group& n) = 0;
    virtual void visit(TexturedRectangle& n) = 0;
    virtual void visit(MediaRectangle& n) = 0;
    virtual void visit(LineStrip& n) = 0;
    virtual void visit(Scene& n) = 0;
};

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

#endif // VISITOR_H
