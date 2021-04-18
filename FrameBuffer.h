#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "RenderingManager.h"

#define FBI_JPEG_QUALITY 90

/**
 * @brief The FrameBufferImage class stores an RGB image in RAM
 * Direct access to rgb array, and exchange format to JPEG in RAM
 */
class FrameBufferImage
{
public:
    uint8_t *rgb = nullptr;
    int width;
    int height;

    struct jpegBuffer {
        unsigned char *buffer = nullptr;
        uint len = 0;
    };
    jpegBuffer getJpeg();

    FrameBufferImage(int w, int h);
    FrameBufferImage(jpegBuffer jpgimg);
    // non assignable class
    FrameBufferImage(FrameBufferImage const&) = delete;
    FrameBufferImage& operator=(FrameBufferImage const&) = delete;
    ~FrameBufferImage();
};

/**
 * @brief The FrameBuffer class holds an OpenGL Frame Buffer Object.
 */
class FrameBuffer {

public:
    // size descriptions
    static const char* aspect_ratio_name[5];
    static glm::vec2 aspect_ratio_size[5];
    static const char* resolution_name[4];
    static float resolution_height[4];
    static glm::vec3 getResolutionFromParameters(int ar, int h);
    static glm::ivec2 getParametersFromResolution(glm::vec3 res);
    // unbind any framebuffer object
    static void release();

    FrameBuffer(glm::vec3 resolution, bool useAlpha = false, bool multiSampling = false);
    FrameBuffer(uint width, uint height, bool useAlpha = false, bool multiSampling = false);
    FrameBuffer(FrameBuffer const&) = delete;
    ~FrameBuffer();

    // Bind & push attribs to prepare draw
    void begin(bool clear = true);
    // pop attrib and unbind to end draw
    void end();
    // blit copy to another, returns true on success
    bool blit(FrameBuffer *destination);
    // bind the FrameBuffer in READ and perform glReadPixels
    // (to be used after preparing a target PBO)
    void readPixels(uint8_t* target_data = 0);

    // clear color
    inline void setClearColor(glm::vec4 color) { attrib_.clear_color = color; }
    inline glm::vec4 clearColor() const { return attrib_.clear_color; }

    // width & height
    inline uint width() const { return attrib_.viewport.x; }
    inline uint height() const { return attrib_.viewport.y; }
    glm::vec3 resolution() const;
    float aspectRatio() const;
    std::string info() const;

    // projection area (crop)
    glm::mat4 projection() const;
    glm::vec2 projectionArea() const;
    void setProjectionArea(glm::vec2 c);

    // internal pixel format
    inline bool use_alpha() const { return use_alpha_; }
    inline bool use_multisampling() const { return use_multi_sampling_; }

    // index for texturing
    uint texture() const;

    // get and fill image
    FrameBufferImage *image();
    bool fill(FrameBufferImage *image);

private:
    void init();
    void checkFramebufferStatus();

    RenderingAttrib attrib_;
    glm::mat4 projection_;
    glm::vec2 projection_area_;
    uint textureid_, intermediate_textureid_;
    uint framebufferid_, intermediate_framebufferid_;
    bool use_alpha_, use_multi_sampling_;

};



#endif // FRAMEBUFFER_H
