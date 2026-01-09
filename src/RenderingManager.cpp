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

#include "FrameGrabbing.h"
#include "MainWindow.h"
#include <cstring>
#include <stdlib.h>

// Desktop OpenGL function loader
#include <glad/glad.h>  // Initialized with gladLoadGLLoader()

// Include glfw3.h after our OpenGL definitions
#define GLFW_INCLUDE_GLEXT
#include <GLFW/glfw3.h>

#ifdef APPLE
#define GLFW_EXPOSE_NATIVE_COCOA
#define GLFW_EXPOSE_NATIVE_NSGL
#else
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_GLX
#endif
//#include <GLFW/glfw3native.h>

#include <glm/gtc/matrix_transform.hpp> // glm::translate, glm::rotate, glm::scale
#include <glm/ext/matrix_clip_space.hpp> // glm::perspective

// Include GStreamer
#include <gst/gl/gl.h>
#include <gst/gl/gstglcontext.h>

#ifdef GLFW_EXPOSE_NATIVE_COCOA
//#include "osx/CocoaToolkit.h"
//#include <gst/gl/cocoa/gstgldisplay_cocoa.h>
#endif
#ifdef GLFW_EXPOSE_NATIVE_GLX
#include <gst/gl/x11/gstgldisplay_x11.h>
#include <gio/gio.h>
#endif

#ifdef USE_GST_OPENGL_SYNC_HANDLER
#include <GLFW/glfw3native.h>
#endif

// vimix
#include "defines.h"
#include "Log.h"
#include "Settings.h"
#include "Mixer.h"
#include "Canvas.h"
#include "Toolkit/SystemToolkit.h"
#include "Toolkit/GstToolkit.h"
#include "UserInterfaceManager.h"
#include "Scene/Primitives.h"
#include "TabletInput.h"

#include "RenderingManager.h"


static void glfw_error_callback(int error, const char* description)
{
    g_printerr("GLFW Error %d: %s\n",  error, description);
}


void Rendering::close()
{
    glfwSetWindowShouldClose(main_.window(), GLFW_TRUE);
}

Rendering::Rendering()
{
    request_screenshot_ = false;
    wayland_ = false;
}

bool Rendering::init()
{
#if GLFW_VERSION_MAJOR > 2 && GLFW_VERSION_MINOR > 3
    const char* desktop = getenv("XDG_CURRENT_DESKTOP");
    if (desktop) {
        // Forcing X11 on Gnome makes the server use xWayland which has proper Server Side Decorations as opposed to Wayland.
        if (strstr(desktop, "GNOME") != nullptr || 
            strstr(desktop, "Unity") != nullptr ){
            g_printerr("Forcing X11 / xWayland on %s desktop\n", desktop);
            glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
        }
        else {
            g_printerr("Detected %s desktop\n", desktop);
            glfwInitHint(GLFW_PLATFORM, GLFW_ANY_PLATFORM);
        }
    }
#endif
    //
    // Setup GLFW
    //
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()){
        g_printerr("Failed to Initialize GLFW.\n");
        return false;
    }

#if GLFW_VERSION_MAJOR > 2 && GLFW_VERSION_MINOR > 3
    wayland_ = glfwGetPlatform() == GLFW_PLATFORM_WAYLAND;
#endif

    //
    // Gstreamer setup
    //
    std::string plugins_scanner = SystemToolkit::cwd_path() + "gst-plugin-scanner" ;
    if ( SystemToolkit::file_exists(plugins_scanner)) {
        Log::Info("Found Gstreamer scanner %s", plugins_scanner.c_str());
        g_setenv ("GST_PLUGIN_SCANNER", plugins_scanner.c_str(), TRUE);
    }

    const gchar *c = g_getenv("GST_PLUGIN_PATH");
    std::string plugins_path = c != NULL ? std::string(c) : "";

    std::string local_plugin_path = SystemToolkit::cwd_path() + "gstreamer-1.0";
    if ( SystemToolkit::file_exists(local_plugin_path)){
        if (!plugins_path.empty())
            plugins_path += ":";
        plugins_path += local_plugin_path;
    }

#ifdef GSTREAMER_SHMDATA_PLUGIN
    std::string shmdata_plugin_path = GSTREAMER_SHMDATA_PLUGIN;
    if ( SystemToolkit::file_exists(shmdata_plugin_path)) {
        if (!plugins_path.empty())
            plugins_path += ":";
        plugins_path += SystemToolkit::path_filename(shmdata_plugin_path);
    }
