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
#include "View/View.h"
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

#include "Toolkit/ImGuiToolkit.h"
#include "imgui_internal.h"

#include "Log.h"
#include "Filter/ImageFilter.h"
#include "Mixer.h"
#include "defines.h"
#include "Source/Source.h"
#include "Settings.h"
#include "Visitor/PickingVisitor.h"
#include "Visitor/DrawVisitor.h"
#include "Scene/Decorations.h"
#include "UserInterfaceManager.h"
#include "Visitor/BoundingBoxVisitor.h"
#include "RenderingManager.h"
#include "GeometryHandleManipulation.h"
#include "Canvas.h"

#include "DisplaysView.h"

#define WINDOW_TITLEBAR_HEIGHT 0.03f

FilteringProgram _whitebalance("Whitebalance", "shaders/filters/whitebalance.glsl", "", { { "Red", 1.0}, { "Green", 1.0}, { "Blue", 1.0}, { "Temperature", 0.5} });



DisplaysView::DisplaysView() : View(DISPLAYS)
{
    scene.root()->scale_ = glm::vec3(DISPLAYS_DEFAULT_SCALE, DISPLAYS_DEFAULT_SCALE, 1.0f);

    // read default settings
    if ( Settings::application.views[mode_].name.empty() ) {
        // no settings found: store application default
        saveSettings();
    }
    else
        restoreSettings();
    Settings::application.views[mode_].name = "Displays";


    boundingbox_surface_ = new Surface;
    boundingbox_surface_->visible_ = true;
    boundingbox_surface_->shader()->color = glm::vec4(1.f, 1.f, 1.f, 0.4f);
    // TODO : is the output surface needed in displays view ? 
    scene.bg()->attach(boundingbox_surface_);

    // monitors layout background
    monitors_layout_ = new Group;
    monitors_layout_->visible_ = true;
    scene.bg()->attach(monitors_layout_);
    horizontal_layout_ = true;

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

    // grid is attached to a transform group
    // to adapt to windows geometry; see adaptGridToWindow()
    gridroot_ = new Group;
    gridroot_->visible_ = false;
    scene.root()->attach(gridroot_);

    // replace grid with appropriate one
    if (grid)  delete grid;
    grid = new TranslationGrid(gridroot_);
    grid->root()->visible_ = false;

    // manage canvas sources
    current_canvas_source_ = nullptr;

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
                grid->setAspectRatio( output->aspectRatio() );
            else
                grid->setAspectRatio( 1.f );
        }


        // change grid color
        ImVec4 c = ImGuiToolkit::HighlightColor();
        grid->setColor( glm::vec4(c.x, c.y, c.z, 0.3) );
    }

    // specific update when this view is active
    if ( Mixer::manager().view() == this ) {

        // TODO update selection overlay
        
    }
}

