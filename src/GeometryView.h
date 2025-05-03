#ifndef GEOMETRYVIEW_H
#define GEOMETRYVIEW_H

// #define ENABLE_CANVAS

#include "View.h"

struct Canvas
{
    bool current_;
    Group *root_;
    Switch *frames_;
    Group *overlays_;
    Handles *handles_[3];
    Handles *menu_;

    Canvas()
    {
        current_ = false;
        root_ = nullptr;
        frames_ = nullptr;
        overlays_ = nullptr;
        menu_ = nullptr;
        handles_[0] = nullptr;
        handles_[1] = nullptr;
        handles_[2] = nullptr;
    }

    void setCurrent(bool on);
};

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
    void initiate () override;
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

    std::vector<Canvas> canvas_;
    uint canvas_current_;
    Group *canvas_stored_status_;

    enum GeometryModes {
        EDIT_SOURCES = 0,
        EDIT_CANVAS = 1
    };
    uint editor_mode_;
    static const char* editor_icons[2];
    static const char* editor_names[2];

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