#endif

    if ( !plugins_path.empty() ) {
        Log::Info("Found Gstreamer plugins in %s", plugins_path.c_str());
        g_setenv ("GST_PLUGIN_PATH", plugins_path.c_str(), TRUE);
    }
    std::string frei0r_path = SystemToolkit::cwd_path() + "frei0r-1" ;
    if ( SystemToolkit::file_exists(frei0r_path)) {
        Log::Info("Found Frei0r plugins in %s", frei0r_path.c_str());
        g_setenv ("FREI0R_PATH", frei0r_path.c_str(), TRUE);
    }

    // init gstreamer with opengl API
    g_setenv ("GST_GL_API", VIMIX_GL_VERSION, TRUE);
    gst_init (NULL, NULL);

    // increase selection rank for GPU decoding plugins
    std::list<std::string> gpuplugins = GstToolkit::enable_gpu_decoding_plugins(Settings::application.render.gpu_decoding);
    if (gpuplugins.size() > 0) {
        Settings::application.render.gpu_decoding_available = true;
        Log::Info("Found the following hardware decoding gstreamer plugin(s):");
        int i = 1;
        for(auto it = gpuplugins.begin(); it != gpuplugins.end(); it++, ++i)
            Log::Info("%d. %s", i, (*it).c_str());
        if (Settings::application.render.gpu_decoding)
            Log::Info("Hardware decoding enabled.");
        else
            Log::Info("Hardware decoding disabled.");
    }
    else {
        Log::Info("No hardware decoding plugin found.");
    }

    //
    // Monitors
    //
    // initial detection of monitors
    Rendering::MonitorConnect(nullptr, GLFW_DONT_CARE);
    // automatic detection of monitor connect & disconnect
    glfwSetMonitorCallback(Rendering::MonitorConnect);

    //
    // Main window
    //
    if ( !main_.init() )
        return false;

    //
    // Initialize OpenGL
    //

    // take opengl context ownership on main window
    glfwMakeContextCurrent(main_.window());

    // Initialize OpenGL loader 
    bool err = gladLoadGL((GLADloadfunc) glfwGetProcAddress) == 0;
    if (err) {
        g_printerr("Failed to initialize GLAD OpenGL loader.");
        return false;
    }

    // We decide for byte aligned textures all over
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);

    // acurate derivative for shader
    glHint(GL_FRAGMENT_SHADER_DERIVATIVE_HINT, GL_NICEST);

    // nice line smoothing
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    // Enable Antialiasing multisampling for main window
    if ( Rendering::openGLExtensionAvailable( "GL_ARB_multisample" ) ||
         Rendering::openGLExtensionAvailable( "GL_ARB_texture_multisample" )||
         Rendering::openGLExtensionAvailable( "GL_EXT_multisample" )||
         Rendering::openGLExtensionAvailable( "GL_EXT_framebuffer_multisample" )) {

        if (Settings::application.render.multisampling > 0) {
            glEnable(GL_MULTISAMPLE);
            if ( Rendering::openGLExtensionAvailable( "GL_NV_multisample_filter_hint") )
                glHint(GL_MULTISAMPLE_FILTER_HINT_NV, GL_NICEST);
        }
        else
            glDisable(GL_MULTISAMPLE);
    }
    else
        Settings::application.render.multisampling = 0;

    //  no vsync on main opengl context (blocs rendering if main window is in background on some systems)
    glfwSwapInterval(0);


#ifdef USE_GST_OPENGL_SYNC_HANDLER
#if GST_GL_HAVE_PLATFORM_WGL
    global_gl_context = gst_gl_context_new_wrapped (display, (guintptr) wglGetCurrentContext (),
                                                   GST_GL_PLATFORM_WGL, GST_GL_API_OPENGL);
#elif GST_GL_HAVE_PLATFORM_CGL
    //    global_display = GST_GL_DISPLAY ( glfwGetCocoaMonitor(main_.window()) );
    global_display = GST_GL_DISPLAY (gst_gl_display_cocoa_new ());

    global_gl_context = gst_gl_context_new_wrapped (global_display,
                                                   (guintptr) 0,
                                                   GST_GL_PLATFORM_CGL, GST_GL_API_OPENGL);
#elif GST_GL_HAVE_PLATFORM_GLX
    global_display = (GstGLDisplay*) gst_gl_display_x11_new_with_display( glfwGetX11Display() );
    global_gl_context = gst_gl_context_new_wrapped (global_display,
                                                   (guintptr) glfwGetGLXContext(main_.window()),
                                                   GST_GL_PLATFORM_GLX, GST_GL_API_OPENGL);
