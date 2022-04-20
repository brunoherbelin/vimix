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

#include <fstream>
#include <sstream>
#include <iostream>
#include <chrono>
#include <ctime>

#include <glad/glad.h> 
#include <GLFW/glfw3.h>

#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include "Resource.h"
#include "FrameBuffer.h"
#include "Log.h"
#include "Visitor.h"
#include "BaseToolkit.h"
#include "RenderingManager.h"

#include "Shader.h"

#ifndef NDEBUG
#define SHADER_DEBUG
#endif

// Globals
ShadingProgram *ShadingProgram::currentProgram_ = nullptr;
ShadingProgram simpleShadingProgram("shaders/simple.vs", "shaders/simple.fs");

// Blending presets for matching with Shader::BlendModes:
GLenum blending_equation[9] = { GL_FUNC_ADD,  // normal
                                GL_FUNC_ADD,  // screen
                                GL_FUNC_REVERSE_SUBTRACT, // subtract
                                GL_FUNC_ADD,  // multiply
                                GL_FUNC_ADD,  // soft light
                                GL_FUNC_ADD,  // hard light
                                GL_FUNC_REVERSE_SUBTRACT, // soft subtract
                                GL_MAX,       // lighten only
                                GL_FUNC_ADD};
GLenum blending_source_function[9] = { GL_ONE,  // normal
                                       GL_ONE,  // screen
                                       GL_SRC_COLOR,  // subtract (can be GL_ONE)
                                       GL_DST_COLOR,  // multiply : src x dst color
                                       GL_DST_COLOR,  // soft light : src x dst color
                                       GL_SRC_COLOR,  // hard light : src x src color
                                       GL_DST_COLOR,  // soft subtract
                                       GL_ONE,        //  lighten only
                                       GL_ONE};
GLenum blending_destination_function[9] = {GL_ONE_MINUS_SRC_ALPHA,// normal
                                           GL_ONE,   // screen
                                           GL_ONE,   // subtract
                                           GL_ONE_MINUS_SRC_ALPHA, // multiply
                                           GL_ONE,   // soft light
                                           GL_ONE,   // hard light
                                           GL_ONE,   // soft subtract
                                           GL_ONE,   // lighten only
                                           GL_ZERO};

ShadingProgram::ShadingProgram(const std::string& vertex, const std::string& fragment) :
    id_(0), need_compile_(true), vertex_(vertex), fragment_(fragment)
{
}

void ShadingProgram::setShaders(const std::string& vertex, const std::string& fragment)
{
    vertex_ = vertex;
    fragment_ = fragment;
    need_compile_ = true;
}

bool ShadingProgram::compile()
{
    char infoLog[1024];
    int success = GL_FALSE;

    std::string vertex_code = vertex_;
    if (Resource::hasPath(vertex_))
        vertex_code = Resource::getText(vertex_);
    std::string fragment_code = fragment_;
    if (Resource::hasPath(fragment_))
        fragment_code = Resource::getText(fragment_);

    // VERTEX SHADER
    const char* vcode = vertex_code.c_str();
    unsigned int vertex_id_ = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_id_, 1, &vcode, NULL);
    glCompileShader(vertex_id_);

    glGetShaderiv(vertex_id_, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertex_id_, 1024, NULL, infoLog);
        Log::Warning("Error compiling Vertex ShadingProgram:\n%s", infoLog);
    }
    else {
        // FRAGMENT SHADER
        const char* fcode = fragment_code.c_str();
        unsigned int fragment_id_ = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment_id_, 1, &fcode, NULL);
        glCompileShader(fragment_id_);

        glGetShaderiv(fragment_id_, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(fragment_id_, 1024, NULL, infoLog);
            Log::Warning("Error compiling Fragment ShadingProgram:\n%s", infoLog);
            glDeleteShader(vertex_id_);
        }
        else {
            // LINK PROGRAM

            // create new GL Program
            if (id_ != 0)
                glDeleteProgram(id_);
            id_ = glCreateProgram();

            // attach shaders and link
            glAttachShader(id_, vertex_id_);
            glAttachShader(id_, fragment_id_);
            glLinkProgram(id_);

            glGetProgramiv(id_, GL_LINK_STATUS, &success);
            if (!success) {
                glGetProgramInfoLog(id_, 1024, NULL, infoLog);
                Log::Warning("Error linking ShadingProgram:\n%s", infoLog);
                glDeleteProgram(id_);
                id_ = 0;
            }
            else {
                // all good, set default uniforms
                glUseProgram(id_);
                glUniform1i(glGetUniformLocation(id_, "iChannel0"), 0);
                glUniform1i(glGetUniformLocation(id_, "iChannel1"), 1);
#ifdef SHADER_DEBUG
                g_printerr("New GLSL Program %d \n", id_);
#endif
            }

            // done (no more need for shaders)
            glUseProgram(0);
            glDeleteShader(vertex_id_);
            glDeleteShader(fragment_id_);
        }
    }

    // do not compile indefinitely
    need_compile_ = false;

    // inform of success
    return success;
}

