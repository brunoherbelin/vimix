#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "RenderingManager.h"

#define FBI_JPEG_QUALITY 90
#define MIPMAP_LEVEL 7

/**
 * @brief The FrameBufferImage class stores an RGB image in RAM
 * Direct access to rgb array, and exchange format to JPEG in RAM
 */
class FrameBufferImage
{
public:
    uint8_t *rgb;
    int width;
    int height;
    bool is_stbi;

    struct jpegBuffer {
        unsigned char *buffer = nullptr;
        uint len = 0;
    };
    jpegBuffer getJpeg() const;

    FrameBufferImage(int w, int h);
    FrameBufferImage(jpegBuffer jpgimg);
    FrameBufferImage(const std::string &filename);
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

    enum FrameBufferCreationFlags_
    {
        FrameBuffer_rgb            = 0,
        FrameBuffer_alpha          = 1 << 1,
        FrameBuffer_multisampling  = 1 << 2,
        FrameBuffer_mipmap         = 1 << 3
    };
    typedef int FrameBufferFlags;

    FrameBuffer(glm::vec3 resolution, FrameBufferFlags flags = FrameBuffer_rgb);
    FrameBuffer(uint width, uint height, FrameBufferFlags flags = FrameBuffer_rgb);
    FrameBuffer(FrameBuffer const&) = delete;
    ~FrameBuffer();

    // Bind & push attribs to prepare draw
    void begin(bool clear = true);
    // pop attrib and unbind to end draw
    void end();
    // unbind (any) framebuffer object
    static void release();
    // blit copy to another, returns true on success
    bool blit(FrameBuffer *destination);
    // bind the FrameBuffer in READ and perform glReadPixels
    // (to be used after preparing a target PBO)
    void readPixels();

    // clear color
    inline void setClearColor(glm::vec4 color) { attrib_.clear_color = color; }
    inline glm::vec4 clearColor() const { return attrib_.clear_color; }

    // width & height
    inline uint width() const { return attrib_.viewport.x; }
    inline uint height() const { return attrib_.viewport.y; }
    glm::vec3 resolution() const;
    void resize(glm::vec3 res);
    float aspectRatio() const;
    std::string info() const;

    // projection area (crop)
    glm::mat4 projection() const;
    glm::vec2 projectionArea() const;
    void setProjectionArea(glm::vec2 c);

    // internal pixel format
    inline uint opengl_id() const { return framebufferid_; }
    inline FrameBufferFlags flags() const { return flags_; }

    // index for texturing
    uint texture() const;

    // get and fill image
    FrameBufferImage *image();
    bool fill(FrameBufferImage *image);

private:
    void init();
    void reset();
    bool checkFramebufferStatus();

    FrameBufferFlags flags_;
    RenderingAttrib attrib_;
    glm::mat4 projection_;
    glm::vec2 projection_area_;
    uint textureid_, multisampling_textureid_;
    uint framebufferid_, multisampling_framebufferid_;
    uint mem_usage_;
};



#endif // FRAMEBUFFER_H
