#ifndef COPYVISITOR_H
#define COPYVISITOR_H

#include "Visitor.h"

class SourceCore;

class CopyVisitor : public Visitor
{
    Node *current_;
    CopyVisitor();

public:

    static Node *deepCopy(Node *node);

    void visit(Scene& n) override;
    void visit(Node& n) override;
    void visit(Primitive& n) override;
    void visit(Group& n) override;
    void visit(Switch& n) override;
};

#endif // COPYVISITOR_H
