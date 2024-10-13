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
#include <ctime>
#include <algorithm>
#include <regex>

#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Resource.h"
#include "Visitor.h"
#include "FrameBuffer.h"
#include "Primitives.h"
#include "BaseToolkit.h"

#include "Mixer.h"

#include "ImageFilter.h"

std::string fragmentHeader = "#version 330 core\n"
                             "out vec4 FragColor;\n"
                             "in vec4 vertexColor;\n"
                             "in vec2 vertexUV;\n"
                             "vec3 iChannelResolution[2];\n"
                             "uniform mat4      iTransform;\n"
                             "uniform vec3      iResolution;\n"
                             "uniform sampler2D iChannel0;\n"
                             "uniform sampler2D iChannel1;\n"
                             "uniform float     iTime;\n"
                             "uniform float     iTimeDelta;\n"
                             "uniform int       iFrame;\n"
                             "uniform vec4      iDate;\n"
                             "uniform vec4      iMouse;\n";

std::string filterDefault  = "void mainImage( out vec4 fragColor, in vec2 fragCoord )\n"
                             "{\n"
                             "    vec2 uv = fragCoord.xy / iResolution.xy;\n"
                             "    fragColor = texture(iChannel0, uv);\n"
                             "}\n";

std::string fragmentFooter = "void main() {\n"
                             "    iChannelResolution[0] = vec3(textureSize(iChannel0, 0), 0.f);\n"
                             "    iChannelResolution[1] = vec3(textureSize(iChannel1, 0), 0.f);\n"
                             "    vec4 texcoord = iTransform * vec4(vertexUV.x, vertexUV.y, 0.0, 1.0);\n"
                             "    mainImage( FragColor, texcoord.xy * iResolution.xy );\n"
                             "}\n";

std::list< FilteringProgram > FilteringProgram::presets = {
    FilteringProgram(),
    FilteringProgram("Blend",    "shaders/filters/blend.glsl",      "",     { }),
    FilteringProgram("Bilateral","shaders/filters/focus.glsl",      "",     { }),
    FilteringProgram("Pixelate", "shaders/filters/pixelate.glsl",   "",     { }),
    FilteringProgram("Earlybird","shaders/filters/earlybird.glsl",  "",     { }),
    FilteringProgram("Bloom",    "shaders/filters/bloom.glsl",      "",     { }),
    FilteringProgram("Bokeh",    "shaders/filters/bokeh.glsl",      "",     { }),
    FilteringProgram("Talk",     "shaders/filters/talk.glsl",       "",     { }),
    FilteringProgram("Stippling","shaders/filters/stippling.glsl",  "",     { }),
    FilteringProgram("Dithering","shaders/filters/dithering.glsl",  "",     { }),
    FilteringProgram("Fisheye",  "shaders/filters/fisheye.glsl",    "",     { }),
    FilteringProgram("Logo",     "shaders/filters/logo.glsl",       "",     { })
};

std::string FilteringProgram::getFilterCodeInputs()
{
    static std::string filterHeaderHelp =  "vec3      iResolution;           // viewport resolution (in pixels)\n"
                                           "float     iTime;                 // shader playback time (in seconds)\n"
                                           "float     iTimeDelta;            // render time (in seconds)\n"
                                           "int       iFrame;                // shader playback frame\n"
                                           "vec3      iChannelResolution[2]; // input channels resolution (in pixels)\n"
                                           "sampler2D iChannel0;             // input channel 0 (source).\n"
                                           "sampler2D iChannel1;             // input channel 1 (display loopback).\n"
                                           "vec4      iDate;                 // (year, month, day, time in seconds)\n"
                                           "vec4      iMouse;                // simulate mouse input with sliders:";
    return filterHeaderHelp;
}

std::string FilteringProgram::getFilterCodeDefault()
{
    return filterDefault;
}

glm::vec4 FilteringProgram::iMouse = glm::vec4(0.f,0.f,0.f,0.f);

////////////////////////////////////////
/////                                 //
////  PROGRAM DEFINING A FILTER      ///
///                                 ////
////////////////////////////////////////

FilteringProgram::FilteringProgram() : name_("Default"), code_({"shaders/filters/default.glsl",""}), two_pass_filter_(false)
{

}

FilteringProgram::FilteringProgram(const std::string &name, const std::string &first_pass, const std::string &second_pass,
                         const std::map<std::string, float> &parameters) :
    name_(name), code_({first_pass, second_pass}), parameters_(parameters)
{
    two_pass_filter_ = !second_pass.empty();
}