#endif
#endif

    // Initialize tablet input for pressure support
    TabletInput::instance().init();

    // // error message management
    // unsigned int err = 0;
    // while((err = glGetError()) != GL_NO_ERROR){
    //     fprintf(stderr, "GLFW  error %d \n",  err );
    // }

    return true;
}

void Rendering::show(bool show_main_window)
{
    // show main window (command line option --no-main-window to disable it)
    if (show_main_window) {
        main_.show();
        // show menu on first show
        UserInterface::manager().showPannel(NAV_MENU);
    }
    else {
        // find if any output window is active in settings
        auto it = Settings::application.monitors.begin();
        for (; it != Settings::application.monitors.end(); ++it) {
            if (it->second.active_output)
                break;
        }
        // if no config specified or none has an active output, activate first monitor output
        if ( it == Settings::application.monitors.end()) {
            std::lock_guard<std::mutex> lock(monitors_mutex_);
            if (!monitors_.empty())
                monitors_.begin()->output.setActive(true);
        }
    }

}

bool Rendering::isActive()
{
    return ( main_.window() != NULL && !glfwWindowShouldClose(main_.window()) );
}

void Rendering::pushBackDrawCallback(RenderingCallback function)
{
    draw_callbacks_.push_back(function);
}

void Rendering::draw()
{
    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
    // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
    glfwPollEvents();

    // Poll tablet input for pressure support
#ifndef WIN32
    TabletInput::instance().pollEvents();
#endif

    // change windows fullscreen mode or title if requested
    main_.changeFullscreen_();
    main_.changeTitle_();

    // draw output windows
    drawOutputWindows();

    // operate on main window context
    main_.makeCurrent();

    // draw scene and GUI in main window
    std::list<Rendering::RenderingCallback>::iterator iter;
    for (iter=draw_callbacks_.begin(); iter != draw_callbacks_.end(); ++iter)
    {
        (*iter)();
    }

    // perform screenshot if requested
    if (request_screenshot_) {
        screenshot_.captureGL(main_.width(), main_.height());
        request_screenshot_ = false;
    }

    glfwSwapBuffers(main_.window());

    // software framerate limiter < 60 FPS
    {
        static GTimer *timer = g_timer_new ();
        double elapsed = g_timer_elapsed (timer, NULL) * 1000000.0;
        if ( (elapsed < 16600.0) && (elapsed > 0.0) )
            g_usleep( 16600 - (gulong)elapsed  );
        g_timer_start(timer);
    }
}

void Rendering::terminate()
{
    // Terminate tablet input
#ifndef WIN32
    TabletInput::instance().terminate();
#endif

    // terminate all output windows in monitors
    {
        std::lock_guard<std::mutex> lock(monitors_mutex_);
        for (auto it = monitors_.begin(); it != monitors_.end(); ++it) {
            if (it->output.isInitialized())
                it->output.terminate();
        }
    }

    // terminate main window
    main_.terminate();
}

void Rendering::pushAttrib(GlmToolkit::RenderingAttrib ra)
{
    // push it to top of pile
    draw_attributes_.push_front(ra);

    // apply Changes to OpenGL
    glViewport(0, 0, ra.viewport.x, ra.viewport.y);
    glClearColor(ra.clear_color.r, ra.clear_color.g, ra.clear_color.b, ra.clear_color.a);
}

void Rendering::popAttrib()
{
    // pops the top of the pile
    if (draw_attributes_.size() > 0)
        draw_attributes_.pop_front();

    // set attribute element to default
    GlmToolkit::RenderingAttrib ra = currentAttrib();

    // apply Changes to OpenGL
    glViewport(0, 0, ra.viewport.x, ra.viewport.y);
    glClearColor(ra.clear_color.r, ra.clear_color.g, ra.clear_color.b, ra.clear_color.a);
}

GlmToolkit::RenderingAttrib Rendering::currentAttrib()
{
    // default rendering attrib is the main window's
    GlmToolkit::RenderingAttrib ra = main_.attribs();

    // but if there is an element at top, return it
    if (draw_attributes_.size() > 0)
        ra = draw_attributes_.front();
    return ra;
}

