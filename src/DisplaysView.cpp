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

#include <string>
#include <sstream>
#include <iomanip>

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/vector_angle.hpp>

#include "ImGuiToolkit.h"

#include "Mixer.h"
#include "defines.h"
#include "Source.h"
#include "Settings.h"
#include "PickingVisitor.h"
#include "DrawVisitor.h"
#include "Decorations.h"
#include "UserInterfaceManager.h"
#include "BoundingBoxVisitor.h"
#include "RenderingManager.h"

#include "DisplaysView.h"


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

    // stored status of output_
    output_status_ = new Group;

    // Geometry Scene workspace
    output_ = new Group;
    scene.ws()->attach(output_);
    output_surface_ = new Surface;
    output_->attach(output_surface_);
    output_render_ = new Surface;
    output_->attach(output_render_);

    output_visible_ = new Handles(Handles::EYESLASHED);
    output_visible_->visible_ = false;
    output_visible_->color = glm::vec4( COLOR_FRAME, 1.f );
    output_->attach(output_visible_);

    // overlays for selected and not selected
    output_overlays_ = new Switch;
    output_->attach(output_overlays_);

    // output_overlays_ [0] is for not active output frame
    Frame *frame = new Frame(Frame::SHARP, Frame::THIN, Frame::NONE);
    frame->color = glm::vec4( COLOR_FRAME, 1.f );
    output_overlays_->attach(frame);

    // output_overlays_ [1] is for active frame
    Group *g = new Group;
    output_overlays_->attach(g);
    // Overlay menu icon
    output_menu_ = new Handles(Handles::MENU);
    output_menu_->color = glm::vec4( COLOR_FRAME, 1.f );
    g->attach(output_menu_);
    // selected frame
    frame = new Frame(Frame::SHARP, Frame::LARGE, Frame::NONE);
    frame->color = glm::vec4( COLOR_FRAME, 1.f );
    g->attach(frame);

    // Overlay has two modes : window or fullscreen
    output_mode_ = new Switch;
    g->attach(output_mode_);

    // output_mode_ [0] is for WINDOWED
    output_handles_ = new Handles(Handles::RESIZE);
    output_handles_->color = glm::vec4( COLOR_FRAME, 1.f );
    output_mode_->attach(output_handles_);

    // output_mode_ [1] is for FULLSCREEN
    output_fullscreen_ = new Symbol(Symbol::TELEVISION);
    output_fullscreen_->scale_ = glm::vec3(2.f, 2.f, 1.f);
    output_fullscreen_->color = glm::vec4( COLOR_FRAME, 1.f );
    output_mode_->attach(output_fullscreen_);

    // default behavior : selected output in windowed mode
    show_output_menu_ = false;
    output_selected_=true;
    output_overlays_->setActive(1);
    output_mode_->setActive(0);
    output_monitor_ = "";
    update_pending_ = false;

    // display actions : 0 = move output, 1 paint, 2 erase
    display_action_ = 0;
}

void DisplaysView::update(float dt)
{
    View::update(dt);

    // a more complete update is requested
    if (View::need_deep_update_ > 0) {

        // update rendering of render frame
        FrameBuffer *render = Mixer::manager().session()->frame();
        if (render)
            output_render_->setTextureIndex( render->texture() );


//        // prevent invalid scaling
//        float s = CLAMP(scene.root()->scale_.x, DISPLAYS_MIN_SCALE, DISPLAYS_MAX_SCALE);
//        scene.root()->scale_.x = s;
//        scene.root()->scale_.y = s;
    }

}


