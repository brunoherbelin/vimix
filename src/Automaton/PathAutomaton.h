#ifndef PATHAUTOMATON_H
#define PATHAUTOMATON_H

#include <list>

#include "AutomatonMath.h"
#include "Automaton.h"

/**
 * @brief The AutomatonLink attaches an element of a source to the moving part of a PathAutomaton
 *
 * The element is either the source itself (moved as a whole) or one of its
 * handles in the Geometry view (resize, rotate, crop, etc.).
 * The displacement of the element is the displacement of the moving part of
 * the automaton since the link was established (mover - anchor).
 */
struct AutomatonLink
{
    // id of the linked source
    uint64_t source = 0;
    // Handles::Type of the element, or -1 for the source itself
    int handle = -1;
    // corner of the handle (rounded picking point), and picking point in handle coordinates
    glm::vec2 corner = glm::vec2(0.f);
    glm::vec2 pick = glm::vec2(0.f);
    // grabbed point in source coordinates (where the handle was clicked)
    glm::vec2 point = glm::vec2(0.f);
    // position of the moving part when the link was established
    glm::vec2 anchor = glm::vec2(0.f);

    // runtime: position of the linked element in the scene (for drawing)
    glm::vec2 target = glm::vec2(0.f);

    bool sameElement (const AutomatonLink &other) const;
};

/**
 * @brief The PathAutomaton is an automaton with a moving part traveling on a path
 */
class PathAutomaton : public Automaton
{
public:
    // position of the moving part
    inline glm::vec2 mover () const { return mover_; }

    // links to sources
    inline std::list<AutomatonLink> &links () { return links_; }
    // add the link, or remove it if the same element is already linked; returns true if added
    bool toggleLink (const AutomatonLink &link);
    void removeLinks (uint64_t source);
    // update drawing of links (after targets were updated)
    void updateLinks ();
    // draw a line from mover to given point while user is creating a link
    void showLinking (bool on, glm::vec2 to = glm::vec2(0.f));

    int pick (glm::vec2 p) const override;
    void accept (Visitor &v) override;

protected:
    PathAutomaton (View::Mode m, uint64_t id);

    // place the moving part (and its symbol)
    void setMover (glm::vec2 p);
    // update the line of the path
    void setPath (const std::vector<glm::vec2> &path, bool loop = false);

    glm::vec2 mover_;
    std::list<AutomatonLink> links_;

    Symbol *mover_node_;
    LineStrip *path_node_;
    Group *links_node_;
    std::vector<LineStrip *> link_lines_;
    std::vector<Symbol *> link_points_;
    LineStrip *linking_line_;
};

/**
 * @brief The LinearAutomaton moves a circle back and forth on a line
 * controls: [ A, B ] the ends of the line
 */
class LinearAutomaton : public PathAutomaton
{
public:
    LinearAutomaton (View::Mode m, uint64_t id = 0);

    Type type () const override { return AUTOMATON_LINEAR; }
    void reset (glm::vec2 center, float size) override;
    float curve (float u) const override;
    std::vector<float> events () const override;

protected:
    void motion () override;
    void rebuild () override;
    glm::vec2 center () const override;
};

/**
 * @brief The CircularAutomaton turns a circle around an ellipse
 * (clockwise by default, counter-clockwise if reverse)
 * controls: [ C, Rx, Ry ] the center and two points on the horizontal and vertical radius
 */
class CircularAutomaton : public PathAutomaton
{
public:
    CircularAutomaton (View::Mode m, uint64_t id = 0);

    Type type () const override { return AUTOMATON_CIRCULAR; }
    void reset (glm::vec2 center, float size) override;
    float curve (float u) const override;
    std::vector<float> events () const override;

protected:
    void motion () override;
    void rebuild () override;
    void constrain (int grip) override;
    glm::vec2 center () const override { return controls_[0]; }
    bool gripShown (size_t i) const override { return i > 0; }
    glm::vec2 radius () const;
};

/**
 * @brief The FreePathAutomaton moves a circle back and forth on a smooth path
 * passing through key points
 * controls: [ K0, K1, ... Kn ] the key points (at least 2)
 */
class FreePathAutomaton : public PathAutomaton
{
public:
    FreePathAutomaton (View::Mode m, uint64_t id = 0);

    Type type () const override { return AUTOMATON_PATH; }
    void reset (glm::vec2 center, float size) override;
    float curve (float u) const override;
    std::vector<float> events () const override;

    // add a key point after the last one, or remove the last one
    void addKey ();
    void removeKey ();

protected:
    void motion () override;
    void rebuild () override;

    AutomatonMath::ArcPath arc_;
    std::vector<float> key_fractions_;
};

#endif // PATHAUTOMATON_H
