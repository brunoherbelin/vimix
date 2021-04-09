#ifndef RENDERVIEW_H
#define RENDERVIEW_H

#include "View.h"

class RenderView : public View
{
    FrameBuffer *frame_buffer_;
    Surface *fading_overlay_;

public:
    RenderView ();
    ~RenderView ();

    void draw () override;
    bool canSelect(Source *) override;

    void setResolution (glm::vec3 resolution = glm::vec3(0.f), bool useAlpha = false);
    glm::vec3 resolution() const { return frame_buffer_->resolution(); }

    void setFading(float f = 0.f);
    float fading() const;

    inline FrameBuffer *frame () const { return frame_buffer_; }
};

#endif // RENDERVIEW_H
