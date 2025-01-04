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
#endif

// standalone image loader
#include <stb_image.h>

// vimix
#include "defines.h"
#include "Log.h"
#include "Stream.h"
#include "Resource.h"
#include "Settings.h"
#include "ImageShader.h"
#include "Mixer.h"
#include "SystemToolkit.h"
#include "GstToolkit.h"
#include "UserInterfaceManager.h"
#include "ControlManager.h"
#include "ImageFilter.h"
#include "Primitives.h"

#include "RenderingManager.h"

#ifdef USE_GST_OPENGL_SYNC_HANDLER

GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>

//
// Discarded because not working under OSX - kept in case it would become useful
//
// Linking pipeline to the rendering instance ensures the opengl contexts
// created by gstreamer inside plugins (e.g. glsinkbin) is the same
//
static GstGLContext *global_gl_context = NULL;
static GstGLDisplay *global_display = NULL;

static GstBusSyncReply bus_sync_handler( GstBus *, GstMessage * msg, gpointer )
{
    if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_NEED_CONTEXT) {
        const gchar* contextType;
        gst_message_parse_context_type(msg, &contextType);

        if (!g_strcmp0(contextType, GST_GL_DISPLAY_CONTEXT_TYPE)) {
            GstContext *displayContext = gst_context_new(GST_GL_DISPLAY_CONTEXT_TYPE, TRUE);
            gst_context_set_gl_display(displayContext, global_display);
            gst_element_set_context(GST_ELEMENT(msg->src), displayContext);
            gst_context_unref (displayContext);

            g_info ("Managed %s\n", contextType);
        }
        if (!g_strcmp0(contextType, "gst.gl.app_context")) {
            GstContext *appContext = gst_context_new("gst.gl.app_context", TRUE);
            GstStructure* structure = gst_context_writable_structure(appContext);
            gst_structure_set(structure, "context", GST_TYPE_GL_CONTEXT, global_gl_context, nullptr);
            gst_element_set_context(GST_ELEMENT(msg->src), appContext);
            gst_context_unref (appContext);

            g_info ("Managed %s\n", contextType);
        }
    }

    gst_message_unref (msg);

    return GST_BUS_DROP;
}

void Rendering::LinkPipeline( GstPipeline *pipeline )
{
    // capture bus signals to force a unique opengl context for all GST elements
    GstBus* m_bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    gst_bus_set_sync_handler (m_bus, (GstBusSyncHandler) bus_sync_handler, pipeline, NULL);
    gst_object_unref (m_bus);
}
#endif


bool openGLExtensionAvailable(const char *extensionname)
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

static void glfw_error_callback(int error, const char* description)
{
    g_printerr("Glfw Error %d: %s\n",  error, description);
}

static void WindowResizeCallback( GLFWwindow *w, int width, int height)
{
    if (Rendering::manager().mainWindow().window() == w) {
        // UI manager tries to keep windows in the workspace
        WorkspaceWindow::notifyWorkspaceSizeChanged(Rendering::manager().mainWindow().previous_size.x, Rendering::manager().mainWindow().previous_size.y, width, height);
        Rendering::manager().mainWindow().previous_size = glm::vec2(width, height);
        Rendering::manager().draw();
    }

    int id = Rendering::manager().window(w)->index();
    Settings::application.windows[id].fullscreen = glfwGetWindowMonitor(w) != nullptr;
    if (!Settings::application.windows[id].fullscreen) {
        Settings::application.windows[id].w = width;
        Settings::application.windows[id].h = height;
    }

}

static void WindowMoveCallback( GLFWwindow *w, int x, int y)
{
    int id = Rendering::manager().window(w)->index();
    if (!Settings::application.windows[id].fullscreen) {
        Settings::application.windows[id].x = x;
        Settings::application.windows[id].y = y;
    }
}

