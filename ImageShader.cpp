#include <glad/glad.h>

#include "defines.h"
#include "Visitor.h"
#include "ImageShader.h"
#include "Resource.h"

static ShadingProgram imageShadingProgram("shaders/image.vs", "shaders/image.fs");

const char* ImageShader::mask_names[11] = { "None", "Glow", "Halo", "Circle", "Round", "Vignette", "Top", "Botton", "Left", "Right", "Custom" };
std::vector< uint > ImageShader::mask_presets;

ImageShader::ImageShader(): Shader(), mask(0), custom_textureindex(0), stipple(0.0)
{
    // first initialization
    if ( mask_presets.empty() ) {
        mask_presets.push_back(Resource::getTextureWhite());
        mask_presets.push_back(Resource::getTextureImage("images/mask_glow.png"));
        mask_presets.push_back(Resource::getTextureImage("images/mask_halo.png"));
        mask_presets.push_back(Resource::getTextureImage("images/mask_circle.png"));
        mask_presets.push_back(Resource::getTextureImage("images/mask_roundcorner.png"));
        mask_presets.push_back(Resource::getTextureImage("images/mask_vignette.png"));
        mask_presets.push_back(Resource::getTextureImage("images/mask_linear_top.png"));
        mask_presets.push_back(Resource::getTextureImage("images/mask_linear_bottom.png"));
        mask_presets.push_back(Resource::getTextureImage("images/mask_linear_left.png"));
        mask_presets.push_back(Resource::getTextureImage("images/mask_linear_right.png"));
    }
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
    if ( mask < 10 )
        glBindTexture(GL_TEXTURE_2D, mask_presets[mask]);
    else
        glBindTexture(GL_TEXTURE_2D, custom_textureindex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glActiveTexture(GL_TEXTURE0);

}

void ImageShader::reset()
{
    Shader::reset();

    // default mask
    mask = 0;
    custom_textureindex = mask_presets[0];
    // no stippling
    stipple = 0.f;
}

void ImageShader::operator = (const ImageShader &S )
{
    Shader::operator =(S);

    mask = S.mask;
    custom_textureindex = S.custom_textureindex;
    stipple = S.stipple;
}


void ImageShader::accept(Visitor& v) {
    Shader::accept(v);
    v.visit(*this);
}
