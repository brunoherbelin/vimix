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

#include <algorithm>

#include "Log.h"
#include "Scene.h"
#include "GarbageVisitor.h"


GarbageVisitor::GarbageVisitor(Node *nodetocollect) : Visitor()
{
    targets_.push_front(nodetocollect);
    current_ = nullptr;
    found_ = false;
}

GarbageVisitor::GarbageVisitor(Source *sourcetocollect) : Visitor()
{
    targets_.push_front(sourcetocollect->group(View::MIXING));
    targets_.push_front(sourcetocollect->group(View::GEOMETRY));
    targets_.push_front(sourcetocollect->group(View::RENDERING));
    current_ = nullptr;
    found_ = false;
}


void GarbageVisitor::visit(Node &n)
{
    // found the target
//    if (n.id() == target_->id())
//    if ( &n == target_ )
    if ( std::count(targets_.begin(), targets_.end(), &n) )
    {
        // end recursive
        found_ = true;

        // take the node out of the Tree
        if (current_)
            current_->detach(&n);

    }

}

GarbageVisitor::~GarbageVisitor()
{
    // actually delete the Node

}


void GarbageVisitor::visit(Group &n)
{
    // no need to go further if the node was found (or is this group)
    if (found_)
        return;

    // current group
    current_ = &n;

    // loop over members of a group
    // and stop when found
    for (NodeSet::iterator node = n.begin(); !found_ && node != n.end(); ++node) {
        // visit the child node
        (*node)->accept(*this);
        // un-stack recursive browsing
        current_ = &n;
    }

}

void GarbageVisitor::visit(Scene &n)
{
    n.root()->accept(*this);
}

void GarbageVisitor::visit(Switch &n)
{

}

void GarbageVisitor::visit(Primitive &n)
{

}

