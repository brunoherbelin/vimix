#include "GlmToolkit.h"
#include "Scene.h"
#include "Source.h"

#include "CopyVisitor.h"

Node *CopyVisitor::deepCopy(Node *node)
{
    CopyVisitor cv;
    node->accept(cv);

    return cv.current_;
}

CopyVisitor::CopyVisitor() : current_(nullptr)
{

}

void CopyVisitor::visit(Node &n)
{
}

void CopyVisitor::visit(Group &n)
{
    Group *here = new Group;

    // node
    current_ = here;
    current_->copyTransform(&n);
    current_->visible_ = n.visible_;

    // loop
    for (NodeSet::iterator node = n.begin(); node != n.end(); ++node) {
        (*node)->accept(*this);

        here->attach( current_ );

        current_ = here;
    }

}

void CopyVisitor::visit(Switch &n)
{
    Switch *here = new Switch;

    // node
    current_ = here;
    current_->copyTransform(&n);
    current_->visible_ = n.visible_;

    // switch properties
    here->setActive( n.active() );

    // loop
    for (uint i = 0; i < n.numChildren(); ++i) {
        n.child(i)->accept(*this);

        here->attach( current_ );

        current_ = here;
    }

}

void CopyVisitor::visit(Scene &n)
{
    Scene *here = new Scene;

    current_ = here->root();
    n.root()->accept(*this);
}


void CopyVisitor::visit(Primitive &n)
{
    Primitive *here = new Primitive;

    // node
    current_ = here;
    current_->copyTransform(&n);
    current_->visible_ = n.visible_;

}
