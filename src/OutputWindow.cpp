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
#include <glad/glad.h>  // Initialized with gladLoadGLLoader()

// Include glfw3.h after our OpenGL definitions
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

// Include GStreamer
#include <gst/gl/gl.h>
#include <gst/gl/gstglcontext.h>

#ifdef USE_GST_OPENGL_SYNC_HANDLER
#include <GLFW/glfw3native.h>
#endif

// standalone image loader
#include <stb_image.h>

// vimix
#include "defines.h"
#include "Log.h"
#include "Resource.h"
#include "Settings.h"
#include "Mixer.h"
#include "UserInterfaceManager.h"
#include "ControlManager.h"
#include "RenderingManager.h"

#include "OutputWindow.h"


OutputWindow::OutputWindow()
{

}

OutputWindow::~OutputWindow()
{

}






// static void OutputWindowEvent( GLFWwindow *w, int button, int action, int)
// {
//     // detect mouse press
//     if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
//     {
//         static double seconds = 0.f;
//         // detect double clic
//         if ( glfwGetTime() - seconds < 0.2f ) {

//             // show main window
//             Rendering::manager().mainWindow().show();
//         }
//         // for next double clic detection
//         seconds = glfwGetTime();
//     }
// }

// static void WindowCloseCallback( GLFWwindow* w )
// {
//     // // not main window
//     // else {
//     //     // if headless (main window not visile)
//     //     if (glfwGetWindowAttrib(Rendering::manager().mainWindow().window(), GLFW_VISIBLE)
//     //         == GL_FALSE) {
//     //         // close rendering manager = quit
//     //         Rendering::manager().close();
//     //     }
//     //     // attempt to close shows display view
//     //     else {
//     //         Mixer::manager().setView(View::DISPLAYS);
//     //         Rendering::manager().mainWindow().show();
//     //     }
//     // }
// }
    

// std::vector<RenderingOutput> RenderingOutput::vector_;

// void RenderingOutput::Connect(GLFWmonitor* monitor, int event)
// {
//     // list monitors with GLFW
//     int count_monitors = 0;
//     GLFWmonitor** monitors = glfwGetMonitors(&count_monitors);

//     // Disconnection of a monitor messes up with output windows
//     if (event == GLFW_DISCONNECTED) {
//         for (int i = 0; i < vector_.size();  i++) {
//             if (vector_[i].monitor == monitor) {
//                 // if a window is on this monitor
//                 if (vector_[i].window.initialized()) {
//                     // close window
//                     vector_[i].window.terminate();
//                 }
//                 vector_.erase( vector_.begin() + i );
//                 return;
//             }
//         }
//     }
 
//     // Fill list of monitors
//     for (int i = 0; i < count_monitors;  i++) {

//         // if monitor already known, skip it
//         bool known = false;
//         for (int j = 0; j < vector_.size(); j++) {
//             if (vector_[j].monitor == monitors[i]) {
//                 known = true;
//                 break;
//             }
//         }
//         if (known)
//             continue;

//         // add new monitor 
//         vector_.resize(vector_.size() + 1);

//         // fill monitor structure
//         int x = 0, y = 0;
//         glfwGetMonitorPos(monitors[i], &x, &y);
//         const GLFWvidmode *vm = glfwGetVideoMode(monitors[i]);
//         std::string n = glfwGetMonitorName(monitors[i]);
        
//         vector_[i].monitor = monitors[i];
//         vector_[i].name = n;
//         vector_[i].geometry = glm::ivec4(x, y, vm->width, vm->height);  

//     }

//     // inform Displays View that monitors changed
//     Mixer::manager().view(View::DISPLAYS)->recenter();

// #ifndef NDEBUG
//     if (event == GLFW_CONNECTED)
//         g_printerr("Monitor %s connected\n",  glfwGetMonitorName(monitor));
//     else if (event == GLFW_DISCONNECTED)
//         g_printerr("Monitor %s disconnected\n",  glfwGetMonitorName(monitor));
// #endif
// }

