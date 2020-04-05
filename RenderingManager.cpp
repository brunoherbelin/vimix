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

#include <glm/ext/matrix_transform.hpp> // glm::translate, glm::rotate, glm::scale
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
#include "UserInterfaceManager.h"
#include "RenderingManager.h"

// local statics
static GstGLContext *global_gl_context = NULL;
static GstGLDisplay *global_display = NULL;
static guintptr global_window_handle = 0;

static void glfw_error_callback(int error, const char* description)
{
    Log::Error("Glfw Error %d: %s",  error, description);
}

static void WindowRefreshCallback( GLFWwindow* window )
{
    Rendering::manager().Draw();
}

Rendering::Rendering()
{
    main_window_ = nullptr;
    request_screenshot_ = false;
}

bool Rendering::Init()
{
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()){
        Log::Error("Failed to Initialize GLFW.");
        return false;
    }

    // Decide GL+GLSL versions GL 3.2 + GLSL 150
    glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
#if __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#endif

    Settings::Window winset = Settings::application.windows.front();

    // Create window with graphics context
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    main_window_ = glfwCreateWindow(winset.w, winset.h, winset.name.c_str(), NULL, NULL);
    if (main_window_ == NULL){
        Log::Error("Failed to Create GLFW Window.");
        return false;
    }

    // Add application icon
    size_t fpsize = 0;
    const char *fp = Resource::getData("images/v-mix_256x256.png", &fpsize);
    if (fp != nullptr) {
        GLFWimage icon;
        icon.pixels = stbi_load_from_memory( (const stbi_uc*)fp, fpsize, &icon.width, &icon.height, nullptr, 4 );
        glfwSetWindowIcon( main_window_, 1, &icon );
        free( icon.pixels );
    }

    glfwSetWindowPos(main_window_, winset.x, winset.y);
    glfwMakeContextCurrent(main_window_);
    glfwSwapInterval(1); // Enable vsync3
    glfwSetWindowRefreshCallback( main_window_, WindowRefreshCallback );

    // Initialize OpenGL loader
    bool err = gladLoadGLLoader((GLADloadproc) glfwGetProcAddress) == 0;
    if (err) {
        Log::Error("Failed to initialize GLAD OpenGL loader.");
        return false;
    }

    // show window
    glfwShowWindow(main_window_);
    // restore fullscreen
    if (winset.fullscreen)
        ToggleFullscreen();

    // Rendering area (here same as window)
    glfwGetFramebufferSize(main_window_, &(main_window_attributes_.viewport.x), &(main_window_attributes_.viewport.y));
    glViewport(0, 0, main_window_attributes_.viewport.x, main_window_attributes_.viewport.y);
    main_window_attributes_.clear_color = glm::vec3(COLOR_BGROUND);

    // Gstreamer link to context
    g_setenv ("GST_GL_API", "opengl3", FALSE);
    gst_init (NULL, NULL);

#if GST_GL_HAVE_PLATFORM_WGL
    global_gl_context = gst_gl_context_new_wrapped (display, (guintptr) wglGetCurrentContext (),
                                                    GST_GL_PLATFORM_WGL, GST_GL_API_OPENGL);

#elif GST_GL_HAVE_PLATFORM_CGL


//    global_display = GST_GL_DISPLAY ( glfwGetCocoaMonitor(window) );
//    global_display = GST_GL_DISPLAY (gst_gl_display_cocoa_new ());

//    global_gl_context = gst_gl_context_new_wrapped (global_display,
//                                         (guintptr) 0,
//                                         GST_GL_PLATFORM_CGL, GST_GL_API_OPENGL);


#elif GST_GL_HAVE_PLATFORM_GLX

    //
    global_display = (GstGLDisplay*) gst_gl_display_x11_new_with_display( glfwGetX11Display() );

    global_gl_context = gst_gl_context_new_wrapped (global_display,
                                        (guintptr) glfwGetGLXContext(main_window_),
                                        GST_GL_PLATFORM_GLX, GST_GL_API_OPENGL);

    global_window_handle =  (guintptr) glfwGetX11Window(main_window_);

#endif

    // TODO : force GPU decoding

    // GstElementFactory *vdpauh264dec = gst_element_factory_find("vdpauh264dec");
    // if (vdpauh264dec)
    //     gst_plugin_feature_set_rank (GST_PLUGIN_FEATURE(vdpauh264dec), GST_RANK_PRIMARY); // or GST_RANK_MARGINAL
    // GstElementFactory *vdpaumpeg4dec = gst_element_factory_find("vdpaumpeg4dec");
    // if (vdpaumpeg4dec)
    //     gst_plugin_feature_set_rank (GST_PLUGIN_FEATURE(vdpaumpeg4dec), GST_RANK_PRIMARY);
    // GstElementFactory *vdpaumpegdec = gst_element_factory_find("vdpaumpegdec");
    // if (vdpaumpegdec)
    //     gst_plugin_feature_set_rank (GST_PLUGIN_FEATURE(vdpaumpegdec), GST_RANK_PRIMARY);

    // file drop callback
    glfwSetDropCallback(main_window_, Rendering::FileDropped);

    return true;
}

bool Rendering::isActive()
{
    return !glfwWindowShouldClose(main_window_);
}


void Rendering::PushFrontDrawCallback(RenderingCallback function)
{
    draw_callbacks_.push_front(function);
}

void Rendering::PushBackDrawCallback(RenderingCallback function)
{
    draw_callbacks_.push_back(function);
}

