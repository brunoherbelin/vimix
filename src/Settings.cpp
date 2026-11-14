/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2023 Bruno Herbelin <bruno.herbelin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
**/


#include <cstddef>
#include <tinyxml2.h>
#include "Toolkit/tinyxml2Toolkit.h"
using namespace tinyxml2;

#include "defines.h"
#include "Toolkit/SystemToolkit.h"
#include "Settings.h"


Settings::Application Settings::application;
std::string settingsFilename = "";


XMLElement *save_history(Settings::History &h, const char *nodename, XMLDocument &xmlDoc)
{
    XMLElement *pElement = xmlDoc.NewElement( nodename );
    pElement->SetAttribute("path", h.path.c_str());
    pElement->SetAttribute("autoload", h.load_at_start);
    pElement->SetAttribute("autosave", h.save_on_exit);
    pElement->SetAttribute("valid", h.front_is_valid);
    pElement->SetAttribute("ordering", h.ordering);
    for(auto it = h.filenames.cbegin();
        it != h.filenames.cend(); ++it) {
        XMLElement *fileNode = xmlDoc.NewElement("path");
        XMLText *text = xmlDoc.NewText( (*it).c_str() );
        fileNode->InsertEndChild( text );
        pElement->InsertFirstChild(fileNode);
    }
    return pElement;
}


XMLElement *save_knownhost(Settings::KnownHosts &h, const char *nodename, XMLDocument &xmlDoc)
{
    XMLElement *pElement = xmlDoc.NewElement( nodename );
    pElement->SetAttribute("protocol", h.protocol.c_str());
    for(auto it = h.hosts.cbegin(); it != h.hosts.cend(); ++it) {
        XMLElement *hostNode = xmlDoc.NewElement("host");
        XMLText *text = xmlDoc.NewText( it->first.c_str() );
        hostNode->InsertEndChild( text );
        hostNode->SetAttribute("port", it->second.c_str());
        pElement->InsertFirstChild(hostNode);
    }
    return pElement;
}



XMLElement *save_window(const Settings::WindowConfig &w, XMLDocument &xmlDoc)
{
    XMLElement *node = xmlDoc.NewElement( "MainWindow" );
    node->SetAttribute("x", w.x);
    node->SetAttribute("y", w.y);
    node->SetAttribute("w", w.w);
    node->SetAttribute("h", w.h);
    node->SetAttribute("f", w.fullscreen);
    node->SetAttribute("m", w.monitor.c_str());
    return node;
}

XMLElement *save_widgets(const Settings::WidgetsConfig &w, XMLDocument &xmlDoc)
{
    XMLElement *node = xmlDoc.NewElement( "Widgets" );
    node->SetAttribute("preview", w.preview);
    node->SetAttribute("preview_view", w.preview_view);
    node->SetAttribute("inputs", w.inputs);
    node->SetAttribute("inputs_view", w.inputs_view);
    node->SetAttribute("timer", w.timer);
    node->SetAttribute("timer_view", w.timer_view);
    node->SetAttribute("media_player", w.media_player);
    node->SetAttribute("media_player_view", w.media_player_view);
    node->SetAttribute("timeline_editmode", w.media_player_timeline_editmode);
    node->SetAttribute("timeline_flag", w.media_player_timeline_flag);
    node->SetAttribute("media_player_slider", w.media_player_slider);
    node->SetAttribute("draft_slider", w.draft_slider);
    node->SetAttribute("draft", w.draft);
    node->SetAttribute("draft_corner", w.draft_corner);
    node->SetAttribute("shader_editor", w.shader_editor);
    node->SetAttribute("shader_editor_view", w.shader_editor_view);
    node->SetAttribute("stats", w.stats);
    node->SetAttribute("stats_mode", w.stats_mode);
    node->SetAttribute("stats_corner", w.stats_corner);
    node->SetAttribute("logs", w.logs);
    node->SetAttribute("toolbox", w.toolbox);
    node->SetAttribute("source_toolbar", w.source_toolbar);
    node->SetAttribute("source_toolbar_mode", w.source_toolbar_mode);
    node->SetAttribute("source_toolbar_border", w.source_toolbar_border);
    node->SetAttribute("pannel_always_visible", w.pannel_always_visible);
    return node;
}

