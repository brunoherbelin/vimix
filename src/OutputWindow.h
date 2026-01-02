#ifndef __OUTPUT_WINDOW_H_
#define __OUTPUT_WINDOW_H_

#include <string>

#include <gst/gl/gl.h>
#include <glm/glm.hpp>

#include "Toolkit/GlmToolkit.h"

typedef struct GLFWmonitor GLFWmonitor;
typedef struct GLFWwindow GLFWwindow;

class ImageFilteringShader;
class OutputWindowSurface;
class FrameBuffer;
class Stream;

class OutputWindow
{
    GLFWwindow *window_;
    std::string monitor_name_;
    bool active_;
    bool initialized_;
    GlmToolkit::RenderingAttrib window_attributes_;        
    OutputWindowSurface *surface_;
    ImageFilteringShader *shader_;
    bool show_pattern_;
    Stream *pattern_;

    static uint num_active_outputs;

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
    
};


#endif /* #define __OUTPUT_WINDOW_H_ */
