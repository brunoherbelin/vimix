#ifndef __SETTINGS_H_
#define __SETTINGS_H_

#include "defines.h"

#include <string>
#include <vector>

namespace Settings {

struct WindowConfig
{
    std::string name;
    int x,y,w,h;
    bool fullscreen;

    WindowConfig(std::string n) : name(n), x(15), y(15), w(1280), h(720), fullscreen(false) { }

};

struct ViewConfig
{
    std::string name;

    ViewConfig(std::string n) : name(n) { }

};

struct Application
{
    // Verification
    std::string name;

    // Global settings Application interface
    float scale;
    int accent_color;
    bool preview;
    bool media_player;
    bool shader_editor;

    // Settings of Views
    int current_view;
    std::vector<ViewConfig> views;

    // multiple windows handling
    std::vector<WindowConfig> windows;

    Application() : name(APP_NAME){
        scale = 1.f;
        accent_color = 0;
        preview = true;
        media_player = false;
        shader_editor = false;
        current_view = 1;
        windows.push_back(WindowConfig(APP_TITLE));
    }

};


// minimal implementation of settings
// Can be accessed r&w anywhere
extern Application application;

// Save and Load store settings in XML file
void Save();
void Load();
void Check();

}

#endif /* __SETTINGS_H_ */
