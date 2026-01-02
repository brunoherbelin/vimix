#ifndef __OUTPUT_WINDOW_H_
#define __OUTPUT_WINDOW_H_

#include <cstddef>
#include <string>

#include <gst/gl/gl.h>
#include <glm/glm.hpp>

typedef struct GLFWmonitor GLFWmonitor;
typedef struct GLFWwindow GLFWwindow;
typedef struct RenderingAttrib RenderingAttrib;

class FrameBuffer;

class OutputWindow
{
    GLFWwindow *window_;
    GLFWmonitor *monitor_;
    bool active_;
    bool initialized_;
    RenderingAttrib window_attributes_;

public:
    OutputWindow();
    ~OutputWindow();

    // initialization
    bool init(GLFWmonitor *monitor, GLFWwindow *share);
    void terminate();
    inline bool isInitialized() const { return initialized_; }

    // active state
    inline void setActive(bool on) { active_ = on; }
    inline bool isActive() const { return active_; }

    // draw a framebuffer
    bool draw(FrameBuffer *fb);

    // get GLFW window
    inline GLFWwindow *window() const { return window_; }

    // glfw callbacks
    static void MouseButtonCallback(GLFWwindow *w, int button, int action, int mods);
};


#endif /* #define __OUTPUT_WINDOW_H_ */

// class RenderingOutput
// {
//     // friend class Rendering;
//     static std::vector<RenderingOutput> vector_;

//     GLFWmonitor* monitor;
//     std::string name;
//     glm::ivec4 geometry;
//     GLFWwindow *window_;
//     bool enabled;

// public:
//     RenderingOutput() : monitor(nullptr), name(""), geometry(0,0,0,0), enabled(false) {}

//     static bool isEnabled(size_t index) {
//         if (index >= vector_.size())
//             return false;
//         return vector_[index].enabled;
//     }
//     static void setEnabled(size_t index, bool en) {
//         if (index >= vector_.size())
//             return;
//         vector_[index].enabled = en;
//     }

//     static void drawAll();

//     static size_t count() { return vector_.size(); }  
//     static RenderingOutput at(size_t index)   { 
//         if (index >= vector_.size())
//             return RenderingOutput();
//         return vector_[index]; 
//     }  

// };