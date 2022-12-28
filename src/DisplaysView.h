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

    glm::ivec4 outputCoordinates() const;

private:

    Group *output_;
    Group *output_status_;
    Surface *output_surface_;
    Surface *output_render_;
    Switch *output_overlays_;
    Switch *output_mode_;
    Handles *output_handles_;
    Handles *output_menu_;
    Handles *output_visible_;
    Symbol *output_fullscreen_;

    bool output_selected_;
    bool show_output_menu_;

//    Node *overlay_position_;
//    Node *overlay_position_cross_;
//    Symbol *overlay_rotation_;
//    Symbol *overlay_rotation_fix_;
//    Group *overlay_rotation_clock_;
//    Symbol *overlay_rotation_clock_tic_;
//    Node *overlay_rotation_clock_hand_;
//    Symbol *overlay_scaling_;
//    Symbol *overlay_scaling_cross_;
//    Node *overlay_scaling_grid_;
//    Node *overlay_crop_;
};


#endif // DISPLAYSVIEW_H
