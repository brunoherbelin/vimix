#ifndef DISPLAYSVIEW_H
#define DISPLAYSVIEW_H

#include "View/View.h"
#include "Selection.h"
#include "Source/SourceList.h"

class CanvasSource;

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

private:

    Surface *boundingbox_surface_;
    Group *monitors_layout_;
    std::map<std::string, Switch *> monitors_;
    bool horizontal_layout_;
    float monitors_scaling_ratio_;

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

    CanvasSource *current_canvas_source_;
    void setCurrentCanvasSource(Source *c = nullptr);
    Source *currentCanvasSource() const;
    void menuCanvasSource();
    void contextMenuCanvasSource();

    Selection selected_canvas_sources_;
    void updateSelectionOverlay(glm::vec4 color) override;
    void applySelectionTransform(glm::mat4 M);
    void contextMenuCanvasSelection();
    bool overlay_selection_active_;
    Group *overlay_selection_stored_status_;
    Handles *overlay_selection_scale_;
    Handles *overlay_selection_rotate_;

    void adaptGridToSource(Source *s = nullptr, Node *picked = nullptr);
    TranslationGrid *translation_grid_;
    RotationGrid *rotation_grid_;

};


#endif // DISPLAYSVIEW_H
