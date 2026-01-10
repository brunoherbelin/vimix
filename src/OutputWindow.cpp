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
#include <glib.h>
#include <stdlib.h>

// Desktop OpenGL function loader
#include <glad/glad.h>

// Include glfw3.h after our OpenGL definitions
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

// vimix
#include "FrameBuffer.h"
#include "Settings.h"
#include "Filter/ImageFilter.h"
#include "ControlManager.h"
#include "RenderingManager.h"
#include "ImageShader.h"
#include "Scene/Primitives.h"
#include "Toolkit/GstToolkit.h"
#include "Stream.h"

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


OutputWindow::OutputWindow() : window_(nullptr), active_(false), initialized_(false), 
surface_(nullptr), shader_(nullptr), show_pattern_(false), pattern_(nullptr) 
{

}

OutputWindow::~OutputWindow()
{
    if (initialized_)
        terminate();
}

uint OutputWindow::num_active_outputs = 0;

bool OutputWindow::init(GLFWmonitor *monitor, GLFWwindow *share)
{
    if (initialized_ || !monitor)
        return false;

    monitor_name_ = glfwGetMonitorName(monitor);

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
    const GLFWvidmode *mode = glfwGetVideoMode(monitor);

    // create fullscreen window on this monitor
    window_ = glfwCreateWindow(mode->width, mode->height, "vimix Output", monitor, share);

    if (window_ == NULL) {
        g_printerr("Failed to create GLFW Output Window on monitor %s\n", glfwGetMonitorName(monitor));
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

    // vsync on output windows if this is the first active output window
    glfwSwapInterval(OutputWindow::num_active_outputs > 1 ? 0 : Settings::application.render.vsync);

    // hide cursor
    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

    // show window
    glfwShowWindow(window_);

    // create pattern source
    pattern_ = new Stream;
    if (GstToolkit::has_feature("frei0r-src-test-pat-b") )
        pattern_->open("frei0r-src-test-pat-b type=0.7", mode->width, mode->height);
    else 
        pattern_->open("videotestsrc pattern=smpte", mode->width, mode->height);

    // done !
    initialized_ = true;

    // save settings
    Settings::application.monitors[monitor_name_].active_output = active_;

    return true;
}

void OutputWindow::setShowPattern(bool on) 
{ 
    show_pattern_ = on; 
    pattern_->play(show_pattern_);
}

void OutputWindow::terminate()
{
    // save settings
    Settings::application.monitors[monitor_name_].active_output = active_;

    if (window_ != NULL) {
        glfwDestroyWindow(window_);
    }

    if (surface_) {
        delete surface_;
        surface_ = nullptr;
        shader_ = nullptr; // shader is deleted by surface
    }

    // invalidate
    window_ = nullptr;
    monitor_name_.clear();
    initialized_ = false;
    active_ = false;

    // end pattern
    show_pattern_ = false;
    pattern_->close();
    delete pattern_;
    pattern_ = nullptr;

    // detect end
    if ( OutputWindow::num_active_outputs == 0 ) {
        if (glfwGetWindowAttrib(Rendering::manager().mainWindow().window(), GLFW_VISIBLE) == GL_FALSE) {    
            // all monitors disconnected and main window hidden : quit application
            Rendering::manager().close();
        }   
    }
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

        // create surface if needed 
        // (NB: special OutputWindowSurface because VAO are not shared between contexts)
        if (surface_ == nullptr) {
            shader_ = new ImageFilteringShader;
            shader_->setCode( whitebalance.code().first );
            surface_ = new OutputWindowSurface(shader_);
            // scale surface to framebuffer size, vertical flip
            surface_->scale_ = glm::vec3(fb->resolution().x / 2.f, -fb->resolution().y / 2.f, 1.f);
            // position surface at center of window
            surface_->translation_ = glm::vec3(surface_->scale_.x , -surface_->scale_.y, 0.f);
        }
        if (shader_) {
            // set uniforms from settings
            Settings::MonitorConfig conf = Settings::application.monitors[monitor_name_];
            shader_->uniforms_["Red"] = conf.whitebalance.r;
            shader_->uniforms_["Green"] = conf.whitebalance.g;
            shader_->uniforms_["Blue"] = conf.whitebalance.b;
            shader_->uniforms_["Temperature"] = conf.whitebalance.a;
            shader_->uniforms_["Contrast"] = conf.contrast;
            shader_->uniforms_["Brightness"] = conf.brightness;
            if (conf.custom_geometry )
                shader_->iNodes = shader_->iNodes = conf.nodes;
            else
                shader_->iNodes = glm::zero<glm::mat4>();
        }

        // render the framebuffer texture to the output window
        int x = 0, y = 0, w = fb->resolution().x, h = fb->resolution().y;

        // if test pattern requested, set its texture
        if (show_pattern_) {
            pattern_->update();
            surface_->setTextureIndex(pattern_->texture());
        }
        // otherwise render the framebuffer
        else {
            surface_->setTextureIndex(fb->texture());
            // project only the part of framebuffer visible in the window
            glfwGetWindowPos(window_, &x, &y);
            glfwGetWindowSize(window_, &w, &h);
        }

        // draw texture on surface
        surface_->update(0.f);

        // draw surface on output window
        // using the orthographic projection adapted to window position and size (vertical flip)
        glm::mat4 projection = glm::ortho(float(x), float(x+w),
                                         float(y+h), float(y),
                                         -1.f, 1.f);
        surface_->draw(glm::identity<glm::mat4>(), projection);

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
        double t = glfwGetTime();
        static double seconds = 0.f;
        // detect double click
        if ( t - seconds < 0.25f ) {
            // bring main window back to focus
            glfwFocusWindow(Rendering::manager().mainWindow().window());
        }
        // for next double click detection
        seconds = t;
    }
}
