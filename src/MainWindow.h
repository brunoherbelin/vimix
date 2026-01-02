#ifndef __MAIN_WINDOW_H_
#define __MAIN_WINDOW_H_

#include <cstddef>
#include <string>

#include <gst/gl/gl.h>
#include <glm/glm.hpp>

typedef struct GLFWmonitor GLFWmonitor;
typedef struct GLFWwindow GLFWwindow;

class FrameBuffer;
class Stream;

struct RenderingAttrib
{
    RenderingAttrib() {}
    glm::ivec2 viewport;
    glm::vec4 clear_color;
};


class MainWindow
{
    friend class Rendering;

    GLFWwindow *window_;
    RenderingAttrib window_attributes_;
    std::string title_changed_;
    std::string main_new_title_;
    float dpi_scale_;

protected:
    void changeTitle_();
    void setTitle_(const std::string &title = "");
    void setIcon(const std::string &resource);
    bool request_change_fullscreen_;
    void changeFullscreen_ ();
    void setFullscreen_(GLFWmonitor *mo);

public:
    MainWindow();
    ~MainWindow();


    bool init(int index, GLFWwindow *share = NULL);
    void terminate();

    // show window (bring front, set fullscreen if needed)
    void show();

    // make context current and set viewport
    void makeCurrent();

    // fullscreen
    bool isFullscreen ();
    void exitFullscreen ();
    void setFullscreen (std::string monitorname);
    void toggleFullscreen ();

    // set title (deferred to avoid context issues)
    inline void setTitle(const std::string t) { main_new_title_ = t; }

    // get GLFW stuff
    inline RenderingAttrib& attribs() { return window_attributes_; }
    inline GLFWwindow *window() const { return window_; }
    GLFWmonitor *monitor() const;

    // HACK 
    glm::vec2 previous_size;

    // get width of rendering area
    int width();
    // get height of rendering area
    int height();
    // get aspect ratio of rendering area
    float aspectRatio();
    // high dpi monitor scaling
    inline float dpiScale() const { return dpi_scale_; }
    // get number of pixels to render X milimeters in height
    int pixelsforRealHeight(float milimeters);

    // glfw callbacks
    static void FileDropped(GLFWwindow* w, int path_count, const char* paths[]);
    static void WindowCloseCallback( GLFWwindow* w );
    static void WindowResizeCallback( GLFWwindow *w, int width, int height);

};

#endif /* #define __MAIN_WINDOW_H_ */
