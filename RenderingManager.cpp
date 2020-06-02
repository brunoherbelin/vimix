#include <cstring>
#include <thread>
#include <mutex>
#include <chrono>

// Desktop OpenGL function loader
#include <glad/glad.h>  // Initialized with gladLoadGLLoader()

// Include glfw3.h after our OpenGL definitions
#define GLFW_INCLUDE_GLEXT
#include <GLFW/glfw3.h>

#ifdef APPLE
#include "osx/CocoaToolkit.h"
#define GLFW_EXPOSE_NATIVE_COCOA
#define GLFW_EXPOSE_NATIVE_NSGL
#else
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_GLX
#endif
#include <GLFW/glfw3native.h>

#include <glm/gtc/matrix_transform.hpp> // glm::translate, glm::rotate, glm::scale
#include <glm/ext/matrix_projection.hpp> // glm::unproject
#include <glm/ext/matrix_clip_space.hpp> // glm::perspective

// Include GStreamer
#include <gst/gl/gl.h>
#include <gst/gl/gstglcontext.h>

#ifdef GLFW_EXPOSE_NATIVE_COCOA
#include <gst/gl/cocoa/gstgldisplay_cocoa.h>
#endif
#ifdef GLFW_EXPOSE_NATIVE_GLX
#include <gst/gl/x11/gstgldisplay_x11.h>
#endif

// standalone image loader
#include <stb_image.h>

// vmix
#include "defines.h"
#include "Log.h"
#include "Resource.h"
#include "Settings.h"
#include "Primitives.h"
#include "Mixer.h"
#include "SystemToolkit.h"
#include "UserInterfaceManager.h"
#include "RenderingManager.h"

// local statics
static GstGLContext *global_gl_context = NULL;
static GstGLDisplay *global_display = NULL;

static std::map<GLFWwindow *, RenderingWindow*> GLFW_window_;

static void glfw_error_callback(int error, const char* description)
{
    Log::Error("Glfw Error %d: %s",  error, description);
}

static void WindowRefreshCallback( GLFWwindow *w )
{
    Rendering::manager().draw();
}

static void WindowResizeCallback( GLFWwindow *w, int width, int height)
{
    int id = GLFW_window_[w]->id();
    if (!Settings::application.windows[id].fullscreen) {
        Settings::application.windows[id].w = width;
        Settings::application.windows[id].h = height;
    }
}

static void WindowMoveCallback( GLFWwindow *w, int x, int y)
{
    int id = GLFW_window_[w]->id();
    if (!Settings::application.windows[id].fullscreen) {
        Settings::application.windows[id].x = x;
        Settings::application.windows[id].y = y;
    }
}

static void WindowEscapeFullscreen( GLFWwindow *w, int key, int scancode, int action, int)
{
    if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE)
    {
        // escape fullscreen
        GLFW_window_[w]->setFullscreen(nullptr);
    }
}

static void WindowToggleFullscreen( GLFWwindow *w, int button, int action, int)
{
    static double seconds = 0.f;

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        // detect double clic
        if ( glfwGetTime() - seconds < 0.2f ) {
            // toggle fullscreen
            GLFW_window_[w]->toggleFullscreen();
        }
        // for next clic detection
        seconds = glfwGetTime();
    }
}

Rendering::Rendering()
{
//    main_window_ = nullptr;
    request_screenshot_ = false;
}

bool Rendering::init()
{
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()){
        Log::Error("Failed to Initialize GLFW.");
        return false;
    }

    // Decide GL+GLSL versions GL 3.3 + GLSL 150
    glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
#if __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#endif

    //
    // OpenGL Multisampling main window
    //
    glfwWindowHint(GLFW_SAMPLES, Settings::application.multisampling_level);
    main_.init(0);
    // set application icon
    main_.setIcon("images/vimix_256x256.png");
    // additional window callbacks for main window
    glfwSetWindowRefreshCallback( main_.window(), WindowRefreshCallback );
    glfwSetDropCallback( main_.window(), Rendering::FileDropped);

    //
    // Gstreamer setup for OpenGL
    //
    g_setenv ("GST_GL_API", "opengl3", FALSE);
    gst_init (NULL, NULL);
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

    //
    // output window
    //
    glfwWindowHint(GLFW_SAMPLES, 0); // no need for multisampling in displaying output
    output_.init(1, main_.window());
    // set special icon
    output_.setIcon("images/vimix_square_256x256.png");
    // special callbacks for user input in output window
    glfwSetKeyCallback( output_.window(), WindowEscapeFullscreen);
    glfwSetMouseButtonCallback( output_.window(), WindowToggleFullscreen);

    return true;
}


