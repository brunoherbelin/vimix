
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2020-2021 Bruno Herbelin <bruno.herbelin@gmail.com>
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
#include "Log.h"
#include "Source.h"
#include "ImageProcessingShader.h"
#include "UpdateCallback.h"

#include "Interpolator.h"


SourceInterpolator::SourceInterpolator(Source *subject, const SourceCore &target) :
    subject_(subject), from_(static_cast<SourceCore>(*subject)), to_(target), current_cursor_(0.f)
{


}


void SourceInterpolator::interpolateGroup(View::Mode m)
{
    current_state_.group(m)->translation_ =
            (1.f - current_cursor_) * from_.group(m)->translation_
            + current_cursor_ * to_.group(m)->translation_;
    current_state_.group(m)->scale_ =
            (1.f - current_cursor_) * from_.group(m)->scale_
            + current_cursor_ * to_.group(m)->scale_;
    current_state_.group(m)->rotation_ =
            (1.f - current_cursor_) * from_.group(m)->rotation_
            + current_cursor_ * to_.group(m)->rotation_;
    current_state_.group(m)->crop_ =
            (1.f - current_cursor_) * from_.group(m)->crop_
            + current_cursor_ * to_.group(m)->crop_;

    CopyCallback *anim = new CopyCallback( current_state_.group(m) );
    subject_->group(m)->update_callbacks_.clear();
    subject_->group(m)->update_callbacks_.push_back(anim);
}

void SourceInterpolator::interpolateImageProcessing()
{
    current_state_.processingShader()->brightness =
            (1.f - current_cursor_) * from_.processingShader()->brightness
            + current_cursor_ * to_.processingShader()->brightness;

    current_state_.processingShader()->contrast =
            (1.f - current_cursor_) * from_.processingShader()->contrast
            + current_cursor_ * to_.processingShader()->contrast;

    current_state_.processingShader()->saturation =
            (1.f - current_cursor_) * from_.processingShader()->saturation
            + current_cursor_ * to_.processingShader()->saturation;

    current_state_.processingShader()->hueshift =
            (1.f - current_cursor_) * from_.processingShader()->hueshift
            + current_cursor_ * to_.processingShader()->hueshift;

    current_state_.processingShader()->threshold =
            (1.f - current_cursor_) * from_.processingShader()->threshold
            + current_cursor_ * to_.processingShader()->threshold;

    current_state_.processingShader()->lumakey =
            (1.f - current_cursor_) * from_.processingShader()->lumakey
            + current_cursor_ * to_.processingShader()->lumakey;

    current_state_.processingShader()->nbColors =
            (1.f - current_cursor_) * from_.processingShader()->nbColors
            + current_cursor_ * to_.processingShader()->nbColors;

    current_state_.processingShader()->gamma =
            (1.f - current_cursor_) * from_.processingShader()->gamma
            + current_cursor_ * to_.processingShader()->gamma;

    current_state_.processingShader()->levels =
            (1.f - current_cursor_) * from_.processingShader()->levels
            + current_cursor_ * to_.processingShader()->levels;

    current_state_.processingShader()->chromakey =
            (1.f - current_cursor_) * from_.processingShader()->chromakey
            + current_cursor_ * to_.processingShader()->chromakey;

    current_state_.processingShader()->chromadelta =
            (1.f - current_cursor_) * from_.processingShader()->chromadelta
            + current_cursor_ * to_.processingShader()->chromadelta;

    subject_->processingShader()->copy( *current_state_.processingShader() );

// not interpolated : invert , filterid
}

float SourceInterpolator::current() const
{
    return current_cursor_;
}

void SourceInterpolator::apply(float percent)
{
    percent = CLAMP( percent, 0.f, 1.f);

    if ( subject_ && ABS_DIFF(current_cursor_, percent) > EPSILON)
    {
        current_cursor_ = percent;

        if (current_cursor_ < EPSILON) {
            current_cursor_ = 0.f;
            current_state_ = from_;
            subject_->copy(current_state_);
        }
        else if (current_cursor_ > 1.f - EPSILON) {
            current_cursor_ = 1.f;
            current_state_ = to_;
            subject_->copy(current_state_);
        }
        else {
            interpolateGroup(View::MIXING);
            interpolateGroup(View::GEOMETRY);
            interpolateGroup(View::LAYER);
            interpolateGroup(View::TEXTURE);
            interpolateImageProcessing();
//            Log::Info("SourceInterpolator::update %f", cursor);
        }

        subject_->touch();
    }
}

Interpolator::Interpolator()
{

}

Interpolator::~Interpolator()
{
    clear();
}

void Interpolator::clear()
{
    for (auto i = interpolators_.begin(); i != interpolators_.end(); ) {
        delete *i;
        i = interpolators_.erase(i);
    }
}


void Interpolator::add (Source *s, const SourceCore &target)
{
    SourceInterpolator *i = new SourceInterpolator(s, target);
    interpolators_.push_back(i);
}


float Interpolator::current() const
{
    float ret = 0.f;
    if (interpolators_.size() > 0)
        ret = interpolators_.front()->current();

    return ret;
}

void Interpolator::apply(float percent)
{
    for (auto i = interpolators_.begin(); i != interpolators_.end(); ++i)
        (*i)->apply( percent );

}


