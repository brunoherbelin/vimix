#ifndef GEOMETRYVIEW_H
#define GEOMETRYVIEW_H

#include "View.h"


class GeometryView : public View
{
public:
    GeometryView();
    // non assignable class
    GeometryView(GeometryView const&) = delete;
    GeometryView& operator=(GeometryView const&) = delete;

    void draw () override;
    void update (float dt) override;
    void resize (int) override;
    int  size () override;
    bool canSelect(Source *) override;

    std::pair<Node *, glm::vec2> pick(glm::vec2 P) override;
    Cursor grab (Source *s, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2> pick) override;
    void terminate(bool force = false) override;
    void arrow (glm::vec2) override;
    Cursor over (glm::vec2) override;

private:
    Surface *output_surface_;
    Node *overlay_position_;
    Node *overlay_position_cross_;
    Symbol *overlay_rotation_;
    Symbol *overlay_rotation_fix_;
    Group *overlay_rotation_clock_;
    Symbol *overlay_rotation_clock_tic_;
    Node *overlay_rotation_clock_hand_;
    Symbol *overlay_scaling_;
    Symbol *overlay_scaling_cross_;
    Node *overlay_scaling_grid_;
    Node *overlay_crop_;

    void updateSelectionOverlay(glm::vec4 color) override;
    bool overlay_selection_active_;
    Group *overlay_selection_stored_status_;
    Handles *overlay_selection_scale_;
    Handles *overlay_selection_rotate_;

    void applySelectionTransform(glm::mat4 M);

    void adaptGridToSource(Source *s = nullptr, Node *picked = nullptr);
    TranslationGrid *translation_grid_;
    RotationGrid *rotation_grid_;
};


#endif // GEOMETRYVIEW_H
