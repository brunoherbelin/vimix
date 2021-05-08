#ifndef INTERPOLATOR_H
#define INTERPOLATOR_H

#include "Source.h"
#include "SourceList.h"

class SourceInterpolator
{
public:
    SourceInterpolator(Source *subject, const SourceCore &target);

    void apply (float percent);
    float current() const;

protected:
    Source *subject_;

    SourceCore from_;
    SourceCore to_;
    SourceCore current_state_;
    float current_cursor_;

    void interpolateGroup (View::Mode m);
    void interpolateImageProcessing ();
};

class Interpolator
{
public:
    Interpolator();
    ~Interpolator();

    void add (Source *s, const SourceCore &target );

    void apply (float percent);
    float current() const;

protected:
    std::list<SourceInterpolator *> interpolators_;

};

#endif // INTERPOLATOR_H
