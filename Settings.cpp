#include <algorithm>
#include <iostream>
using namespace std;

#include <tinyxml2.h>
#include "tinyxml2Toolkit.h"
using namespace tinyxml2;

#include "defines.h"
#include "Settings.h"
#include "SystemToolkit.h"


Settings::Application Settings::application;
static string settingsFilename = "";

void Settings::Save()
{
    XMLDocument xmlDoc;
    XMLDeclaration *pDec = xmlDoc.NewDeclaration();
    xmlDoc.InsertFirstChild(pDec);

    XMLElement *pRoot = xmlDoc.NewElement(application.name.c_str());
    xmlDoc.InsertEndChild(pRoot);

    string comment = "Settings for " + application.name;
    comment += "Version " + std::to_string(APP_VERSION_MAJOR) + "." + std::to_string(APP_VERSION_MINOR);
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
            window->SetAttribute("m", w.monitor);
            windowsNode->InsertEndChild(window);
		}

        pRoot->InsertEndChild(windowsNode);
	}

    XMLElement *applicationNode = xmlDoc.NewElement( "Application" );
    applicationNode->SetAttribute("scale", application.scale);
    applicationNode->SetAttribute("accent_color", application.accent_color);
    applicationNode->SetAttribute("preview", application.preview);
    applicationNode->SetAttribute("media_player", application.media_player);
    applicationNode->SetAttribute("shader_editor", application.shader_editor);
    applicationNode->SetAttribute("stats", application.stats);
    applicationNode->SetAttribute("stats_corner", application.stats_corner);
    applicationNode->SetAttribute("logs", application.logs);
    applicationNode->SetAttribute("toolbox", application.toolbox);
    applicationNode->SetAttribute("framebuffer_ar", application.framebuffer_ar);
    applicationNode->SetAttribute("framebuffer_h", application.framebuffer_h);
    applicationNode->SetAttribute("multisampling_level", application.multisampling_level);
    pRoot->InsertEndChild(applicationNode);

    // bloc views
    {
        XMLElement *viewsNode = xmlDoc.NewElement( "Views" );
        viewsNode->SetAttribute("current", application.current_view);

        map<int, Settings::ViewConfig>::iterator iter;
        for (iter=application.views.begin(); iter != application.views.end(); iter++)
        {
            const Settings::ViewConfig& v = iter->second;

            XMLElement *view = xmlDoc.NewElement( "View" );
            view->SetAttribute("name", v.name.c_str());
            view->SetAttribute("id", iter->first);

            XMLElement *scale = xmlDoc.NewElement("default_scale");
            scale->InsertEndChild( XMLElementFromGLM(&xmlDoc, v.default_scale) );
            view->InsertEndChild(scale);
            XMLElement *translation = xmlDoc.NewElement("default_translation");
            translation->InsertEndChild( XMLElementFromGLM(&xmlDoc, v.default_translation) );
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
        for(auto it = application.recentSessions.filenames.begin();
            it != application.recentSessions.filenames.end(); it++) {
            XMLElement *fileNode = xmlDoc.NewElement("path");
            XMLText *text = xmlDoc.NewText( (*it).c_str() );
            fileNode->InsertEndChild( text );
            recentsession->InsertFirstChild(fileNode);
        };
        recent->InsertEndChild(recentsession);

        XMLElement *recentmedia = xmlDoc.NewElement( "Import" );
        recentmedia->SetAttribute("path", application.recentImport.path.c_str());
        for(auto it = application.recentImport.filenames.begin();
            it != application.recentImport.filenames.end(); it++) {
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
        settingsFilename = SystemToolkit::settings_prepend_path(APP_SETTINGS);

    XMLError eResult = xmlDoc.SaveFile(settingsFilename.c_str());
    XMLResultError(eResult);
}

void Settings::Load()
{
    XMLDocument xmlDoc;
    if (settingsFilename.empty())
        settingsFilename = SystemToolkit::settings_prepend_path(APP_SETTINGS);
    XMLError eResult = xmlDoc.LoadFile(settingsFilename.c_str());

	// do not warn if non existing file
    if (eResult == XML_ERROR_FILE_NOT_FOUND)
        return;
    // warn and return on other error
    else if (XMLResultError(eResult))
        return;

    XMLElement *pRoot = xmlDoc.FirstChildElement(application.name.c_str());
    if (pRoot == nullptr) return;

    if (application.name.compare( string( pRoot->Value() ) ) != 0 ) 
        // different root name
        return;

    XMLElement * pElement = pRoot->FirstChildElement("Application");
    if (pElement == nullptr) return;
    pElement->QueryFloatAttribute("scale", &application.scale);
    pElement->QueryIntAttribute("accent_color", &application.accent_color);
    pElement->QueryBoolAttribute("preview", &application.preview);
    pElement->QueryBoolAttribute("media_player", &application.media_player);
    pElement->QueryBoolAttribute("shader_editor", &application.shader_editor);
    pElement->QueryBoolAttribute("stats", &application.stats);
    pElement->QueryBoolAttribute("logs", &application.logs);
    pElement->QueryBoolAttribute("toolbox", &application.toolbox);
    pElement->QueryIntAttribute("stats_corner", &application.stats_corner);
    pElement->QueryIntAttribute("framebuffer_ar", &application.framebuffer_ar);
    pElement->QueryIntAttribute("framebuffer_h", &application.framebuffer_h);
    pElement->QueryIntAttribute("multisampling_level", &application.multisampling_level);

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
                windowNode->QueryIntAttribute("m", &w.monitor);

                int i = 0;
                windowNode->QueryIntAttribute("id", &i);
                application.windows[i] = w;
            }
        }
	}

    // bloc views
    {
        application.views.clear(); // trash existing list
        XMLElement * pElement = pRoot->FirstChildElement("Views");
        if (pElement)
        {
            pElement->QueryIntAttribute("current", &application.current_view);

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
            XMLElement * pSession = pElement->FirstChildElement("Session");
            if (pSession)
            {
                const char *path_ = pSession->Attribute("path");
                if (path_)
                    application.recentSessions.path = std::string(path_);
                else
                    application.recentSessions.path = SystemToolkit::home_path();
                pSession->QueryBoolAttribute("autoload", &application.recentSessions.load_at_start);
                pSession->QueryBoolAttribute("autosave", &application.recentSessions.save_on_exit);
                application.recentSessions.filenames.clear();
                XMLElement* path = pSession->FirstChildElement("path");
                for( ; path ; path = path->NextSiblingElement())
                {
                    const char *p = path->GetText();
                    if (p)
                    application.recentSessions.push( std::string (p) );
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


void Settings::Check()
{
	Settings::Save();

    XMLDocument xmlDoc;
    XMLError eResult = xmlDoc.LoadFile(settingsFilename.c_str());
    if (XMLResultError(eResult))
        return;

	xmlDoc.Print();
}
