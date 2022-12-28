/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2022 Bruno Herbelin <bruno.herbelin@gmail.com>
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

#include <iostream>
#include <locale>
using namespace std;

#include <tinyxml2.h>
#include "tinyxml2Toolkit.h"
using namespace tinyxml2;

#include "defines.h"
#include "SystemToolkit.h"
#include "Settings.h"


Settings::Application Settings::application;
string settingsFilename = "";


XMLElement *save_history(Settings::History &h, const char *nodename, XMLDocument &xmlDoc)
{
    XMLElement *pElement = xmlDoc.NewElement( nodename );
    pElement->SetAttribute("path", h.path.c_str());
    pElement->SetAttribute("autoload", h.load_at_start);
    pElement->SetAttribute("autosave", h.save_on_exit);
    pElement->SetAttribute("valid", h.front_is_valid);
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



void Settings::Save(uint64_t runtime)
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
#endif
    // runtime
    if (runtime>0)
        pRoot->SetAttribute("runtime", runtime + application.total_runtime);

    string comment = "Settings for " + application.name;
    XMLComment *pComment = xmlDoc.NewComment(comment.c_str());
    pRoot->InsertEndChild(pComment);

    // Windows
	{
		XMLElement *windowsNode = xmlDoc.NewElement( "Windows" );  

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
            window->SetAttribute("s", w.scaled);
            window->SetAttribute("m", w.monitor.c_str());
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
    applicationNode->SetAttribute("smooth_cursor", application.smooth_cursor);
    applicationNode->SetAttribute("action_history_follow_view", application.action_history_follow_view);
    applicationNode->SetAttribute("show_tooptips", application.show_tooptips);
    applicationNode->SetAttribute("accept_connections", application.accept_connections);
    applicationNode->SetAttribute("pannel_history_mode", application.pannel_current_session_mode);
    applicationNode->SetAttribute("stream_protocol", application.stream_protocol);
    applicationNode->SetAttribute("broadcast_port", application.broadcast_port);
    applicationNode->SetAttribute("loopback_camera", application.loopback_camera);
    applicationNode->SetAttribute("shm_socket_path", application.shm_socket_path.c_str());
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
    pRoot->InsertEndChild(widgetsNode);

    // Render
    XMLElement *RenderNode = xmlDoc.NewElement( "Render" );
    RenderNode->SetAttribute("vsync", application.render.vsync);
    RenderNode->SetAttribute("multisampling", application.render.multisampling);
    RenderNode->SetAttribute("blit", application.render.blit);
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
    RecordNode->SetAttribute("resolution_mode", application.record.resolution_mode);
    RecordNode->SetAttribute("framerate_mode", application.record.framerate_mode);
    RecordNode->SetAttribute("buffering_mode", application.record.buffering_mode);
    RecordNode->SetAttribute("priority_mode", application.record.priority_mode);
    RecordNode->SetAttribute("naming_mode", application.record.naming_mode);
    pRoot->InsertEndChild(RecordNode);

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

    // bloc views
    {
        XMLElement *viewsNode = xmlDoc.NewElement( "Views" );
        // do not save Transition view as current
        int v = application.current_view == 5 ? 1 : application.current_view;
        viewsNode->SetAttribute("current", v);
        viewsNode->SetAttribute("workspace", application.current_workspace);

        map<int, Settings::ViewConfig>::iterator iter;
        for (iter=application.views.begin(); iter != application.views.end(); ++iter)
        {
            const Settings::ViewConfig& view_config = iter->second;

            XMLElement *view = xmlDoc.NewElement( "View" );
            view->SetAttribute("name", view_config.name.c_str());
            view->SetAttribute("id", iter->first);

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
        recent->InsertEndChild( save_history(application.recentFolders, "Folder", xmlDoc));

        // recent import media uri
        recent->InsertEndChild( save_history(application.recentImport, "Import", xmlDoc));

        // recent import folders
        recent->InsertEndChild( save_history(application.recentImportFolders, "ImportFolder", xmlDoc));

        // recent recordings
        recent->InsertEndChild( save_history(application.recentRecordings, "Record", xmlDoc));

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
    pRoot->InsertEndChild(mappingConfNode);

    // Controller
    XMLElement *controlConfNode = xmlDoc.NewElement( "Control" );
    controlConfNode->SetAttribute("osc_port_receive", application.control.osc_port_receive);
    controlConfNode->SetAttribute("osc_port_send", application.control.osc_port_send);

    // First save : create filename
    if (settingsFilename.empty())
        settingsFilename = SystemToolkit::full_filename(SystemToolkit::settings_path(), APP_SETTINGS);

    XMLError eResult = xmlDoc.SaveFile(settingsFilename.c_str());
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

void Settings::Load()
{
    // impose C locale for all app
    setlocale(LC_ALL, "C");

    // set filenames from settings path
    application.control.osc_filename = SystemToolkit::full_filename(SystemToolkit::settings_path(), OSC_CONFIG_FILE);
    settingsFilename = SystemToolkit::full_filename(SystemToolkit::settings_path(), APP_SETTINGS);

    // try to load settings file
    XMLDocument xmlDoc;
    XMLError eResult = xmlDoc.LoadFile(settingsFilename.c_str());

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

    // runtime
    pRoot->QueryUnsigned64Attribute("runtime", &application.total_runtime);

    // General application preferences
    XMLElement * applicationNode = pRoot->FirstChildElement("Application");
    if (applicationNode != nullptr) {
        applicationNode->QueryFloatAttribute("scale", &application.scale);
        applicationNode->QueryIntAttribute("accent_color", &application.accent_color);
        applicationNode->QueryBoolAttribute("smooth_transition", &application.smooth_transition);
        applicationNode->QueryBoolAttribute("save_snapshot", &application.save_version_snapshot);
        applicationNode->QueryBoolAttribute("smooth_cursor", &application.smooth_cursor);
        applicationNode->QueryBoolAttribute("action_history_follow_view", &application.action_history_follow_view);
        applicationNode->QueryBoolAttribute("show_tooptips", &application.show_tooptips);
        applicationNode->QueryBoolAttribute("accept_connections", &application.accept_connections);
        applicationNode->QueryIntAttribute("pannel_history_mode", &application.pannel_current_session_mode);
        applicationNode->QueryIntAttribute("stream_protocol", &application.stream_protocol);
        applicationNode->QueryIntAttribute("broadcast_port", &application.broadcast_port);
        applicationNode->QueryIntAttribute("loopback_camera", &application.loopback_camera);

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
    }

    // Render
    XMLElement * rendernode = pRoot->FirstChildElement("Render");
    if (rendernode != nullptr) {
        rendernode->QueryIntAttribute("vsync", &application.render.vsync);
        rendernode->QueryIntAttribute("multisampling", &application.render.multisampling);
        rendernode->QueryBoolAttribute("blit", &application.render.blit);
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
        recordnode->QueryIntAttribute("resolution_mode", &application.record.resolution_mode);
        recordnode->QueryIntAttribute("framerate_mode", &application.record.framerate_mode);
        recordnode->QueryIntAttribute("buffering_mode", &application.record.buffering_mode);
        recordnode->QueryIntAttribute("priority_mode", &application.record.priority_mode);
        recordnode->QueryIntAttribute("naming_mode", &application.record.naming_mode);

        const char *path_ = recordnode->Attribute("path");
        if (path_)
            application.record.path = std::string(path_);
        else
            application.record.path = SystemToolkit::home_path();
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
        transitionnode->QueryIntAttribute("profile", &application.transition.profile);
    }

    // Windows
    {
        XMLElement * pElement = pRoot->FirstChildElement("Windows");
        if (pElement)
        {
            XMLElement* windowNode = pElement->FirstChildElement("Window");
            for( ; windowNode ; windowNode=windowNode->NextSiblingElement())
            {
                Settings::WindowConfig w;
                windowNode->QueryIntAttribute("x", &w.x); // If this fails, original value is left as-is
                windowNode->QueryIntAttribute("y", &w.y);
                windowNode->QueryIntAttribute("w", &w.w);
                windowNode->QueryIntAttribute("h", &w.h);
                windowNode->QueryBoolAttribute("f", &w.fullscreen);
                windowNode->QueryBoolAttribute("s", &w.scaled);
                const char *text = windowNode->Attribute("m");
                if (text)
                    w.monitor = std::string(text);

                int i = 0;
                windowNode->QueryIntAttribute("id", &i);
                w.name = application.windows[i].name; // keep only original name
                application.windows[i] = w;
            }
        }
	}

    // Brush
    XMLElement * brushnode = pRoot->FirstChildElement("Brush");
    if (brushnode != nullptr) {
        tinyxml2::XMLElementToGLM( brushnode->FirstChildElement("vec3"), application.brush);
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

                XMLElement* scaleNode = viewNode->FirstChildElement("default_scale");
                tinyxml2::XMLElementToGLM( scaleNode->FirstChildElement("vec3"),
                                           application.views[id].default_scale);

                XMLElement* translationNode = viewNode->FirstChildElement("default_translation");
                tinyxml2::XMLElementToGLM( translationNode->FirstChildElement("vec3"),
                                           application.views[id].default_translation);

            }
        }

    }

    // bloc history of recent
    {
        XMLElement * pElement = pRoot->FirstChildElement("Recent");
        if (pElement)
        {
            // recent session filenames
            load_history(application.recentSessions, "Session", pElement);

            // recent session folders
            load_history(application.recentFolders, "Folder", pElement);

            // recent media uri
            load_history(application.recentImport, "Import", pElement);

            // recent import folders
            load_history(application.recentImportFolders, "ImportFolder", pElement);

            // recent recordings
            load_history(application.recentRecordings, "Record", pElement);

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

    // Mapping
    XMLElement * mappingconfnode = pRoot->FirstChildElement("Mapping");
    if (mappingconfnode != nullptr) {
        mappingconfnode->QueryUnsigned64Attribute("mode", &application.mapping.mode);
        mappingconfnode->QueryUnsignedAttribute("current", &application.mapping.current);
        mappingconfnode->QueryBoolAttribute("disabled", &application.mapping.disabled);
    }

    // bloc Controller
    XMLElement *controlconfnode = pRoot->FirstChildElement("Control");
    if (controlconfnode != nullptr) {
        controlconfnode->QueryIntAttribute("osc_port_receive", &application.control.osc_port_receive);
        controlconfnode->QueryIntAttribute("osc_port_send", &application.control.osc_port_send);
    }

}

void Settings::History::push(const string &filename)
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
}

void Settings::KnownHosts::push(const string &ip, const string &port)
{
    if (!ip.empty()) {

        std::pair<std::string, std::string> h = { ip, port };
        hosts.remove(h);
        hosts.push_front(h);
        if (hosts.size() > MAX_RECENT_HISTORY)
            hosts.pop_back();
    }
}

void Settings::KnownHosts::remove(const string &ip)
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

