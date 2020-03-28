#include "defines.h"
#include "ImageShader.h"


ImageShader::ImageShader()
{
    vertex_file = "shaders/texture-shader.vs";
    fragment_file = "shaders/texture-shader.fs";
}

void ImageShader::use()
{
    Shader::use();

    program_.setUniform("brightness", brightness);
    program_.setUniform("contrast", contrast);
}


void ImageShader::reset()
{
    Shader::reset();

    brightness = 0.f;
    contrast = 0.f;
}
