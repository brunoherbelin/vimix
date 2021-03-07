#ifndef MIXINGGROUP_H
#define MIXINGGROUP_H

#include <map>

#include "View.h"
#include "SourceList.h"

class LineLoop;
class Symbol;

class MixingGroup
{

public:
    MixingGroup (SourceList sources);
    ~MixingGroup ();

    // actions for update
    typedef enum {
        ACTION_NONE = 0,
        ACTION_GRAB_ONE = 1,
        ACTION_GRAB_ALL = 2,
        ACTION_ROTATE_ALL = 3
    } Action;
    inline void setAction (Action a) { update_action_ = a; }
    inline Action action () { return update_action_; }
    inline void follow (Source *s) { updated_source_ = s; }

    // node to draw
    inline Group *group () { return root_; }

    // accept all kind of visitors
    virtual void accept (Visitor& v);

    // to update in Mixing View
    void update (float dt);
    inline  bool active () const { return active_; }
    void setActive (bool on);

    // Source list manipulation
    SourceList::iterator begin ();
    SourceList::iterator end ();
    uint size ();
    bool contains (Source *s);
    void detach (Source *s);

private:

    // Drawing elements
    Group *root_;
    LineLoop *lines_;
    Symbol *center_;
    void createLineStrip();
    void move (Source *s);

    // properties linked to sources
    glm::vec2  center_pos_;
    SourceList sources_;
    std::map< Source *, uint> index_points_;

    // status and actions
    bool active_;
    Action update_action_;
    Source *updated_source_;


};

#endif // MIXINGGROUP_H
