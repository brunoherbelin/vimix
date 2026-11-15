#ifndef FIELDAUTOMATON_H
#define FIELDAUTOMATON_H

#include "Automaton.h"

class Mesh;

/**
 * @brief The FieldAutomaton acts on all the sources of its view,
 * by pushing their positions away from an area
 */
class FieldAutomaton : public Automaton
{
public:
    bool field () const override { return true; }

    // position of a source at rest point p, given the current state of the field
    virtual glm::vec2 push (glm::vec2 p) const = 0;

protected:
    FieldAutomaton (View::Mode m, uint64_t id) : Automaton(m, id) {}
};

/**
 * @brief The HeartAutomaton is a disk with a ring beating around it:
 * the ring grows from the disk to a maximum radius and shrinks back at each cycle,
 * pushing away the sources (and bringing them back), and no source can be inside the disk.
 * controls: [ C, P0, P1 ] the center, a point on the disk and a point at maximum radius
 */
class HeartAutomaton : public FieldAutomaton
{
public:
    HeartAutomaton (View::Mode m, uint64_t id = 0);

    Type type () const override { return AUTOMATON_HEART; }
    void reset (glm::vec2 center, float size) override;
    float curve (float u) const override;
    std::vector<float> events () const override;
    glm::vec2 push (glm::vec2 p) const override;

protected:
    void motion () override;
    void rebuild () override;
    void constrain (int grip) override;
    glm::vec2 center () const override { return controls_[0]; }
    bool gripShown (size_t i) const override { return i > 0; }

    float radius_;
    Mesh *disk_;
    LineStrip *ring_;
    LineStrip *max_ring_;
};

/**
 * @brief The RiftAutomaton is a pair of lines separating from each other and
 * closing back at each cycle: no source can remain between the lines.
 * controls: [ C, E, G ] the center, the end of the lines and the maximum gap
 */
class RiftAutomaton : public FieldAutomaton
{
public:
    RiftAutomaton (View::Mode m, uint64_t id = 0);

    Type type () const override { return AUTOMATON_RIFT; }
    void reset (glm::vec2 center, float size) override;
    float curve (float u) const override;
    std::vector<float> events () const override;
    glm::vec2 push (glm::vec2 p) const override;

protected:
    void motion () override;
    void rebuild () override;
    void constrain (int grip) override;
    glm::vec2 center () const override { return controls_[0]; }
    bool gripShown (size_t i) const override { return i > 0; }

    glm::vec2 direction () const;
    float halfgap_;
    float gap_;
    LineStrip *axis_;
    LineStrip *line_a_;
    LineStrip *line_b_;
};

#endif // FIELDAUTOMATON_H
