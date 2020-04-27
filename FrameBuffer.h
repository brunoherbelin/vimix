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
    // unbind any framebuffer object
    static void release();
    // Bind & push attribs to prepare draw
    void begin();
    // pop attrib and unbind to end draw
    void end();

    // blit copy to another, returns true on success
    bool blit(FrameBuffer *other);

    // clear color
    inline void setClearColor(glm::vec3 color) { attrib_.clear_color = color; }
    inline glm::vec3 clearColor() const { return attrib_.clear_color; }

    // width & height
    inline uint width() const { return attrib_.viewport.x; }
    inline uint height() const { return attrib_.viewport.y; }
    float aspectRatio() const;
    // texture index for draw
    uint texture() const;

private:
    void init();
    void checkFramebufferStatus();

    RenderingAttrib attrib_;
    uint textureid_;
    uint framebufferid_;
    bool usedepth_;
};



#endif // FRAMEBUFFER_H
