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

#include "Log.h"
#include "Mixer.h"
#include "Session.h"
#include "ActionManager.h"
#include "Source/Source.h"
#include "SessionCreator.h"

#include "Draft.h"

// maximum number of frames to wait for the draft session to be ready
#define DRAFT_MAX_PENDING 30

Draft::Draft() : state_(DRAFT_OFF), duration_(0.f), progress_(0.f), draft_(nullptr), pending_(0)
{

}

bool Draft::enter()
{
    // not possible during edit or animation
    if (state_ != DRAFT_OFF)
        return false;

    if (Mixer::manager().busy()) {
        Log::Notify("Draft mode not available while session is loading or saving.");
        return false;
    }

    // create the draft session from the live session
    Session *live = Mixer::manager().session();
    draft_ = DraftSessionLoader::createDraft(live);
    if (draft_ == nullptr)
        return false;

    // draft sources start exactly at the state of live sources, kept as reference
    for (auto it = draft_->begin(); it != draft_->end(); ++it) {
        SourceList::iterator l = live->find( (*it)->id() );
        if ( l != live->end() ) {
            SourceCoreField::copy(**it, **l);
            (*it)->setParameters( (*l)->parameters() );
            SourceCore *base = new SourceCore;
            SourceCoreField::copy(*base, **l);
            base_[(*it)->id()] = base;
            base_parameters_[(*it)->id()] = (*l)->parameters();
            base_processing_[(*it)->id()] = (*l)->imageProcessingEnabled();
        }
    }

    // the draft session will be edited when ready (see update)
    pending_ = 0;
    state_ = DRAFT_EDIT;

    Log::Info("Draft mode started.");
    return true;
}

void Draft::cancel()
{
    if (state_ != DRAFT_EDIT)
        return;

    close();
    state_ = DRAFT_OFF;

    Log::Info("Draft cancelled.");
}

void Draft::apply(float duration)
{
    if (state_ != DRAFT_EDIT)
        return;

    Session *live = Mixer::manager().liveSession();

    // animate live sources to the properties modified in draft
    interpolator_.clear();
    for (auto it = draft_->begin(); it != draft_->end(); ++it) {
        auto b = base_.find( (*it)->id() );
        SourceList::iterator l = live->find( (*it)->id() );
        if ( b == base_.end() || l == live->end() )
            continue;

        // properties modified in draft
        SourceCoreField::Mask mask = SourceCoreField::diff(**it, *b->second);
        Source::Parameters parameters = SourceCoreField::diff( (*it)->parameters(),
                                                               base_parameters_[(*it)->id()] );
        // image processing switched in draft
        SourceInterpolator::ProcessingSwitch processing = SourceInterpolator::PROCESSING_KEEP;
        if ( (*it)->imageProcessingEnabled() != base_processing_[(*it)->id()] )
            processing = (*it)->imageProcessingEnabled() ? SourceInterpolator::PROCESSING_ENABLE
                                                         : SourceInterpolator::PROCESSING_DISABLE;

        if ( mask != 0 || !parameters.empty() || processing != SourceInterpolator::PROCESSING_KEEP ) {
            SourceCore target;
            SourceCoreField::copy(target, **it);
            interpolator_.add(*l, target, mask, parameters, processing);
        }
    }

    close();

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

void Draft::close()
{
    // edit the live session again (draft session is deleted)
    if ( Mixer::manager().editingDraft() )
        Mixer::manager().restoreEditedSession();
    // draft session was not edited yet
    else
        delete draft_;
    draft_ = nullptr;

    clearBase();
}

void Draft::clearBase()
{
    for (auto b = base_.begin(); b != base_.end(); ++b)
        delete b->second;
    base_.clear();
    base_parameters_.clear();
    base_processing_.clear();
}

void Draft::merge()
{
    Session *live = Mixer::manager().liveSession();

    for (auto it = draft_->begin(); it != draft_->end(); ++it) {
        auto b = base_.find( (*it)->id() );
        SourceList::iterator l = live->find( (*it)->id() );
        if ( b == base_.end() || l == live->end() )
            continue;

        // properties not modified in draft follow live
        if ( SourceCoreField::mergeUntouched(**it, *b->second, **l) )
            (*it)->touch();

        Source::Parameters parameters = (*it)->parameters();
        if ( SourceCoreField::mergeUntouched(parameters, base_parameters_[(*it)->id()], (*l)->parameters()) )
            (*it)->setParameters(parameters);

        // image processing not switched in draft follows live
        bool &base_processing = base_processing_[(*it)->id()];
        if ( (*it)->imageProcessingEnabled() == base_processing
             && base_processing != (*l)->imageProcessingEnabled() ) {
            base_processing = (*l)->imageProcessingEnabled();
            (*it)->setImageProcessingEnabled(base_processing);
        }
    }

    // same fading as live
    if ( draft_->fadingTarget() != live->fading() )
        draft_->setFadingTarget( live->fading() );
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
    if (state_ == DRAFT_EDIT) {
        // draft session is not yet edited
        if ( !Mixer::manager().editingDraft() ) {
            // edit the draft session once all its sources are ready
            // NB: it is then updated by the Mixer as the edited session
            if ( draft_->ready() || ++pending_ > DRAFT_MAX_PENDING )
                Mixer::manager().setEditedSession(draft_);
            // keep updating draft session until ready
            else
                draft_->update(dt);
        }
        merge();
    }

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
