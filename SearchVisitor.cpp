#include "SearchVisitor.h"

#include "Scene.h"

SearchVisitor::SearchVisitor(Node *node) : Visitor(), node_(node), id_(0), found_(false)
{
    id_ = node->id();
}

SearchVisitor::SearchVisitor(int id) : Visitor(), node_(nullptr), id_(id), found_(false)
{

}

void SearchVisitor::visit(Node &n)
{
    if ( n.id() == id_ ){
        found_ = true;
        node_ = &n;
    }
}

void SearchVisitor::visit(Group &n)
{
    for (NodeSet::iterator node = n.begin(); node != n.end(); node++) {
        (*node)->accept(*this);
        if (found_)
            break;
    }
}

void SearchVisitor::visit(Switch &n)
{
    (*n.activeChild())->accept(*this);
}


void SearchVisitor::visit(Scene &n)
{
    // TODO maybe search only forground?
    n.root()->accept(*this);
}
