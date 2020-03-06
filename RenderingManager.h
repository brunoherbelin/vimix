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
    static class GLFWwindow* window;
    static Screenshot window_screenshot;
    static std::string glsl_version;
    static int render_width, render_height;
    static bool request_screenshot;

public:

    // Initialization OpenGL and GLFW window creation
    static bool Init();
    // true if active rendering window
    static bool isActive();
    // request close of the UI (Quit the program)
    static void Close();
    // Post-loop termination
    static void Terminate();

    // draw one frame
    static void Draw();
    // add function to call during Draw
    typedef void (* RenderingCallback)(void);
    static void AddDrawCallback(RenderingCallback function);
    // request screenshot
    static void RequestScreenshot();
    // get Screenshot
    static Screenshot *CurrentScreenshot();
    
    // request fullscreen
    static void ToggleFullscreen();
    // get width of rendering area
    static float Width() { return render_width; }
    // get height of rendering area
    static float Height() { return render_height; }
    // get aspect ratio of rendering area
    static float AspectRatio();
    // get projection matrix (for sharers)
    static glm::mat4 Projection();

    // for opengl pipile in gstreamer
    static void LinkPipeline( GstPipeline *pipeline );


private:

    // loop update to begin new frame
    static bool Begin();
    // loop update end frame
    static void End();
    // list of functions to call at each Draw
    static std::list<RenderingCallback> drawCallbacks;

    // file drop callback
    static void FileDropped(GLFWwindow* window, int path_count, const char* paths[]);


};


#endif /* #define __RENDERING_MANAGER_H_ */
