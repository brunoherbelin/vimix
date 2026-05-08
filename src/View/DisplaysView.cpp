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

#include "IconsFontAwesome5.h"
#include "imgui.h"

#include <algorithm>
#include <cstddef>
#include <glib.h>
#include <iomanip>
#include <string>
#include <sstream>

#include <glad/glad.h>
#include <glm/fwd.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/vector_angle.hpp>

#include "defines.h"
#include "Mixer.h"
#include "Settings.h"
#include "Scene/Scene.h"
#include "Source/SourceList.h"
#include "Source/Source.h"
#include "Source/CanvasSource.h"
#include "Visitor/PickingVisitor.h"
#include "Visitor/DrawVisitor.h"
#include "Visitor/BoundingBoxVisitor.h"
#include "Toolkit/ImGuiToolkit.h"
#include "Scene/Decorations.h"
#include "UserInterfaceManager.h"
#include "RenderingManager.h"
#include "GeometryHandleManipulation.h"
#include "MousePointer.h"
#include "Canvas.h"

#include "DisplaysView.h"

#define WINDOW_TITLEBAR_HEIGHT 0.03f

FilteringProgram _whitebalance("Whitebalance", "shaders/filters/whitebalance.glsl", "", { { "Red", 1.0}, { "Green", 1.0}, { "Blue", 1.0}, { "Temperature", 0.5} });



DisplaysView::DisplaysView() : View(DISPLAYS)
{
    scene.root()->scale_ = glm::vec3(DISPLAYS_DEFAULT_SCALE, DISPLAYS_DEFAULT_SCALE, 1.0f);
    monitors_scaling_ratio_ = 1.f;

    // read default settings
    if ( Settings::application.views[mode_].name.empty() ) {
        // no settings found: store application default
        saveSettings();
    }
    else
        restoreSettings();
    Settings::application.views[mode_].name = "Displays";

    //
    // Monitors in the background
    //
    // a surface showing bounding box of all monitors
    boundingbox_surface_ = new Surface;
    boundingbox_surface_->visible_ = true;
    boundingbox_surface_->shader()->color = glm::vec4(1.f, 1.f, 1.f, 0.4f);
    scene.bg()->attach(boundingbox_surface_);

    // monitors layout surfaces and frames
    monitors_layout_ = new Group;
    monitors_layout_->visible_ = true;
    scene.bg()->attach(monitors_layout_);
    horizontal_layout_ = true;

    //
    // User interface foreground
    //
    // point to show POSITION
    overlay_position_ = new Symbol(Symbol::SQUARE_POINT);
    overlay_position_->scale_ = glm::vec3(0.5f, 0.5f, 1.f);
    scene.fg()->attach(overlay_position_);
    overlay_position_->visible_ = false;
    // cross to show the axis for POSITION
    overlay_position_cross_ = new Symbol(Symbol::GRID);
    overlay_position_cross_->scale_ = glm::vec3(0.5f, 0.5f, 1.f);
    scene.fg()->attach(overlay_position_cross_);
    overlay_position_cross_->visible_ = false;
    // 'clock' : tic marks every 10 degrees for ROTATION
    // with dark background
    overlay_rotation_clock_ = new Group;
    overlay_rotation_clock_tic_ = new Symbol(Symbol::CLOCK);
    overlay_rotation_clock_->attach(overlay_rotation_clock_tic_);
    Symbol *s = new Symbol(Symbol::CIRCLE_POINT);
    s->color = glm::vec4(0.f, 0.f, 0.f, 0.1f);
    s->scale_ = glm::vec3(28.f, 28.f, 1.f);
    s->translation_.z = -0.1;
    overlay_rotation_clock_->attach(s);
    overlay_rotation_clock_->scale_ = glm::vec3(0.25f, 0.25f, 1.f);
    scene.fg()->attach(overlay_rotation_clock_);
    overlay_rotation_clock_->visible_ = false;
    // circle to show fixed-size  ROTATION
    overlay_rotation_clock_hand_ = new Symbol(Symbol::CLOCK_H);
    overlay_rotation_clock_hand_->scale_ = glm::vec3(0.25f, 0.25f, 1.f);
    scene.fg()->attach(overlay_rotation_clock_hand_);
    overlay_rotation_clock_hand_->visible_ = false;
    overlay_rotation_fix_ = new Symbol(Symbol::SQUARE);
    overlay_rotation_fix_->scale_ = glm::vec3(0.25f, 0.25f, 1.f);
    scene.fg()->attach(overlay_rotation_fix_);
    overlay_rotation_fix_->visible_ = false;
    // circle to show the center of ROTATION
    overlay_rotation_ = new Symbol(Symbol::CIRCLE);
    overlay_rotation_->scale_ = glm::vec3(0.25f, 0.25f, 1.f);
    scene.fg()->attach(overlay_rotation_);
    overlay_rotation_->visible_ = false;
    // 'grid' : tic marks every 0.1 step for SCALING
    // with dark background
    Group *g = new Group;
    s = new Symbol(Symbol::GRID);
    s->scale_ = glm::vec3(1.655f, 1.655f, 1.f);
    g->attach(s);
    s = new Symbol(Symbol::SQUARE_POINT);
    s->color = glm::vec4(0.f, 0.f, 0.f, 0.1f);
    s->scale_ = glm::vec3(17.f, 17.f, 1.f);
    s->translation_.z = -0.1;
    g->attach(s);
    overlay_scaling_grid_ = g;
    overlay_scaling_grid_->scale_ = glm::vec3(0.3f, 0.3f, 1.f);
    scene.fg()->attach(overlay_scaling_grid_);
    overlay_scaling_grid_->visible_ = false;
    // cross in the square for proportional SCALING
    overlay_scaling_cross_ = new Symbol(Symbol::CROSS);
    overlay_scaling_cross_->scale_ = glm::vec3(0.3f, 0.3f, 1.f);
    scene.fg()->attach(overlay_scaling_cross_);
    overlay_scaling_cross_->visible_ = false;
    // square to show the center of SCALING
    overlay_scaling_ = new Symbol(Symbol::SQUARE);
    overlay_scaling_->scale_ = glm::vec3(0.3f, 0.3f, 1.f);
    scene.fg()->attach(overlay_scaling_);
    overlay_scaling_->visible_ = false;
    // frame to show the full image size for CROP
    Frame *border = new Frame(Frame::SHARP, Frame::THIN, Frame::NONE);
    border->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 0.2f );
    overlay_crop_ = border;
    scene.fg()->attach(overlay_crop_);
    overlay_crop_->visible_ = false;

    // will be init later
    overlay_selection_scale_ = nullptr;
    overlay_selection_rotate_ = nullptr;
    overlay_selection_stored_status_ = nullptr;
    overlay_selection_active_ = false;

    // replace grid with appropriate one
    translation_grid_ = new TranslationGrid(scene.root());
    translation_grid_->root()->visible_ = false;
    rotation_grid_ = new RotationGrid(scene.root());
    rotation_grid_->root()->visible_ = false;
    if (grid) delete grid;
    grid = translation_grid_;

    // manage canvas sources
    current_canvas_source_ = nullptr;

    // initial layouting
    recenter();
}

void DisplaysView::update(float dt)
{
    View::update(dt);

    // a more complete update is requested
    if (View::need_deep_update_ > 0) {

        // update rendering of render frame
        FrameBuffer *output = Canvas::manager().session()->frame();
        if (output){

            boundingbox_surface_->setTextureIndex( output->texture() );

            // set grid aspect ratio
            if (Settings::application.proportional_grid)
                translation_grid_->setAspectRatio( output->aspectRatio() );
            else
                translation_grid_->setAspectRatio( 1.f );
        }

        // change grid color
        ImVec4 c = ImGuiToolkit::HighlightColor();
        translation_grid_->setColor( glm::vec4(c.x, c.y, c.z, 0.3) );
        rotation_grid_->setColor( glm::vec4(c.x, c.y, c.z, 0.3) );
    }

    // specific update when this view is active
    if ( Mixer::manager().view() == this ) {

        // update the selection overlay
        const ImVec4 c = ImGuiToolkit::HighlightColor();
        updateSelectionOverlay(glm::vec4(c.x, c.y, c.z, c.w));
        
    }
}

