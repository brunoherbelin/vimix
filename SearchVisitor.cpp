#include "SearchVisitor.h"

#include "Scene.h"

SearchVisitor::SearchVisitor(Node *node) : Visitor(), node_(node), found_(false)
{

}


void SearchVisitor::visit(Node &n)
{
    if ( !found_ && node_ == &n ){
        found_ = true;
    }
}

void SearchVisitor::visit(Group &n)
{
    if (found_)
        return;

    for (NodeSet::iterator node = n.begin(); node != n.end(); node++) {
        (*node)->accept(*this);
        if (found_)
            break;
    }
}

void SearchVisitor::visit(Switch &n)
{
    if (n.numChildren()>0)
        n.activeChild()->accept(*this);
}


void SearchVisitor::visit(Scene &n)
{
    // search only in workspace
    n.ws()->accept(*this);
}
