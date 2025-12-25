#ifndef __RENDERING_MANAGER_H_
#define __RENDERING_MANAGER_H_

#include <string>
#include <list>
#include <map>
#include <vector>

#include <gst/gl/gl.h>
#include <glm/glm.hpp> 

#include "Screenshot.h"

typedef struct GLFWmonitor GLFWmonitor;
typedef struct GLFWwindow GLFWwindow;
class FrameBuffer;
class Stream;

struct RenderingAttrib
{
    RenderingAttrib() {}
    glm::ivec2 viewport;
    glm::vec4 clear_color;
};

class RenderingWindow
{
    friend class Rendering;

    GLFWwindow *window_, *master_;
    RenderingAttrib window_attributes_;
    std::string title_changed_;
    int index_;
    float dpi_scale_;

    // objects to render
    uint textureid_;
    uint fbo_;
    static Stream *pattern_;
    class WindowSurface *surface_;
    class ImageFilteringShader *shader_;

protected:
    void setTitle(const std::string &title = "");
    void setIcon(const std::string &resource);
    bool request_change_fullscreen_;
    void changeFullscreen_ ();
    void setFullscreen_(GLFWmonitor *mo);

public:
    RenderingWindow();
    ~RenderingWindow();

    inline int index() const { return index_; }
    inline RenderingAttrib& attribs() { return window_attributes_; }
    inline GLFWwindow *window() const { return window_; }

    bool init(int index, GLFWwindow *share = NULL);
    void terminate();

    // show window (fullscreen if needed)
    void show();

    // make context current and set viewport
    void makeCurrent();

    // draw a framebuffer
    bool draw(FrameBuffer *fb);
    inline uint texture() const {return textureid_; }

    // fullscreen
    bool isFullscreen ();
    void exitFullscreen ();
    void setFullscreen (std::string monitorname);
    void toggleFullscreen ();
    // get monitor in which the window is
    GLFWmonitor *monitor();

    // set geometry, color correction and decoration
    void setCoordinates  (glm::ivec4 rect);
    void setDecoration   (bool on);

    // get width of rendering area
    int width();
    // get height of rendering area
    int height();
    // get aspect ratio of rendering area
    float aspectRatio();
    // high dpi monitor scaling
    inline float dpiScale() const { return dpi_scale_; }
    // get number of pixels to render X milimeters in height
    int pixelsforRealHeight(float milimeters);

    glm::vec2 previous_size;
};

class Rendering
{
    friend class UserInterface;
    friend class RenderingWindow;

    // Private Constructor
    Rendering();
    Rendering(Rendering const& copy) = delete;
    Rendering& operator=(Rendering const& copy) = delete;

public:

    static Rendering& manager()
    {
        // The only instance
        static Rendering _instance;
        return _instance;
    }

    // Initialization OpenGL and GLFW window creation
    bool init();
    // show windows and reset views
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

    // Application main window management
    inline RenderingWindow& mainWindow() { return main_; }
    inline void setMainWindowTitle(const std::string t) { main_new_title_ = t; }
    // request screenshot
    void requestScreenshot();
    // get Screenshot
    class Screenshot *currentScreenshot();

    // Rendering output windows management
    inline RenderingWindow& outputWindow(size_t i) { return outputs_[i]; }

    // windows access (cannot fail; defaults to main window on invalid argument)
    RenderingWindow* window(GLFWwindow *w);
    RenderingWindow* window(int index);

    // get hold on the monitors
    inline std::map<std::string, glm::ivec4> monitors() { return monitors_geometry_; }
    // get which monitor contains this point
    GLFWmonitor *monitorAt(int x, int y);
    std::string monitorNameAt(int x, int y);
    // get which monitor has this name
    GLFWmonitor *monitorNamed(const std::string &name);

    // memory management
    static glm::ivec2 getGPUMemoryInformation();
    static bool shouldHaveEnoughMemory(glm::vec3 resolution, int flags, int num_buffers = 1);

#ifdef USE_GST_OPENGL_SYNC_HANDLER
    // for opengl pipeline in gstreamer    
    GstGLContext *global_gl_context;
    GstGLDisplay *global_display;
#endif

protected:
    // GLFW windows management
    std::map<GLFWwindow *, RenderingWindow*> windows_;

    // file drop callback
    static void FileDropped(GLFWwindow* main_window_, int path_count, const char* paths[]);

    bool wayland_;

private:

    // list of rendering attributes
    std::list<RenderingAttrib> draw_attributes_;

    // list of functions to call at each Draw
    std::list<RenderingCallback> draw_callbacks_;

    // windows
    RenderingWindow main_;
    std::string main_new_title_;
    std::vector<RenderingWindow> outputs_;

    // monitors
    std::map<std::string, glm::ivec4> monitors_geometry_;
    static void MonitorConnect(GLFWmonitor* monitor, int event);

    Screenshot screenshot_;
    bool request_screenshot_;


};


#endif /* #define __RENDERING_MANAGER_H_ */