float setCoordinates(Node *node, glm::ivec4 coordinates, glm::ivec4 bounding_box)
{
    glm::vec4 rect = DISPLAYS_UNIT * glm::vec4(coordinates);
    
    // screen coordinates to scene coordinates
    node->scale_ = glm::vec3( 0.5f * rect.z, 0.5f * rect.w, 1.f );
    node->translation_ = glm::vec3( rect.x + 0.5f * rect.z, - (rect.y + 0.5f * rect.w), 0.f );

    // centered on bounding box
    node->translation_.x -= 0.5f * DISPLAYS_UNIT * (bounding_box.x + bounding_box.z);
    node->translation_.y += 0.5f * DISPLAYS_UNIT * (bounding_box.y + bounding_box.w);

    // scaling to fit in scene
    // scale to height (w component of bounding box) 
    // because projection in OutputWindow is based on height 1.0
    float scalingratio = 1.f / (0.5f * DISPLAYS_UNIT * float(bounding_box.w) );
    node->scale_ *= glm::vec3( scalingratio, scalingratio, 1.f ); 
    node->translation_ *= glm::vec3( scalingratio, scalingratio, 1.f ); 

    return scalingratio;
}

// recenter is called also when RenderingManager detects a change of monitors
void  DisplaysView::recenter ()
{
    glm::ivec4 bbox;
    bbox.x = bbox.y =  std::numeric_limits<int>::max();
    bbox.z = bbox.w =  std::numeric_limits<int>::min(); 

    // lock monitors list while reading it
    std::lock_guard<std::mutex> lock(Rendering::manager().getMonitorsMutex());

    // update bounding box
    for (auto& monitor : Rendering::manager().monitorsUnsafe()) {
        bbox.x = MIN(bbox.x, monitor.geometry.x);
        bbox.y = MIN(bbox.y, monitor.geometry.y);
        bbox.z = MAX(bbox.z, monitor.geometry.x + monitor.geometry.z);
        bbox.w = MAX(bbox.w, monitor.geometry.y + monitor.geometry.w);  
    }
    monitors_scaling_ratio_ = setCoordinates(boundingbox_surface_, bbox, bbox);

    // fill scene background with the frames to show monitors
    // Reset and fill monitors_layout_ and list of monitors_
    int index = 1;
    monitors_layout_->clear();
    monitors_.clear();
    for (auto& monitor : Rendering::manager().monitorsUnsafe()) {

        Switch *monitor_switch = new Switch;
        setCoordinates(monitor_switch, monitor.geometry, bbox);

        // DISABLED
        Group *m1 = new Group;
        Surface *surf = new Surface( new Shader);
        surf->shader()->color =  glm::vec4( 0.1f, 0.1f, 0.1f, 0.8f );
        m1->attach(surf);
        // Monitor color frame
        Frame *frame = new Frame(Frame::SHARP, Frame::THIN, Frame::NONE);
        frame->color = glm::vec4( COLOR_MONITOR, 0.8f);
        m1->attach(frame);
        // central label
        Character  *label = new Character(4);
        label->setChar( std::to_string(index).back() );
        label->color = glm::vec4( COLOR_MONITOR, 0.8f );
        label->translation_.y =  0.015f ;
        label->scale_.y =  0.25f / (DISPLAYS_UNIT * monitor.geometry.w);
        m1->attach(label);

        // ENABLED
        Group *m2 = new Group;
        Surface *surf2 = new Surface( new Shader);
        surf2->shader()->color =  glm::vec4( 0.05f, 0.05f, 0.05f, 1.f );
        m2->attach(surf2);
        // Monitor color frame
        Frame *frame2 = new Frame(Frame::SHARP, Frame::THIN, Frame::GLOW);
        frame2->color = glm::vec4( COLOR_MONITOR, 1.f);
        m2->attach(frame2);
        // central label
        m2->attach(label);

        // SETUP SWITCH
        monitor_switch->attach( m1 );
        monitor_switch->attach( m2 );
        monitor_switch->setActive(0); // by default disabled

        // add monitor switch to layout
        monitors_layout_->attach( monitor_switch );
        monitors_[monitor.name] = monitor_switch;

        // count monitors
        ++index;
    }

    // Adjust scene scaling to fit all monitors in view
    // take top-left corner in scene coordinates as reference for extents of view
    glm::vec3 top_left = Rendering::manager().unProject(glm::vec3(0.f, 0.f, 0.f));
    glm::vec3 view = glm::abs(top_left);
    // reset scene translation
    scene.root()->translation_ = glm::vec3(0.f, 0.f, 0.f);
    // apply scene scaling depending on layout
    if (boundingbox_surface_->scale_.x >= boundingbox_surface_->scale_.y) {
        horizontal_layout_ = true;
        scene.root()->scale_.x = view.x / (DISPLAYS_DEFAULT_SCALE * boundingbox_surface_->scale_.x) ;
        scene.root()->scale_.y = scene.root()->scale_.x;
        scene.root()->translation_.x = scene.root()->scale_.x / 5.f;
    }
    else {
        horizontal_layout_ = false;
        scene.root()->scale_.y = view.y / (DISPLAYS_DEFAULT_SCALE * boundingbox_surface_->scale_.y) ;
        scene.root()->scale_.x = scene.root()->scale_.y;
    }

}

void DisplaysView::resize ( int scale )
{
    // compute scale
    float z = CLAMP(0.01f * (float) scale, 0.f, 1.f);
    z *= z; // square
    z *= DISPLAYS_MAX_SCALE - DISPLAYS_MIN_SCALE;
    z += DISPLAYS_MIN_SCALE;

    // centered zoom : if zooming, adjust translation to ratio of scaling
    if (scale != size())
        scene.root()->translation_ *= z / scene.root()->scale_.x;

    // apply new scale
    scene.root()->scale_.x = z;
    scene.root()->scale_.y = z;

    // Clamp translation to acceptable area
    glm::vec3 border = boundingbox_surface_->scale_ * scene.root()->scale_;
    scene.root()->translation_ = glm::clamp(scene.root()->translation_, -border, border);

}

int  DisplaysView::size ()
{
    float z = (scene.root()->scale_.x - DISPLAYS_MIN_SCALE) / (DISPLAYS_MAX_SCALE - DISPLAYS_MIN_SCALE);
    return (int) ( sqrt(z) * 100.f);
}


void popup_adjustment_color(Settings::MonitorConfig &conf) {
    
    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_DEFAULT);

    // top icons
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 1.f, 1.0f)); // High Kelvin = blue
    ImGui::Text("  " ICON_FA_THERMOMETER_FULL "  ");
    ImGui::PopStyleColor(1);
    if (ImGui::IsItemHovered())
        ImGuiToolkit::ToolTip("Color Temperature, in Kelvin");
    ImGui::SameLine(0, IMGUI_SAME_LINE);
    ImGuiToolkit::Indication("Contrast", 2, 1);
    ImGui::SameLine(0, IMGUI_SAME_LINE);
    ImGuiToolkit::Indication("Brightness", 4, 1 );

    // Slider Temperature K
    ImGui::VSliderFloat("##Temperatureslider", ImVec2(30,260), &conf.whitebalance.w, 0.0, 1.0, "");
    if (ImGui::IsItemHovered() || ImGui::IsItemActive() )  {
        ImGui::BeginTooltip();
        ImGui::Text("%d K", 4000 + (int) ceil(conf.whitebalance.w * 5000.f));
        ImGui::EndTooltip();
    }
    // Slider Contrast
    ImGui::SameLine(0, IMGUI_SAME_LINE);
    ImGui::VSliderFloat("##contrastslider", ImVec2(30,260), &conf.contrast, -0.5, 0.5, "");
    if (ImGui::IsItemHovered() || ImGui::IsItemActive() )  {
        ImGui::BeginTooltip();
        ImGui::Text("%.1f %%", (100.f * conf.contrast));
        ImGui::EndTooltip();
    }
    // Slider Brightness
    ImGui::SameLine(0, IMGUI_SAME_LINE);
    ImGui::VSliderFloat("##brightnessslider", ImVec2(30,260), &conf.brightness, -0.5, 0.5, "");
    if (ImGui::IsItemHovered() || ImGui::IsItemActive() )  {
        ImGui::BeginTooltip();
        ImGui::Text("%.1f %%", (100.f * conf.brightness));
        ImGui::EndTooltip();
    }

    // bottom icons
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.4f, 0.4f, 1.0f));  // Low Kelvin = red
    ImGui::Text("  " ICON_FA_THERMOMETER_EMPTY  "  ");
    ImGui::PopStyleColor(1);
    ImGui::SameLine(0, IMGUI_SAME_LINE);
    ImGuiToolkit::Icon(1, 1, false);
    ImGui::SameLine(0, IMGUI_SAME_LINE);
    ImGuiToolkit::Icon(3, 1, false);

    ImGui::PopFont();
}

void DisplaysView::addCanvasSource()
{
    // create a canvas source of default Canvas 01 
    CanvasSource *s = new CanvasSource;
    Canvas::manager().attachCanvasSource( s );

    // initialize its frame size to match canvas resolution in monitors space
    CanvasSurface *canvas = Canvas::manager().at(0);
    float AR = canvas->aspectRatio();
    float ryh = canvas->resolution().y * canvas->scale().y * DISPLAYS_UNIT * 0.5f * monitors_scaling_ratio_;
    s->group(View::GEOMETRY)->scale_.y = ryh;
    s->group(View::GEOMETRY)->scale_.x = ryh * AR;
    s->group(View::GEOMETRY)->rotation_.z = 0;
    s->group(View::GEOMETRY)->translation_ = monitors_.begin()->second->translation_;
    s->touch();

    // set as current canvas source
    setCurrentCanvasSource(s);
}

