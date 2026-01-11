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

#include <glib.h>
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

    // set resolution & update to draw framebuffer
    output_session_->setResolution(Rendering::manager().monitorsResolution());
    output_session_->update( 0.f );
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

    // reset all canvas surfaces to attach to new framebuffer
    for (auto cit = begin(); cit != end(); ++cit) {
        (*cit)->reload();
        (*cit)->touch();
    }
}


void Canvas::attachCanvasSource(CanvasSource *cs)
{
    if (!cs)
        return;

    std::lock_guard<std::mutex> lock(output_mutex_);

    // activate it
    cs->setActive( true );

    // set initial depth 
    float depth = output_session_->empty() ? LAYER_BACKGROUND : output_session_->depthRange().second;
    cs->group(View::LAYER)->translation_.z = depth + LAYER_STEP; 

    // add to output session
    output_session_->addSource( cs );
    cs->touch();

    // request reordering of scene at next update
    ++View::need_deep_update_;

    // attach to displays view scene
    DisplaysView *displays_ = static_cast<DisplaysView *>(Mixer::manager().view(View::DISPLAYS));
    displays_->scene.ws()->attach( cs->group(View::GEOMETRY) ); 
}

void Canvas::detachCanvasSource(CanvasSource *cs)
{
    if (!cs)
        return;

    std::lock_guard<std::mutex> lock(output_mutex_);

    DisplaysView *displays_ = static_cast<DisplaysView *>(Mixer::manager().view(View::DISPLAYS));
    displays_->scene.ws()->detach( cs->group(View::GEOMETRY) ); 

    cs->setActive( false );
    output_session_->deleteSource( cs );
}


void Canvas::bringToFront(CanvasSource *cs)
{
    if (!cs)
        return;

    // get max depth
    float depth = output_session_->depthRange().second;

    // already at front
    if (cs->depth() >= depth)
        return;

    std::lock_guard<std::mutex> lock(output_mutex_);

    if ( depth > MAX_DEPTH - LAYER_STEP ) {
        // renormalize all depths to avoid overflow
        depth = LAYER_BACKGROUND;
        SourceList dsl = output_session_->getDepthSortedList();
        for (auto it : dsl) {
            depth += LAYER_STEP;
            it->group(View::LAYER)->translation_.z = depth;
        }
    }
    
    // set depth to front
    cs->group(View::LAYER)->translation_.z = depth + LAYER_STEP; 

    // request reordering of scene at next update
    ++View::need_deep_update_;

    // request update of source
    cs->touch();
}

