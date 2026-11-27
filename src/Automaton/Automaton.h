#ifndef AUTOMATON_H
#define AUTOMATON_H

#include <string>
#include <vector>
#include <glm/glm.hpp>

#include "View/View.h"

class Group;
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
    // ViewSpace : an automaton acts only on the sources of its ViewSpace,
    // and is shown in views only if its ViewSpace is shown
    inline uint viewSpace () const { return viewspace_; }
    void setViewSpace (uint v);
    // drawn in its view (NB: hidden automatons cannot be picked)
    bool visible () const;
    void setVisible (bool on);
    // the user can interact with the automaton in its view only if its ViewSpace
    // is the only one shown (otherwise it is drawn semi-transparent)
    inline bool interactive () const { return interactive_; }
    inline void setInteractive (bool on) { interactive_ = on; }

    // A paused automaton stays still but keeps acting on sources.
    // When played again, it waits for the next beat where it can resume its motion
    inline bool paused () const { return paused_; }
    void setPaused (bool on);
    // waiting for the next beat to resume after pause
    inline bool resuming () const { return resuming_; }
    inline void setResuming (bool on) { resuming_ = on; resume_at_ = RESUME_UNSET; }
    // beat time when motion will resume (valid while resuming, after first animate)
    inline double resumeTime () const { return resume_at_; }
    // go back to the start of the cycle. If playing, the automaton holds at start
    // until the next beat and starts its cycle from there; if paused, it stays at
    // start, and its cycle will begin on a beat when played
    void restart (double beats);
    // A looping automaton repeats its cycle indefinitely. Otherwise, it plays
    // until the end of its cycle and pauses (waiting to be played again)
    inline bool loop () const { return loop_; }
    void setLoop (bool on);

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
    // beat time of the current state (frozen while paused)
    inline double time () const { return time_; }
    inline void setTime (double t) { time_ = t; }
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
    // (buttons are clicked, not dragged)
    enum { GRIP_NONE = -1, GRIP_MOVE = -2, GRIP_MOVER = -3, GRIP_ADD_KEY = -4, GRIP_REMOVE_KEY = -5 };
    virtual int pick (glm::vec2 p) const;
    // move grip by delta, from the stored controls (at the start of grab)
    void grab (int grip, const std::vector<glm::vec2> &stored, glm::vec2 delta);
    // highlight the grip under the cursor
    inline void hover (int grip) { hover_ = grip; }
    // emphasize the drawing (e.g. selected in the control window)
    inline void setHighlight (bool on) { highlight_ = on; }

    // accept all kind of visitors
    virtual void accept (Visitor &v);

    static constexpr double RESUME_UNSET = -1e300;

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
    // control point marked with a border (double diamond), e.g. end of an open path
    virtual bool gripBorder (size_t) const { return false; }
    // update the graphics of grips (after motion)
    virtual void updateGrips ();

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
    uint viewspace_;
    bool interactive_;
    bool paused_;
    bool resuming_;
    double resume_at_;
    bool loop_;
    double stop_at_;
    double time_;
    double period_;
    double offset_;
    int shape_;
    bool reverse_;
    std::vector<glm::vec2> controls_;
    double progress_;
    float uiscale_;

    // scene
    Group *frame_;
    Group *root_;
    Group *parent_;
    Group *grips_;
    std::vector<Symbol *> grip_nodes_;
    Symbol *move_grip_;
    Symbol *border_grip_;
    int hover_;
    bool highlight_;
};

//
// Look of automatons (color is COLOR_AUTOMATON in defines.h)
//
// Sizes of graphics on screen (in units of the projection, independent of zoom)
#define AUTOMATON_GRIP_SIZE          0.04f
#define AUTOMATON_GRIP_PICK          0.25f
#define AUTOMATON_GRIP_HOVER_SCALE   1.3f   // grips under the cursor are bigger
#define AUTOMATON_GRIP_BORDER    1.3f   // border around marked grips (relative to grip size)
#define AUTOMATON_MOVER_SIZE         0.07f
#define AUTOMATON_MOVER_PICK_SCALE   1.3f   // mover easier to pick than grips
#define AUTOMATON_MOVER_HOVER_SCALE  1.2f
#define AUTOMATON_LINK_POINT_SIZE    0.02f  // point at the end of link lines, on the source
#define AUTOMATON_KEY_BUTTON_SIZE    0.06f  // buttons to add / remove key points of free paths
#define AUTOMATON_KEY_BUTTON_PICK    0.07f
#define AUTOMATON_KEY_BUTTON_AHEAD   0.16f  // distance of buttons after the end point (along the path)
#define AUTOMATON_KEY_BUTTON_SIDE    0.07f // distance of buttons on each side of the path
// Width of lines (in pixels)
#define AUTOMATON_LINE_WIDTH         3.0f   // path of path automatons
#define AUTOMATON_LINE_WIDTH_MOTION  (AUTOMATON_LINE_WIDTH * 1.6f) // moving lines of field automatons (heart ring, rift lines)
#define AUTOMATON_LINE_WIDTH_GUIDE   (AUTOMATON_LINE_WIDTH * 0.8f) // limits of field automatons (heart max ring, rift axis)
#define AUTOMATON_LINE_WIDTH_LINK    (AUTOMATON_LINE_WIDTH * 0.8f)  // lines from mover to linked sources
// Opacity of drawings [0 1]
#define AUTOMATON_ALPHA_PATH         0.8f
#define AUTOMATON_ALPHA_MOVER        1.0f
#define AUTOMATON_ALPHA_MOTION       0.9f   // moving lines of field automatons
#define AUTOMATON_ALPHA_GUIDE        0.5f   // limits of field automatons
#define AUTOMATON_ALPHA_FILL         0.25f  // disk of heart
#define AUTOMATON_ALPHA_LINK         0.5f
#define AUTOMATON_ALPHA_LINK_POINT   0.8f
#define AUTOMATON_ALPHA_LINKING      1.0f   // line while dragging the mover to link
#define AUTOMATON_ALPHA_GRIP         0.7f
#define AUTOMATON_ALPHA_GRIP_MOVE    0.6f   // grip to move whole automaton
#define AUTOMATON_ALPHA_HOVER        1.0f   // grip under the cursor
// Factors of opacity
#define AUTOMATON_ALPHA_DISABLED     0.4f   // automaton disabled
#define AUTOMATON_ALPHA_VIEWSPACE    0.3f   // other ViewSpaces shown (not interactive)
#define AUTOMATON_ALPHA_HIGHLIGHT    1.3f   // automaton selected in the control window
// Sampling of curves
#define AUTOMATON_PATH_SAMPLES 96
// half size of the meshes of symbols used for drawing
#define AUTOMATON_MESH_POINT_SIZE 0.044f
#define AUTOMATON_MESH_CROSS_SIZE 0.7f
#define AUTOMATON_MESH_SQUARE_SIZE 0.574f
#define AUTOMATON_MESH_ICON_SIZE   0.11f

#endif // AUTOMATON_H