// RenderingOutput RenderingOutput::atCoordinates(int x, int y)
// {
//     // try every monitor
//     for (int i = 0; i < vector_.size();  i++) {        
//         if ( x >= vector_[i].geometry.x && x <= vector_[i].geometry.x + vector_[i].geometry.z &&
//                 y >= vector_[i].geometry.y && y <= vector_[i].geometry.y + vector_[i].geometry.w) {
//             // found the monitor containing this point !
//             return vector_[i];
//         }
//     }

//     return RenderingOutput();
// }

// void RenderingOutput::drawAll()
// {
    // for (size_t index = 0; index < vector_.size(); ++index) {
    //     // initialize output windows to match number of output windows

    //     if (vector_[index].window.initialized()) {
            
    //         vector_[index].window.draw( Mixer::manager().session()->frame() );

    //         if (!vector_[index].enabled)
    //             vector_[index].window.terminate();
    //     }
    //     else {
    //         // enabling requested
    //         if (vector_[index].enabled) {

    //             vector_[index].window.init( index+1, Rendering::manager().mainWindow().window());

    //             // fullscreen 
    //             const GLFWvidmode * mode = glfwGetVideoMode( vector_[index].monitor);
    //             glfwSetWindowMonitor( vector_[index].window.window(), vector_[index].monitor, 
    //                 0, 0, mode->width, mode->height, mode->refreshRate);

    //             glfwSetInputMode( vector_[index].window.window(), GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
                
    //             vector_[index].window.show();
    //         }
    //     }
    // }
// }

// // custom surface with a new VAO
// class WindowSurface : public MeshSurface
// {
// public:
//     WindowSurface(Shader *s = new ImageShader)
//         : MeshSurface(s)
//     {}
//     void init () override
//     {
//         generate_mesh(32, 32);
//         Primitive::init();
//     }
// };

// // pattern source can be shared because all windows render the same framebuffer resolution
// class Stream *MainWindow::pattern_ = new Stream;

// MainWindow::MainWindow() : window_(NULL), master_(NULL),
//     index_(-1), dpi_scale_(1.f), textureid_(0), fbo_(0), surface_(nullptr), request_change_fullscreen_(false)
// {

// }

// MainWindow::~MainWindow()
// {
//     if (window_ != NULL)
//         terminate();
//     // delete pattern_;
//     MainWindow::pattern_->close();
// }

// void MainWindow::setTitle(const std::string &title)
// {
//     std::string fulltitle;
//     if ( title.empty() )
//         fulltitle = Settings::application.mainwindow.name;
//     else
//         fulltitle = title + std::string(" - " APP_NAME);

//     if (window_ != NULL)
//         glfwSetWindowTitle(window_, fulltitle.c_str());
// }

// void MainWindow::setIcon(const std::string &resource)
// {
// #ifndef APPLE
//     if (!Rendering::manager().wayland_) {
//         size_t fpsize = 0;
//         const char *fp = Resource::getData(resource, &fpsize);
//         if (fp != nullptr && window_) {
//             GLFWimage icon;
//             icon.pixels = stbi_load_from_memory( (const stbi_uc*)fp, fpsize, &icon.width, &icon.height, nullptr, 4 );
//             glfwSetWindowIcon( window_, 1, &icon );
//             free( icon.pixels );
//         }
//     }
// #endif
// }

// GLFWmonitor *MainWindow::monitor()
// {
//     // get monitor at the center of the window
//     int x = 0, y = 0, w = 2, h = 2;
//     if (window_) {
//         glfwGetWindowSize(window_, &w, &h);

//         if (!Rendering::manager().wayland_)
//            glfwGetWindowPos(window_, &x, &y);
//     }
    
//     RenderingOutput mo = RenderingOutput::atCoordinates(x + w/2, y + h/2);
//     return mo.monitor;
// }

