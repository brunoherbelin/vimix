/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2023 Bruno Herbelin <bruno.herbelin@gmail.com>
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

#include "Scene.h"
#include "Primitives.h"

#include "MediaSource.h"
#include "CloneSource.h"
#include "RenderSource.h"
#include "SessionSource.h"
#include "PatternSource.h"
#include "DeviceSource.h"
#include "NetworkSource.h"
#include "SrtReceiverSource.h"
#include "MultiFileSource.h"
#include "Session.h"

#include "CountVisitor.h"


CountVisitor::CountVisitor() : num_source_(0), num_playable_(0)
{
}

void CountVisitor::visit(Node &)
{

}

void CountVisitor::visit(Group &)
{
}

void CountVisitor::visit(Switch &)
{
}

void CountVisitor::visit(Scene &)
{
}

void CountVisitor::visit(Primitive &)
{
}


void CountVisitor::visit(MediaPlayer &)
{

}

void CountVisitor::visit(Stream &)
{

}

void CountVisitor::visit (MediaSource& s)
{
    ++num_source_;
    if (s.playable())
        ++num_playable_;
}

void CountVisitor::visit (SessionFileSource& s)
{
    if (s.session() != nullptr)
        num_source_ += s.session()->numSources();
    else
        ++num_source_;

    if (s.playable())
        ++num_playable_;
}

void CountVisitor::visit (SessionGroupSource& s)
{
    if (s.session() != nullptr)
        num_source_ += s.session()->numSources();
    else
        ++num_source_;

    if (s.playable())
        ++num_playable_;
}

void CountVisitor::visit (RenderSource& )
{
    ++num_source_;
    ++num_playable_;
}

void CountVisitor::visit (CloneSource& )
{
    ++num_source_;
    ++num_playable_;
}

void CountVisitor::visit (PatternSource& s)
{
    ++num_source_;
    if (s.playable())
        ++num_playable_;
}

void CountVisitor::visit (DeviceSource& s)
{
    ++num_source_;
    if (s.playable())
        ++num_playable_;
}

void CountVisitor::visit (NetworkSource& s)
{
    ++num_source_;
    if (s.playable())
        ++num_playable_;
}

void CountVisitor::visit (MultiFileSource& s)
{
    ++num_source_;
    if (s.playable())
        ++num_playable_;
}

void CountVisitor::visit (GenericStreamSource& s)
{
    ++num_source_;
    if (s.playable())
        ++num_playable_;
}

void CountVisitor::visit (SrtReceiverSource& s)
{
    ++num_source_;
    if (s.playable())
        ++num_playable_;
}
