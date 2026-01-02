#ifndef __RENDERING_MANAGER_H_
#define __RENDERING_MANAGER_H_

#include <string>
#include <list>
#include <map>

#include <gst/gl/gl.h>
#include <glm/glm.hpp> 

#include "MainWindow.h"
#include "OutputWindow.h"
#include "Screenshot.h"

typedef struct GLFWmonitor GLFWmonitor;


class Rendering
{
    friend class UserInterface;
    friend class MainWindow;

    // Private Constructor
    Rendering();
    Rendering(Rendering const& copy) = delete;
    Rendering& operator=(Rendering const& copy) = delete;

public:

    struct Monitor
    {
        GLFWmonitor* monitor;
        std::string name;
        glm::ivec4 geometry;
        OutputWindow output;

        Monitor() : monitor(nullptr), name(""), geometry(0, 0, 0, 0) {}
        Monitor(GLFWmonitor* m, const std::string& n, const glm::ivec4& g)
            : monitor(m), name(n), geometry(g) {}
    };

    static Rendering& manager()
    {
        // The only instance
        static Rendering _instance;
        return _instance;
    }

    // Initialization OpenGL and GLFW window creation
    bool init();

    // show outputs and main window
    void show(bool show_main_window = true);

    // true if active rendering window
    bool isActive();

    // draw one frame
    void draw();

    // request close of the UI (Quit the program)
    void close();

    // Post-loop termination
    void terminate();

    // add function to call during draw
    typedef void (* RenderingCallback)(void);
    void pushBackDrawCallback(RenderingCallback function);

    // push and pop rendering attributes
    void pushAttrib(RenderingAttrib ra);
    void popAttrib();
    RenderingAttrib currentAttrib();

    // get projection matrix (for sharers) => Views
    glm::mat4 Projection();
    // unproject from window coordinate to scene
    glm::vec3 unProject(glm::vec2 screen_coordinate, glm::mat4 modelview = glm::mat4(1.f));
    // project from scene coordinate to window
    glm::vec2 project(glm::vec3 scene_coordinate, glm::mat4 modelview = glm::mat4(1.f), bool to_framebuffer = true);

    // monitors management
    inline std::list<Monitor>& monitors() { return monitors_; }
    // get which monitor has this name, main monitor if not found
    GLFWmonitor *monitorNamed(const std::string &name);
    // get which monitor contains this point, main monitor if not found
    GLFWmonitor *monitorAt(int x, int y);
    // draw all output windows
    void drawOutputWindows();
    bool isAnyOutputActive();

    // Application main window management
    inline MainWindow& mainWindow() { return main_; }
    
    // request screenshot
    void requestScreenshot();
    // get Screenshot
    class Screenshot *currentScreenshot();

    // GPU management
    static bool openGLExtensionAvailable(const char *extensionname);
    static glm::ivec2 getGPUMemoryInformation();
    static bool shouldHaveEnoughMemory(glm::vec3 resolution, int flags, int num_buffers = 1);


#ifdef USE_GST_OPENGL_SYNC_HANDLER
    // for opengl pipeline in gstreamer    
    GstGLContext *global_gl_context;
    GstGLDisplay *global_display;
#endif

protected:
    // true if wayland display (linux)
    bool wayland_;

    // callback when monitor is connected/disconnected
    static void MonitorConnect(GLFWmonitor* monitor, int event);

    // inhibit system screensaver
    static void inhibitScreensaver (bool on);

private:

    // list of rendering attributes
    std::list<RenderingAttrib> draw_attributes_;

    // list of functions to call at each Draw
    std::list<RenderingCallback> draw_callbacks_;

    // main window
    MainWindow main_;

    // monitors
    std::list<Monitor> monitors_;

    Screenshot screenshot_;
    bool request_screenshot_;

};


#endif /* #define __RENDERING_MANAGER_H_ */
