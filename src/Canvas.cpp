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
#include "Session.h"
#include "Mixer.h"

#include "Canvas.h"

Canvas::Canvas() : framebuffer_(nullptr)
{
    output_session_ = new Session;
}

void Canvas::init()
{
    // minimum of one canvas
    addSurface();

    // load configuration
    load();

    output_session_->setResolution(Rendering::manager().monitorsResolution());
    //  update to draw framebuffer
    output_session_->update( 0.f );


    // start with one source to put a canvas on
    CanvasSource *s = new CanvasSource;
    attachCanvasSource( s );

}

void Canvas::terminate()
{
    // save configuration
    save();

    // empty the output session
    while ( !output_session_->empty() ) 
        output_session_->popSource();

    // delete all canvases
    while ( canvases_.size() > 0 ) {        
        Source *tmp = canvases_.back();
        GeometryView *geometryView = static_cast<GeometryView *>(Mixer::manager().view(View::GEOMETRY));
        geometryView->detach(tmp);
        canvases_.pop_back();
        delete tmp;
    }

}

void Canvas::setInputFrameBuffer(FrameBuffer *fb)
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


void Canvas::attachCanvasSource(CanvasSource *cs)
{
    if (!cs)
        return;

    cs->setActive( true );
    output_session_->addSource( cs );
    
    DisplaysView *displays_ = static_cast<DisplaysView *>(Mixer::manager().view(View::DISPLAYS));
    displays_->scene.ws()->attach( cs->group(View::GEOMETRY) ); 
}

void Canvas::detachCanvasSource(CanvasSource *cs)
{
    if (!cs)
        return;

    cs->setActive( false );
    output_session_->deleteSource( cs );

    DisplaysView *displays_ = static_cast<DisplaysView *>(Mixer::manager().view(View::DISPLAYS));
    displays_->scene.ws()->detach( cs->group(View::GEOMETRY) ); 
}

FrameBuffer *Canvas::getRenderedFrameBuffer() const
{
    return output_session_->frame();
}

void Canvas::update(float dt)
{
    // Render canvases
    for (auto cit = begin(); cit != end(); ++cit) {
        // render each canvas with their section of the background surface
        (*cit)->update(dt);
        (*cit)->render();
    }

    // update all CanvasSources in the output session
    if ( output_session_ ) {

        // update session
        output_session_->update(dt);

        // manage failed SessionSources 
        SourceListUnique _failedsources = output_session_->failedSources();
        for(auto it = _failedsources.begin(); it != _failedsources.end(); ++it)  {
            // intervention depends on the severity of the failure
            Source::Failure fail = (*it)->failed();
            if (fail == Source::FAIL_RETRY) {
                // resolution of output changed : recreate source / update ? TODO
            }
            else {
                // only other case is FAIL_FATAL ; delete source
                output_session_->deleteSource(*it);
            }
        }
    }
}

void Canvas::addSurface()
{
    // create a new canvas source
    CanvasSurface *canvas = new CanvasSurface;

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
    GeometryView *geometryView = static_cast<GeometryView *>(Mixer::manager().view(View::GEOMETRY));
    geometryView->attach(canvas);
}

void Canvas::removeSurface()
{
    // minumum one canvas
    if ( canvases_.size() > 1 ) {

        // get last canvas
        Source *tmp = canvases_.back();

        // detach from scene
        GeometryView *geometryView = static_cast<GeometryView *>(Mixer::manager().view(View::GEOMETRY));
        geometryView->detach(tmp);

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

CanvasSurface *Canvas::at (size_t index) 
{    
    if (canvases_.size() == 0 || index >= canvases_.size())
        return nullptr;

    auto it = canvases_.begin();
    std::advance(it, MIN(index, canvases_.size() - 1));
    return static_cast<CanvasSurface *>(*it);
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
            addSurface();
            canvas_it = --canvases_.end();
        }

        // session loader loads source configuration
        loader.setCurrentXML( sourceNode );
        (*canvas_it)->accept( loader );
        ++canvas_it;
    }
    // delete extra canvases if any
    while ( canvas_it != canvases_.end() ) {
        removeSurface();
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