void DisplaysView::removeCurrentCanvasSource()
{
    // detach current source (this deletes it from session)
    CanvasSource *cs = dynamic_cast<CanvasSource *>(current_canvas_source_);
    if (cs) {
        setCurrentCanvasSource( nullptr );
        Canvas::manager().detachCanvasSource(cs);
    }

    // set another current source if any
    if ( Canvas::manager().session()->size() > 0 )
        setCurrentCanvasSource( *Canvas::manager().session()->begin() );
}

void DisplaysView::removeCurrentCanvasSelection()
{
    // get copy of selected sources before clearing selection
    SourceList tmp = selected_canvas_sources_.getCopy();
    selected_canvas_sources_.clear();

    // detach all sources that were in the selection
    for ( auto cs : tmp ) {
        if ( cs == current_canvas_source_ )
            setCurrentCanvasSource( nullptr );
        Canvas::manager().detachCanvasSource( static_cast<CanvasSource*>(cs));
    }

    // set another current source if any
    if ( Canvas::manager().session()->size() > 0 )
        setCurrentCanvasSource( *Canvas::manager().session()->begin() );
}

void DisplaysView::menuCanvasSource ()
{
    // Buttons to add new CanvasSource
    glm::vec3 pos;
    pos = boundingbox_surface_->translation_;
    pos.x -= boundingbox_surface_->scale_.x;
    pos.y += boundingbox_surface_->scale_.y;

    glm::vec2 P = Rendering::manager().project(pos, scene.root()->transform_,
                Settings::application.mainwindow.fullscreen);

    // Set window position depending on icons size
    if (!horizontal_layout_)
        ImGui::SetNextWindowPos(ImVec2(P.x - IMGUI_SAME_LINE, P.y - ImGui::GetFrameHeightWithSpacing() - IMGUI_TOP_ALIGN ), ImGuiCond_Always);
    else
        ImGui::SetNextWindowPos(ImVec2(P.x - 2.f * ImGui::GetFrameHeightWithSpacing(), P.y), ImGuiCond_Always);

    if (ImGui::Begin("CanvasSourcesManipulation", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground
                    | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings
                    | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus ))
    {
        // colors for UI
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.14f, 0.14f, 0.14f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.15f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.16f, 0.16f, 0.16f, 0.99f));
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.85f, 0.85f, 0.85f, 0.86f));
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.95f, 0.95f, 0.95f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.15f, 0.15f, 0.99f));

        const float padding = ImGui::GetStyle().FramePadding.x ;
        const float spacing = ImGui::GetStyle().FrameRounding ;
        const float button_width = ImGui::GetFrameHeightWithSpacing() + spacing + padding;

        // OUTPUT DISABLED button
        ImGuiToolkit::ButtonToggle(ICON_FA_EYE_SLASH, &Settings::application.render.disabled);
        if (!horizontal_layout_)
            ImGui::SameLine(0, IMGUI_SAME_LINE);
        if (ImGui::IsItemHovered())
                ImGuiToolkit::ToolTip(MENU_OUTPUTDISABLE, SHORTCUT_OUTPUTDISABLE);

        // - action REMOVE canvas source
        if (Canvas::manager().session()->size() > 0 && current_canvas_source_ != nullptr) {
            if (ImGui::Button(ICON_FA_MINUS, ImVec2(button_width, 0))) {
                removeCurrentCanvasSource();
            }
            if (ImGui::IsItemHovered())
                ImGuiToolkit::ToolTip("Remove selected canvas frame");
        }
        else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.f, 0.f, 0.f, 0.f));
            ImGui::Button(ICON_FA_MINUS, ImVec2(button_width, 0));
            ImGui::PopStyleColor(2);
        }

        if (!horizontal_layout_)
            ImGui::SameLine(0, IMGUI_SAME_LINE);

        // + action ADD new canvas source
        if (ImGui::Button(ICON_FA_PLUS, ImVec2(button_width, 0))) {
            addCanvasSource();
        }
        if (ImGui::IsItemHovered())
            ImGuiToolkit::ToolTip("Add new canvas frame");
        
        ImGui::PopStyleColor(8);
        ImGui::End();
    }
}

void DisplaysView::draw()
{
    // render CanvasSources of Canvas::manager
    std::vector<Node *> source_surfaces;
    std::vector<Node *> source_overlays;

    for (auto source_iter = Canvas::manager().session()->begin();
            source_iter != Canvas::manager().session()->end(); ++source_iter) {
                
        // will draw its surface
        source_surfaces.push_back((*source_iter)->rendersurface_);
        // will draw its frame and manipulators
        source_overlays.push_back((*source_iter)->overlays_[GEOMETRY]);
        source_overlays.push_back((*source_iter)->frames_[GEOMETRY]);
    }

    // 0. prepare projection for draw visitors
    glm::mat4 projection = Rendering::manager().Projection();

    // Draw background display of monitors bounding box
    DrawVisitor draw_rendering(boundingbox_surface_, projection);
    scene.accept(draw_rendering);

    // Draw background display of monitors layout
    // TODO MAKE IT OPTIONAL ?
    DrawVisitor draw_monitors(monitors_layout_, projection);
    scene.accept(draw_monitors);

    // Draw surface of Canvas sources 
    DrawVisitor draw_sources(source_surfaces, projection); 
    scene.accept(draw_sources);

    // Draw frames and icons of Canvas sources on top 
    DrawVisitor draw_overlays(source_overlays, projection);
    scene.accept(draw_overlays);

    // Finally, draw overlays of view on top of all
    DrawVisitor draw_foreground(scene.fg(), projection);
    scene.accept(draw_foreground);

    // Display grid in overlay
    if (grid->active() && current_action_ongoing_) {
        const glm::mat4 projection = Rendering::manager().Projection();
        DrawVisitor draw_grid(grid->root(), projection, true);
        scene.accept(draw_grid);
    }
        
    //
    // 10. Display interface
    //
    {
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        // protected access to monitors list
        size_t monitor_index = 0;
        std::lock_guard<std::mutex> lock(Rendering::manager().getMonitorsMutex());
        for (auto& monitor : Rendering::manager().monitorsUnsafe()) {

            monitor_index++;
            std::string monitor_title = "Monitor " + std::to_string(monitor_index);
            monitor_title += "\n" + monitor.name;
            monitor_title += " (" + std::to_string(monitor.geometry.z) + "x" + std::to_string(monitor.geometry.w) + ")";

            // Locate monitor view upper left corner
            glm::vec3 pos;
            auto it = monitors_.find(monitor.name);
            if (it != monitors_.end()) {
                Switch* group = it->second;
                pos = group->translation_;
                pos.x -= group->scale_.x;
                pos.y += group->scale_.y;
                group->setActive(monitor.output.isActive() ? 1 : 0); // update active state
            }
            glm::vec2 P = Rendering::manager().project(pos, scene.root()->transform_,
                                Settings::application.mainwindow.fullscreen);

            // Set window position depending on icons size
            if (horizontal_layout_)
                ImGui::SetNextWindowPos(ImVec2(P.x - IMGUI_SAME_LINE, P.y - ImGui::GetFrameHeightWithSpacing() - IMGUI_TOP_ALIGN ), ImGuiCond_Always);
            else
                ImGui::SetNextWindowPos(ImVec2(P.x - 2.f * ImGui::GetFrameHeightWithSpacing(), P.y), ImGuiCond_Always);

            if (ImGui::Begin(monitor.name.c_str(), NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground
                            | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings
                            | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus ))
            {
                // colors for UI
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.14f, 0.14f, 0.14f, 0.9f));
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.15f, 0.5f));
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.16f, 0.16f, 0.16f, 0.99f));
                ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.85f, 0.85f, 0.85f, 0.86f));
                ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.95f, 0.95f, 0.95f, 1.00f));
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.15f, 0.15f, 0.99f));

                //
                // Buttons on top
                //
                if (horizontal_layout_)
                    ImGui::SameLine(0, IMGUI_SAME_LINE);
                if (monitor.output.isActive()) {
                    // deactivate output
                    if (ImGui::Button(ICON_FA_TOGGLE_ON )) 
                        monitor.output.setActive(false);
                    if (ImGui::IsItemHovered())
                        ImGuiToolkit::ToolTip(std::string("Deactivate ").append(monitor_title).c_str());
                }
                else {
                    // activate output
                    if (ImGui::Button(ICON_FA_TOGGLE_OFF )) 
                        monitor.output.setActive(true);
                    if (ImGui::IsItemHovered())
                        ImGuiToolkit::ToolTip(std::string("Activate ").append(monitor_title).c_str());

                    // skip rest of window
                    ImGui::PopStyleColor(8);
                    ImGui::End();
                    continue;
                }

                // TEST PATTERN BUTTON
                if (horizontal_layout_)
                    ImGui::SameLine(0, IMGUI_SAME_LINE);
                static bool show_test_pattern = false;  
                show_test_pattern = monitor.output.isShowPattern();
                if ( ImGuiToolkit::ButtonIconToggle(11,1, &show_test_pattern, "Test pattern") )
                    monitor.output.setShowPattern(show_test_pattern);

                // WHITE BALANCE BUTTON
                if (horizontal_layout_) {
                    ImGui::SameLine(0, IMGUI_SAME_LINE);
                    ImGui::SetCursorPosY(2.f * ImGui::GetStyle().FramePadding.y); // hack to align color button to text
                }
                
                ImGuiToolkit::PushFont(ImGuiToolkit::FONT_DEFAULT);
                glm::vec4 color = Settings::application.monitors[monitor.name].whitebalance;
                if (ImGui::ColorButton("White balance", ImVec4(color.x, color.y, color.z, 1.f),
                                    ImGuiColorEditFlags_NoAlpha)) {
                    if ( !DialogToolkit::ColorPickerDialog::instance().busy()) {
                        // prepare the color picker to start with white balance color
                        DialogToolkit::ColorPickerDialog::instance().setRGB( std::make_tuple(color.x, color.y, color.z) );
                        // declare function to be called
                        std::string m = monitor.name;
                        auto applyColor = [m](std::tuple<float, float, float> c)  {
                            Settings::application.monitors[m].whitebalance.x = std::get<0>(c);
                            Settings::application.monitors[m].whitebalance.y = std::get<1>(c);
                            Settings::application.monitors[m].whitebalance.z = std::get<2>(c);
                        };
                        // open dialog (starts a thread that will call the 'applyColor' function
                        DialogToolkit::ColorPickerDialog::instance().open( applyColor );
                    }
                }
                ImGui::PopFont();

                // COLOR ADJUSTMENT POPUP BUTTON
                if (horizontal_layout_) {
                    ImGui::SameLine(0, IMGUI_SAME_LINE);
                    ImGui::SetCursorPosY(ImGui::GetStyle().FramePadding.y); // hack to re-align sliders button back
                }

                if (ImGui::Button(ICON_FA_SLIDERS_H ICON_FA_SORT_DOWN )) {
                    ImGui::OpenPopup("adjustments_popup");
                }
                if (ImGui::BeginPopup("adjustments_popup", ImGuiWindowFlags_NoMove))  {
                    popup_adjustment_color(Settings::application.monitors[monitor.name]);
                    ImGui::EndPopup();
                }

                // RESET BUTTON
                if (horizontal_layout_) 
                    ImGui::SameLine(0, IMGUI_SAME_LINE);
                if (ImGui::Button(ICON_FA_BACKSPACE )) {
                    Settings::application.monitors[monitor.name] = Settings::MonitorConfig();
                }
                if (ImGui::IsItemHovered())
                    ImGuiToolkit::ToolTip("Reset adjustments");

                ImGui::PopStyleColor(8);
                ImGui::End();
            }

        }

        // canvas source manipulation menu
        menuCanvasSource ();

        ImGui::PopFont();
    }


    // display popup menu canvas source
    if (show_context_menu_ == MENU_SOURCE) {
        ImGui::OpenPopup( "DisplaysSourceContextMenu" );
        show_context_menu_ = MENU_NONE;
    }
    if (ImGui::BeginPopup("DisplaysSourceContextMenu")) {

        if (current_canvas_source_ == nullptr) {
            ImGui::EndPopup();
            return;
        }
        contextMenuCanvasSource();
        ImGui::EndPopup();
    }

    // display popup menu canvas selection
    if (show_context_menu_ == MENU_SELECTION) {
        ImGui::OpenPopup( "DisplaysSelectionContextMenu" );
        show_context_menu_ = MENU_NONE;
    }
    if (ImGui::BeginPopup("DisplaysSelectionContextMenu")) {
        contextMenuCanvasSelection();
        ImGui::EndPopup();
    }
}


