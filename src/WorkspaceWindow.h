#ifndef WORKSPACEWINDOW_H
#define WORKSPACEWINDOW_H

#include <list>

class WorkspaceWindow
{
    static std::list<WorkspaceWindow *> windows_;

public:
    WorkspaceWindow(const char* name);

    // global access to Workspace control
    static bool clear() { return clear_workspace_enabled; }
    static void toggleClearRestoreWorkspace();
    static void clearWorkspace();
    static void restoreWorkspace(bool instantaneous = false);
    static void notifyWorkspaceSizeChanged(int prev_width, int prev_height, int curr_width, int curr_height);

    // for inherited classes
    virtual void Update();
    virtual bool Visible() const { return true; }

protected:

    static bool clear_workspace_enabled;

    const char *name_;
    struct ImGuiProperties *impl_;
};

#endif // WORKSPACEWINDOW_H
