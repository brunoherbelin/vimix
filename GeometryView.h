#ifndef GEOMETRYVIEW_H
#define GEOMETRYVIEW_H

#include "View.h"

class GeometryView : public View
{
public:
    GeometryView();

    void draw () override;
    void update (float dt) override;
    void resize (int) override;
    int  size () override;
    bool canSelect(Source *) override;

    std::pair<Node *, glm::vec2> pick(glm::vec2 P) override;
    Cursor grab (Source *s, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2> pick) override;
    void terminate() override;
    void arrow (glm::vec2) override;

private:
    Surface *output_surface_;
    Node *overlay_position_;
    Node *overlay_position_cross_;
    Node *overlay_rotation_;
    Node *overlay_rotation_fix_;
    Node *overlay_rotation_clock_;
    Node *overlay_rotation_clock_hand_;
    Node *overlay_scaling_;
    Node *overlay_scaling_cross_;
    Node *overlay_scaling_grid_;
    Node *overlay_crop_;
};


#endif // GEOMETRYVIEW_H
