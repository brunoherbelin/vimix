#ifndef FRAMEBUFFERFILTER_H
#define FRAMEBUFFERFILTER_H

#include <string>
#include <vector>
#include <sys/types.h>
#include <glm/glm.hpp>

class Visitor;
class FrameBuffer;

#define ICON_FILTER_NONE 7, 11
#define ICON_FILTER_DELAY 10, 15
#define ICON_FILTER_RESAMPLE 1, 10
#define ICON_FILTER_BLUR 0, 9
#define ICON_FILTER_SHARPEN 2, 1
#define ICON_FILTER_SMOOTH 14, 8
#define ICON_FILTER_EDGE 16, 8
#define ICON_FILTER_ALPHA 13, 4
#define ICON_FILTER_IMAGE 1, 4

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
        FILTER_SMOOTH,
        FILTER_EDGE,
        FILTER_ALPHA,
        FILTER_IMAGE,
        FILTER_INVALID
    } Type;
    static std::vector< std::tuple<int, int, std::string> > Types;
    virtual Type type () const = 0;

    // get the texture id of the rendered filtered framebuffer
    // when not enabled, getOutputTexture should return InputTexture
    virtual uint texture () const = 0;

    // get the resolution of the rendered filtered framebuffer
    virtual glm::vec3 resolution () const = 0;

    // perform update (non rendering), given dt in milisecond
    virtual void update (float) {}

    // reset update data and time
    virtual void reset () {}

    // total time of update, in second
    virtual double updateTime () { return 0.; }

    // draw the input framebuffer and apply the filter
    virtual void draw (FrameBuffer *input);

    // visitor for UI and XML
    virtual void accept (Visitor& v);

    // when enabled, draw is effective
    inline  void setEnabled (bool on) { enabled_ = on; }
    inline  bool enabled () const { return enabled_; }

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