void setCoordinates(Node *node, glm::ivec4 coordinates, glm::ivec4 bounding_box)
{
    glm::vec4 rect = DISPLAYS_UNIT * glm::vec4(coordinates);
    
    // screen coordinates to scene coordinates
    node->scale_ = glm::vec3( 0.5f * rect.z, 0.5f * rect.w, 1.f );
    node->translation_ = glm::vec3( rect.x + 0.5f * rect.z, - (rect.y + 0.5f * rect.w), 0.f );

    // centered on bounding box
    node->translation_.x -= 0.5f * DISPLAYS_UNIT * (bounding_box.x + bounding_box.z);
    node->translation_.y += 0.5f * DISPLAYS_UNIT * (bounding_box.y + bounding_box.w);

    // scaling to fit in scene
    // horizontal layout : scale to height (w component of bounding box) 
    // because projection in OutputWindow is based on height 1.0
    float scalingratio = 1.f / (0.5f * DISPLAYS_UNIT * float(bounding_box.w) );
    node->scale_ *= glm::vec3( scalingratio, scalingratio, 1.f ); 
    node->translation_ *= glm::vec3( scalingratio, scalingratio, 1.f ); 
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
    setCoordinates(boundingbox_surface_, bbox, bbox);

    // fill scene background with the frames to show monitors
    // Reset and fill monitors_layout_ and list of monitors_
    int index = 1;
    monitors_layout_->clear();
    monitors_.clear();
    for (auto& monitor : Rendering::manager().monitorsUnsafe()) {

        // add a background dark surface with glow shadow
        Group *m = new Group;
        setCoordinates(m, monitor.geometry, bbox);
        Surface *surf = new Surface( new Shader);
        surf->shader()->color =  glm::vec4( 0.1f, 0.1f, 0.1f, 1.f );
        m->attach(surf);
        // Monitor color frame
        Frame *frame = new Frame(Frame::SHARP, Frame::THIN, Frame::GLOW);
        frame->color = glm::vec4( COLOR_MONITOR, 1.f);
        m->attach(frame);
        // central label
        Character  *label = new Character(4);
        label->setChar( std::to_string(index).back() );
        label->color = glm::vec4( COLOR_MONITOR, 1.f );
        label->translation_.y =  0.015f ;
        label->scale_.y =  0.3f / (DISPLAYS_UNIT * monitor.geometry.w);
        m->attach(label);

        // add monitor to layout
        monitors_layout_->attach( m );
        monitors_[monitor.name] = m;

        // add a color frame 
        // Group *f = new Group;
        // f->copyTransform(m);
        // frame = new Frame(Frame::SHARP, Frame::THIN, Frame::NONE);
        // frame->color = glm::vec4( COLOR_MONITOR, 0.2f );
        // f->attach(frame);
        // monitors_layout_->attach(f);

        ++index;
    }

    // Adjust scene scaling to fit all monitors in view
    // take top-left corner in scene coordinates as reference for extents of view
    glm::vec3 top_left = Rendering::manager().unProject(glm::vec3(0.f, 0.f, 0.f));
    glm::vec3 view = glm::abs(top_left);
    // apply scene scaling depending on layout
    if (boundingbox_surface_->scale_.x >= boundingbox_surface_->scale_.y) {
        horizontal_layout_ = true;
        scene.root()->scale_.x = view.x / (DISPLAYS_DEFAULT_SCALE * boundingbox_surface_->scale_.x) ;
        scene.root()->scale_.y = scene.root()->scale_.x;
    }
    else {
        horizontal_layout_ = false;
        scene.root()->scale_.y = view.y / (DISPLAYS_DEFAULT_SCALE * boundingbox_surface_->scale_.y) ;
        scene.root()->scale_.x = scene.root()->scale_.y;
    }
    // reset scene translation
    scene.root()->translation_ = glm::vec3(0.f);

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


void DisplaysView::adaptGridToWindow(int w)
{
    // reset by default
    gridroot_->scale_ = glm::vec3(1.f);
    gridroot_->translation_ = glm::vec3(0.f);
    grid->setAspectRatio(1.f);


    // set grid aspect ratio to display size otherwise
   {

        if (Settings::application.proportional_grid) {
            // std::string m = Rendering::manager()
            //                     .monitorNameAt(Settings::application.windows[current_window_ + 1].x,
            //                                    Settings::application.windows[current_window_ + 1].y);
            // glm::ivec4 rect = Rendering::manager().monitors()[m];

            // gridroot_->translation_.x = rect.x * DISPLAYS_UNIT;
            // gridroot_->translation_.y = rect.y * -DISPLAYS_UNIT;
            // gridroot_->scale_.x = rect.p * 0.5f * DISPLAYS_UNIT;
            // gridroot_->scale_.y = rect.q * 0.5f * DISPLAYS_UNIT;
        }
    }
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

void DisplaysView::draw()
{
    // render CanvasSources of Canvas::manager
    std::vector<Node *> source_surfaces;
    std::vector<Node *> source_overlays;

    for (auto source_iter = Canvas::manager().session()->begin();
            source_iter != Canvas::manager().session()->end(); ++source_iter) {
                
        // will draw its surface
        source_surfaces.push_back((*source_iter)->groups_[GEOMETRY]);
        // will draw its frame 
        source_overlays.push_back((*source_iter)->frames_[GEOMETRY]);
    }

    // 0. prepare projection for draw visitors
    glm::mat4 projection = Rendering::manager().Projection();

    // Draw background display of monitors
    DrawVisitor draw_monitors(monitors_layout_, projection);
    scene.accept(draw_monitors);

    DrawVisitor draw_rendering(boundingbox_surface_, projection);
    scene.accept(draw_rendering);

    // Draw surface of Canvas sources 
    DrawVisitor draw_sources(source_surfaces, projection, true); 
    scene.accept(draw_sources);

    // Draw frames and icons of Canvas sources 
    DrawVisitor draw_overlays(source_overlays, projection);
    scene.accept(draw_overlays);

    // Finally, draw overlays of view
    DrawVisitor draw_foreground(scene.fg(), projection);
    scene.accept(draw_foreground);

    // TODO GRID
    // // Display grid in overlay
    // if (grid->active() && current_action_ongoing_) {
    //     const glm::mat4 projection = Rendering::manager().Projection();
    //     DrawVisitor draw_grid(grid->root(), projection, true);
    //     scene.accept(draw_grid);
    // }
        
    //
    // 10. Display interface
    //
    {
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ImGuiContext& g = *GImGui;

        // protected access to monitors list
        std::lock_guard<std::mutex> lock(Rendering::manager().getMonitorsMutex());
        for (auto& monitor : Rendering::manager().monitorsUnsafe()) {

            // Locate monitor view upper left corner
            glm::vec3 pos;
            auto it = monitors_.find(monitor.name);
            if (it != monitors_.end()) {
                Group* group = it->second;
                pos = group->translation_;
                pos.x -= group->scale_.x;
                pos.y += group->scale_.y;
            }
            glm::vec2 P = Rendering::manager().project(pos, scene.root()->transform_,
                                Settings::application.mainwindow.fullscreen);

            // Set window position depending on icons size
            if (horizontal_layout_)
                ImGui::SetNextWindowPos(ImVec2(P.x - 20.f, P.y - 2.f * ImGui::GetFrameHeight() ), ImGuiCond_Always);
            else
                ImGui::SetNextWindowPos(ImVec2(P.x - 2.f * ImGui::GetFrameHeight(), P.y + 10.f), ImGuiCond_Always);

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
                if ((monitor.output).isActive()) {
                    // deactivate output
                    if (ImGui::Button(ICON_FA_TOGGLE_ON )) 
                        (monitor.output).setActive(false);
                    if (ImGui::IsItemHovered())
                        ImGuiToolkit::ToolTip(std::string("Deactivate monitor ").append(monitor.name).c_str());
                }
                else {
                    // activate output
                    if (ImGui::Button(ICON_FA_TOGGLE_OFF )) 
                        (monitor.output).setActive(true);
                    if (ImGui::IsItemHovered())
                        ImGuiToolkit::ToolTip(std::string("Activate monitor ").append(monitor.name).c_str());

                    // skip rest of window
                    ImGui::PopStyleColor(8);
                    ImGui::End();
                    continue;
                }

                // TEST PATTERN BUTTON
                if (horizontal_layout_)
                    ImGui::SameLine(0, IMGUI_SAME_LINE);
                static bool show_test_pattern = false;  
                show_test_pattern = (monitor.output).isShowPattern();
                if ( ImGuiToolkit::ButtonIconToggle(11,1, &show_test_pattern, "Test pattern") )
                    (monitor.output).setShowPattern(show_test_pattern);

                // WHITE BALANCE BUTTON
                if (horizontal_layout_) {
                    ImGui::SameLine(0, IMGUI_SAME_LINE);
                    ImGui::SetCursorPosY(2.f * g.Style.FramePadding.y); // hack to align color button to text
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
                    ImGui::SetCursorPosY(g.Style.FramePadding.y); // hack to re-align sliders button back
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
                    (monitor.output).setShowPattern(false);
                }
                if (ImGui::IsItemHovered())
                    ImGuiToolkit::ToolTip("Reset adjustments");

                ImGui::PopStyleColor(8);
                ImGui::End();

            }

        }

        ImGui::PopFont();
    }


    // // display popup menu
    // if (show_window_menu_ ) {
    //     ImGui::OpenPopup( "DisplaysOutputContextMenu" );
    //     show_window_menu_ = false;
    // }
    // if (ImGui::BeginPopup("DisplaysOutputContextMenu")) {

    //     ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(COLOR_MENU_HOVERED, 0.5f));
    //     ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(COLOR_WINDOW, 1.f));


    //     ImGui::PopStyleColor(2);
    //     ImGui::EndPopup();
    // }

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

        // canvas source are not managed as sources; cancel normal source picking
        pick = { nullptr, glm::vec2(0.f) };

        // remember picked sources to cycle through them
        static SourceListUnique picked_sources;

        // keep current canvas active if it is clicked
        Source *picked_canvas_source = current_canvas_source_;
        if (picked_canvas_source != nullptr) {
            // find if the current canvas was picked
            auto itp = pv.rbegin();
            for (; itp != pv.rend(); ++itp){
                // test if canvas contains this node
                Source::hasNode is_in_source((*itp).first );
                if ( is_in_source( picked_canvas_source ) ){
                    // a node in the current canvas was clicked !
                    pick = *itp;
                    break;
                }
            }
            // not found: the current canvas was not clicked
            if (itp == pv.rend() ) {
                picked_sources.clear();
                picked_canvas_source = nullptr;
            }
            // picking on the menu handle: show context menu & reset picked sources
            else if ( pick.first == picked_canvas_source->handles_[View::GEOMETRY][Handles::MENU] ) {
                openContextMenu(MENU_CANVAS); 
                picked_sources.clear();
            }
            // picking on the crop handle : switch to shape manipulation mode
            else if (pick.first == picked_canvas_source->handles_[View::GEOMETRY][Handles::EDIT_CROP]) {
                picked_canvas_source->manipulator_->setActive(0);
                picked_sources.clear();
            }
            // picking on the shape handle : switch to crop manipulation mode
            else if (pick.first == picked_canvas_source->handles_[View::GEOMETRY][Handles::EDIT_SHAPE]) {
                picked_canvas_source->manipulator_->setActive(1);
                picked_sources.clear();
            }
            // TODO
            // 
            // Pick LOCK ?
            //
            // second clic not on a handle, maybe select another canvas source below
            else {
                if (picked_sources.empty()) {
                    // loop over all nodes picked to fill the list of canvas sources clicked
                    for (auto itp = pv.rbegin(); itp != pv.rend(); ++itp) {
                        SourceList::iterator sit = std::find_if(Canvas::manager().session()->begin(), 
                            Canvas::manager().session()->end(), Source::hasNode( (*itp).first ));
                        if ( sit != Canvas::manager().session()->end() ) {
                            picked_sources.insert( *sit );
                        }
                    }
                }
                if (!picked_sources.empty()){
                    setCurrentCanvasSource( *picked_sources.begin() );
                    picked_sources.erase(picked_sources.begin());
                }
            }
        }
        // the clicked canvas source might have changed
        if (picked_canvas_source == nullptr) {

            // default to no canvas source picked
            if(current_canvas_source_ != nullptr)
                setCurrentCanvasSource(nullptr);

            // loop over all nodes picked
            for (auto itp = pv.rbegin(); itp != pv.rend(); ++itp){

                // loop over all canvas sources to get if one was picked
                SourceList::iterator sit = std::find_if(Canvas::manager().session()->begin(), 
                    Canvas::manager().session()->end(), Source::hasNode( (*itp).first ));

                // accept picked canvas
                if ( sit != Canvas::manager().session()->end() ) {
                    setCurrentCanvasSource(*sit);
                    break;
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
    if (c == nullptr) {
        if (current_canvas_source_ != nullptr) {
            current_canvas_source_->setMode(Source::VISIBLE);
            current_canvas_source_ = nullptr;
        }
        return;
    }

    current_canvas_source_ = c;
    current_canvas_source_->setMode(Source::CURRENT);

}

bool DisplaysView::canSelect(Source *) {

    return false;
}

void DisplaysView::select(glm::vec2 A, glm::vec2 B)
{
    // unproject mouse coordinate into scene coordinates
    glm::vec3 scene_point_A = Rendering::manager().unProject(A);
    glm::vec3 scene_point_B = Rendering::manager().unProject(B);

    // picking visitor traverses the scene
    PickingVisitor pv(scene_point_A, scene_point_B, true);
    scene.accept(pv);

    // TODO Multiple window selection?

    if (!pv.empty()) {

        
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
}


View::Cursor DisplaysView::grab (Source *, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2> pick)
{
    View::Cursor ret = Cursor();

    // grab coordinates in scene-View reference frame
    glm::vec3 scene_from = Rendering::manager().unProject(from, scene.root()->transform_);
    glm::vec3 scene_to   = Rendering::manager().unProject(to, scene.root()->transform_);

    // TODO : grab selection
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
            for (auto it = Mixer::selection().begin(); it != Mixer::selection().end(); ++it) {
                if ( *it != current_canvas_source_ && !(*it)->locked() ) {
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

bool DisplaysView::doubleclic (glm::vec2 P)
{
    // // bring window forward
    // if ( pick(P).first != nullptr) {
    //     Rendering::manager().outputWindow(current_window_).show();
    //     return true;
    // }

    return false;
}

#define TIME_STEP 500

void DisplaysView::arrow (glm::vec2 movement)
{
    static uint _time = 0;
    static glm::vec2 _from(0.f);
    static glm::vec2 _displacement(0.f);

}



