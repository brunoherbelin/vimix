#ifndef __OUTPUT_WINDOW_H_
#define __OUTPUT_WINDOW_H_

#include <string>

#include "Toolkit/GlmToolkit.h"

typedef struct GLFWmonitor GLFWmonitor;
typedef struct GLFWwindow GLFWwindow;

class ImageFilteringShader;
class OutputWindowSurface;
class FrameBuffer;
class Stream;

class OutputWindow
{
    
public:
    OutputWindow();
    ~OutputWindow();

    // initialization
    bool init(GLFWmonitor *monitor, GLFWwindow *share);
    void terminate();
    inline bool isInitialized() const { return initialized_; }

    // active state
    inline void setActive(bool on) { active_ = on; OutputWindow::num_active_outputs += on ? 1 : -1; }
    inline bool isActive() const { return active_; }

    // pattern state
    void setShowPattern(bool on);
    inline bool isShowPattern() const { return show_pattern_; }

    // draw a framebuffer
    bool draw(FrameBuffer *fb);

    // get GLFW window
    inline GLFWwindow *window() const { return window_; }

    // glfw callbacks
    static void MouseButtonCallback(GLFWwindow *w, int button, int action, int mods);
    
private:

    // number of active output windows (to detect when all are closed)
    static uint num_active_outputs;

    // GLFW window 
    GLFWwindow *window_;
    // name of monitor on which window is displayed
    std::string monitor_name_;

    // state management
    bool active_;
    bool initialized_;

    // Drawing of framebuffer
    GlmToolkit::RenderingAttrib window_attributes_;        
    OutputWindowSurface *surface_;


    // options 
    ImageFilteringShader *shader_;
    bool show_pattern_;
    Stream *pattern_;


};


#endif /* #define __OUTPUT_WINDOW_H_ */
