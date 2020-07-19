#include "UpdateCallback.h"

#include "defines.h"
#include "Scene.h"
#include "Log.h"

#include <glm/gtc/type_ptr.hpp>

UpdateCallback::UpdateCallback() : enabled_(true), finished_(false)
{

}

MoveToCallback::MoveToCallback(glm::vec3 target, float duration) : UpdateCallback(),
    target_(target), duration_(duration), progress_(0.f), initialized_(false)
{

}

void MoveToCallback::update(Node *n, float dt)
{
    // set start position on first run or upon call of reset()
    if (!initialized_){
        startingpoint_ = n->translation_;
        target_.z = startingpoint_.z; // ignore depth
        initialized_ = true;
    }

    // calculate amplitude of movement
    progress_ += dt / duration_;

    // perform movement
    n->translation_ = startingpoint_ + progress_ * (target_ - startingpoint_);

    // end of movement
    if ( progress_ > 1.f ) {
        n->translation_ = target_;
        finished_ = true;
    }
}

BounceScaleCallback::BounceScaleCallback(float duration) : UpdateCallback(),
    duration_(duration), progress_(0.f), initialized_(false)
{

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
InfiniteGlowCallback::InfiniteGlowCallback(float amplitude) : UpdateCallback(),
    amplitude_(amplitude), time_(0.f), initialized_(false)
{

}

void InfiniteGlowCallback::update(Node *n, float dt)
{
    // set start scale on first run
    if (!initialized_){
        initial_scale_ = n->scale_;
        initialized_ = true;
    }

    time_ += dt / 1000.f;

    n->scale_.x = initial_scale_.x + amplitude_ * sin(M_PI * time_);
    n->scale_.y = initial_scale_.y + amplitude_ * sin(M_PI * time_);

}
