#ifndef __UI_MANAGER_H_
#define __UI_MANAGER_H_

#include <string>
#include <list>
using namespace std;

#define NAV_MENU 0
#define NAV_NEW 63
#define NAV_MAX 64

class SourceNavigator
{
    int selected_source_index;
    bool selected_button[NAV_MAX];
    void clearSelection();
    void toggle(int index);

public:
    SourceNavigator();

    void Render();
};

class MainWindow
{
    bool show_app_about;
    bool show_gst_about;
    bool show_opengl_about;
    bool show_demo_window;
//    bool show_logs_window;
    bool show_icons_window;
    unsigned int screenshot_step;

public:
    MainWindow();

    void ToggleLogs();
    void StartScreenshot();
    void Render();
};

class UserInterface
{
    friend class MainWindow;
    MainWindow mainwindow;
    SourceNavigator navigator;
    std::string currentTextEdit;

    void handleKeyboard();
    void handleMouse();

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

