#ifndef DELAYFILTER_H
#define DELAYFILTER_H

#include <queue>
#include <glm/glm.hpp>

#include "FrameBufferFilter.h"

class Surface;
class FrameBuffer;

class DelayFilter : public FrameBufferFilter
{
public:
    DelayFilter();
    ~DelayFilter();

    // delay property
    inline void setDelay(double second) { delay_ = second; }
    inline double delay() const { return delay_; }

    // implementation of FrameBufferFilter
    Type type() const override { return FrameBufferFilter::FILTER_DELAY; }
    uint texture () const override;
    glm::vec3 resolution () const override;
    void update (float dt) override;
    void reset () override;
    double updateTime () override;
    void draw   (FrameBuffer *input) override;
    void accept (Visitor& v) override;

private:
    // queue of frames
    std::queue<FrameBuffer *> frames_;
    std::queue<double> elapsed_;

    // render management
    FrameBuffer *temp_frame_;

    // time management
    double now_;
    double delay_;
};

#endif // DELAYFILTER_H
