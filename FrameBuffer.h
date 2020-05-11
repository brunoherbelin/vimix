#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "Scene.h"
#include "RenderingManager.h"

class FrameBuffer {

public:
    // size descriptions
    static const char* aspect_ratio_name[4];
    static glm::vec2 aspect_ratio_size[4];
    static const char* resolution_name[4];
    static float resolution_height[4];

    typedef enum {
        AR_4_3 = 0,
        AR_3_2,
        AR_16_10,
        AR_16_9
    } ResolutionAspectRatio;

    typedef enum {
        RES_720 = 0,
        RES_1080,
        RES_1440,
        RES_2160
    } ResolutionHeight;


    FrameBuffer(ResolutionAspectRatio aspect_ratio = AR_16_9, ResolutionHeight height = RES_720);
    FrameBuffer(glm::vec3 resolution);
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
    inline void setClearColor(glm::vec4 color) { attrib_.clear_color = color; }
    inline glm::vec4 clearColor() const { return attrib_.clear_color; }

    // width & height
    inline uint width() const { return attrib_.viewport.x; }
    inline uint height() const { return attrib_.viewport.y; }
    glm::vec3 resolution() const;
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
