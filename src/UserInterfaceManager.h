#ifndef __UI_MANAGER_H_
#define __UI_MANAGER_H_

#define NAV_COUNT 68
#define NAV_MAX   64
#define NAV_NEW   65
#define NAV_MENU  66
#define NAV_TRANS 67

#include <string>

#include "View/View.h"
#include "Toolkit/DialogToolkit.h"
#include "Window/SourceControlWindow.h"
#include "Window/OutputPreviewWindow.h"
#include "Window/TimerMetronomeWindow.h"
#include "Window/InputMappingWindow.h"
#include "Window/ShaderEditWindow.h"
#include "Playlist.h"
#include "Navigator.h"


struct ImVec2;
void DrawInspector(uint texture, ImVec2 texturesize, ImVec2 cropsize, ImVec2 origin);

class ToolBox
{
    bool show_demo_window;
    bool show_icons_window;
    bool show_sandbox;

public:
    ToolBox();

    void Render();
};

class UserInterface
{
    friend class ImGuiVisitor;
    friend class Navigator;
    friend class OutputPreviewWindow;
    friend class SourceControlWindow;

    // Private Constructor
    UserInterface();
    UserInterface(UserInterface const& copy) = delete;
    UserInterface& operator=(UserInterface const& copy) = delete;

public:

    static UserInterface& manager()
    {
        // The only instance
        static UserInterface _instance;
        return _instance;
    }

    // pre-loop initialization
    bool Init(int font_size = 0);
    // loop update start new frame
    void NewFrame();
    // loop update rendering
    void Render();
    // try to close, return false if cannot
    bool TryClose();
    // Post-loop termination
    void Terminate();
    // Runtime
    uint64_t Runtime() const;

    // status querries
    inline bool keyboardAvailable() const { return keyboard_available; }
    inline bool ctrlModifier() const { return ctrl_modifier_active; }
    inline bool altModifier() const  { return alt_modifier_active; }
    inline bool shiftModifier() const  { return shift_modifier_active; }

    void showPannel(int id = -1);
    void setSourceInPanel(int index);
    void setSourceInPanel(Source *s);
    Source *sourceInPanel();
    void showSourceEditor(Source *s);

    void StartScreenshot();

protected:

    // internal
    char inifilepath[2048];
    uint64_t start_time;
    bool mousedown;
    bool ctrl_modifier_active;
    bool alt_modifier_active;
    bool shift_modifier_active;
    bool keyboard_available;
    bool show_vimix_about;
    bool show_imgui_about;
    bool show_gst_about;
    bool show_opengl_about;
    int  show_view_navigator;
    int  target_view_navigator;
    unsigned int screenshot_step;
    bool pending_save_on_exit;
    typedef enum { PREVIEW_NONE = 0, PREVIEW_OUTPUT, PREVIEW_SOURCE } PreviewMode;
    PreviewMode show_preview;

    // Dialogs
    DialogToolkit::OpenFileDialog *sessionopendialog;
    DialogToolkit::OpenFileDialog *sessionimportdialog;
    DialogToolkit::SaveFileDialog *sessionsavedialog;
    DialogToolkit::SaveFileDialog *settingsexportdialog;

    // Favorites and playlists
    Playlist favorites;
    std::string playlists_path;

    // objects and windows
    Navigator navigator;
    ToolBox toolbox;
    SourceControlWindow sourcecontrol;
    OutputPreviewWindow outputcontrol;
    TimerMetronomeWindow timercontrol;
    InputMappingWindow inputscontrol;
    ShaderEditWindow shadercontrol;

    void showMenuFile();
    void showMenuEdit();
    void showMenuWindows();
    void showMenuBundle();
    bool saveOrSaveAs(bool force_versioning = false);
    void selectSaveFilename();
    void selectOpenFilename();

    void RenderMetrics (bool* p_open, int* p_corner, int *p_mode);
    void RenderSourceToolbar(bool *p_open, int* p_border, int *p_mode);
    int  RenderViewNavigator(int* shift);
    void RenderPreview();
    void RenderAbout(bool* p_open);
    void RenderNotes();
    void RenderHelp();

    void setView(View::Mode mode);
    void handleKeyboard();
    void handleMouse();
    void handleScreenshot();
};

#endif /* #define __UI_MANAGER_H_ */

