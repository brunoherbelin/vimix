
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
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

#include "defines.h"
#include "Source/Source.h"
#include "ImageProcessingShader.h"

#include "Interpolator.h"

#define G(c, m) (c).group(View::m)
#define P(c) (c).processingShader()

namespace SourceCoreField
{

bool equal(const SourceCore &a, const SourceCore &b, Field f)
{
    switch (f) {
    case MIXING_POSITION:
        return glm::vec2(G(a,MIXING)->translation_) == glm::vec2(G(b,MIXING)->translation_);
    case GEOMETRY_POSITION:
        return glm::vec2(G(a,GEOMETRY)->translation_) == glm::vec2(G(b,GEOMETRY)->translation_);
    case GEOMETRY_SCALE:
        return glm::vec2(G(a,GEOMETRY)->scale_) == glm::vec2(G(b,GEOMETRY)->scale_);
    case GEOMETRY_ROTATION:
        return G(a,GEOMETRY)->rotation_.z == G(b,GEOMETRY)->rotation_.z;
    case GEOMETRY_CROP:
        return G(a,GEOMETRY)->crop_ == G(b,GEOMETRY)->crop_;
    case GEOMETRY_NODES:
        return G(a,GEOMETRY)->data_ == G(b,GEOMETRY)->data_;
    case LAYER_DEPTH:
        return G(a,LAYER)->translation_.z == G(b,LAYER)->translation_.z;
    case TEXTURE_POSITION:
        return glm::vec2(G(a,TEXTURE)->translation_) == glm::vec2(G(b,TEXTURE)->translation_);
    case TEXTURE_SCALE:
        return glm::vec2(G(a,TEXTURE)->scale_) == glm::vec2(G(b,TEXTURE)->scale_);
    case TEXTURE_ROTATION:
        return G(a,TEXTURE)->rotation_.z == G(b,TEXTURE)->rotation_.z;
    case COLOR_BRIGHTNESS:
        return P(a)->brightness == P(b)->brightness;
    case COLOR_CONTRAST:
        return P(a)->contrast == P(b)->contrast;
    case COLOR_SATURATION:
        return P(a)->saturation == P(b)->saturation;
    case COLOR_HUESHIFT:
        return P(a)->hueshift == P(b)->hueshift;
    case COLOR_THRESHOLD:
        return P(a)->threshold == P(b)->threshold;
    case COLOR_GAMMA:
        return P(a)->gamma == P(b)->gamma;
    case COLOR_LEVELS:
        return P(a)->levels == P(b)->levels;
    case COLOR_POSTERIZE:
        return P(a)->nbColors == P(b)->nbColors;
    case COLOR_INVERT:
        return P(a)->invert == P(b)->invert;
    default:
        return true;
    }
}

static inline void mixXY(glm::vec3 &dst, const glm::vec3 &a, const glm::vec3 &b, float t)
{
    dst.x = glm::mix(a.x, b.x, t);
    dst.y = glm::mix(a.y, b.y, t);
}

static void mixField(SourceCore &dst, const SourceCore &a, const SourceCore &b, float t, Field f)
{
    switch (f) {
    case MIXING_POSITION:
        mixXY(G(dst,MIXING)->translation_, G(a,MIXING)->translation_, G(b,MIXING)->translation_, t);
        break;
    case GEOMETRY_POSITION:
        mixXY(G(dst,GEOMETRY)->translation_, G(a,GEOMETRY)->translation_, G(b,GEOMETRY)->translation_, t);
        break;
    case GEOMETRY_SCALE:
        mixXY(G(dst,GEOMETRY)->scale_, G(a,GEOMETRY)->scale_, G(b,GEOMETRY)->scale_, t);
        break;
    case GEOMETRY_ROTATION:
        G(dst,GEOMETRY)->rotation_.z = glm::mix(G(a,GEOMETRY)->rotation_.z, G(b,GEOMETRY)->rotation_.z, t);
        break;
    case GEOMETRY_CROP:
        G(dst,GEOMETRY)->crop_ = glm::mix(G(a,GEOMETRY)->crop_, G(b,GEOMETRY)->crop_, t);
        break;
    case GEOMETRY_NODES:
        G(dst,GEOMETRY)->data_ = (1.f - t) * G(a,GEOMETRY)->data_ + t * G(b,GEOMETRY)->data_;
        break;
    case LAYER_DEPTH:
        G(dst,LAYER)->translation_.z = glm::mix(G(a,LAYER)->translation_.z, G(b,LAYER)->translation_.z, t);
        break;
    case TEXTURE_POSITION:
        mixXY(G(dst,TEXTURE)->translation_, G(a,TEXTURE)->translation_, G(b,TEXTURE)->translation_, t);
        break;
    case TEXTURE_SCALE:
        mixXY(G(dst,TEXTURE)->scale_, G(a,TEXTURE)->scale_, G(b,TEXTURE)->scale_, t);
        break;
    case TEXTURE_ROTATION:
        G(dst,TEXTURE)->rotation_.z = glm::mix(G(a,TEXTURE)->rotation_.z, G(b,TEXTURE)->rotation_.z, t);
        break;
    case COLOR_BRIGHTNESS:
        P(dst)->brightness = glm::mix(P(a)->brightness, P(b)->brightness, t);
        break;
    case COLOR_CONTRAST:
        P(dst)->contrast = glm::mix(P(a)->contrast, P(b)->contrast, t);
        break;
    case COLOR_SATURATION:
        P(dst)->saturation = glm::mix(P(a)->saturation, P(b)->saturation, t);
        break;
    case COLOR_HUESHIFT:
        P(dst)->hueshift = glm::mix(P(a)->hueshift, P(b)->hueshift, t);
        break;
    case COLOR_THRESHOLD:
        P(dst)->threshold = glm::mix(P(a)->threshold, P(b)->threshold, t);
        break;
    case COLOR_GAMMA:
        P(dst)->gamma = glm::mix(P(a)->gamma, P(b)->gamma, t);
        break;
    case COLOR_LEVELS:
        P(dst)->levels = glm::mix(P(a)->levels, P(b)->levels, t);
        break;
    case COLOR_POSTERIZE:
        P(dst)->nbColors = (int) roundf( glm::mix((float) P(a)->nbColors, (float) P(b)->nbColors, t) );
        break;
    case COLOR_INVERT:
        // discrete: switch at the end
        P(dst)->invert = t < 1.f ? P(a)->invert : P(b)->invert;
        break;
    default:
        break;
    }
}

void copy(SourceCore &dst, const SourceCore &src, Mask m)
{
    mix(dst, src, src, 0.f, m);
}

void mix(SourceCore &dst, const SourceCore &a, const SourceCore &b, float t, Mask m)
{
    for (int f = 0; f < FIELD_COUNT; ++f) {
        if ( m & bit((Field) f) )
            mixField(dst, a, b, t, (Field) f);
    }
}

Mask diff(const SourceCore &a, const SourceCore &b, Mask m)
{
    Mask d = 0;
    for (int f = 0; f < FIELD_COUNT; ++f) {
        if ( (m & bit((Field) f)) && !equal(a, b, (Field) f) )
            d |= bit((Field) f);
    }
    return d;
}

bool mergeUntouched(SourceCore &draft, SourceCore &base, const SourceCore &live)
{
    bool changed = false;
    for (int f = 0; f < FIELD_COUNT; ++f) {
        // field not modified in draft, and changed in live
        if ( equal(draft, base, (Field) f) && !equal(base, live, (Field) f) ) {
            mixField(draft, live, live, 0.f, (Field) f);
            mixField(base, live, live, 0.f, (Field) f);
            changed = true;
        }
    }
    return changed;
}

} // namespace SourceCoreField


