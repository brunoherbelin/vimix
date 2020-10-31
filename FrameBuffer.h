#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "Scene.h"
#include "RenderingManager.h"

class FrameBuffer {

public:
    // size descriptions
    static const char* aspect_ratio_name[5];
    static glm::vec2 aspect_ratio_size[5];
    static const char* resolution_name[4];
    static float resolution_height[4];
    static glm::vec3 getResolutionFromParameters(int ar, int h);
    // unbind any framebuffer object
    static void release();

    FrameBuffer(glm::vec3 resolution, bool useAlpha = false, bool multiSampling = false);
    FrameBuffer(uint width, uint height, bool useAlpha = false, bool multiSampling = false);
    ~FrameBuffer();

    // Bind & push attribs to prepare draw
    void begin();
    // pop attrib and unbind to end draw
    void end();

    // blit copy to another, returns true on success
    bool blit(FrameBuffer *other);
    // bind the FrameBuffer in READ and perform glReadPixels
    // return the size of the buffer
    void readPixels();

    // clear color
    inline void setClearColor(glm::vec4 color) { attrib_.clear_color = color; }
    inline glm::vec4 clearColor() const { return attrib_.clear_color; }

    // width & height
    inline uint width() const { return attrib_.viewport.x; }
    inline uint height() const { return attrib_.viewport.y; }
    glm::vec3 resolution() const;
    float aspectRatio() const;
    std::string info() const;

    // internal pixel format
    inline bool use_alpha() const { return use_alpha_; }
    inline bool use_multisampling() const { return use_multi_sampling_; }

    // index for texturing
    uint texture() const;

private:
    void init();
    void checkFramebufferStatus();

    RenderingAttrib attrib_;
    uint textureid_, intermediate_textureid_;
    uint framebufferid_, intermediate_framebufferid_;
    bool use_alpha_, use_multi_sampling_;
};



#endif // FRAMEBUFFER_H
