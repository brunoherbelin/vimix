#include "FrameBuffer.h"
#include "Resource.h"
#include "Visitor.h"

#include "FrameBufferFilter.h"
#include "FrameBufferFilter.h"

const char* FrameBufferFilter::type_label[FrameBufferFilter::FILTER_INVALID] = {
    "None", "Delay", "Blur", "Sharpen", "Edge", "Shader code"
};

FrameBufferFilter::FrameBufferFilter() : enabled_(true), input_(nullptr)
{

}

void FrameBufferFilter::draw (FrameBuffer *input)
{
    input_ = input;
}

void FrameBufferFilter::accept(Visitor& v)
{
    if (input_)
        v.visit(*this);
}

PassthroughFilter::PassthroughFilter() : FrameBufferFilter()
{

}

uint PassthroughFilter::texture() const
{
    if (input_)
        return input_->texture();
    else
        return Resource::getTextureBlack();
}

glm::vec3 PassthroughFilter::resolution() const
{
    if (input_)
        return input_->resolution();
    else
        return glm::vec3(1,1,0);
}
