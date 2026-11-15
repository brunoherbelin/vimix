// Standalone prototype test of the math of automatons (no vimix, no OpenGL)
//
/*
   Build and run from the root of the repository:

   g++ -std=c++17 -Iext/glm -Isrc/Automaton tests/automaton/test_automaton_math.cpp src/Automaton/AutomatonMath.cpp -o /tmp/test_automaton_math && /tmp/test_automaton_math
*/
#include <cstdio>
#include <cmath>
#include <vector>

#include "AutomatonMath.h"

using namespace AutomatonMath;

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { ++checks; if (!(cond)) { ++failures; \
    std::printf("FAILED line %d: %s\n", __LINE__, #cond); } } while (0)

static bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) < eps; }
static bool near(glm::vec2 a, glm::vec2 b, float eps = 1e-4f) { return glm::length(a - b) < eps; }

static void test_timing()
{
    // progress is in [0 1[ and periodic, for integer and fractional periods
    const double periods[] = { 0.25, 0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 8.0 };
    for (double p : periods) {
        for (double b = -20.0; b < 20.0; b += 0.013) {
            double u = progress(b, p);
            CHECK(u >= 0.0 && u < 1.0);
            double u2 = progress(b + 3.0 * p, p);
            CHECK(std::fabs(u - u2) < 1e-6 || std::fabs(std::fabs(u - u2) - 1.0) < 1e-6);
        }
    }
    // known values
    CHECK(near(progress(0.0, 2.0), 0.f));
    CHECK(near(progress(1.0, 2.0), 0.5f));
    CHECK(near(progress(3.0, 2.0), 0.5f));
    CHECK(near(progress(0.125, 0.5), 0.25f));
    // negative beats (count-in of Ableton Link)
    CHECK(near(progress(-0.5, 2.0), 0.75f));
    // offset shifts the cycle
    CHECK(near(progress(1.0, 4.0, 1.0), 0.f));
    CHECK(near(progress(2.0, 4.0, 1.0), 0.25f));
    // cycle count
    CHECK(cycle(5.0, 2.0) == 2.0);
    CHECK(cycle(-0.5, 2.0) == -1.0);
    // two automatons of 2 and 4 beats: the first does two cycles during one of the second
    for (double b = 0.0; b < 16.0; b += 0.1) {
        double u2 = progress(b, 2.0), u4 = progress(b, 4.0);
        double twice = 2.0 * u4 - std::floor(2.0 * u4);
        CHECK(std::fabs(u2 - twice) < 1e-6 || std::fabs(std::fabs(u2 - twice) - 1.0) < 1e-6);
    }
}

static void test_shapes()
{
    const int shapes[] = { SHAPE_LINEAR, SHAPE_SMOOTH, SHAPE_PULSE };
    for (int s : shapes) {
        // ramp 0 -> 1 monotonic
        CHECK(near(ramp(0.f, s), 0.f));
        CHECK(near(ramp(1.f, s), 1.f));
        float prev = 0.f;
        for (float u = 0.f; u <= 1.f; u += 0.01f) {
            float r = ramp(u, s);
            CHECK(r >= prev - 1e-5f && r >= 0.f && r <= 1.f);
            prev = r;
        }
        // wave 0 -> 1 -> 0, maximum at peak
        CHECK(near(wave(0.f, s), 0.f));
        CHECK(near(wave(1.f, s), 0.f));
        CHECK(near(wave(wavePeak(s), s), 1.f));
        for (float u = 0.f; u <= 1.f; u += 0.01f) {
            float w = wave(u, s);
            CHECK(w >= -1e-5f && w <= 1.f + 1e-5f);
        }
    }
    // back and forth is symmetric for linear and smooth
    for (float u = 0.f; u <= 0.5f; u += 0.01f) {
        CHECK(near(wave(u, SHAPE_LINEAR), wave(1.f - u, SHAPE_LINEAR)));
        CHECK(near(wave(u, SHAPE_SMOOTH), wave(1.f - u, SHAPE_SMOOTH)));
    }
    // linear bounce at half cycle
    CHECK(near(wave(0.25f, SHAPE_LINEAR), 0.5f));
    CHECK(near(wave(0.75f, SHAPE_LINEAR), 0.5f));
}

static void test_paths()
{
    // ellipse
    glm::vec2 c(1.f, 2.f), r(2.f, 0.5f);
    CHECK(near(ellipse(c, r, 0.f), glm::vec2(3.f, 2.f)));
    CHECK(near(ellipse(c, r, 0.25f), glm::vec2(1.f, 2.5f)));
    CHECK(near(ellipse(c, r, 0.5f), glm::vec2(-1.f, 2.f)));
    CHECK(near(ellipse(c, r, 1.f), ellipse(c, r, 0.f), 1e-3f));

    // smooth path passes through its keys
    std::vector<glm::vec2> keys = { {0.f, 0.f}, {1.f, 1.f}, {2.f, -0.5f}, {3.5f, 0.2f} };
    std::vector<size_t> idx;
    std::vector<glm::vec2> path = smoothPath(keys, 128, &idx);
    CHECK(path.size() == 128);
    CHECK(idx.size() == keys.size());
    for (size_t k = 0; k < keys.size(); ++k)
        CHECK(near(path[idx[k]], keys[k], 0.05f));
    CHECK(near(path.front(), keys.front()));
    CHECK(near(path.back(), keys.back()));
    // no NaN, and continuity (no jump between samples)
    for (size_t i = 1; i < path.size(); ++i) {
        CHECK(path[i].x == path[i].x && path[i].y == path[i].y);
        CHECK(glm::length(path[i] - path[i-1]) < 0.2f);
    }
    // two keys only : straight line
    std::vector<glm::vec2> line = smoothPath({ {0.f, 0.f}, {2.f, 0.f} }, 16);
    for (auto &p : line)
        CHECK(near(p.y, 0.f));

    // constant speed along the path
    ArcPath arc(path);
    CHECK(arc.length() > 0.f);
    CHECK(near(arc.at(0.f), keys.front()));
    CHECK(near(arc.at(1.f), keys.back()));
    const int N = 400;
    const float step = arc.length() / float(N);
    for (int i = 1; i <= N; ++i) {
        float d = glm::length( arc.at(float(i) / float(N)) - arc.at(float(i-1) / float(N)) );
        // chord is never longer than the arc, and close to it
        CHECK(d <= step * 1.001f);
        CHECK(d > step * 0.8f);
    }
    CHECK(near(arc.fraction(0), 0.f));
    CHECK(near(arc.fraction(path.size() - 1), 1.f));
    // degenerate paths
    ArcPath empty;
    CHECK(near(empty.at(0.5f), glm::vec2(0.f)));
    ArcPath single( std::vector<glm::vec2>{ {1.f, 1.f} } );
    CHECK(near(single.at(0.5f), glm::vec2(1.f, 1.f)));
}

static void test_heart()
{
    const glm::vec2 C(0.2f, -0.1f);
    const float r0 = 0.3f, r1 = 1.2f;
    // sources at rest: inside disk, between disk and max, outside, and exactly on center
    const std::vector<glm::vec2> rest = { C + glm::vec2(0.1f, 0.f), C + glm::vec2(-0.4f, 0.4f),
                                          C + glm::vec2(0.f, 2.f), C, C + glm::vec2(0.9f, -0.1f) };
    for (int s = 0; s < 3; ++s) {
        for (auto &p0 : rest) {
            float maxdist = 0.f;
            for (float u = 0.f; u <= 1.f; u += 0.005f) {
                float R = r0 + (r1 - r0) * wave(u, s);
                glm::vec2 p = pushRadial(p0, C, R);
                float d = glm::length(p - C);
                // never inside the disk, never inside the ring
                CHECK(d >= r0 - 1e-4f);
                CHECK(d >= R - 1e-4f);
                // never pushed beyond the ring if it was inside
                CHECK(d <= std::max(R, glm::length(p0 - C)) + 1e-4f);
                // pushed along its radial direction
                if (glm::length(p0 - C) > 1e-3f)
                    CHECK(near(glm::normalize(p - C), glm::normalize(p0 - C), 1e-3f));
                maxdist = std::max(maxdist, d);
            }
            // at the end of the cycle, restored at rest (or on the disk border)
            glm::vec2 end = pushRadial(p0, C, r0);
            if (glm::length(p0 - C) >= r0)
                CHECK(near(end, p0));
            else
                CHECK(near(glm::length(end - C), r0));
            // at the peak, pushed at max radius (unless already further)
            CHECK(near(maxdist, std::max(r1, glm::length(p0 - C)), 1e-2f));
        }
    }
    // pushing is idempotent
    glm::vec2 p = pushRadial(C + glm::vec2(0.1f, 0.1f), C, 0.5f);
    CHECK(near(pushRadial(p, C, 0.5f), p));
}

static void test_rift()
{
    const glm::vec2 C(0.f, 0.f);
    const float L = 1.f;
    // horizontal rift
    CHECK(near(pushRift({0.2f, 0.1f}, C, 0.f, L, 0.5f), glm::vec2(0.2f, 0.5f)));
    CHECK(near(pushRift({0.2f, -0.1f}, C, 0.f, L, 0.5f), glm::vec2(0.2f, -0.5f)));
    // on the axis : positive side
    CHECK(near(pushRift({0.2f, 0.f}, C, 0.f, L, 0.5f), glm::vec2(0.2f, 0.5f)));
    // outside the gap or beyond the ends : not affected
    CHECK(near(pushRift({0.2f, 0.7f}, C, 0.f, L, 0.5f), glm::vec2(0.2f, 0.7f)));
    CHECK(near(pushRift({1.5f, 0.1f}, C, 0.f, L, 0.5f), glm::vec2(1.5f, 0.1f)));
    // closed rift does nothing
    CHECK(near(pushRift({0.2f, 0.1f}, C, 0.f, L, 0.f), glm::vec2(0.2f, 0.1f)));
    // vertical rift pushes horizontally
    const float a = 3.14159265f / 2.f;
    glm::vec2 q = pushRift({0.1f, 0.3f}, C, a, L, 0.4f);
    CHECK(near(q, glm::vec2(-0.4f, 0.3f), 1e-3f) || near(q, glm::vec2(0.4f, 0.3f), 1e-3f));
    CHECK(near(q.y, 0.3f, 1e-3f));
    // symmetric, and restored when the rift closes
    for (float angle = 0.f; angle < 6.28f; angle += 0.37f) {
        glm::vec2 n(-std::sin(angle), std::cos(angle));
        glm::vec2 a0 = C + n * 0.1f, b0 = C - n * 0.1f;
        for (float u = 0.f; u <= 1.f; u += 0.01f) {
            float g = 0.6f * wave(u, SHAPE_SMOOTH);
            glm::vec2 pa = pushRift(a0, C, angle, L, g), pb = pushRift(b0, C, angle, L, g);
            CHECK(near(pa - C, -(pb - C), 1e-3f));
            CHECK(std::fabs(glm::dot(pa - C, n)) >= std::max(g, 0.1f) - 1e-3f);
        }
        CHECK(near(pushRift(a0, C, angle, L, 0.f), a0));
    }
}

static void test_composition()
{
    // Rest pose composition, as done by the rig every frame:
    // the pose of a source is always recomputed from its base pose,
    // so that bots never accumulate errors and removing them restores the base.
    const glm::vec2 base(0.3f, 0.2f);
    const glm::vec2 A(-1.f, 0.f), B(1.f, 0.5f);     // linear bot
    const glm::vec2 anchor = A;                     // source attached when mover is on A
    const glm::vec2 C(0.5f, 0.3f);                  // heart
    glm::vec2 node = base;
    for (double beats = 0.0; beats < 64.0; beats += 0.0166) {
        // linear bot of period 2 beats, heart of period 1 beat
        glm::vec2 mover = glm::mix(A, B, wave(float(progress(beats, 2.0)), SHAPE_LINEAR));
        float R = 0.2f + 0.6f * wave(float(progress(beats, 1.0)), SHAPE_PULSE);
        node = base;
        node += mover - anchor;
        node = pushRadial(node, C, R);
        CHECK(glm::length(node - C) >= R - 1e-4f);
    }
    // on the beat, the linear bot is back to A, the heart is at rest
    {
        glm::vec2 mover = glm::mix(A, B, wave(float(progress(64.0, 2.0)), SHAPE_LINEAR));
        float R = 0.2f + 0.6f * wave(float(progress(64.0, 1.0)), SHAPE_PULSE);
        node = pushRadial(base + mover - anchor, C, R);
        CHECK(near(node, pushRadial(base, C, 0.2f)));
    }
    // bots removed : exactly the base pose
    node = base;
    CHECK(node == base);
}

int main()
{
    test_timing();
    test_shapes();
    test_paths();
    test_heart();
    test_rift();
    test_composition();

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
