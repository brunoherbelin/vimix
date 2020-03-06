#ifndef __UI_MANAGER_H_
#define __UI_MANAGER_H_

#include <string>
#include <list>
using namespace std;

class MessageBox 
{
public:
    list<string> messages;

    MessageBox();
    void Append(const string& text);
    void Render(float width);
};

class MainWindow
{
    bool show_overlay_stats;
    bool show_app_about;
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

    // own implementation of ImGui 
    static unsigned int textureicons;
    static MainWindow mainwindow;
    static MessageBox warnings;
    static std::string currentFileDialog;
    static std::string currentTextEdit;

    static void handleKeyboard();
    static void handleMouse();

public:

    // pre-loop initialization
    static bool Init();
    // loop update start new frame
    static void NewFrame();
    // loop update rendering
    static void Render();
    // Post-loop termination
    static void Terminate();
    // Boxer integration in OS of Error messages
    static void Error(const string& text, const string& title = std::string());
    // ImGui modal dialog for Warning messages
    static void Warning(const char* fmt, ...);
    // Open an Open File dialog for TEXT file 
    static void OpenFileText();
    // Open an Open File dialog for IMAGE file 
    static void OpenFileImage();
    // Open an Open File dialog for MEDIA file 
    static void OpenFileMedia();
    
    static void OpenTextEditor(std::string text);
};

#endif /* #define __UI_MANAGER_H_ */

