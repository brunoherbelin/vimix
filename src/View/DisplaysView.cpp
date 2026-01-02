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
#include <cstddef>
#include <glm/fwd.hpp>
#include <string>
#include <sstream>


#include <glad/glad.h>
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
#include "MousePointer.h"

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

    // initial behavior: no window selected, no menu
    show_window_menu_ = false;
    current_window_status_ = new Group;
    current_output_status_ = new Group;
    draw_pending_ = false;
    output_ar = 1.f;

    // grid is attached to a transform group
    // to adapt to windows geometry; see adaptGridToWindow()
    gridroot_ = new Group;
    gridroot_->visible_ = false;
    scene.root()->attach(gridroot_);

    // replace grid with appropriate one
    if (grid)  delete grid;
    grid = new TranslationGrid(gridroot_);
    grid->root()->visible_ = false;

    recenter();
}

void DisplaysView::update(float dt)
{
    View::update(dt);

    // specific update when this view is active
    if ( Mixer::manager().view() == this ) {

        

        output_ar = Mixer::manager().session()->frame()->aspectRatio();
    }

    // a more complete update is requested
    if (View::need_deep_update_ > 0) {
        // change grid color
        ImVec4 c = ImGuiToolkit::HighlightColor();
        grid->setColor( glm::vec4(c.x, c.y, c.z, 0.3) );


    }
}


// recenter is called also when RenderingManager detects a change of monitors
void  DisplaysView::recenter ()
{
    // clear background display of monitors
    scene.clearBackground();
    scene.clearForeground();

    // reset scene transform
    scene.root()->translation_.x = 0.f;
    scene.root()->translation_.y = 0.f;
    scene.root()->scale_.x = 1.f;
    scene.root()->scale_.y = 1.f;

    // reset the list of monitor groups
    monitors_.clear();

    // fill scene background with the frames to show monitors
    int index = 1;
    for (auto monitor_iter = Rendering::manager().monitors().begin();
         monitor_iter != Rendering::manager().monitors().end(); ++monitor_iter, ++index) {

        // get coordinates of monitor in Display units
        glm::vec4 rect = DISPLAYS_UNIT * glm::vec4(monitor_iter->geometry);

        // add a background dark surface with glow shadow
        Group *m = new Group;
        m->scale_ = glm::vec3( 0.5f * rect.p, 0.5f * rect.q, 1.f );
        m->translation_ = glm::vec3( rect.x + m->scale_.x, -rect.y - m->scale_.y, 0.f );
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
        label->scale_.y =  0.3f / rect.p;
        m->attach(label);

        scene.bg()->attach( m );
        monitors_.push_back(m);

        // add a foreground color frame (semi transparent for overlay)
        Group *f = new Group;
        f->copyTransform(m);
        frame = new Frame(Frame::SHARP, Frame::THIN, Frame::NONE);
        frame->color = glm::vec4( COLOR_MONITOR, 0.2f );
        f->attach(frame);
        scene.fg()->attach(f);

//        g_printerr("- display %f,%f  %f,%f\n", rect.x, rect.y, rect.p, rect.q);
//        g_printerr("          %f,%f  %f,%f\n", m->translation_.x, m->translation_.y,
//                   m->scale_.x, m->scale_.y);
    }

    // calculate screen area required to see the entire scene
    BoundingBoxVisitor scene_visitor_bbox(true);
    scene.accept(scene_visitor_bbox);
    GlmToolkit::AxisAlignedBoundingBox scene_box = scene_visitor_bbox.bbox();

    // calculate the coordinates of top-left window corner: this indicates space available in view
    static glm::mat4 projection = glm::ortho(-SCENE_UNIT, SCENE_UNIT, -SCENE_UNIT, SCENE_UNIT, -SCENE_DEPTH, 1.f);
    float viewar = (float) Settings::application.mainwindow.w / (float) Settings::application.mainwindow.h;
    glm::mat4 scale = glm::scale(glm::identity<glm::mat4>(), glm::vec3(viewar > 1.f ? 1.f : 1.f / viewar, viewar > 1.f ? viewar : 1.f, 1.f));
    glm::vec4 viewport = glm::vec4(0.f, 0.f, Settings::application.mainwindow.w, Settings::application.mainwindow.h);
    glm::vec3 view = glm::unProject(glm::vec3(0.f),glm::identity<glm::mat4>(), projection * scale, viewport);
    view = glm::abs(view);

    // compute scaling to fit the scene box into the view
    if ( scene_box.scale().x > scene_box.scale().y ) {
        // horizontal arrangement
        scene.root()->scale_.x = 1.1f * glm::min(view.x, view.y) / ( scene_box.scale().x );
        scene.root()->scale_.y = scene.root()->scale_.x;
    }
    else {
        // vertical arrangement
        scene.root()->scale_.y = glm::min(view.x, view.y) / ( 1.1f * scene_box.scale().y );
        scene.root()->scale_.x = scene.root()->scale_.y;
    }

    // compute translation to place at the center (considering scaling, + shift for buttons left and above)
    scene.root()->translation_ = -scene.root()->scale_.x * (scene_box.center() + glm::vec3(-0.02f, 0.02f, 0.f));
}

