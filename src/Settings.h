#ifndef __SETTINGS_H_
#define __SETTINGS_H_

#include <sys/types.h>
#include <string>
#include <map>
#include <vector>
#include <list>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

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
    bool media_player_timeline_editmode;
    float media_player_slider;
    bool timer;
    int  timer_view;
    bool inputs;
    int  inputs_view;
    bool shader_editor;
    int  shader_editor_view;
    bool toolbox;
    bool help;
    bool source_toolbar;
    int  source_toolbar_border;
    int  source_toolbar_mode;

    WidgetsConfig() {
        stats = false;
        stats_mode = 0;
        stats_corner = 1;
        logs = false;
        preview = false;
        preview_view = -1;
        media_player = false;
        media_player_view = -1;
        media_player_timeline_editmode = false;
        media_player_slider = 0.f;
        toolbox = false;
        help = false;
        timer = false;
        timer_view = -1;
        shader_editor = false;
        shader_editor_view = -1;
        inputs = false;
        inputs_view = -1;
        source_toolbar = false;
        source_toolbar_mode = 0;
        source_toolbar_border = -1;
    }
};

struct WindowConfig
{
    std::string name;
    int x,y,w,h;
    bool fullscreen;
    bool custom;
    bool decorated;
    std::string monitor;
    bool show_pattern;
    glm::vec4 whitebalance;
    glm::mat4 nodes;

    WindowConfig() : name(APP_TITLE), x(15), y(15), w(1280), h(720),
        fullscreen(false), custom(false), decorated(true),
        monitor(""), show_pattern(false), whitebalance(glm::vec4(1.f, 1.f, 1.f, 0.5f)),
        nodes(glm::zero<glm::mat4>())
    { }

};

struct ViewConfig
{
    std::string name;
    glm::vec3 default_scale;
    glm::vec3 default_translation;
    bool ignore_mix;

    ViewConfig() : name("") {
        default_scale = glm::vec3(1.f);
        default_translation = glm::vec3(0.f);
        ignore_mix = false;
    }

};


struct RecordConfig
{
    std::string path;
    int profile;
    uint timeout;
    int delay;
    int resolution_mode;
    int framerate_mode;
    int buffering_mode;
    int priority_mode;
    int naming_mode;
    std::string audio_device;

    RecordConfig() : path("") {
        profile = 0;
        timeout = RECORD_MAX_TIMEOUT;
        delay = 0;
        resolution_mode = 1;
        framerate_mode = 1;
        buffering_mode = 2;
        priority_mode = 1;
        naming_mode = 1;
        audio_device = "";
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
    int  ordering;

    History() {
        path = "";
        front_is_valid = false;
        load_at_start = true;
        save_on_exit = true;
        changed = false;
        ordering = 3;
    }
    void push   (const std::string &filename);
    void remove (const std::string &filename);
    void assign (const std::string &filename);
    void validate();
};

struct KnownHosts
{
    std::string protocol;
    std::pair<std::string, std::string> default_host;
    std::list< std::pair<std::string, std::string> > hosts;

    KnownHosts() {}

    void push(const std::string &ip, const std::string &port = "");
    void remove(const std::string &ip);
};

struct TransitionConfig
{
    bool cross_fade;
    float duration;
    int profile;

    TransitionConfig() {
        cross_fade = true;
        duration = 1.f;
        profile = 0;
    }
};

struct RenderConfig
{
    bool disabled;
    int vsync;
    int multisampling;
    int ratio;
    int res;
    int custom_width, custom_height;
    float fading;
    bool gpu_decoding;
    bool gpu_decoding_available;

    RenderConfig() {
        disabled = false;
        vsync = 1;
        multisampling = 3;
        ratio = 3;
        res = 1;
        custom_width = 1200;
        custom_height = 600;
        fading = 0.0;
        gpu_decoding = true;
        gpu_decoding_available = false;
    }
};

struct SourceConfig
{
    int new_type;
    int ratio;
    int res;
    std::string capture_path;
    int capture_naming;
    float inspector_zoom;

