/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2023 Bruno Herbelin <bruno.herbelin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
**/

#include "Visitor/Visitor.h"
#include "ImageProcessingShader.h"

ShadingProgram imageProcessingShadingProgram("shaders/image.vs", "shaders/imageprocessing.fs");


ImageProcessingShader::ImageProcessingShader(): Shader()
{
    program_ = &imageProcessingShadingProgram;
    ImageProcessingShader::reset();
}

void ImageProcessingShader::use()
{
    Shader::use();

    program_->setUniform("brightness", brightness);
    program_->setUniform("contrast", contrast);
    program_->setUniform("saturation", saturation);
    program_->setUniform("hueshift", hueshift);

    program_->setUniform("threshold", threshold);
    program_->setUniform("nbColors", nbColors);
    program_->setUniform("invert", invert);

    program_->setUniform("gamma", gamma);
    program_->setUniform("levels", levels);

}

void ImageProcessingShader::reset()
{
    Shader::reset();

    // default values for image processing
    brightness = 0.f;
    contrast = 0.f;
    saturation = 0.f;
    hueshift = 0.f;
    threshold = 0.f;
    nbColors = 0;
    invert = 0;
    gamma = glm::vec4(1.f, 1.f, 1.f, 1.f);
    levels = glm::vec4(0.f, 1.f, 0.f, 1.f);

}

void ImageProcessingShader::copy(ImageProcessingShader const& S)
{
    brightness = S.brightness;
    contrast = S.contrast;
    saturation = S.saturation;
    hueshift = S.hueshift;
    threshold = S.threshold;
    nbColors = S.nbColors;
    invert = S.invert;
    gamma = S.gamma;
    levels = S.levels;
}


void ImageProcessingShader::accept(Visitor& v)
{
//    Shader::accept(v);
    v.visit(*this);
}