void Settings::terminate(uint64_t runtime, const std::string &filename)
{
    // RESET FACTORY DEFAULT BY REMOVING SETTINGS FILE
    if (application.request_factory_reset && filename.empty()) {

        // remove vimix settings file, if exists
        if (!settingsFilename.empty()) 
            SystemToolkit::remove_file(settingsFilename);

        // remove settings file for OSC, if exists
        if (!application.control.osc_filename.empty()) 
            SystemToolkit::remove_file(application.control.osc_filename);

        // remove settings file for IMGUI, if exists
        std::string imguiFilename = SystemToolkit::full_filename(SystemToolkit::settings_path(), "imgui.ini");
        SystemToolkit::remove_file(imguiFilename);

        return;
    }

    // impose C locale for all app
    setlocale(LC_ALL, "C");

    XMLDocument xmlDoc;
    XMLDeclaration *pDec = xmlDoc.NewDeclaration();
    xmlDoc.InsertFirstChild(pDec);

    XMLElement *pRoot = xmlDoc.NewElement(application.name.c_str());
    xmlDoc.InsertEndChild(pRoot);
#ifdef VIMIX_VERSION_MAJOR
    pRoot->SetAttribute("major", VIMIX_VERSION_MAJOR);
    pRoot->SetAttribute("minor", VIMIX_VERSION_MINOR);
    pRoot->SetAttribute("patch", VIMIX_VERSION_PATCH);
#endif

    // runtime
    pRoot->SetAttribute("runtime", runtime + application.total_runtime);

    std::string comment = "Settings for " + application.name;
    XMLComment *pComment = xmlDoc.NewComment(comment.c_str());
    pRoot->InsertEndChild(pComment);

    // main window
    XMLElement *window = save_window(application.mainwindow, xmlDoc);
    window->SetAttribute("name", application.mainwindow.name.c_str());
    pRoot->InsertEndChild(window);

    // save application.monitors MonitorConfig
    XMLElement *monitorsNode = xmlDoc.NewElement( "Monitors" );
    for( auto it = application.monitors.begin(); it != application.monitors.end(); it++ ) {
        XMLElement *monitorNode = xmlDoc.NewElement( "Monitor" );
        monitorNode->SetAttribute("name", (*it).first.c_str());
        monitorNode->SetAttribute("active_output", (*it).second.active_output);
        monitorNode->SetAttribute("brightness", (*it).second.brightness);
        monitorNode->SetAttribute("contrast", (*it).second.contrast);
        monitorNode->SetAttribute("custom_geometry", (*it).second.custom_geometry);

        XMLElement *whitebalanceNode = xmlDoc.NewElement( "whitebalance" );
        whitebalanceNode->InsertEndChild( XMLElementFromGLM(&xmlDoc, (*it).second.whitebalance) );
        monitorNode->InsertEndChild(whitebalanceNode);

        XMLElement *nodesNode = xmlDoc.NewElement( "nodes" );
        nodesNode->InsertEndChild( XMLElementFromGLM(&xmlDoc, (*it).second.nodes) );
        monitorNode->InsertEndChild(nodesNode);

        monitorsNode->InsertEndChild(monitorNode);
    }
    pRoot->InsertEndChild(monitorsNode);

    // General application preferences
    XMLElement *applicationNode = xmlDoc.NewElement( "Application" );
    applicationNode->SetAttribute("scale", application.scale);
    applicationNode->SetAttribute("accent_color", application.accent_color);
    applicationNode->SetAttribute("smooth_transition", application.smooth_transition);
    applicationNode->SetAttribute("draft_duration", application.draft_duration);
    applicationNode->SetAttribute("save_snapshot", application.save_version_snapshot);
    applicationNode->SetAttribute("action_history_follow_view", application.action_history_follow_view);
    applicationNode->SetAttribute("show_tooptips", application.show_tooptips);
    applicationNode->SetAttribute("accept_connections", application.accept_connections);
    applicationNode->SetAttribute("pannel_main_mode", application.pannel_main_mode);
    applicationNode->SetAttribute("pannel_playlist_mode", application.pannel_playlist_mode);
    applicationNode->SetAttribute("pannel_source_0", application.pannel_source[0]);
    applicationNode->SetAttribute("pannel_source_1", application.pannel_source[1]);
    applicationNode->SetAttribute("pannel_source_2", application.pannel_source[2]);
    applicationNode->SetAttribute("pannel_source_3", application.pannel_source[3]);
    applicationNode->SetAttribute("pannel_session_0", application.pannel_session[0]);
    applicationNode->SetAttribute("pannel_session_1", application.pannel_session[1]);
    applicationNode->SetAttribute("pannel_session_2", application.pannel_session[2]);
    applicationNode->SetAttribute("pannel_session_3", application.pannel_session[3]);
    applicationNode->SetAttribute("pannel_settings_0", application.pannel_settings[0]);
    applicationNode->SetAttribute("pannel_settings_1", application.pannel_settings[1]);
    applicationNode->SetAttribute("pannel_settings_2", application.pannel_settings[2]);
    applicationNode->SetAttribute("pannel_settings_3", application.pannel_settings[3]);
    applicationNode->SetAttribute("pannel_settings_4", application.pannel_settings[4]);
    applicationNode->SetAttribute("pannel_settings_5", application.pannel_settings[5]);
    applicationNode->SetAttribute("exclusive_header", application.exclusive_header);
    applicationNode->SetAttribute("pannel_list_all_viewspaces", application.pannel_list_all_viewspaces);
    applicationNode->SetAttribute("stream_protocol", application.stream_protocol);
    applicationNode->SetAttribute("broadcast_port", application.broadcast_port);
    applicationNode->SetAttribute("loopback_camera", application.loopback_camera);
    applicationNode->SetAttribute("shm_socket_path", application.shm_socket_path.c_str());
    applicationNode->SetAttribute("shm_method", application.shm_method);
    applicationNode->SetAttribute("accept_audio", application.accept_audio);
    applicationNode->SetAttribute("media_keys", application.media_keys);

    XMLElement *transcodeNode = xmlDoc.NewElement( "Transcode" );
    transcodeNode->SetAttribute("option_0", application.transcode_options[0]);
    transcodeNode->SetAttribute("option_1", application.transcode_options[1]);
    transcodeNode->SetAttribute("option_2", application.transcode_options[2]);
    transcodeNode->SetAttribute("image_format", application.transcode_image_format);
    transcodeNode->SetAttribute("upscaler", application.transcode_upscaler.c_str());
    applicationNode->InsertEndChild(transcodeNode);

    XMLElement *exportNode = xmlDoc.NewElement( "Export" );
    exportNode->SetAttribute("copy_media", application.export_options[0]);
    exportNode->SetAttribute("archive", application.export_options[1]);
    exportNode->SetAttribute("discard_versions", application.export_options[2]);
    applicationNode->InsertEndChild(exportNode);

    pRoot->InsertEndChild(applicationNode);

    // Widgets
    pRoot->InsertEndChild( save_widgets(application.widget, xmlDoc) );

    // Windows layouts (only those saved by the user)
    XMLElement *layoutsNode = xmlDoc.NewElement( "Layouts" );
    layoutsNode->SetAttribute("start", application.layout_at_start);
    for (int i = 0; i < MAX_WINDOWS_LAYOUT; ++i) {
        const Settings::WindowsLayout &layout = application.layouts[i];
        if (!layout.saved)
            continue;
        XMLElement *layoutNode = xmlDoc.NewElement( "Layout" );
        layoutNode->SetAttribute("index", i);
        layoutNode->InsertEndChild( save_window(layout.mainwindow, xmlDoc) );
        layoutNode->InsertEndChild( save_widgets(layout.widget, xmlDoc) );
        for (auto it = layout.windows.cbegin(); it != layout.windows.cend(); ++it) {
            XMLElement *windowNode = xmlDoc.NewElement( "Window" );
            windowNode->SetAttribute("id", it->first.c_str());
            windowNode->SetAttribute("x", it->second.x);
            windowNode->SetAttribute("y", it->second.y);
            windowNode->SetAttribute("w", it->second.z);
            windowNode->SetAttribute("h", it->second.w);
            layoutNode->InsertEndChild(windowNode);
        }
        layoutsNode->InsertEndChild(layoutNode);
    }
    pRoot->InsertEndChild(layoutsNode);

    // Render
    XMLElement *RenderNode = xmlDoc.NewElement( "Render" );
    RenderNode->SetAttribute("gpu_decoding", application.render.gpu_decoding);
    RenderNode->SetAttribute("x11", application.render.x11);
    RenderNode->SetAttribute("ratio", application.render.ratio);
    RenderNode->SetAttribute("res", application.render.res);
    RenderNode->SetAttribute("custom_width", application.render.custom_width);
    RenderNode->SetAttribute("custom_height", application.render.custom_height);
    pRoot->InsertEndChild(RenderNode);

    // Record
    XMLElement *RecordNode = xmlDoc.NewElement( "Record" );
    RecordNode->SetAttribute("path", application.record.path.c_str());
    RecordNode->SetAttribute("profile", application.record.profile);
    RecordNode->SetAttribute("timeout", application.record.timeout);
    RecordNode->SetAttribute("delay", application.record.delay);
    RecordNode->SetAttribute("framerate_mode", application.record.framerate_mode);
    RecordNode->SetAttribute("buffering_mode", application.record.buffering_mode);
    RecordNode->SetAttribute("priority_mode", application.record.priority_mode);
    RecordNode->SetAttribute("naming_mode", application.record.naming_mode);
    RecordNode->SetAttribute("audio_device", application.record.audio_device.c_str());
    pRoot->InsertEndChild(RecordNode);

    // Image sequence
    XMLElement *SequenceNode = xmlDoc.NewElement( "Sequence" );
    SequenceNode->SetAttribute("profile", application.image_sequence.profile);
    SequenceNode->SetAttribute("framerate", application.image_sequence.framerate_mode);
    SequenceNode->SetAttribute("rife_mode", application.image_sequence.buffering_mode);
    SequenceNode->SetAttribute("loop_mode", application.image_sequence.priority_mode);
    pRoot->InsertEndChild(SequenceNode);

    // Transition
    XMLElement *TransitionNode = xmlDoc.NewElement( "Transition" );
    TransitionNode->SetAttribute("cross_fade", application.transition.cross_fade);
    TransitionNode->SetAttribute("duration", application.transition.duration);
    TransitionNode->SetAttribute("profile", application.transition.profile);
    pRoot->InsertEndChild(TransitionNode);

    // Source
    XMLElement *SourceConfNode = xmlDoc.NewElement( "Source" );
    SourceConfNode->SetAttribute("new_type", application.source.new_type);
    SourceConfNode->SetAttribute("ratio", application.source.ratio);
    SourceConfNode->SetAttribute("res", application.source.res);
    SourceConfNode->SetAttribute("capture_naming", application.source.capture_naming);
    SourceConfNode->SetAttribute("capture_format", application.source.capture_format);
    SourceConfNode->SetAttribute("capture_path", application.source.capture_path.c_str());
    SourceConfNode->SetAttribute("inspector_zoom", application.source.inspector_zoom);
    pRoot->InsertEndChild(SourceConfNode);

    // Brush
    XMLElement *BrushNode = xmlDoc.NewElement( "Brush" );
    BrushNode->InsertEndChild( XMLElementFromGLM(&xmlDoc, application.brush) );
    BrushNode->SetAttribute("brush_pressure_mode", application.brush_pressure_mode);
    pRoot->InsertEndChild(BrushNode);

    // Pointer
    XMLElement *PointerNode = xmlDoc.NewElement( "MousePointer" );
    PointerNode->SetAttribute("mode", application.mouse_pointer);
    PointerNode->SetAttribute("lock", application.mouse_pointer_lock);
    PointerNode->SetAttribute("proportional_grid", application.proportional_grid);
    for (size_t i = 0; i < application.mouse_pointer_strength.size(); ++i ) {
        float v = application.mouse_pointer_strength[i];
        PointerNode->InsertEndChild( XMLElementFromGLM(&xmlDoc, glm::vec2((float)i, v)) );
    }
    pRoot->InsertEndChild(PointerNode);

    // bloc views
    {
        XMLElement *viewsNode = xmlDoc.NewElement( "Views" );
        // do not save Transition or Presentation view as current
        int v = (application.current_view == 5 || application.current_view == 7) ? 1 : application.current_view;
        viewsNode->SetAttribute("current", v);

        std::map<int, Settings::ViewConfig>::iterator iter;
        for (iter=application.views.begin(); iter != application.views.end(); ++iter)
        {
            const Settings::ViewConfig& view_config = iter->second;

            XMLElement *view = xmlDoc.NewElement( "View" );
            view->SetAttribute("name", view_config.name.c_str());
            view->SetAttribute("id", iter->first);
            view->SetAttribute("ignore_mix", view_config.ignore_mix);

            XMLElement *scale = xmlDoc.NewElement("default_scale");
            scale->InsertEndChild( XMLElementFromGLM(&xmlDoc, view_config.default_scale) );
            view->InsertEndChild(scale);
            XMLElement *translation = xmlDoc.NewElement("default_translation");
            translation->InsertEndChild( XMLElementFromGLM(&xmlDoc, view_config.default_translation) );
            view->InsertEndChild(translation);

            viewsNode->InsertEndChild(view);
        }

        pRoot->InsertEndChild(viewsNode);
    }

    // bloc history
    {
        XMLElement *recent = xmlDoc.NewElement( "Recent" );

        // recent session filenames
        recent->InsertEndChild( save_history(application.recentSessions, "Session", xmlDoc));

        // recent session folders
        recent->InsertEndChild( save_history(application.recentPlaylists, "Playlist", xmlDoc));

        // recent export folder
        recent->InsertEndChild( save_history(application.recentExportFolder, "ExportFolder", xmlDoc));

        // recent session folders
        recent->InsertEndChild( save_history(application.recentFolders, "Folder", xmlDoc));

        // recent import media uri
        recent->InsertEndChild( save_history(application.recentImport, "Import", xmlDoc));

        // recent import folders
        recent->InsertEndChild( save_history(application.recentImportFolders, "ImportFolder", xmlDoc));

        // recent recordings
        recent->InsertEndChild( save_history(application.recentRecordings, "Record", xmlDoc));

        // recent shader files
        recent->InsertEndChild( save_history(application.recentShaderCode, "Shaders", xmlDoc));

        // recent dialog path
        XMLElement *recentdialogpath = xmlDoc.NewElement( "Dialog" );
        for(auto it = application.dialogRecentFolder.cbegin();
            it != application.dialogRecentFolder.cend(); ++it) {
            XMLElement *pathNode = xmlDoc.NewElement("path");
            pathNode->SetAttribute("label", (*it).first.c_str() );
            XMLText *text = xmlDoc.NewText( (*it).second.c_str() );
            pathNode->InsertEndChild( text );
            recentdialogpath->InsertFirstChild(pathNode);
        }
        recentdialogpath->InsertEndChild( XMLElementFromGLM(&xmlDoc, application.dialogPosition) );

        recent->InsertEndChild(recentdialogpath);

        pRoot->InsertEndChild(recent);
    }

    // hosts known hosts
    {
        XMLElement *knownhosts = xmlDoc.NewElement( "Hosts" );

        // recent SRT hosts
        knownhosts->InsertEndChild( save_knownhost(application.recentSRT, "SRT", xmlDoc));
        pRoot->InsertEndChild(knownhosts);
    }

    // Timer Metronome
    XMLElement *timerConfNode = xmlDoc.NewElement( "Timer" );
    timerConfNode->SetAttribute("mode", application.timer.mode);
    timerConfNode->SetAttribute("link_enabled", application.timer.link_enabled);
    timerConfNode->SetAttribute("link_tempo", application.timer.link_tempo);
    timerConfNode->SetAttribute("link_quantum", application.timer.link_quantum);
    timerConfNode->SetAttribute("link_start_stop_sync", application.timer.link_start_stop_sync);
    timerConfNode->SetAttribute("stopwatch_duration", application.timer.stopwatch_duration);
    pRoot->InsertEndChild(timerConfNode);    

    // Inputs mapping
    XMLElement *mappingConfNode = xmlDoc.NewElement( "Mapping" );
    mappingConfNode->SetAttribute("mode", application.mapping.mode);
    mappingConfNode->SetAttribute("current", application.mapping.current);
    mappingConfNode->SetAttribute("disabled", application.mapping.disabled);
    mappingConfNode->SetAttribute("gamepad", application.gamepad_1_id);
    mappingConfNode->SetAttribute("gamepad2", application.gamepad_2_id);
    if (!application.gamepad_mapping_filename.empty()) {
        XMLElement *gamepadNode = xmlDoc.NewElement("gamepad_filename");
        XMLText *text = xmlDoc.NewText( application.gamepad_mapping_filename.c_str() );
        gamepadNode->InsertEndChild( text );
        mappingConfNode->InsertEndChild(gamepadNode);
    }
    pRoot->InsertEndChild(mappingConfNode);

    // Controller
    XMLElement *controlConfNode = xmlDoc.NewElement( "Control" );
    controlConfNode->SetAttribute("osc_port_receive", application.control.osc_port_receive);
    controlConfNode->SetAttribute("osc_port_send", application.control.osc_port_send);
    pRoot->InsertEndChild(controlConfNode);

    // First save : create filename
    if (settingsFilename.empty())
        settingsFilename = SystemToolkit::full_filename(SystemToolkit::settings_path(), APP_SETTINGS);

    XMLError eResult = xmlDoc.SaveFile( filename.empty() ?
                                           settingsFilename.c_str() : filename.c_str());
    XMLResultError(eResult);

}


