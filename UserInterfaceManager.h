#ifndef __UI_MANAGER_H_
#define __UI_MANAGER_H_

#include <string>
#include <list>

#define NAV_COUNT 68
#define NAV_MAX 64
#define NAV_NEW 65
#define NAV_MENU 66
#define NAV_TRANS 67

#include "SourceList.h"
#include "InfoVisitor.h"
#include "DialogToolkit.h"
#include "SessionParser.h"


struct ImVec2;
class MediaPlayer;
class FrameBufferImage;
class FrameGrabber;
class VideoRecorder;

class SourcePreview {

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

class Thumbnail
{
    float aspect_ratio_;
    uint texture_;

public:
    Thumbnail();
    ~Thumbnail();

    void reset();
    void fill (const FrameBufferImage *image);
    bool filled();
    void Render(float width);
};

class Navigator
{
    // geometry left bar & pannel
    float width_;
    float height_;
    float pannel_width_;
    float padding_width_;

    // behavior pannel
    bool show_config_;
    bool pannel_visible_;
    bool view_pannel_visible;
    bool selected_button[NAV_COUNT];
    int  pattern_type;
    void clearButtonSelection();
    void applyButtonSelection(int index);

    // side pannels
    void RenderSourcePannel(Source *s);
    void RenderMainPannel();
    void RenderMainPannelVimix();
    void RenderMainPannelSettings();
    void RenderTransitionPannel();
    void RenderNewPannel();
    void RenderViewPannel(ImVec2 draw_pos, ImVec2 draw_size);

public:
    Navigator();

    bool pannelVisible() { return pannel_visible_; }
    void hidePannel();
    void showPannelSource(int index);
    void togglePannelMenu();
    void togglePannelNew();
    void showConfig();

    typedef enum {
        MEDIA_RECENT = 0,
        MEDIA_RECORDING,
        MEDIA_FOLDER
    } MediaCreateMode;
    void setNewMedia(MediaCreateMode mode, std::string path = std::string());

    void Render();

private:
    // for new source panel
    typedef enum {
        SOURCE_FILE = 0,
        SOURCE_SEQUENCE,
        SOURCE_CONNECTED,
        SOURCE_GENERATED,
        SOURCE_INTERNAL,
        SOURCE_TYPES
    } NewSourceType;
    SourcePreview new_source_preview_;
    std::list<std::string> sourceSequenceFiles;
    std::list<std::string> sourceMediaFiles;
    std::string sourceMediaFileCurrent;
    MediaCreateMode new_media_mode;
    bool new_media_mode_changed;
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

class HelperToolbox
{
    SessionParser parser_;

public:
    HelperToolbox();

    void Render();

};

class SourceController
{
    bool focused_;
    float min_width_;
    float h_space_;
    float v_space_;
    float scrollbar_;
    float timeline_height_;
    float mediaplayer_height_;
    float buttons_width_;
    float buttons_height_;

    bool play_toggle_request_, replay_request_;
    bool pending_;
    std::string active_label_;
    int active_selection_;
    InfoVisitor info_;
    SourceList selection_;

    // re-usable ui parts
    void DrawButtonBar(ImVec2 bottom, float width);
    const char *SourcePlayIcon(Source *s);
    bool SourceButton(Source *s, ImVec2 framesize);

    // Render the sources dynamically selected
    void RenderSelectedSources();

    // Render a stored selection
    bool selection_context_menu_;
    MediaPlayer *selection_mediaplayer_;
    double selection_target_slower_;
    double selection_target_faster_;
    void RenderSelectionContextMenu();
    void RenderSelection(size_t i);

    // Render a single source
    void RenderSingleSource(Source *s);

    // Render a single media player
    MediaPlayer *mediaplayer_active_;
    bool mediaplayer_edit_fading_;
    bool mediaplayer_mode_;
    bool mediaplayer_slider_pressed_;
    float mediaplayer_timeline_zoom_;
    void RenderMediaPlayer(MediaPlayer *mp);

public:
    SourceController();

    inline void Play()   { play_toggle_request_  = true; }
    inline void Replay() { replay_request_= true; }
    void Update();

    void resetActiveSelection();
    void Render();
    bool Visible() const;
    inline bool Foccused() const { return focused_; }
};


class UserInterface
{
    friend class Navigator;
    Navigator navigator;
    ToolBox toolbox;
    SourceController sourcecontrol;
    HelperToolbox sessiontoolbox;

    uint64_t start_time;
    bool ctrl_modifier_active;
    bool alt_modifier_active;
    bool shift_modifier_active;
    bool show_vimix_about;
    bool show_imgui_about;
    bool show_gst_about;
    bool show_opengl_about;
    int  show_view_navigator;
    int  target_view_navigator;
    unsigned int screenshot_step;

    // frame grabbers
    VideoRecorder *video_recorder_;

#if defined(LINUX)
    FrameGrabber *webcam_emulator_;
#endif

    // Dialogs
    DialogToolkit::OpenSessionDialog *sessionopendialog;
    DialogToolkit::OpenSessionDialog *sessionimportdialog;
    DialogToolkit::SaveSessionDialog *sessionsavedialog;

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
    bool Init();
    // loop update start new frame
    void NewFrame();
    // loop update rendering
    void Render();
    // Post-loop termination
    void Terminate();
    // Runtime
    uint64_t Runtime() const;

    // status querries
    inline bool ctrlModifier() const { return ctrl_modifier_active; }
    inline bool altModifier() const  { return alt_modifier_active; }
    inline bool shiftModifier() const  { return shift_modifier_active; }

    void showPannel(int id = 0);
    void showSourceEditor(Source *s);

    // TODO implement the shader editor
    std::string currentTextEdit;
    void fillShaderEditor(const std::string &text);

    void StartScreenshot();
    inline bool isRecording() const { return video_recorder_ != nullptr; }

protected:

    void showMenuFile();
    void showMenuEdit();
    bool saveOrSaveAs(bool force_versioning = false);
    void selectSaveFilename();
    void selectOpenFilename();

    void RenderMetrics (bool* p_open, int* p_corner, int *p_mode);
    void RenderPreview();
    void RenderTimer();
    void RenderShaderEditor();
    int  RenderViewNavigator(int* shift);
    void RenderAbout(bool* p_open);
    void RenderNotes();

    void handleKeyboard();
    void handleMouse();
    void handleScreenshot();
};

#endif /* #define __UI_MANAGER_H_ */

