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

#include <algorithm>

#include "defines.h"
#include "IconsFontAwesome5.h"
#include "Source/Source.h"
#include "Source/SourceCallback.h"
#include "Source/SessionSource.h"
#include "ControlManager.h"
#include "Session.h"

#include "InputCallbacks.h"

// any change anywhere invalidates the lists of nested actions of all sessions
uint InputCallbacks::revision_ = 1;

InputCallbacks::Assignment::~Assignment()
{
    clear();
}

void InputCallbacks::Assignment::clear()
{
    // go through all instances stored in Assignment
    for (auto clb = instances_.begin(); clb != instances_.end(); ++clb) {
        // finish all
        if (clb->second.first != nullptr)
            clb->second.first->finish();
        if (clb->second.second != nullptr)
            clb->second.second->finish();
    }
    // do not keep references: will be deleted when terminated
    instances_.clear();
}

InputCallbacks::InputCallbacks(Session *parent) : session_(parent), nested_revision_(0)
{
    input_sync_.resize(INPUT_MAX, Metronome::SYNC_NONE);

    // a session appeared: it could be nested in another one
    touch();
}

InputCallbacks::~InputCallbacks()
{
    // a session disappeared: it could have been nested in another one
    touch();

    // delete all callbacks
    for (auto iter = input_callbacks_.begin(); iter != input_callbacks_.end();
         iter = input_callbacks_.erase(iter))  {
        if ( iter->second.model_ != nullptr)
            delete iter->second.model_;
        iter->second.clear();
    }
}

void InputCallbacks::assign(uint input, Target target, SourceCallback *callback)
{
    // the actions changed
    touch();

    // find if this callback is already assigned
    auto k = input_callbacks_.begin();
    for (; k != input_callbacks_.end(); ++k)
    {
        // yes, then just change the target
        if ( k->second.model_ == callback) {
            k->second.target_ = target;
            // reverse became invalid
            k->second.clear();
            break;
        }
    }

    // if this callback is not assigned yet (looped until end)
    if ( k == input_callbacks_.end() ) {
        // create new entry
        Map::iterator added = input_callbacks_.emplace(input, Assignment() );
        added->second.model_ = callback;
        added->second.target_ = target;
    }
}

void InputCallbacks::swap(uint from, uint to)
{
    // the actions changed
    touch();

    Map swapped_callbacks_;

    for (auto k = input_callbacks_.begin(); k != input_callbacks_.end(); ++k)
    {
        if ( k->first == from )
            swapped_callbacks_.emplace( to,  k->second);
        else
            swapped_callbacks_.emplace( k->first,  k->second);
    }

    input_callbacks_.swap(swapped_callbacks_);
}

void InputCallbacks::copy(uint from, uint to)
{
    // the actions changed
    touch();

    if ( input_callbacks_.count(from) > 0 ) {
        auto from_callbacks = at(from);
        for (auto it = from_callbacks.cbegin(); it != from_callbacks.cend(); ++it){
            assign(to, it->first, it->second->clone() );
        }
    }
}

std::list<std::pair<Target, SourceCallback *> > InputCallbacks::at(uint input)
{
    std::list< std::pair< Target, SourceCallback*> > ret;

    if ( input_callbacks_.count(input) > 0 ) {
        auto result = input_callbacks_.equal_range(input);
        for (auto it = result.first; it != result.second; ++it)
            ret.push_back( std::pair< Target, SourceCallback*>(it->second.target_, it->second.model_) );
    }

    return ret;
}

void InputCallbacks::remove(SourceCallback *callback)
{
    // the actions changed
    touch();

    for (auto k = input_callbacks_.begin(); k != input_callbacks_.end(); ++k)
    {
        if ( k->second.model_ == callback) {
            delete callback;
            k->second.clear();
            input_callbacks_.erase(k);
            break;
        }
    }
}

void InputCallbacks::removeAll(uint input)
{
    // the actions changed
    touch();

    for (auto k = input_callbacks_.begin(); k != input_callbacks_.end();)
    {
        if ( k->first == input) {
            if (k->second.model_)
                delete k->second.model_;
            k->second.clear();
            k = input_callbacks_.erase(k);
        }
        else
            ++k;
    }
}

void InputCallbacks::removeAll(Target target)
{
    // the actions changed
    touch();

    for (auto k = input_callbacks_.begin(); k != input_callbacks_.end();)
    {
        if ( k->second.target_ == target) {
            if (k->second.model_)
                delete k->second.model_;
            k->second.clear();
            k = input_callbacks_.erase(k);
        }
        else
            ++k;
    }
}


void InputCallbacks::clear()
{
    // the actions changed
    touch();

    for (auto k = input_callbacks_.begin(); k != input_callbacks_.end(); )
    {
        if (k->second.model_)
            delete k->second.model_;
        k->second.clear();

        k = input_callbacks_.erase(k);
    }
}

std::list<uint> InputCallbacks::assignedInputs()
{
    std::list<uint> inputs;

    // fill with list of keys
    for(const auto& [key, value] : input_callbacks_) {
        inputs.push_back(key);
    }

    // remove duplicates
    inputs.unique();

    return inputs;
}

