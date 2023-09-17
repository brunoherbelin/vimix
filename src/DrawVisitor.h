#ifndef DRAWVISITOR_H
#define DRAWVISITOR_H

#include <vector>
#include <glm/glm.hpp>
#include "Visitor.h"

///
/// \brief The DrawVisitor is typically called on a scene.
/// It traverses its root and draws only the nodes
/// listed in the constructor. The nodes listed must already
/// be in the hierarchy of the scene
///
class DrawVisitor : public Visitor
{
    glm::mat4 modelview_;
    glm::mat4 projection_;
    std::vector<Node *> targets_;
    bool force_;
    int num_duplicat_;
    glm::mat4 transform_duplicat_;

public:
    DrawVisitor(Node *nodetodraw, glm::mat4 projection, bool force = false);
    DrawVisitor(const std::vector<Node *> &nodestodraw, glm::mat4 projection, bool force = false);

    void loop(int num, glm::mat4 transform);

    void visit(Scene& n) override;
    void visit(Node& n) override;
    void visit(Group& n) override;
    void visit(Switch& n) override;
    void visit(Primitive& ) override {}
};


///
/// \brief The ColorVisitor changes the colors of
/// all nodes that can draw (e.g. primitive, decorations)
/// to the given color
///
class ColorVisitor : public Visitor
{
    glm::vec4 color_;

public:
    ColorVisitor(glm::vec4 color) : color_(color) {}

    void visit(Node&) override {}
    void visit(Scene& n) override;
    void visit(Group& n) override;
    void visit(Switch& n) override;

    void visit(Primitive& ) override;
    void visit(Frame& ) override;
    void visit(Handles& ) override;
    void visit(Symbol& ) override;
    void visit(Disk& ) override;
    void visit(Character& ) override;
};

///
/// \brief The VisibleVisitor changes the visible flag of
/// all nodes to the given value
///
class VisibleVisitor : public Visitor
{
    bool visible_;

public:
    VisibleVisitor(bool visible) : visible_(visible) {}

    void visit(Scene& n) override;
    void visit(Node& n) override;
    void visit(Group& n) override;
    void visit(Switch& n) override;
};

#endif // DRAWVISITOR_H
