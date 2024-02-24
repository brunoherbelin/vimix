#ifndef __UI_MANAGER_H_
#define __UI_MANAGER_H_

#define NAV_COUNT 68
#define NAV_MAX   64
#define NAV_NEW   65
#define NAV_MENU  66
#define NAV_TRANS 67

#include <string>

#include "View.h"
#include "DialogToolkit.h"
#include "SourceControlWindow.h"
#include "OutputPreviewWindow.h"
#include "TimerMetronomeWindow.h"
#include "InputMappingWindow.h"
#include "ShaderEditWindow.h"
#include "Playlist.h"


struct ImVec2;
void DrawInspector(uint texture, ImVec2 texturesize, ImVec2 cropsize, ImVec2 origin);

class SourcePreview
{
    Source *source_;
    std::string label_;
    bool reset_;

public:
    SourcePreview();

    void setSource(Source *s = nullptr, const std::string &label = "");
    Source *getSource();

    void Render(float width);
    bool ready() const;
    inline bool filled() const { return source_ != nullptr; }
};


class Navigator
{
    // geometry left bar & pannel
    float width_;
    float height_;
    float pannel_width_;
    float padding_width_;

    // behavior pannel
    bool pannel_visible_;
    int  pannel_main_mode_;
    float pannel_alpha_;
    bool view_pannel_visible;
    bool selected_button[NAV_COUNT];
    int  selected_index;
    int  pattern_type;
    int  generated_type;
    bool custom_connected;
    bool custom_screencapture;
    void clearButtonSelection();
    void clearNewPannel();
    void applyButtonSelection(int index);

    // side pannels
    void RenderSourcePannel(Source *s, const ImVec2 &iconsize);
    void RenderMainPannel(const ImVec2 &iconsize);
    void RenderMainPannelSession();
    void RenderMainPannelPlaylist();
    void RenderMainPannelSettings();
    void RenderTransitionPannel(const ImVec2 &iconsize);
    void RenderNewPannel(const ImVec2 &iconsize);
    void RenderViewOptions(uint *timeout, const ImVec2 &pos, const ImVec2 &size);
    bool RenderMousePointerSelector(const ImVec2 &size);

public:

    typedef enum {
        SOURCE_FILE = 0,
        SOURCE_SEQUENCE,
        SOURCE_CONNECTED,
        SOURCE_GENERATED
    } NewSourceType;

    Navigator();
    void Render();

    bool pannelVisible();
    void discardPannel();
    void showPannelSource(int index);
    int  selectedPannelSource();
    void togglePannelMenu();
    void togglePannelNew();
    void showConfig();
    void togglePannelAutoHide();

    typedef enum {
        MEDIA_RECENT = 0,
        MEDIA_RECORDING,
        MEDIA_FOLDER
    } MediaCreateMode;
    void setNewMedia(MediaCreateMode mode, std::string path = std::string());


private:
    // for new source panel
    SourcePreview new_source_preview_;
    std::list<std::string> sourceSequenceFiles;
    std::list<std::string> sourceMediaFiles;
    std::string sourceMediaFileCurrent;
    MediaCreateMode new_media_mode;
    bool new_media_mode_changed;
    Source *source_to_replace;

    static std::vector< std::pair<int, int> > icons_ordering_files;
    static std::vector< std::string > tooltips_ordering_files;
};

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

