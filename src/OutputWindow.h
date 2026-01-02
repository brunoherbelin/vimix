#ifndef __OUTPUT_WINDOW_H_
#define __OUTPUT_WINDOW_H_

#include <cstddef>
#include <string>

#include <gst/gl/gl.h>
#include <glm/glm.hpp>

typedef struct GLFWmonitor GLFWmonitor;
typedef struct GLFWwindow GLFWwindow;


class OutputWindow
{
public:
    OutputWindow();
    ~OutputWindow();

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