FilteringProgram::FilteringProgram(const FilteringProgram &other) :
    name_(other.name_), code_(other.code_), two_pass_filter_(other.two_pass_filter_), parameters_(other.parameters_)
{

}

FilteringProgram& FilteringProgram::operator= (const FilteringProgram& other)
{
    if (this != &other) {
        this->name_ = other.name_;
        this->code_ = other.code_;
        this->parameters_.clear();
        this->parameters_ = other.parameters_;
        this->two_pass_filter_ = other.two_pass_filter_;
    }

    return *this;
}

std::pair< std::string, std::string > FilteringProgram::code()
{
    // code for filter can be provided by the name of a ressource file
    if (Resource::hasPath(code_.first))
        code_.first = Resource::getText(code_.first);
    if (Resource::hasPath(code_.second))
        code_.second = Resource::getText(code_.second);

    return code_;
}

bool FilteringProgram::operator!= (const FilteringProgram& other) const
{
    if (this->code_.first != other.code_.first)
        return true;
    if (this->code_.second != other.code_.second)
        return true;

    return false;
}

bool FilteringProgram::hasParameter(const std::string &p)
{
    return parameters_.find(p) != parameters_.end();
}

void FilteringProgram::removeParameter(const std::string &p)
{
    if (hasParameter(p))
        parameters_.erase(p);
}


////////////////////////////////////////
/////                                 //
////  IMAGE SHADER FOR FILTERS       ///
///                                 ////
////////////////////////////////////////


ImageFilteringShader::ImageFilteringShader(): ImageShader()
{
    program_ = &custom_shading_;

    shader_code_ = fragmentHeader + filterDefault + fragmentFooter;
    custom_shading_.setShaders("shaders/image.vs", shader_code_);

    timer_ = g_timer_new ();
    iTime_ = 0.0;
    iFrame_ = 0;
    uniforms_changed_ = true;

    ImageShader::reset();
}

ImageFilteringShader::~ImageFilteringShader()
{
    custom_shading_.reset();
    g_timer_destroy(timer_);
}

void ImageFilteringShader::update(float dt)
{
    iTime_ += 0.001 * dt;
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

    // scale iMouse to resolution
    program_->setUniform("iMouse", FilteringProgram::iMouse );

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
    for (auto u = uniforms_.begin(); u != uniforms_.end(); ) {
        // set uniform to current value
        if ( program_->setUniform(u->first, u->second) )
            // uniform variable could be set, keep it
            ++u;
        else {
            // uniform variable does not exist in code, remove it
            u = uniforms_.erase(u);
            uniforms_changed_ = true;
        }
    }
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
        // ensure code to compile is correct
        if (code_.empty())
            code_ = filterDefault;
        // shader is composed of a header, the given code and a footer
        shader_code_ = fragmentHeader + code_ + fragmentFooter;
        // shift line numbers by number of lines in header
        static std::string::difference_type n = std::count(fragmentHeader.begin(), fragmentHeader.end(), '\n');
        // launch build
        custom_shading_.setShaders("shaders/image.vs", shader_code_, (int)n, ret);
    }
    else if (ret != nullptr) {
        ret->set_value("No change");
    }
}

void ImageFilteringShader::copy(ImageFilteringShader const& S)
{
    ImageShader::copy(S);

    // change the shading code for fragment
    shader_code_ = S.shader_code_;
    custom_shading_.setShaders("shaders/image.vs", shader_code_);
}


////////////////////////////////////////
/////                                 //
////  GENERIC IMAGE FILTER           ///
///                                 ////
////////////////////////////////////////

ImageFilter::ImageFilter (): FrameBufferFilter(), buffers_({nullptr, nullptr}), channel1_output_session(true)
{
    // surface and shader for first pass
    shaders_.first  = new ImageFilteringShader;
    surfaces_.first = new Surface(shaders_.first);

    // surface and shader for second pass
    shaders_.second  = new ImageFilteringShader;
    surfaces_.second = new Surface(shaders_.second);
}

ImageFilter::~ImageFilter ()
{
    if ( buffers_.first!= nullptr )
        delete buffers_.first;
    if ( buffers_.second!= nullptr )
        delete buffers_.second;

    delete surfaces_.first;
    delete surfaces_.second;
    // NB: shaders_ are removed with surface
}

void ImageFilter::reset ()
{
    shaders_.first->reset();

    if ( program_.isTwoPass() )
        shaders_.second->reset();
}

double ImageFilter::updateTime ()
{
    return shaders_.first->iTime_;
}

