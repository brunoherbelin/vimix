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
#include <ctime>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "defines.h"
#include "Resource.h"
#include "ImageShader.h"
#include "Visitor.h"
#include "FrameBuffer.h"
#include "Primitives.h"
#include "Log.h"

#include "ImageFilter.h"

std::string fragmentHeader = "#version 330 core\n"
                             "out vec4 FragColor;\n"
                             "in vec4 vertexColor;\n"
                             "in vec2 vertexUV;\n"
                             "vec3 iChannelResolution[2];\n"
                             "uniform mat4      iTransform;\n"
                             "uniform vec4      color;\n"
                             "uniform float     stipple;\n"
                             "uniform vec3      iResolution;\n"
                             "uniform sampler2D iChannel0;\n"
                             "uniform sampler2D iChannel1;\n"
                             "uniform float     iTime;\n"
                             "uniform float     iTimeDelta;\n"
                             "uniform int       iFrame;\n"
                             "uniform vec4      iDate;\n";

// Filter code starts at line 16 :
std::string filterDefault  = "void mainImage( out vec4 fragColor, in vec2 fragCoord )\n"
                             "{\n"
                             "    vec2 uv = fragCoord.xy / iResolution.xy;\n"
                             "    fragColor = texture(iChannel0, uv);\n"
                             "}\n";

std::string fragmentFooter = "void main() {\n"
                             "    iChannelResolution[0] = vec3(textureSize(iChannel0, 0), 0.f);\n"
                             "    iChannelResolution[1] = vec3(textureSize(iChannel1, 0), 0.f);\n"
                             "    vec4 texcoord = iTransform * vec4(vertexUV.x, vertexUV.y, 0.0, 1.0);\n"
                             "    mainImage( FragColor, texcoord.xy * iChannelResolution[0].xy );\n"
                             "}\n";

//std::string fragmentFooter = "void main() {\n"
//                             "    iChannelResolution[0] = vec3(textureSize(iChannel0, 0), 0.f);\n"
//                             "    iChannelResolution[1] = vec3(textureSize(iChannel1, 0), 0.f);\n"
//                             "    vec4 texcoord = iTransform * vec4(vertexUV.x, vertexUV.y, 0.0, 1.0);\n"
//                             "    vec4 filtered;\n"
//                             "    mainImage( filtered, texcoord.xy * iChannelResolution[0].xy );\n"
//                             "    vec3 RGB = filtered.rgb * vertexColor.rgb * color.rgb;\n"
//                             "    float maskIntensity = dot(texture(iChannel1, vertexUV).rgb, vec3(1.0/3.0));\n"
//                             "    float A = clamp(filtered.a * vertexColor.a * color.a * maskIntensity, 0.0, 1.0);\n"
//                             "    FragColor = vec4(RGB * A, A);\n"
//                             "} \n";


std::list< ImageFilter > ImageFilter::presets = {
    ImageFilter(),
    ImageFilter("Blur",     "shaders/filters/blur_1.glsl",     "shaders/filters/blur_2.glsl",     { { "Amount", 0.5} }),
    ImageFilter("HashBlur", "shaders/filters/hashedblur.glsl", "",     { { "Radius", 0.5}, { "Iterations", 0.5 } }),
    ImageFilter("Unfocus",  "shaders/filters/focus.glsl",      "",     { { "Factor", 0.5} }),
    ImageFilter("Smooth",   "shaders/filters/bilinear.glsl",   "",     { }),
    ImageFilter("Denoise",  "shaders/filters/denoise.glsl",    "",     { { "Threshold", 0.5} }),
    ImageFilter("Noise",    "shaders/filters/noise.glsl",      "",     { { "Amount", 0.25} }),
    ImageFilter("Grain",    "shaders/filters/grain.glsl",      "",     { { "Amount", 0.5} }),
    ImageFilter("Bloom",    "shaders/filters/bloom.glsl",      "",     { { "Intensity", 0.5} }),
    ImageFilter("Bokeh",    "shaders/filters/bokeh.glsl",      "",     { { "Radius", 1.0} }),
    ImageFilter("Kuwahara", "shaders/filters/kuwahara.glsl",   "",     { { "Radius", 1.0} }),
    ImageFilter("Chalk",    "shaders/filters/talk.glsl",       "",     { { "Factor", 1.0} }),
    ImageFilter("Sharpen",  "shaders/filters/sharp.glsl",  "",     { { "Amount", 0.5} }),
    ImageFilter("Unsharp Mask", "shaders/filters/sharpen_1.glsl",  "shaders/filters/sharpen_2.glsl",     { { "Amount", 0.5} }),
    ImageFilter("Sharp Edge", "shaders/filters/bilinear.glsl", "shaders/filters/sharpenedge.glsl",  { { "Strength", 0.5} }),
    ImageFilter("Edge",     "shaders/filters/bilinear.glsl",   "shaders/filters/edge.glsl",     { { "Threshold", 0.5} }),
    ImageFilter("Sobel",    "shaders/filters/sobel.glsl",      "",     { { "Factor", 0.5} }),
    ImageFilter("Freichen", "shaders/filters/freichen.glsl",   "",     { { "Factor", 0.5} }),
    ImageFilter("Stippling","shaders/filters/stippling.glsl",  "",     { { "Factor", 0.5} }),
    ImageFilter("Dithering","shaders/filters/dithering.glsl",  "",     { { "Factor", 0.5} }),
    ImageFilter("Pixelate", "shaders/filters/pixelate.glsl",   "",     { { "Size", 0.5}, { "Sharpen", 0.5} }),
    ImageFilter("Chromakey","shaders/filters/chromakey.glsl",  "",     { { "Red", 0.05}, { "Green", 0.63}, { "Blue", 0.14}, { "Tolerance", 0.54} }),
    ImageFilter("Fisheye",  "shaders/filters/fisheye.glsl",    "",     { { "Factor", 0.5} }),
};