// // TODO : only for main window
// void MainWindow::setFullscreen_(GLFWmonitor *mo)
// {
//     if (!window_)
//         return;

//     // disable fullscreen mode
//     if (mo == nullptr) {
//         // store fullscreen mode
//         Settings::application.mainwindow.fullscreen = false;

//         // set to window mode
//         glfwSetInputMode( window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
//         glfwSetWindowMonitor( window_, NULL, 0, 0,
//                                                 Settings::application.mainwindow.w,
//                                                 Settings::application.mainwindow.h, 0 );
//     }
//     // set fullscreen mode
//     else {
//         // store fullscreen mode
//         Settings::application.mainwindow.fullscreen = true;
//         Settings::application.mainwindow.monitor = glfwGetMonitorName(mo);

//         // set to fullscreen mode
//         const GLFWvidmode * mode = glfwGetVideoMode(mo);
//         glfwSetWindowMonitor( window_, mo, 0, 0, mode->width, mode->height, mode->refreshRate);

//         glfwSetInputMode( window_, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
//     }

//     // Enable vsync on main window only (i.e. 0 if has a master)
//     // Workaround for disabled vsync in fullscreen (https://github.com/glfw/glfw/issues/1072)
//     glfwSwapInterval( master_ != nullptr ? 0 : Settings::application.render.vsync);

// }

// // TODO only Main window
// bool MainWindow::isFullscreen ()
// {
//    return (glfwGetWindowMonitor(window_) != nullptr);
// }

// // TODO only Main window
// void MainWindow::exitFullscreen ()
// {
//     // exit fullscreen
//     request_change_fullscreen_ = isFullscreen();
// }

// // TODO only Main window
// void MainWindow::toggleFullscreen ()
// {
//     request_change_fullscreen_ = true;
// }

// // TODO only Main window
// void MainWindow::setFullscreen (std::string monitorname)
// {
//     Settings::application.mainwindow.monitor = monitorname;
//     request_change_fullscreen_ = true;
// }

// void MainWindow::changeFullscreen_()
// {
//     // change upon request
//     if (request_change_fullscreen_) {

//         // done request
//         request_change_fullscreen_ = false;

//         GLFWmonitor *mo = monitor();
//         // if (index_ > 0)
//         //     mo = Rendering::manager().monitorNamed( Settings::application.windows[index_].monitor );

//         // if in fullscreen mode
//         if (isFullscreen ()) {

//             // changing fullscreen monitor
//             if ( glfwGetWindowMonitor(window_) != mo)
//                 setFullscreen_(mo);
//             else
//                 // exit fullscreen
//                 setFullscreen_(nullptr);
//         }
//         // not in fullscreen mode
//         else {
//             // enter fullscreen
//             setFullscreen_(mo);
//         }
//     }
// }


// int MainWindow::width()
// {
//     return window_attributes_.viewport.x;
// }

// int MainWindow::height()
// {
//      return window_attributes_.viewport.y;
// }

// int MainWindow::pixelsforRealHeight(float milimeters)
// {
//     GLFWmonitor *mo = monitor();

//     int mm_w = 0;
//     int mm_h = 0;
//     glfwGetMonitorPhysicalSize(mo, &mm_w, &mm_h);

//     float pixels = milimeters;
//     if (mm_h > 0)
//         pixels *= static_cast<float>(glfwGetVideoMode(mo)->height) / static_cast<float>(mm_h);
//     else
//         pixels *= 8; // something reasonnable if monitor's physical size is unknown

//     return static_cast<int>( round(pixels) );
// }

// float MainWindow::aspectRatio()
// {
//     return static_cast<float>(window_attributes_.viewport.x) / static_cast<float>(window_attributes_.viewport.y);
// }

// bool MainWindow::init(int index, GLFWwindow *share)
// {
//     if (window_)
//         return false;

//     glfwMakeContextCurrent(NULL);

//     ///
//     /// Settings
//     ///
//     index_ = index;
//     master_ = share;
//     Settings::WindowConfig winset = Settings::application.mainwindow;

