#ifndef __UI_MANAGER_H_
#define __UI_MANAGER_H_

#include <string>
#include <list>

#define NAV_COUNT 68
#define NAV_MAX 64
#define NAV_NEW 65
#define NAV_MENU 66
#define NAV_TRANS 67

struct ImVec2;
class Source;
class MediaPlayer;

class SourcePreview {

    Source *source_;
    std::string label_;

public:
    SourcePreview();

    void setSource(Source *s = nullptr, std::string label = "");
    Source *getSource();

    void Render(float width, bool controlbutton = false);
    bool ready() const;
    inline bool filled() const { return source_ != nullptr; }
};

class Navigator
{
    // geometry left bar & pannel
    float width_;
    float height_;
    float pannel_width_;
    float sourcelist_height_;
    float padding_width_;

    // behavior pannel
    bool pannel_visible_;
    bool view_pannel_visible;
    bool selected_button[NAV_COUNT];
    int  pattern_type;
    void clearButtonSelection();
    void applyButtonSelection(int index);

    // side pannels
    void RenderSourcePannel(Source *s);
    void RenderMainPannel();
    void RenderTransitionPannel();
    void RenderNewPannel();
    void RenderViewPannel(ImVec2 draw_pos, ImVec2 draw_size);

    SourcePreview new_source_preview_;

public:
    Navigator();

    bool pannelVisible() { return pannel_visible_; }
    void hidePannel();
    void showPannelSource(int index);
    void togglePannelMenu();
    void togglePannelNew();


    void Render();
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


class MediaController
{
    MediaPlayer *mp_;
    std::string current_;
    bool follow_active_source_;
    bool media_playing_mode_;
    bool slider_pressed_;

public:
    MediaController();

    void setMediaPlayer(MediaPlayer *mp = nullptr);
    void followCurrentSource();

    void Render();
};

class UserInterface
{
    friend class Navigator;
    Navigator navigator;
    ToolBox toolbox;
    MediaController mediacontrol;

    bool ctrl_modifier_active;
    bool alt_modifier_active;
    bool shift_modifier_active;
    bool show_vimix_config;
    bool show_imgui_about;
    bool show_gst_about;
    bool show_opengl_about;
    unsigned int screenshot_step;

    // frame grabbers
    uint64_t video_recorder_;
    uint64_t video_streamer_;

    // Private Constructor
    UserInterface();
    UserInterface(UserInterface const& copy);            // Not Implemented
    UserInterface& operator=(UserInterface const& copy); // Not Implemented

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

    // status querries
    inline bool ctrlModifier() const { return ctrl_modifier_active; }
    inline bool altModifier() const  { return alt_modifier_active; }
    inline bool shiftModifier() const  { return shift_modifier_active; }

    void StartScreenshot();
    void showPannel(int id = 0);

    void showMediaPlayer(MediaPlayer *mp);

    // TODO implement the shader editor
    std::string currentTextEdit;
    void fillShaderEditor(std::string text);

protected:

    void showMenuFile();
    void selectSaveFilename();
    void selectOpenFilename();

    void RenderPreview();
    void RenderHistory();
    void RenderShaderEditor();
    void handleKeyboard();
    void handleMouse();
    void handleScreenshot();
};

#endif /* #define __UI_MANAGER_H_ */

