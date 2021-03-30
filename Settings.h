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
    bool stats_timer;
    bool logs;
    bool preview;
    bool history;
    bool media_player;
    bool media_player_view;
    bool shader_editor;
    bool toolbox;

    WidgetsConfig() {
        stats = false;
        stats_timer = false;
        stats_corner = 1;
        logs = false;
        preview = false;
        history = false;
        media_player = false;
        media_player_view = true;
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

    WindowConfig() : name(""), x(15), y(15), w(1280), h(720), fullscreen(false), monitor("") { }

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

#define RECORD_MAX_TIMEOUT 1800.f

struct RecordConfig
{
    std::string path;
    int profile;
    float timeout;

    RecordConfig() : path("") {
        profile = 0;
        timeout = RECORD_MAX_TIMEOUT;
    }

};

struct History
{
    std::string path;
    std::list<std::string> filenames;
    bool front_is_valid;
    bool load_at_start;
    bool save_on_exit;
    bool changed;

    History() {
        path = IMGUI_LABEL_RECENT_FILES;
        front_is_valid = false;
        load_at_start = false;
        save_on_exit = false;
        changed = false;
    }
    void push(const std::string &filename) {
        if (filename.empty()) {
            front_is_valid = false;
            return;
        }
        filenames.remove(filename);
        filenames.push_front(filename);
        if (filenames.size() > MAX_RECENT_HISTORY)
            filenames.pop_back();
        front_is_valid = true;
        changed = true;
    }
    void remove(const std::string &filename) {
        if (filename.empty())
            return;
        if (filenames.front() == filename)
            front_is_valid = false;
        filenames.remove(filename);
        changed = true;
    }
};

struct TransitionConfig
{
    bool cross_fade;
    bool hide_windows;
    float duration;
    int profile;

    TransitionConfig() {
        cross_fade = true;
        hide_windows = true;
        duration = 1.f;
        profile = 0;
    }
};

struct RenderConfig
{
    bool blit;
    int vsync;
    int multisampling;
    int ratio;
    int res;
    float fading;
    bool gpu_decoding;

    RenderConfig() {
        blit = false;
        vsync = 1;
        multisampling = 2;
        ratio = 3;
        res = 1;
        fading = 0.0;
        gpu_decoding = true;
    }
};

struct SourceConfig
{
    int new_type;
    int ratio;
    int res;

    SourceConfig() {
        new_type = 0;
        ratio = 3;
        res = 1;
    }
};


struct Application
{
    // instance check
    bool fresh_start;
    int instance_id;

    // Verification
    std::string name;
    std::string executable;

    // Global settings Application interface
    float scale;
    int  accent_color;
    bool pannel_stick;
    bool smooth_transition;
    bool smooth_cursor;
    bool action_history_follow_view;

    // connection settings
    bool accept_connections;
//    std::map<int, std::string> instance_names;

    // Settings of widgets
    WidgetsConfig widget;

    // Settings of Views
    int current_view;
    int current_workspace;
    std::map<int, ViewConfig> views;

    // settings brush texture paint
    glm::vec3 brush;

    // settings render
    RenderConfig render;

    // settings exporters
    RecordConfig record;

    // settings new source
    SourceConfig source;

    // settings transition
    TransitionConfig transition;

    // multiple windows handling
    std::vector<WindowConfig> windows;

    // recent files histories
    History recentSessions;
    History recentFolders;
    History recentImport;

    Application() : fresh_start(false), instance_id(0), name(APP_NAME), executable(APP_NAME) {
        scale = 1.f;
        accent_color = 0;
        pannel_stick = false;
        smooth_transition = true;
        smooth_cursor = false;
        action_history_follow_view = false;
        accept_connections = false;
        current_view = 1;
        current_workspace= 1;
        brush = glm::vec3(0.5f, 0.1f, 0.f);
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
void Lock();
void Unlock();
void Check();

}

#endif /* __SETTINGS_H_ */
