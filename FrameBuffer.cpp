#include <sstream>

#include "FrameBuffer.h"
#include "ImageShader.h"
#include "Resource.h"
#include "Settings.h"
#include "Log.h"

#include <glm/gtc/matrix_transform.hpp>

#include <glad/glad.h>
#include <stb_image.h>
#include <stb_image_write.h>

const char* FrameBuffer::aspect_ratio_name[5] = { "4:3", "3:2", "16:10", "16:9", "21:9" };
glm::vec2 FrameBuffer::aspect_ratio_size[5] = { glm::vec2(4.f,3.f), glm::vec2(3.f,2.f), glm::vec2(16.f,10.f), glm::vec2(16.f,9.f) , glm::vec2(21.f,9.f) };
const char* FrameBuffer::resolution_name[4] = { "720", "1080", "1440", "2160" };
float FrameBuffer::resolution_height[4] = { 720.f, 1080.f, 1440.f, 2160.f };


glm::vec3 FrameBuffer::getResolutionFromParameters(int ar, int h)
{
    float width = aspect_ratio_size[ar].x * resolution_height[h] / aspect_ratio_size[ar].y;
    glm::vec3 res = glm::vec3( width, resolution_height[h] , 0.f);

    return res;
}

glm::ivec2 FrameBuffer::getParametersFromResolution(glm::vec3 res)
{
    glm::ivec2 p = glm::ivec2(-1);

    // get aspect ratio parameter
    static int num_ar = ((int)(sizeof(FrameBuffer::aspect_ratio_size) / sizeof(*FrameBuffer::aspect_ratio_size)));
    float myratio = res.x / res.y;
    for(int ar = 0; ar < num_ar; ar++) {
        if ( myratio - (FrameBuffer::aspect_ratio_size[ar].x / FrameBuffer::aspect_ratio_size[ar].y ) < EPSILON){
            p.x = ar;
            break;
        }
    }
    // get height parameter
    static int num_height = ((int)(sizeof(FrameBuffer::resolution_height) / sizeof(*FrameBuffer::resolution_height)));
    for(int h = 0; h < num_height; h++) {
        if ( res.y - FrameBuffer::resolution_height[h] < 1){
            p.y = h;
            break;
        }
    }

    return p;
}

FrameBuffer::FrameBuffer(glm::vec3 resolution, bool useAlpha, bool multiSampling):
    textureid_(0), intermediate_textureid_(0), framebufferid_(0), intermediate_framebufferid_(0),
    use_alpha_(useAlpha), use_multi_sampling_(multiSampling)
{
    attrib_.viewport = glm::ivec2(resolution);
    setProjectionArea(glm::vec2(1.f, 1.f));
    attrib_.clear_color = glm::vec4(0.f, 0.f, 0.f, 0.f);
}

FrameBuffer::FrameBuffer(uint width, uint height, bool useAlpha, bool multiSampling):
    textureid_(0), intermediate_textureid_(0), framebufferid_(0), intermediate_framebufferid_(0),
    use_alpha_(useAlpha), use_multi_sampling_(multiSampling)
{
    attrib_.viewport = glm::ivec2(width, height);
    setProjectionArea(glm::vec2(1.f, 1.f));
    attrib_.clear_color = glm::vec4(0.f, 0.f, 0.f, 0.f);
}

void FrameBuffer::init()
{
    // generate texture
    glGenTextures(1, &textureid_);
    glBindTexture(GL_TEXTURE_2D, textureid_);
    glTexStorage2D(GL_TEXTURE_2D, 1, use_alpha_ ? GL_RGBA8 : GL_RGB8, attrib_.viewport.x, attrib_.viewport.y);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // create a framebuffer object
    glGenFramebuffers(1, &framebufferid_);
    glBindFramebuffer(GL_FRAMEBUFFER, framebufferid_);

    // take settings into account: no multisampling for level 0
    use_multi_sampling_ &= Settings::application.render.multisampling > 0;

    if (use_multi_sampling_){

        // create a multisample texture
        glGenTextures(1, &intermediate_textureid_);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, intermediate_textureid_);
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, Settings::application.render.multisampling,
                                use_alpha_ ? GL_RGBA8 : GL_RGB8, attrib_.viewport.x, attrib_.viewport.y, GL_TRUE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);

        // attach the multisampled texture to FBO (framebufferid_  currently binded)
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, intermediate_textureid_, 0);

        // create an intermediate FBO : this is the FBO to use for reading
        glGenFramebuffers(1, &intermediate_framebufferid_);
        glBindFramebuffer(GL_FRAMEBUFFER, intermediate_framebufferid_);

        // attach the 2D texture to intermediate FBO (intermediate_framebufferid_)
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureid_, 0);

