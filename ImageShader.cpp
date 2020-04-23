#include "defines.h"
#include "Visitor.h"
#include "ImageShader.h"

ShadingProgram imageShadingProgram("shaders/texture-shader.vs", "shaders/texture-shader.fs");

ImageShader::ImageShader()
{
    program_ = &imageShadingProgram;
    reset();
}

void ImageShader::use()
{
    Shader::use();

    program_->setUniform("brightness", brightness);
    program_->setUniform("contrast", contrast);
    program_->setUniform("stipple", stipple);
}


void ImageShader::reset()
{
    Shader::reset();

    brightness = 0.f;
    contrast = 0.f;
    stipple = 0.f;
}

void ImageShader::accept(Visitor& v) {
    Shader::accept(v);
    v.visit(*this);
}