static void OutputWindowEvent( GLFWwindow *w, int button, int action, int)
{
    // detect mouse press
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        static double seconds = 0.f;
        // detect double clic
        if ( glfwGetTime() - seconds < 0.2f ) {

            // exit fullscreen if its the case
            if (glfwGetWindowMonitor(w) != nullptr)
                Rendering::manager().window(w)->exitFullscreen();
            // show main window
            else
            Rendering::manager().mainWindow().show();
        }
        // for next double clic detection
        seconds = glfwGetTime();
    }
}

static void WindowCloseCallback( GLFWwindow* w )
{
    // close main window
    if (Rendering::manager().mainWindow().window() == w) {
        if (!UserInterface::manager().TryClose())
            glfwSetWindowShouldClose(w, GLFW_FALSE);
    }
    // not main window
    else {
        // if headless (main window not visile)
        if (glfwGetWindowAttrib(Rendering::manager().mainWindow().window(), GLFW_VISIBLE)
            == GL_FALSE) {
            // close rendering manager = quit
            Rendering::manager().close();
        }
        // attempt to close shows display view
        else {
            Mixer::manager().setView(View::DISPLAYS);
            Rendering::manager().mainWindow().show();
        }
    }
}

void Rendering::MonitorConnect(GLFWmonitor* monitor, int event)
{
    // reset list of monitors
    Rendering::manager().monitors_geometry_.clear();

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
        // add
        Rendering::manager().monitors_geometry_[n] = glm::ivec4(x, y, vm->width, vm->height);
    }

    // Disconnection of a monitor messes up with fullscreen windows
    if (event == GLFW_DISCONNECTED) {
        // exit fullscreen all windows
        for (auto w = Rendering::manager().windows_.begin();
             w != Rendering::manager().windows_.end(); ++w)
            w->second->setFullscreen_(nullptr);
    }

    // inform Displays View that monitors changed
    Mixer::manager().view(View::DISPLAYS)->recenter();

#ifndef NDEBUG
    if (event == GLFW_CONNECTED)
        g_printerr("Monitor %s connected\n",  glfwGetMonitorName(monitor));
    else if (event == GLFW_DISCONNECTED)
        g_printerr("Monitor %s disconnected\n",  glfwGetMonitorName(monitor));
#endif
}

GLFWmonitor *Rendering::monitorAt(int x, int y)
{
    // default to primary monitor
    GLFWmonitor *mo = glfwGetPrimaryMonitor();

    // list all monitors
    int count_monitors = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count_monitors);

    // if there is more than one monitor
    if (count_monitors > 1) {
        // try every monitor
        for (int i = 0; i < count_monitors;  i++) {
            int workarea_x, workarea_y;
            glfwGetMonitorPos(monitors[i], &workarea_x, &workarea_y);
            const GLFWvidmode *vm = glfwGetVideoMode(monitors[i]);
            if ( x >= workarea_x && x <= workarea_x + vm->width &&
                 y >= workarea_y && y <= workarea_y + vm->height) {
                // found the monitor containing this point !
                mo = monitors[i];
                break;
            }
        }
    }

    return mo;
}

std::string Rendering::monitorNameAt(int x, int y)
{
    return glfwGetMonitorName(monitorAt(x, y));
}

GLFWmonitor *Rendering::monitorNamed(const std::string &name)
{
    // default to primary monitor
    GLFWmonitor *mo = glfwGetPrimaryMonitor();

    // list all monitors
    int count_monitors = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count_monitors);

    // if there is more than one monitor
    if (count_monitors > 1) {
        // try every monitor
        for (int i = 0; i < count_monitors;  i++) {
            if ( name.compare( glfwGetMonitorName(monitors[i]) ) == 0 ){
                // found the monitor with this name
                mo = monitors[i];
                break;
            }
        }
    }

    return mo;
}

void Rendering::close()
{
    glfwSetWindowShouldClose(main_.window(), GLFW_TRUE);
}

Rendering::Rendering()
{
//    main_window_ = nullptr;
    request_screenshot_ = false;
}

