#ifndef __RENDERING_MANAGER_H_
#define __RENDERING_MANAGER_H_

#include <string>
#include <list>

#include <gst/gl/gl.h>
#include <glm/glm.hpp> 

#include "Screenshot.h"

struct RenderingAttrib
{
    RenderingAttrib() {}
    glm::ivec2 viewport;
    glm::vec4 clear_color;
};


class Rendering
{
    friend class UserInterface;

    // GLFW integration in OS window management
    class GLFWwindow* main_window_;
    std::string glsl_version;

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
    bool Init();
    // true if active rendering window
    bool isActive();
    // draw one frame
    void Draw();
    // request close of the UI (Quit the program)
    void Close();
    // Post-loop termination
    void Terminate();

    void GrabWindow(int dx, int dy);

    // add function to call during Draw
    typedef void (* RenderingCallback)(void);
    void PushFrontDrawCallback(RenderingCallback function);
    void PushBackDrawCallback(RenderingCallback function);

    // push and pop rendering attributes
    void PushAttrib(RenderingAttrib ra);
    void PopAttrib();
    RenderingAttrib currentAttrib();

    // request screenshot
    void RequestScreenshot();
    // get Screenshot
    class Screenshot *CurrentScreenshot();
    
    // request fullscreen
    void ToggleFullscreen();
    // get width of rendering area
    float Width();
    // get height of rendering area
    float Height();
    // get aspect ratio of rendering area
    float AspectRatio();

    // get projection matrix (for sharers) => Views
    glm::mat4 Projection();
    // unproject from window coordinate
    glm::vec3 unProject(glm::vec2 screen_coordinate, glm::mat4 modelview = glm::mat4(1.f));

    // for opengl pipeline in gstreamer
    void LinkPipeline( GstPipeline *pipeline );

private:

    // loop update to begin new frame
    bool Begin();
    // loop update end frame
    void End();

    // list of rendering attributes
    std::list<RenderingAttrib> draw_attributes_;
    RenderingAttrib main_window_attributes_;

    // list of functions to call at each Draw
    std::list<RenderingCallback> draw_callbacks_;

    // file drop callback
    static void FileDropped(GLFWwindow* main_window_, int path_count, const char* paths[]);

    Screenshot screenshot_;
    bool request_screenshot_;


};


#endif /* #define __RENDERING_MANAGER_H_ */
