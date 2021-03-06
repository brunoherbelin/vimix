#ifndef MIXINGGROUP_H
#define MIXINGGROUP_H

#include <map>

#include "View.h"

class LineLoop;
class Symbol;

class MixingGroup
{

public:
    MixingGroup (SourceList sources);
    ~MixingGroup ();

    typedef enum {
        ACTION_NONE = 0,
        ACTION_GRAB_ONE = 1,
        ACTION_GRAB_ALL = 2,
        ACTION_ROTATE_ALL = 3
    } Action;

    // actions for update
    inline void setAction (Action a) { update_action_ = a; }
    inline Action action () { return update_action_; }
    inline void follow (Source *s) { updated_source_ = s; }

    // get node to draw
    inline Group *group () { return root_; }

    // to update in Mixing View
    void update (float dt);
    inline  bool active () const { return active_; }
    void setActive (bool on);

    // individual change of a source in the group
    void detach (Source *s);
    void move   (Source *s);

private:

    // Drawing elements
    Group *root_;
    LineLoop *lines_;
    Symbol *center_;
    void createLineStrip();

    // properties linked to sources
    glm::vec2 center_pos_;
    std::vector<Source *> sources_;
    std::map< Source *, uint> index_points_;

    // status and actions
    bool active_;
    Action update_action_;
    Source *updated_source_;


};

#endif // MIXINGGROUP_H
