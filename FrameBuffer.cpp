#include "FrameBuffer.h"
#include "ImageShader.h"
#include "Resource.h"
#include "Log.h"


#include <glad/glad.h>

const char* FrameBuffer::aspect_ratio_name[4] = { "4:3", "3:2", "16:10", "16:9" };
glm::vec2 FrameBuffer::aspect_ratio_size[4] = { glm::vec2(4.f,3.f), glm::vec2(3.f,2.f), glm::vec2(16.f,10.f), glm::vec2(16.f,9.f) };
const char* FrameBuffer::resolution_name[4] = { "720p", "1080p", "1440", "4K" };
float FrameBuffer::resolution_height[4] = { 720.f, 1080.f, 1440.f, 2160.f };


glm::vec3 FrameBuffer::getResolutionFromParameters(int ar, int h)
{
    float width = aspect_ratio_size[ar].x * resolution_height[h] / aspect_ratio_size[ar].y;
    glm::vec3 res = glm::vec3( width, resolution_height[h] , 0.f);

    return res;
}

FrameBuffer::FrameBuffer(glm::vec3 resolution, bool useAlpha, bool useDepthBuffer): textureid_(0), framebufferid_(0), usealpha_(useAlpha), usedepth_(useDepthBuffer)
{
    attrib_.viewport = glm::ivec2(resolution);
    attrib_.clear_color = glm::vec4(0.f, 0.f, 0.f, usealpha_ ? 0.f : 1.f);
}

FrameBuffer::FrameBuffer(uint width, uint height, bool useAlpha, bool useDepthBuffer): textureid_(0), framebufferid_(0), usealpha_(useAlpha), usedepth_(useDepthBuffer)
{
    attrib_.viewport = glm::ivec2(width, height);
    attrib_.clear_color = glm::vec4(0.f, 0.f, 0.f, usealpha_ ? 0.f : 1.f);
}

void FrameBuffer::init()
{
    // create a renderbuffer object to store depth info
    GLuint rboId;
    if (usedepth_){
        glGenRenderbuffers(1, &rboId);
        glBindRenderbuffer(GL_RENDERBUFFER, rboId);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, attrib_.viewport.x, attrib_.viewport.y);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }

    // create a framebuffer object
    glGenFramebuffers(1, &framebufferid_);
    glBindFramebuffer(GL_FRAMEBUFFER, framebufferid_);

    // generate texture
    glGenTextures(1, &textureid_);
    glBindTexture(GL_TEXTURE_2D, textureid_);
    if (usealpha_)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, attrib_.viewport.x, attrib_.viewport.y,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    else
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, attrib_.viewport.x, attrib_.viewport.y,
                     0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // attach the texture to FBO color attachment point
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, textureid_, 0);

    // attach the renderbuffer to depth attachment point
    if (usedepth_){
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, rboId);
    }
    checkFramebufferStatus();
}

FrameBuffer::~FrameBuffer()
{
    if (framebufferid_)
        glDeleteFramebuffers(1, &framebufferid_);
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


glm::vec3 FrameBuffer::resolution() const
{
    return glm::vec3(attrib_.viewport.x, attrib_.viewport.y, 0.f);
}

void FrameBuffer::bind()
{
    if (!framebufferid_)
        init();

    glBindFramebuffer(GL_FRAMEBUFFER, framebufferid_);
}

void FrameBuffer::begin()
{
    bind();

    Rendering::manager().PushAttrib(attrib_);

    glClear(GL_COLOR_BUFFER_BIT);
}

void FrameBuffer::end()
{
    Rendering::manager().PopAttrib();

    FrameBuffer::release();
}

void FrameBuffer::release()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool FrameBuffer::blit(FrameBuffer *other)
{
    if (!framebufferid_)
        return false;

    if (attrib_.viewport.x != other->width() || attrib_.viewport.y != other->height())
        return false;

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, other->framebufferid_);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebufferid_);
    // blit to the frame buffer object
    glBlitFramebuffer(0, attrib_.viewport.y, attrib_.viewport.x, 0, 0, 0,
                    other->width(), other->height(),
                    GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

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