//     ///
//     /// GLFW window creation parameters
//     ///
//     glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
//     glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
//     glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only

// #if __APPLE__
//     glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
// #endif

//     // multisampling in main window
//     glfwWindowHint(GLFW_SAMPLES, master_ == NULL ? Settings::application.render.multisampling : 0);

//     // do not show at creation
//     glfwWindowHint(GLFW_FOCUSED, GLFW_FALSE);
//     glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
//     glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);

//     // create the window
//     window_ = glfwCreateWindow(winset.w, winset.h, winset.name.c_str(), NULL, master_);
//     if (window_ == NULL){
//         g_printerr("Failed to create GLFW Window %d (%s, %d x %d)\n",
//                    index_, winset.name.c_str(), winset.w, winset.h);
//         return false;
//     }

//     // ensure minimal window size
//     glfwSetWindowSizeLimits(window_, 500, 500, GLFW_DONT_CARE, GLFW_DONT_CARE);
//     previous_size = glm::vec2(winset.w, winset.h);

//     // set icon
//     setIcon("images/vimix_256x256.png");

//     ///
//     /// CALLBACKS
//     ///
//     // store global ref to pointers (used by callbacks)
//     Rendering::manager().windows_[window_] = this;

//     //
//     // set keyboard callback
//     //
//     // all windows capture keys
//     glfwSetKeyCallback( window_, Control::keyboardCalback);
//     glfwSetWindowCloseCallback( window_, WindowCloseCallback );

//     //
//     // set specific callbacks
//     //
//     if (master_ != NULL) {
//         // additional window callbacks for user input in output windows
//         glfwSetMouseButtonCallback( window_, OutputWindowEvent);
//     }
//     else {
//         // additional window callbacks for main window
//         glfwSetDropCallback( window_, MainWindow::FileDropped);
//         glfwSetWindowSizeCallback( window_, WindowResizeCallback );
//     }

//     //
//     // Initialize OpenGL
//     //
//     // take opengl context ownership
//     glfwMakeContextCurrent(window_);

//     // Initialize OpenGL loader on first call
//     static bool glad_initialized = false;
//     if ( !glad_initialized ) {
//         bool err = gladLoadGL((GLADloadfunc) glfwGetProcAddress) == 0;
//         if (err) {
//             g_printerr("Failed to initialize GLAD OpenGL loader.");
//             return false;
//         }
//         glad_initialized = true;
//     }

//     // get rendering area
//     glfwGetFramebufferSize(window_, &(window_attributes_.viewport.x), &(window_attributes_.viewport.y));
//     // DPI scaling (retina)
//     dpi_scale_ = float(window_attributes_.viewport.y) / float(winset.h);

//     // We decide for byte aligned textures all over
//     glPixelStorei(GL_UNPACK_ALIGNMENT,1);

//     // acurate derivative for shader
//     glHint(GL_FRAGMENT_SHADER_DERIVATIVE_HINT, GL_NICEST);

//     // nice line smoothing
//     glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

//     // Enable Antialiasing multisampling for main window
//     if ( openGLExtensionAvailable( "GL_ARB_multisample" ) ||
//          openGLExtensionAvailable( "GL_ARB_texture_multisample" )||
//          openGLExtensionAvailable( "GL_EXT_multisample" )||
//          openGLExtensionAvailable( "GL_EXT_framebuffer_multisample" )) {

//         if (Settings::application.render.multisampling > 0 && master_ == NULL) {
//             glEnable(GL_MULTISAMPLE);
//             if ( openGLExtensionAvailable( "GL_NV_multisample_filter_hint") )
//                 glHint(GL_MULTISAMPLE_FILTER_HINT_NV, GL_NICEST);
//         }
//         else
//             glDisable(GL_MULTISAMPLE);
//     }
//     else
//         Settings::application.render.multisampling = 0;