void Rendering::show()
{
    // show output window
    output_.show();

    // show main window
    main_.show();

    // show menu on first show
    UserInterface::manager().showPannel();
}

bool Rendering::isActive()
{
    return !glfwWindowShouldClose(main_.window());
}


void Rendering::pushFrontDrawCallback(RenderingCallback function)
{
    draw_callbacks_.push_front(function);
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

    main_.makeCurrent();

    UserInterface::manager().NewFrame();

    std::list<Rendering::RenderingCallback>::iterator iter;
    for (iter=draw_callbacks_.begin(); iter != draw_callbacks_.end(); iter++)
    {
        (*iter)();
    }

    UserInterface::manager().Render();

    // perform screenshot if requested
    if (request_screenshot_) {
        // glfwMakeContextCurrent(main_window_);
        screenshot_.CreateFromCaptureGL(0, 0, main_.width(), main_.height());
        request_screenshot_ = false;
    }

    // swap GL buffers
    glfwSwapBuffers(main_.window());

    // no g_main_loop_run(loop) : update global GMainContext
    g_main_context_iteration(NULL, FALSE);

    // draw output window
    output_.draw( Mixer::manager().session()->frame() );
}


void Rendering::terminate()
{
    // close window
    glfwDestroyWindow(output_.window());
    glfwDestroyWindow(main_.window());
    glfwTerminate();
}


void Rendering::close()
{
    glfwSetWindowShouldClose(main_.window(), true);
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

void Rendering::FileDropped(GLFWwindow *, int path_count, const char* paths[])
{
    for (int i = 0; i < path_count; ++i) {
        std::string filename(paths[i]);
        if (filename.empty())
            break;
        // try to create a source
        Mixer::manager().insertSource ( Mixer::manager().createSourceFile( filename ) );
    }
    UserInterface::manager().showPannel();
}


Screenshot *Rendering::currentScreenshot()
{
    return &screenshot_;
}

void Rendering::requestScreenshot()
{ 
    screenshot_.Clear();
    request_screenshot_ = true;
}

RenderingWindow::RenderingWindow() : window_(nullptr), master_(nullptr), id_(-1), dpi_scale_(1.f)
{
}

RenderingWindow::~RenderingWindow()
{

}

void RenderingWindow::setTitle(const std::string &title)
{
    std::string fulltitle = Settings::application.windows[id_].name;
    if ( !title.empty() )
        fulltitle += " -- " + title;

    glfwSetWindowTitle(window_, fulltitle.c_str());
}

void RenderingWindow::setIcon(const std::string &resource)
{
    size_t fpsize = 0;
    const char *fp = Resource::getData(resource, &fpsize);
    if (fp != nullptr) {
        GLFWimage icon;
        icon.pixels = stbi_load_from_memory( (const stbi_uc*)fp, fpsize, &icon.width, &icon.height, nullptr, 4 );
        glfwSetWindowIcon( window_, 1, &icon );
        free( icon.pixels );
    }
}

bool RenderingWindow::isFullscreen ()
{
    return (glfwGetWindowMonitor(window_) != nullptr);
//    return Settings::application.windows[id_].fullscreen;
}

GLFWmonitor *RenderingWindow::monitorAt(int x, int y)
{
    // default to primary monitor
    GLFWmonitor *mo = glfwGetPrimaryMonitor();

    // list all monitors
    int count_monitors = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count_monitors);
    // if there is more than one monitor
    if (count_monitors > 1) {
        // pick at the coordinates given or at pos of window
        // try every monitor
        int i = 0;
        for (; i < count_monitors;  i++) {
            int workarea_x, workarea_y, workarea_width, workarea_height;
            glfwGetMonitorWorkarea(monitors[i], &workarea_x, &workarea_y, &workarea_width, &workarea_height);
            if ( x >= workarea_x && x <= workarea_x + workarea_width &&
                 y >= workarea_y && y <= workarea_y + workarea_height)
                break;
        }
        // found the monitor containing this point !
        if ( i < count_monitors)
            mo = monitors[i];
    }

    return mo;
}

GLFWmonitor *RenderingWindow::monitorNamed(const std::string &name)
{
    // default to primary monitor
    GLFWmonitor *mo = glfwGetPrimaryMonitor();

    // list all monitors
    int count_monitors = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count_monitors);
    // if there is more than one monitor
    if (count_monitors > 1) {
        // pick at the coordinates given or at pos of window
        // try every monitor
        int i = 0;
        for (; i < count_monitors;  i++) {
            if ( std::string( glfwGetMonitorName(monitors[i])) == name )
                break;
        }
        // found the monitor with this name
        if ( i < count_monitors)
            mo = monitors[i];
    }

    return mo;
}