void ImageFilter::update (float dt)
{
    shaders_.first->update(dt);

    if ( program_.isTwoPass() )
        shaders_.second->update(dt);

    // uniforms changed in main shader
    if ( shaders_.first->uniforms_changed_ ) {
        // loop over the parameters of the program...
        std::map<std::string, float> __P = program_.parameters();
        for (auto param = __P.begin(); param != __P.end(); ++param) {
            // .. and remove the parameters that are not valid uniforms
            if (shaders_.first->uniforms_.count(param->first) < 1)
                program_.removeParameter(param->first);
        }
        // done
        shaders_.first->uniforms_changed_ = false;
    }
}

uint ImageFilter::texture () const
{
    if (buffers_.first && buffers_.second)
        return program_.isTwoPass() ? buffers_.second->texture() : buffers_.first->texture();

    if (input_)
        return input_->texture();

    return Resource::getTextureBlack();
}

glm::vec3 ImageFilter::resolution () const
{
    if (buffers_.first && buffers_.second)
        return program_.isTwoPass() ? buffers_.second->resolution() : buffers_.first->resolution();

    if (input_)
        return input_->resolution();

    return glm::vec3(1,1,0);
}

void ImageFilter::draw (FrameBuffer *input)
{
    bool forced = false;

    // if input changed (typically on first draw)
    if (input_ != input || buffers_.first == nullptr || buffers_.second == nullptr) {
        // keep reference to input framebuffer
        input_ = input;
        // create first-pass surface and shader, taking as texture the input framebuffer
        surfaces_.first->setTextureIndex( input_->texture() );
        shaders_.first->secondary_texture = input_->texture();
        // (re)create framebuffer for result of first-pass
        if (buffers_.first != nullptr)
            delete buffers_.first;
        // FBO
        buffers_.first = new FrameBuffer( input_->resolution(), input_->flags() );
        // enforce framebuffer if first-pass is created now, filled with input framebuffer
        input_->blit( buffers_.first );
        // create second-pass surface and shader, taking as texture the first-pass framebuffer
        surfaces_.second->setTextureIndex( buffers_.first->texture() );
        shaders_.second->secondary_texture = input_->texture();
        // (re)create framebuffer for result of second-pass
        if (buffers_.second != nullptr)
            delete buffers_.second;
        buffers_.second = new FrameBuffer( buffers_.first->resolution(), buffers_.first->flags() );
        // forced draw
        forced = true;
    }

    if ( enabled() || forced )
    {
        // FIRST PASS
        if (channel1_output_session)
            shaders_.first->secondary_texture = Mixer::manager().session()->frame()->texture();
        // render input surface into frame buffer
        buffers_.first->begin();
        surfaces_.first->draw(glm::identity<glm::mat4>(), buffers_.first->projection());
        buffers_.first->end();

        // SECOND PASS
        if ( program_.isTwoPass() ) {
            if (channel1_output_session)
                shaders_.second->secondary_texture = Mixer::manager().session()->frame()->texture();
            // render filtered surface from first pass into frame buffer
            buffers_.second->begin();
            surfaces_.second->draw(glm::identity<glm::mat4>(), buffers_.second->projection());
            buffers_.second->end();
        }
    }
}

void ImageFilter::accept (Visitor& v)
{
    FrameBufferFilter::accept(v);
    v.visit(*this);
}


FilteringProgram ImageFilter::program () const
{
    return program_;
}

#define REGEX_UNIFORM_DECLARATION "uniform\\s+float\\s+"
#define REGEX_UNIFORM_VALUE "(\\s*=\\s*[[:digit:]](\\.[[:digit:]])?)?\\s*\\;"

