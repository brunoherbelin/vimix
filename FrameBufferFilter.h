#ifndef FRAMEBUFFERFILTER_H
#define FRAMEBUFFERFILTER_H

#include <sys/types.h>
#include <glm/glm.hpp>

class Visitor;
class FrameBuffer;

class FrameBufferFilter
{
    bool enabled_;

public:

    FrameBufferFilter();
    virtual ~FrameBufferFilter() {}

    // types of sub-classes of filters
    typedef enum {
        FILTER_PASSTHROUGH = 0,
        FILTER_DELAY,
        FILTER_RESAMPLE,
        FILTER_BLUR,
        FILTER_SHARPEN,
        FILTER_EDGE,
        FILTER_ALPHA,
        FILTER_IMAGE,
        FILTER_INVALID
    } Type;
    static const char* type_label[FILTER_INVALID];
    virtual Type type () const = 0;

    // get the texture id of the rendered filtered framebuffer
    // when not enabled, getOutputTexture should return InputTexture
    virtual uint texture () const = 0;

    // get the resolution of the rendered filtered framebuffer
    virtual glm::vec3 resolution () const = 0;

    // perform update (non rendering)
    virtual void update (float dt) {}

    // draw the input framebuffer and apply the filter
    virtual void draw (FrameBuffer *input);

    // visitor for UI and XML
    virtual void accept (Visitor& v);

    // when enabled, draw is effective
    inline void setEnabled (bool on) { enabled_ = on; }
    inline bool enabled () const { return enabled_; }

protected:
    FrameBuffer *input_;
};

class PassthroughFilter : public FrameBufferFilter
{
public:

    PassthroughFilter();

    // implementation of FrameBufferFilter
    Type type() const override { return FrameBufferFilter::FILTER_PASSTHROUGH; }
    uint texture() const override;
    glm::vec3 resolution() const override;
};


#endif // FRAMEBUFFERFILTER_H
