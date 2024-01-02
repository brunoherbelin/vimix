#ifndef DISPLAYSVIEW_H
#define DISPLAYSVIEW_H

#include "View.h"


struct WindowPreview
{
    class FrameBuffer *renderbuffer_;
    class ImageFilteringShader *shader_;
    class FrameBufferSurface *surface_;
    class MeshSurface *output_render_;
    Group *root_;
    Group *output_group_;
    class LineLoop *output_lines_;
    Handles *output_handles_[4];
    Switch  *overlays_;
    Switch  *mode_;
    Handles *resize_;
    Handles *menu_;
    Handles *icon_;
    Surface *title_;
    Symbol  *fullscreen_;
    std::string monitor_;
    int need_update_;

    WindowPreview() {
        renderbuffer_ = nullptr;
        shader_ = nullptr;
        surface_ = nullptr;
        output_render_ = nullptr;
        root_ = nullptr;
        output_group_ = nullptr;
        output_handles_[0] = nullptr;
        output_handles_[1] = nullptr;
        output_handles_[2] = nullptr;
        output_handles_[3] = nullptr;
        overlays_ = nullptr;
        mode_ = nullptr;
        resize_ = nullptr;
        menu_ = nullptr;
        icon_ = nullptr;
        title_ = nullptr;
        fullscreen_ = nullptr;
        monitor_ = "";
        need_update_ = 2;
    }
    ~WindowPreview();

    struct hasNode
    {
        bool operator()(const WindowPreview &elem) const;
        explicit hasNode(Node *n) : _n(n) { }
    private:
        Node *_n;
    };

};


class DisplaysView : public View
{
public:
    DisplaysView();
    // non assignable class
    DisplaysView(DisplaysView const&) = delete;
    DisplaysView& operator=(DisplaysView const&) = delete;

    void draw () override;
    void update (float dt) override;
    void resize (int) override;
    int  size () override;
    bool canSelect(Source *) override;
    void select(glm::vec2 A, glm::vec2 B) override;
    void recenter () override;

    std::pair<Node *, glm::vec2> pick(glm::vec2 P) override;
    void initiate () override;
    void terminate (bool force = false) override;
    Cursor grab (Source *s, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2> pick) override;
    void arrow (glm::vec2) override;
    bool doubleclic (glm::vec2) override;

    glm::ivec4 windowCoordinates(int index) const;
    std::string fullscreenMonitor(int index) const;

private:

    bool draw_pending_;
    float output_ar;
    std::vector<WindowPreview> windows_;
    int current_window_;
    Group *current_window_status_;
    Group *current_output_status_;
    bool show_window_menu_;

    Group *gridroot_;
    void adaptGridToWindow(int w = -1);

};


#endif // DISPLAYSVIEW_H