void ImageFilter::setProgram(const FilteringProgram &f, std::promise<std::string> *ret)
{
    // always keep local copy
    program_ = f;

    // change code
    std::pair<std::string, std::string> codes = program_.code();

    // FIRST PASS
    // set code to the shader for first-pass
    shaders_.first->setCode( codes.first, ret );

    // Parse code to detect additional declaration of uniform variables
    // Search for "uniform float", a variable name, with possibly a '=' and float value
    std::string glslcode(codes.first);
    std::smatch found_uniform;
    std::regex is_a_uniform(REGEX_UNIFORM_DECLARATION "[[:alpha:]]+" REGEX_UNIFORM_VALUE);
    // loop over every uniform declarations in the GLSL code
    while (std::regex_search(glslcode, found_uniform, is_a_uniform)) {
        // found a complete declaration of uniform variable
        std::string declaration = found_uniform.str();
        // extract variable name by erasing everything else
        std::string varname =
            std::regex_replace(declaration,std::regex(REGEX_UNIFORM_DECLARATION),"");
        varname = std::regex_replace(varname, std::regex(REGEX_UNIFORM_VALUE), "");
        // add to list of parameters if was not already there, with default value
        if ( !program_.hasParameter(varname) )
            program_.setParameter(varname, 0.f);

        // try to find a value in uniform declaration, and set parameter value if valid
        float val = 0.f;
        std::smatch found_value;
        std::regex is_a_float_value("[[:digit:]](\\.[[:digit:]])?");
        if (std::regex_search(declaration, found_value, is_a_float_value)) {
            // set value only if a value is given
            if ( BaseToolkit::is_a_value(found_value.str(), &val) )
                program_.setParameter(varname, val);
        }
        // keep parsing
        glslcode = found_uniform.suffix().str();
    }

    // SECOND PASS
    if ( program_.isTwoPass() )
        // set the code to the shader for second-pass
        shaders_.second->setCode(codes.second);

    // UPDATE UNIFORMS
    updateParameters();
}

void ImageFilter::updateParameters()
{
    // change uniforms
    shaders_.first->uniforms_ = program_.parameters();

    if ( program_.isTwoPass() )
        shaders_.second->uniforms_ = program_.parameters();
}

void ImageFilter::setProgramParameters(const std::map< std::string, float > &parameters)
{
    for (const auto &p : parameters) {
        if (p.second == NAN || p.first.empty())
            return;
    }

    program_.setParameters(parameters);
    updateParameters();
}

void ImageFilter::setProgramParameter(const std::string &p, float value)
{
    if (value == NAN || p.empty())
        return;

    program_.setParameter(p, value);
    updateParameters();
}

////////////////////////////////////////
/////                                 //
////  RESAMPLING FILTERS             ///
///                                 ////
////////////////////////////////////////

const char* ResampleFilter::factor_label[ResampleFilter::RESAMPLE_INVALID] = {
    "Double resolution", "Half resolution", "Quarter resolution"
};

std::vector< FilteringProgram > ResampleFilter::programs_ = {
    FilteringProgram("Double",   "shaders/filters/resample_double.glsl",  "",   { }),
    FilteringProgram("Half",     "shaders/filters/resample_half.glsl",  "",     { }),
    FilteringProgram("Quarter",  "", "shaders/filters/resample_half.glsl",      { })
};

ResampleFilter::ResampleFilter (): ImageFilter(), factor_(RESAMPLE_INVALID)
{
    channel1_output_session = false;
}

void ResampleFilter::setFactor(int factor)
{
    factor_ = (ResampleFactor) CLAMP(factor, RESAMPLE_DOUBLE, RESAMPLE_INVALID-1);
    setProgram( programs_[ (int) factor_] );

    // force re-init
    input_ = nullptr;
}

bool ResampleFilter::setFactor(const std::string &label)
{
    std::string target_factor = label;
    std::transform(target_factor.begin(), target_factor.end(), target_factor.begin(), ::tolower);

    // find method by name
    int __f = ResampleFilter::RESAMPLE_DOUBLE;
    for (; __f != ResampleFilter::RESAMPLE_INVALID; __f++) {
        std::string _b = ResampleFilter::factor_label[__f];
        auto loc = _b.find_first_of(" ");
        if (loc != std::string::npos)
            _b = _b.erase(loc);
        std::transform(_b.begin(), _b.end(), _b.begin(), ::tolower);
        if (target_factor.compare(_b) == 0)
            break;
    }

    // invalid filter name
    if (__f == ResampleFilter::RESAMPLE_INVALID)
        return false;

    // Apply if provided method name is different from current
    if (factor() != __f)
        setFactor(__f);

    return true;
}