bool Rendering::init()
{
    //
    // Setup GLFW
    //
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()){
        g_printerr("Failed to Initialize GLFW.\n");
        return false;
    }

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
    Rendering::MonitorConnect(nullptr, GLFW_DONT_CARE);
    // automatic detection of monitor connect & disconnect
    glfwSetMonitorCallback(Rendering::MonitorConnect);

    //
    // Main window
    //
    if ( !main_.init(0) )
        return false;


    unsigned int err = 0;

    while((err = glGetError()) != GL_NO_ERROR){
        fprintf(stderr, "GLFW  error %d \n",  err );
    }

    //
    // Output windows will be initialized in draw
    //
    outputs_ = std::vector<RenderingWindow>(MAX_OUTPUT_WINDOW);

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

    return true;
}

RenderingWindow* Rendering::window(GLFWwindow *w)
{
    if (windows_.count(w))
        return windows_[w];
    else
        return &main_;
}

RenderingWindow* Rendering::window(int index)
{
    if (index > 0 && index <= MAX_OUTPUT_WINDOW )
        return &outputs_[index - 1];
    else
        return &main_;
}


void Rendering::show(bool show_main_window)
{
    // show output windows
    for (auto it = outputs_.begin(); it != outputs_.end(); ++it)
        it->show();

    // show main window
    if (show_main_window || Settings::application.num_output_windows < 1 )
        main_.show();

    // show menu on first show
    UserInterface::manager().showPannel(NAV_MENU);
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

    // change windows fullscreen mode if requested
    main_.changeFullscreen_();
    for (auto it = outputs_.begin(); it != outputs_.end(); ++it)
        it->changeFullscreen_();

    // change main window title if requested
    if (!main_new_title_.empty()) {
        main_.setTitle(main_new_title_);
        main_new_title_.clear();
    }

    // operate on main window context
    main_.makeCurrent();

    // draw
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


    // draw output windows and count number of success
    int count = 0;
    for (auto it = outputs_.begin(); it != outputs_.end(); ++it) {
        if ( it->draw( Mixer::manager().session()->frame() ) )
            ++count;
    }
    // terminate or initialize output windows to match number of output windows
    if (count > Settings::application.num_output_windows)
        outputs_[count-1].terminate();
    else if (count < Settings::application.num_output_windows) {
        outputs_[count].init( count+1, main_.window());
        outputs_[count].show();
    }

    // software framerate limiter < 62 FPS
    {
        static GTimer *timer = g_timer_new ();
        double elapsed = g_timer_elapsed (timer, NULL) * 1000000.0;
        if ( (elapsed < 15600.0) && (elapsed > 0.0) )
            g_usleep( 15600 - (gulong)elapsed  );
        g_timer_start(timer);
    }
}

void Rendering::terminate()
{
    // terminate all windows
    for (auto it = outputs_.begin(); it != outputs_.end(); ++it)
        it->terminate();

    main_.terminate();
}

void Rendering::pushAttrib(RenderingAttrib ra)
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
    RenderingAttrib ra = currentAttrib();

    // apply Changes to OpenGL
    glViewport(0, 0, ra.viewport.x, ra.viewport.y);
    glClearColor(ra.clear_color.r, ra.clear_color.g, ra.clear_color.b, ra.clear_color.a);
}

RenderingAttrib Rendering::currentAttrib()
{
    // default rendering attrib is the main window's
    RenderingAttrib ra = main_.attribs();

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

void Rendering::FileDropped(GLFWwindow *, int path_count, const char* paths[])
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
        UserInterface::manager().showPannel(  Mixer::manager().numSource() );
        Rendering::manager().mainWindow().show();
    }
}


Screenshot *Rendering::currentScreenshot()
{
    return &screenshot_;
}