std::string ImageFilter::getFilterCodeInputs()
{
    static std::string filterHeaderHelp =  "vec3      iResolution;           // viewport resolution (in pixels)\n"
                                           "float     iTime;                 // shader playback time (in seconds)\n"
                                           "float     iTimeDelta;            // render time (in seconds)\n"
                                           "int       iFrame;                // shader playback frame\n"
                                           "vec3      iChannelResolution[2]; // input channel resolution (in pixels)\n"
                                           "sampler2D iChannel0;             // input channel (texture).\n"
                                           "sampler2D iChannel1;             // input channel (mask).\n"
                                           "vec4      iDate;                 // (year, month, day, time in seconds)";
    return filterHeaderHelp;
}

std::string ImageFilter::getFilterCodeDefault()
{
    return filterDefault;
}

ImageFilter::ImageFilter() : name_("None"), code_({"shaders/filters/default.glsl",""})
{

}

ImageFilter::ImageFilter(const std::string &name, const std::string &first_pass, const std::string &second_pass,
                         const std::map<std::string, float> &parameters) :
    name_(name), code_({first_pass, second_pass}), parameters_(parameters)
{

}

ImageFilter::ImageFilter(const ImageFilter &other) :
    name_(other.name_), code_(other.code_), parameters_(other.parameters_)
{

}

ImageFilter& ImageFilter::operator= (const ImageFilter& other)
{
    if (this != &other) {
        this->name_ = other.name_;
        this->code_ = other.code_;
        this->parameters_.clear();
        this->parameters_ = other.parameters_;
    }

    return *this;
}

std::pair< std::string, std::string > ImageFilter::code()
{
    // code for filter can be provided by the name of a ressource file
    if (Resource::hasPath(code_.first))
        code_.first = Resource::getText(code_.first);
    if (Resource::hasPath(code_.second))
        code_.second = Resource::getText(code_.second);

    return code_;
}

bool ImageFilter::operator!= (const ImageFilter& other) const
{
    if (this->code_.first != other.code_.first)
        return true;
    if (this->code_.second != other.code_.second)
        return true;

    return false;
}


class ImageFilteringShader : public ImageShader
{
    // GLSL Program
    ShadingProgram custom_shading_;

    // fragment shader GLSL code
    std::string shader_code_;
    std::string code_;

    // for iTimedelta
    GTimer *timer_;
    double iTime_;
    uint iFrame_;

public:

    // list of uniforms to control shader
    std::map< std::string, float > uniforms_;

    ImageFilteringShader();
    ~ImageFilteringShader();

    void update(float dt);

    void use() override;
    void reset() override;
    void copy(ImageFilteringShader const& S);

    // set the code of the filter
    void setCode(const std::string &code, std::promise<std::string> *ret = nullptr);

};


ImageFilteringShader::ImageFilteringShader(): ImageShader()
{
    program_ = &custom_shading_;

    shader_code_ = fragmentHeader + filterDefault + fragmentFooter;
    custom_shading_.setShaders("shaders/image.vs", shader_code_);

    timer_ = g_timer_new ();

    ImageShader::reset();
}

ImageFilteringShader::~ImageFilteringShader()
{
    custom_shading_.reset();
    g_timer_destroy(timer_);
}

void ImageFilteringShader::update(float dt)
{
    iTime_ += dt;
    if (iTime_ > FLT_MAX)
        iTime_ = 0.0;

    iFrame_++;
    if (iFrame_ > INT_MAX)
        iFrame_ = 0;
}

