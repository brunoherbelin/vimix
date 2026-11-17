#ifndef AUTOMATONRIG_H
#define AUTOMATONRIG_H

#include <list>
#include <map>
#include <mutex>
#include <string>
#include <glm/glm.hpp>

#include "View/View.h"
#include "Automaton.h"

class Session;
class Source;
class PathAutomaton;
struct AutomatonLink;

/**
 * @brief AutomatonPose is the geometry of the node of a source in a view
 */
struct AutomatonPose
{
    glm::vec3 translation = glm::vec3(0.f);
    glm::vec3 rotation = glm::vec3(0.f);
    glm::vec3 scale = glm::vec3(1.f);
    glm::vec4 crop = glm::vec4(-1.f, 1.f, 1.f, -1.f);
    glm::mat4 data = glm::mat4(0.f);

    void capture (const Node *n);
    void apply (Node *n) const;
};

/**
 * @brief The AutomatonRig holds the automatons of a Session and applies their effects on the sources
 *
 * For every source (in a view) influenced by automatons, the rig keeps its
 * REST POSE: where the source would be without automatons. At every frame
 * (before the sources are updated by the session) the pose of the source is
 * recomputed from its rest pose, by applying the links of path automatons
 * then the fields of field automatons.
 *
 * Changes made by someone else (user, OSC, snapshot) between two frames are
 * detected by comparing the node of the source with the last pose written,
 * and are transfered to the rest pose. While the user grabs a source in a view,
 * the automatons do not act on it (frozen).
 *
 * When a source is not influenced anymore, it is placed back at its rest pose.
 */
class AutomatonRig
{
public:
    AutomatonRig (Session *session);
    ~AutomatonRig ();
    AutomatonRig (AutomatonRig const&) = delete;
    AutomatonRig& operator= (AutomatonRig const&) = delete;

    // list of automatons
    inline std::list<Automaton *>::iterator begin () { return automatons_.begin(); }
    inline std::list<Automaton *>::iterator end () { return automatons_.end(); }
    inline size_t size () const { return automatons_.size(); }
    inline bool empty () const { return automatons_.empty(); }
    // create an automaton of given type in the visible area of the view
    Automaton *add (Automaton::Type t, View *view);
    void add (Automaton *a);
    void remove (Automaton *a);
    void clear ();
    Automaton *find (uint64_t id) const;
    // remove a link of a path automaton (the source stays where it is)
    void unlink (PathAutomaton *a, const AutomatonLink &link);
    // enable or disable an automaton: linked sources of path automatons stay
    // where they are (their rest pose takes the effect of the link), while
    // sources pushed by field automatons are given back
    void setEnabled (Automaton *a, bool on);
    // after sources changed ViewSpace, path automatons follow their linked sources:
    // - if all its linked sources are in another ViewSpace, the automaton goes there
    // - otherwise, links to sources in another ViewSpace are removed (with a warning)
    void followViewSpaces ();

    // automaton selected for edition
    Automaton *selected () const;
    inline void setSelected (Automaton *a) { selected_ = a ? a->id() : 0; }

    // timing
    // pause or play all automatons (see Automaton::setPaused)
    void setPaused (bool on);
    bool allPaused () const;
    bool anyPaused () const;
    inline double beats () const { return beats_; }

    // per frame, called by Session::update
    // update before sources are updated (applies automatons)
    void update ();
    // commit after sources are updated (remember pose written)
    void commit ();

    // interaction in a view, with screen coordinates
    // grab a grip of an automaton (returns false if none under point)
    bool pick (View *view, glm::vec2 screen);
    View::Cursor drag (View *view, glm::vec2 screen);
    void release (View *view, glm::vec2 screen);
    void cancel ();
    inline bool grabbing () const { return grab_.bot != nullptr; }
    // mouse over (highlight grips)
    View::Cursor over (View *view, glm::vec2 screen);

    // persistence of rest poses
    struct RestPose {
        AutomatonPose base;
        AutomatonPose written;
        bool written_valid = false;
        bool frozen = false;
        bool influenced = false;
        Source *source = nullptr;
    };
    typedef std::pair<uint64_t, int> PoseKey;
    inline const std::map<PoseKey, RestPose> &poses () const { return poses_; }
    void setRestPose (uint64_t source, View::Mode m, const AutomatonPose &base);
    // lock access (for saving in a thread)
    inline void lock () { access_.lock(); }
    inline void unlock () { access_.unlock(); }

    // label of the element linked
    static std::string linkLabel (const AutomatonLink &link);

private:
    RestPose &acquire (Source *s, View::Mode m);
    // transfer the current effect of a link in the rest pose of its source
    void bake (PathAutomaton *a, const AutomatonLink &l);
    // anchor the links of a source to the current position of their automatons
    // (returns false if the source is not linked)
    bool rebase (Source *s, View::Mode m);
    // update the end points of the lines drawn for the links to a source
    // (handles: compute points of handles from the current pose of the source)
    void updateTargets (Source *s, View::Mode m, bool handles);
    void compose (Source *s, View::Mode m, RestPose &p);
    bool userGrabbing (Source *s, View::Mode m) const;
    bool pickTarget (View *view, glm::vec2 screen, uint viewspace, AutomatonLink &link, std::string &label) const;

    Session *session_;
    std::list<Automaton *> automatons_;
    std::map<PoseKey, RestPose> poses_;
    std::mutex access_;
    uint64_t selected_;
    double beats_;

    struct Grab {
        Automaton *bot = nullptr;
        int grip = Automaton::GRIP_NONE;
        glm::vec2 from = glm::vec2(0.f);
        std::vector<glm::vec2> stored;
        bool moved = false;
        // ViewSpaces shown before linking (to restore after)
        bool linking = false;
        uint viewspaces = 0;
    };
    // while dragging to link, only the ViewSpace of the automaton is shown
    void beginLinking ();
    void endLinking ();
    Grab grab_;
};

#endif // AUTOMATONRIG_H
