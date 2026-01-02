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
#include <glad/glad.h>  // Initialized with gladLoadGLLoader()

// Include glfw3.h after our OpenGL definitions
#include <GLFW/glfw3.h>

// standalone image loader
#include <stb_image.h>

// vimix
#include "defines.h"
#include "Resource.h"
#include "Settings.h"
#include "Mixer.h"
#include "UserInterfaceManager.h"
#include "ControlManager.h"
#include "RenderingManager.h"

#include "MainWindow.h"


MainWindow::MainWindow() : window_(NULL), dpi_scale_(1.f), request_change_fullscreen_(false)
{

}

MainWindow::~MainWindow()
{
    if (window_ != NULL)
        terminate();
}

void MainWindow::setIcon(const std::string &resource)
{
#ifndef APPLE
    if (!Rendering::manager().wayland_) {
        size_t fpsize = 0;
        const char *fp = Resource::getData(resource, &fpsize);
        if (fp != nullptr && window_) {
            GLFWimage icon;
            icon.pixels = stbi_load_from_memory( (const stbi_uc*)fp, fpsize, &icon.width, &icon.height, nullptr, 4 );
            glfwSetWindowIcon( window_, 1, &icon );
            free( icon.pixels );
        }
    }
#endif
}

GLFWmonitor *MainWindow::monitor() const
{
    // get monitor at the center of the window
    int x = 0, y = 0, w = 2, h = 2;
    if (window_) {
        glfwGetWindowSize(window_, &w, &h);

        if (!Rendering::manager().wayland_)
           glfwGetWindowPos(window_, &x, &y);
    }

    return Rendering::manager().monitorAt(x + w/2, y + h/2);
}

// TODO : only for main window
void MainWindow::setFullscreen_(GLFWmonitor *mo)
{
    if (!window_)
        return;

    // disable fullscreen 
    if (mo == nullptr) {
        // store fullscreen windowed mode
        Settings::application.mainwindow.fullscreen = false;

        // set to window mode
        // NB: this allows restoring window position and size
        glfwSetWindowMonitor(window_, NULL, 
            Settings::application.mainwindow.x, 
            Settings::application.mainwindow.y,
            Settings::application.mainwindow.w,
            Settings::application.mainwindow.h, 
        0 );  
                                                
    }
    // set fullscreen 
    else {
        // store fullscreen mode
        Settings::application.mainwindow.fullscreen = true;
        Settings::application.mainwindow.monitor = glfwGetMonitorName(mo);

        // set to fullscreen 
        const GLFWvidmode * mode = glfwGetVideoMode(mo);
        glfwSetWindowMonitor( window_, mo, 0, 0, mode->width, mode->height, mode->refreshRate);
    }

}

// TODO only Main window
bool MainWindow::isFullscreen ()
{
   return (glfwGetWindowMonitor(window_) != nullptr);
}

void MainWindow::exitFullscreen ()
{
    // exit fullscreen
    request_change_fullscreen_ = isFullscreen();
}

void MainWindow::toggleFullscreen ()
{
    request_change_fullscreen_ = true;
}

void MainWindow::setFullscreen (std::string monitorname)
{
    Settings::application.mainwindow.monitor = monitorname;
    request_change_fullscreen_ = true;
}

void MainWindow::changeFullscreen_()
{
    // change upon request
    if (request_change_fullscreen_) {

        // done request
        request_change_fullscreen_ = false;

        // if in fullscreen mode
        if (isFullscreen ()) {
            // exit fullscreen
            setFullscreen_(nullptr);
        }
        // not in fullscreen mode
        else {
            // enter fullscreen
            setFullscreen_(monitor());
        }
    }
}

void MainWindow::changeTitle_()
{
    // change main window title if requested
    if (!main_new_title_.empty()) {
        setTitle_(main_new_title_);
        main_new_title_.clear();
    }
}

void MainWindow::setTitle_(const std::string &title)
{
    std::string fulltitle;
    if ( title.empty() )
        fulltitle = Settings::application.mainwindow.name;
    else
        fulltitle = title + std::string(" - " APP_NAME);

    if (window_ != NULL)
        glfwSetWindowTitle(window_, fulltitle.c_str());
}

int MainWindow::width()
{
    return window_attributes_.viewport.x;
}

int MainWindow::height()
{
     return window_attributes_.viewport.y;
}

int MainWindow::pixelsforRealHeight(float milimeters)
{
    GLFWmonitor *mo = monitor();

    int mm_w = 0;
    int mm_h = 0;
    glfwGetMonitorPhysicalSize(mo, &mm_w, &mm_h);

    float pixels = milimeters;
    if (mm_h > 0)
        pixels *= static_cast<float>(glfwGetVideoMode(mo)->height) / static_cast<float>(mm_h);
    else
        pixels *= 8; // something reasonnable if monitor's physical size is unknown

    return static_cast<int>( round(pixels) );
}

float MainWindow::aspectRatio()
{
    return static_cast<float>(window_attributes_.viewport.x) / static_cast<float>(window_attributes_.viewport.y);
}