void ResampleFilter::draw (FrameBuffer *input)
{
    bool forced = false;

    // Default
    if (factor_ == RESAMPLE_INVALID)
        setFactor( RESAMPLE_DOUBLE );

    // if input changed (typically on first draw)
    if (input_ != input) {
        // keep reference to input framebuffer
        input_ = input;

        // create first-pass surface and shader, taking as texture the input framebuffer
        surfaces_.first->setTextureIndex( input_->texture() );
        shaders_.first->secondary_texture = input_->texture();
        // (re)create framebuffer for result of first-pass
        if (buffers_.first != nullptr)
            delete buffers_.first;
        // set resolution depending on resample factor
        glm::vec3 res = input_->resolution();
        switch (factor_) {
        case RESAMPLE_DOUBLE:
            res *= 2.;
            break;
        case RESAMPLE_HALF:
        case RESAMPLE_QUARTER:
            res /= 2.;
            break;
        default:
        case RESAMPLE_INVALID:
            break;
        }
        buffers_.first = new FrameBuffer( res, input_->flags() );
        // enforce framebuffer if first-pass is created now, filled with input framebuffer
        input_->blit( buffers_.first );

        // SECOND PASS for QUARTER resolution (divide by 2 after first pass divide by 2)
        // create second-pass surface and shader, taking as texture the first-pass framebuffer
        surfaces_.second->setTextureIndex( buffers_.first->texture() );
        shaders_.second->secondary_texture = input_->texture();
        // (re)create framebuffer for result of second-pass
        if (buffers_.second != nullptr)
            delete buffers_.second;
        res /= 2.;
        buffers_.second = new FrameBuffer( res, buffers_.first->flags() );
        // forced draw
        forced = true;
    }

    if ( enabled() || forced )
    {
        // FIRST PASS
        // render input surface into frame buffer
        buffers_.first->begin();
        surfaces_.first->draw(glm::identity<glm::mat4>(), buffers_.first->projection());
        buffers_.first->end();

        // SECOND PASS
        if ( program().isTwoPass() ) {
            // render filtered surface from first pass into frame buffer
            buffers_.second->begin();
            surfaces_.second->draw(glm::identity<glm::mat4>(), buffers_.second->projection());
            buffers_.second->end();
        }
    }
}

void ResampleFilter::accept (Visitor& v)
{
    FrameBufferFilter::accept(v);
    v.visit(*this);
}




////////////////////////////////////////
/////                                 //
////  BLURING FILTERS                ///
///                                 ////
////////////////////////////////////////

const char* BlurFilter::method_label[BlurFilter::BLUR_INVALID] = {
    "Gaussian", "Scattered", "Opening", "Closing", "Fast"
};

std::vector< FilteringProgram > BlurFilter::programs_ = {
    FilteringProgram("Gaussian", "shaders/filters/blur_1.glsl",        "shaders/filters/blur_2.glsl",     { { "Radius", 0.55} }),
    FilteringProgram("Scattered","shaders/filters/hashedblur.glsl",    "",     { { "Radius", 0.5}, { "Iterations", 0.25 } }),
    FilteringProgram("Opening",  "shaders/filters/hashederosion.glsl", "shaders/filters/hasheddilation.glsl",   { { "Radius", 0.5} }),
    FilteringProgram("Closing",  "shaders/filters/hasheddilation.glsl","shaders/filters/hashederosion.glsl",   { { "Radius", 0.5} }),
    FilteringProgram("Fast",     "shaders/filters/blur.glsl", "", { })
};

BlurFilter::BlurFilter (): ImageFilter(), method_(BLUR_INVALID), mipmap_buffer_(nullptr)
{
    mipmap_surface_ = new Surface;
    channel1_output_session = false;
}

BlurFilter::~BlurFilter ()
{
    delete mipmap_surface_;
    if ( mipmap_buffer_!= nullptr )
        delete mipmap_buffer_;
}

void BlurFilter::setMethod(int method)
{
    method_ = (BlurMethod) CLAMP(method, BLUR_GAUSSIAN, BLUR_INVALID-1);

    setProgram( programs_[ (int) method_] );
}

bool BlurFilter::setMethod(const std::string &label)
{
    std::string target_method = label;
    std::transform(target_method.begin(), target_method.end(), target_method.begin(), ::tolower);

    // find method by name
    int __m = BlurFilter::BLUR_GAUSSIAN;
    for (; __m != BlurFilter::BLUR_INVALID; __m++) {
        std::string _b = BlurFilter::method_label[__m];
        auto loc = _b.find_first_of(" ");
        if (loc != std::string::npos)
            _b = _b.erase(loc);
        std::transform(_b.begin(), _b.end(), _b.begin(), ::tolower);
        if (target_method.compare(_b) == 0)
            break;
    }

    // invalid filter name
    if (__m == BlurFilter::BLUR_INVALID)
        return false;

    // Apply if provided method name is different from current
    if (method() != __m)
        setMethod(__m);

    return true;
}

