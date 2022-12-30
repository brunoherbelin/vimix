#ifndef DISPLAYSVIEW_H
#define DISPLAYSVIEW_H

#include "View.h"

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

    glm::ivec4 outputCoordinates() const;
    std::string outputFullscreenMonitor() const;

private:

    Group *output_;
    Group *output_status_;
    Surface *output_surface_;
    Surface *output_render_;
    Switch  *output_overlays_;
    Switch  *output_mode_;
    Handles *output_handles_;
    Handles *output_menu_;
    Handles *output_visible_;
    Symbol  *output_fullscreen_;

    bool output_selected_;
    bool show_output_menu_;
    int  display_action_;
    bool draw_pending_;

    std::string output_monitor_;
};


#endif // DISPLAYSVIEW_H
