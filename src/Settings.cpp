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


#include <tinyxml2.h>
#include "tinyxml2Toolkit.h"
using namespace tinyxml2;

#include "defines.h"
#include "SystemToolkit.h"
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



void Settings::Save(uint64_t runtime, const std::string &filename)
{
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

    // Windows
	{
        XMLElement *windowsNode = xmlDoc.NewElement( "OutputWindows" );
        windowsNode->SetAttribute("num_output_windows", application.num_output_windows);

        for (int i = 0; i < (int) application.windows.size(); ++i)
        {
            const Settings::WindowConfig& w = application.windows[i];

            XMLElement *window = xmlDoc.NewElement( "Window" );
            window->SetAttribute("name", w.name.c_str());
            window->SetAttribute("id", i);
			window->SetAttribute("x", w.x);
			window->SetAttribute("y", w.y);
			window->SetAttribute("w", w.w);
            window->SetAttribute("h", w.h);
            window->SetAttribute("f", w.fullscreen);
            window->SetAttribute("s", w.custom);
            window->SetAttribute("d", w.decorated);
            window->SetAttribute("m", w.monitor.c_str());
            XMLElement *tmp = xmlDoc.NewElement("whitebalance");
            tmp->SetAttribute("brightness", w.brightness);
            tmp->SetAttribute("contrast", w.contrast);
            tmp->InsertEndChild( XMLElementFromGLM(&xmlDoc, w.whitebalance) );
            window->InsertEndChild( tmp );
            tmp = xmlDoc.NewElement("nodes");
            tmp->InsertEndChild( XMLElementFromGLM(&xmlDoc, w.nodes) );
            window->InsertEndChild( tmp );
            windowsNode->InsertEndChild(window);
		}

        pRoot->InsertEndChild(windowsNode);
	}

    // General application preferences
    XMLElement *applicationNode = xmlDoc.NewElement( "Application" );
    applicationNode->SetAttribute("scale", application.scale);
    applicationNode->SetAttribute("accent_color", application.accent_color);
    applicationNode->SetAttribute("smooth_transition", application.smooth_transition);
    applicationNode->SetAttribute("save_snapshot", application.save_version_snapshot);
    applicationNode->SetAttribute("action_history_follow_view", application.action_history_follow_view);
    applicationNode->SetAttribute("show_tooptips", application.show_tooptips);
    applicationNode->SetAttribute("accept_connections", application.accept_connections);
    applicationNode->SetAttribute("pannel_main_mode", application.pannel_main_mode);
    applicationNode->SetAttribute("pannel_playlist_mode", application.pannel_playlist_mode);
    applicationNode->SetAttribute("pannel_history_mode", application.pannel_current_session_mode);
    applicationNode->SetAttribute("pannel_always_visible", application.pannel_always_visible);
    applicationNode->SetAttribute("stream_protocol", application.stream_protocol);
    applicationNode->SetAttribute("broadcast_port", application.broadcast_port);
    applicationNode->SetAttribute("loopback_camera", application.loopback_camera);
    applicationNode->SetAttribute("shm_socket_path", application.shm_socket_path.c_str());
    applicationNode->SetAttribute("shm_method", application.shm_method);
    applicationNode->SetAttribute("accept_audio", application.accept_audio);
    pRoot->InsertEndChild(applicationNode);

    // Widgets
    XMLElement *widgetsNode = xmlDoc.NewElement( "Widgets" );
    widgetsNode->SetAttribute("preview", application.widget.preview);
    widgetsNode->SetAttribute("preview_view", application.widget.preview_view);
    widgetsNode->SetAttribute("inputs", application.widget.inputs);
    widgetsNode->SetAttribute("inputs_view", application.widget.inputs_view);
    widgetsNode->SetAttribute("timer", application.widget.timer);
    widgetsNode->SetAttribute("timer_view", application.widget.timer_view);
    widgetsNode->SetAttribute("media_player", application.widget.media_player);
    widgetsNode->SetAttribute("media_player_view", application.widget.media_player_view);
    widgetsNode->SetAttribute("timeline_editmode", application.widget.media_player_timeline_editmode);
    widgetsNode->SetAttribute("media_player_slider", application.widget.media_player_slider);
    widgetsNode->SetAttribute("shader_editor", application.widget.shader_editor);
    widgetsNode->SetAttribute("shader_editor_view", application.widget.shader_editor_view);
    widgetsNode->SetAttribute("stats", application.widget.stats);
    widgetsNode->SetAttribute("stats_mode", application.widget.stats_mode);
    widgetsNode->SetAttribute("stats_corner", application.widget.stats_corner);
    widgetsNode->SetAttribute("logs", application.widget.logs);
    widgetsNode->SetAttribute("toolbox", application.widget.toolbox);
    widgetsNode->SetAttribute("source_toolbar", application.widget.source_toolbar);
    widgetsNode->SetAttribute("source_toolbar_mode", application.widget.source_toolbar_mode);
    widgetsNode->SetAttribute("source_toolbar_border", application.widget.source_toolbar_border);
    pRoot->InsertEndChild(widgetsNode);

    // Render
    XMLElement *RenderNode = xmlDoc.NewElement( "Render" );
    RenderNode->SetAttribute("vsync", application.render.vsync);
    RenderNode->SetAttribute("multisampling", application.render.multisampling);
    RenderNode->SetAttribute("gpu_decoding", application.render.gpu_decoding);
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
    SourceConfNode->SetAttribute("capture_path", application.source.capture_path.c_str());
    SourceConfNode->SetAttribute("inspector_zoom", application.source.inspector_zoom);
    pRoot->InsertEndChild(SourceConfNode);

    // Brush
    XMLElement *BrushNode = xmlDoc.NewElement( "Brush" );
    BrushNode->InsertEndChild( XMLElementFromGLM(&xmlDoc, application.brush) );
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
        // do not save Transition view as current
        int v = application.current_view == 5 ? 1 : application.current_view;
        viewsNode->SetAttribute("current", v);
        viewsNode->SetAttribute("workspace", application.current_workspace);

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
    mappingConfNode->SetAttribute("gamepad", application.gamepad_id);
    pRoot->InsertEndChild(mappingConfNode);

    // Controller
    XMLElement *controlConfNode = xmlDoc.NewElement( "Control" );
    controlConfNode->SetAttribute("osc_port_receive", application.control.osc_port_receive);
    controlConfNode->SetAttribute("osc_port_send", application.control.osc_port_send);

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

void Settings::Load(const std::string &filename)
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
            applicationNode->QueryBoolAttribute("save_snapshot", &application.save_version_snapshot);
            applicationNode->QueryBoolAttribute("action_history_follow_view", &application.action_history_follow_view);
            applicationNode->QueryBoolAttribute("show_tooptips", &application.show_tooptips);
            applicationNode->QueryBoolAttribute("accept_connections", &application.accept_connections);
            applicationNode->QueryBoolAttribute("pannel_always_visible", &application.pannel_always_visible);
            applicationNode->QueryIntAttribute("pannel_main_mode", &application.pannel_main_mode);
            applicationNode->QueryIntAttribute("pannel_playlist_mode", &application.pannel_playlist_mode);
            applicationNode->QueryIntAttribute("pannel_history_mode", &application.pannel_current_session_mode);
            applicationNode->QueryIntAttribute("stream_protocol", &application.stream_protocol);
            applicationNode->QueryIntAttribute("broadcast_port", &application.broadcast_port);
            applicationNode->QueryIntAttribute("loopback_camera", &application.loopback_camera);
            applicationNode->QueryBoolAttribute("accept_audio", &application.accept_audio);
            applicationNode->QueryIntAttribute("shm_method", &application.shm_method);

            // text attributes
            const char *tmpstr = applicationNode->Attribute("shm_socket_path");
            if (tmpstr)
                application.shm_socket_path = std::string(tmpstr);
        }

        // Widgets
        XMLElement * widgetsNode = pRoot->FirstChildElement("Widgets");
        if (widgetsNode != nullptr) {
            widgetsNode->QueryBoolAttribute("preview", &application.widget.preview);
            widgetsNode->QueryIntAttribute("preview_view", &application.widget.preview_view);
            widgetsNode->QueryBoolAttribute("timer", &application.widget.timer);
            widgetsNode->QueryIntAttribute("timer_view", &application.widget.timer_view);
            widgetsNode->QueryBoolAttribute("inputs", &application.widget.inputs);
            widgetsNode->QueryIntAttribute("inputs_view", &application.widget.inputs_view);
            widgetsNode->QueryBoolAttribute("media_player", &application.widget.media_player);
            widgetsNode->QueryIntAttribute("media_player_view", &application.widget.media_player_view);
            widgetsNode->QueryBoolAttribute("timeline_editmode", &application.widget.media_player_timeline_editmode);
            widgetsNode->QueryFloatAttribute("media_player_slider", &application.widget.media_player_slider);
            widgetsNode->QueryBoolAttribute("shader_editor", &application.widget.shader_editor);
            widgetsNode->QueryIntAttribute("shader_editor_view", &application.widget.shader_editor_view);
            widgetsNode->QueryBoolAttribute("stats", &application.widget.stats);
            widgetsNode->QueryIntAttribute("stats_mode", &application.widget.stats_mode);
            widgetsNode->QueryIntAttribute("stats_corner", &application.widget.stats_corner);
            widgetsNode->QueryBoolAttribute("logs", &application.widget.logs);
            widgetsNode->QueryBoolAttribute("toolbox", &application.widget.toolbox);
            widgetsNode->QueryBoolAttribute("source_toolbar", &application.widget.source_toolbar);
            widgetsNode->QueryIntAttribute("source_toolbar_mode", &application.widget.source_toolbar_mode);
            widgetsNode->QueryIntAttribute("source_toolbar_border", &application.widget.source_toolbar_border);
        }

        // Render
        XMLElement * rendernode = pRoot->FirstChildElement("Render");
        if (rendernode != nullptr) {
            rendernode->QueryIntAttribute("vsync", &application.render.vsync);
            rendernode->QueryIntAttribute("multisampling", &application.render.multisampling);
            rendernode->QueryBoolAttribute("gpu_decoding", &application.render.gpu_decoding);
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

        // Record
        XMLElement * sequencenode = pRoot->FirstChildElement("Sequence");
        if (sequencenode != nullptr) {
            sequencenode->QueryIntAttribute("profile", &application.image_sequence.profile);
            sequencenode->QueryIntAttribute("framerate", &application.image_sequence.framerate_mode);
        }

        // Source
        XMLElement * sourceconfnode = pRoot->FirstChildElement("Source");
        if (sourceconfnode != nullptr) {
            sourceconfnode->QueryIntAttribute("new_type", &application.source.new_type);
            sourceconfnode->QueryIntAttribute("ratio", &application.source.ratio);
            sourceconfnode->QueryIntAttribute("res", &application.source.res);

            sourceconfnode->QueryIntAttribute("capture_naming", &application.source.capture_naming);
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

        // Windows
        {
            XMLElement * pElement = pRoot->FirstChildElement("OutputWindows");
            if (pElement)
            {
                pElement->QueryIntAttribute("num_output_windows", &application.num_output_windows);

                XMLElement* windowNode = pElement->FirstChildElement("Window");
                for( ; windowNode ; windowNode=windowNode->NextSiblingElement())
                {
                    Settings::WindowConfig w;
                    windowNode->QueryIntAttribute("x", &w.x); // If this fails, original value is left as-is
                    windowNode->QueryIntAttribute("y", &w.y);
                    windowNode->QueryIntAttribute("w", &w.w);
                    windowNode->QueryIntAttribute("h", &w.h);
                    windowNode->QueryBoolAttribute("f", &w.fullscreen);
                    windowNode->QueryBoolAttribute("s", &w.custom);
                    windowNode->QueryBoolAttribute("d", &w.decorated);
                    const char *text = windowNode->Attribute("m");
                    if (text)
                        w.monitor = std::string(text);

                    int i = 0;
                    windowNode->QueryIntAttribute("id", &i);
                    if (i > 0)
                        w.name = "Output " + std::to_string(i) + " - " APP_NAME;
                    else
                        w.name = APP_TITLE;
                    // vec4 values for white balance correction
                    XMLElement *tmp = windowNode->FirstChildElement("whitebalance");
                    if (tmp) {
                        tinyxml2::XMLElementToGLM( tmp->FirstChildElement("vec4"), w.whitebalance);
                        tmp->QueryFloatAttribute("brightness", &w.brightness);
                        tmp->QueryFloatAttribute("contrast", &w.contrast);
                    }
                    // mat4 values for custom fit distortion
                    w.nodes = glm::zero<glm::mat4>();
                    tmp = windowNode->FirstChildElement("nodes");
                    if (tmp)
                        tinyxml2::XMLElementToGLM( tmp->FirstChildElement("mat4"), w.nodes);
                    else {
                        // backward compatibility
                        glm::vec3 scale, translation;
                        tmp = windowNode->FirstChildElement("scale");
                        if (tmp) {
                            tinyxml2::XMLElementToGLM(tmp->FirstChildElement("vec3"), scale);
                            tmp = windowNode->FirstChildElement("translation");
                            if (tmp) {
                                tinyxml2::XMLElementToGLM(tmp->FirstChildElement("vec3"), translation);
                                // calculate nodes with scale and translation
                                w.nodes[0].x =  1.f - ( 1.f * scale.x - translation.x );
                                w.nodes[0].y =  1.f - ( 1.f * scale.y - translation.y );
                                w.nodes[1].x =  1.f - ( 1.f * scale.x - translation.x );
                                w.nodes[1].y = -1.f - (-1.f * scale.y - translation.y );
                                w.nodes[2].x = -1.f - (-1.f * scale.x - translation.x );
                                w.nodes[2].y =  1.f - ( 1.f * scale.y - translation.y );
                                w.nodes[3].x = -1.f - (-1.f * scale.x - translation.x );
                                w.nodes[3].y = -1.f - (-1.f * scale.y - translation.y );
                            }
                        }
                    }
                    application.windows[i] = w;
                }
            }
        }

        // Brush
        XMLElement * brushnode = pRoot->FirstChildElement("Brush");
        if (brushnode != nullptr) {
            tinyxml2::XMLElementToGLM( brushnode->FirstChildElement("vec3"), application.brush);
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
                application.mouse_pointer_strength[ (size_t) ceil(val.x) ] = val.y;
            }
        }

        // bloc views
        {
            XMLElement * pElement = pRoot->FirstChildElement("Views");
            if (pElement)
            {
                application.views.clear(); // trash existing list
                pElement->QueryIntAttribute("current", &application.current_view);
                pElement->QueryIntAttribute("workspace", &application.current_workspace);

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
            mappingconfnode->QueryIntAttribute("gamepad", &application.gamepad_id);
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


void Settings::Check()
{
    XMLDocument xmlDoc;
    XMLError eResult = xmlDoc.LoadFile(settingsFilename.c_str());
    if (XMLResultError(eResult)) {
        return;
    }
	xmlDoc.Print();
}

