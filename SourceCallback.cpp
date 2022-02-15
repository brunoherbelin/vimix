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

#include "defines.h"
#include "Source.h"
#include "Visitor.h"
#include "Log.h"

#include "SourceCallback.h"


SourceCallback *SourceCallback::create(CallbackType type)
{
    SourceCallback *loadedcallback = nullptr;

    switch (type) {
    case SourceCallback::CALLBACK_ALPHA:
        loadedcallback = new SetAlpha;
        break;
    case SourceCallback::CALLBACK_LOOM:
        loadedcallback = new Loom;
        break;
    case SourceCallback::CALLBACK_DEPTH:
        loadedcallback = new SetDepth;
        break;
    case SourceCallback::CALLBACK_GRAB:
        loadedcallback = new Grab;
        break;
    case SourceCallback::CALLBACK_RESIZE:
        loadedcallback = new Resize;
        break;
    case SourceCallback::CALLBACK_TURN:
        loadedcallback = new Turn;
        break;
    case SourceCallback::CALLBACK_PLAY:
        loadedcallback = new Play;
        break;
    case SourceCallback::CALLBACK_REPLAY:
        loadedcallback = new RePlay;
        break;
    case SourceCallback::CALLBACK_RESETGEO:
        loadedcallback = new ResetGeometry;
        break;
    case SourceCallback::CALLBACK_LOCK:
        loadedcallback = new Lock;
        break;
    default:
        break;
    }

    return loadedcallback;
}


bool SourceCallback::overlap( SourceCallback *a,  SourceCallback *b)
{
    bool ret = false;

    if (a->type() == b->type()) {

        // same type means overlap
        ret = true;

        // but there are some exceptions..
        switch (a->type()) {
        case SourceCallback::CALLBACK_GRAB:
        {
            const Grab *_a = static_cast<Grab*>(a);
            const Grab *_b = static_cast<Grab*>(b);
            // there is no overlap if the X or Y of a vector is zero
            if ( ABS(_a->value().x) < EPSILON || ABS(_b->value().x) < EPSILON )
                ret = false;
            else if ( ABS(_a->value().y) < EPSILON || ABS(_b->value().y) < EPSILON )
                ret = false;
        }
            break;
        case SourceCallback::CALLBACK_RESIZE:
        {
            const Resize *_a = static_cast<Resize*>(a);
            const Resize *_b = static_cast<Resize*>(b);
            if ( ABS(_a->value().x) < EPSILON || ABS(_b->value().x) < EPSILON )
                ret = false;
            else if ( ABS(_a->value().y) < EPSILON || ABS(_b->value().y) < EPSILON )
                ret = false;
        }
            break;
        default:
            break;
        }

    }

    return ret;
}

SourceCallback::SourceCallback(): finished_(false), initialized_(false)
{
}

void SourceCallback::accept(Visitor& v)
{
    v.visit(*this);
}

void ResetGeometry::update(Source *s, float)
{
    s->group(View::GEOMETRY)->scale_ = glm::vec3(1.f);
    s->group(View::GEOMETRY)->rotation_.z = 0;
    s->group(View::GEOMETRY)->crop_ = glm::vec3(1.f);
    s->group(View::GEOMETRY)->translation_ = glm::vec3(0.f);
    s->touch();
    finished_ = true;
}

SourceCallback *ResetGeometry::clone() const
{
    return new ResetGeometry;
}

SetAlpha::SetAlpha() : SourceCallback(), duration_(0.f), progress_(0.f), alpha_(0.f)
{
    start_  = glm::vec2();
    target_  = glm::vec2();
}

SetAlpha::SetAlpha(float alpha, float duration) : SetAlpha()
{
    alpha_ = CLAMP(alpha, 0.f, 1.f);
    duration_ = duration;
}

void SetAlpha::update(Source *s, float dt)
{
    if (s && !s->locked()) {
        // set start position on first run or upon call of reset()
        if (!initialized_){
            // initial mixing view position
            start_ = glm::vec2(s->group(View::MIXING)->translation_);

            // step in diagonal by default
            glm::vec2 step = glm::normalize(glm::vec2(1.f, 1.f));
            // step in direction of source translation if possible
            if ( glm::length(start_) > DELTA_ALPHA)
                step = glm::normalize(start_);
            //
            // target mixing view position
            //
            // special case Alpha = 0
            if (alpha_ < DELTA_ALPHA) {
                target_ = step;
            }
            // special case Alpha = 1
            else if (alpha_ > 1.f - DELTA_ALPHA) {
                target_ = step * 0.005f;
            }
            // general case
            else {
                // converge to reduce the difference of alpha using dichotomic algorithm
                target_ = start_;
                float delta = 1.f;
                do {
                    target_ += step * (delta / 2.f);
                    delta = SourceCore::alphaFromCordinates(target_.x, target_.y) - alpha_;
                } while (glm::abs(delta) > DELTA_ALPHA);
            }

            initialized_ = true;
        }

        // time passed
        progress_ += dt;

        // perform movement
        if ( ABS(duration_) > 0.f)
            s->group(View::MIXING)->translation_ = glm::vec3(start_ + (progress_/duration_) * target_, s->group(View::MIXING)->translation_.z);

        // time-out
        if ( progress_ > duration_ ) {
            // apply depth to target
            s->group(View::MIXING)->translation_ = glm::vec3(target_, s->group(View::MIXING)->translation_.z);
            // done
            finished_ = true;
        }

    }
    else
        finished_ = true;
}