    SourceConfig() {
        new_type = 0;
        ratio = 3;
        res = 6;
        capture_naming = 0;
        inspector_zoom = 8.f;
    }
};

struct TimerConfig
{
    uint64_t mode;
    bool link_enabled;
    double link_tempo;
    double link_quantum;
    bool link_start_stop_sync;
    uint64_t stopwatch_duration;

    TimerConfig() {
        mode = 0;
        link_enabled = true;
        link_tempo = 120.;
        link_quantum = 4.;
        link_start_stop_sync = true;
        stopwatch_duration = 60;
    }
};

struct InputMappingConfig
{
    uint64_t mode;
    uint current;
    bool disabled;

    InputMappingConfig() {
        mode = 0;
        current = 0;
        disabled = false;
    }
};

struct ControllerConfig
{
    int osc_port_receive;
    int osc_port_send;
    std::string osc_filename;

    ControllerConfig() {
        osc_port_receive = OSC_PORT_RECV_DEFAULT;
        osc_port_send = OSC_PORT_SEND_DEFAULT;
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
    uint64_t total_runtime;

    // Global settings Application interface
    float scale;
    int  accent_color;
    bool save_version_snapshot;
    bool smooth_transition;
    bool proportional_grid;
    int  mouse_pointer;
    bool mouse_pointer_lock;
    std::vector<float> mouse_pointer_strength;
    bool action_history_follow_view;
    bool show_tooptips;

    int  pannel_main_mode;
    int  pannel_playlist_mode;
    int  pannel_current_session_mode;
    bool pannel_always_visible;

    // connection settings
    bool accept_connections;
    int stream_protocol;
    int broadcast_port;
    KnownHosts recentSRT;
    int loopback_camera;
    int shm_method;
    std::string shm_socket_path;

    // audio
    bool accept_audio;

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

    // settings controller
    ControllerConfig control;

    // multiple windows handling
    int num_output_windows;
    std::vector<WindowConfig> windows;

    // recent files histories
    History recentSessions;
    History recentPlaylists;
    History recentFolders;
    History recentImport;
    History recentImportFolders;
    History recentRecordings;
    std::map< std::string, std::string > dialogRecentFolder;

    // Metronome & stopwatch
    TimerConfig timer;

    // Inputs mapping (callbacks)
    InputMappingConfig mapping;

    Application() : fresh_start(false), instance_id(0), name(APP_NAME), executable(APP_NAME) {
        scale = 1.f;
        accent_color = 0;
        smooth_transition = true;
        save_version_snapshot = false;
        proportional_grid = true;
        mouse_pointer = 1;
        mouse_pointer_lock = false;
        mouse_pointer_strength = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
        action_history_follow_view = false;
        show_tooptips = true;
        accept_connections = false;
        stream_protocol = 0;
        broadcast_port = 7070;
        recentSRT.protocol = "srt://";
        recentSRT.default_host = { "127.0.0.1", "7070"};
        loopback_camera = 0;
        shm_method = 0;
        shm_socket_path = "";
        pannel_main_mode = 0;
        pannel_playlist_mode = 0;
        pannel_current_session_mode = 0;
        current_view = 1;
        current_workspace= 3;
        brush = glm::vec3(0.5f, 0.1f, 0.f);
        num_output_windows = 1;
        windows = std::vector<WindowConfig>(1+MAX_OUTPUT_WINDOW);
        windows[0].w = 1600;
        windows[0].h = 930;
        accept_audio = false;
    }

};


// minimal implementation of settings
// Can be accessed r&w anywhere
extern Application application;

// Save and Load store settings in XML file
void Save(uint64_t runtime = 0);
void Load();
void Lock();
void Unlock();
void Check();

}

#endif /* __SETTINGS_H_ */
