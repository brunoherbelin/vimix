#include "UpdateCallback.h"

#include "defines.h"
#include "Scene.h"
#include "Log.h"

#include <glm/gtc/type_ptr.hpp>

UpdateCallback::UpdateCallback() : enabled_(true), finished_(false)
{

}

void MoveToCenterCallback::update(Node *n, float dt)
{

    // set start position on first run or upon call of reset()
    if (!initialized_){
        initial_position_ = n->translation_;
        initialized_ = true;
    }

    // calculate amplitude of movement
    progress_ += dt / duration_;

    // perform movement : translation to the center (0, 0)
    n->translation_.x = initial_position_.x - progress_ * initial_position_.x;
    n->translation_.y = initial_position_.y - progress_ * initial_position_.y;

    // end of movement
    if ( progress_ > 1.f ) {
        n->translation_.x = 0.f;
        n->translation_.y = 0.f;
        finished_ = true;
    }
}

void BounceScaleCallback::update(Node *n, float dt)
{
    // set start scale on first run or upon call of reset()
    if (!initialized_){
        initial_scale_ = n->scale_;
        initialized_ = true;
    }

    // calculate amplitude of movement
    progress_ += dt / duration_;

    n->scale_.x = initial_scale_.x + (initial_scale_.x * 0.05f) * sin(M_PI * progress_);
    n->scale_.y = initial_scale_.y + (initial_scale_.y * 0.05f) * sin(M_PI * progress_);

    // end of movement
    if ( progress_ > 1.f) {
        n->scale_ = initial_scale_;
        finished_ = true;
    }
}