//     // if not main window
//     if ( master_ != NULL ) {
//         // NO vsync on output windows
//         glfwSwapInterval(0);

//         // clear to black
//         window_attributes_.clear_color = glm::vec4(0.f, 0.f, 0.f, 1.f);

//         glfwShowWindow(window_);
//     }
//     else {
//         //  vsync on main window
//         glfwSwapInterval(Settings::application.render.vsync);

//         // clear to grey
//         window_attributes_.clear_color = glm::vec4(COLOR_BGROUND, 1.f);
//     }

//     //
//     // default render black
//     //
//     (void) Resource::getTextureWhite(); // init white texture too
//     textureid_ = Resource::getTextureBlack();

//     //
//     // capture possible init errors
//     //
//     unsigned int err = 0;
//     while((err = glGetError()) != GL_NO_ERROR){
//         Log::Warning("Error %d during OpenGL init.", err);
//     }

//     return true;
// }

// void MainWindow::terminate()
// {
//     // cleanup
//     if (surface_ != nullptr)
//         delete surface_;
//     if (fbo_ != 0)
//         glDeleteFramebuffers(1, &fbo_);
//     if (window_ != NULL) {
//         // remove global ref to pointers
//         Rendering::manager().windows_.erase(window_);
//         // delete window
//         glfwDestroyWindow(window_);
//     }

//     // invalidate
//     window_  = NULL;
//     shader_  = nullptr;
//     surface_ = nullptr;
//     fbo_     = 0;
//     index_   = -1;
//     textureid_ = Resource::getTextureBlack();

//     // SCREENSAVER UNINHIBIT
//     // inhibitScreensaver( Settings::application.num_output_windows > 0 );
// }

// void MainWindow::show()
// {
//     if (!window_)
//         return;

//     glfwShowWindow(window_);

//     // if ( Settings::application.windows[index_].fullscreen ) {
//     //     GLFWmonitor *mo = Rendering::manager().monitorNamed(Settings::application.windows[index_].monitor);
//     //     setFullscreen_(mo);
//     // }

//     // SCREENSAVER INHIBIT
//     // inhibitScreensaver( Settings::application.num_output_windows > 0 );
// }


// void MainWindow::makeCurrent()
// {
//     if (!window_) {
//         g_printerr("RenderingWindow::makeCurrent() not initialized yet.");
//         return;
//     }

//     // handle window resize
//     glfwGetFramebufferSize(window_, &(window_attributes_.viewport.x), &(window_attributes_.viewport.y));

//     // ensure main context is current
//     glfwMakeContextCurrent(window_);

//     // set and clear
//     glViewport(0, 0, window_attributes_.viewport.x, window_attributes_.viewport.y);
//     glClearColor(window_attributes_.clear_color.r, window_attributes_.clear_color.g,
//                  window_attributes_.clear_color.b, window_attributes_.clear_color.a);
//     glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
// }


// FilteringProgram whitebalance("Whitebalance",
//                               "shaders/filters/whitebalance.glsl",
//                               "",
//                               {{"Red", 1.0},
//                                {"Green", 1.0},
//                                {"Blue", 1.0},
//                                {"Temperature", 0.5},
//                                {"Contrast", 0.0},
//                                {"Brightness", 0.0}
//                               }
//                               );

// bool MainWindow::draw(FrameBuffer *fb)
// {
//     // cannot draw if there is no window or invalid framebuffer
//     if (!window_ || !fb)
//         return false;

//     // only draw if window is not iconified
//     if( !glfwGetWindowAttrib(window_, GLFW_ICONIFIED ) ) {

//         // update viewport (could be done with callback)
//         glfwGetFramebufferSize(window_, &(window_attributes_.viewport.x), &(window_attributes_.viewport.y));

//         // take context ownership
//         glfwMakeContextCurrent(window_);

//         // setup attribs
//         Rendering::manager().pushAttrib(window_attributes_);
//         glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

