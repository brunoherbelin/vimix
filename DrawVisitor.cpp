
#include <glm/gtc/matrix_transform.hpp>

#include "DrawVisitor.h"
#include "Scene.h"


DrawVisitor::DrawVisitor(Node *nodetodraw, glm::mat4 projection)
{
    target_ = nodetodraw;
    modelview_ = glm::identity<glm::mat4>();
    projection_ = projection;
    done_ = false;
}


void DrawVisitor::visit(Node &n)
{
    // draw the target
    if ( n.id() == target_->id()) {
        n.draw(modelview_, projection_);
        done_ = true;
    }

    if (done_) return;

    // update transform
    modelview_ *= n.transform_;
}


void DrawVisitor::visit(Group &n)
{
    // no need to traverse deeper if this node was drawn already
    if (done_) return;

    // traverse children
    glm::mat4 mv = modelview_;
    for (NodeSet::iterator node = n.begin(); !done_ && node != n.end(); node++) {
        (*node)->accept(*this);
        modelview_ = mv;
    }
}

void DrawVisitor::visit(Scene &n)
{
    n.root()->accept(*this);
}

void DrawVisitor::visit(Switch &n)
{
    glm::mat4 mv = modelview_;
    n.activeChild()->accept(*this);
    modelview_ = mv;
}

void DrawVisitor::visit(Primitive &n)
{

}

void DrawVisitor::visit(Surface &n)
{

}

void DrawVisitor::visit(ImageSurface &n)
{

}

void DrawVisitor::visit(FrameBufferSurface &n)
{

}

void DrawVisitor::visit(MediaSurface &n)
{

}

void DrawVisitor::visit(MediaPlayer &n)
{

}

void DrawVisitor::visit(Shader &n)
{

}

void DrawVisitor::visit(ImageShader &n)
{

}

void DrawVisitor::visit(ImageProcessingShader &n)
{

}

void DrawVisitor::visit(LineStrip &n)
{

}

void DrawVisitor::visit(LineSquare &)
{

}

void DrawVisitor::visit(LineCircle &n)
{

}

void DrawVisitor::visit(Mesh &n)
{

}

void DrawVisitor::visit(Frame &n)
{

}


void DrawVisitor::visit (Source& s)
{

}

void DrawVisitor::visit (MediaSource& s)
{

}