void DisplaysView::contextMenuCanvasSource()
{
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(COLOR_MENU_HOVERED, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(COLOR_WINDOW, 1.f));

    // list of available canvases
    size_t selected = current_canvas_source_->canvas();
    for (auto canvas_index = 0;
        canvas_index < Canvas::manager().size(); ++canvas_index) {
    
        // label with canvas name
        std::string label = ICON_FA_BORDER_ALL "  ";
        label += Canvas::manager().at(canvas_index)->name();    

        // select a canvas surface for current canvas source
        if (ImGui::MenuItem(label.c_str(), NULL, selected == canvas_index)) {
            if (selected != canvas_index)
                current_canvas_source_->setCanvas(canvas_index);
        }   
    }
    
    ImGui::Separator();

    // CROP source manipulation mode
    if (current_canvas_source_->manipulator_->active() > 0) {
        if (ImGui::MenuItem(ICON_FA_VECTOR_SQUARE "  Switch to Shape mode"))
            current_canvas_source_->manipulator_->setActive(0);

        if (ImGui::MenuItem(ICON_FA_CROP_ALT "  Reset crop")) {
            current_canvas_source_->group(View::GEOMETRY)->crop_ = glm::vec4(-1.f, 1.f, 1.f, -1.f);
            current_canvas_source_->touch();
        }
        ImGui::Text(ICON_FA_ANGLE_LEFT);
        ImGui::SameLine(18);
        if (ImGui::MenuItem(ICON_FA_ANGLE_RIGHT "   Reset corners")) {
            current_canvas_source_->group(View::GEOMETRY)->data_ = glm::zero<glm::mat4>();
            current_canvas_source_->touch();
        }
    }
    // SHAPE source manipulation mode
    else {
        if (ImGui::MenuItem( ICON_FA_CROP_ALT "  Switch to Crop mode" ))
            current_canvas_source_->manipulator_->setActive(1);

        if (ImGui::MenuItem( ICON_FA_VECTOR_SQUARE "  Reset shape" )){
            CanvasSurface *canvas = Canvas::manager().at(current_canvas_source_->canvas());
            float AR = canvas->aspectRatio();
            current_canvas_source_->group(View::GEOMETRY)->scale_ = glm::vec3(AR, 1.f, 1.f);
            current_canvas_source_->group(View::GEOMETRY)->rotation_.z = 0;
            current_canvas_source_->touch();
        }
        if (ImGui::MenuItem( ICON_FA_CIRCLE_NOTCH "  Reset rotation" )){
            current_canvas_source_->group(View::GEOMETRY)->rotation_.z = 0;
            current_canvas_source_->touch();
        }
    }

    // Restore aspect ratio of Canvas
    if (ImGui::MenuItem( ICON_FA_DESKTOP "  1:1 pixel ratio" )){

        CanvasSurface *canvas = Canvas::manager().at(current_canvas_source_->canvas());
        float AR = canvas->aspectRatio();
        float height = current_canvas_source_->group(View::GEOMETRY)->crop_[2] - current_canvas_source_->group(View::GEOMETRY)->crop_[3];
        float width = current_canvas_source_->group(View::GEOMETRY)->crop_[1] - current_canvas_source_->group(View::GEOMETRY)->crop_[0];
        float ryh = canvas->resolution().y * canvas->scale().y * height * DISPLAYS_UNIT * 0.25f * monitors_scaling_ratio_;
        current_canvas_source_->group(View::GEOMETRY)->scale_.y = ryh;
        current_canvas_source_->group(View::GEOMETRY)->scale_.x = current_canvas_source_->group(View::GEOMETRY)->scale_.y * AR;
        current_canvas_source_->group(View::GEOMETRY)->scale_.x *= width / height;
        current_canvas_source_->touch();
    }

    // list of fit to options
    if (ImGui::BeginMenu(ICON_FA_EXPAND "  Fit to...")) {

        float AR = current_canvas_source_->frame()->aspectRatio();

        // option to fit to each monitor
        // protected access to monitors list
        size_t monitor_index = 0;
        for (auto& monitor : Rendering::manager().monitorsUnsafe()) {

            monitor_index++;
            std::string monitor_title = "Monitor " + std::to_string(monitor_index);
            monitor_title += " (" + monitor.name + ")";

            if (ImGui::MenuItem( monitor_title.c_str() )) {
                auto it = monitors_.find(monitor.name);
                if (it != monitors_.end()) {
                    Switch* group = it->second;
                    current_canvas_source_->group(View::GEOMETRY)->scale_.x = group->scale_.x / AR;
                    current_canvas_source_->group(View::GEOMETRY)->scale_.y = group->scale_.y;
                    current_canvas_source_->group(View::GEOMETRY)->rotation_.z = 0;
                    current_canvas_source_->group(View::GEOMETRY)->translation_ = group->translation_;
                    current_canvas_source_->touch();
                }
            }
        }

        // for more than one monitor, option to fit to all monitors bounding box
        if (monitor_index > 1 && ImGui::MenuItem(  "All monitors" )){
            current_canvas_source_->group(View::GEOMETRY)->scale_ = glm::vec3(1.f);
            current_canvas_source_->group(View::GEOMETRY)->scale_.x = boundingbox_surface_->scale_.x / AR;
            current_canvas_source_->group(View::GEOMETRY)->rotation_.z = 0;
            current_canvas_source_->group(View::GEOMETRY)->translation_ = glm::vec3(0.f);
            current_canvas_source_->touch();
        }

        ImGui::EndMenu();
    }

    ImGui::Separator();
    if (ImGui::MenuItem( ICON_FA_ARROW_UP "  Bring to front" ))
        Canvas::manager().bringToFront(current_canvas_source_);

    if (ImGui::MenuItem( ICON_FA_ARROW_DOWN "  Send to back" ))
        Canvas::manager().sendToBack(current_canvas_source_);

    ImGui::PopStyleColor(2);
}