void ShadingProgram::use()
{
    if (currentProgram_ == nullptr || currentProgram_ != this)
    {
        // first time use ; compile
        if (need_compile_)
            compile();
        // use program
        glUseProgram(id_);  // NB: if not linked, use 0 as default
        // remember (avoid switching program)
        currentProgram_ = this;
    }
}

void ShadingProgram::enduse()
{
    glUseProgram(0);
    currentProgram_ = nullptr ;
}

void ShadingProgram::reset()
{
    if (id_ != 0) {
#ifdef SHADER_DEBUG
        g_printerr("Delete GLSL Program %d \n", id_);
#endif
        glDeleteProgram(id_);
        id_ = 0;
    }
    ShadingProgram::enduse();
}

template<>
void ShadingProgram::setUniform<int>(const std::string& name, int val) {
	glUniform1i(glGetUniformLocation(id_, name.c_str()), val);
}

template<>
void ShadingProgram::setUniform<bool>(const std::string& name, bool val) {
	glUniform1i(glGetUniformLocation(id_, name.c_str()), val);
}

template<>
void ShadingProgram::setUniform<float>(const std::string& name, float val) {
	glUniform1f(glGetUniformLocation(id_, name.c_str()), val);
}

template<>
void ShadingProgram::setUniform<float>(const std::string& name, float val1, float val2) {
    glUniform2f(glGetUniformLocation(id_, name.c_str()), val1, val2);
}

template<>
void ShadingProgram::setUniform<float>(const std::string& name, float val1, float val2, float val3) {
    glUniform3f(glGetUniformLocation(id_, name.c_str()), val1, val2, val3);
}

template<>
void ShadingProgram::setUniform<glm::vec2>(const std::string& name, glm::vec2 val) {
    glm::vec2 v(val);
    glUniform2fv(glGetUniformLocation(id_, name.c_str()), 1, glm::value_ptr(v));
}

template<>
void ShadingProgram::setUniform<glm::vec3>(const std::string& name, glm::vec3 val) {
    glm::vec3 v(val);
    glUniform3fv(glGetUniformLocation(id_, name.c_str()), 1, glm::value_ptr(v));
}

template<>
void ShadingProgram::setUniform<glm::vec4>(const std::string& name, glm::vec4 val) {
    glm::vec4 v(val);
    glUniform4fv(glGetUniformLocation(id_, name.c_str()), 1, glm::value_ptr(v));
}

template<>
void ShadingProgram::setUniform<glm::mat4>(const std::string& name, glm::mat4 val) {
    glm::mat4 m(val);
	glUniformMatrix4fv(glGetUniformLocation(id_, name.c_str()), 1, GL_FALSE, glm::value_ptr(m));
}


// template<>
// void ShadingProgram::setUniform<float*>(const std::string& name, float* val) {
// 	glUniformMatrix4fv(glGetUniformLocation(id_, name.c_str()), 1, GL_FALSE, val);
// }


bool Shader::force_blending_opacity = false;

Shader::Shader() : blending(BLEND_OPACITY)
{
    // create unique id
    id_ = BaseToolkit::uniqueId();

    program_ = &simpleShadingProgram;
    Shader::reset();
}


void Shader::copy(Shader const& S)
{
    color = S.color;
    blending = S.blending;
    iTransform = S.iTransform;
}

void Shader::accept(Visitor& v) {
    v.visit(*this);
}

void Shader::use()
{
    // Use program
    program_->use();

    // set uniforms
    program_->setUniform("projection", projection);
    program_->setUniform("modelview", modelview);
    program_->setUniform("iTransform", iTransform);
    program_->setUniform("color", color);

    glm::vec3 iResolution = glm::vec3( Rendering::manager().currentAttrib().viewport, 0.f);
    program_->setUniform("iResolution", iResolution);

    // Blending Function
    if (force_blending_opacity) {
        glEnable(GL_BLEND);
        glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
        glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }
    else if ( blending < BLEND_NONE ) {
        glEnable(GL_BLEND);
        glBlendEquationSeparate(blending_equation[blending], GL_FUNC_ADD);
        glBlendFuncSeparate(blending_source_function[blending], blending_destination_function[blending], GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }
    else
        glDisable(GL_BLEND);
}


void Shader::reset()
{
    projection = glm::identity<glm::mat4>();
    modelview  = glm::identity<glm::mat4>();
    iTransform = glm::identity<glm::mat4>();
    color = glm::vec4(1.f, 1.f, 1.f, 1.f);
    blending = BLEND_OPACITY;
}