void DisplaysView::resize ( int scale )
{
    float z = CLAMP(0.01f * (float) scale, 0.f, 1.f);
    z *= z; // square
    z *= DISPLAYS_MAX_SCALE - DISPLAYS_MIN_SCALE;
    z += DISPLAYS_MIN_SCALE;

    // centered zoom : if zooming, adjust translation to ratio of scaling
    if (scale != size())
        scene.root()->translation_ *= z / scene.root()->scale_.x;

    // apply scaling
    scene.root()->scale_.x = z;
    scene.root()->scale_.y = z;
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

void menuMonitor() {


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

    // main call to draw the view
    View::draw();

    // Display grid in overlay
    if (grid->active() && current_action_ongoing_) {
        const glm::mat4 projection = Rendering::manager().Projection();
        DrawVisitor draw_grid(grid->root(), projection, true);
        scene.accept(draw_grid);
    }
        
    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
    ImGuiContext& g = *GImGui;

    auto group_it = monitors_.begin();
    for (auto monitor_iter = Rendering::manager().monitors().begin();
         monitor_iter != Rendering::manager().monitors().end(); ++monitor_iter, ++group_it) {

        // Locate monitor upper left corner
        glm::vec3 pos = (*group_it)->translation_;
        pos.x -= (*group_it)->scale_.x;
        pos.y += (*group_it)->scale_.y;
        pos.z = 0.f;
        glm::vec2 P = Rendering::manager().project(pos, scene.root()->transform_,
                            Settings::application.mainwindow.fullscreen);

        // Set window position depending on icons size
        ImGui::SetNextWindowPos(ImVec2(P.x - 20.f, P.y - 2.f * ImGui::GetFrameHeight() ), ImGuiCond_Always);
        if (ImGui::Begin((*monitor_iter).name.c_str(), NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground
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
            ImGui::SameLine(0, IMGUI_SAME_LINE);
            if ((monitor_iter->output).isActive()) {
                // deactivate output
                if (ImGui::Button(ICON_FA_TOGGLE_ON )) 
                    (monitor_iter->output).setActive(false);
                if (ImGui::IsItemHovered())
                    ImGuiToolkit::ToolTip("Deactivate output");
            }
            else {
                // activate output
                if (ImGui::Button(ICON_FA_TOGGLE_OFF )) 
                    (monitor_iter->output).setActive(true);
                if (ImGui::IsItemHovered())
                    ImGuiToolkit::ToolTip("Activate output");

                // skip rest of window
                ImGui::PopStyleColor(8);
                ImGui::End();
                continue;
            }

            // TEST PATTERN BUTTON
            ImGui::SameLine(0, IMGUI_SAME_LINE);
            static bool show_test_pattern = false;  
            show_test_pattern = (monitor_iter->output).isShowPattern();
            if ( ImGuiToolkit::ButtonIconToggle(11,1, &show_test_pattern, "Test pattern") )
                (monitor_iter->output).setShowPattern(show_test_pattern);

            // WHITE BALANCE BUTTON
            ImGui::SameLine(0, IMGUI_SAME_LINE);
            ImGui::SetCursorPosY(2.f * g.Style.FramePadding.y); // hack to re-align color button to text
            
            ImGuiToolkit::PushFont(ImGuiToolkit::FONT_DEFAULT);
            glm::vec4 color = Settings::application.monitors[monitor_iter->name].whitebalance;
            if (ImGui::ColorButton("White balance", ImVec4(color.x, color.y, color.z, 1.f),
                                   ImGuiColorEditFlags_NoAlpha)) {
                if ( !DialogToolkit::ColorPickerDialog::instance().busy()) {
                    // prepare the color picker to start with white balance color
                    DialogToolkit::ColorPickerDialog::instance().setRGB( std::make_tuple(color.x, color.y, color.z) );
                    // declare function to be called
                    std::string m = monitor_iter->name;
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
            ImGui::SameLine(0, IMGUI_SAME_LINE);
            ImGui::SetCursorPosY(g.Style.FramePadding.y); // hack to re-align sliders to text

            if (ImGui::Button(ICON_FA_SLIDERS_H ICON_FA_SORT_DOWN )) {
                ImGui::OpenPopup("adjustments_popup");
            }
            if (ImGui::BeginPopup("adjustments_popup", ImGuiWindowFlags_NoMove))  {
                popup_adjustment_color(Settings::application.monitors[monitor_iter->name]);
                ImGui::EndPopup();
            }

            // RESET BUTTON
            ImGui::SameLine(0, IMGUI_SAME_LINE);
            if (ImGui::Button(ICON_FA_BACKSPACE )) {
                Settings::application.monitors[monitor_iter->name] = Settings::MonitorConfig();
                (monitor_iter->output).setShowPattern(false);
            }
            if (ImGui::IsItemHovered())
                ImGuiToolkit::ToolTip("Reset adjustments");

            ImGui::PopStyleColor(8);
            ImGui::End();
        }

    }
    ImGui::PopFont();


    // display popup menu
    if (show_window_menu_ ) {
        ImGui::OpenPopup( "DisplaysOutputContextMenu" );
        show_window_menu_ = false;
    }
    if (ImGui::BeginPopup("DisplaysOutputContextMenu")) {

        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(COLOR_MENU_HOVERED, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(COLOR_WINDOW, 1.f));


        ImGui::PopStyleColor(2);
        ImGui::EndPopup();
    }

    draw_pending_ = false;
}

std::pair<Node *, glm::vec2> DisplaysView::pick(glm::vec2 P)
{
    // prepare empty return value
    std::pair<Node *, glm::vec2> pick = { nullptr, glm::vec2(0.f) };

    // get picking from generic View
    pick = View::pick(P);

    return pick;
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

        // prevent next draw
        draw_pending_ = true;
    }
}



View::Cursor DisplaysView::grab (Source *, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2> pick)
{
    std::ostringstream info;
    View::Cursor ret = Cursor();

    // grab coordinates in scene-View reference frame
    glm::vec3 scene_from = Rendering::manager().unProject(from, scene.root()->transform_);
    glm::vec3 scene_to   = Rendering::manager().unProject(to, scene.root()->transform_);
    glm::vec3 scene_translation = scene_to - scene_from;

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



