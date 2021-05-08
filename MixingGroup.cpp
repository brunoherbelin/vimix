#include <algorithm>

#include <glm/gtx/vector_angle.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include "defines.h"
#include "Source.h"
#include "Decorations.h"
#include "Visitor.h"
#include "BaseToolkit.h"
#include "Log.h"

#include "MixingGroup.h"


MixingGroup::MixingGroup (SourceList sources) : parent_(nullptr), root_(nullptr), lines_(nullptr), center_(nullptr),
    center_pos_(glm::vec2(0.f, 0.f)), active_(true), update_action_(ACTION_NONE), updated_source_(nullptr)
{
    // create unique id
    id_ = BaseToolkit::uniqueId();

    // fill the vector of sources with the given list
    for (auto it = sources.begin(); it != sources.end(); ++it){
        // add only if not linked already
        if ((*it)->mixinggroup_ == nullptr) {
            (*it)->mixinggroup_ = this;
            sources_.push_back(*it);
        }
    }

    // scene elements
    root_ = new Group;
    root_->visible_ = false;
    center_ = new Symbol(Symbol::CIRCLE_POINT);
    center_->visible_ = false;
    center_->color  = glm::vec4(COLOR_MIXING_GROUP, 0.75f);
    center_->scale_ = glm::vec3(0.6f, 0.6f, 1.f);
    root_->attach(center_);

    // create
    recenter();
    createLineStrip();
}

MixingGroup::~MixingGroup ()
{
    for (auto it = sources_.begin(); it != sources_.end(); ++it)
        (*it)->clearMixingGroup();

    if (parent_)
        parent_->detach( root_ );
    delete root_;
}

void MixingGroup::accept(Visitor& v)
{
    v.visit(*this);
}

void MixingGroup::attachTo( Group *parent )
{
    if (parent_ != nullptr)
        parent_->detach( root_ );

    parent_ = parent;

    if (parent_ != nullptr)
        parent_->attach(root_);
}

void MixingGroup::recenter()
{
    // compute barycenter (0)
    center_pos_ = glm::vec2(0.f, 0.f);
    for (auto it = sources_.begin(); it != sources_.end(); ++it){
        // compute barycenter (1)
        center_pos_ += glm::vec2((*it)->group(View::MIXING)->translation_);
    }
    // compute barycenter (2)
    center_pos_ /= sources_.size();

    // set center
    center_->translation_ = glm::vec3(center_pos_, 0.f);
}

void MixingGroup::setAction (Action a)
{
    if (a == ACTION_UPDATE) {
        // accept UPDATE action only if no other action is ongoing
        if (update_action_ == ACTION_NONE)
            update_action_ = ACTION_UPDATE;
    }
    else if (a == ACTION_FINISH) {
        // only needs to finish if an action was done
        if (update_action_ != ACTION_NONE)
            update_action_ = ACTION_FINISH;
    }
    else
        update_action_ = a;
}

void MixingGroup::update (float)
{
    // after creation, root is not visible: wait that all sources are initialized to make it visible
    if (!root_->visible_) {
        auto unintitializedsource = std::find_if_not(sources_.begin(), sources_.end(), Source::isInitialized);
        root_->visible_ = (unintitializedsource == sources_.end());
    }

    // group is active if one source in the group is current
    auto currentsource = std::find_if(sources_.begin(), sources_.end(), Source::isCurrent);
    setActive(currentsource != sources_.end());

    // perform action
    if (update_action_ == ACTION_FINISH ) {
        // update barycenter
        recenter();
        // clear index, delete lines_, and recreate path and index with remaining sources
        createLineStrip();
        // update only once
        update_action_ = ACTION_NONE;
    }
    else if (update_action_ == ACTION_UPDATE ) {

        std::vector<glm::vec2> p = lines_->path();

        // compute barycenter (0)
        center_pos_ = glm::vec2(0.f, 0.f);
        auto it = sources_.begin();
        for (; it != sources_.end(); ++it){
            // update point
            p[ index_points_[*it] ] = glm::vec2((*it)->group(View::MIXING)->translation_);

            // compute barycenter (1)
            center_pos_ += glm::vec2((*it)->group(View::MIXING)->translation_);
        }
        // compute barycenter (2)
        center_pos_ /= sources_.size();
        center_->translation_ = glm::vec3(center_pos_, 0.f);

        // update path
        lines_->changePath(p);

        // update only once
        update_action_ = ACTION_NONE;
    }
    else if (update_action_ != ACTION_NONE && updated_source_ != nullptr) {

        if (update_action_ == ACTION_GRAB_ONE ) {

            // update path
            move(updated_source_);
            recenter();
        }
        else if (update_action_ == ACTION_GRAB_ALL ) {

            std::vector<glm::vec2> p = lines_->path();
            glm::vec2 displacement = glm::vec2(updated_source_->group(View::MIXING)->translation_);
            displacement -= p[ index_points_[updated_source_] ];

            // compute barycenter (0)
            center_pos_ = glm::vec2(0.f, 0.f);
            auto it = sources_.begin();
            for (; it != sources_.end(); ++it){

                // modify all but the already updated source
                if ( *it != updated_source_ && !(*it)->locked() ) {
                    (*it)->group(View::MIXING)->translation_.x += displacement.x;
                    (*it)->group(View::MIXING)->translation_.y += displacement.y;
                    (*it)->touch();
                }

                // update point
                p[ index_points_[*it] ] = glm::vec2((*it)->group(View::MIXING)->translation_);

                // compute barycenter (1)
                center_pos_ += glm::vec2((*it)->group(View::MIXING)->translation_);
            }
            // compute barycenter (2)
            center_pos_ /= sources_.size();
            center_->translation_ = glm::vec3(center_pos_, 0.f);

            // update path
            lines_->changePath(p);
        }
        else if (update_action_ == ACTION_ROTATE_ALL ) {

            std::vector<glm::vec2> p = lines_->path();

            // get angle rotation and distance scaling
            glm::vec2 pos_first = glm::vec2(updated_source_->group(View::MIXING)->translation_) -center_pos_;
            float angle = glm::orientedAngle( glm::normalize(pos_first), glm::vec2(1.f, 0.f) );
            float dist  = glm::length( pos_first );
            glm::vec2 pos_second = glm::vec2(p[ index_points_[updated_source_] ]) -center_pos_;
            angle -= glm::orientedAngle( glm::normalize(pos_second), glm::vec2(1.f, 0.f) );
            dist /= glm::length( pos_second );

            int numactions = 0;
            auto it = sources_.begin();
            for (; it != sources_.end(); ++it){

                // modify all but the already updated source
                if ( *it != updated_source_  && !(*it)->locked() ) {
                    glm::vec2 vec = glm::vec2((*it)->group(View::MIXING)->translation_) -center_pos_;
                    vec = glm::rotate(vec, -angle) * dist;
                    vec += center_pos_;

                    (*it)->group(View::MIXING)->translation_.x = vec.x;
                    (*it)->group(View::MIXING)->translation_.y = vec.y;
                    (*it)->touch();
                    numactions++;
                }

                // update point
                p[ index_points_[*it] ] = glm::vec2((*it)->group(View::MIXING)->translation_);
            }
            // update path
            lines_->changePath(p);
            // no source was rotated? better action is thus to grab
            if (numactions<1)
                update_action_ = ACTION_GRAB_ALL;
        }

        // done
        updated_source_ = nullptr;
    }

}