void SetAlpha::multiply (float factor)
{
    alpha_ *= factor;
}

SourceCallback *SetAlpha::clone() const
{
    return new SetAlpha(alpha_);
}

SourceCallback *SetAlpha::reverse(Source *s) const
{
    return new SetAlpha(s->alpha());
}

void SetAlpha::accept(Visitor& v)
{
    SourceCallback::accept(v);
    v.visit(*this);
}

Lock::Lock() : SourceCallback(), lock_(true)
{
}

Lock::Lock(bool on) : Lock()
{
    lock_ = on;
}

void Lock::update(Source *s, float)
{
    if (s)
        s->setLocked(lock_);

    finished_ = true;
}

SourceCallback *Lock::clone() const
{
    return new Lock(lock_);
}

Loom::Loom() : SourceCallback(), speed_(0), duration_(0), progress_(0.f)
{
    pos_  = glm::vec2();
    step_ = glm::normalize(glm::vec2(1.f, 1.f));   // step in diagonal by default
}

Loom::Loom(float d, float duration) : Loom()
{
    speed_ = d;
    duration_ = duration;
    pos_  = glm::vec2();
    step_ = glm::normalize(glm::vec2(1.f, 1.f));   // step in diagonal by default
}

void Loom::update(Source *s, float dt)
{
    if (s && !s->locked()) {
        // reset on first run or upon call of reset()
        if (!initialized_){
            // start animation
            progress_ = 0.f;
            // initial position
            pos_ = glm::vec2(s->group(View::MIXING)->translation_);
            // step in direction of source translation if possible
            if ( glm::length(pos_) > DELTA_ALPHA)
                step_ = glm::normalize(pos_);

            initialized_ = true;
        }

        // time passed
        progress_ += dt;

        // move target by speed vector (in the direction of step_, amplitude of speed * time (in second))
        pos_ -= step_ * ( speed_ * dt * 0.001f );

        // apply alpha if valid in range [0 1]
        float alpha = SourceCore::alphaFromCordinates(pos_.x, pos_.y);
        if ( alpha > DELTA_ALPHA && alpha < 1.0 - DELTA_ALPHA )
            s->group(View::MIXING)->translation_ = glm::vec3(pos_, s->group(View::MIXING)->translation_.z);

        // time-out
        if ( progress_ > duration_ ) {
            // done
            finished_ = true;
        }
    }
    else
        finished_ = true;
}

void Loom::multiply (float factor)
{
    speed_ *= factor;
}

SourceCallback *Loom::clone() const
{
    return new Loom(speed_, duration_);
}

void Loom::accept(Visitor& v)
{
    SourceCallback::accept(v);
    v.visit(*this);
}


SetDepth::SetDepth() : SourceCallback(),
    duration_(0), progress_(0.f), start_(0.f), target_(MIN_DEPTH)
{
}

SetDepth::SetDepth(float target, float duration) : SetDepth()
{
    target_ = CLAMP(target, MIN_DEPTH, MAX_DEPTH);
    duration_ = duration;
}

void SetDepth::update(Source *s, float dt)
{
    if (s && !s->locked()) {
        // set start position on first run or upon call of reset()
        if (!initialized_){
            start_ = s->group(View::LAYER)->translation_.z;
            progress_ = 0.f;
            initialized_ = true;
        }

        // time passed
        progress_ += dt;

        // perform movement
        if ( ABS(duration_) > 0.f)
            s->group(View::LAYER)->translation_.z = start_ + (progress_/duration_) * (target_ - start_);

        // time-out
        if ( progress_ > duration_ ) {
            // apply depth to target
            s->group(View::LAYER)->translation_.z = target_;
            // ensure reordering of view
            ++View::need_deep_update_;
            // done
            finished_ = true;
        }
    }
    else
        finished_ = true;
}

void SetDepth::multiply (float factor)
{
    target_ *= factor;
}

SourceCallback *SetDepth::clone() const
{
    return new SetDepth(target_, duration_);
}

SourceCallback *SetDepth::reverse(Source *s) const
{
    return new SetDepth(s->depth(), duration_);
}

