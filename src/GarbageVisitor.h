#ifndef GARBAGEVISITOR_H
#define GARBAGEVISITOR_H

#include <list>

#include "Source.h"
#include "Visitor.h"


class GarbageVisitor : public Visitor
{
    Group *current_;
    std::list<Node *> targets_;
    bool found_;

public:
    GarbageVisitor(Source *sourcetocollect);
    GarbageVisitor(Node *nodetocollect);
    ~GarbageVisitor();

    void visit(Scene& n) override;
    void visit(Node& n) override;
    void visit(Group& n) override;
    void visit(Switch& n) override;
    void visit(Primitive& n) override;

};

#endif // GARBAGEVISITOR_H