void load_history(Settings::History &h, const char *nodename, XMLElement *root)
{
    XMLElement * pElement = root->FirstChildElement(nodename);
    if (pElement)
    {
        // list of path
        h.filenames.clear();
        XMLElement* path = pElement->FirstChildElement("path");
        for( ; path ; path = path->NextSiblingElement())
        {
            const char *p = path->GetText();
            if (p)
                h.push( std::string (p) );
        }
        // path attribute
        const char *path_ = pElement->Attribute("path");
        if (path_)
            h.path = std::string(path_);
        else
            h.path = SystemToolkit::home_path();
        // other attritutes
        pElement->QueryBoolAttribute("autoload", &h.load_at_start);
        pElement->QueryBoolAttribute("autosave", &h.save_on_exit);
        pElement->QueryBoolAttribute("valid", &h.front_is_valid);
        pElement->QueryIntAttribute("ordering", &h.ordering);

        h.changed = true;
    }
}


void load_knownhost(Settings::KnownHosts &h, const char *nodename, XMLElement *root)
{
    XMLElement * pElement = root->FirstChildElement(nodename);
    if (pElement)
    {
        // list of hosts
        h.hosts.clear();
        XMLElement* host_ = pElement->FirstChildElement("host");
        for( ; host_ ; host_ = host_->NextSiblingElement())
        {
            const char *ip_ = host_->GetText();
            if (ip_) {
                const char *port_ = host_->Attribute("port");
                if (port_)
                    h.push( std::string(ip_), std::string(port_) );
                else
                    h.push( std::string(ip_) );
            }
        }
        // protocol attribute
        const char *protocol_ = pElement->Attribute("protocol");
        if (protocol_)
            h.protocol = std::string(protocol_);
    }
}

