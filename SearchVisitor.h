#ifndef SEARCHVISITOR_H
#define SEARCHVISITOR_H

#include "Visitor.h"

class SearchVisitor: public Visitor
{
    Node *node_;
    bool found_;

public:
    SearchVisitor(Node *node);
    inline bool found() const { return found_; }
    inline Node *node() const { return found_ ? node_ : nullptr; }

    // Elements of Scene
    void visit(Scene& n);
    void visit(Node& n);
    void visit(Primitive&) {}
    void visit(Group& n);
    void visit(Switch& n);

};

#endif // SEARCHVISITOR_H
