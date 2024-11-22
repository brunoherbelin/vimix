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
#include <fstream>
#include <algorithm>

#include "defines.h"
#include "Source.h"
#include "ImageProcessingShader.h"
#include "CloneSource.h"
#include "ImageFilter.h"
#include "DelayFilter.h"
#include "MediaSource.h"
#include "MediaPlayer.h"
#include "Visitor.h"
#include "Log.h"
#include "Mixer.h"

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
    case SourceCallback::CALLBACK_GEOMETRY:
        loadedcallback = new SetGeometry;
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
    case SourceCallback::CALLBACK_DEPTH:
        loadedcallback = new SetDepth;
        break;
    case SourceCallback::CALLBACK_PLAY:
        loadedcallback = new Play;
        break;
    case SourceCallback::CALLBACK_PLAYSPEED:
        loadedcallback = new PlaySpeed;
        break;
    case SourceCallback::CALLBACK_PLAYFFWD:
        loadedcallback = new PlayFastForward;
        break;
    case SourceCallback::CALLBACK_SEEK:
        loadedcallback = new Seek;
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
    case SourceCallback::CALLBACK_BRIGHTNESS:
        loadedcallback = new SetBrightness;
        break;
    case SourceCallback::CALLBACK_CONTRAST:
        loadedcallback = new SetContrast;
        break;
    case SourceCallback::CALLBACK_SATURATION:
        loadedcallback = new SetSaturation;
        break;
    case SourceCallback::CALLBACK_HUE:
        loadedcallback = new SetHue;
        break;
    case SourceCallback::CALLBACK_THRESHOLD:
        loadedcallback = new SetThreshold;
        break;
    case SourceCallback::CALLBACK_GAMMA:
        loadedcallback = new SetGamma;
        break;
    case SourceCallback::CALLBACK_INVERT:
        loadedcallback = new SetInvert;
        break;
    case SourceCallback::CALLBACK_POSTERIZE:
        loadedcallback = new SetPosterize;
        break;
    case SourceCallback::CALLBACK_FILTER:
        loadedcallback = new SetFilter;
        break;
    default:
        break;
    }

    return loadedcallback;
}

SourceCallback::SourceCallback(): status_(PENDING), delay_(0.f), elapsed_(0.f)
{
}

void SourceCallback::accept(Visitor& v)
{
    v.visit(*this);
}

void SourceCallback::update (Source *s, float dt)
{
    if (s != nullptr) {
        // time passed
        elapsed_ += dt;
        // wait for delay to start
        if ( status_ == PENDING && elapsed_ > delay_ )
            status_ = READY;
    }
    // invalid
    else
        status_ = FINISHED;

}


ValueSourceCallback::ValueSourceCallback(float target, float ms, bool revert) : SourceCallback(),
    duration_(ms), start_(0.f), target_(target), bidirectional_(revert)
{
}


void ValueSourceCallback::update(Source *s, float dt)
{
    SourceCallback::update(s, dt);

    // set start on first time it is ready
    if ( status_ == READY ){
        start_ = readValue(s);
        status_ = ACTIVE;
    }

    // update when active
    if ( status_ == ACTIVE && s->ready() ) {

        // time passed since start
        float progress = elapsed_ - delay_;

        // time-out or instantaneous
        if ( !(ABS(duration_) > 0.f) || progress > duration_ ) {
            // apply target
            writeValue(s, target_);
            // done
            status_ = FINISHED;
        }
        // perform iteration of interpolation
        else {
            // apply calculated intermediate
            writeValue(s, glm::mix(start_, target_, progress/duration_) );
        }
    }
}

void ValueSourceCallback::multiply (float factor)
{
    target_ *= factor;
}

SourceCallback *ValueSourceCallback::clone() const
{
    SourceCallback *ret = SourceCallback::create(type());
    ValueSourceCallback *vsc = static_cast<ValueSourceCallback *>(ret);
    vsc->setValue( target_ );
    vsc->setDuration( duration_ );
    vsc->setBidirectional( bidirectional_ );

    return ret;
}

SourceCallback *ValueSourceCallback::reverse(Source *s) const
{
    SourceCallback *ret = nullptr;
    if (bidirectional_) {
        ret = SourceCallback::create(type());
        ValueSourceCallback *vsc = static_cast<ValueSourceCallback *>(ret);
        vsc->setValue( readValue(s) );
        vsc->setDuration( duration_ );
        vsc->setBidirectional( true );
    }

    return ret;
}

void ValueSourceCallback::accept(Visitor& v)
{
    SourceCallback::accept(v);
    v.visit(*this);
}


