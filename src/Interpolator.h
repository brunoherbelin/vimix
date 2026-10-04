#ifndef INTERPOLATOR_H
#define INTERPOLATOR_H

#include <cstdint>

#include "Source/Source.h"
#include "Source/SourceList.h"

//
// SourceCoreField : the independent properties of a SourceCore
// that can be compared, copied and interpolated one by one
// (derived values, e.g. depth of the mixing node or icon scale, are excluded)
//
namespace SourceCoreField
{
    enum Field {
        MIXING_POSITION = 0,
        GEOMETRY_POSITION,
        GEOMETRY_SCALE,
        GEOMETRY_ROTATION,
        GEOMETRY_CROP,
        GEOMETRY_NODES,
        LAYER_DEPTH,
        TEXTURE_POSITION,
        TEXTURE_SCALE,
        TEXTURE_ROTATION,
        COLOR_BRIGHTNESS,
        COLOR_CONTRAST,
        COLOR_SATURATION,
        COLOR_HUESHIFT,
        COLOR_THRESHOLD,
        COLOR_GAMMA,
        COLOR_LEVELS,
        COLOR_POSTERIZE,
        COLOR_INVERT,
        FIELD_COUNT
    };

    typedef uint32_t Mask;
    constexpr Mask bit (Field f) { return Mask(1) << f; }
    constexpr Mask ALL = (Mask(1) << FIELD_COUNT) - 1;
    // fields applied when rendering the content of the source (in its frame buffer)
    constexpr Mask COLOR = ALL & ~( bit(COLOR_BRIGHTNESS) - 1 );
    constexpr Mask CONTENT = COLOR | bit(GEOMETRY_CROP) | bit(TEXTURE_POSITION)
                             | bit(TEXTURE_SCALE) | bit(TEXTURE_ROTATION);

    bool equal (const SourceCore &a, const SourceCore &b, Field f);
    void copy (SourceCore &dst, const SourceCore &src, Mask m = ALL);
    void mix (SourceCore &dst, const SourceCore &a, const SourceCore &b, float t, Mask m = ALL);

    // mask of fields that are different between a and b
    Mask diff (const SourceCore &a, const SourceCore &b, Mask m = ALL);

    // 3-way merge: fields not modified in draft (i.e. equal to base) follow live
    // returns true if draft was changed
    bool mergeUntouched (SourceCore &draft, SourceCore &base, const SourceCore &live);
}

class SourceInterpolator
{
public:
    SourceInterpolator(Source *subject, const SourceCore &target,
                       SourceCoreField::Mask mask = SourceCoreField::ALL);

    void apply (float percent);
    float current() const;
    inline Source *subject() const { return subject_; }

protected:
    Source *subject_;

    SourceCore from_;
    SourceCore to_;
    SourceCoreField::Mask mask_;
    float current_cursor_;
    bool started_;
};

class Interpolator
{
public:
    Interpolator();
    ~Interpolator();

    void clear ();
    void add (Source *s, const SourceCore &target,
              SourceCoreField::Mask mask = SourceCoreField::ALL);
    void remove (Source *s);
    inline bool empty () const { return interpolators_.empty(); }

    void apply (float percent);
    float current() const;

protected:
    std::list<SourceInterpolator *> interpolators_;

};

#endif // INTERPOLATOR_H