glm::mat4 Rendering::Projection()
{
    static glm::mat4 projection = glm::ortho(-SCENE_UNIT, SCENE_UNIT, -SCENE_UNIT, SCENE_UNIT, -SCENE_DEPTH, 1.f);
    glm::mat4 scale = glm::scale(glm::identity<glm::mat4>(), glm::vec3(1.f, main_.aspectRatio(), 1.f));

    return projection * scale;
}


glm::vec3 Rendering::unProject(glm::vec2 screen_coordinate, glm::mat4 modelview)
{
    glm::vec3 coordinates = glm::vec3( screen_coordinate.x, main_.height() - screen_coordinate.y, 0.f);
    glm::vec4 viewport = glm::vec4( 0.f, 0.f, main_.width(), main_.height());

//    Log::Info("unproject %d x %d", main_window_attributes_.viewport.x, main_window_attributes_.viewport.y);

    glm::vec3 point = glm::unProject(coordinates, modelview, Projection(), viewport);

    return point;
}


glm::vec2 Rendering::project(glm::vec3 scene_coordinate, glm::mat4 modelview, bool to_framebuffer)
{
    glm::vec4 viewport;
    if (to_framebuffer)
        viewport= glm::vec4( 0.f, 0.f, main_.width(), main_.height());
    else
        viewport= glm::vec4( 0.f, 0.f, main_.width() / main_.dpiScale(), main_.height() / main_.dpiScale());
    glm::vec3 P = glm::project( scene_coordinate, modelview, Projection(), viewport );

    return glm::vec2(P.x, viewport.w - P.y);
}

Screenshot *Rendering::currentScreenshot()
{
    return &screenshot_;
}

void Rendering::requestScreenshot()
{
    request_screenshot_ = true;
}

bool Rendering::openGLExtensionAvailable(const char *extensionname)
{
    bool ret = false;

    GLint numExtensions = 0;
    glGetIntegerv( GL_NUM_EXTENSIONS, &numExtensions );
    for (int i = 0; i < numExtensions; ++i){
        const GLubyte *ccc = glGetStringi(GL_EXTENSIONS, i);
        //  extension available
        if ( strcmp( (const char*)ccc, extensionname) == 0 ){
            ret = true;
            break;
        }
    }

    return ret;
}

// RAM usage in GPU
// returns { CurAvailMemoryInKB, TotalMemoryInKB }
// MAX values means the info in not available

#define GL_GPU_MEM_INFO_TOTAL_AVAILABLE_MEM_NVX 0x9048
#define GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX 0x9049
#define GL_TEXTURE_FREE_MEMORY_ATI		0x87FC

glm::ivec2 Rendering::getGPUMemoryInformation()
{
    glm::ivec2 ret(INT_MAX, INT_MAX);

    // Detect method to get info
    static int meminfomode = -1;
    if (meminfomode<0) {
        // initialized
        meminfomode = 0;
        GLint numExtensions = 0;
        glGetIntegerv( GL_NUM_EXTENSIONS, &numExtensions );
        for (int i = 0; i < numExtensions; ++i){
            const GLubyte *ccc = glGetStringi(GL_EXTENSIONS, i);
            // NVIDIA extension available
            if ( strcmp( (const char*)ccc, "GL_NVX_gpu_memory_info") == 0 ){
                meminfomode = 1;
                break;
            }
            // ATI extension available
            else if ( strcmp( (const char*)ccc, "GL_ATI_meminfo") == 0 ){
                meminfomode = 2;
                break;
            }
        }

    }

    // NVIDIA
    if (meminfomode == 1) {
        static GLint nTotalMemoryInKB = -1;
        if (nTotalMemoryInKB<0)
            glGetIntegerv( GL_GPU_MEM_INFO_TOTAL_AVAILABLE_MEM_NVX,  &nTotalMemoryInKB );
        ret.y = nTotalMemoryInKB;

        glGetIntegerv( GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX, &ret.x );
    }
    // ATI
    else if (meminfomode == 2) {
        GLint memInKB[4] = { 0, 0, 0, 0 };
        glGetIntegerv( GL_TEXTURE_FREE_MEMORY_ATI, &memInKB[0] );
        ret.x = memInKB[3] ;
    }

    return ret;
}


bool Rendering::shouldHaveEnoughMemory(glm::vec3 resolution, int flags, int num_buffers)
{
    glm::ivec2 RAM = getGPUMemoryInformation();

    // approximation of RAM needed for such FBO
    GLint framebufferMemoryInKB = ( resolution.x * resolution.x *
                                    ((flags & FrameBuffer::FrameBuffer_alpha)?4:3) * ((flags & FrameBuffer::FrameBuffer_multisampling)?2:1) ) / 1024;

    return ( RAM.x > framebufferMemoryInKB * (num_buffers + 1) );
}


