#ifndef TEXTUREVIEW_H
#define TEXTUREVIEW_H

#include "View.h"


class TextureView : public View
{
public:
    TextureView();
    // non assignable class
    TextureView(TextureView const&) = delete;
    TextureView& operator=(TextureView const&) = delete;

    void draw () override;
    void update (float dt) override;
    void resize (int) override;
    int  size () override;
    bool canSelect(Source *) override;
    void select(glm::vec2 A, glm::vec2 B) override;

    std::pair<Node *, glm::vec2> pick(glm::vec2 P) override;
    Cursor grab (Source *s, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2> pick) override;
    Cursor over (glm::vec2) override;
    void arrow (glm::vec2) override;

    void initiate() override;
    void terminate(bool force) override;

private:

    Source *edit_source_;
    bool need_edit_update_;
    Source *getEditOrCurrentSource();
    void adjustBackground();

    Surface *preview_surface_;
    class ImageShader *preview_shader_;
    Surface *preview_checker_;
    Frame *preview_frame_;
    Surface *background_surface_;
    Frame *background_frame_;
    Mesh *horizontal_mark_;
    Mesh *vertical_mark_;
    bool show_scale_;
    Group *mask_node_;
    Frame *mask_square_;
    Mesh *mask_circle_;
    Mesh *mask_horizontal_;
    Group *mask_vertical_;

    Symbol *overlay_position_;
    Symbol *overlay_position_cross_;
    Symbol *overlay_scaling_;
    Symbol *overlay_scaling_cross_;
    Node *overlay_scaling_grid_;
    Symbol *overlay_rotation_;
    Symbol *overlay_rotation_fix_;
    Node *overlay_rotation_clock_;
    Symbol *overlay_rotation_clock_hand_;

    // for mask shader draw: 0=cursor, 1=brush, 2=eraser, 3=crop_shape
    int mask_cursor_paint_;
    int mask_cursor_shape_;
    Mesh *mask_cursor_circle_;
    Mesh *mask_cursor_square_;
    Mesh *mask_cursor_crop_;
    glm::vec3 stored_mask_size_;
    bool show_cursor_forced_;

};

#endif // TEXTUREVIEW_H