SourceInterpolator::SourceInterpolator(Source *subject, const SourceCore &target, SourceCoreField::Mask mask) :
    subject_(subject), to_(target), mask_(mask), current_cursor_(0.f), started_(false)
{

}

float SourceInterpolator::current() const
{
    return current_cursor_;
}

void SourceInterpolator::apply(float percent)
{
    if ( subject_ == nullptr )
        return;

    percent = CLAMP( percent, 0.f, 1.f);
    if (percent < EPSILON)
        percent = 0.f;
    else if (percent > 1.f - EPSILON)
        percent = 1.f;

    // start from the state of the source at first application
    if ( !started_ ) {
        SourceCoreField::copy(from_, *subject_, mask_);
        started_ = true;
    }
    else if ( ABS_DIFF(current_cursor_, percent) < EPSILON )
        return;

    current_cursor_ = percent;

    // apply interpolation only on fields of the mask
    SourceCoreField::mix(*subject_, from_, to_, current_cursor_, mask_);

    // ensure reordering of sources in view
    if ( mask_ & SourceCoreField::bit(SourceCoreField::LAYER_DEPTH) )
        ++View::need_deep_update_;

    subject_->touch();
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

void Interpolator::add (Source *s, const SourceCore &target, SourceCoreField::Mask mask)
{
    SourceInterpolator *i = new SourceInterpolator(s, target, mask);
    interpolators_.push_back(i);
}

void Interpolator::remove (Source *s)
{
    for (auto i = interpolators_.begin(); i != interpolators_.end(); ) {
        if ( (*i)->subject() == s ) {
            delete *i;
            i = interpolators_.erase(i);
        }
        else
            ++i;
    }
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
