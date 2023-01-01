/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2022 Bruno Herbelin <bruno.herbelin@gmail.com>
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

#include <glad/glad.h>

#include "defines.h"
#include "Visitor.h"
#include "Resource.h"
#include "IconsFontAwesome5.h"

#include "ImageShader.h"

ShadingProgram imageShadingProgram("shaders/image.vs", "shaders/image.fs");
ShadingProgram imageAlphaProgram  ("shaders/image.vs", "shaders/imageblending.fs");
std::vector< ShadingProgram > maskPrograms = {
    ShadingProgram("shaders/simple.vs", "shaders/simple.fs"),
    ShadingProgram("shaders/image.vs",  "shaders/mask_draw.fs"),
    ShadingProgram("shaders/simple.vs", "shaders/mask_elipse.fs"),
    ShadingProgram("shaders/simple.vs", "shaders/mask_round.fs"),
    ShadingProgram("shaders/simple.vs", "shaders/mask_box.fs"),
    ShadingProgram("shaders/simple.vs", "shaders/mask_horizontal.fs"),
    ShadingProgram("shaders/simple.vs", "shaders/mask_vertical.fs")
};

const char* MaskShader::mask_names[3]  = { ICON_FA_EXPAND, ICON_FA_EDIT, ICON_FA_SHAPES };
const char* MaskShader::mask_shapes[5] = { "Ellipse", "Oblong", "Rectangle", "Horizontal", "Vertical" };

ImageShader::ImageShader(): Shader(), mask_texture(0), stipple(0.f)
{
    // static program shader
    program_ = &imageShadingProgram;
    // reset instance
    ImageShader::reset();
}

void ImageShader::use()
{
    Shader::use();

    // set stippling
    program_->setUniform("stipple", stipple);

    // default mask
    if (mask_texture == 0)
        mask_texture = Resource::getTextureWhite();

    // setup mask texture
    glActiveTexture(GL_TEXTURE1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture  (GL_TEXTURE_2D, mask_texture);
    glActiveTexture(GL_TEXTURE0);
}

void ImageShader::reset()
{
    Shader::reset();
    mask_texture = 0;

    // no stippling
    stipple = 0.f;
}

void ImageShader::copy(ImageShader const& S)
{
    mask_texture = S.mask_texture;
    stipple = S.stipple;
}


void ImageShader::accept(Visitor& v) {
    Shader::accept(v);
    v.visit(*this);
}


AlphaShader::AlphaShader(): ImageShader()
{
    // to inverse alpha mode, use dedicated shading program
    program_ = &imageAlphaProgram;
    // reset instance
    reset();

    blending = Shader::BLEND_NONE;
}


MaskShader::MaskShader(): Shader(), mode(0)
{
    // reset instance
    MaskShader::reset();
    // static program shader
    program_ = &maskPrograms[0];
}

void MaskShader::use()
{
    // select program to use
    mode = MINI(mode, 2);
    shape = MINI(shape, 4);
    program_ = mode < 2 ? &maskPrograms[mode] : &maskPrograms[shape+2] ;

    // actual use of shader program
    Shader::use();

    // shape parameters
    size = shape < HORIZONTAL ? glm::max(glm::abs(size), glm::vec2(0.2)) : size;
    program_->setUniform("size", size);
    program_->setUniform("blur", blur);

    // brush parameters
    program_->setUniform("cursor", cursor);
    program_->setUniform("brush", brush);
    program_->setUniform("option", option);
    program_->setUniform("effect", effect);
}

void MaskShader::reset()
{
    Shader::reset();

    // default mask
    mode = 0;

    // default shape
    shape = 0;
    blur = 0.5f;
    size = glm::vec2(1.f, 1.f);

    // default brush
    cursor = glm::vec4(-10.f, -10.f, 1.f, 1.f);
    brush = glm::vec3(0.5f, 0.1f, 0.f);
    option = 0;
    effect = 0;
}

void MaskShader::copy(MaskShader const& S)
{
    mode = S.mode;
    shape = S.shape;
    blur = S.blur;
    size = S.size;
}


void MaskShader::accept(Visitor& v) {
    Shader::accept(v);
    v.visit(*this);
}


