/*
 * This file is part of vimix - Live video mixer
 *
 * **Copyright** (C) 2020-2021 Bruno Herbelin <bruno.herbelin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
**/

#include "Toolkit/GlmToolkit.h"
#include "Scene/Scene.h"
#include "Source/Source.h"

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
