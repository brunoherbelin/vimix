/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2022 Bruno Herbelin <bruno.herbelin@gmail.com>
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

#include "Scene.h"
#include "MediaSource.h"
#include "Session.h"
#include "SessionSource.h"

#include "SearchVisitor.h"

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

void SearchFileVisitor::visit(Node &)
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

