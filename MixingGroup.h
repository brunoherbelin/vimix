#ifndef MIXINGGROUP_H
#define MIXINGGROUP_H

#include <map>

#include "View.h"

class LineLoop;
class Symbol;

class MixingGroup
{
    Group *root_;
    LineLoop *lines_;
    Symbol *center_;
    glm::vec2 pos_;
    std::vector<Source *> sources_;
    std::map< Source *, uint> index_points_;

    void createLineStrip();

public:
    MixingGroup (SourceList sources);

    inline Node *node () { return root_; }

    void detach (Source *s);
    void update (Source *s);

    void draw ();
};

#endif // MIXINGGROUP_H