//        Log::Info("New FBO %d Multi Sampling ", framebufferid_);

    }
    else {

        // direct attach the 2D texture to FBO
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureid_, 0);

//        Log::Info("New FBO %d Single Sampling ", framebufferid_);
    }

    checkFramebufferStatus();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

}

FrameBuffer::~FrameBuffer()
{
    if (framebufferid_)
        glDeleteFramebuffers(1, &framebufferid_);
    if (intermediate_framebufferid_)
        glDeleteFramebuffers(1, &intermediate_framebufferid_);
    if (textureid_)
        glDeleteTextures(1, &textureid_);
    if (intermediate_textureid_)
        glDeleteTextures(1, &intermediate_textureid_);
}


uint FrameBuffer::texture() const
{
    if (framebufferid_ == 0)
        return Resource::getTextureBlack();

    return textureid_;
}

float FrameBuffer::aspectRatio() const
{
    return static_cast<float>(attrib_.viewport.x) / static_cast<float>(attrib_.viewport.y);
}


std::string FrameBuffer::info() const
{
    glm::ivec2 p = FrameBuffer::getParametersFromResolution(resolution());
    std::ostringstream info;

    info << attrib_.viewport.x << "x" << attrib_.viewport.y;
    if (p.x > -1)
        info << "px, " << FrameBuffer::aspect_ratio_name[p.x];

    return info.str();
}

glm::vec3 FrameBuffer::resolution() const
{
    return glm::vec3(attrib_.viewport.x, attrib_.viewport.y, 0.f);
}

void FrameBuffer::begin(bool clear)
{
    if (!framebufferid_)
        init();

    glBindFramebuffer(GL_FRAMEBUFFER, framebufferid_);

    Rendering::manager().pushAttrib(attrib_);

    if (clear)
        glClear(GL_COLOR_BUFFER_BIT);
}

void FrameBuffer::end()
{    
    // if multisampling frame buffer
    if (use_multi_sampling_) {
        // blit the multisample FBO into unisample FBO to generate 2D texture
        // Doing this blit will automatically resolve the multisampled FBO.
        glBindFramebuffer(GL_READ_FRAMEBUFFER, framebufferid_);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, intermediate_framebufferid_);
        glBlitFramebuffer(0, 0, attrib_.viewport.x, attrib_.viewport.y,
                          0, 0, attrib_.viewport.x, attrib_.viewport.y, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }

    FrameBuffer::release();

    Rendering::manager().popAttrib();
}