void DisplaysView::contextMenuCanvasSelection()
{        
    // colored context menu
    ImGui::PushStyleColor(ImGuiCol_Text, ImGuiToolkit::HighlightColor());
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(COLOR_MENU_HOVERED, 0.8f));

    if (ImGui::Selectable( ICON_FA_CROSSHAIRS "  Center" )){
        glm::mat4 T = glm::translate(glm::identity<glm::mat4>(), -overlay_selection_->translation_);
        initiate();
        applySelectionTransform(T);
        terminate();
    }
    if (ImGui::Selectable( ICON_FA_COMPASS "  Align" )){
        for (auto sit = selected_canvas_sources_.begin(); sit != selected_canvas_sources_.end(); ++sit){
            (*sit)->group(View::GEOMETRY)->rotation_.z = overlay_selection_->rotation_.z;
            (*sit)->touch();
        }
    }
    if (ImGui::Selectable( ICON_FA_OBJECT_GROUP "  Best Fit" )){
        glm::mat4 T = glm::translate(glm::identity<glm::mat4>(), -overlay_selection_->translation_);
        float factor = 1.f;
        float angle = -overlay_selection_->rotation_.z;
        if ( overlay_selection_->scale_.x < overlay_selection_->scale_.y) {
            factor *= boundingbox_surface_->scale_.x / overlay_selection_->scale_.y;
            angle += glm::pi<float>() / 2.f;
        }
        else {
            factor *= boundingbox_surface_->scale_.x / overlay_selection_->scale_.x;
        }
        glm::mat4 S = glm::scale(glm::identity<glm::mat4>(), glm::vec3(factor, factor, 1.f));
        glm::mat4 R = glm::rotate(glm::identity<glm::mat4>(), angle, glm::vec3(0.f, 0.f, 1.f) );
        glm::mat4 M = S * R * T;
        initiate();
        applySelectionTransform(M);
        terminate();
    }
    if (ImGui::Selectable( ICON_FA_EXCHANGE_ALT "  Mirror" )){
        glm::mat4 T = glm::translate(glm::identity<glm::mat4>(), -overlay_selection_->translation_);
        glm::mat4 F = glm::scale(glm::identity<glm::mat4>(), glm::vec3(-1.f, 1.f, 1.f));
        glm::mat4 M = glm::inverse(T) * F * T;
        initiate();
        applySelectionTransform(M);
        terminate();
    }
    ImGui::PopStyleColor(2);
}

void DisplaysView::adaptGridToSource(Source *s, Node *picked)
{
    // Reset by default
    rotation_grid_->root()->translation_ = glm::vec3(0.f);
    rotation_grid_->root()->scale_ = glm::vec3(1.f);
    translation_grid_->root()->translation_ = glm::vec3(0.f);
    translation_grid_->root()->rotation_.z  = 0.f;

    if (s != nullptr && picked != nullptr) {
        if (picked == s->handles_[View::GEOMETRY][Handles::ROTATE]) {
            // shift grid at center of source
            rotation_grid_->root()->translation_ = s->group(View::GEOMETRY)->translation_;
            rotation_grid_->root()->scale_.x = glm::length(
                        glm::vec2(s->frame()->aspectRatio() * s->group(View::GEOMETRY)->scale_.x,
                                  s->group(View::GEOMETRY)->scale_.y) );
            rotation_grid_->root()->scale_.y = rotation_grid_->root()->scale_.x;
            // Swap grid to rotation grid
            rotation_grid_->setActive( grid->active() );
            translation_grid_->setActive( false );
            grid = rotation_grid_;
            return;
        }
        else if ( picked == s->handles_[View::GEOMETRY][Handles::RESIZE] ||
                  picked == s->handles_[View::GEOMETRY][Handles::RESIZE_V] ||
                  picked == s->handles_[View::GEOMETRY][Handles::RESIZE_H] ){
            translation_grid_->root()->translation_ = glm::vec3(0.f);
            translation_grid_->root()->rotation_.z = s->group(View::GEOMETRY)->rotation_.z;
            // Swap grid to translation grid
            translation_grid_->setActive( grid->active() );
            rotation_grid_->setActive( false );
            grid = translation_grid_;
        }
        else if ( picked == s->handles_[View::GEOMETRY][Handles::SCALE] ||
                  picked == s->handles_[View::GEOMETRY][Handles::NODE_LOWER_LEFT] ||
                  picked == s->handles_[View::GEOMETRY][Handles::NODE_UPPER_LEFT] ||
                  picked == s->handles_[View::GEOMETRY][Handles::NODE_LOWER_RIGHT] ||
                  picked == s->handles_[View::GEOMETRY][Handles::NODE_UPPER_RIGHT] ||
                  picked == s->handles_[View::GEOMETRY][Handles::CROP_V] ||
                  picked == s->handles_[View::GEOMETRY][Handles::CROP_H] ){
            translation_grid_->root()->translation_ = s->group(View::GEOMETRY)->translation_;
            translation_grid_->root()->rotation_.z = s->group(View::GEOMETRY)->rotation_.z;
            // Swap grid to translation grid
            translation_grid_->setActive( grid->active() );
            rotation_grid_->setActive( false );
            grid = translation_grid_;
        }
    }
    else {
        // Default:
        // Grid in scene global coordinate
        translation_grid_->setActive( grid->active()  );
        rotation_grid_->setActive( false );
        grid = translation_grid_;
    }
}

