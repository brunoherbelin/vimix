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

#include <sstream>
#include <iomanip>

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

Canvas::Canvas() : framebuffer_(nullptr)
{
    canvas_scene_ = new Group;
}


void Canvas::init()
{
    // minimum of one canvas
    add();

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

    // delete canvas scene
    delete canvas_scene_;
}

void Canvas::setFrameBuffer(FrameBuffer *fb)
{
    // detect change of aspect ratio
    if ( framebuffer_ && framebuffer_->aspectRatio() != fb->aspectRatio() ) {
        // adjust all canvases x translation coordinate to new aspect ratio
        for (auto cit = begin(); cit != end(); ++cit) {
            (*cit)->group(View::GEOMETRY)->translation_.x *= fb->aspectRatio() / framebuffer_->aspectRatio();
        }
    }

    // set new framebuffer
    framebuffer_ = fb;

    // reset all canvases to attach to new framebuffer
    for (auto cit = begin(); cit != end(); ++cit) {
        (*cit)->reload();
    }
}

void Canvas::update()
{
    // Render canvases
    for (auto cit = begin(); cit != end(); ++cit) {
        // render each canvas with their section of the background surface
        (*cit)->update(0.f);
        (*cit)->render();
    }
}

void Canvas::add()
{
    // create a new canvas source
    CanvasSource *canvas = new CanvasSource;

    // set Canvas name and label characters with 2-digit format
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << (canvases_.size() + 1);
    std::string label_number = oss.str();
    canvas->setName( "Canvas" + label_number );
    canvas->label_0_->setChar(label_number[label_number.size() - 2]);
    canvas->label_1_->setChar(label_number[label_number.size() - 1]);

    // add to list of canvases
    canvases_.push_back( canvas );

    // activate it
    canvas->setActive( true );

    // attach to geometry view scene 
    canvas_scene_->attach( canvas->groups_[View::GEOMETRY] );
    canvas_scene_->attach( canvas->frames_[View::GEOMETRY] );
}

void Canvas::remove()
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

SourceList::iterator Canvas::begin ()
{
    return canvases_.begin();
}

SourceList::iterator Canvas::end ()
{
    return canvases_.end();
}

CanvasSource *Canvas::at (size_t index) {

    auto it = canvases_.begin();
    std::advance(it, MIN(index, canvases_.size() - 1));
    // NB : assume that canvases_ contains only CanvasSource elements and has at least one element
    return static_cast<CanvasSource *>(*it);
}

void Canvas::setLayout(int rows, int columns)
{
    // Implementation for setting layout.
    // Total display area is [-1, 1] in both x and y directions.
    // Center of display is at (0,0).
    // Each canvas will crop the display area in a grid defined by (columns x rows).
    // example: 2 columns and 1 row: 
    // canvas 1: crop = (-1, 0, 1, -1), scale = (0.5, 1, 1), translation = (-0.5, 0, 0) 
    // canvas 2: crop = (0, 1, 1, -1), scale = (0.5, 1, 1), translation = (0.5, 0, 0)
    // NB : the x axis is then multiplied by the aspect ratio of the framebuffer in CanvasSource::render(), 

    if (canvases_.size() == 0)
        return;
    else if (canvases_.size() == 1) {
        (*canvases_.begin())->group(View::GEOMETRY)->translation_ = glm::vec3(0.f, 0.f, 0.f);
        (*canvases_.begin())->group(View::GEOMETRY)->scale_ = glm::vec3(1.f, 1.f, 1.f);
        (*canvases_.begin())->group(View::GEOMETRY)->crop_ = glm::vec4(-1.f, 1.f, 1.f, -1.f);
    }
    // general case (more than one canvas); follow columns and rows
    else {
        // Calculate width and height of each cell in normalized coordinates
        float cellWidth = 2.0f / columns;   // Total width is 2.0 (from -1 to 1)
        float cellHeight = 2.0f / rows;     // Total height is 2.0 (from -1 to 1)

        // Scale for each canvas (half the cell size since scale is from center)
        float scaleX = cellWidth / 2.0f;
        float scaleY = cellHeight / 2.0f;

        int canvasIndex = 0;
        for (auto cit = begin(); cit != end() && canvasIndex < columns * rows; ++cit, ++canvasIndex) {
            // Calculate row and column for this canvas
            int row = canvasIndex / columns;
            int col = canvasIndex % columns;

            // Calculate crop area for this cell
            float left = -1.0f + col * cellWidth;
            float right = left + cellWidth;
            float top = 1.0f - row * cellHeight;
            float bottom = top - cellHeight;

            // Calculate translation (center position of the cell)
            float transX = left + scaleX;
            float transY = top - scaleY;

            // Apply transformation to canvas
            (*cit)->group(View::GEOMETRY)->crop_ = glm::vec4(left, right, top, bottom);
            (*cit)->group(View::GEOMETRY)->scale_ = glm::vec3(scaleX, scaleY, 1.f);
            (*cit)->group(View::GEOMETRY)->translation_ = glm::vec3(transX * framebuffer_->aspectRatio(), transY, 0.f);

            // make sure the canvas is updated
            (*cit)->touch();
        }
    }

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

    // no canvases found
    if (sourceNode == nullptr)
        return;

    // iterate over XML source nodes
    for(int i = 0 ; sourceNode ; sourceNode = sourceNode->NextSiblingElement("Source"))
    {
        // create a new canvas if needed
        if ( canvas_it == canvases_.end() ) {
            add();
            canvas_it = --canvases_.end();
        }

        // session loader loads source configuration
        loader.setCurrentXML( sourceNode );
        (*canvas_it)->accept( loader );
        ++canvas_it;
    }
    // delete extra canvases if any
    while ( canvas_it != canvases_.end() ) {
        remove();
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
    for (auto iter = begin(); iter != end(); ++iter, sv.setRoot(canvasesNode) )
        // session visitor saves source configuration
        (*iter)->accept(sv);

    // save canvases config
    XMLError eResult = xmlDoc.SaveFile( settingsFilename.c_str());
    XMLResultError(eResult);
    
}