//         // make sure previous shader in another glcontext is disabled
//         ShadingProgram::enduse();

//         // draw geometry
//         if (Settings::application.render.disabled)
//             // no draw; indicate texture is black
//             textureid_ = Resource::getTextureBlack();
//         else {
//             // normal draw

//             // VAO is not shared between multiple contexts of different windows
//             // so we have to create a new VAO for rendering the surface in this window
//             if (surface_ == nullptr) {
//                 // create shader that performs white balance correction
//                 shader_ = new ImageFilteringShader;
//                 shader_->setCode( whitebalance.code().first );
//                 // create surface using the shader
//                 surface_ = new WindowSurface(shader_);
//             }
// //             // update values of the shader
// //             if (shader_) {
// //                 shader_->uniforms_["Red"] = Settings::application.windows[index_].whitebalance.x;
// //                 shader_->uniforms_["Green"] = Settings::application.windows[index_].whitebalance.y;
// //                 shader_->uniforms_["Blue"] = Settings::application.windows[index_].whitebalance.z;
// //                 shader_->uniforms_["Temperature"] = Settings::application.windows[index_].whitebalance.w;
// //                 shader_->uniforms_["Contrast"] = Settings::application.windows[index_].contrast;
// //                 shader_->uniforms_["Brightness"] = Settings::application.windows[index_].brightness;

// //                 if (Settings::application.windows[index_].custom)
// //                     shader_->iNodes = Settings::application.windows[index_].nodes;
// //                 else
// //                     shader_->iNodes = glm::zero<glm::mat4>();
// //             }

// //             // Display option: scaled or corrected aspect ratio
// //             if (Settings::application.windows[index_].custom) {
// // //                surface_->scale_ = Settings::application.windows[index_].scale;
// // //                surface_->translation_ = Settings::application.windows[index_].translation;

// //                 surface_->scale_ = glm::vec3(1.f);
// //                 surface_->translation_ = glm::vec3(0.f);
// //             }
// //             else{
// //                 // calculate scaling factor of frame buffer inside window
// //                 const float windowAspectRatio = aspectRatio();
// //                 const float renderingAspectRatio = fb->aspectRatio();
// //                 if (windowAspectRatio < renderingAspectRatio)
// //                     surface_->scale_ = glm::vec3(1.f, windowAspectRatio / renderingAspectRatio, 1.f);
// //                 else
// //                     surface_->scale_ = glm::vec3(renderingAspectRatio / windowAspectRatio, 1.f, 1.f);
// //                 surface_->translation_ = glm::vec3(0.f);
// //             }

// //             // Display option: draw calibration pattern
// //             if (Settings::application.windows[index_].show_pattern) {
// //                 // (re) create pattern at frame buffer resolution
// //                 if ( pattern_->width() != fb->width() || pattern_->height() != fb->height()) {
// //                     if (GstToolkit::has_feature("frei0r-src-test-pat-b") )
// //                         pattern_->open("frei0r-src-test-pat-b type=0.7", fb->width(), fb->height());
// //                     else {
// //                         pattern_->open("videotestsrc pattern=smpte", fb->width(), fb->height());
// //                         pattern_->play(true);
// //                     }
// //                 }
// //                 // draw pattern texture
// //                 pattern_->update();
// //                 textureid_ = pattern_->texture();
// //             }
// //             else
//                 // draw normal texture
//                 textureid_ = fb->texture();

//             // actual render of the textured surface
//             static glm::mat4 projection = glm::ortho(-1.f, 1.f, -1.f, 1.f, -1.f, 1.f);
//             surface_->setTextureIndex(textureid_);
//             surface_->update(0.f);
//             surface_->draw(glm::identity<glm::mat4>(), projection);

//             // done drawing (unload shader from this glcontext)
//             ShadingProgram::enduse();
//         }

//         // restore attribs
//         Rendering::manager().popAttrib();

//         glfwSwapBuffers(window_);

//     }

//     return true;
// }