GLFWmonitor *RenderingWindow::monitor()
{
    // pick at the coordinates given or at pos of window
    int x, y;
    glfwGetWindowPos(window_, &x, &y);

    return monitorAt(x, y);
}

void RenderingWindow::setFullscreen(GLFWmonitor *mo)
{
    // if in fullscreen mode
    if (mo == nullptr) {
        // set to window mode
        glfwSetInputMode( window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        glfwSetWindowMonitor( window_, nullptr, Settings::application.windows[id_].x,
                                                Settings::application.windows[id_].y,
                                                Settings::application.windows[id_].w,
                                                Settings::application.windows[id_].h, 0 );
        Settings::application.windows[id_].fullscreen = false;
    }
    // not in fullscreen mode
    else {
        // set to fullscreen mode
        Settings::application.windows[id_].fullscreen = true;
        Settings::application.windows[id_].monitor = glfwGetMonitorName(mo);

        const GLFWvidmode * mode = glfwGetVideoMode(mo);
        glfwSetWindowMonitor( window_, mo, 0, 0, mode->width, mode->height, mode->refreshRate);
        glfwSetInputMode( window_, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    }
}

void RenderingWindow::toggleFullscreen()
{
    // if in fullscreen mode
    if (isFullscreen()) {
        // exit fullscreen
        setFullscreen(nullptr);
    }
    // not in fullscreen mode
    else {
        // enter fullscreen in monitor where the window is
        setFullscreen(monitor());
    }
}

int RenderingWindow::width()
{
    return window_attributes_.viewport.x;
}

int RenderingWindow::height()
{
     return window_attributes_.viewport.y;
}

int RenderingWindow::maxHeight()
{
    int workarea_x, workarea_y, workarea_width, workarea_height;
    glfwGetMonitorWorkarea(monitor(), &workarea_x, &workarea_y, &workarea_width, &workarea_height);

    return workarea_height * dpi_scale_;
}

float RenderingWindow::aspectRatio()
{
    return static_cast<float>(window_attributes_.viewport.x) / static_cast<float>(window_attributes_.viewport.y);
}

bool RenderingWindow::init(int id, GLFWwindow *share)
{
    id_ = id;
    master_ = share;

    // access Settings
    Settings::WindowConfig winset = Settings::application.windows[id_];

    // do not show at creation
    glfwWindowHint(GLFW_FOCUSED, GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);

    // create the window normal
    window_ = glfwCreateWindow(winset.w, winset.h, winset.name.c_str(), NULL, master_);
    if (window_ == NULL){
        Log::Error("Failed to create GLFW Window %d", id_);
        return false;
    }

    // store global ref to pointers (used by callbacks)
    GLFW_window_[window_] = this;

    // set position
    glfwSetWindowPos(window_, winset.x, winset.y);
    // window position and resize callbacks
    glfwSetWindowSizeCallback( window_, WindowResizeCallback );
//    glfwSetFramebufferSizeCallback( window_, WindowResizeCallback );
    glfwSetWindowPosCallback( window_, WindowMoveCallback );

    // take opengl context ownership
    glfwMakeContextCurrent(window_);

    static bool glad_initialized = false;
    if ( !glad_initialized ) {
        //
        // Initialize OpenGL loader
        //
        bool err = gladLoadGLLoader((GLADloadproc) glfwGetProcAddress) == 0;
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

    // This hint can improve the speed of texturing when perspective-correct texture coordinate interpolation isn't needed
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
    // This hint can improve the speed of shading when dFdx dFdy aren't needed in GLSL
    glHint(GL_FRAGMENT_SHADER_DERIVATIVE_HINT, GL_FASTEST);

    // if not main window
    if ( master_ != NULL ) {
        // Disable vsync
        glfwSwapInterval(0);
        // no need for multisampling
        glDisable(GL_MULTISAMPLE);
        // clear to black
        window_attributes_.clear_color = glm::vec4(0.f, 0.f, 0.f, 1.0);
        // give back context ownership
        glfwMakeContextCurrent(master_);
    }
    else {
        // Enable vsync on main window
        glfwSwapInterval(1);
        // enable Antialiasing multisampling
        if (Settings::application.multisampling_level > 0) {
            glEnable(GL_MULTISAMPLE);
            glHint(GL_MULTISAMPLE_FILTER_HINT_NV, GL_NICEST);
        }
        // clear to grey
        window_attributes_.clear_color = glm::vec4(COLOR_BGROUND, 1.0);
    }

    return true;
}

void RenderingWindow::show()
{
    glfwShowWindow(window_);

    if ( Settings::application.windows[id_].fullscreen ) {
        GLFWmonitor *mo = monitorNamed(Settings::application.windows[id_].monitor);
        setFullscreen(mo);
    }

}

// custom surface with a new VAO
class WindowSurface : public Primitive {

public:
    WindowSurface(Shader *s = new ImageShader);
};

WindowSurface::WindowSurface(Shader *s) : Primitive(s)
{
    points_ = std::vector<glm::vec3> { glm::vec3( -1.f, -1.f, 0.f ), glm::vec3( -1.f, 1.f, 0.f ),
            glm::vec3( 1.f, -1.f, 0.f ), glm::vec3( 1.f, 1.f, 0.f ) };
    colors_ = std::vector<glm::vec4> { glm::vec4( 1.f, 1.f, 1.f , 1.f ), glm::vec4(  1.f, 1.f, 1.f, 1.f  ),
            glm::vec4( 1.f, 1.f, 1.f, 1.f ), glm::vec4( 1.f, 1.f, 1.f, 1.f ) };
    texCoords_ = std::vector<glm::vec2> { glm::vec2( 0.f, 1.f ), glm::vec2( 0.f, 0.f ),
            glm::vec2( 1.f, 1.f ), glm::vec2( 1.f, 0.f ) };
    indices_ = std::vector<uint> { 0, 1, 2, 3 };
    drawMode_ = GL_TRIANGLE_STRIP;
}


void RenderingWindow::makeCurrent()
{
    // handle window resize
    glfwGetFramebufferSize(window_, &(window_attributes_.viewport.x), &(window_attributes_.viewport.y));

    // ensure main context is current
    glfwMakeContextCurrent(window_);

    // set and clear
    glViewport(0, 0, window_attributes_.viewport.x, window_attributes_.viewport.y);
    glClearColor(window_attributes_.clear_color.r, window_attributes_.clear_color.g,
                 window_attributes_.clear_color.b, window_attributes_.clear_color.a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void RenderingWindow::draw(FrameBuffer *fb)
{
    if (!window_)
        return;

    // only draw if window is not iconified
    if( !glfwGetWindowAttrib(window_, GLFW_ICONIFIED ) ) {

        // update viewport (could be done with callback)
        glfwGetFramebufferSize(window_, &(window_attributes_.viewport.x), &(window_attributes_.viewport.y));

        // take context ownership
        glfwMakeContextCurrent(window_);

        // setup attribs
        Rendering::manager().pushAttrib(window_attributes_);
        glClear(GL_COLOR_BUFFER_BIT);
        static glm::mat4 projection = glm::ortho(-1.f, 1.f, -1.f, 1.f, -1.f, 1.f);

        // VAO is not shared between multiple contexts of different windows
        // so we have to create a new VAO for rendering the surface in this window
        static WindowSurface *surface  = new WindowSurface;

        // draw frame buffer provided
        if (fb) {

            // calculate scaling factor of frame buffer inside window
            float windowAspectRatio = aspectRatio();
            float renderingAspectRatio = fb->aspectRatio();
            glm::vec3 scale;
            if (windowAspectRatio < renderingAspectRatio)
                scale = glm::vec3(1.f, windowAspectRatio / renderingAspectRatio, 1.f);
            else
                scale = glm::vec3(renderingAspectRatio / windowAspectRatio, 1.f, 1.f);

            // draw
            glBindTexture(GL_TEXTURE_2D, fb->texture());
//            surface->shader()->color.a = 0.4f; // TODO alpha blending ?
            surface->draw(glm::scale(glm::identity<glm::mat4>(), scale), projection);

            // done drawing
            ShadingProgram::enduse();
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        // restore attribs
        Rendering::manager().popAttrib();

        // swap buffer
        glfwSwapBuffers(window_);
    }

    // give back context ownership
    glfwMakeContextCurrent(master_);
}

//
// Discarded because not working under OSX - kept in case it would become useful
//
// Linking pipeline to the rendering instance ensures the opengl contexts
// created by gstreamer inside plugins (e.g. glsinkbin) is the same
//

static GstBusSyncReply
bus_sync_handler (GstBus *, GstMessage * msg, gpointer )
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


    // GstBus*  m_bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    // gst_bus_enable_sync_message_emission (m_bus);
    // g_signal_connect (m_bus, "sync-message", G_CALLBACK (bus_sync_handler), pipeline);
    // gst_object_unref (m_bus);
}
