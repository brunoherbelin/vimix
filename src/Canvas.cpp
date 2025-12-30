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
using namespace tinyxml2;

#include "defines.h"
#include "Toolkit/tinyxml2Toolkit.h"
#include "Toolkit/SystemToolkit.h"
#include "Visitor/SessionVisitor.h"
#include "SessionCreator.h"
#include "Source/CanvasSource.h"    
#include "Scene/Primitives.h"
#include "Scene/Decorations.h"
#include "FrameBuffer.h"

#include "Canvas.h"

Canvas::Canvas() : canvas_surface_(nullptr)
{
    canvas_scene_ = new Group;
}


void Canvas::init()
{
    // minimum of one canvas
    addCanvas();

    // load configuration
    load();
}

void Canvas::terminate()
{
    // save configuration
    save();

    // delete all canvases
    while ( canvases_.size() > 0 ) {        
        Source *tmp = canvases_.back();
        canvas_scene_->detach( tmp->groups_[View::GEOMETRY] );
        canvas_scene_->detach( tmp->frames_[View::GEOMETRY] );
        canvases_.pop_back();
        delete tmp;
    }

    // delete canvas surface
    if ( canvas_surface_ != nullptr) {
        delete canvas_surface_;
        canvas_surface_ = nullptr;
    }

    // delete canvas scene
    delete canvas_scene_;
}

void Canvas::setFrameBuffer(FrameBuffer *fb)
{
    // create surface to render canvases
    if ( canvas_surface_ == nullptr) 
        canvas_surface_ = new FrameBufferSurface(fb);
    
    // update canvas surface framebuffer
    canvas_surface_->setFrameBuffer(fb);
    canvas_surface_->scale_.x = fb->aspectRatio();

    // reset all canvases
    for (auto cit = canvasBegin(); cit != canvasEnd(); ++cit) {
        (*cit)->reload();
    }
}

void Canvas::update()
{
    // Render canvases
    for (auto cit = canvasBegin(); cit != canvasEnd(); ++cit) {
        // render each canvas with their section of the background surface
        (*cit)->update(0.f);
        (*cit)->render();
    }
}

void Canvas::addCanvas()
{
    // create a new canvas source
    CanvasSource *canvas = new CanvasSource;
    canvas->setName( "Canvas " + std::to_string( canvases_.size() + 1) );

    // set Canvas label characters
    std::string label_number = std::to_string( canvases_.size() + 1); 
    char c0 = label_number.size() >= 1 ? label_number[label_number.size() - 1] : '0';
    char c1 = label_number.size() >= 2 ? label_number[label_number.size() - 2] : '0';
    canvas->label_0_->setChar(c1);
    canvas->label_1_->setChar(c0);

    // add to list of canvases
    canvases_.push_back( canvas );

    // activate it
    canvas->setActive( true );

    // attach to geometry view scene 
    canvas_scene_->attach( canvas->groups_[View::GEOMETRY] );
    canvas_scene_->attach( canvas->frames_[View::GEOMETRY] );
}

void Canvas::removeCanvas()
{
    // minumum one canvas
    if ( canvases_.size() > 1 ) {

        // get last canvas
        Source *tmp = canvases_.back();

        // detach from scene
        canvas_scene_->detach( tmp->groups_[View::GEOMETRY] );
        canvas_scene_->detach( tmp->frames_[View::GEOMETRY] );

        // remove last canvas from list
        canvases_.pop_back();

        // delete it
        delete tmp;
    }
}

SourceList::iterator Canvas::canvasBegin ()
{
    return canvases_.begin();
}

SourceList::iterator Canvas::canvasEnd ()
{
    return canvases_.end();
}



void Canvas::load(const std::string &filename)
{
    // read settings file of default configuration
    std::string settingsFilename = filename;
    if (settingsFilename.empty()) 
        settingsFilename = SystemToolkit::full_filename(SystemToolkit::settings_path(), APP_CANVAS);

    // try to load settings file
    XMLDocument xmlDoc;
    XMLError eResult = xmlDoc.LoadFile(settingsFilename.c_str());

	// do not warn if non existing file
    if (eResult == XML_ERROR_FILE_NOT_FOUND)
        return;
    // warn and return on other error
    else if (XMLResultError(eResult))
        return;

    // first element should be the application name
    XMLElement *pRoot = xmlDoc.FirstChildElement(APP_NAME);
    if (pRoot == nullptr)
        return;

    // first child should be canvases
    XMLElement * canvasesNode = pRoot->FirstChildElement("Canvases");
    if (canvasesNode == nullptr)
        return;
    
    // use a session loader visitor to load canvases as sources
    SessionLoader loader;

    SourceList::iterator canvas_it = canvases_.begin();
    XMLElement* sourceNode = canvasesNode->FirstChildElement("Source");
    for(int i = 0 ; sourceNode ; sourceNode = sourceNode->NextSiblingElement("Source"))
    {
        // create a new canvas if needed
        if ( canvas_it == canvases_.end() ) {
            addCanvas();
            canvas_it = --canvases_.end();
        }

        // session loader loads source configuration
        loader.setCurrentXML( sourceNode );
        (*canvas_it)->accept( loader );
        ++canvas_it;
    }
    // delete extra canvases if any
    while ( canvas_it != canvases_.end() ) {
        removeCanvas();
        canvas_it = --canvases_.end();  
    }
}

void Canvas::save(const std::string &filename)
{
    // save settings file or default configuration
    std::string settingsFilename = filename;
    if (settingsFilename.empty()) 
        settingsFilename = SystemToolkit::full_filename(SystemToolkit::settings_path(), APP_CANVAS);
    
    // creation of XML doc
    setlocale(LC_ALL, "C");
    XMLDocument xmlDoc;
    XMLDeclaration *pDec = xmlDoc.NewDeclaration();
    xmlDoc.InsertFirstChild(pDec);

    // first element is application name
    XMLElement *pRoot = xmlDoc.NewElement(APP_NAME);
    xmlDoc.InsertEndChild(pRoot);

    // list of canvases
    // TODO : muliple list of Canvases as set of configurations ?
    XMLElement *canvasesNode = xmlDoc.NewElement("Canvases");
    pRoot->InsertEndChild(canvasesNode);
    SessionVisitor sv(&xmlDoc, canvasesNode);
    for (auto iter = canvasBegin(); iter != canvasEnd(); ++iter, sv.setRoot(canvasesNode) )
        // session visitor saves source configuration
        (*iter)->accept(sv);

    // save canvases config
    XMLError eResult = xmlDoc.SaveFile( settingsFilename.c_str());
    XMLResultError(eResult);
    
}