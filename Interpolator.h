#ifndef INTERPOLATOR_H
#define INTERPOLATOR_H

#include "Source.h"

class Interpolator
{
public:
    Interpolator(Source *subject, const SourceCore &target);

    Source *subject_;

    SourceCore from_;
    SourceCore to_;
    SourceCore current_;

    float cursor_;

    void apply(float percent);

};

#endif // INTERPOLATOR_H
