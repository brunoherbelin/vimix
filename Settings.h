#ifndef __SETTINGS_H_
#define __SETTINGS_H_

#include "defines.h"

#include <string>
#include <map>
#include <vector>
#include <list>
#include <glm/glm.hpp>

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
    glm::vec3 default_scale;
    glm::vec3 default_translation;

    ViewConfig() : name("") {
        default_scale = glm::vec3(1.f);
        default_translation = glm::vec3(0.f);
    }

};

struct History
{
    std::list<std::string> filenames;
    bool automatic;

    History() {
        automatic = false;
    }
    void push(std::string filename) {
        filenames.remove(filename);
        filenames.push_front(filename);
        if (filenames.size() > MAX_RECENT_HISTORY)
            filenames.pop_back();
    }
};

struct Application
{
    // Verification
    std::string name;

    // Global settings Application interface
    float scale;
    int  accent_color;
    bool stats;
    int  stats_corner;
    bool logs;
    bool preview;
    bool media_player;
    bool shader_editor;
    bool toolbox;

    // Settings of Views
    int current_view;
    std::map<int, ViewConfig> views;

    // multiple windows handling
    // TODO: manage other windows
    std::vector<WindowConfig> windows;

    // recent files histories
    History recentSessions;
    History recentMedia;

    Application() : name(APP_NAME){
        scale = 1.f;
        accent_color = 0;
        stats = false;
        stats_corner = 1;
        logs = false;
        preview = false;
        media_player = false;
        shader_editor = false;
        toolbox = false;
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