void Rendering::requestScreenshot()
{
    request_screenshot_ = true;
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


bool Rendering::shouldHaveEnoughMemory(glm::vec3 resolution, int flags)
{
    glm::ivec2 RAM = getGPUMemoryInformation();

    // approximation of RAM needed for such FBO
    GLint framebufferMemoryInKB = ( resolution.x * resolution.x *
                                    ((flags & FrameBuffer::FrameBuffer_alpha)?4:3) * ((flags & FrameBuffer::FrameBuffer_multisampling)?2:1) ) / 1024;

    return ( RAM.x > framebufferMemoryInKB * 3 );
}



// custom surface with a new VAO
class WindowSurface : public MeshSurface
{
public:
    WindowSurface(Shader *s = new ImageShader)
        : MeshSurface(s)
    {}
    void init () override
    {
        generate_mesh(32, 32);
        Primitive::init();
    }
};

// pattern source can be shared because all windows render the same framebuffer resolution
class Stream *RenderingWindow::pattern_ = new Stream;

RenderingWindow::RenderingWindow() : window_(NULL), master_(NULL),
    index_(-1), dpi_scale_(1.f), textureid_(0), fbo_(0), surface_(nullptr), request_change_fullscreen_(false)
{

}

RenderingWindow::~RenderingWindow()
{
    if (window_ != NULL)
        terminate();
    // delete pattern_;
    RenderingWindow::pattern_->close();
}

void RenderingWindow::setTitle(const std::string &title)
{
    std::string fulltitle;
    if ( title.empty() )
        fulltitle = Settings::application.windows[index_].name;
    else
        fulltitle = title + std::string(" - " APP_NAME);

    if (window_ != NULL)
        glfwSetWindowTitle(window_, fulltitle.c_str());
}

void RenderingWindow::setIcon(const std::string &resource)
{
#ifndef APPLE
    size_t fpsize = 0;
    const char *fp = Resource::getData(resource, &fpsize);
    if (fp != nullptr && window_) {
        GLFWimage icon;
        icon.pixels = stbi_load_from_memory( (const stbi_uc*)fp, fpsize, &icon.width, &icon.height, nullptr, 4 );
        glfwSetWindowIcon( window_, 1, &icon );
        free( icon.pixels );
    }
#endif
}

GLFWmonitor *RenderingWindow::monitor()
{
    // get monitor at the center of the window
    int x = 0, y = 0, w = 2, h = 2;
    if (window_) {
        glfwGetWindowSize(window_, &w, &h);
        glfwGetWindowPos(window_, &x, &y);
    }
    return Rendering::manager().monitorAt(x + w/2, y + h/2);
}

void RenderingWindow::setFullscreen_(GLFWmonitor *mo)
{
    if (!window_)
        return;

    // disable fullscreen mode
    if (mo == nullptr) {
        // store fullscreen mode
        Settings::application.windows[index_].fullscreen = false;

        // set to window mode
        glfwSetInputMode( window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        glfwSetWindowMonitor( window_, NULL, Settings::application.windows[index_].x,
                                                Settings::application.windows[index_].y,
                                                Settings::application.windows[index_].w,
                                                Settings::application.windows[index_].h, 0 );
    }
    // set fullscreen mode
    else {
        // store fullscreen mode
        Settings::application.windows[index_].fullscreen = true;
        Settings::application.windows[index_].monitor = glfwGetMonitorName(mo);

        // set to fullscreen mode
        const GLFWvidmode * mode = glfwGetVideoMode(mo);
        glfwSetWindowMonitor( window_, mo, 0, 0, mode->width, mode->height, mode->refreshRate);

        glfwSetInputMode( window_, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    }

    // Enable vsync on main window only (i.e. 0 if has a master)
    // Workaround for disabled vsync in fullscreen (https://github.com/glfw/glfw/issues/1072)
    glfwSwapInterval( master_ != nullptr ? 0 : Settings::application.render.vsync);

}

bool RenderingWindow::isFullscreen ()
{
//    return (glfwGetWindowMonitor(window_) != nullptr);
    return Settings::application.windows[index_].fullscreen;
}

void RenderingWindow::exitFullscreen ()
{
    // exit fullscreen
    request_change_fullscreen_ = isFullscreen();
}

void RenderingWindow::toggleFullscreen ()
{
    request_change_fullscreen_ = true;
}

void RenderingWindow::setFullscreen (std::string monitorname)
{
    Settings::application.windows[index_].monitor = monitorname;
    request_change_fullscreen_ = true;
}

void RenderingWindow::changeFullscreen_()
{
    // change upon request
    if (request_change_fullscreen_) {

        // done request
        request_change_fullscreen_ = false;

        GLFWmonitor *mo = monitor();
        if (index_ > 0)
            mo = Rendering::manager().monitorNamed( Settings::application.windows[index_].monitor );

        // if in fullscreen mode
        if (isFullscreen ()) {

            // changing fullscreen monitor
            if ( glfwGetWindowMonitor(window_) != mo)
                setFullscreen_(mo);
            else
                // exit fullscreen
                setFullscreen_(nullptr);
        }
        // not in fullscreen mode
        else {
            // enter fullscreen
            setFullscreen_(mo);
        }
    }
}

void RenderingWindow::setDecoration (bool on)
{
    if (window_ == NULL)
        return;

    Settings::application.windows[index_].decorated = on;
    glfwSetWindowAttrib( window_, GLFW_RESIZABLE, on ? GLFW_TRUE : GLFW_FALSE);
    glfwSetWindowAttrib( window_, GLFW_DECORATED, on ? GLFW_TRUE : GLFW_FALSE);
}

void RenderingWindow::setCoordinates(glm::ivec4 rect)
{
    if (window_ == NULL)
        return;

    // restore maximized window to be able to change its coordinates
    if (glfwGetWindowAttrib(window_, GLFW_MAXIMIZED))
        glfwRestoreWindow(window_);

    glfwSetWindowSize( window_, glm::max(50, rect.p), glm::max(50, rect.q));
    glfwSetWindowPos( window_, rect.x, rect.y);
}

int RenderingWindow::width()
{
    return window_attributes_.viewport.x;
}

int RenderingWindow::height()
{
     return window_attributes_.viewport.y;
}

int RenderingWindow::pixelsforRealHeight(float milimeters)
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

float RenderingWindow::aspectRatio()
{
    return static_cast<float>(window_attributes_.viewport.x) / static_cast<float>(window_attributes_.viewport.y);
}

bool RenderingWindow::init(int index, GLFWwindow *share)
{
    if (window_)
        return false;

    glfwMakeContextCurrent(NULL);

    ///
    /// Settings
    ///
    index_ = index;
    master_ = share;
    Settings::WindowConfig winset = Settings::application.windows[index_];

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
    glfwWindowHint(GLFW_SAMPLES, master_ == NULL ? Settings::application.render.multisampling : 0);

    // do not show at creation
    glfwWindowHint(GLFW_FOCUSED, GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);

    // restore decoration state
    if (master_ != NULL) {
        glfwWindowHint(GLFW_RESIZABLE, winset.decorated ? GLFW_TRUE : GLFW_FALSE);
        glfwWindowHint(GLFW_DECORATED, winset.decorated ? GLFW_TRUE : GLFW_FALSE);
    }

    // create the window
    window_ = glfwCreateWindow(winset.w, winset.h, winset.name.c_str(), NULL, master_);
    if (window_ == NULL){
        Log::Error("Failed to create GLFW Window %d", index_);
        return false;
    }

    // ensure minimal window size
    glfwSetWindowSizeLimits(window_, 500, 500, GLFW_DONT_CARE, GLFW_DONT_CARE);
    previous_size = glm::vec2(winset.w, winset.h);

    // set initial position
    glfwSetWindowPos(window_, winset.x, winset.y);

    // set icon
    setIcon("images/vimix_256x256.png");

    ///
    /// CALLBACKS
    ///
    // store global ref to pointers (used by callbacks)
    Rendering::manager().windows_[window_] = this;

    //
    // window position and resize callbacks
    //
    glfwSetWindowPosCallback( window_, WindowMoveCallback );
    glfwSetWindowSizeCallback( window_, WindowResizeCallback );

    //
    // set keyboard callback
    //
    // all windows capture keys
    glfwSetKeyCallback( window_, Control::keyboardCalback);
    glfwSetWindowCloseCallback( window_, WindowCloseCallback );

    if (master_ != NULL) {
        // additional window callbacks for user input in output windows
        glfwSetMouseButtonCallback( window_, OutputWindowEvent);
    }
    else {
        // additional window callbacks for main window
        glfwSetDropCallback( window_, Rendering::FileDropped);
    }

    //
    // Initialize OpenGL
    //
    // take opengl context ownership
    glfwMakeContextCurrent(window_);

    // Initialize OpenGL loader on first call
    static bool glad_initialized = false;
    if ( !glad_initialized ) {
        bool err = gladLoadGL((GLADloadfunc) glfwGetProcAddress) == 0;
        if (err) {
            Log::Error("Failed to initialize GLAD OpenGL loader.");
            return false;
        }
        glad_initialized = true;
    }

    // get rendering area
    glfwGetFramebufferSize(window_, &(window_attributes_.viewport.x), &(window_attributes_.viewport.y));
    // DPI scaling (retina)
    dpi_scale_ = float(window_attributes_.viewport.y) / float(winset.h);

    // We decide for byte aligned textures all over
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);

    // acurate derivative for shader
    glHint(GL_FRAGMENT_SHADER_DERIVATIVE_HINT, GL_NICEST);

    // nice line smoothing
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    // Enable Antialiasing multisampling for main window
    if ( openGLExtensionAvailable( "GL_ARB_multisample" ) ||
         openGLExtensionAvailable( "GL_ARB_texture_multisample" )||
         openGLExtensionAvailable( "GL_EXT_multisample" )||
         openGLExtensionAvailable( "GL_EXT_framebuffer_multisample" )) {

        if (Settings::application.render.multisampling > 0 && master_ == NULL) {
            glEnable(GL_MULTISAMPLE);
            if ( openGLExtensionAvailable( "GL_NV_multisample_filter_hint") )
                glHint(GL_MULTISAMPLE_FILTER_HINT_NV, GL_NICEST);
        }
        else
            glDisable(GL_MULTISAMPLE);
    }
    else
        Settings::application.render.multisampling = 0;

    // if not main window
    if ( master_ != NULL ) {
        // NO vsync on output windows
        glfwSwapInterval(0);

        // clear to black
        window_attributes_.clear_color = glm::vec4(0.f, 0.f, 0.f, 1.f);

        glfwShowWindow(window_);
    }
    else {
        //  vsync on main window
        glfwSwapInterval(Settings::application.render.vsync);

        // clear to grey
        window_attributes_.clear_color = glm::vec4(COLOR_BGROUND, 1.f);
    }

    //
    // default render black
    //
    (void) Resource::getTextureWhite(); // init white texture too
    textureid_ = Resource::getTextureBlack();

    //
    // capture possible init errors
    //
    unsigned int err = 0;
    while((err = glGetError()) != GL_NO_ERROR){
        Log::Warning("Error %d during OpenGL init.", err);
    }

    return true;
}

void RenderingWindow::terminate()
{
    // cleanup
    if (surface_ != nullptr)
        delete surface_;
    if (fbo_ != 0)
        glDeleteFramebuffers(1, &fbo_);
    if (window_ != NULL) {
        // remove global ref to pointers
        Rendering::manager().windows_.erase(window_);
        // delete window
        glfwDestroyWindow(window_);
    }

    // invalidate
    window_  = NULL;
    shader_  = nullptr;
    surface_ = nullptr;
    fbo_     = 0;
    index_   = -1;
    textureid_ = Resource::getTextureBlack();
}

void RenderingWindow::show()
{
    if (!window_)
        return;

    glfwShowWindow(window_);

    if ( Settings::application.windows[index_].fullscreen ) {
        GLFWmonitor *mo = Rendering::manager().monitorNamed(Settings::application.windows[index_].monitor);
        setFullscreen_(mo);
    }
}


void RenderingWindow::makeCurrent()
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

bool RenderingWindow::draw(FrameBuffer *fb)
{
    // cannot draw if there is no window or invalid framebuffer
    if (!window_ || !fb)
        return false;

    // only draw if window is not iconified
    if( !glfwGetWindowAttrib(window_, GLFW_ICONIFIED ) ) {

        // update viewport (could be done with callback)
        glfwGetFramebufferSize(window_, &(window_attributes_.viewport.x), &(window_attributes_.viewport.y));

        // take context ownership
        glfwMakeContextCurrent(window_);

        // setup attribs
        Rendering::manager().pushAttrib(window_attributes_);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // make sure previous shader in another glcontext is disabled
        ShadingProgram::enduse();

        // draw geometry
        if (Settings::application.render.disabled)
            // no draw; indicate texture is black
            textureid_ = Resource::getTextureBlack();
        else {
            // normal draw

            // VAO is not shared between multiple contexts of different windows
            // so we have to create a new VAO for rendering the surface in this window
            if (surface_ == nullptr) {
                // create shader that performs white balance correction
                shader_ = new ImageFilteringShader;
                shader_->setCode( whitebalance.code().first );
                // create surface using the shader
                surface_ = new WindowSurface(shader_);
            }
            // update values of the shader
            if (shader_) {
                shader_->uniforms_["Red"] = Settings::application.windows[index_].whitebalance.x;
                shader_->uniforms_["Green"] = Settings::application.windows[index_].whitebalance.y;
                shader_->uniforms_["Blue"] = Settings::application.windows[index_].whitebalance.z;
                shader_->uniforms_["Temperature"] = Settings::application.windows[index_].whitebalance.w;
                shader_->uniforms_["Contrast"] = Settings::application.windows[index_].contrast;
                shader_->uniforms_["Brightness"] = Settings::application.windows[index_].brightness;

                if (Settings::application.windows[index_].custom)
                    shader_->iNodes = Settings::application.windows[index_].nodes;
                else
                    shader_->iNodes = glm::zero<glm::mat4>();
            }

            // Display option: scaled or corrected aspect ratio
            if (Settings::application.windows[index_].custom) {
//                surface_->scale_ = Settings::application.windows[index_].scale;
//                surface_->translation_ = Settings::application.windows[index_].translation;

                surface_->scale_ = glm::vec3(1.f);
                surface_->translation_ = glm::vec3(0.f);
            }
            else{
                // calculate scaling factor of frame buffer inside window
                const float windowAspectRatio = aspectRatio();
                const float renderingAspectRatio = fb->aspectRatio();
                if (windowAspectRatio < renderingAspectRatio)
                    surface_->scale_ = glm::vec3(1.f, windowAspectRatio / renderingAspectRatio, 1.f);
                else
                    surface_->scale_ = glm::vec3(renderingAspectRatio / windowAspectRatio, 1.f, 1.f);
                surface_->translation_ = glm::vec3(0.f);
            }

            // Display option: draw calibration pattern
            if (Settings::application.windows[index_].show_pattern) {
                // (re) create pattern at frame buffer resolution
                if ( pattern_->width() != fb->width() || pattern_->height() != fb->height()) {
                    if (GstToolkit::has_feature("frei0r-src-test-pat-b") )
                        pattern_->open("frei0r-src-test-pat-b type=0.7", fb->width(), fb->height());
                    else {
                        pattern_->open("videotestsrc pattern=smpte", fb->width(), fb->height());
                        pattern_->play(true);
                    }
                }
                // draw pattern texture
                pattern_->update();
                textureid_ = pattern_->texture();
            }
            else
                // draw normal texture
                textureid_ = fb->texture();

            // actual render of the textured surface
            static glm::mat4 projection = glm::ortho(-1.f, 1.f, -1.f, 1.f, -1.f, 1.f);
            surface_->setTextureIndex(textureid_);
            surface_->update(0.f);
            surface_->draw(glm::identity<glm::mat4>(), projection);

            // done drawing (unload shader from this glcontext)
            ShadingProgram::enduse();
        }

        // restore attribs
        Rendering::manager().popAttrib();

        glfwSwapBuffers(window_);

    }

    return true;
}

