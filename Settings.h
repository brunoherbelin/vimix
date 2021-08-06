#ifndef __SETTINGS_H_
#define __SETTINGS_H_

#ifdef __APPLE__
#include <sys/types.h>
#endif

#include <string>
#include <map>
#include <vector>
#include <list>
#include <glm/glm.hpp>

#include "defines.h"

namespace Settings {

struct WidgetsConfig
{
    bool stats;
    int  stats_corner;
    int  stats_mode;
    bool logs;
    bool preview;
    int  preview_view;
    bool media_player;
    int  media_player_view;
    bool timeline_editmode;
    bool shader_editor;
    bool toolbox;
    bool history;

    WidgetsConfig() {
        stats = false;
        stats_mode = 0;
        stats_corner = 1;
        logs = false;
        preview = false;
        preview_view = -1;
        history = false;
        media_player = false;
        media_player_view = -1;
        timeline_editmode = false;
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


struct RecordConfig
{
    std::string path;
    int profile;
    uint timeout;
    int delay;

    RecordConfig() : path("") {
        profile = 0;
        timeout = RECORD_MAX_TIMEOUT;
        delay = 0;
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
    void push(const std::string &filename);
    void remove(const std::string &filename);
    void validate();
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
    bool smooth_snapshot;
    bool smooth_transition;
    bool smooth_cursor;
    bool action_history_follow_view;

    int  pannel_history_mode;

    // connection settings
    bool accept_connections;

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
    std::map< std::string, std::string > dialogRecentFolder;

    Application() : fresh_start(false), instance_id(0), name(APP_NAME), executable(APP_NAME) {
        scale = 1.f;
        accent_color = 0;
        smooth_transition = false;
        smooth_snapshot = false;
        smooth_cursor = false;
        action_history_follow_view = false;
        accept_connections = false;
        pannel_history_mode = 0;
        current_view = 1;
        current_workspace= 1;
        brush = glm::vec3(0.5f, 0.1f, 0.f);
        windows = std::vector<WindowConfig>(3);
        windows[0].name = APP_TITLE;
        windows[0].w = 1600;
        windows[0].h = 900;
        windows[1].name = "Output " APP_TITLE;
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