void load_window(XMLElement *node, Settings::WindowConfig &w)
{
    if (node == nullptr)
        return;
    node->QueryIntAttribute("x", &w.x);
    node->QueryIntAttribute("y", &w.y);
    node->QueryIntAttribute("w", &w.w);
    node->QueryIntAttribute("h", &w.h);
    node->QueryBoolAttribute("f", &w.fullscreen);
    const char *text = node->Attribute("m");
    if (text)
        w.monitor = std::string(text);
}

void load_widgets(XMLElement *node, Settings::WidgetsConfig &w)
{
    if (node == nullptr)
        return;
    node->QueryBoolAttribute("preview", &w.preview);
    node->QueryIntAttribute("preview_view", &w.preview_view);
    node->QueryBoolAttribute("timer", &w.timer);
    node->QueryIntAttribute("timer_view", &w.timer_view);
    node->QueryBoolAttribute("inputs", &w.inputs);
    node->QueryIntAttribute("inputs_view", &w.inputs_view);
    node->QueryBoolAttribute("media_player", &w.media_player);
    node->QueryIntAttribute("media_player_view", &w.media_player_view);
    node->QueryIntAttribute("timeline_editmode", &w.media_player_timeline_editmode);
    node->QueryIntAttribute("timeline_flag", &w.media_player_timeline_flag);
    node->QueryFloatAttribute("media_player_slider", &w.media_player_slider);
    node->QueryFloatAttribute("draft_slider", &w.draft_slider);
    node->QueryBoolAttribute("draft", &w.draft);
    node->QueryIntAttribute("draft_corner", &w.draft_corner);
    node->QueryBoolAttribute("shader_editor", &w.shader_editor);
    node->QueryIntAttribute("shader_editor_view", &w.shader_editor_view);
    node->QueryBoolAttribute("stats", &w.stats);
    node->QueryIntAttribute("stats_mode", &w.stats_mode);
    node->QueryIntAttribute("stats_corner", &w.stats_corner);
    node->QueryBoolAttribute("logs", &w.logs);
    node->QueryBoolAttribute("toolbox", &w.toolbox);
    node->QueryBoolAttribute("source_toolbar", &w.source_toolbar);
    node->QueryIntAttribute("source_toolbar_mode", &w.source_toolbar_mode);
    node->QueryIntAttribute("source_toolbar_border", &w.source_toolbar_border);
    node->QueryBoolAttribute("pannel_always_visible", &w.pannel_always_visible);
}

