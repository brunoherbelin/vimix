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

    // Get unique id
    inline uint64_t id () const { return id_; }

    // Source list manipulation
    SourceList getCopy() const;
    SourceList::iterator begin ();
    SourceList::iterator end ();
    uint size ();
    bool contains (Source *s);
    void detach (Source *s);
    void detach (SourceList l);
    void attach (Source *s);
    void attach (SourceList l);

    // actions for update
    typedef enum {
        ACTION_NONE = 0,
        ACTION_UPDATE = 1,
        ACTION_GRAB_ONE = 2,
        ACTION_GRAB_ALL = 3,
        ACTION_ROTATE_ALL = 4,
        ACTION_FINISH = 5
    } Action;
    void setAction (Action a) ;
    inline Action action () { return update_action_; }
    inline void follow (Source *s) { updated_source_ = s; }

    // to update in Session
    void update (float);

    // active if one of its source is current
    inline bool active () const { return active_; }
    void setActive (bool on);

    // attach node to draw in Mixing View
    void attachTo( Group *parent );

    // accept all kind of visitors
    virtual void accept (Visitor& v);

private:

    // Drawing elements
    Group *parent_;
    Group *root_;
    LineLoop *lines_;
    Symbol *center_;
    void createLineStrip();
    void recenter();
    void move (Source *s);

    // properties linked to sources
    glm::vec2  center_pos_;
    SourceList sources_;
    std::map< Source *, uint> index_points_;

    // status and actions
    uint64_t id_;
    bool active_;
    Action update_action_;
    Source *updated_source_;

};

#endif // MIXINGGROUP_H
