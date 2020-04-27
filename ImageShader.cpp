#include <glad/glad.h>

#include "defines.h"
#include "Visitor.h"
#include "ImageShader.h"
#include "Resource.h"

ShadingProgram imageShadingProgram("shaders/image.vs", "shaders/image.fs");

ImageShader::ImageShader()
{
    program_ = &imageShadingProgram;
    reset();
}

void ImageShader::use()
{
    Shader::use();

    program_->setUniform("stipple", stipple);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, mask);
    glActiveTexture(GL_TEXTURE0);

}


void ImageShader::reset()
{
    Shader::reset();

    // setup double texturing
    program_->use();
    program_->setUniform("iChannel0", 0);
    program_->setUniform("iChannel1", 1);
    program_->enduse();

    // default mask (channel1)
    mask = Resource::getTextureWhite();

    // no stippling
    stipple = 0.f;
}

void ImageShader::accept(Visitor& v) {
    Shader::accept(v);
    v.visit(*this);
}