std::pair<Node *, glm::vec2> DisplaysView::pick(glm::vec2 P)
{
    // prepare empty return value
    std::pair<Node *, glm::vec2> pick = { nullptr, glm::vec2(0.f) };

    // unproject mouse coordinate into scene coordinates
    glm::vec3 scene_point_ = Rendering::manager().unProject(P);

    // picking visitor traverses the scene
    PickingVisitor pv(scene_point_, false);
    scene.accept(pv);

    // picking visitor found nodes?
    if ( !pv.empty() ) {

        // keep current canvas active if it is clicked
        Source *picked_current_source = current_canvas_source_;
        if (picked_current_source != nullptr) {
            // find if the current canvas was picked
            auto itp = pv.rbegin();
            for (; itp != pv.rend(); ++itp){
                // test if canvas contains this node
                Source::hasNode is_in_source((*itp).first );
                if ( is_in_source( picked_current_source ) ){
                    // a node in the current canvas was clicked !
                    pick = *itp;
                    // adapt grid to prepare grab action
                    adaptGridToSource(picked_current_source, pick.first);
                    break;
                }
            }
            // not found: the current canvas was not clicked
            if (itp == pv.rend() ) {
                picked_current_source = nullptr;
                pick = { nullptr, glm::vec2(0.f) };
            }
            // picking on the menu handle: show context menu & reset picked sources
            else if ( pick.first == picked_current_source->handles_[View::GEOMETRY][Handles::MENU] ) {
                openContextMenu(MENU_SOURCE);
            }
            // picking on the crop handle : switch to shape manipulation mode
            else if (pick.first == picked_current_source->handles_[View::GEOMETRY][Handles::EDIT_CROP]) {
                picked_current_source->manipulator_->setActive(0);
            }
            // picking on the shape handle : switch to crop manipulation mode
            else if (pick.first == picked_current_source->handles_[View::GEOMETRY][Handles::EDIT_SHAPE]) {
                picked_current_source->manipulator_->setActive(1);
            }
            else {
                // ctrl pressed: maybe remove from selection
                if (UserInterface::manager().ctrlModifier()) {
                    if ( selected_canvas_sources_.contains(picked_current_source) && 
                         selected_canvas_sources_.size() > 1 ) {
                        selected_canvas_sources_.remove( picked_current_source );
                        setCurrentCanvasSource(selected_canvas_sources_.front());
                    }
                }
            }
        }
        // the clicked canvas source might have changed
        if (picked_current_source == nullptr) {

            if (pv.empty()) {
                setCurrentCanvasSource(nullptr);
                selected_canvas_sources_.clear();
                return pick;
            }

            // loop over all nodes picked
            for (auto itp = pv.rbegin(); itp != pv.rend(); ++itp){

                // loop over all canvas sources to get if one was picked
                SourceList::iterator sit = std::find_if(Canvas::manager().session()->begin(), 
                    Canvas::manager().session()->end(), Source::hasNode( (*itp).first ));

                // accept picked canvas source
                if ( sit != Canvas::manager().session()->end() ) {

                    Source *s = *sit;
                    
                    // if ctrl is pressed
                    if (UserInterface::manager().ctrlModifier()) {

                        if ( !selected_canvas_sources_.contains(s) )
                            selected_canvas_sources_.add( s );
                        else if ( selected_canvas_sources_.size() > 1 ) {
                            selected_canvas_sources_.remove( s );
                            s = selected_canvas_sources_.front();
                        }
                    } 
                    else {
                        // a source is picked but is not in an active selection? don't pick this one!
                        if ( selected_canvas_sources_.size() > 1 && !selected_canvas_sources_.contains(s) )
                            continue;
                    }
                    
                    if (s) {
                        // set as current canvas source
                        setCurrentCanvasSource(s);                            
                        pick = { s->group(View::GEOMETRY), (*itp).second };
                    }
                    else 
                        setCurrentCanvasSource(nullptr);        
                    break;
                }
                //  not a source clicked, maybe a handle of the selection?
                else if ( pick.first == nullptr && selected_canvas_sources_.size() > 1 ) {
                    
                    // picked on selection handles
                    if ( (*itp).first == overlay_selection_scale_ || (*itp).first == overlay_selection_rotate_ ) {
                        pick = (*itp);
                        // initiate selection manipulation
                        if (overlay_selection_stored_status_) {
                            overlay_selection_stored_status_->copyTransform(overlay_selection_);
                            overlay_selection_active_ = true;
                        }
                        break;
                    }
                    else if ( overlay_selection_icon_ != nullptr && (*itp).first == overlay_selection_icon_ ) {
                        pick = (*itp);
                        openContextMenu(MENU_SELECTION);
                        break;
                    }
                }
            }

        }

    }
    // nothing picked
    else {
        // cancel current canvas source if clicked outside
        setCurrentCanvasSource(nullptr);
    }

    return pick;
}


void DisplaysView::setCurrentCanvasSource(Source *c)
{
    // reset previous current canvas source
    if (current_canvas_source_ != nullptr) {

        // current source is part of the selection, keep inside and change status
        if (selected_canvas_sources_.size() > 1) 
            current_canvas_source_->setMode(Source::SELECTED);
        // current source is the only selected source, unselect fully
        else
        {
            // remove from selection
            selected_canvas_sources_.clear();
            current_canvas_source_->setMode(Source::VISIBLE);
        }

        // no more current
        current_canvas_source_ = nullptr;
    }

    // no new current canvas source requested
    if (c == nullptr) 
        return;

    // SET CURRENT CANVAS SOURCE
    current_canvas_source_ = static_cast<CanvasSource *>(c);

    // set selection for this only source if not already part of a selection
    if (!selected_canvas_sources_.contains(c)) 
        selected_canvas_sources_.set(c);

    if (selected_canvas_sources_.size() > 1) 
        current_canvas_source_->setMode(Source::SELECTED);
    else 
        current_canvas_source_->setMode(Source::CURRENT);
}

Source *DisplaysView::currentCanvasSource() const { 
    return static_cast<Source *>(current_canvas_source_); 
}

bool DisplaysView::canSelect(Source *) {

    return true;
}

void DisplaysView::select(glm::vec2 A, glm::vec2 B)
{
    // unproject mouse coordinate into scene coordinates
    glm::vec3 scene_point_A = Rendering::manager().unProject(A);
    glm::vec3 scene_point_B = Rendering::manager().unProject(B);

    // picking visitor traverses the scene
    PickingVisitor pv(scene_point_A, scene_point_B, true);
    scene.accept(pv);

    // reset selection
    selected_canvas_sources_.clear();
    setCurrentCanvasSource( nullptr);

    // picking visitor found nodes in the area?
    if ( !pv.empty()) {

        // create a list of source matching the list of picked nodes
        SourceList selection;
        // loop over the nodes and add all sources found.
         for(std::vector< std::pair<Node *, glm::vec2> >::const_reverse_iterator p = pv.rbegin(); p != pv.rend(); ++p){
            SourceList::iterator sit = Canvas::manager().session()->find( p->first );

            if ( sit != Canvas::manager().session()->end() )
                selection.push_back( *sit );
        }
        selection.unique();

        if (selection.size() < 2 ) {
            // if only one source selected, set it as current
            setCurrentCanvasSource( *selection.begin() );
        }
        else {
            // set the selection with list of picked (overlaped) sources
            selected_canvas_sources_.set(selection);
        }
    }

}

void DisplaysView::initiate()
{
    // initiate pending action
    if (!current_action_ongoing_ ) {

        // all sources store their status at initiation of an action
        for (auto sit = Canvas::manager().session()->begin();
             sit != Canvas::manager().session()->end(); ++sit)
            (*sit)->store(View::GEOMETRY);

        // initiated
        current_action_ = "";
        current_action_ongoing_ = true;
    }
}

void DisplaysView::terminate(bool force)
{
    // terminate pending action
    if (current_action_ongoing_ || force) {
        // terminated
        current_action_ = "";
        current_action_ongoing_ = false;
    }

    // hide all view overlays
    overlay_position_->visible_       = false;
    overlay_position_cross_->visible_ = false;
    overlay_rotation_clock_->visible_ = false;
    overlay_rotation_clock_hand_->visible_ = false;
    overlay_rotation_fix_->visible_   = false;
    overlay_rotation_->visible_       = false;
    overlay_scaling_grid_->visible_   = false;
    overlay_scaling_cross_->visible_  = false;
    overlay_scaling_->visible_        = false;
    overlay_crop_->visible_           = false;

    // restore possible color change after selection operation
    overlay_rotation_->color = glm::vec4(1.f, 1.f, 1.f, 0.8f);
    overlay_rotation_fix_->color = glm::vec4(1.f, 1.f, 1.f, 0.8f);
    overlay_rotation_clock_tic_->color = glm::vec4(1.f, 1.f, 1.f, 0.8f);
    overlay_scaling_->color = glm::vec4(1.f, 1.f, 1.f, 0.8f);
    overlay_scaling_cross_->color =  glm::vec4(1.f, 1.f, 1.f, 0.8f);

    // restore of all handles overlays
    glm::vec2 c(0.f, 0.f);
    for (auto sit = Canvas::manager().session()->begin();
         sit != Canvas::manager().session()->end(); ++sit){
        (*sit)->handles_[View::GEOMETRY][Handles::RESIZE]->overlayActiveCorner(c);
        (*sit)->handles_[View::GEOMETRY][Handles::RESIZE_H]->overlayActiveCorner(c);
        (*sit)->handles_[View::GEOMETRY][Handles::RESIZE_V]->overlayActiveCorner(c);
        for (auto h = 0; h < 15; ++h)
            (*sit)->handles_[View::GEOMETRY][h]->visible_ = true;
    }

    overlay_selection_active_ = false;

    // reset grid
    adaptGridToSource();
}


