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
#include "Log.h"
#include "Mixer.h"
#include "Session.h"
#include "ActionManager.h"
#include "Source/Source.h"

#include "Draft.h"

Draft::Draft() : state_(DRAFT_OFF), duration_(0.f), progress_(0.f)
{

}

bool Draft::enter()
{
    // finish ongoing animation
    if (state_ == DRAFT_ANIMATE)
        finish();

    if (state_ != DRAFT_OFF)
        return false;

    if (Mixer::manager().busy()) {
        Log::Notify("Draft mode not available while session is loading or saving.");
        return false;
    }

    Session *se = Mixer::manager().session();
    if (se == nullptr || se->frame() == nullptr)
        return false;

    // all sources of the session keep their live state aside
    for (auto it = se->begin(); it != se->end(); ++it)
        (*it)->beginDraft();

    // session renders both draft and live
    se->setDraft(true);

    state_ = DRAFT_EDIT;
    ++View::need_deep_update_;

    Log::Info("Draft mode started.");
    return true;
}

void Draft::cancel()
{
    if (state_ != DRAFT_EDIT)
        return;

    Session *se = Mixer::manager().session();

    // all sources get back to their (current) live state
    for (auto it = se->begin(); it != se->end(); ++it)
        (*it)->endDraft();

    se->setDraft(false);

    state_ = DRAFT_OFF;
    ++View::need_deep_update_;

    Log::Info("Draft cancelled.");
}

void Draft::apply(float duration)
{
    if (state_ != DRAFT_EDIT)
        return;

    Session *se = Mixer::manager().session();

    interpolator_.clear();
    for (auto it = se->begin(); it != se->end(); ++it) {
        Source *s = *it;
        if ( !s->drafting() )
            continue;

        // get the draft and the properties modified in draft
        SourceCore target;
        SourceCoreField::copy(target, *s);
        SourceCoreField::Mask mask = SourceCoreField::diff(target, *s->liveState());

        // source gets back to live state (no change in output)
        s->endDraft();

        // animate modified properties from live to draft
        if (mask != 0)
            interpolator_.add(s, target, mask);
    }

    se->setDraft(false);
    ++View::need_deep_update_;

    // nothing to animate
    if (interpolator_.empty()) {
        state_ = DRAFT_OFF;
        Log::Info("Draft applied (no change).");
        return;
    }

    state_ = DRAFT_ANIMATE;
    duration_ = MAX(duration, 0.f);
    progress_ = 0.f;

    // start (or finish) the animation
    if (duration_ > 0.f)
        interpolator_.apply(0.f);
    else
        finish();
}

void Draft::terminate()
{
    if (state_ == DRAFT_EDIT)
        cancel();
    else if (state_ == DRAFT_ANIMATE)
        finish();
}

float Draft::progress() const
{
    if (state_ == DRAFT_ANIMATE && duration_ > 0.f)
        return CLAMP(progress_ / duration_, 0.f, 1.f);
    return 0.f;
}

void Draft::update(float dt)
{
    if (state_ != DRAFT_ANIMATE)
        return;

    progress_ += dt;

    if (progress_ < duration_)
        interpolator_.apply(progress_ / duration_);
    else
        finish();
}

void Draft::finish()
{
    interpolator_.apply(1.f);
    interpolator_.clear();

    state_ = DRAFT_OFF;

    // single entry in history for the draft
    Action::manager().store("Draft applied");
}

void Draft::forget(Source *s)
{
    interpolator_.remove(s);
}
