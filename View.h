#ifndef VIEW_H
#define VIEW_H

#include <glm/glm.hpp>

#include "Scene.h"
#include "FrameBuffer.h"

class Source;
typedef std::list<Source *> SourceList;

class Session;
class SessionFileSource;
class Surface;
class Symbol;
class Mesh;
class Frame;
class Disk;
class Handles;

class View
{
public:

    typedef enum {RENDERING = 0, MIXING=1, GEOMETRY=2, LAYER=3, APPEARANCE=4, TRANSITION=5, INVALID=6 } Mode;

    View(Mode m);
    virtual ~View() {}

    inline Mode mode() const { return mode_; }

    virtual void update (float dt);
    virtual void draw ();

    virtual void zoom (float);
    virtual void resize (int) {}
    virtual int  size () { return 50; }
    virtual void recenter();
    virtual void centerSource(Source *) {}

    typedef enum {
        Cursor_Arrow = 0,
        Cursor_TextInput,
        Cursor_ResizeAll,
        Cursor_ResizeNS,
        Cursor_ResizeEW,
        Cursor_ResizeNESW,
        Cursor_ResizeNWSE,
        Cursor_Hand,
        Cursor_NotAllowed
    } CursorType;

    typedef struct Cursor {
        CursorType type;
        std::string info;
        Cursor() { type = Cursor_Arrow; info = "";}
        Cursor(CursorType t, std::string i = "") { type = t; info = i;}
    } Cursor;

    // picking of nodes in a view provided a point coordinates in screen coordinates
    virtual std::pair<Node *, glm::vec2> pick(glm::vec2);

    // select sources provided a start and end selection points in screen coordinates
    virtual void select(glm::vec2, glm::vec2);
    virtual void selectAll();
    virtual bool canSelect(Source *);

    // drag the view provided a start and an end point in screen coordinates
    virtual Cursor drag (glm::vec2, glm::vec2);

    // grab a source provided a start and an end point in screen coordinates and the picking point
    virtual void initiate();
    virtual void terminate();
    virtual Cursor grab (Source*, glm::vec2, glm::vec2, std::pair<Node *, glm::vec2>) {
        return Cursor();
    }

    // test mouse over provided a point in screen coordinates
    virtual Cursor over (glm::vec2) {
        return Cursor();
    }

    // left-right [-1 1] and up-down [1 -1] action from arrow keys
    virtual void arrow (glm::vec2) {}

    // accessible scene
    Scene scene;

    // reordering scene when necessary
    static uint need_deep_update_;

protected:

    virtual void restoreSettings();
    virtual void saveSettings();

    std::string current_action_;
    uint64_t current_id_;
    Mode mode_;

    // contex menu
    bool show_context_menu_;
    inline void openContextMenu() { show_context_menu_ = true; }
};


class MixingView : public View
{
public:
    MixingView();

    void draw () override;
    void update (float dt) override;
    void resize (int) override;
    int  size () override;
    void centerSource(Source *) override;

    std::pair<Node *, glm::vec2> pick(glm::vec2) override;
    Cursor grab (Source *s, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2>) override;
    void arrow (glm::vec2) override;

    void setAlpha (Source *s);
    inline float limboScale() { return limbo_scale_; }

private:
    uint textureMixingQuadratic();
    float limbo_scale_;

    Group *slider_root_;
    Disk *slider_;
    Disk *button_white_;
    Disk *button_black_;
    Disk *stashCircle_;
    Mesh *mixingCircle_;
    Mesh *circle_;
};

class RenderView : public View
{
    FrameBuffer *frame_buffer_;
    Surface *fading_overlay_;

public:
    RenderView ();
    ~RenderView ();

    void draw () override;
    bool canSelect(Source *) override;

    void setResolution (glm::vec3 resolution = glm::vec3(0.f), bool useAlpha = false);
    glm::vec3 resolution() const { return frame_buffer_->resolution(); }

    void setFading(float f = 0.f);
    float fading() const;

    inline FrameBuffer *frame () const { return frame_buffer_; }
};

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

class LayerView : public View
{
public:
    LayerView();

    void draw () override;
    void update (float dt) override;
    void resize (int) override;
    int  size () override;
    bool canSelect(Source *) override;

    std::pair<Node *, glm::vec2> pick(glm::vec2) override;
    Cursor grab (Source *s, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2> pick) override;
    void arrow (glm::vec2) override;

    float setDepth (Source *, float d = -1.f);

private:
    float aspect_ratio;
    Mesh *persp_layer_;
    Mesh *persp_left_, *persp_right_;
    Group *frame_;
    Group *overlay_group_;
    Frame *overlay_group_frame_;
    Handles *overlay_group_icon_;

};

class TransitionView : public View
{
public:
    TransitionView();

    void draw () override;
    void update (float dt) override;
    void zoom (float factor) override;
    bool canSelect(Source *) override;

    std::pair<Node *, glm::vec2> pick(glm::vec2 P) override;
    Cursor grab (Source *s, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2> pick) override;
    void arrow (glm::vec2) override;
    Cursor drag (glm::vec2, glm::vec2) override;

    void attach(SessionFileSource *ts);
    Session *detach();
    void play(bool open);

private:
    Surface *output_surface_;
    Mesh *mark_100ms_, *mark_1s_;
    Switch *gradient_;
    SessionFileSource *transition_source_;
};


class AppearanceView : public View
{
public:
    AppearanceView();

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
    void terminate() override;

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


#endif // VIEW_H
