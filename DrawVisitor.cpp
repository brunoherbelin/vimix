
#include <glm/gtc/matrix_transform.hpp>

#include "defines.h"
#include "DrawVisitor.h"
#include "Scene.h"


DrawVisitor::DrawVisitor(Node *nodetodraw, glm::mat4 projection)
{
    target_ = nodetodraw;
    modelview_ = glm::identity<glm::mat4>();
    projection_ = projection;
    done_ = false;
    num_duplicat_ = 1;
    transform_duplicat_ = glm::identity<glm::mat4>();
}


void DrawVisitor::loop(int num, glm::mat4 transform)
{
    num_duplicat_ = CLAMP(num, 1, 10000);
    transform_duplicat_ = transform;
}

void DrawVisitor::visit(Node &n)
{
    // draw the target
    if ( target_ && n.id() == target_->id()) {

        for (int i = 0; i < num_duplicat_; ++i) {
            // draw multiple copies if requested
            n.draw(modelview_, projection_);
            modelview_ *= transform_duplicat_;
        }

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
        if ( (*node)->visible_ )
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
