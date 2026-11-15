#ifndef AUTOMATONMATH_H
#define AUTOMATONMATH_H

#include <vector>
#include <cstddef>
#include <glm/glm.hpp>

/**
 * Pure math of automatons: no dependency to vimix (only glm),
 * so that it can be compiled and tested standalone
 * (see tests/automaton/test_automaton_math.cpp)
 *
 * All timing is expressed in beats (of the Metronome); the
 * progress in the cycle of an automaton is a fraction in [0 1[
 */
namespace AutomatonMath {

// shape of the motion along the cycle
typedef enum {
    SHAPE_LINEAR = 0,   // constant speed
    SHAPE_SMOOTH = 1,   // sinusoidal, slow at extremities
    SHAPE_PULSE  = 2    // fast start, slow end (heartbeat)
} Shape;

// fraction of cycle where the pulse wave reaches its maximum
#define AUTOMATON_PULSE_ATTACK 0.2f

// progress [0 1[ in the cycle of given period at given time (all in beats)
// (valid for negative beats and fractional periods)
double progress (double beats, double period, double offset = 0.0);

// number of the cycle (integer part) at given time
double cycle (double beats, double period, double offset = 0.0);

// ramp 0 -> 1 during the cycle (looping motion, e.g. turn around a circle)
float ramp (float u, int shape = SHAPE_LINEAR);

// wave 0 -> 1 -> 0 during the cycle (back and forth motion, beating)
float wave (float u, int shape = SHAPE_LINEAR);

// fraction of the cycle where the wave is at maximum
float wavePeak (int shape);

// point on ellipse at given turn [0 1] (counter clockwise, starting on the right)
glm::vec2 ellipse (glm::vec2 center, glm::vec2 radius, float turn);

// smooth curve passing through all given key points (centripetal Catmull-Rom)
// returns the polyline of 'samples' points (first and last are first and last keys)
// fills the list of indices of samples corresponding to keys, if given
std::vector<glm::vec2> smoothPath (const std::vector<glm::vec2> &keys, size_t samples,
                                   std::vector<size_t> *key_indices = nullptr);

/**
 * Polyline parameterized by its arc length,
 * to move at constant speed along a path
 */
class ArcPath
{
public:
    ArcPath () : length_(0.f) {}
    ArcPath (const std::vector<glm::vec2> &points) { set(points); }

    void set (const std::vector<glm::vec2> &points);
    inline const std::vector<glm::vec2> &points () const { return points_; }
    inline float length () const { return length_; }

    // position at fraction t [0 1] of the total length
    glm::vec2 at (float t) const;
    // fraction [0 1] of the total length at point of given index
    float fraction (size_t index) const;

private:
    std::vector<glm::vec2> points_;
    std::vector<float> cumulated_;
    float length_;
};

// Push a point out of a disk: a point closer to center than radius
// is placed on the circle, in its radial direction.
// (a point exactly on center is pushed upward)
glm::vec2 pushRadial (glm::vec2 p, glm::vec2 center, float radius);

// Push a point out of a rift: a band of given half-gap around the segment
// centered on 'center', of direction 'angle' and of given half-length.
// A point inside the band is placed on the border of its side.
// (a point exactly on the axis is pushed on the positive side)
glm::vec2 pushRift (glm::vec2 p, glm::vec2 center, float angle, float halflength, float halfgap);

}

#endif // AUTOMATONMATH_H
