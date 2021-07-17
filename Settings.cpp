#include <algorithm>
#include <iostream>
#include <locale>
using namespace std;

#include <tinyxml2.h>
#include "tinyxml2Toolkit.h"
using namespace tinyxml2;

#include "defines.h"
#include "Settings.h"
#include "SystemToolkit.h"


Settings::Application Settings::application;
string settingsFilename = "";

void Settings::Save()
{
    // impose C locale for all app
    setlocale(LC_ALL, "C");

    XMLDocument xmlDoc;
    XMLDeclaration *pDec = xmlDoc.NewDeclaration();
    xmlDoc.InsertFirstChild(pDec);

    XMLElement *pRoot = xmlDoc.NewElement(application.name.c_str());
#ifdef VIMIX_VERSION_MAJOR
    pRoot->SetAttribute("major", VIMIX_VERSION_MAJOR);
    pRoot->SetAttribute("minor", VIMIX_VERSION_MINOR);
    xmlDoc.InsertEndChild(pRoot);
#endif

    string comment = "Settings for " + application.name;
    XMLComment *pComment = xmlDoc.NewComment(comment.c_str());
    pRoot->InsertEndChild(pComment);

	// block: windows
	{
		XMLElement *windowsNode = xmlDoc.NewElement( "Windows" );  

        for (int i = 0; i < application.windows.size(); i++)
        {
            const Settings::WindowConfig& w = application.windows[i];

            XMLElement *window = xmlDoc.NewElement( "Window" );
            window->SetAttribute("id", i);
            window->SetAttribute("name", w.name.c_str());
			window->SetAttribute("x", w.x);
			window->SetAttribute("y", w.y);
			window->SetAttribute("w", w.w);
			window->SetAttribute("h", w.h);
            window->SetAttribute("f", w.fullscreen);
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
    applicationNode->SetAttribute("smooth_snapshot", application.smooth_snapshot);
    applicationNode->SetAttribute("smooth_cursor", application.smooth_cursor);
    applicationNode->SetAttribute("action_history_follow_view", application.action_history_follow_view);
    applicationNode->SetAttribute("accept_connections", application.accept_connections);
    applicationNode->SetAttribute("pannel_history_mode", application.pannel_history_mode);
    pRoot->InsertEndChild(applicationNode);

    // Widgets
    XMLElement *widgetsNode = xmlDoc.NewElement( "Widgets" );
    widgetsNode->SetAttribute("preview", application.widget.preview);
    widgetsNode->SetAttribute("preview_view", application.widget.preview_view);
    widgetsNode->SetAttribute("history", application.widget.history);
    widgetsNode->SetAttribute("media_player", application.widget.media_player);
    widgetsNode->SetAttribute("media_player_view", application.widget.media_player_view);
    widgetsNode->SetAttribute("timeline_editmode", application.widget.timeline_editmode);
    widgetsNode->SetAttribute("shader_editor", application.widget.shader_editor);
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
    pRoot->InsertEndChild(RenderNode);

    // Record
    XMLElement *RecordNode = xmlDoc.NewElement( "Record" );
    RecordNode->SetAttribute("path", application.record.path.c_str());
    RecordNode->SetAttribute("profile", application.record.profile);
    RecordNode->SetAttribute("timeout", application.record.timeout);
    RecordNode->SetAttribute("delay", application.record.delay);
    pRoot->InsertEndChild(RecordNode);

    // Transition
    XMLElement *TransitionNode = xmlDoc.NewElement( "Transition" );
    TransitionNode->SetAttribute("hide_windows", application.transition.hide_windows);
    TransitionNode->SetAttribute("cross_fade", application.transition.cross_fade);
    TransitionNode->SetAttribute("duration", application.transition.duration);
    TransitionNode->SetAttribute("profile", application.transition.profile);
    pRoot->InsertEndChild(TransitionNode);

    // Source
    XMLElement *SourceConfNode = xmlDoc.NewElement( "Source" );
    SourceConfNode->SetAttribute("new_type", application.source.new_type);
    SourceConfNode->SetAttribute("ratio", application.source.ratio);
    SourceConfNode->SetAttribute("res", application.source.res);
    pRoot->InsertEndChild(SourceConfNode);

    // Brush
    XMLElement *BrushNode = xmlDoc.NewElement( "Brush" );
    BrushNode->InsertEndChild( XMLElementFromGLM(&xmlDoc, application.brush) );
    pRoot->InsertEndChild(BrushNode);

    // bloc connections
    {
        XMLElement *connectionsNode = xmlDoc.NewElement( "Connections" );

//        map<int, std::string>::iterator iter;
//        for (iter=application.instance_names.begin(); iter != application.instance_names.end(); iter++)
//        {
//            XMLElement *connection = xmlDoc.NewElement( "Instance" );
//            connection->SetAttribute("name", iter->second.c_str());
//            connection->SetAttribute("id", iter->first);
//            connectionsNode->InsertEndChild(connection);
//        }
        pRoot->InsertEndChild(connectionsNode);
    }

    // bloc views
    {
        XMLElement *viewsNode = xmlDoc.NewElement( "Views" );
        // save current view only if [mixing, geometry, layers, appearance]
        int v = application.current_view > 4 ? 1 : application.current_view;
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

        XMLElement *recentsession = xmlDoc.NewElement( "Session" );
        recentsession->SetAttribute("path", application.recentSessions.path.c_str());
        recentsession->SetAttribute("autoload", application.recentSessions.load_at_start);
        recentsession->SetAttribute("autosave", application.recentSessions.save_on_exit);
        recentsession->SetAttribute("valid", application.recentSessions.front_is_valid);
        for(auto it = application.recentSessions.filenames.begin();
            it != application.recentSessions.filenames.end(); ++it) {
            XMLElement *fileNode = xmlDoc.NewElement("path");
            XMLText *text = xmlDoc.NewText( (*it).c_str() );
            fileNode->InsertEndChild( text );
            recentsession->InsertFirstChild(fileNode);
        };
        recent->InsertEndChild(recentsession);

        XMLElement *recentfolder = xmlDoc.NewElement( "Folder" );
        for(auto it = application.recentFolders.filenames.begin();
            it != application.recentFolders.filenames.end(); ++it) {
            XMLElement *fileNode = xmlDoc.NewElement("path");
            XMLText *text = xmlDoc.NewText( (*it).c_str() );
            fileNode->InsertEndChild( text );
            recentfolder->InsertFirstChild(fileNode);
        };
        recent->InsertEndChild(recentfolder);

        XMLElement *recentmedia = xmlDoc.NewElement( "Import" );
        recentmedia->SetAttribute("path", application.recentImport.path.c_str());
        for(auto it = application.recentImport.filenames.begin();
            it != application.recentImport.filenames.end(); ++it) {
            XMLElement *fileNode = xmlDoc.NewElement("path");
            XMLText *text = xmlDoc.NewText( (*it).c_str() );
            fileNode->InsertEndChild( text );
            recentmedia->InsertFirstChild(fileNode);
        }
        recent->InsertEndChild(recentmedia);

        pRoot->InsertEndChild(recent);
    }


    // First save : create filename
    if (settingsFilename.empty())
        settingsFilename = SystemToolkit::full_filename(SystemToolkit::settings_path(), APP_SETTINGS);

    XMLError eResult = xmlDoc.SaveFile(settingsFilename.c_str());
    XMLResultError(eResult);

}

void Settings::Load()
{
    // impose C locale for all app
    setlocale(LC_ALL, "C");

    XMLDocument xmlDoc;
    if (settingsFilename.empty())
        settingsFilename = SystemToolkit::full_filename(SystemToolkit::settings_path(), APP_SETTINGS);
    XMLError eResult = xmlDoc.LoadFile(settingsFilename.c_str());

	// do not warn if non existing file
    if (eResult == XML_ERROR_FILE_NOT_FOUND)
        return;
    // warn and return on other error
    else if (XMLResultError(eResult))
        return;

    XMLElement *pRoot = xmlDoc.FirstChildElement(application.name.c_str());
    if (pRoot == nullptr) return;

    // cancel on different root name
    if (application.name.compare( string( pRoot->Value() ) ) != 0 )
        return;

#ifdef VIMIX_VERSION_MAJOR
    // cancel on different version
    int version_major = -1, version_minor = -1;
    pRoot->QueryIntAttribute("major", &version_major);
    pRoot->QueryIntAttribute("minor", &version_minor);
    if (version_major != VIMIX_VERSION_MAJOR || version_minor != VIMIX_VERSION_MINOR)
        return;
#endif

    XMLElement * applicationNode = pRoot->FirstChildElement("Application");
    if (applicationNode != nullptr) {
        applicationNode->QueryFloatAttribute("scale", &application.scale);
        applicationNode->QueryIntAttribute("accent_color", &application.accent_color);
        applicationNode->QueryBoolAttribute("smooth_transition", &application.smooth_transition);
        applicationNode->QueryBoolAttribute("smooth_snapshot", &application.smooth_snapshot);
        applicationNode->QueryBoolAttribute("smooth_cursor", &application.smooth_cursor);
        applicationNode->QueryBoolAttribute("action_history_follow_view", &application.action_history_follow_view);
        applicationNode->QueryBoolAttribute("accept_connections", &application.accept_connections);
        applicationNode->QueryIntAttribute("pannel_history_mode", &application.pannel_history_mode);
    }

    // Widgets
    XMLElement * widgetsNode = pRoot->FirstChildElement("Widgets");
    if (widgetsNode != nullptr) {
        widgetsNode->QueryBoolAttribute("preview", &application.widget.preview);
        widgetsNode->QueryIntAttribute("preview_view", &application.widget.preview_view);
        widgetsNode->QueryBoolAttribute("history", &application.widget.history);
        widgetsNode->QueryBoolAttribute("media_player", &application.widget.media_player);
        widgetsNode->QueryIntAttribute("media_player_view", &application.widget.media_player_view);
        widgetsNode->QueryBoolAttribute("timeline_editmode", &application.widget.timeline_editmode);
        widgetsNode->QueryBoolAttribute("shader_editor", &application.widget.shader_editor);
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
    }

    // Record
    XMLElement * recordnode = pRoot->FirstChildElement("Record");
    if (recordnode != nullptr) {
        recordnode->QueryIntAttribute("profile", &application.record.profile);
        recordnode->QueryUnsignedAttribute("timeout", &application.record.timeout);
        recordnode->QueryIntAttribute("delay", &application.record.delay);

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
    }

    // Transition
    XMLElement * transitionnode = pRoot->FirstChildElement("Transition");
    if (transitionnode != nullptr) {
        transitionnode->QueryBoolAttribute("hide_windows", &application.transition.hide_windows);
        transitionnode->QueryBoolAttribute("cross_fade", &application.transition.cross_fade);
        transitionnode->QueryFloatAttribute("duration", &application.transition.duration);
        transitionnode->QueryIntAttribute("profile", &application.transition.profile);
    }

    // bloc windows
    {
        XMLElement * pElement = pRoot->FirstChildElement("Windows");
        if (pElement)
        {
            XMLElement* windowNode = pElement->FirstChildElement("Window");
            for( ; windowNode ; windowNode=windowNode->NextSiblingElement())
            {
                Settings::WindowConfig w;
                w.name = std::string(windowNode->Attribute("name"));
                windowNode->QueryIntAttribute("x", &w.x); // If this fails, original value is left as-is
                windowNode->QueryIntAttribute("y", &w.y);
                windowNode->QueryIntAttribute("w", &w.w);
                windowNode->QueryIntAttribute("h", &w.h);
                windowNode->QueryBoolAttribute("f", &w.fullscreen);
                w.monitor = std::string(windowNode->Attribute("m"));

                int i = 0;
                windowNode->QueryIntAttribute("id", &i);
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

    // bloc Connections
    {
        XMLElement * pElement = pRoot->FirstChildElement("Connections");
        if (pElement)
        {
//            XMLElement* connectionNode = pElement->FirstChildElement("Instance");
//            for( ; connectionNode ; connectionNode=connectionNode->NextSiblingElement())
//            {
//                int id = 0;
//                connectionNode->QueryIntAttribute("id", &id);
//                application.instance_names[id] = connectionNode->Attribute("name");
//            }
        }

    }

    // bloc history of recent
    {
        XMLElement * pElement = pRoot->FirstChildElement("Recent");
        if (pElement)
        {
            // recent session filenames
            XMLElement * pSession = pElement->FirstChildElement("Session");
            if (pSession)
            {
                const char *path_ = pSession->Attribute("path");
                if (path_)
                    application.recentSessions.path = std::string(path_);
                else
                    application.recentSessions.path = SystemToolkit::home_path();
                application.recentSessions.filenames.clear();
                XMLElement* path = pSession->FirstChildElement("path");
                for( ; path ; path = path->NextSiblingElement())
                {
                    const char *p = path->GetText();
                    if (p)
                    application.recentSessions.push( std::string (p) );
                }
                pSession->QueryBoolAttribute("autoload", &application.recentSessions.load_at_start);
                pSession->QueryBoolAttribute("autosave", &application.recentSessions.save_on_exit);
                pSession->QueryBoolAttribute("valid", &application.recentSessions.front_is_valid);
            }
            // recent session folders
            XMLElement * pFolder = pElement->FirstChildElement("Folder");
            if (pFolder)
            {
                application.recentFolders.filenames.clear();
                XMLElement* path = pFolder->FirstChildElement("path");
                for( ; path ; path = path->NextSiblingElement())
                {
                    const char *p = path->GetText();
                    if (p)
                    application.recentFolders.push( std::string (p) );
                }
            }
            // recent media uri
            XMLElement * pImport = pElement->FirstChildElement("Import");
            if (pImport)
            {
                const char *path_ = pImport->Attribute("path");
                if (path_)
                    application.recentImport.path = std::string(path_);
                else
                    application.recentImport.path = SystemToolkit::home_path();
                application.recentImport.filenames.clear();
                XMLElement* path = pImport->FirstChildElement("path");
                for( ; path ; path = path->NextSiblingElement())
                {
                    const char *p = path->GetText();
                    if (p)
                    application.recentImport.push( std::string (p) );
                }
            }
        }
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
	Settings::Save();

    XMLDocument xmlDoc;
    XMLError eResult = xmlDoc.LoadFile(settingsFilename.c_str());
    if (XMLResultError(eResult))
        return;

	xmlDoc.Print();
}

