#include "ImageProcessingShader.h"


ShadingProgram imageProcessingShadingProgram("shaders/processing-shader.vs", "shaders/texture-shader.fs");

ImageProcessingShader::ImageProcessingShader()
{
    program_ = &imageProcessingShadingProgram;
    reset();
}

void ImageProcessingShader::use()
{
    Shader::use();

    program_->setUniform("brightness", brightness);
    program_->setUniform("contrast", contrast);
    program_->setUniform("stipple", stipple);
}


void ImageProcessingShader::reset()
{
    Shader::reset();

    brightness = 0.f;
    contrast = 0.f;
    stipple = 0.f;
}

void ImageProcessingShader::accept(Visitor& v) {
    Shader::accept(v);
    v.visit(*this);
}


::ImageProcessingShader()
{

}