std::list<Rendering::Monitor> Rendering::monitorsList() const
{
    std::lock_guard<std::mutex> lock(monitors_mutex_);
    return monitors_;
}

glm::vec3 Rendering::monitorsResolution() 
{
    std::lock_guard<std::mutex> lock(monitors_mutex_);

    glm::vec3 resolution(0, 0, 0);
    for (const auto& monitor : monitors_) {
        resolution.x = std::max(resolution.x, (float) (monitor.geometry.x + monitor.geometry.z) );
        resolution.y = std::max(resolution.y, (float) (monitor.geometry.y + monitor.geometry.w) );
    }
    resolution.z = 0; 
    return resolution;
}

void Rendering::deactivateOutput(const GLFWmonitor* monitor)
{
    if (monitor == nullptr)
        return;

    std::lock_guard<std::mutex> lock(monitors_mutex_);

    for (auto it = monitors_.begin(); it != monitors_.end(); ++it) {
        if (it->monitor == monitor) {
            it->output.setActive(false);
            break;
        }
    }
}

GLFWmonitor *Rendering::monitorNamed(const std::string &name)
{
    std::lock_guard<std::mutex> lock(monitors_mutex_);

    // default to primary monitor
    GLFWmonitor *mo = glfwGetPrimaryMonitor();

    // search in the monitors list
    for (auto it = monitors_.begin(); it != monitors_.end(); ++it) {
        if (it->name == name) {
            // found the monitor with this name
            mo = it->monitor;
            break;
        }
    }

    return mo;
}

GLFWmonitor *Rendering::monitorAt(int x, int y)
{
    std::lock_guard<std::mutex> lock(monitors_mutex_);

    // default to primary monitor
    GLFWmonitor *mo = glfwGetPrimaryMonitor();

    // search in the monitors list
    for (auto it = monitors_.begin(); it != monitors_.end(); ++it) {
        if (x >= it->geometry.x && x <= it->geometry.x + it->geometry.z &&
            y >= it->geometry.y && y <= it->geometry.y + it->geometry.w) {
            // found the monitor containing this point !
            mo = it->monitor;
            break;
        }
    }

    return mo;
}

void Rendering::drawOutputWindows()
{
    std::lock_guard<std::mutex> lock(Rendering::manager().monitors_mutex_);

    bool busy = false;
    // iterate through all monitors
    for (auto it = monitors_.begin(); it != monitors_.end(); ++it) {
        // if output is active and initialized, draw it
        if (it->output.isActive() && it->output.isInitialized()) {
            it->output.draw(Canvas::manager().session()->frame());
            busy = true;
        }
        // if output is active but not initialized, initialize it
        else if (it->output.isActive() && !it->output.isInitialized()) {
            it->output.init(it->monitor, Rendering::manager().mainWindow().window());
        }
        // if output is not active but initialized, terminate it
        else if (!it->output.isActive() && it->output.isInitialized()) {
            it->output.terminate();
        }
    }

    // inhibit screensaver if any output is active
    inhibitScreensaver(busy);
}

void Rendering::MonitorConnect(GLFWmonitor* monitor, int event)
{
    // lock monitors list while updating it
    {
        std::lock_guard<std::mutex> lock(Rendering::manager().monitors_mutex_);

        // reset list of monitors
        Rendering::manager().monitors_.clear();

        // list monitors with GLFW
        int count_monitors = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&count_monitors);

        // Fill list of monitors of rendering manager
        for (int i = 0; i < count_monitors;  i++) {
            // fill monitor structure
            int x = 0, y = 0;
            glfwGetMonitorPos(monitors[i], &x, &y);
            const GLFWvidmode *vm = glfwGetVideoMode(monitors[i]);
            std::string n = glfwGetMonitorName(monitors[i]);
            glm::ivec4 geometry(x, y, vm->width, vm->height);
            // add to list
            Rendering::manager().monitors_.push_back(Monitor(monitors[i], n, geometry));
            // add to settings if not already present
            if ( Settings::application.monitors.find(n) == Settings::application.monitors.end() ) {
                // sets default config for this monitor
                Settings::application.monitors[n] = Settings::MonitorConfig();
            }
            else if (Settings::application.monitors[n].active_output) {
                // activate output window if previously active
                Rendering::manager().monitors_.back().output.setActive(true);
            }
        }
    }

    // inform Displays View that monitors changed (after releasing the lock)
    if (event != GLFW_DONT_CARE)
        Mixer::manager().view(View::DISPLAYS)->recenter();

