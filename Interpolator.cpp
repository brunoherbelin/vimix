
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "defines.h"
#include "Log.h"
#include "ImageProcessingShader.h"

#include "Interpolator.h"


Interpolator::Interpolator(Source *subject, const SourceCore &target) :
    subject_(subject), from_(static_cast<SourceCore> (*subject)), to_(target), cursor_(0.f)
{


}


void Interpolator::apply(float percent)
{
    cursor_ = CLAMP( percent, 0.f, 1.f);


    if (cursor_ < EPSILON)
        current_ = from_;
    else if (cursor_ > 1.f - EPSILON)
        current_ = to_;
    else {
//        current_ =
    }

    subject_->copy(current_);

}
