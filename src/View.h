#ifndef VIEW_H
#define VIEW_H

#include <glm/glm.hpp>

#include "Scene.h"
#include "FrameBuffer.h"
#include "SourceList.h"

class Session;
class SessionFileSource;
class Surface;
class Symbol;
class Mesh;
class Frame;
class Disk;
class Handles;
class Source;


class View
{
public:

    typedef enum {
        RENDERING = 0,
        MIXING    = 1,
        GEOMETRY  = 2,
        LAYER     = 3,
        TEXTURE   = 4,
        TRANSITION = 5,
        DISPLAYS  = 6,
        INVALID   = 7
    } Mode;

    inline Mode mode () const { return mode_; }

    virtual void update (float dt);
    virtual void draw ();

    virtual void zoom (float);
    virtual void resize (int) {}
    virtual int  size () { return 50; }
    virtual void recenter ();
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
        Cursor () { type = Cursor_Arrow; info = "";}
        Cursor (CursorType t, std::string i = "") { type = t; info = i;}
    } Cursor;

    // picking of nodes in a view provided a point coordinates in screen coordinates
    virtual std::pair<Node *, glm::vec2> pick(glm::vec2);

    // select sources provided a start and end selection points in screen coordinates
    virtual void select (glm::vec2, glm::vec2);
    virtual void selectAll ();
    virtual bool canSelect (Source *);

    // drag the view provided a start and an end point in screen coordinates
    virtual Cursor drag (glm::vec2, glm::vec2);

    // grab a source provided a start and an end point in screen coordinates and the picking point
    virtual void initiate ();
    virtual void terminate (bool force = false);
    virtual Cursor grab (Source*, glm::vec2, glm::vec2, std::pair<Node *, glm::vec2>) {
        return Cursor ();
    }

    // test mouse over provided a point in screen coordinates
    virtual Cursor over (glm::vec2) {
        return Cursor ();
    }

    // left-right [-1 1] and up-down [1 -1] action from arrow keys
    virtual void arrow (glm::vec2) {}

    // resolution on screen
    glm::vec2 resolution() const;

    // accessible scene
    Scene scene;

    // reordering scene when necessary
    static uint need_deep_update_;

protected:

    View (Mode m);
    virtual ~View () {}

    virtual void restoreSettings ();
    virtual void saveSettings ();

    Mode mode_;

    bool current_action_ongoing_;
    std::string current_action_;

    // selection
    Group *overlay_selection_;
    Frame *overlay_selection_frame_;
    Handles *overlay_selection_icon_;
    virtual void updateSelectionOverlay ();

    // contex menu
    typedef enum {
        MENU_NONE = 0,
        MENU_SOURCE = 1,
        MENU_SELECTION = 2
    } ContextMenu;
    ContextMenu show_context_menu_;
    inline void openContextMenu (ContextMenu m) { show_context_menu_ = m; }
    void lock(Source *s, bool on);

    float dt_;
};


#endif // VIEW_H
