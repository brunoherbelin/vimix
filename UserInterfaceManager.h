#ifndef __UI_MANAGER_H_
#define __UI_MANAGER_H_

#include <string>
#include <list>
using namespace std;

#define NAV_COUNT 67
#define NAV_MAX 64
#define NAV_NEW 65
#define NAV_MENU 66

struct ImVec2;
class Source;

class SourcePreview {

    Source *source_;
    std::string label_;

public:
    SourcePreview();

    void setSource(Source *s = nullptr, std::string label = "");
    Source *getSource();

    void draw(float width);
    inline bool ready() const { return source_ != nullptr; }
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
    bool pannel_stick_;
    bool selected_button[NAV_COUNT];
    void clearButtonSelection();
    void applyButtonSelection(int index);

    // side pannels
    void RenderSourcePannel(Source *s);
    void RenderMainPannel();

    void RenderNewPannel();
    int new_source_type_;
    char new_source_filename_[2048];

    SourcePreview new_source_preview_;

public:
    Navigator();

    bool pannelVisible() { return pannel_visible_; }
    void hidePannel();
    void showPannelSource(int index);
    void toggleMenu();

    void Render();
};

class ToolBox
{
    bool show_demo_window;
    bool show_icons_window;

public:
    ToolBox();

    void Render();
};


class UserInterface
{
    friend class Navigator;
    Navigator navigator;
    ToolBox toolbox;

    bool keyboard_modifier_active;
    bool show_about;
    bool show_imgui_about;
    bool show_gst_about;
    bool show_opengl_about;
    unsigned int screenshot_step;


//    typedef enum {
//        FILE_DIALOG_INACTIVE = 0,
//        FILE_DIALOG_ACTIVE,
//        FILE_DIALOG_FINISHED
//    } FileDialogStatus;
//    FileDialogStatus filestatus_;
//    std::string filename_;
//    void startOpenFileDialog();

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
    inline bool keyboardModifier() { return keyboard_modifier_active; }

    void StartScreenshot();

    // TODO implement the shader editor
    std::string currentTextEdit;
    void fillShaderEditor(std::string text);

protected:

    void showMenuFile();
    void RenderPreview();
    void RenderMediaPlayer();
    void RenderShaderEditor();
    void handleKeyboard();
    void handleMouse();
    void handleScreenshot();

};

#endif /* #define __UI_MANAGER_H_ */

