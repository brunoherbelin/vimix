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

class Navigator
{
    // geometry left bar
    float width;
    float pannel_width;
    float height;
    float sourcelist_height;
    float padding_width;

    // top items : group of buttons openning a pannel
    int selected_source_index;
    bool selected_button[NAV_COUNT];
    void clearSelection();
    void toggle(int index);

    // side pannels
    void RenderSourcePannel(Source *s);
    void RenderNewPannel();
    void RenderMainPannel();

public:
    Navigator();

    void toggleMenu();
    void hidePannel();
    void showPannelSource(int index);
    void Render();

};

class ToolBox
{
    bool show_demo_window;
    bool show_icons_window;
    unsigned int screenshot_step;

public:
    ToolBox();

    void StartScreenshot();
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

};

#endif /* #define __UI_MANAGER_H_ */

