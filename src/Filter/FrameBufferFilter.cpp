#include "FrameBuffer.h"
#include "Resource.h"
#include "Visitor/Visitor.h"

#include "FrameBufferFilter.h"

std::vector< std::tuple<int, int, std::string> > FrameBufferFilter::Types = {
    { ICON_FILTER_NONE, std::string("None") },
    { ICON_FILTER_DELAY, std::string("Delay") },
    { ICON_FILTER_RESAMPLE, std::string("Resample") },
    { ICON_FILTER_BLUR, std::string("Blur") },
    { ICON_FILTER_SHARPEN, std::string("Sharpen") },
    { ICON_FILTER_SMOOTH, std::string("Smooth & Noise") },
    { ICON_FILTER_EDGE, std::string("Edge") },
    { ICON_FILTER_ALPHA, std::string("Alpha") },
    { ICON_FILTER_IMAGE, std::string("Custom shader") }
};

FrameBufferFilter::FrameBufferFilter() : enabled_(true), input_(nullptr)
{

}

void FrameBufferFilter::draw (FrameBuffer *input)
{
    if (input && ( enabled_ || input_ == nullptr ) )
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
