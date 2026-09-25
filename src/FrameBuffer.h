#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "RenderingManager.h"

#ifndef NDEBUG
#define FRAMEBUFFER_DEBUG
#endif

#define FBI_JPEG_QUALITY 90
#define MIPMAP_LEVEL 7

// Smallest framebuffer accepted (pixels, per axis)
#define FRAMEBUFFER_MIN_SIZE 2
// Memory a single framebuffer may use when the GPU RAM is unknown (Bytes)
#define FRAMEBUFFER_MAX_MEMORY 1000000000UL

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
    // returns false if the frame buffer could not be allocated:
    // in that case nothing is bound and end() shall NOT be called
    bool begin(bool clear = true);
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
    glm::vec2 projectionSize() const;
    glm::vec4 projectionArea() const;
    void setProjectionArea(glm::vec4 c);

    // internal pixel format
    inline uint opengl_id() const { return framebufferid_; }
    inline FrameBufferFlags flags() const { return flags_; }

    // true when the frame buffer is allocated and can be drawn into
    inline bool isValid() const { return framebufferid_ > 0; }

    // index for texturing
    uint texture() const;

    // get and fill image
    FrameBufferImage *image();
    bool fill(FrameBufferImage *image);

    // how much memory is used by all frame buffers, in Bytes
    static unsigned long memory_usage();

    // estimated GPU memory, in Bytes, needed for such a frame buffer
    // (0 if the resolution is not finite or smaller than one pixel)
    static unsigned long memoryUsage(glm::vec3 resolution, FrameBufferFlags flags);
    // maximum resolution supported by the GPU (GL_MAX_TEXTURE_SIZE)
    static glm::vec3 maxResolution();
    // how much memory, in Bytes, a single frame buffer is allowed to use
    static unsigned long memoryBudget();
    // false if the resolution is not finite, too small, larger than
    // maxResolution(), or needs more memory than memoryBudget()
    static bool isValidResolution(glm::vec3 resolution, FrameBufferFlags flags);

#ifdef FRAMEBUFFER_DEBUG
    // list all frame buffers still alive (debug builds only)
    static void dumpRegistry();
#endif

private:
    void init();
    void reset();
    bool checkFramebufferStatus();

    FrameBufferFlags flags_;
    GlmToolkit::RenderingAttrib attrib_;
    glm::mat4 projection_;
    glm::vec4 projection_area_;
    uint textureid_, multisampling_textureid_;
    uint framebufferid_, multisampling_framebufferid_;
    bool failed_;
    unsigned long mem_usage_;
    static unsigned long total_mem_usage;
};



#endif // FRAMEBUFFER_H
