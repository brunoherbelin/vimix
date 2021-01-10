#include <glad/glad.h>

#include "defines.h"
#include "Visitor.h"
#include "ImageShader.h"
#include "Resource.h"
#include "rsc/fonts/IconsFontAwesome5.h"
//#include

static ShadingProgram imageShadingProgram("shaders/image.vs", "shaders/image.fs");

const char* MaskShader::mask_names[3] = { ICON_FA_EXPAND,  ICON_FA_EDIT, ICON_FA_SHAPES };
const char* MaskShader::mask_shapes[5] = { "Elipse", "Oblong", "Rectangle", "Horizontal", "Vertical" };
std::vector< ShadingProgram* > MaskShader::mask_programs;

ImageShader::ImageShader(): Shader(), stipple(0.0)
{
    // static program shader
    program_ = &imageShadingProgram;
    // reset instance
    reset();
}

void ImageShader::use()
{
    Shader::use();

    program_->setUniform("stipple", stipple);

    glActiveTexture(GL_TEXTURE1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, mask_texture);

    glActiveTexture(GL_TEXTURE0);

}

void ImageShader::reset()
{
    Shader::reset();

    // default mask
    mask_texture = Resource::getTextureWhite();
    // no stippling
    stipple = 0.f;
}

void ImageShader::operator = (const ImageShader &S)
{
    Shader::operator =(S);

    mask_texture = S.mask_texture;
    stipple = S.stipple;
}


void ImageShader::accept(Visitor& v) {
    Shader::accept(v);
    v.visit(*this);
}




MaskShader::MaskShader(): Shader(), mode(0)
{
    // first initialization
    if ( mask_programs.empty() ) {
        mask_programs.push_back(new ShadingProgram("shaders/simple.vs", "shaders/simple.fs"));
        mask_programs.push_back(new ShadingProgram("shaders/image.vs", "shaders/mask_draw.fs"));
        mask_programs.push_back(new ShadingProgram("shaders/simple.vs", "shaders/mask_elipse.fs"));
        mask_programs.push_back(new ShadingProgram("shaders/simple.vs", "shaders/mask_round.fs"));
        mask_programs.push_back(new ShadingProgram("shaders/simple.vs", "shaders/mask_box.fs"));
        mask_programs.push_back(new ShadingProgram("shaders/simple.vs", "shaders/mask_horizontal.fs"));
        mask_programs.push_back(new ShadingProgram("shaders/simple.vs", "shaders/mask_vertical.fs"));
    }
    // reset instance
    reset();
    // static program shader
    program_ = mask_programs[0];
}

void MaskShader::use()
{
    // select program to use
    mode = CLAMP(mode, 0, 2);
    shape = CLAMP(shape, 0, 4);
    program_ = mode < 2 ? mask_programs[mode] : mask_programs[shape+2] ;

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
}

void MaskShader::operator = (const MaskShader &S)
{
    Shader::operator =(S);

    mode = S.mode;
    shape = S.shape;
    blur = S.blur;
    size = S.size;
}


void MaskShader::accept(Visitor& v) {
    Shader::accept(v);
    v.visit(*this);
}


