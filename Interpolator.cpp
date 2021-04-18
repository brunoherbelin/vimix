#include "Interpolator.h"

Interpolator::Interpolator(Source *subject, const SourceCore &target) :
    subject_(subject), cursor_(0.f)
{
   from_ = static_cast<SourceCore> (*subject);
   to_ = target;

}
