#ifndef __RENDERING_MANAGER_H_
#define __RENDERING_MANAGER_H_

#include <string>
#include <list>
#include <map>

#include <gst/gl/gl.h>
#include <glm/glm.hpp> 

#include "Screenshot.h"

class GLFWmonitor;
class GLFWwindow;
class FrameBuffer;

struct RenderingAttrib
{
    RenderingAttrib() {}
    glm::ivec2 viewport;
    glm::vec4 clear_color;
};

class RenderingWindow
{
    GLFWwindow *window_, *master_;
    RenderingAttrib window_attributes_;
    int id_;
    float dpi_scale_;

    // objects to render
    int textureid_;
    uint fbo_;
    class WindowSurface *surface_;


public:
    RenderingWindow();
    ~RenderingWindow();

    inline int id() const { return id_; }
    inline RenderingAttrib& attribs() { return window_attributes_; }
    inline GLFWwindow *window() const { return window_; }

    bool init(int id, GLFWwindow *share = NULL);
    void setIcon(const std::string &resource);
    void setTitle(const std::string &title = "");

    // show window (fullscreen if needed)
    void show();

    // make context current and set viewport
    void makeCurrent();

    // draw a framebuffer
    void draw(FrameBuffer *fb);

    // fullscreen
    bool isFullscreen ();
    void setFullscreen(GLFWmonitor *mo);
    void toggleFullscreen ();

    // get width of rendering area
    int width();
    // get height of rendering area
    int height();
    // get aspect ratio of rendering area
    float aspectRatio();
    // get total height available in monitor
    int maxHeight();

    inline float dpiScale() const { return dpi_scale_; }

    // get monitor in which the window is
    GLFWmonitor *monitor();
    // get which monitor contains this point
    static GLFWmonitor *monitorAt(int x, int y);
    // get which monitor has this name
    static GLFWmonitor *monitorNamed(const std::string &name);
};

class Rendering
{
    friend class UserInterface;

    // Private Constructor
    Rendering();
    Rendering(Rendering const& copy);            // Not Implemented
    Rendering& operator=(Rendering const& copy); // Not Implemented

public:

    static Rendering& manager()
    {
        // The only instance
        static Rendering _instance;
        return _instance;
    }

    // Initialization OpenGL and GLFW window creation
    bool init();

    void show();

    // true if active rendering window
    bool isActive();
    // draw one frame
    void draw();
    // request close of the UI (Quit the program)
    void close();
    // Post-loop termination
    void terminate();

    // add function to call during Draw
    typedef void (* RenderingCallback)(void);
    void pushFrontDrawCallback(RenderingCallback function);
    void pushBackDrawCallback(RenderingCallback function);

    // push and pop rendering attributes
    void pushAttrib(RenderingAttrib ra);
    void popAttrib();
    RenderingAttrib currentAttrib();

    // get hold on the main window
    inline RenderingWindow& mainWindow() { return main_; }
    inline RenderingWindow& outputWindow() { return output_; }

    // request screenshot
    void requestScreenshot();
    // get Screenshot
    class Screenshot *currentScreenshot();

    // get projection matrix (for sharers) => Views
    glm::mat4 Projection();
    // unproject from window coordinate to scene
    glm::vec3 unProject(glm::vec2 screen_coordinate, glm::mat4 modelview = glm::mat4(1.f));
    // project from scene coordinate to window
    glm::vec2 project(glm::vec3 scene_coordinate, glm::mat4 modelview = glm::mat4(1.f));

private:

    std::string glsl_version;

    // list of rendering attributes
    std::list<RenderingAttrib> draw_attributes_;

    // list of functions to call at each Draw
    std::list<RenderingCallback> draw_callbacks_;

    RenderingWindow main_;
    RenderingWindow output_;

    // file drop callback
    static void FileDropped(GLFWwindow* main_window_, int path_count, const char* paths[]);

    Screenshot screenshot_;
    bool request_screenshot_;

    // for opengl pipeline in gstreamer
    void LinkPipeline( GstPipeline *pipeline );
};


#endif /* #define __RENDERING_MANAGER_H_ */