bool MainWindow::init()
{
    // only once
    if (window_)
        return false;

    glfwMakeContextCurrent(NULL);

    ///
    /// Settings
    ///
    Settings::WindowConfig winset = Settings::application.mainwindow;

    ///
    /// GLFW window creation parameters
    ///
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only

#if __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#endif

    // multisampling in main window
    // glfwWindowHint(GLFW_SAMPLES, master_ == NULL ? Settings::application.render.multisampling : 0);
    glfwWindowHint(GLFW_SAMPLES, Settings::application.render.multisampling);

    // do not show at creation
    glfwWindowHint(GLFW_FOCUSED, GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_FALSE);

    // create the window
    window_ = glfwCreateWindow(winset.w, winset.h, winset.name.c_str(), NULL, nullptr);
    // window_ = glfwCreateWindow(winset.w, winset.h, winset.name.c_str(), NULL, master_);
    if (window_ == NULL){
        g_printerr("Failed to create GLFW Main Window (%s, %d x %d)\n",
                    winset.name.c_str(), winset.w, winset.h);
        return false;
    }

    // ensure minimal window size
    glfwSetWindowSizeLimits(window_, 500, 500, GLFW_DONT_CARE, GLFW_DONT_CARE);
    previous_size = glm::vec2(winset.w, winset.h);

    // set icon
    setIcon("images/vimix_256x256.png");

    ///
    /// window attributes
    ///
    // get rendering area
    glfwGetFramebufferSize(window_, &(window_attributes_.viewport.x), &(window_attributes_.viewport.y));
    // DPI scaling (retina)
    dpi_scale_ = float(window_attributes_.viewport.y) / float(winset.h);
    
    // clear to grey
    window_attributes_.clear_color = glm::vec4(COLOR_BGROUND, 1.f);

    ///
    /// CALLBACKS
    ///

    // keyboard callback
    glfwSetKeyCallback( window_, Control::keyboardCalback);
    
    // window move and close callbacks
    glfwSetWindowCloseCallback( window_, WindowCloseCallback );
    glfwSetWindowSizeCallback( window_, WindowResizeCallback );
    glfwSetWindowPosCallback( window_, WindowMoveCallback );

    // file drop callbacks
    glfwSetDropCallback( window_, MainWindow::FileDropped);

    return true;
}

void MainWindow::terminate()
{
    if (window_ != NULL) {
        glfwDestroyWindow(window_);
    }

    // invalidate
    window_  = NULL;
}

void MainWindow::show()
{
    if (!window_)
        return;

    glfwShowWindow(window_);

    if ( Settings::application.mainwindow.fullscreen ) {
        GLFWmonitor *mo = Rendering::manager().monitorNamed(Settings::application.mainwindow.monitor);
        setFullscreen_(mo);
    }
}

void MainWindow::makeCurrent()
{
    if (!window_) {
        g_printerr("RenderingWindow::makeCurrent() not initialized yet.");
        return;
    }

    // handle window resize
    glfwGetFramebufferSize(window_, &(window_attributes_.viewport.x), &(window_attributes_.viewport.y));

    // ensure main context is current
    glfwMakeContextCurrent(window_);

    // set and clear
    glViewport(0, 0, window_attributes_.viewport.x, window_attributes_.viewport.y);
    glClearColor(window_attributes_.clear_color.r, window_attributes_.clear_color.g,
                 window_attributes_.clear_color.b, window_attributes_.clear_color.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void MainWindow::WindowCloseCallback( GLFWwindow* w )
{
    // close main window
    if (!UserInterface::manager().TryClose())
        glfwSetWindowShouldClose(w, GLFW_FALSE);

}

void MainWindow::WindowResizeCallback( GLFWwindow *w, int width, int height)
{
    Settings::application.mainwindow.fullscreen = glfwGetWindowMonitor(w) != nullptr;
    if (!Settings::application.mainwindow.fullscreen) {
        Settings::application.mainwindow.w = width;
        Settings::application.mainwindow.h = height;

        // hack to refresh decorations after return from fullscreen
        glfwSetWindowAttrib( w, GLFW_DECORATED, GLFW_TRUE );
    }
    else {
        glfwSetWindowAttrib( w, GLFW_DECORATED, GLFW_FALSE );
    }


    // UI manager tries to keep windows in the workspace
    WorkspaceWindow::notifyWorkspaceSizeChanged(Rendering::manager().mainWindow().previous_size.x, Rendering::manager().mainWindow().previous_size.y, width, height);
    Rendering::manager().mainWindow().previous_size = glm::vec2(width, height);
    // Rendering::manager().draw();

}

void MainWindow::WindowMoveCallback( GLFWwindow *w, int x, int y)
{
    if (!Settings::application.mainwindow.fullscreen) {
        Settings::application.mainwindow.x = x;
        Settings::application.mainwindow.y = y;
    }
}

void MainWindow::FileDropped(GLFWwindow *, int path_count, const char* paths[])
{
    int i = 0;
    for (; i < path_count; ++i) {
        std::string filename(paths[i]);
        if (filename.empty())
            break;
        // try to create a source
        Mixer::manager().addSource ( Mixer::manager().createSourceFile( filename ) );
    }
    if (i>0) {
        // show mixer pannel if at least one file added
        UserInterface::manager().showPannel(  Mixer::manager().numSource() );
        // bring window front
        Rendering::manager().mainWindow().show();
    }
}