void ResetGeometry::update(Source *s, float dt)
{
    SourceCallback::update(s, dt);

    if (s->locked() ||
        (s->mode() == Source::CURRENT
         && Mixer::manager().view()->mode() == View::GEOMETRY
         && Mixer::manager().view()->initiated()) )
        status_ = FINISHED;

    // apply when ready
    if ( status_ == READY ){

        s->group(View::GEOMETRY)->scale_ = glm::vec3(1.f);
        s->group(View::GEOMETRY)->rotation_.z = 0;
        s->group(View::GEOMETRY)->crop_ = glm::vec4(-1.f, 1.f, 1.f, -1.f);
        s->group(View::GEOMETRY)->translation_ = glm::vec3(0.f);
        s->touch();

        status_ = FINISHED;
    }
}

SourceCallback *ResetGeometry::clone() const
{
    return new ResetGeometry;
}


SetAlpha::SetAlpha(float alpha, float ms, bool revert) : SourceCallback(),
    duration_(ms), alpha_(alpha), bidirectional_(revert)
{
    alpha_ = glm::clamp(alpha_, -1.f, 1.f);
    start_  = glm::vec2();
    target_ = glm::vec2();
}

void SetAlpha::update(Source *s, float dt)
{
    SourceCallback::update(s, dt);

    if (s->locked() ||
        (s->mode() == Source::CURRENT
         && Mixer::manager().view()->mode() == View::MIXING
         && Mixer::manager().view()->initiated()) )
        status_ = FINISHED;

    // set start position on first time it is ready
    if ( status_ == READY ){
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
        // special case Alpha < 0 : negative means inactive
        if (alpha_ < -DELTA_ALPHA) {
            // linear interpolation between 1 and the value given (max 2.f)
            target_ = step * ( ABS(alpha_) + 1.f);
        }
        // special case Alpha = 0
        else if (alpha_ < DELTA_ALPHA) {
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

        status_ = ACTIVE;
    }

    if ( status_ == ACTIVE ) {
        // time passed since start
        float progress = elapsed_ - delay_;

        // perform movement
        if ( ABS(duration_) > 0.f)
            s->group(View::MIXING)->translation_ = glm::vec3(glm::mix(start_, target_, progress/duration_), s->group(View::MIXING)->translation_.z);

        // time-out
        if ( progress > duration_ ) {
            // apply alpha to target
            s->group(View::MIXING)->translation_ = glm::vec3(target_, s->group(View::MIXING)->translation_.z);
            // done
            status_ = FINISHED;
        }
    }

}

void SetAlpha::multiply (float factor)
{
    alpha_ *= factor;
}

SourceCallback *SetAlpha::clone() const
{
    return new SetAlpha(alpha_, duration_, bidirectional_);
}

SourceCallback *SetAlpha::reverse(Source *s) const
{
    float _a = glm::length( glm::vec2(s->group(View::MIXING)->translation_) );
    if (_a > 1.f)
        _a *= -1.f;
    else
        _a = s->alpha();

    return bidirectional_ ? new SetAlpha(_a, duration_) : nullptr;
}

void SetAlpha::accept(Visitor& v)
{
    SourceCallback::accept(v);
    v.visit(*this);
}

Lock::Lock(bool on) : SourceCallback(), lock_(on)
{
}

void Lock::update(Source *s, float)
{
    if (s)
        s->setLocked(lock_);

    status_ = FINISHED;
}

SourceCallback *Lock::clone() const
{
    return new Lock(lock_);
}


Loom::Loom(float speed, float ms) : SourceCallback(),
    speed_(speed), duration_(ms)
{
    step_ = glm::normalize(glm::vec2(1.f, 1.f));   // step in diagonal by default
}

void Loom::update(Source *s, float dt)
{
    SourceCallback::update(s, dt);

    if (s->locked() ||
        (s->mode() == Source::CURRENT
         && Mixer::manager().view()->mode() == View::MIXING
         && Mixer::manager().view()->initiated()) )
        status_ = FINISHED;

    // current position
    glm::vec2 pos = glm::vec2(s->group(View::MIXING)->translation_);

    // set start on first time it is ready
    if ( status_ == READY ){
        // step in direction of source translation if possible
        if ( glm::length(pos) > DELTA_ALPHA)
            step_ = glm::normalize(pos);
        status_ = ACTIVE;
    }

    if ( status_ == ACTIVE ) {
        // time passed since start
        float progress = elapsed_ - delay_;

        // move target by speed vector (in the direction of step_, amplitude of speed * time (in second))
        pos -= step_ * ( speed_ * dt * 0.001f );

        // apply alpha if pos in range [0 MIXING_MAX_THRESHOLD]
        float l = glm::length( pos );
        if ( (l > 0.01f && speed_ > 0.f ) || (l < MIXING_MAX_THRESHOLD && speed_ < 0.f ) )
            s->group(View::MIXING)->translation_ = glm::vec3(pos, s->group(View::MIXING)->translation_.z);
        else
            status_ = FINISHED;

        // time-out
        if ( progress > duration_ )
            // done
            status_ = FINISHED;
    }
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


SetDepth::SetDepth(float target, float ms, bool revert) : SourceCallback(),
    duration_(ms), start_(0.f), target_(target), bidirectional_(revert)
{
    target_ = glm::clamp(target_, MIN_DEPTH, MAX_DEPTH);
}

void SetDepth::update(Source *s, float dt)
{
    SourceCallback::update(s, dt);

    if (s->locked() ||
        (s->mode() == Source::CURRENT
         && Mixer::manager().view()->mode() == View::LAYER
         && Mixer::manager().view()->initiated()) )
        status_ = FINISHED;

    // set start position on first time it is ready
    if ( status_ == READY ){
        start_ = s->group(View::LAYER)->translation_.z;
        status_ = ACTIVE;
    }

    if ( status_ == ACTIVE ) {
        // time passed since start
        float progress = elapsed_ - delay_;

        // perform movement
        if ( ABS(duration_) > 0.f)
            s->group(View::LAYER)->translation_.z = glm::mix(start_, target_, progress/duration_);

        // time-out
        if ( progress > duration_ ) {
            // apply depth to target
            s->group(View::LAYER)->translation_.z = target_;
            // done
            status_ = FINISHED;
        }

        // ensure reordering of sources in view
        ++View::need_deep_update_;
    }
}

void SetDepth::multiply (float factor)
{
    target_ *= factor;
}

SourceCallback *SetDepth::clone() const
{
    return new SetDepth(target_, duration_, bidirectional_);
}

SourceCallback *SetDepth::reverse(Source *s) const
{
    return bidirectional_ ? new SetDepth(s->depth(), duration_) : nullptr;
}

void SetDepth::accept(Visitor& v)
{
    SourceCallback::accept(v);
    v.visit(*this);
}

Play::Play(bool on, Session *parentsession, bool revert) : SourceCallback(),
    session_(parentsession), play_(on), bidirectional_(revert)
{
}

void Play::update(Source *s, float dt)
{
    SourceCallback::update(s, dt);

    // toggle play status when ready
    if ( status_ == READY && s->ready() &&
        (session_ ? session_->ready() : true) ){

        if (s->playing() != play_)
            // call play function
            s->play(play_);

        status_ = FINISHED;
    }

}

SourceCallback *Play::clone() const
{
    return new Play(play_, session_, bidirectional_);
}

SourceCallback *Play::reverse(Source *s) const
{
    return bidirectional_ ? new Play(s->playing()) : nullptr;
}

void Play::accept(Visitor& v)
{
    SourceCallback::accept(v);
    v.visit(*this);
}

RePlay::RePlay() : SourceCallback()
{
}

void RePlay::update(Source *s, float dt)
{
    SourceCallback::update(s, dt);

    // apply when ready
    if ( status_ == READY ){

        // call replay function
        s->replay();

        status_ = FINISHED;
    }
}

SourceCallback *RePlay::clone() const
{
    return new RePlay;
}


PlaySpeed::PlaySpeed(float v, float ms, bool r) : ValueSourceCallback(v, ms, r)
{
}

float PlaySpeed::readValue(Source *s) const
{
    double ret = 1.f;
    // access media player if target source is a media source
    MediaSource *ms = dynamic_cast<MediaSource *>(s);
    if (ms != nullptr) {
        ret = (float) ms->mediaplayer()->playSpeed();
    }

    return (float)ret;
}

void PlaySpeed::writeValue(Source *s, float val)
{
    // access media player if target source is a media source
    MediaSource *ms = dynamic_cast<MediaSource *>(s);
    if (ms != nullptr) {
        ms->mediaplayer()->setPlaySpeed((double) val);
    }
}

PlayFastForward::PlayFastForward(uint seekstep, float ms) : SourceCallback(), media_(nullptr),
    step_(seekstep), duration_(ms), playspeed_(0.)
{

}

void PlayFastForward::update(Source *s, float dt)
{
    SourceCallback::update(s, dt);

    // on first start, get the media player
    if ( status_ == READY ){
        // works for media source only
        MediaSource *ms = dynamic_cast<MediaSource *>(s);
        if (ms != nullptr) {
            MediaPlayer *mp = ms->mediaplayer();
            // works only if media player is enabled and valid
            if (mp && mp->isEnabled() && !mp->singleFrame()) {
                media_ = mp;
                playspeed_ = media_->playSpeed();
            }
        }
        status_ = media_ ? ACTIVE : FINISHED;
    }

    if ( status_ == ACTIVE ) {

        //  perform fast forward
        if (media_->isPlaying())
            media_->jump(step_);
        else
            media_->step(step_);

        // time-out
        if ( elapsed_ - delay_ > duration_ )
            // done
            status_ = FINISHED;
    }

    if (media_ && status_ == FINISHED)
        media_->setPlaySpeed( playspeed_ );

}

void PlayFastForward::multiply (float factor)
{
    step_ *= MAX( 1, (uint) ceil( ABS(factor) ) );
}

SourceCallback *PlayFastForward::clone() const
{
    return new PlayFastForward(step_, duration_);
}

void PlayFastForward::accept(Visitor& v)
{
    SourceCallback::accept(v);
    v.visit(*this);
}

Seek::Seek(glm::uint64 target, bool revert)
    : SourceCallback()
    , target_percent_(0.f)
    , target_time_(target)
    , bidirectional_(revert)
{}

Seek::Seek(float t)
    : SourceCallback()
    , target_percent_(t)
    , target_time_(0)
    , bidirectional_(false)
{
    target_percent_ = glm::clamp(target_percent_, 0.f, 1.f);
}

Seek::Seek(int hh, int mm, int ss, int ms)
    : SourceCallback()
    , target_percent_(0.f)
    , target_time_(0)
    , bidirectional_(false)
{
    if (ms > 0)
        target_time_ += GST_MSECOND * ms;
    if (ss > 0)
        target_time_ += GST_SECOND * ss;
    if (mm > 0)
        target_time_ += GST_SECOND * (mm * 60);
    if (hh > 0)
        target_time_ += GST_SECOND * (hh * 60 * 60);
}

void Seek::update(Source *s, float dt)
{
    SourceCallback::update(s, dt);

    // access media player if target source is a media source
    MediaSource *ms = dynamic_cast<MediaSource *>(s);
    if (ms != nullptr) {
        // get media info
        GstClockTime media_duration = ms->mediaplayer()->timeline()->duration();
        // set target position
        GstClockTime t = target_time_;
        // set target as percent if not provided
        if (t<1)
            t = ms->mediaplayer()->timeline()->timeFromPercent(target_percent_);

        // perform seek
        if (GST_CLOCK_TIME_IS_VALID(t) && GST_CLOCK_TIME_IS_VALID(media_duration)
            && media_duration > 0 && t <= media_duration)
            ms->mediaplayer()->seek(t);
    }

    status_ = FINISHED;
}

SourceCallback *Seek::clone() const
{
    if (target_time_ > 0)
        return new Seek(target_time_, bidirectional_);
    return new Seek(target_percent_);
}

SourceCallback *Seek::reverse(Source *s) const
{
    MediaSource *ms = dynamic_cast<MediaSource *>(s);
    if (ms != nullptr && bidirectional_)
        return new Seek( (glm::uint64) ms->mediaplayer()->position() );
    return nullptr;
}

void Seek::accept(Visitor& v)
{
    SourceCallback::accept(v);
    v.visit(*this);
}

SetGeometry::SetGeometry(const Group *g, float ms, bool revert) : SourceCallback(),
    duration_(ms), bidirectional_(revert)
{
    setTarget(g);
}

void  SetGeometry::getTarget (Group *g) const
{
    if (g!=nullptr)
        g->copyTransform(&target_);
}

void  SetGeometry::setTarget (const Group *g)
{
    if (g!=nullptr)
        target_.copyTransform(g);
}

void SetGeometry::update(Source *s, float dt)
{
    SourceCallback::update(s, dt);

    if (s->locked() ||
        (s->mode() == Source::CURRENT
         && Mixer::manager().view()->mode() == View::GEOMETRY
         && Mixer::manager().view()->initiated()) )
        status_ = FINISHED;

    // set start position on first time it is ready
    if ( status_ == READY ){
        start_.copyTransform(s->group(View::GEOMETRY));
        status_ = ACTIVE;
    }

    if ( status_ == ACTIVE ) {
        // time passed since start
        float progress = elapsed_ - delay_;

        // perform movement
        if ( ABS(duration_) > 0.f){
            Group intermediate;
            intermediate.translation_ = glm::mix(start_.translation_, target_.translation_, progress/duration_);
            intermediate.scale_ = glm::mix(start_.scale_, target_.scale_, progress/duration_);
            intermediate.rotation_ = glm::mix(start_.rotation_, target_.rotation_, progress/duration_);
            intermediate.data_[0] = glm::mix(start_.data_[0], target_.data_[0], progress/duration_);
            intermediate.data_[1] = glm::mix(start_.data_[1], target_.data_[1], progress/duration_);
            intermediate.data_[2] = glm::mix(start_.data_[2], target_.data_[2], progress/duration_);
            intermediate.data_[3] = glm::mix(start_.data_[3], target_.data_[3], progress/duration_);
            // apply geometry
            s->group(View::GEOMETRY)->copyTransform(&intermediate);
            s->touch();
        }

        // time-out
        if ( progress > duration_ ) {
            // apply  target
            s->group(View::GEOMETRY)->copyTransform(&target_);
            s->touch();
            // done
            status_ = FINISHED;
        }
    }
}

void SetGeometry::multiply (float factor)
{
    // TODO
}

SourceCallback *SetGeometry::clone() const
{
    return new SetGeometry(&target_, duration_, bidirectional_);
}

SourceCallback *SetGeometry::reverse(Source *s) const
{
    return bidirectional_ ? new SetGeometry( s->group(View::GEOMETRY), duration_) : nullptr;
}

void SetGeometry::accept(Visitor& v)
{
    SourceCallback::accept(v);
    v.visit(*this);
}



Grab::Grab(float dx, float dy, float ms) : SourceCallback(),
    speed_(glm::vec2(dx, dy)), duration_(ms)
{
}

void Grab::update(Source *s, float dt)
{
    SourceCallback::update(s, dt);

    if (s->locked() ||
        (s->mode() == Source::CURRENT
         && Mixer::manager().view()->mode() == View::GEOMETRY
         && Mixer::manager().view()->initiated()) )
        status_ = FINISHED;

    // set start on first time it is ready
    if ( status_ == READY ) {
        // initial position
        status_ = ACTIVE;
    }

    if ( status_ == ACTIVE ) {

        // move target by speed vector * time (in second)
        glm::vec2 pos = glm::vec2(s->group(View::GEOMETRY)->translation_);
        pos += speed_ * ( dt * 0.001f);
        s->group(View::GEOMETRY)->translation_ = glm::vec3(pos, s->group(View::GEOMETRY)->translation_.z);

        // time-out
        if ( (elapsed_ - delay_) > duration_ )
            // done
            status_ = FINISHED;
    }
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

Resize::Resize(float dx, float dy, float ms) : SourceCallback(),
    speed_(glm::vec2(dx, dy)), duration_(ms)
{
}

void Resize::update(Source *s, float dt)
{
    SourceCallback::update(s, dt);

    if (s->locked() ||
        (s->mode() == Source::CURRENT
         && Mixer::manager().view()->mode() == View::GEOMETRY
         && Mixer::manager().view()->initiated()) )
        status_ = FINISHED;

    if ( status_ == READY )
        status_ = ACTIVE;

    if ( status_ == ACTIVE ) {
        // move target by speed vector * time (in second)
        glm::vec2 scale = glm::vec2(s->group(View::GEOMETRY)->scale_) + speed_ * ( dt * 0.001f );
        s->group(View::GEOMETRY)->scale_ = glm::vec3(scale, s->group(View::GEOMETRY)->scale_.z);

        // time-out
        if ( (elapsed_ - delay_) > duration_ )
            status_ = FINISHED;
    }
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

Turn::Turn(float speed, float ms) : SourceCallback(),
    speed_(speed), duration_(ms)
{
}

void Turn::update(Source *s, float dt)
{
    SourceCallback::update(s, dt);

    if (s->locked() ||
        (s->mode() == Source::CURRENT
                        && Mixer::manager().view()->mode() == View::GEOMETRY
                        && Mixer::manager().view()->initiated()) )
        status_ = FINISHED;

    // set start on first time it is ready
    if ( status_ == READY ){
        status_ = ACTIVE;
    }

    if ( status_ == ACTIVE ) {

        // current position
        float angle = s->group(View::GEOMETRY)->rotation_.z;

        // perform movement
        angle -= speed_ * ( dt * 0.001f );
        s->group(View::GEOMETRY)->rotation_.z = angle;

        // timeout
        if ( (elapsed_ - delay_) > duration_ )
            // done
            status_ = FINISHED;
    }
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

SetBrightness::SetBrightness(float v, float ms, bool r) : ValueSourceCallback(v, ms, r)
{
    target_ = glm::clamp(target_, -1.f, 1.f);
}

float SetBrightness::readValue(Source *s) const
{
    return s->processingShader()->brightness;
}

void SetBrightness::writeValue(Source *s, float val)
{
    if (s->imageProcessingEnabled())
        s->processingShader()->brightness = val;
}

SetContrast::SetContrast(float v, float ms, bool r) : ValueSourceCallback(v, ms, r)
{
    target_ = glm::clamp(target_, -1.f, 1.f);
}

float SetContrast::readValue(Source *s) const
{
    return s->processingShader()->contrast;
}

void SetContrast::writeValue(Source *s, float val)
{
    if (s->imageProcessingEnabled())
        s->processingShader()->contrast = val;
}

SetSaturation::SetSaturation(float v, float ms, bool r) : ValueSourceCallback(v, ms, r)
{
    target_ = glm::clamp(target_, -1.f, 1.f);
}

float SetSaturation::readValue(Source *s) const
{
    return s->processingShader()->saturation;
}

void SetSaturation::writeValue(Source *s, float val)
{
    if (s->imageProcessingEnabled())
        s->processingShader()->saturation = val;
}

SetHue::SetHue(float v, float ms, bool r) : ValueSourceCallback(v, ms, r)
{
    target_ = glm::clamp(target_, 0.f, 1.f);
}

float SetHue::readValue(Source *s) const
{
    return s->processingShader()->hueshift;
}

void SetHue::writeValue(Source *s, float val)
{
    if (s->imageProcessingEnabled())
        s->processingShader()->hueshift = val;
}

SetThreshold::SetThreshold(float v, float ms, bool r) : ValueSourceCallback(v, ms, r)
{
    target_ = glm::clamp(target_, 0.f, 1.f);
}

float SetThreshold::readValue(Source *s) const
{
    return s->processingShader()->threshold;
}

void SetThreshold::writeValue(Source *s, float val)
{
    if (s->imageProcessingEnabled())
        s->processingShader()->threshold = val;
}


SetInvert::SetInvert(float v, float ms, bool r) : ValueSourceCallback(v, ms, r)
{
    target_ = glm::clamp(target_, 0.f, 2.f);
}

float SetInvert::readValue(Source *s) const
{
    return static_cast<float>(s->processingShader()->invert);
}

void SetInvert::writeValue(Source *s, float val)
{
    if (s->imageProcessingEnabled())
        s->processingShader()->invert = static_cast<int>(val);
}

SetPosterize::SetPosterize(float v, float ms, bool r) : ValueSourceCallback(v, ms, r)
{
    target_ = glm::clamp(target_, 0.f, 256.f);
}

float SetPosterize::readValue(Source *s) const
{
    return static_cast<float>(s->processingShader()->nbColors);
}

void SetPosterize::writeValue(Source *s, float val)
{
    if (s->imageProcessingEnabled())
        s->processingShader()->nbColors = static_cast<int>(val);
}


SetGamma::SetGamma(glm::vec4 g, float ms, bool revert) : SourceCallback(),
    duration_(ms), start_(glm::vec4()), target_(g), bidirectional_(revert)
{
    start_ = glm::clamp(start_, glm::vec4(0.f), glm::vec4(10.f));
}

void SetGamma::update(Source *s, float dt)
{
    SourceCallback::update(s, dt);

    if (!s->imageProcessingEnabled())
        status_ = FINISHED;

    // set start on first time it is ready
    if ( status_ == READY ){
        start_ = s->processingShader()->gamma;
        status_ = ACTIVE;
    }

    // update when active
    if ( status_ == ACTIVE ) {

        // time passed since start
        float progress = elapsed_ - delay_;

        // time-out or instantaneous
        if ( !(ABS(duration_) > 0.f) || progress > duration_ ) {
            // apply target
            s->processingShader()->gamma = target_;
            // done
            status_ = FINISHED;
        }
        // perform iteration of interpolation
        else {
            // apply calculated intermediate
            s->processingShader()->gamma = glm::mix(start_, target_, progress/duration_);
        }
    }
}

void SetGamma::multiply (float factor)
{
    target_ *= factor;
}

SourceCallback *SetGamma::clone() const
{
    return new SetGamma(target_, duration_, bidirectional_);
}

SourceCallback *SetGamma::reverse(Source *s) const
{
    return bidirectional_ ? new SetGamma(s->processingShader()->gamma, duration_) : nullptr;
}

void SetGamma::accept(Visitor& v)
{
    SourceCallback::accept(v);
    v.visit(*this);
}



SetFilter::SetFilter(const std::string &filter, const std::string &method, float value, float ms)
    : SourceCallback()
    , target_filter_(filter)
    , target_method_(method)
    , target_value_(value)
    , duration_(ms)
{
    imagefilter = nullptr;
    delayfilter = nullptr;
}

void SetFilter::update(Source *s, float dt)
{
    SourceCallback::update(s, dt);
    CloneSource *clonesrc = dynamic_cast<CloneSource *>(s);

    if (s->locked() || !clonesrc)
        status_ = FINISHED;

    // apply when ready
    if (status_ == READY) {

        ///
        /// Set Filter
        ///
        // convert target_filter_ to lower case to avoid mistakes
        std::transform(target_filter_.begin(),
                       target_filter_.end(),
                       target_filter_.begin(),
                       ::tolower);

        if (!target_filter_.empty()) {
            // find filter type ID of the filter name provided, if valid
            size_t __t = FrameBufferFilter::FILTER_PASSTHROUGH;
            for (; __t != FrameBufferFilter::FILTER_INVALID; __t++) {
                // get first word of filter label, in lower case
                std::string _b = std::get<2>(FrameBufferFilter::Types[__t]);
                auto loc = _b.find_first_of(" ");
                if (loc != std::string::npos)
                    _b = _b.erase(loc);
                std::transform(_b.begin(), _b.end(), _b.begin(), ::tolower);
                // end search if found same as target filter name (success)
                if (target_filter_.compare(_b) == 0)
                    break;
            }

            // Provided filter name is valid
            FrameBufferFilter::Type __type = FrameBufferFilter::Type(__t);
            if (__type != FrameBufferFilter::FILTER_INVALID) {
                // change filter if different from source
                if (clonesrc->filter()->type() != __type)
                    clonesrc->setFilter(__type);
            } else
                Log::Info("Filter : unknown type '%s'", target_filter_.c_str());
        }

        ///
        /// Set Method
        ///
        if (!target_method_.empty()) {
            // treat image filter types differently
            // ignore invalid or Nil types
            switch (clonesrc->filter()->type()) {
            case FrameBufferFilter::FILTER_RESAMPLE: {
                // get resamplig filter
                ResampleFilter *__f = dynamic_cast<ResampleFilter *>(clonesrc->filter());
                if ( !__f->setFactor(target_method_) )
                    Log::Info("Filter Resample: unknown factor '%s'", target_method_.c_str());
            } break;
            case FrameBufferFilter::FILTER_BLUR: {
                // get blur filter
                BlurFilter *__f = dynamic_cast<BlurFilter *>(clonesrc->filter());
                if ( !__f->setMethod(target_method_) )
                    Log::Info("Filter Blur: unknown method '%s'", target_method_.c_str());
            } break;
            case FrameBufferFilter::FILTER_SHARPEN: {
                // get sharpen filter
                SharpenFilter *__f = dynamic_cast<SharpenFilter *>(clonesrc->filter());
                if ( !__f->setMethod(target_method_) )
                    Log::Info("Filter Sharpen: unknown method '%s'", target_method_.c_str());
            } break;
            case FrameBufferFilter::FILTER_SMOOTH: {
                // get smooth filter
                SmoothFilter *__f = dynamic_cast<SmoothFilter *>(clonesrc->filter());
                if (!__f->setMethod(target_method_))
                    Log::Info("Filter Smooth: unknown method '%s'", target_method_.c_str());
            } break;
            case FrameBufferFilter::FILTER_EDGE: {
                // get edge filter
                EdgeFilter *__f = dynamic_cast<EdgeFilter *>(clonesrc->filter());
                if (!__f->setMethod(target_method_))
                    Log::Info("Filter Edge: unknown method '%s'", target_method_.c_str());
            } break;
            case FrameBufferFilter::FILTER_ALPHA: {
                // get alpha filter
                AlphaFilter *__f = dynamic_cast<AlphaFilter *>(clonesrc->filter());
                if (!__f->setOperation(target_method_))
                    Log::Info("Filter Alpha: unknown operation '%s'", target_method_.c_str());
            } break;
            case FrameBufferFilter::FILTER_IMAGE: {
                // Open the file
                std::ifstream file(target_method_);
                // Check if the file is opened successfully
                if (file.is_open()) {
                    // Read the content of the file into an std::string
                    std::string fileContent((std::istreambuf_iterator<char>(file)),
                                            std::istreambuf_iterator<char>());
                    FilteringProgram prog;
                    prog.setName(target_method_);
                    prog.setCode({fileContent, ""});
                    // get alpha filter
                    ImageFilter *__f = dynamic_cast<ImageFilter *>(clonesrc->filter());
                    __f->setProgram(prog);
                }
                else
                    Log::Info("Filter Custom: can't read file '%s'", target_method_.c_str());
                // Close the file
                file.close();
            } break;
            default:
                break;
            }
        }

        // by default, consider it's over,
        status_ = FINISHED;

        // ...but if a target value is given,
        if ( !std::isnan(target_value_) ){
            imagefilter = dynamic_cast<ImageFilter *>(clonesrc->filter());
            // ...and if there is a parameter to the filter
            if (imagefilter && imagefilter->program().parameters().size() > 0) {
                target_parameter_ = imagefilter->program().parameters().rbegin()->first;
                start_value_ = imagefilter->program().parameters().rbegin()->second;
                // then activate for value update
                status_ = ACTIVE;
            }
            // or maybe its a delay filter?
            else {
                delayfilter = dynamic_cast<DelayFilter *>(clonesrc->filter());
                if (delayfilter) {
                    start_value_ = delayfilter->delay();
                    // then activate for value update
                    status_ = ACTIVE;
                }
            }
        }
    }

    ///
    /// Set Value
    ///

    if (status_ == ACTIVE) {
        // time passed since start
        float progress = elapsed_ - delay_;
        // update ImageFilter types of filters
        if (imagefilter) {
            // time-out or instantaneous
            if (!(ABS(duration_) > 0.f) || progress > duration_) {
                // apply target value
                imagefilter->setProgramParameter(target_parameter_, target_value_);
                // done
                status_ = FINISHED;
            }
            else {
                // apply calculated intermediate value
                imagefilter->setProgramParameter(target_parameter_,
                                                 glm::mix(start_value_,
                                                          target_value_,
                                                          progress / duration_));
            }
        }
        // update DelayFilter
        else if (delayfilter) {
            // time-out or instantaneous
            if (!(ABS(duration_) > 0.f) || progress > duration_) {
                // apply target value
                delayfilter->setDelay(target_value_);
                // done
                status_ = FINISHED;
            }
            else {
                // apply calculated intermediate value
                delayfilter->setDelay(glm::mix(start_value_, target_value_, progress / duration_));
            }
        }
        else
            status_ = FINISHED;
    }
}

void SetFilter::multiply (float factor)
{
    target_value_ *= factor;
}

SourceCallback *SetFilter::clone() const
{
    return new SetFilter(target_filter_,
                         target_method_,
                         target_value_,
                         duration_);
}

void SetFilter::accept(Visitor& v)
{
    SourceCallback::accept(v);
    v.visit(*this);
}

SetFilterUniform::SetFilterUniform(const std::string &uniform, float value, float ms)
    : SourceCallback()
    , uniform_(uniform)
    , target_(value)
    , start_(0.f)
    , duration_(ms)
{
    imagefilter = nullptr;
}

void SetFilterUniform::update(Source *s, float dt)
{
    SourceCallback::update(s, dt);
    CloneSource *clonesrc = dynamic_cast<CloneSource *>(s);

    if (s->locked() || !clonesrc)
        status_ = FINISHED;


    // set start on first time it is ready
    if (status_ == READY) {
        // by default it will finish if all tests below fail
        status_ = FINISHED;
        // if there is an image filter in the clone source
        imagefilter = dynamic_cast<ImageFilter *>(clonesrc->filter());
        if (imagefilter) {
            // get the list of parameters
            std::map<std::string, float> params = imagefilter->program().parameters();
            // if there is a parameter named as given
            auto p = params.find(uniform_);
            if (p != params.end()) {
                // set start value to the current value
                start_ = p->second;
                // if a valid target value is given
                if (!std::isnan(target_))
                    // then activate for value update
                    status_ = ACTIVE;
            }
            else
                Log::Info("Filter : unknown uniform '%s'", uniform_.c_str());
        }
    }

    // set value
    if (status_ == ACTIVE && imagefilter) {

        // time passed since start
        float progress = elapsed_ - delay_;

        // time-out or instantaneous
        if (!(ABS(duration_) > 0.f) || progress > duration_) {
            // apply target value
            imagefilter->setProgramParameter(uniform_, target_);
            // done
            status_ = FINISHED;
        }
        else {
            // apply calculated intermediate value
            imagefilter->setProgramParameter(uniform_,
                                             glm::mix(start_, target_, progress / duration_));
        }
    }

}

void SetFilterUniform::multiply (float factor)
{
    target_ *= factor;
}

SourceCallback *SetFilterUniform::clone() const
{
    return new SetFilterUniform(uniform_, target_, duration_);
}

void SetFilterUniform::accept(Visitor& v)
{
    SourceCallback::accept(v);
    v.visit(*this);
}

SetBlending::SetBlending(const std::string &method)
    : SourceCallback()
    , target_method_(method)
{}

void SetBlending::update(Source *s, float dt)
{
    SourceCallback::update(s, dt);

    // set method on first time it is ready and finish
    if (status_ == READY) {
        if (!target_method_.empty()) {

            // find blending mode ID of the blending name provided, if valid
            size_t __t = Shader::BLEND_OPACITY;
            for (; __t != Shader::BLEND_NONE; __t++) {
                // get blending label, in lower case
                std::string _b = std::get<2>(Shader::blendingFunction[__t]);
                std::transform(_b.begin(), _b.end(), _b.begin(), ::tolower);
                std::transform(target_method_.begin(), target_method_.end(), target_method_.begin(), ::tolower);
                // end search if found same as target blending name (success)
                if (target_method_.compare(_b) == 0)
                    break;
            }

            // Provided blending name is valid
            Shader::BlendMode __mode = Shader::BlendMode(__t);
            if (__mode != Shader::BLEND_NONE) {
                // change mode if different from source
                if (s->blendingShader()->blending != __mode)
                    s->blendingShader()->blending = __mode;
            }
            else
                Log::Info("Blending : unknown mode '%s'", target_method_.c_str());
        }
        else
            Log::Info("Blending : Invalid mode", target_method_.c_str());

        status_ = FINISHED;
    }
}

SourceCallback *SetBlending::clone() const
{
    return new SetBlending(target_method_);
}
