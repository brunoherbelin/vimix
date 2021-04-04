#include <algorithm>

#include "SearchVisitor.h"

#include "Scene.h"
#include "MediaSource.h"
#include "Session.h"
#include "SessionSource.h"

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

    for (NodeSet::iterator node = n.begin(); node != n.end(); ++node) {
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



SearchFileVisitor::SearchFileVisitor() : Visitor()
{

}

void SearchFileVisitor::visit(Node &n)
{

}

void SearchFileVisitor::visit(Group &n)
{
    for (NodeSet::iterator node = n.begin(); node != n.end(); ++node) {
        (*node)->accept(*this);
    }
}

void SearchFileVisitor::visit(Switch &n)
{
    if (n.numChildren()>0)
        n.activeChild()->accept(*this);
}


void SearchFileVisitor::visit(Scene &n)
{
    // search only in workspace
    n.ws()->accept(*this);
}


void SearchFileVisitor::visit (MediaSource& s)
{
    filenames_.push_back( s.path() );
}

void SearchFileVisitor::visit (SessionFileSource& s)
{

    filenames_.push_back( s.path() );
}

std::list<std::string> SearchFileVisitor::parse (Session *se)
{
    SearchFileVisitor sv;

    for (auto iter = se->begin(); iter != se->end(); iter++){
        (*iter)->accept(sv);
    }

    return sv.filenames();
}

bool SearchFileVisitor::find (Session *se, std::string path)
{
    std::list<std::string> filenames = parse (se);
    return std::find(filenames.begin(), filenames.end(), path) != filenames.end();
}