void load_layouts(XMLElement *node)
{
    if (node == nullptr)
        return;

    XMLElement *layoutNode = node->FirstChildElement("Layout");
    for( ; layoutNode ; layoutNode = layoutNode->NextSiblingElement("Layout")) {
        int i = -1;
        layoutNode->QueryIntAttribute("index", &i);
        if (i < 0 || i >= MAX_WINDOWS_LAYOUT)
            continue;
        Settings::WindowsLayout layout;
        load_window(layoutNode->FirstChildElement("MainWindow"), layout.mainwindow);
        load_widgets(layoutNode->FirstChildElement("Widgets"), layout.widget);
        XMLElement *windowNode = layoutNode->FirstChildElement("Window");
        for( ; windowNode ; windowNode = windowNode->NextSiblingElement("Window")) {
            const char *id = windowNode->Attribute("id");
            if (id == nullptr)
                continue;
            glm::vec4 g(0.f);
            windowNode->QueryFloatAttribute("x", &g.x);
            windowNode->QueryFloatAttribute("y", &g.y);
            windowNode->QueryFloatAttribute("w", &g.z);
            windowNode->QueryFloatAttribute("h", &g.w);
            layout.windows[std::string(id)] = g;
        }
        layout.saved = true;
        Settings::application.layouts[i] = layout;
    }

    // layout at start is valid only if it was saved
    node->QueryIntAttribute("start", &Settings::application.layout_at_start);
    if (Settings::application.layout_at_start < 0
        || Settings::application.layout_at_start >= MAX_WINDOWS_LAYOUT
        || !Settings::application.layouts[Settings::application.layout_at_start].saved )
        Settings::application.layout_at_start = -1;
}

