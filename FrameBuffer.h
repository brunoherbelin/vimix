#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "Scene.h"


class FrameBuffer {

public:
    FrameBuffer(uint width, uint height, bool useDepthBuffer = false);
    ~FrameBuffer();

    // bind the FrameBuffer as current to draw into
    void bind();
    // releases the framebuffer object
    static void release();
    // blit copy to another, returns true on success
    bool blit(FrameBuffer *other);

    inline uint width() const { return width_; }
    inline uint height() const { return height_; }
    inline uint texture() const { return textureid_; }
    float aspectRatio() const;

private:
    void checkFramebufferStatus();
    uint width_;
    uint height_;
    uint textureid_;
    uint framebufferid_;
};



#endif // FRAMEBUFFER_H
