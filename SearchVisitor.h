#ifndef SEARCHVISITOR_H
#define SEARCHVISITOR_H

#include <list>
#include <string>
#include "Visitor.h"

class Session;

class SearchVisitor: public Visitor
{
    Node *node_;
    bool found_;

public:
    SearchVisitor(Node *node);
    inline bool found() const { return found_; }
    inline Node *node() const { return found_ ? node_ : nullptr; }

    // Elements of Scene
    void visit (Scene& n) override;
    void visit (Node& n) override;
    void visit (Primitive&) override {}
    void visit (Group& n) override;
    void visit (Switch& n) override;

};

class SearchFileVisitor: public Visitor
{
    std::list<std::string> filenames_;

public:
    SearchFileVisitor();
    inline std::list<std::string> filenames() const { return filenames_; }

    // Elements of Scene
    void visit (Scene& n) override;
    void visit (Node& n) override;
    void visit (Primitive&) override {}
    void visit (Group& n) override;
    void visit (Switch& n) override;

    // Sources
    void visit (MediaSource& s) override;
    void visit (SessionFileSource& s) override;

    static std::list<std::string> parse (Session *se);
    static bool find (Session *se, std::string path);
};

#endif // SEARCHVISITOR_H
