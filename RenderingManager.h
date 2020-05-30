#ifndef __RENDERING_MANAGER_H_
#define __RENDERING_MANAGER_H_

#include <string>
#include <list>

#include <gst/gl/gl.h>
#include <glm/glm.hpp> 

#include "Screenshot.h"

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
    FrameBuffer *frame_buffer_;
    int id_;

public:
    RenderingWindow();
    ~RenderingWindow();

    void setFrameBuffer(FrameBuffer *fb) { frame_buffer_ = fb; }

    bool init(GLFWwindow *share, int id);
    void draw();

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

    // request screenshot
    void requestScreenshot();
    // get Screenshot
    class Screenshot *currentScreenshot();
    
    // window management
    void setWindowTitle(std::string title);    
    // request fullscreen
    bool isFullscreen ();
    void toggleFullscreen ();
    // get width of rendering area
    float width();
    // get height of rendering area
    float height();
    // get aspect ratio of rendering area
    float aspectRatio();

    // monitor management
    float monitorWidth();
    float monitorHeight();
    inline float DPIScale() const { return dpi_scale_; }

    // get projection matrix (for sharers) => Views
    glm::mat4 Projection();
    // unproject from window coordinate
    glm::vec3 unProject(glm::vec2 screen_coordinate, glm::mat4 modelview = glm::mat4(1.f));

    // utility for settings
    int getWindowId(GLFWwindow *w);

private:

    // GLFW integration in OS window management
    GLFWwindow *main_window_;
    std::string glsl_version;
    float dpi_scale_;

    // loop update to begin new frame
    bool Begin();
    // loop update end frame
    void End();

//    void GrabWindow(int dx, int dy);

    // list of rendering attributes
    std::list<RenderingAttrib> draw_attributes_;
    RenderingAttrib main_window_attributes_;

    // list of functions to call at each Draw
    std::list<RenderingCallback> draw_callbacks_;

    RenderingWindow output;

    // file drop callback
    static void FileDropped(GLFWwindow* main_window_, int path_count, const char* paths[]);

    Screenshot screenshot_;
    bool request_screenshot_;

    // for opengl pipeline in gstreamer
    void LinkPipeline( GstPipeline *pipeline );
};


#endif /* #define __RENDERING_MANAGER_H_ */