void Rendering::Draw()
{
    if ( Begin() )
    {
        UserInterface::manager().NewFrame();

		std::list<Rendering::RenderingCallback>::iterator iter;
        for (iter=draw_callbacks_.begin(); iter != draw_callbacks_.end(); iter++)
		{
            (*iter)();
        }

        UserInterface::manager().Render();
        End();
    }

    // no g_main_loop_run(loop) : update global GMainContext
    g_main_context_iteration(NULL, FALSE);

}

bool Rendering::Begin()
{
    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
    // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
    glfwPollEvents();

    glfwMakeContextCurrent(main_window_);
    if( glfwGetWindowAttrib( main_window_, GLFW_ICONIFIED ) )
    {
        std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
        return false;
    }

    // handle window resize
    glfwGetFramebufferSize(main_window_, &(main_window_attributes_.viewport.x), &(main_window_attributes_.viewport.y));
    glViewport(0, 0, main_window_attributes_.viewport.x, main_window_attributes_.viewport.y);

    // GL Colors
    glClearColor(main_window_attributes_.clear_color.r, main_window_attributes_.clear_color.g,
                 main_window_attributes_.clear_color.b, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    return true;
}

void Rendering::End()
{
    glfwMakeContextCurrent(main_window_);

    // perform screenshot if requested
    if (request_screenshot_) {
        screenshot_.CreateFromCaptureGL(0, 0, main_window_attributes_.viewport.x, main_window_attributes_.viewport.y);
        request_screenshot_ = false;
    }

    // swap GL buffers
    glfwSwapBuffers(main_window_);
}


void Rendering::Terminate()
{
    // settings
    if ( !Settings::application.windows.front().fullscreen) {
        int x, y;
        glfwGetWindowPos(main_window_, &x, &y);
        Settings::application.windows.front().x = x;
        Settings::application.windows.front().y = y;
        glfwGetWindowSize(main_window_,&x, &y);
        Settings::application.windows.front().w = x;
        Settings::application.windows.front().h = y;
    }

    // close window
    glfwDestroyWindow(main_window_);
    glfwTerminate();
}


void Rendering::Close()
{
    glfwSetWindowShouldClose(main_window_, true);
}


void Rendering::PushAttrib(RenderingAttrib ra)
{
    // push it to top of pile
    draw_attributes_.push_front(ra);

    // apply Changes to OpenGL
    glViewport(0, 0, ra.viewport.x, ra.viewport.y);
    glClearColor(ra.clear_color.r, ra.clear_color.g, ra.clear_color.b, 1.f);
}

void Rendering::PopAttrib()
{
    // pops the top of the pile
    if (draw_attributes_.size() > 0)
        draw_attributes_.pop_front();

    // set attribute element to default
    RenderingAttrib ra = main_window_attributes_;
    // if there is an element at top, use it
    if (draw_attributes_.size() > 0)
        ra = draw_attributes_.front();

    // apply Changes to OpenGL
    glViewport(0, 0, ra.viewport.x, ra.viewport.y);
    glClearColor(ra.clear_color.r, ra.clear_color.g, ra.clear_color.b, 1.f);
}


glm::mat4 Rendering::Projection()
{
    glm::mat4 projection = glm::ortho(-5.0, 5.0, -5.0, 5.0);
    glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(1.f, AspectRatio(), 1.f));

    return projection * scale;
}

float Rendering::Width() { return main_window_attributes_.viewport.x; }
float Rendering::Height() { return main_window_attributes_.viewport.y; }

void Rendering::ToggleFullscreen()
{
    // if in fullscreen mode
    if (glfwGetWindowMonitor(main_window_) != nullptr) {
        // set to window mode
        glfwSetWindowMonitor( main_window_, nullptr,  Settings::application.windows.front().x,
                                                Settings::application.windows.front().y,
                                                Settings::application.windows.front().w,
                                                Settings::application.windows.front().h, 0 );
        Settings::application.windows.front().fullscreen = false;
    }
    // not in fullscreen mode
    else {
        // remember window geometry
        int x, y;
        glfwGetWindowPos(main_window_, &x, &y);
        Settings::application.windows.front().x = x;
        Settings::application.windows.front().y = y;
        glfwGetWindowSize(main_window_,&x, &y);
        Settings::application.windows.front().w = x;
        Settings::application.windows.front().h = y;

        // select monitor
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode * mode = glfwGetVideoMode(monitor);

        // set to fullscreen mode
        glfwSetWindowMonitor( main_window_, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        Settings::application.windows.front().fullscreen = true;
    }

}

float Rendering::AspectRatio()
{
    return static_cast<float>(main_window_attributes_.viewport.x) / static_cast<float>(main_window_attributes_.viewport.y);
}

void Rendering::FileDropped(GLFWwindow* window, int path_count, const char* paths[])
{
    for (int i = 0; i < path_count; ++i) {

        Log::Info("Dropped file %s\n", paths[i]);
    }
}


Screenshot *Rendering::CurrentScreenshot()
{
    return &screenshot_;
}

void Rendering::RequestScreenshot() 
{ 
    screenshot_.Clear();
    request_screenshot_ = true;
}


//
// Discarded because not working under OSX - kept in case it would become useful
//
// Linking pipeline to the rendering instance ensures the opengl contexts
// created by gstreamer inside plugins (e.g. glsinkbin) is the same
//

static GstBusSyncReply
bus_sync_handler (GstBus * bus, GstMessage * msg, gpointer user_data)
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
