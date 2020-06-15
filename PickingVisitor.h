#ifndef PICKINGVISITOR_H
#define PICKINGVISITOR_H

#include <glm/glm.hpp>
#include <vector>
#include <utility>

#include "Visitor.h"

class PickingVisitor: public Visitor
{
    std::vector<glm::vec3> points_;
    glm::mat4 modelview_;
    std::vector< std::pair<Node *, glm::vec2> > nodes_;

public:

    PickingVisitor(glm::vec3 coordinates);
    PickingVisitor(glm::vec3 selectionstart, glm::vec3 selection_end);
    std::vector< std::pair<Node *, glm::vec2> > picked() { return nodes_; }

    // Elements of Scene
    void visit(Scene& n) override;
    void visit(Node& n) override;
    void visit(Group& n) override;
    void visit(Switch& n) override;
    void visit(Primitive& n) override;
    void visit(Surface& n) override;
    void visit(Frame& n) override;
    void visit(Handles& n) override;
    void visit(LineSquare&) override;
    void visit(LineCircle& n) override;

};

#endif // PICKINGVISITOR_H
