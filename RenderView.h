#ifndef RENDERVIEW_H
#define RENDERVIEW_H

#include <vector>
#include <future>

#include "View.h"

class RenderView : public View
{
    friend class Session;

    // rendering FBO
    FrameBuffer *frame_buffer_;
    Surface *fading_overlay_;

    // promises of returning thumbnails after an update
    std::vector< std::promise<FrameBufferImage *> > thumbnailer_;

public:
    RenderView ();
    ~RenderView ();

    // render frame (in opengl context)
    void draw () override;
    bool canSelect(Source *) override { return false; }

    void setResolution (glm::vec3 resolution = glm::vec3(0.f), bool useAlpha = false);
    glm::vec3 resolution() const { return frame_buffer_->resolution(); }


    // current frame
    inline FrameBuffer *frame () const { return frame_buffer_; }

protected:

    void setFading(float f = 0.f);
    float fading() const;

    // get a thumbnail outside of opengl context; wait for a promise to be fullfiled after draw
    void drawThumbnail();
    FrameBufferImage *thumbnail ();
    FrameBuffer *frame_thumbnail_;
};

#endif // RENDERVIEW_H