void BlurFilter::draw (FrameBuffer *input)
{
    bool forced = false;

    // Default to Gaussian blur
    if (method_ == BLUR_INVALID)
        setMethod( BLUR_GAUSSIAN );

    // if input changed (typically on first draw)
    if (input_ != input || buffers_.first == nullptr || buffers_.second == nullptr) {
        // keep reference to input framebuffer
        input_ = input;

        // create zero-pass surface taking as texture the input framebuffer
        mipmap_surface_->setTextureIndex( input_->texture() );

        // FBO with mipmapping
        // (re)create framebuffer for mipmapped input
        if ( mipmap_buffer_!= nullptr )
            delete mipmap_buffer_;
        FrameBuffer::FrameBufferFlags f = input_->flags();
        mipmap_buffer_ = new FrameBuffer( input_->resolution(), f | FrameBuffer::FrameBuffer_mipmap );
        // enforce framebuffer created now, filled with input framebuffer
        input_->blit( mipmap_buffer_ );

        // create first-pass surface and shader, taking as texture the input framebuffer
        surfaces_.first->setTextureIndex( mipmap_buffer_->texture() );
        shaders_.first->secondary_texture = input_->texture();
        // (re)create framebuffer for result of first-pass
        if (buffers_.first != nullptr)
            delete buffers_.first;
        buffers_.first = new FrameBuffer( input_->resolution(), f | FrameBuffer::FrameBuffer_mipmap );
        // enforce framebuffer of first-pass is created now, filled with input framebuffer
        mipmap_buffer_->blit( buffers_.first );

        // create second-pass surface and shader, taking as texture the first-pass framebuffer
        surfaces_.second->setTextureIndex( buffers_.first->texture() );
        shaders_.second->secondary_texture = input_->texture();
        // (re)create framebuffer for result of second-pass
        if (buffers_.second != nullptr)
            delete buffers_.second;
        buffers_.second = new FrameBuffer( input_->resolution(), f );
        // forced draw
        forced = true;
    }

    if ( enabled() || forced )
    {
        // ZERO PASS
        // render input surface into frame buffer with Mipmapping (Levels of Details)
        mipmap_buffer_->begin();
        mipmap_surface_->draw(glm::identity<glm::mat4>(), mipmap_buffer_->projection());
        mipmap_buffer_->end();

        // FIRST PASS
        // render mipmapped texture into frame buffer
        buffers_.first->begin();
        surfaces_.first->draw(glm::identity<glm::mat4>(), buffers_.first->projection());
        buffers_.first->end();

        // SECOND PASS
        if ( program().isTwoPass() ) {
            // render filtered surface from first pass into frame buffer
            buffers_.second->begin();
            surfaces_.second->draw(glm::identity<glm::mat4>(), buffers_.second->projection());
            buffers_.second->end();
        }
    }
}

void BlurFilter::accept (Visitor& v)
{
    FrameBufferFilter::accept(v);
    v.visit(*this);
}


////////////////////////////////////////
/////                                 //
////  SHARPENING FILTERS             ///
///                                 ////
////////////////////////////////////////

const char* SharpenFilter::method_label[SharpenFilter::SHARPEN_INVALID] = {
    "Unsharp mask", "Convolution", "Edge", "White hat", "Black hat"
};

std::vector< FilteringProgram > SharpenFilter::programs_ = {
    FilteringProgram("UnsharpMask", "shaders/filters/sharpen_1.glsl",  "shaders/filters/sharpen_2.glsl",     { { "Amount", 0.25} }),
    FilteringProgram("Sharpen",      "shaders/filters/sharpen.glsl",    "",   { { "Amount", 0.5} }),
    FilteringProgram("Sharp Edge",   "shaders/filters/sharpenedge.glsl","",   { { "Amount", 0.25} }),
    FilteringProgram("TopHat",       "shaders/filters/erosion.glsl",    "shaders/filters/tophat.glsl",    { { "Radius", 0.5} }),
    FilteringProgram("BlackHat",     "shaders/filters/dilation.glsl",   "shaders/filters/blackhat.glsl",  { { "Radius", 0.5} }),
};

SharpenFilter::SharpenFilter (): ImageFilter(), method_(SHARPEN_INVALID)
{
    channel1_output_session = false;
}

void SharpenFilter::setMethod(int method)
{
    method_ = (SharpenMethod) CLAMP(method, SHARPEN_MASK, SHARPEN_INVALID-1);
    setProgram( programs_[ (int) method_] );
}

bool SharpenFilter::setMethod(const std::string &label)
{
    std::string target_method = label;
    std::transform(target_method.begin(), target_method.end(), target_method.begin(), ::tolower);

    // find method by name
    int __m = SharpenFilter::SHARPEN_MASK;
    for (; __m != SharpenFilter::SHARPEN_INVALID; __m++) {
        // get first word of label, in lower case
        std::string _b = SharpenFilter::method_label[__m];
        auto loc = _b.find_first_of(" ");
        if (loc != std::string::npos)
            _b = _b.erase(loc);
        std::transform(_b.begin(), _b.end(), _b.begin(), ::tolower);
        if (target_method.compare(_b) == 0)
            break;
    }

    if (__m == SharpenFilter::SHARPEN_INVALID)
        return false;

    // Apply if provided method name is different from current
    if (method() != __m)
        setMethod(__m);

    return true;
}

