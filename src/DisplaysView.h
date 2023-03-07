#ifndef DISPLAYSVIEW_H
#define DISPLAYSVIEW_H

#include "View.h"


struct WindowPreview
{
    Group *root_;
    Group *status_;
    Surface *surface_;
    Surface *render_;
    class ImageFilteringShader *shader_;
    Switch  *overlays_;
    Switch  *mode_;
    Handles *handles_;
    Handles *menu_;
    Handles *icon_;
    Surface *title_;
    Symbol  *fullscreen_;
    std::string monitor_;

    WindowPreview() {
        root_ = nullptr;
        status_ = nullptr;
        surface_ = nullptr;
        render_ = nullptr;
        overlays_ = nullptr;
        status_ = nullptr;
        mode_ = nullptr;
        handles_ = nullptr;
        menu_ = nullptr;
        icon_ = nullptr;
        title_ = nullptr;
        fullscreen_ = nullptr;
        monitor_ = "";
    }

    struct hasNode
    {
        bool operator()(WindowPreview elem) const;
        hasNode(Node *n) : _n(n) { }
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
    Cursor over (glm::vec2) override;
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
    bool show_window_menu_;

    int  display_action_;


//    bool get_UV_window_render_from_pick(const glm::vec3 &pos, glm::vec2 *uv);

};


#endif // DISPLAYSVIEW_H
