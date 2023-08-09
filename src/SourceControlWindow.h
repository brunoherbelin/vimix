#ifndef SOURCECONTROLWINDOW_H
#define SOURCECONTROLWINDOW_H

struct ImVec2;

#include "SourceList.h"
#include "InfoVisitor.h"
#include "DialogToolkit.h"
#include "Screenshot.h"
#include "WorkspaceWindow.h"

class SourceControlWindow : public WorkspaceWindow
{
    float min_width_;
    float h_space_;
    float v_space_;
    float scrollbar_;
    float timeline_height_;
    float mediaplayer_height_;
    float buttons_width_;
    float buttons_height_;

    bool play_toggle_request_, replay_request_, capture_request_;
    bool pending_;
    std::string active_label_;
    int active_selection_;
    InfoVisitor info_;
    SourceList selection_;

    // re-usable ui parts
    void DrawButtonBar(ImVec2 bottom, float width);
    int SourceButton(Source *s, ImVec2 framesize);

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
    bool mediaplayer_set_duration_;
    bool mediaplayer_edit_pipeline_;
    bool mediaplayer_mode_;
    bool mediaplayer_slider_pressed_;
    float mediaplayer_timeline_zoom_;
    void RenderMediaPlayer(MediaSource *ms);

    // magnifying glass
    bool magnifying_glass;

    // dialog to select frame capture location
    DialogToolkit::OpenFolderDialog *captureFolderDialog;
    Screenshot capture;

public:
    SourceControlWindow();

    inline void Play()   { play_toggle_request_  = true; }
    inline void Replay() { replay_request_= true; }
    inline void Capture(){ capture_request_= true; }
    void resetActiveSelection();

    void setVisible(bool on);
    void Render();

    // from WorkspaceWindow
    void Update() override;
    bool Visible() const override;
};


#endif // SOURCECONTROLWINDOW_H
