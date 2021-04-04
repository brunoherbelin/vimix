#ifndef SOURCELIST_H
#define SOURCELIST_H

#include <list>
#include <glm/glm.hpp>

class Source;

typedef std::list<Source *> SourceList;

SourceList depth_sorted  (const SourceList &list);
SourceList mixing_sorted (const SourceList &list, glm::vec2 center = glm::vec2(0.f, 0.f));
SourceList intersect     (const SourceList &first, const SourceList &second);
SourceList join          (const SourceList &first, const SourceList &second);

typedef enum {
    SOURCELIST_DISTINCT = 0,
    SOURCELIST_INTERSECT = 1,
    SOURCELIST_EQUAL = 2,
    SOURCELIST_FIRST_IN_SECOND = 3,
    SOURCELIST_SECOND_IN_FIRST = 4
} SourceListCompare;
SourceListCompare compare (const SourceList &first, const SourceList &second);

typedef std::list<uint64_t> SourceIdList;
SourceIdList ids (const SourceList &list);

#endif // SOURCELIST_H
