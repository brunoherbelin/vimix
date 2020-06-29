#ifndef __SETTINGS_H_
#define __SETTINGS_H_

#include "defines.h"

#include <string>
#include <map>
#include <vector>
#include <list>
#include <glm/glm.hpp>

namespace Settings {

struct WidgetsConfig
{
    bool stats;
    int  stats_corner;
    bool logs;
    bool preview;
    bool media_player;
    bool shader_editor;
    bool toolbox;

    WidgetsConfig() {
        stats = false;
        stats_corner = 1;
        logs = false;
        preview = false;
        media_player = false;
        shader_editor = false;
        toolbox = false;
    }
};

struct WindowConfig
{
    std::string name;
    int x,y,w,h;
    bool fullscreen;
    std::string monitor;

    WindowConfig() : name(""), x(15), y(15), w(1280), h(720), monitor(""), fullscreen(false) { }

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
    std::string path;
    std::list<std::string> filenames;
    bool load_at_start;
    bool save_on_exit;

    History() {
        load_at_start = false;
        save_on_exit = false;
    }
    void push(std::string filename) {
        if (filename.empty()) return;
        filenames.remove(filename);
        filenames.push_front(filename);
        if (filenames.size() > MAX_RECENT_HISTORY)
            filenames.pop_back();
    }
};

struct RenderConfig
{
    int vsync;
    int multisampling;
    bool blit;

    RenderConfig() {
        vsync = 1; // todo GUI selection
        multisampling = 2; // todo GUI selection
        blit = false;
    }
};

struct Application
{
    // Verification
    std::string name;

    // Global settings Application interface
    float scale;
    int  accent_color;
    bool pannel_stick;

    // Settings of widgets
    WidgetsConfig widget;

    // Settings of Views
    int current_view;
    int render_view_ar;
    int render_view_h;
    std::map<int, ViewConfig> views;

    // settings render
    RenderConfig render;

    // multiple windows handling
    std::vector<WindowConfig> windows;

    // recent files histories
    History recentSessions;
    History recentImport;

    Application() : name(APP_NAME){
        scale = 1.f;
        accent_color = 0;
        pannel_stick = false;

        current_view = 1;
        render_view_ar = 3;
        render_view_h = 1;

        windows = std::vector<WindowConfig>(3);
        windows[0].name = APP_NAME APP_TITLE;
        windows[0].w = 1600;
        windows[0].h = 900;
        windows[1].name = APP_NAME " -- Output";
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