void ImageFilteringShader::use()
{
    ImageShader::use();

    //
    // Shader input uniforms
    //

    program_->setUniform("iTime", float(iTime_) );
    program_->setUniform("iFrame", int(iFrame_) );

    // Calculate iTimeDelta
    double elapsed = g_timer_elapsed (timer_, NULL);
    g_timer_reset(timer_);
    program_->setUniform("iTimeDelta", float(elapsed) );

    // calculate iDate
    std::time_t now = std::time(0);
    std::tm *local = std::localtime(&now);
    glm::vec4 iDate(local->tm_year+1900, local->tm_mon, local->tm_mday, local->tm_hour*3600+local->tm_min*60+local->tm_sec);
    program_->setUniform("iDate", iDate);

    //
    // loop over uniforms
    //
    for (auto u = uniforms_.begin(); u != uniforms_.end(); ++u)
        // set uniform to current value
        program_->setUniform( u->first, u->second );
}

void ImageFilteringShader::reset()
{
    ImageShader::reset();

    // reset times
    iFrame_ = 0;
    iTime_ = 0.0;

}

void ImageFilteringShader::setCode(const std::string &code, std::promise<std::string> *ret)
{
    if (code != code_)
    {
        code_ = code;
        shader_code_ = fragmentHeader + code_ + fragmentFooter;
        custom_shading_.setShaders("shaders/image.vs", shader_code_, ret);
    }
    else if (ret != nullptr) {
        ret->set_value("No change.");
    }
}

void ImageFilteringShader::copy(ImageFilteringShader const& S)
{
    ImageShader::copy(S);

    // change the shading code for fragment
    shader_code_ = S.shader_code_;
    custom_shading_.setShaders("shaders/image.vs", shader_code_);
}


ImageFilterRenderer::ImageFilterRenderer(glm::vec3 resolution): enabled_(false)
{
    shaders_.first  = new ImageFilteringShader;
    surfaces_.first = new Surface(shaders_.first);
    buffers_.first  = new FrameBuffer( resolution, true );

    shaders_.second  = nullptr;
    surfaces_.second = nullptr;
    buffers_.second  = nullptr;
}

ImageFilterRenderer::~ImageFilterRenderer()
{
    if ( buffers_.first!= nullptr )
        delete buffers_.first;
    if ( buffers_.second!= nullptr )
        delete buffers_.second;

    if ( surfaces_.first!= nullptr )
        delete surfaces_.first;
    if ( surfaces_.second!= nullptr )
        delete surfaces_.second;

    // NB: shaders_ are removed with surface
}

void ImageFilterRenderer::update(float dt)
{
    shaders_.first->update(dt);
    if (shaders_.second)
        shaders_.second->update(dt);
}

void ImageFilterRenderer::setInputTexture(uint t)
{
    surfaces_.first->setTextureIndex( t );
    shaders_.first->mask_texture = t;
    if (shaders_.second)
        shaders_.second->mask_texture = t;
}

uint ImageFilterRenderer::getOutputTexture() const
{
    if ( enabled_ )
        return buffers_.second ? buffers_.second->texture() : buffers_.first->texture();
    else
        return surfaces_.first->textureIndex();
}

void ImageFilterRenderer::draw()
{
    if ( enabled_ )
    {
        // FIRST PASS
        // render input surface into frame buffer
        buffers_.first->begin();
        surfaces_.first->draw(glm::identity<glm::mat4>(), buffers_.first->projection());
        buffers_.first->end();

        // SECOND PASS
        if ( buffers_.second && surfaces_.second ) {
            // render filtered surface from first pass into frame buffer
            surfaces_.second->setTextureIndex( buffers_.first->texture() );
            buffers_.second->begin();
            surfaces_.second->draw(glm::identity<glm::mat4>(), buffers_.second->projection());
            buffers_.second->end();
        }
    }
}

ImageFilter ImageFilterRenderer::filter() const
{
    return filter_;
}

void ImageFilterRenderer::setFilter(const ImageFilter &f, std::promise<std::string> *ret)
{
    // always keep local copy
    filter_ = f;

    // force disable when using default filter
    enabled_ = filter_ != ImageFilter::presets.front();

    // change code
    std::pair<std::string, std::string> codes = filter_.code();

    // set code for first pass
    shaders_.first->setCode( codes.first, ret );

    // no code provided for second pass
    if ( codes.second.empty() ) {

        // delete second pass if was previously used
        if (buffers_.second!= nullptr )
            delete buffers_.second;
        if (surfaces_.second!= nullptr )
            delete surfaces_.second;

        shaders_.second  = nullptr;
        surfaces_.second = nullptr;
        buffers_.second  = nullptr;
    }
    // set code for second pass
    else {
        // second pass not setup
        if (shaders_.second == nullptr)  {

            // create shader, surface and buffer for second pass
            shaders_.second  = new ImageFilteringShader;
            surfaces_.second = new Surface(shaders_.second);
            buffers_.second  = new FrameBuffer( buffers_.first->resolution(), true );
        }

        // set the code of the shader for second pass
        shaders_.second->setCode( codes.second );
    }

    // change uniforms
    shaders_.first->uniforms_ = filter_.parameters();
    if (shaders_.second != nullptr )
        shaders_.second->uniforms_ = filter_.parameters();

}

