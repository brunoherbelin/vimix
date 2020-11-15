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
    for (NodeSet::iterator node = n.begin(); !found_ && node != n.end(); node++) {
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

void GarbageVisitor::visit(Surface &n)
{

}

void GarbageVisitor::visit(ImageSurface &n)
{

}

void GarbageVisitor::visit(FrameBufferSurface &n)
{

}

void GarbageVisitor::visit(MediaSurface &n)
{

}

void GarbageVisitor::visit(MediaPlayer &n)
{

}

void GarbageVisitor::visit(Shader &n)
{

}

void GarbageVisitor::visit(ImageShader &n)
{

}

void GarbageVisitor::visit(ImageProcessingShader &n)
{

}

void GarbageVisitor::visit(LineStrip &n)
{

}

void GarbageVisitor::visit(LineSquare &)
{

}

void GarbageVisitor::visit(LineCircle &n)
{

}

void GarbageVisitor::visit(Mesh &n)
{

}

void GarbageVisitor::visit(Frame &n)
{

}


void GarbageVisitor::visit (Source& s)
{

}

void GarbageVisitor::visit (MediaSource& s)
{

}