// recenter is called also when RenderingManager detects a change of monitors
void  DisplaysView::recenter ()
{
    // clear background display of monitors
    scene.clearBackground();
    scene.clearForeground();

    // fill scene background with the frames to show monitors
    int index = 1;
    std::map<std::string, glm::ivec4> _monitors = Rendering::manager().monitors();
    for (auto monitor_iter = _monitors.begin();
         monitor_iter != _monitors.end(); ++monitor_iter, ++index) {

        // get coordinates of monitor in Display units
        glm::vec4 rect = DISPLAYS_UNIT * glm::vec4(monitor_iter->second);

        // add a background black surface with glow shadow
        Group *m = new Group;
        m->scale_ = glm::vec3( 0.5f * rect.p, 0.5f * rect.q, 1.f );
        m->translation_ = glm::vec3( rect.x + m->scale_.x, -rect.y - m->scale_.y, 0.f );
        Surface *surf = new Surface( new Shader);
        surf->shader()->color =  glm::vec4( 0.1f, 0.1f, 0.1f, 1.f );

        m->attach(surf);
        Frame *frame = new Frame(Frame::SHARP, Frame::THIN, Frame::GLOW);
        frame->color = glm::vec4(COLOR_MONITOR, 1.f);
        m->attach(frame);
        Glyph  *label = new Glyph(4);
        label->setChar( std::to_string(index).back() );
        label->color = glm::vec4( COLOR_MONITOR, 1.f );
        label->translation_.y =  0.02f ;
        label->scale_.y =  0.4f / rect.p;
        m->attach(label);
        scene.bg()->attach( m );

        // add a foreground color frame
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

    // compute scaling to fit the scene box into the scene, modulo a margin ratio
    scene.root()->scale_.x = SCENE_UNIT / ( scene_box.scale().x * 1.3f);
    scene.root()->scale_.y = scene.root()->scale_.x;
    // compute translation to place at the center (considering scaling)
    scene.root()->translation_ = -scene.root()->scale_.x * scene_box.center();

}

void DisplaysView::resize ( int scale )
{
    float z = CLAMP(0.01f * (float) scale, 0.f, 1.f);
    z *= z;
    z *= DISPLAYS_MAX_SCALE - DISPLAYS_MIN_SCALE;
    z += DISPLAYS_MIN_SCALE;
    scene.root()->scale_.x = z;
    scene.root()->scale_.y = z;
}

int  DisplaysView::size ()
{
    float z = (scene.root()->scale_.x - DISPLAYS_MIN_SCALE) / (DISPLAYS_MAX_SCALE - DISPLAYS_MIN_SCALE);
    return (int) ( sqrt(z) * 100.f);
}

void DisplaysView::draw()
{
//    g_printerr("DisplaysView::draw()\n");

    output_render_->visible_ = !Settings::application.render.disabled;
    output_visible_->visible_ = Settings::application.render.disabled;


    // if user is not manipulating output frame
    // update the output frame to match the window dimensions
    if (!current_action_ongoing_ && !update_pending_) {
        // TODO Mutex for multithread access with changed flag

        if ( Settings::application.windows[1].fullscreen ) {
            // output overlay for fullscreen
            output_mode_->setActive( 1 );

            glm::ivec4 rect = Rendering::manager().monitors()[Settings::application.windows[1].monitor];
            output_->scale_.x = rect.p * 0.5f * DISPLAYS_UNIT;
            output_->scale_.y = rect.q * 0.5f * DISPLAYS_UNIT;
            output_->translation_.x = rect.x * DISPLAYS_UNIT + output_->scale_.x;
            output_->translation_.y = -rect.y * DISPLAYS_UNIT - output_->scale_.y;

        }
        else {
            // output overlay for window
            output_mode_->setActive( 0 );

            output_->scale_.x = Settings::application.windows[1].w * 0.5f * DISPLAYS_UNIT;
            output_->scale_.y = Settings::application.windows[1].h * 0.5f * DISPLAYS_UNIT;
            output_->translation_.x = Settings::application.windows[1].x * DISPLAYS_UNIT + output_->scale_.x;
            output_->translation_.y = -Settings::application.windows[1].y * DISPLAYS_UNIT - output_->scale_.y;
        }

    }

    // rendering of framebuffer in window
    if (Settings::application.windows[1].scaled)  {
        output_render_->scale_ = glm::vec3(1.f, 1.f, 1.f);
    }
    else {
        FrameBuffer *output = Mixer::manager().session()->frame();
        if (output){
            float out_ar = output_->scale_.x / output_->scale_.y;
            if (output->aspectRatio() < out_ar)
                output_render_->scale_ = glm::vec3(output->aspectRatio() / out_ar, 1.f, 1.f);
            else
                output_render_->scale_ = glm::vec3(1.f, out_ar / output->aspectRatio(), 1.f);
        }
    }

    // main call to draw the view
    View::draw();

    update_pending_ = false;
//    // Render the UI
//    if (output_render_ != nullptr){

//        // display interface
//        // Locate window at upper left corner
//        glm::vec2 P = glm::vec2(- 0.02f, 0.01 );
//        P = Rendering::manager().project(glm::vec3(P, 0.f), scene.root()->transform_, false);
//        // Set window position depending on icons size
//        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
//        ImGui::SetNextWindowPos(ImVec2(P.x, P.y - 1.5f * ImGui::GetFrameHeight() ), ImGuiCond_Always);
//        if (ImGui::Begin("##DisplaysMaskOptions", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground
//                         | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings
//                         | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus ))
//        {

//            // style grey
//            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(COLOR_MONITOR, 1.0f));  // 1
//            ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.14f, 0.14f, 0.14f, 0.9f));
//            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.14f, 0.f));
//            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.24f, 0.24f, 0.24f, 0.46f));
//            ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.85f, 0.85f, 0.85f, 0.86f));
//            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.95f, 0.95f, 0.95f, 1.00f));
//            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));
//            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.24f, 0.24f, 0.46f));
//            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.36f, 0.36f, 0.36f, 0.9f));
//            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.36f, 0.36f, 0.36f, 0.5f));
//            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.88f, 0.88f, 0.88f, 0.73f));
//            ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.83f, 0.83f, 0.84f, 0.78f));
//            ImGui::PushStyleColor(ImGuiCol_TabHovered, ImVec4(0.53f, 0.53f, 0.53f, 0.60f));
//            ImGui::PushStyleColor(ImGuiCol_TabActive, ImVec4(0.40f, 0.40f, 0.40f, 1.00f));   // 14 colors

//            // GUI for drawing

//            // select cursor
//            static bool on = true;
//            ImGui::SameLine(0, 60);
//            on = display_action_ == 0;
//            if (ImGuiToolkit::ButtonToggle(ICON_FA_MOUSE_POINTER, &on)) {
//                output_selected_=true;
//                display_action_ = 0;
//            }

//            ImGui::SameLine();
//            on = display_action_ == 1;
//            if (ImGuiToolkit::ButtonToggle(ICON_FA_PAINT_BRUSH, &on)) {
//                output_selected_=false;
//                display_action_ = 1;
//            }

//            ImGui::SameLine();
//            on = display_action_ == 2;
//            if (ImGuiToolkit::ButtonToggle(ICON_FA_ERASER, &on)) {
//                output_selected_=false;
//                display_action_ = 2;
//            }

//            if (display_action_ > 0) {

//                ImGui::SameLine(0, 50);
//                ImGui::SetNextItemWidth( ImGui::GetTextLineHeight() * 2.6);
//                const char* items[] = { ICON_FA_CIRCLE, ICON_FA_SQUARE };
//                static int item = 0;
//                item = (int) round(Settings::application.brush.z);
//                if(ImGui::Combo("##DisplayBrushShape", &item, items, IM_ARRAYSIZE(items))) {
//                    Settings::application.brush.z = float(item);
//                }

//                ImGui::SameLine();
////                show_cursor_forced_ = false;
//                if (ImGui::Button(ICON_FA_DOT_CIRCLE ICON_FA_SORT_DOWN ))
//                    ImGui::OpenPopup("display_brush_size_popup");
//                if (ImGui::BeginPopup("display_brush_size_popup", ImGuiWindowFlags_NoMove))
//                {
////                    int pixel_size_min = int(0.05 * edit_source_->frame()->height() );
////                    int pixel_size_max = int(2.0 * edit_source_->frame()->height() );
////                    int pixel_size = int(Settings::application.brush.x * edit_source_->frame()->height() );
////                    show_cursor_forced_ = true;
////                    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_DEFAULT);
////                    ImGuiToolkit::Indication("Large  ", 16, 1, ICON_FA_ARROW_RIGHT);
////                    if (ImGui::VSliderInt("##BrushSize", ImVec2(30,260), &pixel_size, pixel_size_min, pixel_size_max, "") ){
////                        Settings::application.brush.x = CLAMP(float(pixel_size) / edit_source_->frame()->height(), BRUSH_MIN_SIZE, BRUSH_MAX_SIZE);
////                    }
////                    if (ImGui::IsItemHovered() || ImGui::IsItemActive() )  {
////                        ImGui::BeginTooltip();
////                        ImGui::Text("%d px", pixel_size);
////                        ImGui::EndTooltip();
////                    }
////                    ImGuiToolkit::Indication("Small  ", 15, 1, ICON_FA_ARROW_LEFT);
////                    ImGui::PopFont();
//                    ImGui::EndPopup();
//                }
//                // make sure the visual brush is up to date
////                glm::vec2 s = glm::vec2(Settings::application.brush.x);
////                mask_cursor_circle_->scale_ = glm::vec3(s * 1.16f, 1.f);
////                mask_cursor_square_->scale_ = glm::vec3(s * 1.75f, 1.f);

//                ImGui::SameLine();
//                if (ImGui::Button(ICON_FA_FEATHER_ALT ICON_FA_SORT_DOWN ))
//                    ImGui::OpenPopup("display_brush_pressure_popup");
//                if (ImGui::BeginPopup("display_brush_pressure_popup", ImGuiWindowFlags_NoMove))
//                {
////                    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_DEFAULT);
////                    ImGuiToolkit::Indication("Light  ", ICON_FA_FEATHER_ALT, ICON_FA_ARROW_UP);
////                    ImGui::VSliderFloat("##BrushPressure", ImVec2(30,260), &Settings::application.brush.y, BRUSH_MAX_PRESS, BRUSH_MIN_PRESS, "", 0.3f);
////                    if (ImGui::IsItemHovered() || ImGui::IsItemActive() )  {
////                        ImGui::BeginTooltip();
////                        ImGui::Text("%.1f%%", Settings::application.brush.y * 100.0);
////                        ImGui::EndTooltip();
////                    }
////                    ImGuiToolkit::Indication("Heavy  ", ICON_FA_WEIGHT_HANGING, ICON_FA_ARROW_DOWN);
////                    ImGui::PopFont();
//                    ImGui::EndPopup();
//                }

////                // store mask if changed, reset after applied
////                if (edit_source_->maskShader()->effect > 0)
////                    edit_source_->storeMask();
////                edit_source_->maskShader()->effect = 0;

//                // menu for effects
//                ImGui::SameLine(0, 60);
//                if (ImGui::Button(ICON_FA_MAGIC ICON_FA_SORT_DOWN ))
//                    ImGui::OpenPopup( "display_brush_menu_popup" );
//                if (ImGui::BeginPopup( "display_brush_menu_popup" ))
//                {
////                    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_DEFAULT);
////                    std::ostringstream oss;
////                    oss << edit_source_->name();
////                    int e = 0;
////                    if (ImGui::Selectable( ICON_FA_BACKSPACE "\tClear")) {
////                        e = 1;
////                        oss << ": Clear " << MASK_PAINT_ACTION_LABEL;
////                    }
////                    if (ImGui::Selectable( ICON_FA_ADJUST "\tInvert")) {
////                        e = 2;
////                        oss << ": Invert " << MASK_PAINT_ACTION_LABEL;
////                    }
////                    if (ImGui::Selectable( ICON_FA_WAVE_SQUARE "\tEdge")) {
////                        e = 3;
////                        oss << ": Edge " << MASK_PAINT_ACTION_LABEL;
////                    }
////                    if (e>0) {
////                        edit_source_->maskShader()->effect = e;
////                        edit_source_->maskShader()->cursor = glm::vec4(100.0, 100.0, 0.f, 0.f);
////                        edit_source_->touch();
////                        Action::manager().store(oss.str());
////                    }
////                    ImGui::PopFont();
//                    ImGui::EndPopup();
//                }

//                static DialogToolkit::OpenImageDialog display_mask_dialog("Select Image");

//                ImGui::SameLine();
//                if (ImGui::Button(ICON_FA_FOLDER_OPEN))
//                    display_mask_dialog.open();
//                if (display_mask_dialog.closed() && !display_mask_dialog.path().empty())
//                {
//                    FrameBufferImage *img = new FrameBufferImage(display_mask_dialog.path());
////                    if (edit_source_->maskbuffer_->fill( img )) {
////                        // apply mask filled
////                        edit_source_->storeMask();
////                        // store history
////                        std::ostringstream oss;
////                        oss << edit_source_->name() << ": Mask fill with " << maskdialog.path();
////                        Action::manager().store(oss.str());
////                    }
//                }


//            }

//            ImGui::PopStyleColor(14);  // 14 colors
//            ImGui::End();
//        }
//        ImGui::PopFont();

//    }

    // display popup menu
    if (show_output_menu_) {
        ImGui::OpenPopup( "DisplaysOutputContextMenu" );
        show_output_menu_ = false;
    }
    if (ImGui::BeginPopup("DisplaysOutputContextMenu")) {

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(COLOR_FRAME_LIGHT, 1.f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(COLOR_MENU_HOVERED, 0.5f));

        ImGui::MenuItem( MENU_OUTPUTDISABLE, SHORTCUT_OUTPUTDISABLE, &Settings::application.render.disabled);

        ImGui::MenuItem( ICON_FA_EXPAND_ARROWS_ALT  "   Scaled", nullptr, &Settings::application.windows[1].scaled );

        if (ImGui::MenuItem( ICON_FA_VECTOR_SQUARE "  Reset" )){
            // reset resolution to 1:1
            glm::ivec4 rect = outputCoordinates();
            rect.p = Mixer::manager().session()->frame()->width();
            rect.q = Mixer::manager().session()->frame()->height();
            Rendering::manager().outputWindow().setCoordinates( rect );
            // reset attributes
            Settings::application.windows[1].scaled = false;
            Rendering::manager().outputWindow().exitFullscreen();
        }

        ImGui::Separator();
        bool _windowed = !Settings::application.windows[1].fullscreen;
        if (ImGui::MenuItem( ICON_FA_WINDOW_RESTORE "   Window", nullptr, &_windowed)){
            Rendering::manager().outputWindow().exitFullscreen();
            // not fullscreen on a monitor
            output_monitor_ = "";
        }
        int index = 1;
        std::map<std::string, glm::ivec4> _monitors = Rendering::manager().monitors();
        for (auto monitor_iter = _monitors.begin();
             monitor_iter != _monitors.end(); ++monitor_iter, ++index) {

            bool _fullscreen = Settings::application.windows[1].fullscreen &&
                    Settings::application.windows[1].monitor == monitor_iter->first;
            std::string menutext = std::string( ICON_FA_TV "  Fullscreen ") + std::to_string(index);
            if (ImGui::MenuItem( menutext.c_str(), nullptr, _fullscreen )){
                output_monitor_ = monitor_iter->first;
                Rendering::manager().outputWindow().setFullscreen( output_monitor_ );
            }
        }
        ImGui::PopStyleColor(2);
        ImGui::EndPopup();
    }

}

std::pair<Node *, glm::vec2> DisplaysView::pick(glm::vec2 P)
{
    // get picking from generic View
    std::pair<Node *, glm::vec2> pick = View::pick(P);

    // ignore pick on render surface: it's the same as output surface
    if (pick.first == output_render_ || pick.first == output_fullscreen_)
        pick.first = output_surface_;

    // detect clic on menu
    if (pick.first == output_menu_)
        show_output_menu_ = true;

    // activate / deactivate output if clic on any element of it
    output_selected_ = (pick.first == output_surface_) ||
            (pick.first == output_handles_) ||
            (pick.first == output_menu_);
    output_overlays_->setActive(output_selected_ ? 1 : 0);

    // ignore anything else than selected output
    if (!output_selected_)
        pick.first = nullptr;

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

    output_selected_ = !pv.empty();
    output_overlays_->setActive(output_selected_ ? 0 : 1);
}

void DisplaysView::initiate()
{
    // initiate pending action
    if (!current_action_ongoing_) {

        // store status
        output_status_->copyTransform(output_);

        // initiated
        current_action_ = "";
        current_action_ongoing_ = true;
    }
}

void DisplaysView::terminate(bool force)
{
    // terminate pending action
    if (current_action_ongoing_ || force) {

        if (Settings::application.windows[1].fullscreen) {
            // Apply change of fullscreen monitor
            if ( output_monitor_.compare(Settings::application.windows[1].monitor) != 0 )
                Rendering::manager().outputWindow().setFullscreen( output_monitor_ );
        }
        else {
            // Apply coordinates to actual output window
            Rendering::manager().outputWindow().setCoordinates( outputCoordinates() );
        }

        // reset indicators
        output_handles_->overlayActiveCorner(glm::vec2(0.f, 0.f));

        // terminated
        current_action_ = "";
        current_action_ongoing_ = false;
        update_pending_ = true;
    }
}


glm::ivec4 DisplaysView::outputCoordinates() const
{
    glm::ivec4 rect;

    rect.x = (output_->translation_.x - output_->scale_.x) / DISPLAYS_UNIT;
    rect.y = (output_->translation_.y + output_->scale_.y) / - DISPLAYS_UNIT;
    rect.p = 2.f * output_->scale_.x / DISPLAYS_UNIT;
    rect.q = 2.f * output_->scale_.y / DISPLAYS_UNIT;

    return rect;
}

std::string DisplaysView::outputFullscreenMonitor() const
{
    return output_monitor_;
}

View::Cursor DisplaysView::grab (Source *, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2> pick)
{
    std::ostringstream info;
    View::Cursor ret = Cursor();

    // grab coordinates in scene-View reference frame
    glm::vec3 scene_from = Rendering::manager().unProject(from, scene.root()->transform_);
    glm::vec3 scene_to   = Rendering::manager().unProject(to, scene.root()->transform_);
    glm::vec3 scene_translation = scene_to - scene_from;

    // grab only works if not fullscreen
    if (!Settings::application.windows[1].fullscreen) {

        // grab surface to move
        if ( pick.first == output_surface_ ){

            // apply translation
            output_->translation_ = output_status_->translation_ + scene_translation;
            glm::ivec4 r = outputCoordinates();

            // discretized translation with ALT
            if (UserInterface::manager().altModifier()) {
                r.x = ROUND(r.x, 0.01f);
                r.y = ROUND(r.y, 0.01f);
                output_->translation_.x = (r.x * DISPLAYS_UNIT) + output_->scale_.x;
                output_->translation_.y = (r.y * - DISPLAYS_UNIT) - output_->scale_.y;
            }

            // Show move cursor
            output_handles_->overlayActiveCorner(glm::vec2(-1.f, 1.f));
            ret.type = Cursor_ResizeAll;
            info << "Window position " << r.x << ", " << r.y << " px";
        }
        // grab handle to resize
        else if ( pick.first == output_handles_ ){

            // which corner was picked ?
            glm::vec2 corner = glm::round(pick.second);
            // inform on which corner should be overlayed (opposite)
            output_handles_->overlayActiveCorner(-corner);

            // transform from source center to corner
            glm::mat4 T = GlmToolkit::transform(glm::vec3(corner.x, corner.y, 0.f), glm::vec3(0.f, 0.f, 0.f),
                                                glm::vec3(1.f, 1.f, 1.f));

            // transformation from scene to corner:
            glm::mat4 scene_to_corner_transform = T * glm::inverse(output_status_->transform_);
            glm::mat4 corner_to_scene_transform = glm::inverse(scene_to_corner_transform);

            // compute cursor movement in corner reference frame
            glm::vec4 corner_from = scene_to_corner_transform * glm::vec4( scene_from,  1.f );
            glm::vec4 corner_to   = scene_to_corner_transform * glm::vec4( scene_to,  1.f );

            // operation of scaling in corner reference frame
            glm::vec3 corner_scaling = glm::vec3(corner_to) / glm::vec3(corner_from);
            glm::ivec4 rect;

            // RESIZE CORNER
            // proportional SCALING with SHIFT
            if (UserInterface::manager().shiftModifier()) {
                // calculate proportional scaling factor
                float factor = glm::length( glm::vec2( corner_to ) ) / glm::length( glm::vec2( corner_from ) );
                // scale node
                output_->scale_ = output_status_->scale_ * glm::vec3(factor, factor, 1.f);
            }
            // non-proportional CORNER RESIZE  (normal case)
            else {
                // scale node
                output_->scale_ = output_status_->scale_ * corner_scaling;
            }

            // discretized scaling with ALT
            if (UserInterface::manager().altModifier()) {
                // calculate ratio of scaling modulo the output resolution
                glm::vec3 outputsize = output_->scale_ / DISPLAYS_UNIT;
                glm::vec3 framesize = Mixer::manager().session()->frame()->resolution();
                glm::vec3 ra = outputsize / framesize;
                ra.x = ROUND(ra.x, 20.f);
                ra.y = ROUND(ra.y, 20.f);
                outputsize = ra * framesize;
                output_->scale_.x = outputsize.x * DISPLAYS_UNIT;
                output_->scale_.y = outputsize.y * DISPLAYS_UNIT;
            }
            // update corner scaling to apply to center coordinates
            corner_scaling = output_->scale_ / output_status_->scale_;

            // TRANSLATION CORNER
            // convert source position in corner reference frame
            glm::vec4 center = scene_to_corner_transform * glm::vec4( output_status_->translation_, 1.f);
            // transform source center (in corner reference frame)
            center = glm::scale(glm::identity<glm::mat4>(), corner_scaling) * center;
            // convert center back into scene reference frame
            center = corner_to_scene_transform * center;
            // apply to node
            output_->translation_ = glm::vec3(center);

            // show cursor depending on diagonal (corner picked)
            T = glm::rotate(glm::identity<glm::mat4>(), output_status_->rotation_.z, glm::vec3(0.f, 0.f, 1.f));
            T = glm::scale(T, output_status_->scale_);
            corner = T * glm::vec4( corner, 0.f, 0.f );
            ret.type = corner.x * corner.y > 0.f ? Cursor_ResizeNESW : Cursor_ResizeNWSE;

            rect = outputCoordinates();
            info << "Window size " << rect.p << " x "  << rect.q << " px";
        }
    }
    else {

        // grab fullscreen output
        if ( pick.first == output_surface_ ){

            // convert mouse cursor coordinates to displays coordinates
            scene_to *= glm::vec3(1.f/DISPLAYS_UNIT, -1.f/DISPLAYS_UNIT, 1.f);

            // loop over all monitors
            std::map<std::string, glm::ivec4> _monitors = Rendering::manager().monitors();
            int index = 1;
            for (auto monitor_iter = _monitors.begin();
                 monitor_iter != _monitors.end(); ++monitor_iter, ++index) {

                // if the mouse cursor is over a monitor
                glm::ivec4 r = monitor_iter->second;
                if (scene_to.x > r.x && scene_to.x < r.x + r.p
                        && scene_to.y > r.y && scene_to.y < r.y + r.q) {

                    // show output frame on top of that monitor
                    output_->scale_.x = r.p * 0.5f * DISPLAYS_UNIT;
                    output_->scale_.y = r.q * 0.5f * DISPLAYS_UNIT;
                    output_->translation_.x = r.x * DISPLAYS_UNIT + output_->scale_.x;
                    output_->translation_.y = -r.y * DISPLAYS_UNIT - output_->scale_.y;

                    // remember the output monitor selected
                    output_monitor_ = monitor_iter->first;

                    // Show  cursor
                    ret.type = Cursor_Hand;
                    info << "Fullscreen Monitor " << index << " (" << output_monitor_ << ")\n   "<<
                            r.p << " x "  << r.q << " px";
                }
            }
        }
    }

    // update cursor
    ret.info = info.str();

    return ret;
}


void DisplaysView::arrow (glm::vec2 movement)
{
    static float accumulator = 0.f;
    accumulator += dt_;

    glm::vec3 gl_Position_from = Rendering::manager().unProject(glm::vec2(0.f), scene.root()->transform_);
    glm::vec3 gl_Position_to   = Rendering::manager().unProject(movement, scene.root()->transform_);
    glm::vec3 gl_delta = gl_Position_to - gl_Position_from;

}

