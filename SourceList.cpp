#include <algorithm>

#include <glm/gtx/vector_angle.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include "Source.h"
#include "SourceList.h"

// utility to sort Sources by depth
bool compare_depth (Source * first, Source * second)
{
  return ( first->depth() < second->depth() );
}

SourceList depth_sorted(SourceList list)
{
    SourceList sl = list;
    sl.sort(compare_depth);

    return sl;
}

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

SourceList mixing_sorted(SourceList list, glm::vec2 center)
{
    SourceList sl = list;
    sl.sort(clockwise_centered(center));

    return sl;
}


SourceIdList ids (SourceList list)
{
    SourceIdList idlist;

    for( auto sit = list.begin(); sit != list.end(); sit++)
        idlist.push_back( (*sit)->id() );

    // make sure no duplicate
    idlist.unique();

    return idlist;
}


SourceListCompare compare (SourceList first, SourceList second)
{
    SourceListCompare ret = SOURCELIST_DISTINCT;
    if (first.empty() || first.empty())
        return ret;

    // a new test list: start with the second list and remove all commons with first list
    SourceList test = second;
    for (auto it = first.begin(); it != first.end(); it++){
        test.remove(*it);
    }

    // all sources of the second list were in the first list
    if (test.empty()) {
        // same size, therefore they are the same!
        if (first.size() == second.size())
            ret = SOURCELIST_EQUAL;
        // otherwise, first list contains all sources of the second list.
        else
            ret = SOURCELIST_SECOND_IN_FIRST;
    }
    // some sources of the second list were in the first
    else if ( second.size() != test.size() ){
        // if the number of sources removed from second is the number of sources in the first
        if (second.size() - test.size() == first.size())
            ret = SOURCELIST_FIRST_IN_SECOND;
        // else, there is a patrial intersection
        else
            ret = SOURCELIST_INTERSECT;
    }
    // else no intersection, lists are distinct (return detault)

    return ret;
}


SourceList intersect (SourceList first, SourceList second)
{
    // take second list and remove all elements also in first list
    // -> builds the list of what remains in second list
    SourceList l1 = second;
    for (auto it = first.begin(); it != first.end(); it++)
        l1.remove(*it);
    // take second list and remove all elements in the remainer list
    // -> builds the list of what is in second list and was part of the first list
    SourceList l2 = second;
    for (auto it = l1.begin(); it != l1.end(); it++)
        l2.remove(*it);
    return l2;
}


SourceList join (SourceList first, SourceList second)
{
    SourceList l = second;
    for (auto it = first.begin(); it != first.end(); it++)
        l.push_back(*it);
    l.unique();
    return l;
}

