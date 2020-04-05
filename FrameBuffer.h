#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "Scene.h"
#include "RenderingManager.h"

class FrameBuffer {

public:
    FrameBuffer(uint width, uint height, bool useDepthBuffer = false);
    ~FrameBuffer();

    // bind the FrameBuffer as current to draw into
    void bind();
    void begin();
    void end();

    // release any framebuffer object
    static void release();

    // blit copy to another, returns true on success
    bool blit(FrameBuffer *other);

    inline uint width() const { return attrib_.viewport.x; }
    inline uint height() const { return attrib_.viewport.y; }
    inline uint texture() const { return textureid_; }
    float aspectRatio() const;

private:
    void checkFramebufferStatus();
    RenderingAttrib attrib_;
    uint textureid_;
    uint framebufferid_;
};



#endif // FRAMEBUFFER_H