#ifndef NDEBUG
    if (event == GLFW_CONNECTED)
        g_printerr("Monitor %s connected\n",  glfwGetMonitorName(monitor));
    else if (event == GLFW_DISCONNECTED)
        g_printerr("Monitor %s disconnected\n",  glfwGetMonitorName(monitor));
#endif
}

// GDBus for screensaver inhibition (works on both X11 and Wayland)
#ifdef GLFW_EXPOSE_NATIVE_GLX
guint screensaver_inhibit_cookie_ = 0;
GDBusConnection *session_dbus_ = NULL;

void Rendering::inhibitScreensaver (bool on)
{
    /*
     * Inhibit or un-inhibit the desktop screensaver via the
     * XDG Desktop Portal org.freedesktop.portal.Inhibit API.
     *
     * When 'on' is true:
     *  - Obtain a connection to the session bus (cached in session_dbus_).
     *  - Call the Inhibit method with: window identifier (empty string for whole app),
     *    flags (8 = inhibit idle/screensaver), and options (reason string).
     *  - The call returns an object path handle which identifies the inhibit request.
     *
     * When 'on' is false:
     *  - The inhibition is automatically released when the handle is removed or app exits.
     *  - We explicitly close the session bus connection to clean up.
     *
     * Notes:
     *  - This uses the XDG Desktop Portal which works in Flatpak sandboxes.
     *  - Requires --talk-name=org.freedesktop.portal.Desktop permission.
     *  - This function uses synchronous D-Bus calls. They may block briefly.
     *  - Errors from g_dbus_connection_call_sync are reported to the log and freed.
     *  - Works on both X11 and Wayland.
     */
    if (on ) {
        GError *error = NULL;
        /* Lazily open a connection to the session bus and cache it. */
        if (session_dbus_ == NULL)
            session_dbus_ = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);

        /* Only inhibit once: do nothing if we already have a cookie. */
        if (session_dbus_ != NULL && screensaver_inhibit_cookie_ == 0) {
            /* Build options dictionary with reason */
            GVariantBuilder builder;
            g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
            g_variant_builder_add(&builder, "{sv}", "reason",
                                g_variant_new_string("Video mixing in progress"));

            /* Call org.freedesktop.portal.Inhibit.Inhibit(window, flags, options) */
            GVariant *result = g_dbus_connection_call_sync(
                session_dbus_,
                "org.freedesktop.portal.Desktop",
                "/org/freedesktop/portal/desktop",
                "org.freedesktop.portal.Inhibit",
                "Inhibit",
                g_variant_new("(su@a{sv})",
                              "",    // window identifier (empty for whole app)
                              8,     // flags: 8 = inhibit idle/screensaver
                              g_variant_builder_end(&builder)),
                G_VARIANT_TYPE("(o)"),  // returns object path handle
                G_DBUS_CALL_FLAGS_NONE,
                -1,
                NULL,
                &error
            );

            if (result != NULL) {
                /* The returned variant contains an object path handle.
                 * We store a dummy cookie value to indicate inhibition is active. */
                screensaver_inhibit_cookie_ = 1;
                g_variant_unref(result);
                Log::Info("Screensaver inhibited for vimix via XDG Desktop Portal");
            } else {
                /* If the call failed, log the error and ensure cookie is zero. */
                if (error != NULL) {
                    Log::Info("Could not inhibit screensaver: %s", error->message);
                    g_error_free(error);
                    screensaver_inhibit_cookie_ = 0;
                }
            }
        }
    }
    else {
        /* Un-inhibit: the portal automatically releases when we close the session */
        if (screensaver_inhibit_cookie_ != 0) {
            if (session_dbus_ != NULL) {
                /* Close and drop our cached session bus connection. */
                /* The portal will automatically release the inhibition. */
                g_object_unref(session_dbus_);
                session_dbus_ = NULL;
                Log::Info("Screensaver inhibition disabled");
            }
            /* Reset cookie so subsequent calls can re-inhibit if needed. */
            screensaver_inhibit_cookie_ = 0;
        }
    }
}
#else
// TODO Implement for other platforms (e.g. macOS)
void Rendering::inhibitScreensaver (bool)
{}
#endif
