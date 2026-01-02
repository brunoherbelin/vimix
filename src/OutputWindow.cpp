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

#include <cstring>
#include <stdlib.h>

// Desktop OpenGL function loader
#include <glad/glad.h>

// Include glfw3.h after our OpenGL definitions
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

// vimix
#include "defines.h"
#include "Log.h"
#include "FrameBuffer.h"
#include "Settings.h"
#include "Mixer.h"
#include "UserInterfaceManager.h"
#include "ControlManager.h"
#include "RenderingManager.h"
#include "ImageShader.h"
#include "Scene/Primitives.h"

#include "OutputWindow.h"


// custom surface with a new VAO for output window
class OutputWindowSurface : public MeshSurface
{
public:
    OutputWindowSurface(Shader *s = new ImageShader)
        : MeshSurface(s)
    {}
    void init () override
    {
        generate_mesh(32, 32);
        Primitive::init();
    }
};


OutputWindow::OutputWindow() : window_(nullptr), monitor_(nullptr), active_(false), initialized_(false)
{

}

OutputWindow::~OutputWindow()
{
    if (initialized_)
        terminate();
}

bool OutputWindow::init(GLFWmonitor *monitor, GLFWwindow *share)
{
    if (initialized_ || !monitor)
        return false;

    monitor_ = monitor;

    glfwMakeContextCurrent(NULL);

    ///
    /// GLFW window creation parameters
    ///
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#if __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // no multisampling in output windows
    glfwWindowHint(GLFW_SAMPLES, 0);

    // do not show at creation
    glfwWindowHint(GLFW_FOCUSED, GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

    // get monitor video mode
    const GLFWvidmode *mode = glfwGetVideoMode(monitor_);

    // create fullscreen window on this monitor
    window_ = glfwCreateWindow(mode->width, mode->height, "vimix Output", monitor_, share);

    if (window_ == NULL) {
        g_printerr("Failed to create GLFW Output Window on monitor %s\n", glfwGetMonitorName(monitor_));
        return false;
    }

    // set rendering area
    window_attributes_.viewport.x = mode->width;
    window_attributes_.viewport.y = mode->height;
 
    // clear to black
    window_attributes_.clear_color = glm::vec4(0.f, 0.f, 0.f, 1.f);

    ///
    /// CALLBACKS
    ///
    glfwSetKeyCallback(window_, Control::keyboardCalback);
    glfwSetMouseButtonCallback(window_, OutputWindow::MouseButtonCallback);

    ///
    /// OpenGL context setup
    ///
    glfwMakeContextCurrent(window_);

    // vsync on output windows
    glfwSwapInterval(Settings::application.render.vsync);

    // hide cursor
    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

    // show window
    glfwShowWindow(window_);

    initialized_ = true;

    return true;
}

void OutputWindow::terminate()
{
    if (window_ != NULL) {
        glfwDestroyWindow(window_);
    }

    window_ = nullptr;
    monitor_ = nullptr;
    initialized_ = false;
    active_ = false;
}

FilteringProgram whitebalance("Whitebalance",
                              "shaders/filters/whitebalance.glsl",
                              "",
                              {{"Red", 1.0},
                               {"Green", 1.0},
                               {"Blue", 1.0},
                               {"Temperature", 0.5},
                               {"Contrast", 0.0},
                               {"Brightness", 0.0}
                              }
                              );

bool OutputWindow::draw(FrameBuffer *fb)
{
    // cannot draw if not initialized or no framebuffer
    if (!initialized_ || !window_ || !fb)
        return false;

    // only draw if window is not iconified
    if (glfwGetWindowAttrib(window_, GLFW_ICONIFIED))
        return false;

    // take context ownership
    glfwMakeContextCurrent(window_);

    // set viewport and clear color
    Rendering::manager().pushAttrib(window_attributes_);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // make sure previous shader in another glcontext is disabled
    ShadingProgram::enduse();

    if (!Settings::application.render.disabled) {

        // create surface if needed (VAO is not shared between contexts)
        static OutputWindowSurface *surface = nullptr;
        static ImageFilteringShader *shader_ = nullptr;
        if (surface == nullptr) {
            shader_ = new ImageFilteringShader;
            shader_->setCode( whitebalance.code().first );
            surface = new OutputWindowSurface(shader_);
        }
        if (shader_) {
            // TODO : set uniforms from settings
            shader_->uniforms_["Red"] = 1.f;
            shader_->uniforms_["Green"] = 1.f;
            shader_->uniforms_["Blue"] = 1.f;
            shader_->uniforms_["Temperature"] = 0.5f;
            shader_->uniforms_["Contrast"] = 0.f;
            shader_->uniforms_["Brightness"] = 0.f;
            shader_->iNodes = glm::zero<glm::mat4>();
        }

        // render the framebuffer texture to the output window
        static glm::mat4 projection = glm::ortho(-1.f, 1.f, -1.f, 1.f, -1.f, 1.f);
        surface->setTextureIndex(fb->texture());
        surface->update(0.f);
        surface->draw(glm::identity<glm::mat4>(), projection);

        // done drawing (unload shader from this glcontext)
        ShadingProgram::enduse();
    }

    // restore attribs
    Rendering::manager().popAttrib();

    // swap buffers
    glfwSwapBuffers(window_);

    return true;
}

void OutputWindow::MouseButtonCallback(GLFWwindow *w, int button, int action, int)
{
    // detect mouse press
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        static double seconds = 0.f;
        // detect double click
        if (glfwGetTime() - seconds < 0.2f) {
            // show main window
            Rendering::manager().mainWindow().show();
        }
        // for next double click detection
        seconds = glfwGetTime();
    }
}