void Settings::init(const std::string &filename)
{
    // impose C locale for all app
    setlocale(LC_ALL, "C");

    // set filenames from settings path
    application.control.osc_filename = SystemToolkit::full_filename(SystemToolkit::settings_path(), OSC_CONFIG_FILE);
    settingsFilename = SystemToolkit::full_filename(SystemToolkit::settings_path(), APP_SETTINGS);

    // try to load settings file
    XMLDocument xmlDoc;
    XMLError eResult = xmlDoc.LoadFile( filename.empty() ?
                                           settingsFilename.c_str() : filename.c_str());

	// do not warn if non existing file
    if (eResult == XML_ERROR_FILE_NOT_FOUND)
        return;
    // warn and return on other error
    else if (XMLResultError(eResult))
        return;

    // first element should be called by the application name
    XMLElement *pRoot = xmlDoc.FirstChildElement(application.name.c_str());
    if (pRoot == nullptr)
        return;

    // test version
    bool version_same = true;
#ifdef VIMIX_VERSION_MAJOR
    int major = 0, minor = 0, patch = 0;
    pRoot->QueryIntAttribute("major", &major);
    pRoot->QueryIntAttribute("minor", &minor);
    pRoot->QueryIntAttribute("patch", &patch);
    version_same = (major == VIMIX_VERSION_MAJOR)
                   && (minor == VIMIX_VERSION_MINOR)
                   && (patch == VIMIX_VERSION_PATCH);
#endif

    //
    // Restore settings only if version is same
    //
    if (version_same) {

        // runtime
        pRoot->QueryUnsigned64Attribute("runtime", &application.total_runtime);

        // General application preferences
        XMLElement * applicationNode = pRoot->FirstChildElement("Application");
        if (applicationNode != nullptr) {
            applicationNode->QueryFloatAttribute("scale", &application.scale);
            applicationNode->QueryIntAttribute("accent_color", &application.accent_color);
            applicationNode->QueryBoolAttribute("smooth_transition", &application.smooth_transition);
            applicationNode->QueryFloatAttribute("draft_duration", &application.draft_duration);
            applicationNode->QueryBoolAttribute("save_snapshot", &application.save_version_snapshot);
            applicationNode->QueryBoolAttribute("action_history_follow_view", &application.action_history_follow_view);
            applicationNode->QueryBoolAttribute("show_tooptips", &application.show_tooptips);
            applicationNode->QueryBoolAttribute("accept_connections", &application.accept_connections);
            // (compatibility with settings before pannel_always_visible moved to Widgets)
            applicationNode->QueryBoolAttribute("pannel_always_visible", &application.widget.pannel_always_visible);
            applicationNode->QueryBoolAttribute("exclusive_header", &application.exclusive_header);
            applicationNode->QueryBoolAttribute("pannel_list_all_viewspaces", &application.pannel_list_all_viewspaces);
            applicationNode->QueryIntAttribute("pannel_main_mode", &application.pannel_main_mode);
            applicationNode->QueryIntAttribute("pannel_playlist_mode", &application.pannel_playlist_mode);
            applicationNode->QueryBoolAttribute("pannel_source_0", &application.pannel_source[0]);
            applicationNode->QueryBoolAttribute("pannel_source_1", &application.pannel_source[1]);
            applicationNode->QueryBoolAttribute("pannel_source_2", &application.pannel_source[2]);
            applicationNode->QueryBoolAttribute("pannel_source_3", &application.pannel_source[3]);
            applicationNode->QueryBoolAttribute("pannel_session_0", &application.pannel_session[0]);
            applicationNode->QueryBoolAttribute("pannel_session_1", &application.pannel_session[1]);
            applicationNode->QueryBoolAttribute("pannel_session_2", &application.pannel_session[2]);
            applicationNode->QueryBoolAttribute("pannel_session_3", &application.pannel_session[3]);
            applicationNode->QueryBoolAttribute("pannel_settings_0", &application.pannel_settings[0]);
            applicationNode->QueryBoolAttribute("pannel_settings_1", &application.pannel_settings[1]);
            applicationNode->QueryBoolAttribute("pannel_settings_2", &application.pannel_settings[2]);
            applicationNode->QueryBoolAttribute("pannel_settings_3", &application.pannel_settings[3]);
            applicationNode->QueryBoolAttribute("pannel_settings_4", &application.pannel_settings[4]);
            applicationNode->QueryBoolAttribute("pannel_settings_5", &application.pannel_settings[5]);
            applicationNode->QueryIntAttribute("stream_protocol", &application.stream_protocol);
            applicationNode->QueryIntAttribute("broadcast_port", &application.broadcast_port);
            applicationNode->QueryIntAttribute("loopback_camera", &application.loopback_camera);
            applicationNode->QueryBoolAttribute("accept_audio", &application.accept_audio);
            applicationNode->QueryBoolAttribute("media_keys", &application.media_keys);
            applicationNode->QueryIntAttribute("shm_method", &application.shm_method);

            XMLElement * transcodeNode = applicationNode->FirstChildElement("Transcode");
            if (transcodeNode != nullptr) {
                transcodeNode->QueryIntAttribute("option_0", &application.transcode_options[0]) ;
                transcodeNode->QueryIntAttribute("option_1", &application.transcode_options[1]) ;
                transcodeNode->QueryIntAttribute("option_2", &application.transcode_options[2]) ;
                transcodeNode->QueryIntAttribute("image_format", &application.transcode_image_format) ;
                const char *upscaler = transcodeNode->Attribute("upscaler");
                if (upscaler != nullptr)
                    application.transcode_upscaler = upscaler;
            }

            XMLElement * exportNode = applicationNode->FirstChildElement("Export");
            if (exportNode != nullptr) {
                exportNode->QueryBoolAttribute("copy_media", &application.export_options[0]) ;
                exportNode->QueryBoolAttribute("archive", &application.export_options[1]) ;
                exportNode->QueryBoolAttribute("discard_versions", &application.export_options[2]) ;
            }

            // text attributes
            const char *tmpstr = applicationNode->Attribute("shm_socket_path");
            if (tmpstr)
                application.shm_socket_path = std::string(tmpstr);
        }

        // Widgets
        load_widgets(pRoot->FirstChildElement("Widgets"), application.widget);

        // Render
        XMLElement * rendernode = pRoot->FirstChildElement("Render");
        if (rendernode != nullptr) {
            rendernode->QueryBoolAttribute("gpu_decoding", &application.render.gpu_decoding);
            rendernode->QueryBoolAttribute("x11", &application.render.x11);
            rendernode->QueryIntAttribute("ratio", &application.render.ratio);
            rendernode->QueryIntAttribute("res", &application.render.res);
            rendernode->QueryIntAttribute("custom_width", &application.render.custom_width);
            rendernode->QueryIntAttribute("custom_height", &application.render.custom_height);
        }

        // Record
        XMLElement * recordnode = pRoot->FirstChildElement("Record");
        if (recordnode != nullptr) {
            recordnode->QueryIntAttribute("profile", &application.record.profile);
            recordnode->QueryUnsignedAttribute("timeout", &application.record.timeout);
            recordnode->QueryIntAttribute("delay", &application.record.delay);
            recordnode->QueryIntAttribute("framerate_mode", &application.record.framerate_mode);
            recordnode->QueryIntAttribute("buffering_mode", &application.record.buffering_mode);
            recordnode->QueryIntAttribute("priority_mode", &application.record.priority_mode);
            recordnode->QueryIntAttribute("naming_mode", &application.record.naming_mode);

            const char *path_ = recordnode->Attribute("path");
            if (path_)
                application.record.path = std::string(path_);
            else
                application.record.path = SystemToolkit::home_path();

            const char *dev_ = recordnode->Attribute("audio_device");
            if (dev_) {
                application.record.audio_device = std::string(dev_);
                // if recording with audio and have a device, force priority to Duration
                if (application.accept_audio && !application.record.audio_device.empty())
                    application.record.priority_mode = 0;
            }
            else
                application.record.audio_device = "";
        }

        // Sequence
        XMLElement * sequencenode = pRoot->FirstChildElement("Sequence");
        if (sequencenode != nullptr) {
            sequencenode->QueryIntAttribute("profile", &application.image_sequence.profile);
            sequencenode->QueryIntAttribute("framerate", &application.image_sequence.framerate_mode);
            sequencenode->QueryIntAttribute("rife_mode", &application.image_sequence.buffering_mode);
            sequencenode->QueryIntAttribute("loop_mode", &application.image_sequence.priority_mode);
        }

        // Source
        XMLElement * sourceconfnode = pRoot->FirstChildElement("Source");
        if (sourceconfnode != nullptr) {
            sourceconfnode->QueryIntAttribute("new_type", &application.source.new_type);
            sourceconfnode->QueryIntAttribute("ratio", &application.source.ratio);
            sourceconfnode->QueryIntAttribute("res", &application.source.res);

            sourceconfnode->QueryIntAttribute("capture_naming", &application.source.capture_naming);
            sourceconfnode->QueryIntAttribute("capture_format", &application.source.capture_format);
            const char *path_ = sourceconfnode->Attribute("capture_path");
            if (path_)
                application.source.capture_path = std::string(path_);
            else
                application.source.capture_path = SystemToolkit::home_path();
            sourceconfnode->QueryFloatAttribute("inspector_zoom", &application.source.inspector_zoom);
        }

        // Transition
        XMLElement * transitionnode = pRoot->FirstChildElement("Transition");
        if (transitionnode != nullptr) {
            transitionnode->QueryBoolAttribute("cross_fade", &application.transition.cross_fade);
            transitionnode->QueryFloatAttribute("duration", &application.transition.duration);
            transitionnode->QueryBoolAttribute("profile", &application.transition.profile);
        }

        // Main Window
        load_window(pRoot->FirstChildElement("MainWindow"), application.mainwindow);

        // read application.monitors MonitorConfig
        XMLElement *monitorsNode = pRoot->FirstChildElement("Monitors");
        if (monitorsNode != nullptr) {
            XMLElement *monitorNode = monitorsNode->FirstChildElement("Monitor");
            for( ; monitorNode ; monitorNode = monitorNode->NextSiblingElement()) {
                const char *name = monitorNode->Attribute("name");
                if (name) {
                    std::string monitorName = std::string(name);

                    // create config for this monitor if it doesn't exist
                    if (application.monitors.find(monitorName) == application.monitors.end()) {
                        application.monitors[monitorName] = Settings::MonitorConfig();
                    }

                    // read attributes                    
                    monitorNode->QueryBoolAttribute("active_output", &application.monitors[monitorName].active_output);
                    monitorNode->QueryFloatAttribute("brightness", &application.monitors[monitorName].brightness);
                    monitorNode->QueryFloatAttribute("contrast", &application.monitors[monitorName].contrast);
                    monitorNode->QueryBoolAttribute("custom_geometry", &application.monitors[monitorName].custom_geometry);

                    // read whitebalance vec4
                    XMLElement *whitebalanceNode = monitorNode->FirstChildElement("whitebalance");
                    if (whitebalanceNode) {
                        tinyxml2::XMLElementToGLM(whitebalanceNode->FirstChildElement("vec4"),
                                                 application.monitors[monitorName].whitebalance);
                    }

                    // read nodes mat4
                    XMLElement *nodesNode = monitorNode->FirstChildElement("nodes");
                    if (nodesNode) {
                        tinyxml2::XMLElementToGLM(nodesNode->FirstChildElement("mat4"),
                                                 application.monitors[monitorName].nodes);
                    }
                }
            }
        }


        // Brush
        XMLElement * brushnode = pRoot->FirstChildElement("Brush");
        if (brushnode != nullptr) {
            tinyxml2::XMLElementToGLM( brushnode->FirstChildElement("vec3"), application.brush);
            brushnode->QueryIntAttribute("brush_pressure_mode", &application.brush_pressure_mode);
        }

        // Pointer
        XMLElement * pointernode = pRoot->FirstChildElement("MousePointer");
        if (pointernode != nullptr) {
            pointernode->QueryIntAttribute("mode", &application.mouse_pointer);
            pointernode->QueryBoolAttribute("lock", &application.mouse_pointer_lock);
            pointernode->QueryBoolAttribute("proportional_grid", &application.proportional_grid);

            XMLElement* strengthNode = pointernode->FirstChildElement("vec2");
            for( ; strengthNode ; strengthNode = strengthNode->NextSiblingElement())
            {
                glm::vec2 val;
                tinyxml2::XMLElementToGLM( strengthNode, val);
                size_t i = (size_t) ceil(val.x);
                if (i >= application.mouse_pointer_strength.size())
                    application.mouse_pointer_strength.resize(i + 1, 0.f);
                application.mouse_pointer_strength[ i ] = val.y;
            }
        }

        // bloc views
        {
            XMLElement * pElement = pRoot->FirstChildElement("Views");
            if (pElement)
            {
                application.views.clear(); // trash existing list
                pElement->QueryIntAttribute("current", &application.current_view);

                XMLElement* viewNode = pElement->FirstChildElement("View");
                for( ; viewNode ; viewNode=viewNode->NextSiblingElement())
                {
                    int id = 0;
                    viewNode->QueryIntAttribute("id", &id);
                    application.views[id].name = viewNode->Attribute("name");
                    viewNode->QueryBoolAttribute("ignore_mix", &application.views[id].ignore_mix);

                    XMLElement* scaleNode = viewNode->FirstChildElement("default_scale");
                    if (scaleNode)
                        tinyxml2::XMLElementToGLM( scaleNode->FirstChildElement("vec3"),
                                                  application.views[id].default_scale);

                    XMLElement* translationNode = viewNode->FirstChildElement("default_translation");
                    if (translationNode)
                        tinyxml2::XMLElementToGLM( translationNode->FirstChildElement("vec3"),
                                                  application.views[id].default_translation);
                }
            }
        }

        // Mapping
        XMLElement * mappingconfnode = pRoot->FirstChildElement("Mapping");
        if (mappingconfnode != nullptr) {
            mappingconfnode->QueryUnsigned64Attribute("mode", &application.mapping.mode);
            mappingconfnode->QueryUnsignedAttribute("current", &application.mapping.current);
            mappingconfnode->QueryBoolAttribute("disabled", &application.mapping.disabled);
            mappingconfnode->QueryIntAttribute("gamepad", &application.gamepad_1_id);
            mappingconfnode->QueryIntAttribute("gamepad2", &application.gamepad_2_id);
            XMLElement* gamepadNode = mappingconfnode->FirstChildElement("gamepad_filename");
            if( gamepadNode )
            {
                const char *p = gamepadNode->GetText();
                if (p)
                    application.gamepad_mapping_filename = p;
            }
        }

        // Timer Metronome
        XMLElement * timerconfnode = pRoot->FirstChildElement("Timer");
        if (timerconfnode != nullptr) {
            timerconfnode->QueryUnsigned64Attribute("mode", &application.timer.mode);
            timerconfnode->QueryBoolAttribute("link_enabled", &application.timer.link_enabled);
            timerconfnode->QueryDoubleAttribute("link_tempo", &application.timer.link_tempo);
            timerconfnode->QueryDoubleAttribute("link_quantum", &application.timer.link_quantum);
            timerconfnode->QueryBoolAttribute("link_start_stop_sync", &application.timer.link_start_stop_sync);
            timerconfnode->QueryUnsigned64Attribute("stopwatch_duration", &application.timer.stopwatch_duration);
        }

    }

    //
    // Restore history and user entries
    //

    // bloc history of recent
    {
        XMLElement * pElement = pRoot->FirstChildElement("Recent");
        if (pElement)
        {
            // recent session filenames
            load_history(application.recentSessions, "Session", pElement);

            // recent session playlist
            load_history(application.recentPlaylists, "Playlist", pElement);

            // recent export folder
            load_history(application.recentExportFolder, "ExportFolder", pElement);

            // recent session folders
            load_history(application.recentFolders, "Folder", pElement);

            // recent media uri
            load_history(application.recentImport, "Import", pElement);

            // recent import folders
            load_history(application.recentImportFolders, "ImportFolder", pElement);

            // recent recordings
            load_history(application.recentRecordings, "Record", pElement);

            // recent Shader files
            load_history(application.recentShaderCode, "Shaders", pElement);

            // recent dialog path
            XMLElement * pDialog = pElement->FirstChildElement("Dialog");
            if (pDialog)
            {
                application.dialogRecentFolder.clear();
                XMLElement* path = pDialog->FirstChildElement("path");
                for( ; path ; path = path->NextSiblingElement())
                {
                    const char *l = path->Attribute("label");
                    const char *p = path->GetText();
                    if (l && p)
                        application.dialogRecentFolder[ std::string(l)] = std::string (p);
                }
                tinyxml2::XMLElementToGLM( pDialog->FirstChildElement("ivec2"), application.dialogPosition);
            }
        }
    }

    // bloc of known hosts
    {
        XMLElement * pElement = pRoot->FirstChildElement("Hosts");
        if (pElement)
        {
            // recent SRT hosts
            load_knownhost(application.recentSRT, "SRT", pElement);
        }
    }

    // Windows layouts (kept across versions)
    load_layouts(pRoot->FirstChildElement("Layouts"));

    // start with a windows layout instead of previous state
    if (application.layout_at_start >= 0) {
        const WindowsLayout &layout = application.layouts[application.layout_at_start];
        application.widget = layout.widget;
        application.mainwindow.setGeometry(layout.mainwindow);
    }

    // bloc Controller
    XMLElement * controlconfnode = pRoot->FirstChildElement("Control");
    if (controlconfnode != nullptr) {
        controlconfnode->QueryIntAttribute("osc_port_receive", &application.control.osc_port_receive);
        controlconfnode->QueryIntAttribute("osc_port_send", &application.control.osc_port_send);
    }

}