void SharpenFilter::draw (FrameBuffer *input)
{
    // Default
    if (method_ == SHARPEN_INVALID)
        setMethod( SHARPEN_MASK );

    ImageFilter::draw( input );
}

void SharpenFilter::accept (Visitor& v)
{
    FrameBufferFilter::accept(v);
    v.visit(*this);
}



////////////////////////////////////////
/////                                 //
////  SMOOTHING FILTERS              ///
///                                 ////
////////////////////////////////////////

const char* SmoothFilter::method_label[SmoothFilter::SMOOTH_INVALID] = {
    "Bilateral", "Kuwahara", "Opening", "Closing", "Erosion", "Dilation", "Denoise", "Noise add", "Grain add"
};

std::vector< FilteringProgram > SmoothFilter::programs_ = {
    FilteringProgram("Bilateral","shaders/filters/bilinear.glsl",   "",     { { "Factor", 0.5} }),
    FilteringProgram("Kuwahara", "shaders/filters/kuwahara.glsl",   "",     { { "Radius", 0.5} }),
    FilteringProgram("Opening",  "shaders/filters/erosion.glsl",    "shaders/filters/dilation.glsl",  { { "Radius", 0.5} }),
    FilteringProgram("Closing",  "shaders/filters/dilation.glsl",   "shaders/filters/erosion.glsl",   { { "Radius", 0.5} }),
    FilteringProgram("Erosion",  "shaders/filters/erosion.glsl",    "",     { { "Radius", 0.5} }),
    FilteringProgram("Dilation", "shaders/filters/dilation.glsl",   "",     { { "Radius", 0.5} }),
    FilteringProgram("Denoise",  "shaders/filters/denoise.glsl",    "",     { { "Threshold", 0.5} }),
    FilteringProgram("Noise",    "shaders/filters/noise.glsl",      "",     { { "Amount", 0.25} }),
    FilteringProgram("Grain",    "shaders/filters/grain.glsl",      "",     { { "Amount", 0.5} })
};

SmoothFilter::SmoothFilter (): ImageFilter(), method_(SMOOTH_INVALID)
{
    channel1_output_session = false;
}

void SmoothFilter::setMethod(int method)
{
    method_ = (SmoothMethod) CLAMP(method, SMOOTH_BILINEAR, SMOOTH_INVALID-1);
    setProgram( programs_[ (int) method_] );
}

bool SmoothFilter::setMethod(const std::string &label)
{
    std::string target_method = label;
    std::transform(target_method.begin(), target_method.end(), target_method.begin(), ::tolower);

    // find method by name
    int __m = SmoothFilter::SMOOTH_BILINEAR;
    for (; __m != SmoothFilter::SMOOTH_INVALID; __m++) {
        std::string _b = SmoothFilter::method_label[__m];
        auto loc = _b.find_first_of(" ");
        if (loc != std::string::npos)
            _b = _b.erase(loc);
        std::transform(_b.begin(), _b.end(), _b.begin(), ::tolower);
        if (target_method.compare(_b) == 0)
            break;
    }

    if (__m == SmoothFilter::SMOOTH_INVALID)
        return false;

    // Apply if provided method name is different from current
    if (method() != __m)
        setMethod(__m);

    return true;
}

void SmoothFilter::draw (FrameBuffer *input)
{
    // Default
    if (method_ == SMOOTH_INVALID)
        setMethod( SMOOTH_BILINEAR );

    ImageFilter::draw( input );
}

void SmoothFilter::accept (Visitor& v)
{
    FrameBufferFilter::accept(v);
    v.visit(*this);
}


////////////////////////////////////////
/////                                 //
////  EDGE FILTERS                   ///
///                                 ////
////////////////////////////////////////

const char* EdgeFilter::method_label[EdgeFilter::EDGE_INVALID] = {
    "Sobel", "Freichen", "Thresholding", "Contour"
};