void Canvas::sendToBack(CanvasSource *cs)
{
    if (!cs)
        return;

    // get min depth
    float depth = output_session_->depthRange().first;

    // already at back
    if (cs->depth() <= depth)
        return;

    std::lock_guard<std::mutex> lock(output_mutex_);

    if ( depth < MIN_DEPTH + LAYER_STEP ) {
        // renormalize all depths to avoid overflow
        depth = LAYER_BACKGROUND;
        SourceList dsl = output_session_->getDepthSortedList();
        for (auto it : dsl) {
            depth += LAYER_STEP;
            it->group(View::LAYER)->translation_.z = depth;
        }        
        depth = LAYER_BACKGROUND;
    }

    // set depth to back
    cs->group(View::LAYER)->translation_.z = depth - LAYER_STEP; 

    // request reordering of scene at next update
    ++View::need_deep_update_;

    // request update of source
    cs->touch();
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
            if (fail == Source::FAIL_FATAL) {
                // in case FAIL_FATAL ; delete source
                detachCanvasSource( static_cast<CanvasSource *>(*it));
            }
            else if (fail == Source::FAIL_CRITICAL) {
                // revert to default canvas if critical failure
                CanvasSource *cs = static_cast<CanvasSource *>(*it);
                cs->setCanvas(0);
                cs->reload();
                cs->touch();
            }
            else if (fail == Source::FAIL_RETRY) {
                // Retry if resolution of output changed : just reload
                (*it)->reload();
                (*it)->touch();
            }
        }

        // if resolution of output framebuffer changed, update session resolution
        if (output_session_->frame()) {
            glm::vec3 res = Rendering::manager().monitorsResolution();
            if( output_session_->frame()->resolution() != res)
                output_session_->setResolution(res);
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

    // First main element is the list of canvases
    XMLElement * canvasesNode = pRoot->FirstChildElement("Canvases");
    if (canvasesNode == nullptr)
        return;

    // use a session loader visitor to load canvases as sources
    SessionLoader canvas_loader;

    // iterate over XML source nodes
    SourceList::iterator canvas_it = canvases_.begin();

    // loop over list of canvas as source nodes
    XMLElement* sourceNode = canvasesNode->FirstChildElement("Source");
    for(; sourceNode ; sourceNode = sourceNode->NextSiblingElement("Source"))
    {
        // create a new canvas if needed
        if ( canvas_it == canvases_.end() ) {
            addSurface();
            canvas_it = --canvases_.end();
        }

        // session loader loads source configuration
        canvas_loader.setCurrentXML( sourceNode );
        (*canvas_it)->accept( canvas_loader );
        ++canvas_it;
    }
    // delete extra canvases if any
    while ( canvas_it != canvases_.end() ) {
        removeSurface();
        canvas_it = --canvases_.end();  
    }

    // Second main element is the list of canvas sources for output session
    XMLElement * sessionNode = pRoot->FirstChildElement("CanvasSources");
    if (sessionNode == nullptr)
        return;

    // TODO read id of sessionNode to check if it matches output_session_ id ?    

    // sessionsources contains list of ids of all sources currently in the session (before loading)
    SourceIdList previous_sources = output_session_->getIdList();

    // use a session loader visitor to load canvas sources for output session
    SessionLoader session_loader;

    // loop over session source nodes
    XMLElement* sourceCanvasNode = sessionNode->FirstChildElement("Source");
    for(; sourceCanvasNode ; sourceCanvasNode = sourceCanvasNode->NextSiblingElement("Source"))
    {
        // source to load
        CanvasSource *load_source = nullptr;

        // read the xml id of this source element
        uint64_t id_xml_ = 0;
        sourceCanvasNode->QueryUnsigned64Attribute("id", &id_xml_);

        // check if a source with the given id exists in the output session
        SourceList::iterator sit = output_session_->find(id_xml_);

        // no source with this id exists
        if ( sit == output_session_->end() ) {
            // create a new source depending on type
            const char *pType = sourceCanvasNode->Attribute("type");
            if (!pType) 
                continue;
            else if ( std::string(pType) == "CanvasSource") 
                load_source = new CanvasSource(id_xml_);
            else 
                continue;

            // add to output session
            attachCanvasSource( load_source );
        }
        // get reference to the existing source
        else {
            load_source = static_cast<CanvasSource*>(*sit);
            // remove from previous sources list
            previous_sources.remove( id_xml_ );
        }
    
        // use session loader to load source configuration
        session_loader.setCurrentXML( sourceCanvasNode );
        load_source->accept(session_loader);

        // make sure source is updated
        load_source->touch();

    }

    // remaining ids in list previous_sources : to remove from output session
    for ( auto psit = previous_sources.begin(); psit != previous_sources.end(); psit++ ) {
        SourceList::iterator sit = output_session_->find( *psit );
        if (sit != output_session_->end()) {
            detachCanvasSource( static_cast<CanvasSource *>(*sit) );   
        }
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
    SessionVisitor sv_canvas(&xmlDoc, canvasesNode);
    for (auto iter = begin(); iter != end(); ++iter, sv_canvas.setRoot(canvasesNode) )
        // session visitor saves source configuration
        (*iter)->accept(sv_canvas);

    // lock access 
    std::lock_guard<std::mutex> lock(output_mutex_);

    // Save list of canvas sources
    XMLElement *sessionNode = xmlDoc.NewElement("CanvasSources");
    pRoot->InsertEndChild(sessionNode);
    SessionVisitor sv_session(&xmlDoc, sessionNode);
    for (auto iter = output_session_->begin(); iter != output_session_->end(); ++iter, sv_session.setRoot(sessionNode) )
        // source visitor
        (*iter)->accept(sv_session);

    // save canvases config
    XMLError eResult = xmlDoc.SaveFile( settingsFilename.c_str());
    XMLResultError(eResult);
    
}