View::Cursor DisplaysView::grab (Source *, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2> pick)
{
    View::Cursor ret = Cursor();

    // grab coordinates in scene-View reference frame
    glm::vec3 scene_from = Rendering::manager().unProject(from, scene.root()->transform_);
    glm::vec3 scene_to   = Rendering::manager().unProject(to, scene.root()->transform_);

    // if grabing the selection overlay handles
    if (overlay_selection_ && overlay_selection_active_ ) {

        // rotation center to selection position
        glm::mat4 T = glm::translate(glm::identity<glm::mat4>(), overlay_selection_stored_status_->translation_);
        glm::vec4 selection_from = glm::inverse(T) * glm::vec4( scene_from,  1.f );
        glm::vec4 selection_to   = glm::inverse(T) * glm::vec4( scene_to,  1.f );

        // calculate scaling of selection
        float factor = glm::length( glm::vec2( selection_to ) ) / glm::length( glm::vec2( selection_from ) );
        glm::mat4 S = glm::scale(glm::identity<glm::mat4>(), glm::vec3(factor, factor, 1.f));

        // if interaction with selection SCALING handle
        if (pick.first == overlay_selection_scale_) {

            // show manipulation overlay
            overlay_scaling_cross_->visible_ = true;
            overlay_scaling_grid_->visible_ = false;
            overlay_scaling_->visible_ = true;
            overlay_scaling_->translation_.x = overlay_selection_stored_status_->translation_.x;
            overlay_scaling_->translation_.y = overlay_selection_stored_status_->translation_.y;
            overlay_scaling_->rotation_.z = overlay_selection_stored_status_->rotation_.z;
            overlay_scaling_->update(0);
            overlay_scaling_cross_->copyTransform(overlay_scaling_);
            overlay_scaling_->color = overlay_selection_icon_->color;
            overlay_scaling_cross_->color = overlay_selection_icon_->color;

            //
            // Manipulate the scaling handle in the SCENE coordinates to apply grid snap
            //
            if ( grid->active() ) {
                glm::vec3 handle = glm::vec3(1.f, -1.f, 0.f);
                // Compute handle coordinates into SCENE reference frame
                handle = overlay_selection_stored_status_->transform_ * glm::vec4( handle, 1.f );
                // move the handle we hold by the mouse translation (in scene reference frame)
                handle = glm::translate(glm::identity<glm::mat4>(), scene_to - scene_from) * glm::vec4( handle, 1.f );
                // snap handle coordinates to grid (if active)
                handle = grid->snap(handle);
                // Compute handle coordinates back in SOURCE reference frame
                handle = glm::inverse(overlay_selection_stored_status_->transform_) * glm::vec4( handle,  1.f );
                // The scaling factor is computed by dividing new handle coordinates with the ones before transform
                glm::vec3 handle_scaling = glm::vec3(handle) / glm::vec3(1.f, -1.f, 1.f);
                S = glm::scale(glm::identity<glm::mat4>(), handle_scaling);
            }

            // apply to selection overlay
            glm::vec4 vec = S * glm::vec4( overlay_selection_stored_status_->scale_, 0.f );
            overlay_selection_->scale_ = glm::vec3(vec);

            // apply to selection sources
            // NB: complete transform matrix (right to left) : move to center, scale and move back
            glm::mat4 M = T * S * glm::inverse(T);
            applySelectionTransform(M);

            // store action in history
            current_action_ = "Scale selection";
            ret.type = Cursor_ResizeNWSE;
        }
        // ...or if interaction with selection ROTATION handle
        else if (pick.first == overlay_selection_rotate_) {

            // show manipulation overlay
            overlay_rotation_->visible_ = true;
            overlay_rotation_->translation_.x = overlay_selection_stored_status_->translation_.x;
            overlay_rotation_->translation_.y = overlay_selection_stored_status_->translation_.y;
            overlay_rotation_->update(0);
            overlay_rotation_->color = overlay_selection_icon_->color;
            overlay_rotation_fix_->visible_ = false;
            overlay_rotation_fix_->copyTransform(overlay_rotation_);
            overlay_rotation_fix_->color = overlay_selection_icon_->color;

            // Swap grid to rotation, shifted at center of source
            rotation_grid_->setActive( grid->active() );
            translation_grid_->setActive( false );
            grid = rotation_grid_;
            grid->root()->translation_ = overlay_selection_stored_status_->translation_;

            // prepare variables
            const float diagonal = glm::length( glm::vec2(overlay_selection_stored_status_->scale_));
            glm::vec2 handle_polar = glm::vec2( diagonal, 0.f);

            // compute rotation angle
            float angle = glm::orientedAngle( glm::normalize(glm::vec2(selection_from)), glm::normalize(glm::vec2(selection_to)));
            handle_polar.y = overlay_selection_stored_status_->rotation_.z + angle;

            // compute scaling of diagonal to reach new coordinates
            handle_polar.x *= factor;

            // snap polar coordiantes (diagonal lenght, angle)
            if ( grid->active() ) {
                handle_polar = glm::round( handle_polar / grid->step() ) * grid->step();
                // prevent null size
                handle_polar.x = glm::max( grid->step().x,  handle_polar.x );
            }

            // cancel scaling with SHIFT modifier key
            if (UserInterface::manager().shiftModifier()) {
                overlay_rotation_fix_->visible_ = true;
                handle_polar.x = 1.f;
            }
            else
                handle_polar.x /= diagonal ;

            S = glm::scale(glm::identity<glm::mat4>(), glm::vec3(handle_polar.x, handle_polar.x, 1.f));

            // apply to selection overlay
            glm::vec4 vec = S * glm::vec4( overlay_selection_stored_status_->scale_, 0.f );
            overlay_selection_->scale_ = glm::vec3(vec);
            overlay_selection_->rotation_.z = handle_polar.y;

            // apply to selection sources
            // NB: complete transform matrix (right to left) : move to center, rotate, scale and move back
            angle = handle_polar.y - overlay_selection_stored_status_->rotation_.z;
            glm::mat4 R = glm::rotate(glm::identity<glm::mat4>(), angle, glm::vec3(0.f, 0.f, 1.f) );
            glm::mat4 M = T * S * R * glm::inverse(T);
            applySelectionTransform(M);

            // store action in history
            current_action_ = "Scale and rotate selection";
            ret.type = Cursor_Hand;
        }

        return ret;
    }

    // necessary current canvas source to perform manipulation
    if ( current_canvas_source_ == nullptr )
        return ret;

    /* canvas source grab */    
    Group *sourceNode = current_canvas_source_->group(View::GEOMETRY); 

    // make sure matrix transform of stored status is updated
    current_canvas_source_->stored_status_->update(0);

    // grab coordinates in source-root reference frame
    const glm::mat4 source_scale = glm::scale(glm::identity<glm::mat4>(),
                                              glm::vec3(1.f / current_canvas_source_->frame()->aspectRatio(), 1.f, 1.f));
    const glm::mat4 scene_to_source_transform = source_scale * glm::inverse(current_canvas_source_->stored_status_->transform_);
    const glm::mat4 source_to_scene_transform = glm::inverse(scene_to_source_transform);

    // which manipulation to perform?
    std::ostringstream info;
    if (pick.first)  {

        // which corner was picked ?
        glm::vec2 corner = glm::round(pick.second);

        // keep transform from source center to opposite corner
        const glm::mat4 source_to_corner_transform = glm::translate(glm::identity<glm::mat4>(), glm::vec3(corner, 0.f));

        // transformation from scene to corner:
        const glm::mat4 scene_to_corner_transform = source_to_corner_transform * scene_to_source_transform;
        const glm::mat4 corner_to_scene_transform = glm::inverse(scene_to_corner_transform);

        // Create context for handle manipulation functions
        GeometryHandleManipulation::HandleGrabContext ctx = {
            sourceNode,                     // targetNode
            current_canvas_source_->stored_status_,              // stored_status
            current_canvas_source_->frame(),                     // frame
            scene_from,                     // scene_from
            scene_to,                       // scene_to
            scene_to_source_transform,      // scene_to_target_transform
            source_to_scene_transform,      // target_to_scene_transform
            scene_to_corner_transform,      // scene_to_corner_transform
            corner_to_scene_transform,      // corner_to_scene_transform
            corner,                         // corner
            pick,                           // pick
            grid,                           // grid
            info,                           // info
            ret,                            // cursor
            current_canvas_source_->handles_[View::GEOMETRY],             // handles
            overlay_crop_,                  // overlay_crop
            overlay_scaling_,               // overlay_scaling
            overlay_scaling_cross_,         // overlay_scaling_cross
            overlay_rotation_,              // overlay_rotation
            overlay_rotation_fix_,          // overlay_rotation_fix
            overlay_rotation_clock_hand_    // overlay_rotation_clock_hand
        };

        // picking on a Node
        if (pick.first == current_canvas_source_->handles_[View::GEOMETRY][Handles::NODE_LOWER_LEFT]) {
            GeometryHandleManipulation::handleNodeLowerLeft(ctx);
        }
        else if (pick.first == current_canvas_source_->handles_[View::GEOMETRY][Handles::NODE_UPPER_LEFT]) {
            GeometryHandleManipulation::handleNodeUpperLeft(ctx);
        }
        else if ( pick.first == current_canvas_source_->handles_[View::GEOMETRY][Handles::NODE_LOWER_RIGHT] ) {
            GeometryHandleManipulation::handleNodeLowerRight(ctx);
        }
        else if (pick.first == current_canvas_source_->handles_[View::GEOMETRY][Handles::NODE_UPPER_RIGHT]) {
            GeometryHandleManipulation::handleNodeUpperRight(ctx);
        }
        // horizontal CROP
        else if (pick.first == current_canvas_source_->handles_[View::GEOMETRY][Handles::CROP_H]) {
            GeometryHandleManipulation::handleCropH(ctx);
        }
        // Vertical CROP
        else if (pick.first == current_canvas_source_->handles_[View::GEOMETRY][Handles::CROP_V]) {
            GeometryHandleManipulation::handleCropV(ctx);
        }
        // pick the corner rounding handle
        else if (pick.first == current_canvas_source_->handles_[View::GEOMETRY][Handles::ROUNDING]) {
            GeometryHandleManipulation::handleRounding(ctx);
        }
        // picking on the resizing handles in the corners RESIZE CORNER
        else if ( pick.first == current_canvas_source_->handles_[View::GEOMETRY][Handles::RESIZE] ) {
            GeometryHandleManipulation::handleResize(ctx);
        }
        // picking on the BORDER RESIZING handles left or right
        else if ( pick.first == current_canvas_source_->handles_[View::GEOMETRY][Handles::RESIZE_H] ) {
            GeometryHandleManipulation::handleResizeH(ctx);
        }
        // picking on the BORDER RESIZING handles top or bottom
        else if ( pick.first == current_canvas_source_->handles_[View::GEOMETRY][Handles::RESIZE_V] ) {
            GeometryHandleManipulation::handleResizeV(ctx);
        }
        // picking on the CENTRER SCALING handle
        else if ( pick.first == current_canvas_source_->handles_[View::GEOMETRY][Handles::SCALE] ) {
            GeometryHandleManipulation::handleScale(ctx);
        }
        // picking on the rotating handle
        else if ( pick.first == current_canvas_source_->handles_[View::GEOMETRY][Handles::ROTATE] ) {
            GeometryHandleManipulation::handleRotate(ctx);
        }
        // picking anywhere but on a handle: user wants to move the source
        else {

            // Default is to grab the center (0,0) of the source
            glm::vec3 handle(0.f);
            glm::vec3 offset(0.f);

            // Snap corner with SHIFT
            if (UserInterface::manager().shiftModifier()) {
                // get corner closest representative of the quadrant of the picking point
                handle = glm::vec3( glm::sign(pick.second), 0.f);
                // remember the offset for adjustment of translation to this corner
                offset = source_to_scene_transform * glm::vec4(handle, 0.f);
            }

            // Compute target coordinates of manipulated handle into SCENE reference frame
            glm::vec3 source_target = source_to_scene_transform * glm::vec4(handle, 1.f);

            // apply translation of target in SCENE
            source_target = glm::translate(glm::identity<glm::mat4>(), scene_to - scene_from) * glm::vec4(source_target, 1.f);

            // snap coordinates to grid (if active)
            if ( grid->active() )
                // snap coordinate in scene
                source_target = grid->snap(source_target);

            // Apply translation to the source
            sourceNode->translation_ = source_target - offset;

            //
            // grab all others in selection
            //
            // compute effective translation of current source s
            source_target = sourceNode->translation_ - current_canvas_source_->stored_status_->translation_;

            // loop over selection
            for (auto it = selected_canvas_sources_.begin(); it != selected_canvas_sources_.end(); ++it) {
                if ( *it != current_canvas_source_ ) {
                    // translate and request update
                    (*it)->group(View::GEOMETRY)->translation_ = (*it)->stored_status_->translation_ + source_target;
                    (*it)->touch();
                }
            }

            // Show center overlay for POSITION
            overlay_position_->visible_ = true;
            overlay_position_->translation_.x = sourceNode->translation_.x;
            overlay_position_->translation_.y = sourceNode->translation_.y;
            overlay_position_->update(0);

            // Show move cursor
            ret.type = Cursor_ResizeAll;
            info << "Position " << std::fixed << std::setprecision(3) << sourceNode->translation_.x;
            info << ", "  << sourceNode->translation_.y ;
        }
    }

    // request update
    current_canvas_source_->touch();

    // store action in history
    current_action_ = current_canvas_source_->name() + ": " + info.str();

    // update cursor
    ret.info = info.str();

    return ret;
}


