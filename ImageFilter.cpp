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

#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "defines.h"
#include "Visitor.h"
#include "FrameBuffer.h"
#include "Primitives.h"
#include "Log.h"

#include "ImageFilter.h"

std::string fragmentDeclaration = "#version 330 core\n"
                                  "out vec4 FragColor;\n"
                                  "in vec4 vertexColor;\n"
                                  "in vec2 vertexUV;\n"
                                  "vec3 iChannelResolution[2];\n"
                                  "uniform mat4 iTransform;\n"
                                  "uniform vec4 color;\n"
                                  "uniform float stipple;\n";

std::string filterHeader =  "uniform vec3      iResolution;           // viewport resolution (in pixels)\n"
                            "uniform sampler2D iChannel0;             // input channel (texture)\n"
                            "uniform sampler2D iChannel1;             // input channel (mask)\n"
                            "uniform float     iTime;                 // shader playback time (in seconds)\n"
                            "uniform float     iTimeDelta;            // render time (in seconds)\n"
                            "uniform int       iFrame;                // shader playback frame\n"
                            "uniform vec4      iDate;                 // (year, month, day, time in seconds)\n" ;

std::string filterDefaultCode = "void mainImage( out vec4 fragColor, in vec2 fragCoord )\n"
                                "{\n"
                                "    vec2 uv = fragCoord.xy / iResolution.xy;\n"
                                "    fragColor = texture(iChannel0, uv);\n"
                                "}\n";

std::string fragmentMainCode = "\nvoid main() {\n"
                               "    iChannelResolution[0] = vec3(textureSize(iChannel0, 0), 0.f);\n"
                               "    iChannelResolution[1] = vec3(textureSize(iChannel1, 0), 0.f);\n"
                               "    vec4 texcoord = iTransform * vec4(vertexUV.x, vertexUV.y, 0.0, 1.0);\n"
                               "    mainImage( FragColor, texcoord.xy * iChannelResolution[0].xy );\n"
                               "}\n";


std::string filterBloomCode = "float Threshold = 0.1;"
                              "float Intensity = 1.0;"
                              "float BlurSize = 6.0;"
                              "vec4 BlurColor (in vec2 Coord, in sampler2D Tex, in float MipBias)"
                              "{"
                              "    vec2 TexelSize = MipBias/iChannelResolution[0].xy;"
                              "    vec4  Color = texture(Tex, Coord, MipBias);"
                              "    Color += texture(Tex, Coord + vec2(TexelSize.x,0.0), MipBias);"
                              "    Color += texture(Tex, Coord + vec2(-TexelSize.x,0.0), MipBias);"
                              "    Color += texture(Tex, Coord + vec2(0.0,TexelSize.y), MipBias);"
                              "    Color += texture(Tex, Coord + vec2(0.0,-TexelSize.y), MipBias);"
                              "    Color += texture(Tex, Coord + vec2(TexelSize.x,TexelSize.y), MipBias);"
                              "    Color += texture(Tex, Coord + vec2(-TexelSize.x,TexelSize.y), MipBias);"
                              "    Color += texture(Tex, Coord + vec2(TexelSize.x,-TexelSize.y), MipBias);"
                              "    Color += texture(Tex, Coord + vec2(-TexelSize.x,-TexelSize.y), MipBias);"
                              "    return Color/9.0;"
                              "}"
                              "void mainImage( out vec4 fragColor, in vec2 fragCoord )"
                              "{"
                              "    vec2 uv = (fragCoord.xy/iResolution.xy);"
                              "    vec4 Color = texture(iChannel0, uv);"
                              "    vec4 Highlight = clamp(BlurColor(uv, iChannel0, BlurSize)-Threshold,0.0,1.0)*1.0/(1.0-Threshold);"
                              "    fragColor = 1.0-(1.0-Color)*(1.0-Highlight*Intensity);"
                              "}";