void MixingGroup::setActive (bool on)
{
    active_ = on;

    // overlays
    lines_->shader()->color.a = active_ ? 0.96f : 0.5f;
    center_->visible_ = update_action_ > ACTION_GRAB_ONE;
}

void MixingGroup::detach (Source *s)
{
    // find the source
    SourceList::iterator its = std::find(sources_.begin(), sources_.end(), s);
    // ok, its in the list !
    if (its != sources_.end()) {
        // tell the source
        (*its)->clearMixingGroup();
        // erase the source from the list
        sources_.erase(its);
        // update barycenter
        recenter();
        // clear index, delete lines_, and recreate path and index with remaining sources
        createLineStrip();
    }
}

void MixingGroup::detach (SourceList l)
{
    for (auto sit = l.begin(); sit !=  l.end(); ++sit) {
        // find the source
        SourceList::iterator its = std::find(sources_.begin(), sources_.end(), *sit);
        // ok, its in the list !
        if (its != sources_.end()) {
            // tell the source
            (*its)->clearMixingGroup();
            // erase the source from the list
            sources_.erase(its);
        }
    }
    // update barycenter
    recenter();
    // clear index, delete lines_, and recreate path and index with remaining sources
    createLineStrip();
}


void MixingGroup::attach (Source *s)
{
    // if source is not already in a group (this or other)
    if (s->mixinggroup_ == nullptr) {
        // tell the source
        s->mixinggroup_ = this;
        // add the source
        sources_.push_back(s);
        // update barycenter
        recenter();
        // clear index, delete lines_, and recreate path and index with remaining sources
        createLineStrip();
    }
}

void MixingGroup::attach (SourceList l)
{
    for (auto sit = l.begin(); sit !=  l.end(); ++sit) {
        if ( (*sit)->mixinggroup_ == nullptr) {
            // tell the source
            (*sit)->mixinggroup_ = this;
            // add the source
            sources_.push_back(*sit);
        }
    }
    // update barycenter
    recenter();
    // clear index, delete lines_, and recreate path and index with remaining sources
    createLineStrip();
}

uint MixingGroup::size()
{
    return sources_.size();
}

SourceList MixingGroup::getCopy() const
{
    SourceList sl = sources_;
    return sl;
}

SourceList::iterator MixingGroup::begin ()
{
    return sources_.begin();
}

SourceList::iterator MixingGroup::end ()
{
    return sources_.end();
}

bool MixingGroup::contains (Source *s)
{
    // find the source
    SourceList::iterator its = std::find(sources_.begin(), sources_.end(), s);
    // in the list ?
    return its != sources_.end();
}

void MixingGroup::move (Source *s)
{
    if (contains(s) && lines_) {
        // modify one point in the path
        lines_->editPath(index_points_[s], glm::vec2(s->group(View::MIXING)->translation_));
    }
}

void MixingGroup::createLineStrip()
{
    if (sources_.size() > 1) {

        if (lines_) {
            root_->detach(lines_);
            delete lines_;
        }

        // sort the vector of sources in clockwise order around the center pos_
        sources_ = mixing_sorted( sources_, center_pos_);

        // start afresh list of indices
        index_points_.clear();

        // path linking all sources
        std::vector<glm::vec2> path;
        for (auto it = sources_.begin(); it != sources_.end(); ++it){
            index_points_[*it] = path.size();
            path.push_back(glm::vec2((*it)->group(View::MIXING)->translation_));
        }

        // create
        lines_ = new LineLoop(path, 1.5f);
        lines_->shader()->color = glm::vec4(COLOR_MIXING_GROUP, 0.96f);
        root_->attach(lines_);
    }
}
