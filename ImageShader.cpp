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

    // set image uniforms
    if (imageshader_changed) {
        program_.setUniform("brightness", brightness);
        program_.setUniform("contrast", contrast);
        imageshader_changed = false;
    }
}


void ImageShader::reset()
{
    Shader::reset();

    brightness = 0.f;
    contrast = 0.f;
    imageshader_changed = true;
}

void ImageShader::setBrightness(float v)
{
    brightness = CLAMP(v, -1.f, 1.f);
    imageshader_changed = true;
}

void ImageShader::setContrast(float v)
{
    contrast = CLAMP(v, -1.f, 1.f);
    imageshader_changed = true;
}
