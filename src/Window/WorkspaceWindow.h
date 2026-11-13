#ifndef WORKSPACEWINDOW_H
#define WORKSPACEWINDOW_H

#include <list>
#include <glm/glm.hpp>

class WorkspaceWindow
{
    static std::list<WorkspaceWindow *> windows_;

public:
    WorkspaceWindow(const char* name);

    // global access to Workspace control
    static bool clear() { return clear_workspace_enabled; }
    static void toggleClearRestoreWorkspace();
    static void clearWorkspace();
    static void restoreWorkspace(bool force = false);
    static void notifyWorkspaceSizeChanged(int prev_width, int prev_height, int curr_width, int curr_height);

    // windows layout: get and set the geometry (x, y, width, height) of any ImGui window given its name
    // (for WorkspaceWindows, geometry of user, ignoring cleared workspace and collapsed window)
    static bool getGeometry(const char *name, glm::vec4 &geometry);
    static void setGeometry(const char *name, const glm::vec4 &geometry);

    // for inherited classes
    virtual void Update();
    virtual bool Visible() const { return true; }
    virtual void setHidden(bool h, bool force = false);
    virtual void setCollapsed(bool c);

protected:

    static bool clear_workspace_enabled;

    const char *name_;
    struct ImGuiProperties *impl_;
};

#endif // WORKSPACEWINDOW_H
