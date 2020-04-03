#ifndef __RENDERING_MANAGER_H_
#define __RENDERING_MANAGER_H_

#include <string>
#include <list>

#include <gst/gl/gl.h>
#include <glm/glm.hpp> 

class Screenshot
{
    int             Width, Height;
    unsigned int *  Data;

public:
    Screenshot()    { Width = Height = 0; Data = nullptr; }
    ~Screenshot()   { Clear(); }

    bool IsFull()   { return Data != nullptr; }
    void Clear()    { if (IsFull()) free(Data); Data = nullptr; }

    void CreateEmpty(int w, int h);
    void CreateFromCaptureGL(int x, int y, int w, int h);

    void RemoveAlpha();
    void FlipVertical();

    void SaveFile(const char* filename);
    void BlitTo(Screenshot* dst, int src_x, int src_y, int dst_x, int dst_y, int w, int h) const;
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

    // request screenshot
    void RequestScreenshot();
    // get Screenshot
    Screenshot *CurrentScreenshot();
    
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

    // list of functions to call at each Draw
    std::list<RenderingCallback> drawCallbacks;

    // file drop callback
    static void FileDropped(GLFWwindow* window, int path_count, const char* paths[]);


};


#endif /* #define __RENDERING_MANAGER_H_ */