#define MAX_DURATION 1000.f
#define MIN_SPEED_A 0.005f
#define MAX_SPEED_A 0.5f

void DisplaysView::arrow (glm::vec2 movement)
{
static float _duration = 0.f;
    static glm::vec2 _from(0.f);
    static glm::vec2 _displacement(0.f);

    Source *current = current_canvas_source_;

    if (current) {

        if (current_action_ongoing_) {

            // add movement to displacement
            _duration += dt_;
            const float speed = MIN_SPEED_A + (MAX_SPEED_A - MIN_SPEED_A) * glm::min(1.f,_duration / MAX_DURATION);
            _displacement += movement * dt_ * speed;

            // set coordinates of target
            glm::vec2 _to  = _from + _displacement;

            // update mouse pointer action
            MousePointer::manager().active()->update(_to, dt_ / 1000.f);

            // simulate mouse grab
            grab(current, _from, MousePointer::manager().active()->target(),
                 std::make_pair(current->group(View::GEOMETRY), glm::vec2(0.f) ) );

            // draw mouse pointer effect
            MousePointer::manager().active()->draw();
        }
        else {

            if (UserInterface::manager().altModifier() || Settings::application.mouse_pointer_lock)
                MousePointer::manager().setActiveMode( (Pointer::Mode) Settings::application.mouse_pointer );
            else
                MousePointer::manager().setActiveMode( Pointer::POINTER_DEFAULT );

            // reset
            _duration = 0.f;
            _displacement = glm::vec2(0.f);

            // initiate view action and store status of source
            initiate();

            // get coordinates of source and set this as start of mouse position
            _from = glm::vec2( Rendering::manager().project(current->group(View::GEOMETRY)->translation_, scene.root()->transform_) );

            // Initiate mouse pointer action
            MousePointer::manager().active()->initiate(_from);
        }
    }
    else {
        terminate(true);

        // reset
        _duration = 0.f;
        _from = glm::vec2(0.f);
        _displacement = glm::vec2(0.f);
    }


}


void DisplaysView::updateSelectionOverlay(glm::vec4 color)
{
    // create first
    if (overlay_selection_ == nullptr) {
        overlay_selection_ = new Group;
        overlay_selection_icon_ = new Handles(Handles::MENU);
        overlay_selection_->attach(overlay_selection_icon_);
        overlay_selection_frame_ = new Frame(Frame::SHARP, Frame::LARGE, Frame::NONE);
        overlay_selection_->attach(overlay_selection_frame_);
        scene.fg()->attach(overlay_selection_);

        overlay_selection_stored_status_ = new Group;
        overlay_selection_scale_ = new Handles(Handles::SCALE);
        overlay_selection_->attach(overlay_selection_scale_);
        overlay_selection_rotate_ = new Handles(Handles::ROTATE);
        overlay_selection_->attach(overlay_selection_rotate_);
    }

    // selection active if more than 1 source selected
    if (selected_canvas_sources_.size() > 1) {
        // show group overlay
        overlay_selection_->visible_ = true;
        overlay_selection_frame_->color = color;
        overlay_selection_frame_->color.a *= 0.8;
        overlay_selection_icon_->color = color;
    }
    // no selection: reset drawing selection overlay
    else {
        // no overlay by default
        overlay_selection_->visible_ = false;
        overlay_selection_->scale_ = glm::vec3(0.f, 0.f, 1.f);
    }

    if (overlay_selection_->visible_) {

        if ( !overlay_selection_active_) {

            // calculate ORIENTED bbox on selection
            GlmToolkit::OrientedBoundingBox selection_box = BoundingBoxVisitor::OBB(selected_canvas_sources_.getCopy(), 
                                                        scene.ws(), View::GEOMETRY);

            // apply transform
            overlay_selection_->rotation_ = selection_box.orientation;
            overlay_selection_->scale_ = selection_box.aabb.scale();
            glm::mat4 rot = glm::rotate(glm::identity<glm::mat4>(), selection_box.orientation.z, glm::vec3(0.f, 0.f, 1.f) );
            glm::vec4 vec = rot * glm::vec4(selection_box.aabb.center(), 1.f);
            overlay_selection_->translation_ = glm::vec3(vec);
        }

        // cosmetics
        overlay_selection_scale_->color = overlay_selection_icon_->color;
        overlay_selection_rotate_->color = overlay_selection_icon_->color;
    }

}

void DisplaysView::applySelectionTransform(glm::mat4 M)
{
    for (auto sit = selected_canvas_sources_.begin(); sit != selected_canvas_sources_.end(); ++sit){
        // recompute all from matrix transform
        glm::mat4 transform = M * (*sit)->stored_status_->transform_;
        glm::vec3 tra, rot, sca;
        GlmToolkit::inverse_transform(transform, tra, rot, sca);
        (*sit)->group(View::GEOMETRY)->translation_ = tra;
        (*sit)->group(View::GEOMETRY)->scale_ = sca;
        (*sit)->group(View::GEOMETRY)->rotation_ = rot;
        // will have to be updated
        (*sit)->touch();
    }
}
