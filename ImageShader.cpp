#include <glad/glad.h>

#include "defines.h"
#include "Visitor.h"
#include "ImageShader.h"
#include "Resource.h"

static ShadingProgram imageShadingProgram("shaders/image.vs", "shaders/image.fs");

const char* ImageShader::mask_names[10] = { "None", "Vignette", "Halo", "Circle", "Round", "Top", "Botton", "Left", "Right", "Custom" };
std::vector< uint > ImageShader::mask_presets;

ImageShader::ImageShader(): Shader(), custom_textureindex(0)
{
    // first initialization
    if ( mask_presets.empty() ) {
        mask_presets.push_back(Resource::getTextureWhite());
        mask_presets.push_back(Resource::getTextureDDS("images/mask_vignette.dds"));
        mask_presets.push_back(Resource::getTextureDDS("images/mask_halo.dds"));
        mask_presets.push_back(Resource::getTextureDDS("images/mask_circle.dds"));
        mask_presets.push_back(Resource::getTextureDDS("images/mask_roundcorner.dds"));
        mask_presets.push_back(Resource::getTextureDDS("images/mask_linear_top.dds"));
        mask_presets.push_back(Resource::getTextureDDS("images/mask_linear_bottom.dds"));
        mask_presets.push_back(Resource::getTextureDDS("images/mask_linear_left.dds"));
        mask_presets.push_back(Resource::getTextureDDS("images/mask_linear_right.dds"));
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
    if ( mask < 9 )
        glBindTexture(GL_TEXTURE_2D, mask_presets[mask]);
    else
        glBindTexture(GL_TEXTURE_2D, custom_textureindex);
    glActiveTexture(GL_TEXTURE0);

}


void ImageShader::reset()
{
    Shader::reset();

    // setup double texturing (TODO : do it only once)
    program_->use();
    program_->setUniform("iChannel0", 0);
    program_->setUniform("iChannel1", 1);
    program_->enduse();

    // default mask
    mask = 0;
    custom_textureindex = mask_presets[0];
    // no stippling
    stipple = 0.f;
}

void ImageShader::accept(Visitor& v) {
    Shader::accept(v);
    v.visit(*this);
}
