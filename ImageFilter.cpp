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
#include "Shader.h"
#include "Visitor.h"
#include "FrameBuffer.h"
#include "Primitives.h"
#include "Log.h"

#include "ImageFilter.h"

std::string filterHeaderHelp =  "vec3      iResolution;           // viewport resolution (in pixels)\n"
                                "float     iTime;                 // shader playback time (in seconds)\n"
                                "float     iTimeDelta;            // render time (in seconds)\n"
                                "int       iFrame;                // shader playback frame\n"
                                "vec3      iChannelResolution[2]; // input channel resolution (in pixels)\n"
                                "sampler2D iChannel0;             // input channel (texture).\n"
                                "sampler2D iChannel1;             // input channel (mask).\n"
                                "vec4      iDate;                 // (year, month, day, time in seconds)";

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
std::string filterDefault = "void mainImage( out vec4 fragColor, in vec2 fragCoord )\n"
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


class ImageFilteringShader : public Shader
{
    ShadingProgram custom_shading_;

    // fragment shader GLSL code
    std::string shader_code_;

    // list of uniform vars in GLSL
    // with associated pair (current_value, default_values) in range [0.f 1.f]
    std::map<std::string, glm::vec2> uniforms_;

    // for iTime & iFrame
    GTimer *timer_;
    double iTime_;
    int iFrame_;

public:

    ImageFilteringShader();
    ~ImageFilteringShader();

    void use() override;
    void reset() override;
    void accept(Visitor& v) override;
    void copy(ImageFilteringShader const& S);

    // set fragment shader code and uniforms (with default values)
    void setFilterCode(const std::string &code, std::map<std::string, float> parameters, std::promise<std::string> *prom = nullptr);
};


ImageFilteringShader::ImageFilteringShader(): Shader()
{
    program_ = &custom_shading_;
    custom_shading_.setShaders("shaders/image.vs", "shaders/image.fs");

    timer_ = g_timer_new ();

    Shader::reset();
}

ImageFilteringShader::~ImageFilteringShader()
{
    custom_shading_.reset();
}

void ImageFilteringShader::use()
{
    Shader::use();

    //
    // Shadertoy uniforms
    //
    // Calculate iTime and iTimeDelta
    double elapsed = g_timer_elapsed (timer_, NULL);
    program_->setUniform("iTimeDelta", float(elapsed - iTime_) );
    iTime_ = elapsed;
    if (iTime_ > FLT_MAX) {
        g_timer_start(timer_);
        iTime_ = 0.f;
    }
    program_->setUniform("iTime", float(iTime_) );
    // calculate iFrame
    program_->setUniform("iFrame", ++iFrame_);
    if (iFrame_ > INT_MAX -2)
        iFrame_ = 0;
    // calculate iDate
    std::time_t now = std::time(0);
    std::tm *local = std::localtime(&now);
    glm::vec4 iDate(local->tm_year, local->tm_mon, local->tm_mday, local->tm_hour*3600.0+local->tm_min*60.0+local->tm_sec);
    program_->setUniform("iDate", iDate);

    //
    // loop over all uniforms
    //
    for (auto u = uniforms_.begin(); u != uniforms_.end(); ++u)
        // set uniform to current value
        program_->setUniform( u->first, u->second.x );
}

void ImageFilteringShader::reset()
{
    Shader::reset();

    // reset times
    g_timer_start(timer_);
    iFrame_ = 0;

    // loop over all uniforms
    for (auto u = uniforms_.begin(); u != uniforms_.end(); ++u)
        // reset current value to default value
        u->second.x = u->second.y;
}

void ImageFilteringShader::setFilterCode(const std::string &code, std::map<std::string, float> parameters, std::promise<std::string> *ret)
{
    // change the shading code for fragment
    shader_code_ = fragmentHeader + code + fragmentFooter;
    custom_shading_.setShaders("shaders/image.vs", shader_code_, ret);

    // set the list of uniforms
    uniforms_.clear();
    for (auto u = parameters.begin(); u != parameters.end(); ++u)
        // set uniform with key name to have default and current values
        uniforms_[u->first] = glm::vec2(u->second, u->second);
}

void ImageFilteringShader::copy(ImageFilteringShader const& S)
{
    // change the shading code for fragment
    shader_code_ = S.shader_code_;
    custom_shading_.setShaders("shaders/image.vs", shader_code_);

    // set the list of uniforms
    uniforms_.clear();
    for (auto u = S.uniforms_.begin(); u != S.uniforms_.end(); ++u)
        // set uniform with key name to have default and current values
        uniforms_[u->first] = u->second;
}

void ImageFilteringShader::accept(Visitor& v)
{
//    Shader::accept(v);
    v.visit(*this);
}

std::string ImageFilter::getFilterCodeInputs()
{
    return filterHeaderHelp;
}

std::string ImageFilter::getFilterCodeDefault()
{
    return filterDefault;
}

ImageFilter::ImageFilter() : code_(filterDefault)
{

}

ImageFilter::ImageFilter(const ImageFilter &other) : code_(other.code_)
{

}

ImageFilter& ImageFilter::operator = (const ImageFilter& other)
{
    if (this != &other)
        this->code_ = other.code_;

    return *this;
}

bool ImageFilter::operator != (const ImageFilter& other) const
{
    if (this == &other || this->code_ == other.code_)
        return false;

    return true;
}


// set the code of the filter
void setCode(const std::string &code);

ImageFilterRenderer::ImageFilterRenderer(glm::vec3 resolution): enabled_(true)
{
    shader_ = new ImageFilteringShader;
    surface_ = new Surface(shader_);
    buffer_ = new FrameBuffer( resolution, true );

    std::map< std::string, float > paramfilter;
    shader_->setFilterCode( filter_.code(), paramfilter );
}

ImageFilterRenderer::~ImageFilterRenderer()
{
    if (buffer_)
        delete buffer_;
    // NB: shader_ is removed with surface
    if (surface_)
        delete surface_;
}

void ImageFilterRenderer::setInputTexture(uint t)
{
    surface_->setTextureIndex( t );
}

uint ImageFilterRenderer::getOutputTexture() const
{
    if (enabled_)
        return buffer_->texture();
    else
        return surface_->textureIndex();
}

void ImageFilterRenderer::draw()
{
    if (enabled_)
    {
        // render filtered surface into frame buffer
        buffer_->begin();
        surface_->draw(glm::identity<glm::mat4>(), buffer_->projection());
        buffer_->end();
    }
}

void ImageFilterRenderer::setFilter(const ImageFilter &f, std::promise<std::string> *ret)
{
    if (f != filter_) {
        // set to a different filter : apply change to shader
        std::map< std::string, float > paramfilter;
        shader_->setFilterCode( f.code(), paramfilter, ret );
    }
    else if (ret != nullptr) {
        ret->set_value("No change.");
    }

    // keep new filter
    filter_ = f;
}


