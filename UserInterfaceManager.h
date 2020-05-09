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

// TODO: class ShaderEditor

class UserInterface
{
    friend class Navigator;
    Navigator navigator;
    ToolBox toolbox;

    bool show_about;
    bool show_imgui_about;
    bool show_gst_about;
    bool show_opengl_about;

    std::string currentTextEdit;

    void handleKeyboard();
    void handleMouse();
    void showMenuFile();

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
    //
    void fillShaderEditor(std::string text);

protected:

    void RenderPreview();
    void RenderMediaPlayer();
    void RenderShaderEditor();

};

#endif /* #define __UI_MANAGER_H_ */

