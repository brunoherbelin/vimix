#ifndef AUTOMATON_H
#define AUTOMATON_H

#include <string>
#include <vector>
#include <glm/glm.hpp>

#include "View/View.h"

class Group;
class Disk;
class Symbol;
class LineStrip;
class Visitor;

/**
 * @brief An Automaton is an animated system placed in a View (Mixing, Geometry or Layer),
 * drawn in the scene of the view, which moves the sources as if the user was moving them.
 *
 * Its timing is given in beats of the Metronome: a cycle of the automaton lasts
 * 'period' beats, shifted by 'offset' beats. The progress in the cycle is
 * computed from the absolute beat time of the Metronome.
 *
 * The geometry of an automaton is defined by its 'controls': a list of points
 * in the scene coordinates of its view (e.g. the two ends of a line), which the
 * user can grab with grips drawn in the view.
 *
 * Automatons are owned by the AutomatonRig of a Session, which applies
 * their effect on the sources (see AutomatonRig.h)
 */
class Automaton
{
public:

    // types of sub-classes of automatons
    typedef enum {
        AUTOMATON_LINEAR = 0,
        AUTOMATON_CIRCULAR,
        AUTOMATON_PATH,
        AUTOMATON_HEART,
        AUTOMATON_RIFT,
        AUTOMATON_INVALID
    } Type;
    // icon and name of each type
    static std::vector< std::pair<const char *, std::string> > Types;
    // names of the shapes of motion (see AutomatonMath::Shape)
    static std::vector< std::string > Shapes;

    // create an automaton of given type, for the given view
    static Automaton *create (Type t, View::Mode m, uint64_t id = 0);
    // can an automaton of type t be placed in view m
    static bool accepts (Type t, View::Mode m);

    virtual ~Automaton ();

    virtual Type type () const = 0;
    // field automatons act on all sources of the view; others act on linked sources
    virtual bool field () const { return false; }

    // properties
    inline uint64_t id () const { return id_; }
    inline View::Mode mode () const { return mode_; }
    inline std::string name () const { return name_; }
    inline void setName (const std::string &n) { name_ = n; }
    inline bool enabled () const { return enabled_; }
    void setEnabled (bool on);

    // timing (in beats)
    inline double period () const { return period_; }
    void setPeriod (double p);
    inline double offset () const { return offset_; }
    void setOffset (double o);
    inline int shape () const { return shape_; }
    inline void setShape (int s) { shape_ = s; }
    // direction (meaning depends on type, e.g. clockwise for circular)
    inline bool reverse () const { return reverse_; }
    inline void setReverse (bool on) { reverse_ = on; }

    // geometry: control points in scene coordinates of the view
    inline const std::vector<glm::vec2> &controls () const { return controls_; }
    void setControls (const std::vector<glm::vec2> &c);
    // default geometry centered on given point, of given size
    virtual void reset (glm::vec2 center, float size) = 0;

    // compute the state of the automaton at given beat time, and update graphics
    // uiscale is the inverse of the zoom of the view, to draw grips at constant size
    void animate (double beats, float uiscale);
    // progress [0 1] in the current cycle
    inline double progress () const { return progress_; }
    // value [0 1] of the motion at given fraction of the cycle (to draw timelines)
    virtual float curve (float u) const = 0;
    // fractions of the cycle where remarkable events occur (bounce, key point, peak)
    virtual std::vector<float> events () const { return std::vector<float>(); }

    // node to draw in the scene of the view
    inline Group *root () const { return root_; }
    void attachTo (Group *parent);
    inline Group *parent () const { return parent_; }

    // grips for user interaction (point given in scene coordinates)
    enum { GRIP_NONE = -1, GRIP_MOVE = -2, GRIP_MOVER = -3 };
    virtual int pick (glm::vec2 p) const;
    // move grip by delta, from the stored controls (at the start of grab)
    void grab (int grip, const std::vector<glm::vec2> &stored, glm::vec2 delta);
    // highlight the grip under the cursor
    inline void hover (int grip) { hover_ = grip; }
    // emphasize the drawing (e.g. selected in the control window)
    inline void setHighlight (bool on) { highlight_ = on; }

    // accept all kind of visitors
    virtual void accept (Visitor &v);

protected:
    Automaton (View::Mode m, uint64_t id);

    // update state and moving graphics according to progress_
    virtual void motion () = 0;
    // update static graphics according to controls_
    virtual void rebuild () = 0;
    // apply constraints on controls after grip was moved
    virtual void constrain (int) {}
    // position of the grip to move the whole automaton
    virtual glm::vec2 center () const;
    // control points can be hidden (e.g. when same as center)
    virtual bool gripShown (size_t) const { return true; }

    // utility to replace a line in a group
    static LineStrip *replaceLine (Group *parent, LineStrip *line, const std::vector<glm::vec2> &path,
                                   float width, float alpha, bool loop = false);
    // utility to set the path of a line (keeping its number of points)
    static void setLine (LineStrip *line, const std::vector<glm::vec2> &path);
    // inverse of the wave function, on the rising (or falling) part
    float waveInverse (float value, bool rising) const;
    // color of the drawing of the automaton
    glm::vec4 color (float alpha) const;

    uint64_t id_;
    std::string name_;
    View::Mode mode_;
    bool enabled_;
    double period_;
    double offset_;
    int shape_;
    bool reverse_;
    std::vector<glm::vec2> controls_;
    double progress_;
    float uiscale_;

    // scene
    Group *root_;
    Group *parent_;
    Group *grips_;
    std::vector<Disk *> grip_nodes_;
    Symbol *move_grip_;
    int hover_;
    bool highlight_;

private:
    void updateGrips ();
};

// Sizes of graphics on screen (in units of the projection, independent of zoom)
#define AUTOMATON_GRIP_SIZE   0.03f
#define AUTOMATON_GRIP_PICK   0.05f
#define AUTOMATON_MOVER_SIZE  0.07f
#define AUTOMATON_LINE_WIDTH  1.5f
#define AUTOMATON_PATH_SAMPLES 96
// half size of the meshes of symbols used for drawing
#define AUTOMATON_MESH_POINT_SIZE 0.044f
#define AUTOMATON_MESH_CROSS_SIZE 0.57f

#endif // AUTOMATON_H