std::list<uint> InputCallbacks::inputsForSource( uint64_t sid )
{
    std::list<uint> inputs;

    if (sid > 0 && !input_callbacks_.empty()) {
        // test all targets of the list of input callbacks
        for(const auto& [key, value] : input_callbacks_) {
            if (Source * const* v = std::get_if<Source *>(&value.target_)) {
                // v is a source
                if (sid == (*v)->id())
                    // v is the source we are looking for
                    inputs.push_back(key);
            }
            else if ( const size_t* v = std::get_if<size_t>(&value.target_)) {
                // v is a batch
                SourceIdList::iterator it = std::find(session_->batch_[*v].begin(),
                                    session_->batch_[*v].end(),sid);
                if ( it != session_->batch_[*v].end())
                    // v contains the source we are looking for
                    inputs.push_back(key);
            }
        }
        // remove duplicates
        inputs.unique();
    }
    return inputs;
}

bool InputCallbacks::assigned(uint input)
{
    return input_callbacks_.find(input) != input_callbacks_.end();
}

void InputCallbacks::listNested(Session *se, const std::string &path, int level)
{
    // safety: do not recurse deeper than sessions can be nested
    if (se == nullptr || level > MAX_SESSION_LEVEL)
        return;

    for (auto sit = se->begin(); sit != se->end(); ++sit) {

        // only SessionSources (bundles & session files) embed a session
        SessionSource *ss = dynamic_cast<SessionSource *>(*sit);
        if (ss == nullptr || ss->session() == nullptr)
            continue;

        Session *nested_session = ss->session();
        const std::string nestedpath = path + (*sit)->name() + " " ICON_FA_LONG_ARROW_ALT_RIGHT " ";

        // remember all the actions assigned in the nested session
        for (auto k = nested_session->inputCallbacks()->begin();
                  k != nested_session->inputCallbacks()->end(); ++k) {
            // ignore an incomplete action
            if (k->second.model_ == nullptr)
                continue;
            Nested n;
            n.session  = nested_session;
            n.path     = nestedpath;
            n.target   = k->second.target_;
            n.callback = k->second.model_;
            nested_callbacks_.emplace( k->first, n );
        }

        // a bundle can contain bundles
        listNested(nested_session, nestedpath, level + 1);
    }
}

void InputCallbacks::updateNested()
{
    // nothing changed since the list was established: keep it
    if (nested_revision_ == revision_)
        return;
    nested_revision_ = revision_;

    // establish the list again
    nested_callbacks_.clear();
    listNested(session_, std::string());
}

bool InputCallbacks::assignedNested(uint input)
{
    updateNested();

    return nested_callbacks_.find(input) != nested_callbacks_.end();
}

bool InputCallbacks::assignedAnywhere(uint input)
{
    return assigned(input) || assignedNested(input);
}

std::list<InputCallbacks::Nested> InputCallbacks::nested(uint input)
{
    updateNested();

    std::list<Nested> ret;

    if ( nested_callbacks_.count(input) > 0 ) {
        auto result = nested_callbacks_.equal_range(input);
        for (auto it = result.first; it != result.second; ++it)
            ret.push_back( it->second );
    }

    return ret;
}

void InputCallbacks::removeSource( uint64_t sid )
{
    // the actions changed
    touch();

    if (sid > 0 && !input_callbacks_.empty()) {
        // test all targets of the list of input callbacks
        for (auto k = input_callbacks_.begin(); k != input_callbacks_.end(); )
        {
            if (Source * const* v = std::get_if<Source *>(&k->second.target_)) {
                // v is a source
                if ( sid == (*v)->id() ) {
                    // v is the source we are looking to remove
                    if (k->second.model_)
                        delete k->second.model_;
                    k->second.clear();
                    k = input_callbacks_.erase(k);
                }
                else
                    ++k;
            }
            else
                ++k;
        }
    }
}

InputCallbacks::Map InputCallbacks::copyMap() const
{
    Map _copy;
    if (!input_callbacks_.empty()) {
        for (auto k = input_callbacks_.begin(); k != input_callbacks_.end(); ++k)
        {
            _copy.emplace( k->first, Assignment() );
            _copy.rbegin()->second.model_ = k->second.model_->clone();
            _copy.rbegin()->second.target_ = k->second.target_;
        }
    }

    return _copy;
}

void InputCallbacks::import(InputCallbacks::Map &callbacks)
{
    // the actions changed
    touch();

    // NB: this takes ownership of the callback models given in the map (typically
    // obtained from copyMap), either by giving them to a source of this session,
    // or by deleting them. The map is emptied and owns nothing on return.
    for (auto k = callbacks.begin();
              k != callbacks.end(); ++k)
    {
        bool assigned = false;

        if (k->second.model_ != nullptr) {
            if (Source * const* v = std::get_if<Source *>(&k->second.target_)) {
                // v is a source
                // if the source exists in this session
                SourceList::iterator sit = std::find_if(session_->sources_.begin(), session_->sources_.end(), Source::hasId( (*v)->id() ));;
                if ( sit != session_->sources_.end()) {
                    // assign callback to this source (which takes ownership of the model)
                    assign( k->first, *v, k->second.model_ );
                    assigned = true;
                }
            }
        }

        // the model could not be given to a source of this session (e.g. the target is
        // not a source, or the source was not imported): delete it to avoid a leak
        if (!assigned && k->second.model_ != nullptr)
            delete k->second.model_;

        // in all cases the map does not own the model anymore
        k->second.model_ = nullptr;
    }

    // every callback was either assigned or deleted
    callbacks.clear();
}

void InputCallbacks::setSynchrony(uint input, Metronome::Synchronicity sync)
{
    input_sync_[input] = sync;
}

std::vector<Metronome::Synchronicity> InputCallbacks::synchrony()
{
    return input_sync_;
}

Metronome::Synchronicity InputCallbacks::synchrony(uint input)
{
    return input_sync_[input];
}
