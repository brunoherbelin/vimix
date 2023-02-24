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

#include <glm/gtc/type_ptr.hpp>

#include "Scene.h"

#include "UpdateCallback.h"


UpdateCallback::UpdateCallback() : finished_(false), initialized_(false)
{

}

CopyCallback::CopyCallback(Node *target) : UpdateCallback(), target_(target)
{

}

void CopyCallback::update(Node *n, float)
{
    n->copyTransform(target_);
    finished_ = true;
}


MoveToCallback::MoveToCallback(glm::vec3 target, float duration) : UpdateCallback(),
    duration_(duration), progress_(0.f), target_(target)
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

RotateToCallback::RotateToCallback(float target, float duration) : UpdateCallback(),
    duration_(duration), progress_(0.f), startingangle_(0.f), target_(target)
{

}

void RotateToCallback::update(Node *n, float dt)
{
    // set start position on first run or upon call of reset()
    if (!initialized_){
        startingangle_ = n->rotation_.z;
        initialized_ = true;
        // TODO select direction for shorter animation (CW or CCW)
        //            Log::Info("starting angle %.2f", startingangle_);
        //            Log::Info("target_        %.2f", target_);
    }

    // calculate amplitude of movement
    progress_ += dt / duration_;

    // perform movement
    n->rotation_.z = startingangle_ + progress_ * (target_ - startingangle_);

    // end of movement
    if ( progress_ > 1.f ) {
        n->rotation_.z = target_;
        finished_ = true;
    }
}

BounceScaleCallback::BounceScaleCallback(float scale) : UpdateCallback(),
    duration_(100.f), scale_(scale), progress_(0.f)
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

    n->scale_.x = initial_scale_.x + (initial_scale_.x * scale_) * sin(M_PI * progress_);
    n->scale_.y = initial_scale_.y + (initial_scale_.y * scale_) * sin(M_PI * progress_);

    // end of movement
    if ( progress_ > 1.f) {
        n->scale_ = initial_scale_;
        finished_ = true;
    }
}

InfiniteGlowCallback::InfiniteGlowCallback(float amplitude) : UpdateCallback(),
    amplitude_(amplitude), time_(0.f)
{

}

void InfiniteGlowCallback::update(Node *n, float dt)
{
    // set start scale on first run
    if (!initialized_){
        initial_scale_ = n->scale_;
        initialized_ = true;
    }

    time_ += dt / 600.f;

    n->scale_.x = initial_scale_.x + amplitude_ * sin(M_PI * time_);
    n->scale_.y = initial_scale_.y + amplitude_ * sin(M_PI * time_);

}
