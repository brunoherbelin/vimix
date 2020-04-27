#ifndef __UI_MANAGER_H_
#define __UI_MANAGER_H_

#include <string>
#include <list>
using namespace std;

class MainWindow
{
    bool show_overlay_stats;
    bool show_app_about;
    bool show_gst_about;
    bool show_opengl_about;
    bool show_demo_window;
    bool show_logs_window;
    bool show_icons_window;
    bool show_editor_window;
    unsigned int screenshot_step;

    void ShowStats(bool* p_open);
    void drawTextEditor(bool* p_open);

public:
    MainWindow();

    void ToggleLogs();
    void ToggleStats();
    void StartScreenshot();
    void Render();
};

class UserInterface
{
    friend class MainWindow;
    MainWindow mainwindow;
    std::string currentFileDialog;
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

    // Open an Open File dialog for TEXT file 
    void OpenFileText();
    // Open an Open File dialog for IMAGE file 
    void OpenFileImage();
    // Open an Open File dialog for MEDIA file 
    void OpenFileMedia();

    //
    void OpenTextEditor(std::string text);

protected:
    bool show_preview;
    void RenderPreview();
    bool show_media_player;
    void RenderMediaPlayer();

};

#endif /* #define __UI_MANAGER_H_ */