void SetDepth::accept(Visitor& v)
{
    SourceCallback::accept(v);
    v.visit(*this);
}

Play::Play() : SourceCallback(), play_(true)
{
}

Play::Play(bool on) : Play()
{
    play_ = on;
}

void Play::update(Source *s, float)
{
    if (s && s->playing() != play_) {
        // call play function
        s->play(play_);
    }

    finished_ = true;
}

SourceCallback *Play::clone() const
{
    return new Play(play_);
}

SourceCallback *Play::reverse(Source *s) const
{
    return new Play(s->playing());
}

void Play::accept(Visitor& v)
{
    SourceCallback::accept(v);
    v.visit(*this);
}

RePlay::RePlay() : SourceCallback()
{
}

void RePlay::update(Source *s, float)
{
    if (s) {
        // call replay function
        s->replay();
    }

    finished_ = true;
}

SourceCallback *RePlay::clone() const
{
    return new RePlay;
}

Grab::Grab() : SourceCallback(), speed_(glm::vec2(0.f)), duration_(0.f), progress_(0.f)
{
}

Grab::Grab(float dx, float dy, float duration) : Grab()
{
    speed_ = glm::vec2(dx,dy);
    duration_ = duration;
}

void Grab::update(Source *s, float dt)
{
    if (s && !s->locked()) {
        // reset on first run or upon call of reset()
        if (!initialized_){
            // start animation
            progress_ = 0.f;
            // initial position
            start_ = glm::vec2(s->group(View::GEOMETRY)->translation_);
            initialized_ = true;
        }

        // time passed
        progress_ += dt;

        // move target by speed vector * time (in second)
        glm::vec2 pos = start_ + speed_ * ( dt * 0.001f);
        s->group(View::GEOMETRY)->translation_ = glm::vec3(pos, s->group(View::GEOMETRY)->translation_.z);

        // time-out
        if ( progress_ > duration_ ) {
            // done
            finished_ = true;
        }
    }
    else
        finished_ = true;
}

void Grab::multiply (float factor)
{
    speed_ *= factor;
}

SourceCallback *Grab::clone() const
{
    return new Grab(speed_.x, speed_.y, duration_);
}

void Grab::accept(Visitor& v)
{
    SourceCallback::accept(v);
    v.visit(*this);
}

Resize::Resize() : SourceCallback(), speed_(glm::vec2(0.f)), duration_(0.f), progress_(0.f)
{
}

Resize::Resize(float dx, float dy, float duration) : Resize()
{
    speed_ = glm::vec2(dx,dy);
    duration_ = duration;
}

void Resize::update(Source *s, float dt)
{
    if (s && !s->locked()) {
        // reset on first run or upon call of reset()
        if (!initialized_){
            // start animation
            progress_ = 0.f;
            // initial position
            start_ = glm::vec2(s->group(View::GEOMETRY)->scale_);
            initialized_ = true;
        }

        // time passed
        progress_ += dt;

        // move target by speed vector * time (in second)
        glm::vec2 scale = start_ + speed_ * ( dt * 0.001f);
        s->group(View::GEOMETRY)->scale_ = glm::vec3(scale, s->group(View::GEOMETRY)->scale_.z);

        // time-out
        if ( progress_ > duration_ ) {
            // done
            finished_ = true;
        }
    }
    else
        finished_ = true;
}

void Resize::multiply (float factor)
{
    speed_ *= factor;
}

SourceCallback *Resize::clone() const
{
    return new Resize(speed_.x, speed_.y, duration_);
}

void Resize::accept(Visitor& v)
{
    SourceCallback::accept(v);
    v.visit(*this);
}

Turn::Turn() : SourceCallback(), speed_(0.f), duration_(0.f), progress_(0.f)
{
}

Turn::Turn(float da, float duration) : Turn()
{
    speed_ = da;
    duration_ = duration;
}

void Turn::update(Source *s, float dt)
{
    if (s && !s->locked()) {
        // reset on first run or upon call of reset()
        if (!initialized_){
            // start animation
            progress_ = 0.f;
            // initial position
            start_ = s->group(View::GEOMETRY)->rotation_.z;
            initialized_ = true;
        }

        // calculate amplitude of movement
        progress_ += dt;

        // perform movement
        s->group(View::GEOMETRY)->rotation_.z = start_ + speed_ * ( dt * -0.001f / M_PI);

        // timeout
        if ( progress_ > duration_ ) {
            // done
            finished_ = true;
        }
    }
    else
        finished_ = true;
}

void Turn::multiply (float factor)
{
    speed_ *= factor;
}

SourceCallback *Turn::clone() const
{
    return new Turn(speed_, duration_);
}

void Turn::accept(Visitor& v)
{
    SourceCallback::accept(v);
    v.visit(*this);
}
