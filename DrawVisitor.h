#ifndef DRAWVISITOR_H
#define DRAWVISITOR_H

#include <vector>

#include <glm/glm.hpp>
#include "Visitor.h"

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
    DrawVisitor(std::vector<Node *> nodestodraw, glm::mat4 projection, bool force = false);

    void loop(int num, glm::mat4 transform);

    void visit(Scene& n) override;
    void visit(Node& n) override;
    void visit(Primitive& n) override;
    void visit(Group& n) override;
    void visit(Switch& n) override;
};

#endif // DRAWVISITOR_H