std::string montecarloCode = "#version 330 core\n"
                                 "#define ITER 32\n"
                                 "#define SIZE 100.0\n"
                                 "out vec4 FragColor;\n"
                                 "in vec4 vertexColor;\n"
                                 "in vec2 vertexUV;\n"
                                 "uniform mat4 iTransform;\n"
                                 "uniform vec4 color;\n"
                                 "uniform sampler2D iChannel0;\n"
                                 "uniform sampler2D iChannel1;\n"
                                 "uniform float size;\n"
                                 "uniform float factor;\n"
                                 "void srand(vec2 a, out float r) {r=sin(dot(a,vec2(1233.224,1743.335)));}\n"
                                 "float rand(inout float r) { r=fract(3712.65*r+0.61432); return (r-0.5)*2.0;}\n"
                                 "void main() {"
                                 "vec4 texcoord = iTransform * vec4(vertexUV.x, vertexUV.y, 0.0, 1.0);\n"
                                 "float p = (SIZE * size + 1.0)/textureSize(iChannel0, 0).y * factor;"
                                 "vec4 c=vec4(0.0);"
                                 "float r;"
                                 "srand(vec2(texcoord), r);"
                                 "vec2 rv;"
                                 "for(int i=0;i<ITER;i++) {"
                                 "rv.x=rand(r);"
                                 "rv.y=rand(r);"
                                 "c+=texture(iChannel0,vec2(texcoord)+rv*p)/float(ITER);"
                                 "}"
                                 "FragColor = c;\n"
                                 "}";

std::map< std::string, float > montecarloParam = { { "factor", 0.9 }, {"size", 0.1} };


ImageFilteringShader::ImageFilteringShader(): Shader()
{
    program_ = &custom_shading_;
    custom_shading_.setShaders("shaders/image.vs", "shaders/image.fs");

    Shader::reset();
}

ImageFilteringShader::~ImageFilteringShader()
{
    custom_shading_.reset();
}

void ImageFilteringShader::use()
{
    Shader::use();


    program_->setUniform("iTime", 0.f );
    program_->setUniform("iTimeDelta", 0.f );

    static int f = 0;
    program_->setUniform("iFrame", ++f);
    program_->setUniform("iDate", glm::vec4(0.f, 0.f, 0.f, 0.f) );

    // loop over all uniforms
    for (auto u = uniforms_.begin(); u != uniforms_.end(); ++u)
        // set uniform to current value
        program_->setUniform( u->first, u->second.x );
}

void ImageFilteringShader::reset()
{
    Shader::reset();

    // loop over all uniforms
    for (auto u = uniforms_.begin(); u != uniforms_.end(); ++u)
        // reset current value to detault value
        u->second.x = u->second.y;
}

void ImageFilteringShader::setFragmentCode(const std::string &code, std::map<std::string, float> parameters)
{
    // change the shading code for fragment
    code_ = code;
    custom_shading_.setShaders("shaders/image.vs", code_);

    // set the list of uniforms
    uniforms_.clear();
    for (auto u = parameters.begin(); u != parameters.end(); ++u)
        // set uniform with key name to have default and current values
        uniforms_[u->first] = glm::vec2(u->second, u->second);
}

void ImageFilteringShader::copy(ImageFilteringShader const& S)
{
    // change the shading code for fragment
    code_ = S.code_;
    custom_shading_.setShaders("shaders/image.vs", code_);

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


ImageFilter::ImageFilter(glm::vec3 resolution, bool useAlpha)
{
    shader_ = new ImageFilteringShader;
    surface_ = new Surface(shader_);
    buffer_ = new FrameBuffer( resolution, useAlpha );

    std::string codefilter = fragmentDeclaration + filterHeader + filterDefaultCode + fragmentMainCode;
//    std::string codefilter = fragmentDeclaration + filterHeader + filterBloomCode + fragmentMainCode;

    g_print("Filter code:\n%s", codefilter.c_str());

    std::map< std::string, float > paramfilter;

    shader_->setFragmentCode(codefilter, paramfilter);
}

ImageFilter::~ImageFilter()
{
    if (buffer_)
        delete buffer_;
    // NB: shader_ is removed with surface
    if (surface_)
        delete surface_;
}

void ImageFilter::setInputTexture(uint t)
{
    surface_->setTextureIndex( t );
}

uint ImageFilter::getOutputTexture() const
{
    return buffer_->texture();
}

void ImageFilter::draw()
{
    // render filtered surface into frame buffer
    buffer_->begin();
    surface_->draw(glm::identity<glm::mat4>(), buffer_->projection());
    buffer_->end();
}


