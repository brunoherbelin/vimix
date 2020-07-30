#include "defines.h"
#include "Visitor.h"
#include "Log.h"
#include "ImageProcessingShader.h"

ShadingProgram imageProcessingShadingProgram("shaders/image.vs", "shaders/imageprocessing.fs");

const char* ImageProcessingShader::filter_names[12] = { "None", "Blur", "Sharpen", "Edge", "Emboss", "Denoising",
                                                        "Erosion 3x3", "Erosion 5x5", "Erosion 7x7", "Dilation 3x3", "Dilation 5x5", "Dilation 7x7" };


ImageProcessingShader::ImageProcessingShader()
{
    program_ = &imageProcessingShadingProgram;
    reset();
}

ImageProcessingShader::ImageProcessingShader(const ImageProcessingShader &S)
{
    program_ = &imageProcessingShadingProgram;
    reset();
    brightness = S.brightness;
    contrast = S.contrast;
    saturation = S.saturation;
    hueshift = S.hueshift;
    threshold = S.threshold;
    lumakey = S.lumakey;
    nbColors = S.nbColors;
    invert = S.invert;
    filterid = S.filterid;
    gamma = S.gamma;
    levels = S.levels;
    chromakey = S.chromakey;
    chromadelta = S.chromadelta;
}

void ImageProcessingShader::use()
{
    Shader::use();

//    program_->setUniform("iChannelResolution[0]", iChannelResolution[0].x, iChannelResolution[0].y, iChannelResolution[0].z);

    program_->setUniform("brightness", brightness);
    program_->setUniform("contrast", contrast);
    program_->setUniform("saturation", saturation);
    program_->setUniform("hueshift", hueshift);

    program_->setUniform("threshold", threshold);
    program_->setUniform("lumakey", lumakey);
    program_->setUniform("nbColors", nbColors);
    program_->setUniform("invert", invert);
    program_->setUniform("filterid", filterid);

    program_->setUniform("gamma", gamma);
    program_->setUniform("levels", levels);
    program_->setUniform("chromakey", chromakey);
    program_->setUniform("chromadelta", chromadelta);

}


void ImageProcessingShader::reset()
{
    Shader::reset();

//    // no texture resolution yet
//    iChannelResolution[0] = glm::vec3(1.f);

    // default values for image processing
    brightness = 0.f;
    contrast = 0.f;
    saturation = 0.f;
    hueshift = 0.f;
    threshold = 0.f;
    lumakey = 0.f;
    nbColors = 0;
    invert = 0;
    filterid = 0;
    gamma = glm::vec4(1.f, 1.f, 1.f, 1.f);
    levels = glm::vec4(0.f, 1.f, 0.f, 1.f);
    chromakey = glm::vec4(0.f, 0.8f, 0.f, 0.f);
    chromadelta = 0.f;

}

void ImageProcessingShader::operator = (const ImageProcessingShader &S )
{
    Shader::operator =(S);

    brightness = S.brightness;
    contrast = S.contrast;
    saturation = S.saturation;
    hueshift = S.hueshift;
    threshold = S.threshold;
    lumakey = S.lumakey;
    nbColors = S.nbColors;
    invert = S.invert;
    filterid = S.filterid;
    gamma = S.gamma;
    levels = S.levels;
    chromakey = S.chromakey;
    chromadelta = S.chromadelta;
}


void ImageProcessingShader::accept(Visitor& v)
{
//    Shader::accept(v);
    v.visit(*this);
}