void FrameBuffer::release()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FrameBuffer::readPixels(uint8_t *target_data)
{
    if (!framebufferid_)
        return;

    if (use_multi_sampling_)
        glBindFramebuffer(GL_READ_FRAMEBUFFER, intermediate_framebufferid_);
    else
        glBindFramebuffer(GL_READ_FRAMEBUFFER, framebufferid_);

    if (use_alpha())
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
    else
        glPixelStorei(GL_PACK_ALIGNMENT, 1);

    glReadPixels(0, 0, attrib_.viewport.x, attrib_.viewport.y, (use_alpha_? GL_RGBA : GL_RGB), GL_UNSIGNED_BYTE, target_data);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool FrameBuffer::blit(FrameBuffer *destination)
{
    if (!framebufferid_ || !destination)
        return false;

    if (!destination->framebufferid_)
        destination->init();

    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebufferid_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destination->framebufferid_);
    // blit to the frame buffer object
    glBlitFramebuffer(0, 0, attrib_.viewport.x, attrib_.viewport.y,
                      0, 0, destination->width(), destination->height(), GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return true;
}

void FrameBuffer::checkFramebufferStatus()
{
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    switch (status){
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            Log::Warning("GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT​ is returned if any of the framebuffer attachment points are framebuffer incomplete.");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            Log::Warning("GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT​ is returned if the framebuffer does not have at least one image attached to it.");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
            Log::Warning("GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER​ is returned if the value of GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE​ is GL_NONE​ for any color attachment point(s) named by GL_DRAWBUFFERi​.");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
            Log::Warning("GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER​ is returned if GL_READ_BUFFER​ is not GL_NONE​ and the value of GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE​ is GL_NONE​ for the color attachment point named by GL_READ_BUFFER.");
            break;
        case GL_FRAMEBUFFER_UNSUPPORTED:
            Log::Warning("GL_FRAMEBUFFER_UNSUPPORTED​ is returned if the combination of internal formats of the attached images violates an implementation-dependent set of restrictions.");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
            Log::Warning("GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE​ is returned if the value of GL_RENDERBUFFER_SAMPLES​ is not the same for all attached renderbuffers; if the value of GL_TEXTURE_SAMPLES​ is the not same for all attached textures; or, if the attached images are a mix of renderbuffers and textures, the value of GL_RENDERBUFFER_SAMPLES​ does not match the value of GL_TEXTURE_SAMPLES.\nGL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE​ is also returned if the value of GL_TEXTURE_FIXED_SAMPLE_LOCATIONS​ is not the same for all attached textures; or, if the attached images are a mix of renderbuffers and textures, the value of GL_TEXTURE_FIXED_SAMPLE_LOCATIONS​ is not GL_TRUE​ for all attached textures.");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
            Log::Warning("GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS​ is returned if any framebuffer attachment is layered, and any populated attachment is not layered, or if all populated color attachments are not from textures of the same target.");
            break;
        case GL_FRAMEBUFFER_UNDEFINED:
            Log::Warning(" GL_FRAMEBUFFER_UNDEFINED​ is returned if target​ is the default framebuffer, but the default framebuffer does not exist.");
            break;
        case GL_FRAMEBUFFER_COMPLETE:
#ifndef NDEBUG
            Log::Info("Framebuffer created %d x %d.", width(), height());
#endif
            break;
    }
}


glm::mat4 FrameBuffer::projection() const
{
    return projection_;
}

glm::vec2 FrameBuffer::projectionArea() const
{
    return projection_area_;
}

void FrameBuffer::setProjectionArea(glm::vec2 c)
{
    projection_area_.x = CLAMP(c.x, 0.1f, 1.f);
    projection_area_.y = CLAMP(c.y, 0.1f, 1.f);
    projection_ = glm::ortho(-projection_area_.x, projection_area_.x, projection_area_.y, -projection_area_.y, -1.f, 1.f);
}


FrameBufferImage::FrameBufferImage(int w, int h) :
    rgb(nullptr), width(w), height(h)
{
    if (width>0 && height>0)
        rgb = new uint8_t[width*height*3];
}

FrameBufferImage::FrameBufferImage(jpegBuffer jpgimg) :
    rgb(nullptr), width(0), height(0)
{
    int c = 0;
    if (jpgimg.buffer != nullptr && jpgimg.len >0)
        rgb = stbi_load_from_memory(jpgimg.buffer, jpgimg.len, &width, &height, &c, 3);
}

FrameBufferImage::~FrameBufferImage() {
    if (rgb!=nullptr)
        delete rgb;
}

FrameBufferImage::jpegBuffer FrameBufferImage::getJpeg()
{
    jpegBuffer jpgimg;

    // if we hold a valid image
    if (rgb!=nullptr && width>0 && height>0) {

        // allocate JPEG buffer
        // (NB: JPEG will need less than this but we can't know before...)
        jpgimg.buffer = (unsigned char *) malloc( width * height * 3 * sizeof(unsigned char));

        stbi_write_jpg_to_func( [](void *context, void *data, int size)
        {
            memcpy(((FrameBufferImage::jpegBuffer*)context)->buffer + ((FrameBufferImage::jpegBuffer*)context)->len, data, size);
            ((FrameBufferImage::jpegBuffer*)context)->len += size;
        }
        ,&jpgimg, width, height, 3, rgb, FBI_JPEG_QUALITY);
    }

    return jpgimg;
}

FrameBufferImage *FrameBuffer::image(){

    FrameBufferImage *img = nullptr;

    // not ready
    if (!framebufferid_)
        return img;

    // allocate image
    img = new FrameBufferImage(attrib_.viewport.x, attrib_.viewport.y);

    // get pixels into image
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0); // set buffer target readpixel
    readPixels(img->rgb);

    return img;
}

bool FrameBuffer::fill(FrameBufferImage *image)
{
    if (!framebufferid_)
        init();

    // not compatible for RGB
    if (use_alpha_ || use_multi_sampling_)
        return false;

    // invalid image
    if ( image == nullptr ||
         image->rgb==nullptr ||
         image->width !=attrib_.viewport.x ||
         image->height!=attrib_.viewport.y )
        return false;

    // fill texture with image
    glBindTexture(GL_TEXTURE_2D, textureid_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, image->width, image->height,
                    GL_RGB, GL_UNSIGNED_BYTE, image->rgb);
    return true;
}


