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
    glm::ivec4 viewport;
    glm::vec3 clear_color;
};


class Rendering
{
    friend class UserInterface;

    // GLFW integration in OS window management
    class GLFWwindow* window;
    Screenshot window_screenshot;
    std::string glsl_version;
    int render_width, render_height;
    bool request_screenshot;

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

    // add function to call during Draw
    typedef void (* RenderingCallback)(void);
    void PushFrontDrawCallback(RenderingCallback function);
    void PushBackDrawCallback(RenderingCallback function);

    // push and pop rendering attributes
    void PushAttrib(glm::ivec4 viewport, glm::vec3 color);
    void PopAttrib();

    // request screenshot
    void RequestScreenshot();
    // get Screenshot
    class Screenshot *CurrentScreenshot();
    
    // request fullscreen
    void ToggleFullscreen();
    // get width of rendering area
    float Width() { return render_width; }
    // get height of rendering area
    float Height() { return render_height; }
    // get aspect ratio of rendering area
    float AspectRatio();

    // get projection matrix (for sharers) => Views
    glm::mat4 Projection();

    // for opengl pipeline in gstreamer
    void LinkPipeline( GstPipeline *pipeline );

private:

    // loop update to begin new frame
    bool Begin();
    // loop update end frame
    void End();

    // list of rendering attributes
    std::list<RenderingAttrib> drawAttributes;

    // list of functions to call at each Draw
    std::list<RenderingCallback> drawCallbacks;

    // file drop callback
    static void FileDropped(GLFWwindow* window, int path_count, const char* paths[]);


};


#endif /* #define __RENDERING_MANAGER_H_ */