std::vector< FilteringProgram > EdgeFilter::programs_ = {
    FilteringProgram("Sobel",    "shaders/filters/sobel.glsl",      "",     { { "Factor", 0.5} }),
    FilteringProgram("Freichen", "shaders/filters/freichen.glsl",   "",     { { "Factor", 0.5} }),
    FilteringProgram("Edge",     "shaders/filters/focus.glsl",      "shaders/filters/edge.glsl",     { { "Threshold", 0.5} }),
    FilteringProgram("Contour",  "shaders/filters/sharpen_1.glsl",  "shaders/filters/contour_2.glsl",     { { "Amount", 0.5} }),
};

EdgeFilter::EdgeFilter (): ImageFilter(), method_(EDGE_INVALID)
{
    channel1_output_session = false;
}

void EdgeFilter::setMethod(int method)
{
    method_ = (EdgeMethod) CLAMP(method, EDGE_SOBEL, EDGE_INVALID-1);
    setProgram( programs_[ (int) method_] );
}

bool EdgeFilter::setMethod(const std::string &label)
{
    std::string target_method = label;
    std::transform(target_method.begin(), target_method.end(), target_method.begin(), ::tolower);

    // find method by name
    int __m = EdgeFilter::EDGE_SOBEL;
    for (; __m != EdgeFilter::EDGE_INVALID; __m++) {
        std::string _b = EdgeFilter::method_label[__m];
        auto loc = _b.find_first_of(" ");
        if (loc != std::string::npos)
            _b = _b.erase(loc);
        std::transform(_b.begin(), _b.end(), _b.begin(), ::tolower);
        if (target_method.compare(_b) == 0)
            break;
    }

    if (__m == EdgeFilter::EDGE_INVALID)
        return false;

    // Apply if provided method name is different from current
    if (method() != __m)
        setMethod(__m);

    return true;
}

void EdgeFilter::draw (FrameBuffer *input)
{
    // Default
    if (method_ == EDGE_INVALID)
        setMethod( EDGE_SOBEL );

    ImageFilter::draw( input );
}

void EdgeFilter::accept (Visitor& v)
{
    FrameBufferFilter::accept(v);
    v.visit(*this);
}



////////////////////////////////////////
/////                                 //
////  ALPHA FILTERS                  ///
///                                 ////
////////////////////////////////////////

const char* AlphaFilter::operation_label[AlphaFilter::ALPHA_INVALID] = {
    "Chromakey", "Lumakey", "Fill transparent"
};

std::vector< FilteringProgram > AlphaFilter::programs_ = {
    FilteringProgram("Chromakey","shaders/filters/chromakey.glsl",   "",  { { "Threshold", 0.5}, { "Red", 0.0}, { "Green", 1.0}, { "Blue", 0.0}, { "Tolerance", 0.5} } ),
    FilteringProgram("Lumakey",  "shaders/filters/lumakey.glsl",     "",  { { "Threshold", 0.5}, { "Luminance", 0.0}, { "Tolerance", 0.5} } ),
    FilteringProgram("coloralpha","shaders/filters/coloralpha.glsl", "",  { { "Red", 0.0}, { "Green", 1.0}, { "Blue", 0.0} } )
};

AlphaFilter::AlphaFilter (): ImageFilter(), operation_(ALPHA_INVALID)
{
    channel1_output_session = false;
}

void AlphaFilter::setOperation(int op)
{
    operation_ = (AlphaOperation) CLAMP(op, ALPHA_CHROMAKEY, ALPHA_INVALID-1);
    setProgram( programs_[ (int) operation_] );
}

bool AlphaFilter::setOperation(const std::string &label)
{
    std::string target_op = label;
    std::transform(target_op.begin(), target_op.end(), target_op.begin(), ::tolower);

    // find method by name
    int __o = AlphaFilter::ALPHA_CHROMAKEY;
    for (; __o != AlphaFilter::ALPHA_INVALID; __o++) {
        std::string _b = AlphaFilter::operation_label[__o];
        auto loc = _b.find_first_of(" ");
        if (loc != std::string::npos)
            _b = _b.erase(loc);
        std::transform(_b.begin(), _b.end(), _b.begin(), ::tolower);
        if (target_op.compare(_b) == 0)
            break;
    }

    if (__o == AlphaFilter::ALPHA_INVALID)
        return false;

    // Apply if provided method name is different from current
    if (operation() != __o)
        setOperation(__o);

    return true;
}

void AlphaFilter::draw (FrameBuffer *input)
{
    // Default
    if (operation_ == ALPHA_INVALID)
        setOperation( ALPHA_CHROMAKEY );

    ImageFilter::draw( input );
}

void AlphaFilter::accept (Visitor& v)
{
    FrameBufferFilter::accept(v);
    v.visit(*this);
}