void Settings::History::assign(const std::string &filename)
{
    path.assign(filename);
    changed = true;
}

void Settings::History::push(const std::string &filename)
{
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

void Settings::History::remove(const std::string &filename)
{
    if (filename.empty())
        return;
    if (filenames.front() == filename)
        front_is_valid = false;
    filenames.remove(filename);
    changed = true;
}

void Settings::History::validate()
{
    for (auto fit = filenames.begin(); fit != filenames.end();) {
        if ( SystemToolkit::file_exists( *fit ))
            ++fit;
        else
            fit = filenames.erase(fit);
    }
    if (!path.empty() && !SystemToolkit::file_exists( path )) {
        path = "";
    }
}

void Settings::KnownHosts::push(const std::string &ip, const std::string &port)
{
    if (!ip.empty()) {

        std::pair<std::string, std::string> h = { ip, port };
        hosts.remove(h);
        hosts.push_front(h);
        if (hosts.size() > MAX_RECENT_HISTORY)
            hosts.pop_back();
    }
}

void Settings::KnownHosts::remove(const std::string &ip)
{
    for (auto hit = hosts.begin(); hit != hosts.end();) {
        if ( ip.compare( hit->first ) > 0 )
            ++hit;
        else
            hit = hosts.erase(hit);
    }
}


void Settings::Lock()
{

    std::string lockfile = SystemToolkit::full_filename(SystemToolkit::settings_path(), "lock");
    application.fresh_start = false;

    FILE *file = fopen(lockfile.c_str(), "r");
    int l = 0;
    if (file) {
        if ( fscanf(file, "%d", &l) < 1)
            l = 0;
        fclose(file);
    }

    // not locked or file not existing
    if ( l < 1 ) {
        file = fopen(lockfile.c_str(), "w");
        if (file) {
            fprintf(file, "1");
            fclose(file);
        }
        application.fresh_start = true;
    }

}

void Settings::Unlock()
{
    std::string lockfile = SystemToolkit::full_filename(SystemToolkit::settings_path(), "lock");
    FILE *file = fopen(lockfile.c_str(), "w");
    if (file) {
        fprintf(file, "0");
        fclose(file);
    }
}

