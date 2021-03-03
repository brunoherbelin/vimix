#include <algorithm>
#include <vector>

#include <glm/gtx/vector_angle.hpp>

#include "Source.h"
#include "Decorations.h"
#include "Log.h"

#include "MixingGroup.h"

// utility to sort Sources in MixingView in a clockwise order
// in reference to a center point
struct clockwise_centered {
    clockwise_centered(glm::vec2 c) : center(c) { }
    bool operator() (Source * first, Source * second) {
        glm::vec2 pos_first = glm::vec2(first->group(View::MIXING)->translation_)-center;
        float angle_first = glm::orientedAngle( glm::normalize(pos_first), glm::vec2(1.f, 0.f) );
        glm::vec2 pos_second = glm::vec2(second->group(View::MIXING)->translation_)-center;
        float angle_second = glm::orientedAngle( glm::normalize(pos_second), glm::vec2(1.f, 0.f) );
        return (angle_first < angle_second);
    }
    glm::vec2 center;
};

MixingGroup::MixingGroup (SourceList sources) : root_(nullptr), lines_(nullptr), center_(nullptr), pos_(glm::vec2(0.f, 0.f))
{
    // fill the vector of sources with the given list
    for (auto it = sources.begin(); it != sources.end(); it++){
        (*it)->mixinggroup_ = this;
        sources_.push_back(*it);
        // compute barycenter (1)
        pos_ += glm::vec2((*it)->group(View::MIXING)->translation_);
    }
    // compute barycenter (2)
    pos_ /= sources_.size();

    // sort the vector of sources in clockwise order around the center pos_
    std::sort(sources_.begin(), sources_.end(), clockwise_centered(pos_));

    root_ = new Group;
    center_ = new Symbol(Symbol::CIRCLE_POINT);
    center_->color =  glm::vec4(0.f, 1.f, 0.f, 0.8f);
    center_->translation_ = glm::vec3(pos_, 0.f);
    root_->attach(center_);
    createLineStrip();
}

void MixingGroup::detach (Source *s)
{
    // find the source
    std::vector<Source *>::iterator its = std::find(sources_.begin(), sources_.end(), s);
    // ok, its in the list !
    if (its != sources_.end()) {
        // erase the source from the list
        sources_.erase(its);

        // clear index, delete lines_, and recreate path and index with remaining sources
        createLineStrip();
    }
}

void MixingGroup::update (Source *s)
{
    // find the source
    std::vector<Source *>::iterator its = std::find(sources_.begin(), sources_.end(), s);

    if (its != sources_.end() && lines_) {

        lines_->editPath(index_points_[s], glm::vec2(s->group(View::MIXING)->translation_));//
//        lines_->editPath(0, glm::vec2(s->group(View::MIXING)->translation_));

    }

}

void MixingGroup::draw ()
{

}

//void MixingGroup::grab (glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2>)
//{

//}


void MixingGroup::createLineStrip()
{
    if (lines_) {
        root_->detach(lines_);
        delete lines_;
    }

    index_points_.clear();

    if (sources_.size() > 1) {
        std::vector<glm::vec2> path;
        auto it = sources_.begin();
        // link sources
        for (; it != sources_.end(); it++){
            index_points_[*it] = path.size();
            path.push_back(glm::vec2((*it)->group(View::MIXING)->translation_));
        }
//        // loop
//        it = sources_.begin();
//        index_points_[*it] = path.size();
//        path.push_back(glm::vec2((*it)->group(View::MIXING)->translation_));
        // create
        lines_ = new LineLoop(path, 2.f);
        lines_->shader()->color = glm::vec4(0.f, 1.f, 0.f, 0.8f);

        root_->attach(lines_);
    }
}
