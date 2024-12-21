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

#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>


#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/vector_angle.hpp>

#include "ImGuiToolkit.h"
#include "imgui_internal.h"

#include "Log.h"
#include "ImageFilter.h"
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
#include "MousePointer.h"

#include "DisplaysView.h"

#define WINDOW_TITLEBAR_HEIGHT 0.03f

FilteringProgram _whitebalance("Whitebalance", "shaders/filters/whitebalance.glsl", "", { { "Red", 1.0}, { "Green", 1.0}, { "Blue", 1.0}, { "Temperature", 0.5} });



WindowPreview::~WindowPreview()
{
    if (renderbuffer_)
        delete renderbuffer_;
    if (output_render_)
        delete output_render_;
}

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

    // create and attach all window manipulation objects
    windows_ = std::vector<WindowPreview>(MAX_OUTPUT_WINDOW);
    for (auto w = windows_.begin(); w != windows_.end(); ++w){

        // surface & buffer for render
        w->output_render_ = new MeshSurface;
        w->renderbuffer_ = new FrameBuffer(1024, 1024);

        // root node
        w->root_ = new Group;
        scene.ws()->attach(w->root_);
        w->root_->visible_ = false;
        // title bar
        w->title_ = new Surface (new Shader);
        w->title_->shader()->color = glm::vec4( COLOR_WINDOW, 1.f );
        w->title_->scale_ = glm::vec3(1.002f, WINDOW_TITLEBAR_HEIGHT, 1.f);
        w->title_->translation_ = glm::vec3(0.f, 1.f + WINDOW_TITLEBAR_HEIGHT, 0.f);
        w->root_->attach(w->title_);
        // surface background and texture
        w->shader_ = new ImageFilteringShader;
        w->shader_->setCode( _whitebalance.code().first );
        w->surface_ = new FrameBufferSurface(w->renderbuffer_, w->shader_);
        w->root_->attach(w->surface_);
        // icon if disabled
        w->icon_ = new Handles(Handles::EYESLASHED);
        w->icon_->visible_ = false;
        w->icon_->color = glm::vec4( COLOR_WINDOW, 1.f );
        w->root_->attach(w->icon_);
        // overlays for selected and not selected
        w->overlays_ = new Switch;
        w->root_->attach(w->overlays_);
        // overlays_ [0] is for not active frame
        Frame *frame = new Frame(Frame::SHARP, Frame::THIN, Frame::DROP);
        frame->color = glm::vec4( COLOR_WINDOW, 0.5f );
        w->overlays_->attach(frame);
        // overlays_ [1] is for active frame
        Group *g = new Group;
        w->overlays_->attach(g);
        // Overlay menu icon
        w->menu_ = new Handles(Handles::MENU);
        w->menu_->color = glm::vec4( COLOR_WINDOW, 1.f );
        g->attach(w->menu_);
        // selected frame
        frame = new Frame(Frame::SHARP, Frame::LARGE, Frame::NONE);
        frame->color = glm::vec4( COLOR_WINDOW, 1.f );
        g->attach(frame);
        // Overlay has two modes : window or fullscreen
        w->mode_ = new Switch;
        g->attach(w->mode_);
        // mode_ [0] is for WINDOWED
        w->resize_ = new Handles(Handles::SCALE);
        w->resize_->color = glm::vec4( COLOR_WINDOW, 1.f );
        w->mode_->attach(w->resize_);
        // mode_ [1] is for FULLSCREEN
        w->fullscreen_ = new Symbol(Symbol::TELEVISION);
        w->fullscreen_->scale_ = glm::vec3(2.f, 2.f, 1.f);
        w->fullscreen_->color = glm::vec4( COLOR_WINDOW, 1.f );
        w->mode_->attach(w->fullscreen_);
        // Output frame
        w->output_group_ = new Group;
        w->root_->attach(w->output_group_);
        w->output_handles_[0] = new Handles(Handles::NODE_LOWER_LEFT);
        w->output_handles_[0]->color = glm::vec4( COLOR_FRAME, 1.f );
        w->output_group_->attach(w->output_handles_[0]);
        w->output_handles_[1] = new Handles(Handles::NODE_UPPER_LEFT);
        w->output_handles_[1]->color = glm::vec4( COLOR_FRAME, 1.f );
        w->output_group_->attach(w->output_handles_[1]);
        w->output_handles_[2] = new Handles(Handles::NODE_LOWER_RIGHT);
        w->output_handles_[2]->color = glm::vec4( COLOR_FRAME, 1.f );
        w->output_group_->attach(w->output_handles_[2]);
        w->output_handles_[3] = new Handles(Handles::NODE_UPPER_RIGHT);
        w->output_handles_[3]->color = glm::vec4( COLOR_FRAME, 1.f );
        w->output_group_->attach(w->output_handles_[3]);
        std::vector<glm::vec2> path;
        path.push_back(glm::vec2(-1.f, -1.f));
        path.push_back(glm::vec2(-1.f,  1.f));
        path.push_back(glm::vec2( 1.f,  1.f));
        path.push_back(glm::vec2( 1.f, -1.f));
        w->output_lines_ = new LineLoop(path, 2.f);
        w->output_lines_->shader()->color = glm::vec4(COLOR_FRAME, 0.96f);
        w->output_group_->attach(w->output_lines_);

        // default to not active & window overlay frame
        w->output_group_->visible_ = false;
        w->overlays_->setActive(0);
        w->mode_->setActive(0);
    }

    // initial behavior: no window selected, no menu
    show_window_menu_ = false;
    current_window_ = -1;
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
}

void DisplaysView::update(float dt)
{
    View::update(dt);

    // specific update when this view is active
    if ( Mixer::manager().view() == this ) {

        // update rendering of render frame
        for (int i = 0; i < MAX_OUTPUT_WINDOW; ++i) {

            // ensure to update texture index
            windows_[i].output_render_->setTextureIndex(
                Rendering::manager().outputWindow(i).texture());

            // update visible flag
            windows_[i].root_->visible_ = i < Settings::application.num_output_windows;
            windows_[i].icon_->visible_ = Settings::application.render.disabled;

            // avoid busy update
            if (windows_[i].need_update_) {
                --windows_[i].need_update_;

                // Rendering of output is distorted with custom fit
                if (Settings::application.windows[i+1].custom)  {

                    // reset scale
                    windows_[i].output_render_->scale_ = glm::vec3(1.f);

                    // update node distortion
                    windows_[i].output_group_->data_ = Settings::application.windows[i + 1].nodes;
                    for (int k = 0; k < 4; ++k) {
                        windows_[i].output_handles_[k]->translation_.x
                            = windows_[i].output_group_->data_[k].x;
                        windows_[i].output_handles_[k]->translation_.y
                            = windows_[i].output_group_->data_[k].y;
                    }

                    // update the shape of the distorted outline
                    std::vector<glm::vec2> path;
                    path.push_back(glm::vec2(-1.f, -1.f) + glm::vec2(windows_[i].output_handles_[0]->translation_));
                    path.push_back(glm::vec2(-1.f,  1.f) + glm::vec2(windows_[i].output_handles_[1]->translation_));
                    path.push_back(glm::vec2( 1.f,  1.f) + glm::vec2(windows_[i].output_handles_[3]->translation_));
                    path.push_back(glm::vec2( 1.f, -1.f) + glm::vec2(windows_[i].output_handles_[2]->translation_));
                    windows_[i].output_lines_->changePath(path);

                    // apply nodes distortion
                    ImageShader *is = dynamic_cast<ImageShader*>(windows_[i].output_render_->shader());
                    is->iNodes = windows_[i].output_group_->data_;

                    // show output frame
                    windows_[i].output_group_->visible_ = true;
                }
                // Rendering of output is adjusted to match aspect ratio of framebuffer
                else {
                    // apply scaling
                    float out_ar = windows_[i].root_->scale_.x / windows_[i].root_->scale_.y;
                    if (output_ar < out_ar)
                        windows_[i].output_render_->scale_ = glm::vec3(output_ar / out_ar, 1.f, 1.f);
                    else
                        windows_[i].output_render_->scale_ = glm::vec3(1.f, out_ar / output_ar, 1.f);

                    // reset nodes distortion
                    ImageShader *is = dynamic_cast<ImageShader*>(windows_[i].output_render_->shader());
                    is->iNodes = glm::zero<glm::mat4>();

                    // do not show output frame
                    windows_[i].output_group_->visible_ = false;
                }

                // Highlight current window
                if ( current_window_ == i ) {
                    windows_[i].overlays_->setActive(1);
                    for (int k = 0; k < 4; ++k)
                        windows_[i].output_handles_[k]->visible_ = true;
                    windows_[i].output_lines_->shader()->color = glm::vec4( COLOR_FRAME, 0.96f );
                    windows_[i].title_->shader()->color = glm::vec4( COLOR_WINDOW, 1.f );
                }
                else {
                    windows_[i].overlays_->setActive(0);
                    for (int k = 0; k < 4; ++k)
                        windows_[i].output_handles_[k]->visible_ = false;
                    windows_[i].output_lines_->shader()->color = glm::vec4( COLOR_FRAME, 0.3f );
                    windows_[i].title_->shader()->color = glm::vec4( COLOR_WINDOW, 0.8f );
                }
            }
        }

        output_ar = Mixer::manager().session()->frame()->aspectRatio();
    }

    // a more complete update is requested
    if (View::need_deep_update_ > 0) {
        // change grid color
        ImVec4 c = ImGuiToolkit::HighlightColor();
        grid->setColor( glm::vec4(c.x, c.y, c.z, 0.3) );

        // force update
        for (int i = 0; i < MAX_OUTPUT_WINDOW; ++i)
            ++windows_[i].need_update_;
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

    // fill scene background with the frames to show monitors
    int index = 1;
    std::map<std::string, glm::ivec4> _monitors = Rendering::manager().monitors();
    for (auto monitor_iter = _monitors.begin();
         monitor_iter != _monitors.end(); ++monitor_iter, ++index) {

        // get coordinates of monitor in Display units
        glm::vec4 rect = DISPLAYS_UNIT * glm::vec4(monitor_iter->second);

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
        label->translation_.y =  0.02f ;
        label->scale_.y =  0.4f / rect.p;
        m->attach(label);
        scene.bg()->attach( m );

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
    float viewar = (float) Settings::application.windows[0].w / (float) Settings::application.windows[0].h;
    glm::mat4 scale = glm::scale(glm::identity<glm::mat4>(), glm::vec3(viewar > 1.f ? 1.f : 1.f / viewar, viewar > 1.f ? viewar : 1.f, 1.f));
    glm::vec4 viewport = glm::vec4(0.f, 0.f, Settings::application.windows[0].w, Settings::application.windows[0].h);
    glm::vec3 view = glm::unProject(glm::vec3(0.f),glm::identity<glm::mat4>(), projection * scale, viewport);
    view = glm::abs(view);

    // compute scaling to fit the scene box into the view
    if ( scene_box.scale().x > scene_box.scale().y ) {
        // horizontal arrangement
        scene.root()->scale_.x = glm::min(view.x, view.y) / ( scene_box.scale().x );
        scene.root()->scale_.y = scene.root()->scale_.x;
    }
    else {
        // vertical arrangement
        scene.root()->scale_.y = glm::min(view.x, view.y) / ( scene_box.scale().y );
        scene.root()->scale_.x = scene.root()->scale_.y;
    }

    // compute translation to place at the center (considering scaling, + shift for buttons left and above)
    scene.root()->translation_ = -scene.root()->scale_.x * (scene_box.center() + glm::vec3(-0.2f, 0.2f, 0.f));
}

void DisplaysView::resize ( int scale )
{
    float z = CLAMP(0.01f * (float) scale, 0.f, 1.f);
    z *= z; // square
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


void DisplaysView::adaptGridToWindow(int w)
{
    // reset by default
    gridroot_->scale_ = glm::vec3(1.f);
    gridroot_->translation_ = glm::vec3(0.f);
    grid->setAspectRatio(1.f);

    // adapt grid scaling to given window if parameter is given
    if (w > -1) {
        gridroot_->scale_.x = windows_[w].root_->scale_.x;
        gridroot_->scale_.y = windows_[w].root_->scale_.y;
        gridroot_->translation_.x = windows_[w].root_->translation_.x;
        gridroot_->translation_.y = windows_[w].root_->translation_.y;

        if (!Settings::application.proportional_grid)
            grid->setAspectRatio((float) windows_[w].root_->scale_.y / windows_[w].root_->scale_.x);

    }
    // set grid aspect ratio to display size otherwise
    else if (current_window_ > -1) {

        if (Settings::application.proportional_grid) {
            std::string m = Rendering::manager()
                                .monitorNameAt(Settings::application.windows[current_window_ + 1].x,
                                               Settings::application.windows[current_window_ + 1].y);
            glm::ivec4 rect = Rendering::manager().monitors()[m];

            gridroot_->translation_.x = rect.x * DISPLAYS_UNIT;
            gridroot_->translation_.y = rect.y * -DISPLAYS_UNIT;
            gridroot_->scale_.x = rect.p * 0.5f * DISPLAYS_UNIT;
            gridroot_->scale_.y = rect.q * 0.5f * DISPLAYS_UNIT;
        }
    }
}

void DisplaysView::draw()
{
    // draw all windows
    int i = 0;
    for (; i < Settings::application.num_output_windows; ++i) {

        // Render the output into the render buffer (displayed on the FrameBufferSurface surface_)
        windows_[i].output_render_->update(0.f);
        windows_[i].renderbuffer_->begin();
        windows_[i].output_render_->draw(glm::identity<glm::mat4>(), windows_[i].renderbuffer_->projection());
        windows_[i].renderbuffer_->end();

        // ensure the shader of the surface_ is configured
        windows_[i].shader_->uniforms_["Red"] = Settings::application.windows[i+1].whitebalance.x;
        windows_[i].shader_->uniforms_["Green"] = Settings::application.windows[i+1].whitebalance.y;
        windows_[i].shader_->uniforms_["Blue"] = Settings::application.windows[i+1].whitebalance.z;
        windows_[i].shader_->uniforms_["Temperature"] = Settings::application.windows[i+1].whitebalance.w;
        windows_[i].shader_->uniforms_["Contrast"] = Settings::application.windows[i+1].contrast;
        windows_[i].shader_->uniforms_["Brightness"] = Settings::application.windows[i+1].brightness;

        // update overlay
        if ( Settings::application.windows[i+1].fullscreen ) {
            // output overlay for fullscreen
            windows_[i].mode_->setActive( 1 );
            windows_[i].title_->visible_ = false;

            glm::ivec4 rect = Rendering::manager().monitors()[Settings::application.windows[i+1].monitor];
            glm::vec2 TopLeft = glm::vec2(rect.x * DISPLAYS_UNIT, -rect.y * DISPLAYS_UNIT);
            TopLeft = Rendering::manager().project(glm::vec3(TopLeft, 0.f), scene.root()->transform_, false);
            ImGui::SetNextWindowPos( ImVec2( TopLeft.x, TopLeft.y), ImGuiCond_Always);

            glm::vec2 BottomRight = glm::vec2( (rect.x + rect.p) * DISPLAYS_UNIT, -(rect.y + rect.q) * DISPLAYS_UNIT);
            BottomRight = Rendering::manager().project(glm::vec3(BottomRight, 0.f), scene.root()->transform_, false);
            ImGui::SetNextWindowSize( ImVec2( BottomRight.x - TopLeft.x, BottomRight.y - TopLeft.y), ImGuiCond_Always);

            ImGui::SetNextWindowBgAlpha(0.0f); // Transparent background
            if (ImGui::Begin(Settings::application.windows[i+1].name.c_str(), NULL, ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus))
            {
                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                draw_list->AddRectFilled(ImVec2(TopLeft.x, TopLeft.y + 4), ImVec2(BottomRight.x, TopLeft.y + ImGui::GetTextLineHeightWithSpacing()), IMGUI_COLOR_OVERLAY);

                ImGuiToolkit::PushFont(ImGuiToolkit::FONT_MONO);
                ImGui::TextColored(ImVec4(COLOR_WINDOW, 1.0f), ICON_FA_TV " %d %s  %d x %d px", i+1,
                                   Settings::application.windows[i+1].monitor.c_str(), rect.p, rect.q);
                ImGui::PopFont();

                ImGui::End();
            }
        }
        else {
            // output overlay for window
            windows_[i].mode_->setActive( 0 );
            windows_[i].title_->visible_ = Settings::application.windows[i+1].decorated;

            glm::vec2 TopLeft = glm::vec2(Settings::application.windows[i+1].x * DISPLAYS_UNIT, -Settings::application.windows[i+1].y * DISPLAYS_UNIT);
            TopLeft = Rendering::manager().project(glm::vec3(TopLeft, 0.f), scene.root()->transform_,
                                                   Settings::application.windows[0].fullscreen);
            ImGui::SetNextWindowPos( ImVec2( TopLeft.x, TopLeft.y), ImGuiCond_Always);

            glm::vec2 BottomRight = glm::vec2( (Settings::application.windows[i+1].x + Settings::application.windows[i+1].w) * DISPLAYS_UNIT,
                    -(Settings::application.windows[i+1].y + Settings::application.windows[i+1].h) * DISPLAYS_UNIT);
            BottomRight = Rendering::manager().project(glm::vec3(BottomRight, 0.f), scene.root()->transform_,
                                                        Settings::application.windows[0].fullscreen);
            ImGui::SetNextWindowSize( ImVec2( BottomRight.x - TopLeft.x, BottomRight.y - TopLeft.y), ImGuiCond_Always);

            ImGui::SetNextWindowBgAlpha(0.0f); // Transparent background
            if (ImGui::Begin(Settings::application.windows[i+1].name.c_str(), NULL, ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus))
            {
                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                draw_list->AddRectFilled(ImVec2(TopLeft.x, TopLeft.y + 4), ImVec2(BottomRight.x, TopLeft.y + ImGui::GetTextLineHeightWithSpacing()), IMGUI_COLOR_OVERLAY);

                ImGuiToolkit::PushFont(ImGuiToolkit::FONT_MONO);
                ImGui::TextColored(ImVec4(COLOR_WINDOW, 1.0f), ICON_FA_WINDOW_MAXIMIZE " %d (%d,%d)  %d x %d px", i+1,
                                   Settings::application.windows[i+1].x, Settings::application.windows[i+1].y,
                        Settings::application.windows[i+1].w, Settings::application.windows[i+1].h);
                ImGui::PopFont();

                ImGui::End();
            }
        }
    }
    for (; i < MAX_OUTPUT_WINDOW; ++i)
        windows_[i].root_->visible_ = false;

    // if user is not manipulating output frame
    // update the output frame to match the window dimensions
    // TODO Mutex for multithread access with changed flag
    // TODO less busy update?
    if (!current_action_ongoing_ && !draw_pending_) {

        for (int i = 0; i < Settings::application.num_output_windows; ++i) {

            if ( Settings::application.windows[i+1].fullscreen ) {
                glm::ivec4 rect = Rendering::manager().monitors()[Settings::application.windows[i+1].monitor];
                windows_[i].root_->scale_.x = rect.p * 0.5f * DISPLAYS_UNIT;
                windows_[i].root_->scale_.y = rect.q * 0.5f * DISPLAYS_UNIT;
                windows_[i].root_->translation_.x = rect.x * DISPLAYS_UNIT + windows_[i].root_->scale_.x;
                windows_[i].root_->translation_.y = -rect.y * DISPLAYS_UNIT - windows_[i].root_->scale_.y;
            }
            else {
                windows_[i].root_->scale_.x = Settings::application.windows[i+1].w * 0.5f * DISPLAYS_UNIT;
                windows_[i].root_->scale_.y = Settings::application.windows[i+1].h * 0.5f * DISPLAYS_UNIT;
                windows_[i].root_->translation_.x = Settings::application.windows[i+1].x * DISPLAYS_UNIT + windows_[i].root_->scale_.x;
                windows_[i].root_->translation_.y = -Settings::application.windows[i+1].y * DISPLAYS_UNIT - windows_[i].root_->scale_.y;
                windows_[i].title_->scale_.y = WINDOW_TITLEBAR_HEIGHT / windows_[i].root_->scale_.y;
                windows_[i].title_->translation_.y = 1.f + windows_[i].title_->scale_.y;
            }

        }
    }

    // main call to draw the view
    View::draw();

    // Display grid in overlay
    if (grid->active() && current_action_ongoing_) {
        const glm::mat4 projection = Rendering::manager().Projection();
        DrawVisitor draw_grid(grid->root(), projection, true);
        scene.accept(draw_grid);
    }

    // display interface
    // Locate window at upper left corner
    glm::vec2 P(0.0f, 0.01f);
    P = Rendering::manager().project(glm::vec3(P, 0.f), scene.root()->transform_,
                        Settings::application.windows[0].fullscreen);
    // Set window position depending on icons size
    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
    ImGui::SetNextWindowPos(ImVec2(P.x, P.y - 2.f * ImGui::GetFrameHeight() ), ImGuiCond_Always);
    if (ImGui::Begin("##DisplaysMaskOptions", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground
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

        // Disable output
        ImGuiToolkit::ButtonToggle(ICON_FA_EYE_SLASH, &Settings::application.render.disabled);
        if (ImGui::IsItemHovered())
            ImGuiToolkit::ToolTip(MENU_OUTPUTDISABLE, SHORTCUT_OUTPUTDISABLE);

        // Add / Remove windows
        ImGui::SameLine();
        if ( Settings::application.num_output_windows < MAX_OUTPUT_WINDOW) {
            if (ImGuiToolkit::IconButton(18, 4, "More windows")) {
                ++Settings::application.num_output_windows;
                current_window_ = Settings::application.num_output_windows-1;
                windows_[current_window_].need_update_ += 2;
            }
        }
        else
            ImGuiToolkit::Icon(18, 4, false);
        ImGui::SameLine();
        if ( Settings::application.num_output_windows > 0 ) {
            if (ImGuiToolkit::IconButton(19, 4, "Less windows")) {
                --Settings::application.num_output_windows;
                current_window_ = -1;
            }
        }
        else
            ImGuiToolkit::Icon(19, 4, false);

        // Modify current window
        if (current_window_ > -1) {

            ImGuiContext& g = *GImGui;

            // title output
            ImGui::SameLine(0, 5.f * g.Style.FramePadding.x);
            ImGui::Text("Output %d", current_window_ + 1);

            // Output options
            ImGui::SameLine(0, 2.f * g.Style.FramePadding.x);
            if ( ImGuiToolkit::ButtonIconToggle(9,5, &Settings::application.windows[1+current_window_].custom, "Custom fit") )
                ++windows_[current_window_].need_update_;

            ImGui::SameLine(0, g.Style.FramePadding.x);
            if ( ImGuiToolkit::ButtonIconToggle(11,1, &Settings::application.windows[1+current_window_].show_pattern, "Test pattern") )
                ++windows_[current_window_].need_update_;

            ImGui::SameLine(0, 1.5f * g.Style.FramePadding.x);
            ImGuiToolkit::PushFont(ImGuiToolkit::FONT_DEFAULT);
            ImGuiWindow* window = ImGui::GetCurrentWindow();
            window->DC.CursorPos.y += g.Style.FramePadding.y; // hack to re-align color button to text

            if (ImGui::ColorButton("White balance", ImVec4(Settings::application.windows[1+current_window_].whitebalance.x,
                                                           Settings::application.windows[1+current_window_].whitebalance.y,
                                                           Settings::application.windows[1+current_window_].whitebalance.z, 1.f),
                                   ImGuiColorEditFlags_NoAlpha)) {
                if ( DialogToolkit::ColorPickerDialog::instance().busy()) {
                    Log::Warning("Close previously openned color picker.");
                }
                else {
                    // prepare the color picker to start with white balance color
                    DialogToolkit::ColorPickerDialog::instance().setRGB( std::make_tuple(Settings::application.windows[1+current_window_].whitebalance.x,
                                                           Settings::application.windows[1+current_window_].whitebalance.y,
                                                           Settings::application.windows[1+current_window_].whitebalance.z) );
                    // declare function to be called
                    int w = 1+current_window_;
                    auto applyColor = [w](std::tuple<float, float, float> c)  {
                        Settings::application.windows[w].whitebalance.x = std::get<0>(c);
                        Settings::application.windows[w].whitebalance.y = std::get<1>(c);
                        Settings::application.windows[w].whitebalance.z = std::get<2>(c);
                    };

                    // open dialog (starts a thread that will call the 'applyColor' function
                    DialogToolkit::ColorPickerDialog::instance().open( applyColor );
                }
            }
            ImGui::PopFont();

            // White ballance temperature
            ImGui::SameLine();
            window->DC.CursorPos.y -= g.Style.FramePadding.y;
            if (ImGui::Button(ICON_FA_SLIDERS_H ICON_FA_SORT_DOWN ))
                ImGui::OpenPopup("adjustments_popup");
            ImGuiToolkit::PushFont(ImGuiToolkit::FONT_DEFAULT);
            if (ImGui::BeginPopup("adjustments_popup", ImGuiWindowFlags_NoMove))  {

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
                ImGui::VSliderFloat("##Temperatureslider", ImVec2(30,260), &Settings::application.windows[1+current_window_].whitebalance.w, 0.0, 1.0, "");
                if (ImGui::IsItemHovered() || ImGui::IsItemActive() )  {
                    ImGui::BeginTooltip();
                    ImGui::Text("%d K", 4000 + (int) ceil(Settings::application.windows[1+current_window_].whitebalance.w * 5000.f));
                    ImGui::EndTooltip();
                }
                // Slider Contrast
                ImGui::SameLine(0, IMGUI_SAME_LINE);
                ImGui::VSliderFloat("##contrastslider", ImVec2(30,260), &Settings::application.windows[1+current_window_].contrast, -0.5, 0.5, "");
                if (ImGui::IsItemHovered() || ImGui::IsItemActive() )  {
                    ImGui::BeginTooltip();
                    ImGui::Text("%.1f %%", (100.f * Settings::application.windows[1+current_window_].contrast));
                    ImGui::EndTooltip();
                }
                // Slider Brightness
                ImGui::SameLine(0, IMGUI_SAME_LINE);
                ImGui::VSliderFloat("##brightnessslider", ImVec2(30,260), &Settings::application.windows[1+current_window_].brightness, -0.5, 0.5, "");
                if (ImGui::IsItemHovered() || ImGui::IsItemActive() )  {
                    ImGui::BeginTooltip();
                    ImGui::Text("%.1f %%", (100.f * Settings::application.windows[1+current_window_].brightness));
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

                ImGui::EndPopup();
            }
            ImGui::PopFont();
        }

        ImGui::PopStyleColor(8);
        ImGui::End();
    }
    ImGui::PopFont();

    // display popup menu
    if (show_window_menu_ && current_window_ > -1) {
        ImGui::OpenPopup( "DisplaysOutputContextMenu" );
        show_window_menu_ = false;
    }
    if (ImGui::BeginPopup("DisplaysOutputContextMenu")) {

        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(COLOR_MENU_HOVERED, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(COLOR_WINDOW, 1.f));

        // FULLSCREEN selection: list of monitors
        int index = 1;
        std::map<std::string, glm::ivec4> _monitors = Rendering::manager().monitors();
        for (auto monitor_iter = _monitors.begin();
             monitor_iter != _monitors.end(); ++monitor_iter, ++index) {

            bool _fullscreen = Settings::application.windows[current_window_+1].fullscreen &&
                    Settings::application.windows[current_window_+1].monitor == monitor_iter->first;
            std::string menutext = std::string( ICON_FA_TV "  Fullscreen on Display ") + std::to_string(index);
            if (ImGui::MenuItem( menutext.c_str(), nullptr, _fullscreen )){
                windows_[current_window_].monitor_ = monitor_iter->first;
                Rendering::manager().outputWindow(current_window_).setFullscreen( windows_[current_window_].monitor_ );
                windows_[current_window_].need_update_ += 2;
            }
        }

        // WINDOW mode : set size
        bool _windowed = !Settings::application.windows[current_window_+1].fullscreen;
        if (ImGui::MenuItem( ICON_FA_WINDOW_MAXIMIZE "   Window", nullptr, &_windowed)){
            Rendering::manager().outputWindow(current_window_).exitFullscreen();
            // not fullscreen on a monitor
            windows_[current_window_].monitor_ = "";
            windows_[current_window_].need_update_ += 2;
        }
        ImGui::Separator();

        bool _borderless = !Settings::application.windows[current_window_+1].decorated;
        if (ImGui::MenuItem( ICON_FA_SQUARE_FULL "   Borderless", nullptr, &_borderless, _windowed)){
            Rendering::manager().outputWindow(current_window_).setDecoration(!_borderless);
            ++windows_[current_window_].need_update_;
        }

        if (ImGui::MenuItem( ICON_FA_EXPAND "   Fit all Displays", nullptr, false, _windowed )){
            Rendering::manager().outputWindow(current_window_).setDecoration(false);
            glm::ivec4 rect (INT_MAX, INT_MAX, 0, 0);
            std::map<std::string, glm::ivec4> _monitors = Rendering::manager().monitors();
            for (auto monitor_iter = _monitors.begin();
                 monitor_iter != _monitors.end(); ++monitor_iter) {
                rect.x = MIN(rect.x, monitor_iter->second.x);
                rect.y = MIN(rect.y, monitor_iter->second.y);
                rect.p = MAX(rect.p, monitor_iter->second.x+monitor_iter->second.p);
                rect.q = MAX(rect.q, monitor_iter->second.y+monitor_iter->second.q);
            }
            Rendering::manager().outputWindow(current_window_).setCoordinates( rect );
            windows_[current_window_].need_update_ += 2;
        }

        if (ImGui::MenuItem( ICON_FA_EXPAND_ALT "   Restore aspect ratio" , nullptr, false, _windowed )){
            // reset aspect ratio
            glm::ivec4 rect = windowCoordinates(current_window_);
            float ar = Mixer::manager().session()->frame()->aspectRatio();
            if ( rect.p / rect.q > ar)
                rect.p = ar * rect.q;
            else
                rect.q = rect.p / ar;
            Rendering::manager().outputWindow(current_window_).setCoordinates( rect );
            windows_[current_window_].need_update_ += 2;
        }

        if (ImGui::MenuItem( ICON_FA_RULER_COMBINED "   Rescale to pixel size", nullptr, false, _windowed )){
            // reset resolution to 1:1
            glm::ivec4 rect = windowCoordinates(current_window_);
            rect.p = Mixer::manager().session()->frame()->width();
            rect.q = Mixer::manager().session()->frame()->height();
            Rendering::manager().outputWindow(current_window_).setCoordinates( rect );
            windows_[current_window_].need_update_ += 2;
        }

        ImGui::Separator();
        if ( ImGui::MenuItem( ICON_FA_WINDOW_RESTORE  "   Reset shape") ) {
            glm::ivec4 rect (0, 0, 800, 600);
            rect.p = Mixer::manager().session()->frame()->width();
            rect.q = Mixer::manager().session()->frame()->height();
            Settings::application.windows[1+current_window_].show_pattern = false;
            Settings::application.windows[1+current_window_].custom = false;
            if (Settings::application.windows[current_window_+1].fullscreen)
                Rendering::manager().outputWindow(current_window_).exitFullscreen();
            else
                Rendering::manager().outputWindow(current_window_).setCoordinates( rect );
            Rendering::manager().outputWindow(current_window_).setDecoration(true);
            windows_[current_window_].need_update_ += 2;
        }
        if ( ImGui::MenuItem( ICON_FA_TINT_SLASH "  Reset white balance") ) {
            Settings::application.windows[1+current_window_].whitebalance.x = 1.f;
            Settings::application.windows[1+current_window_].whitebalance.y = 1.f;
            Settings::application.windows[1+current_window_].whitebalance.z = 1.f;
            windows_[current_window_].need_update_ += 2;
        }
        if ( ImGui::MenuItem( ICON_FA_SLIDERS_H "   Reset color corrections") ) {
            Settings::application.windows[1+current_window_].whitebalance.w = 0.5f;
            Settings::application.windows[1+current_window_].brightness = 0.f;
            Settings::application.windows[1+current_window_].contrast = 0.f;
            windows_[current_window_].need_update_ += 2;
        }
        if ( Settings::application.windows[current_window_+1].custom ) {
            ImGui::PopStyleColor(1);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(COLOR_FRAME, 1.f));
            if ( ImGui::MenuItem( ICON_FA_VECTOR_SQUARE  "   Reset custom fit") )
                Settings::application.windows[current_window_+1].nodes = glm::zero<glm::mat4>();
            windows_[current_window_].need_update_ += 2;
        }

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

    // test all windows
    current_window_ = -1;

    for (int i = 0; i < Settings::application.num_output_windows; ++i) {

        // ignore pick on title: it's the same as output surface
        if (pick.first == windows_[i].title_ ||
                pick.first == windows_[i].fullscreen_ )
            pick.first = windows_[i].surface_;

        // detect clic on menu
        if (pick.first == windows_[i].menu_)
            show_window_menu_ = true;

        // activate / deactivate window if clic on any element of it
        if ((pick.first == windows_[i].surface_) ||
            (pick.first == windows_[i].resize_) ||
            (pick.first == windows_[i].menu_) ) {
            current_window_ = i;
            adaptGridToWindow();
        }

        if ((pick.first == windows_[i].output_handles_[0]) ||
            (pick.first == windows_[i].output_handles_[1]) ||
            (pick.first == windows_[i].output_handles_[2]) ||
            (pick.first == windows_[i].output_handles_[3]) ) {
            current_window_ = i;
            adaptGridToWindow(current_window_);
        }
    }

    // ignore anything else than selected window
    if (current_window_ < 0)
        pick.first = nullptr;

    // request update
    for (int i = 0; i < MAX_OUTPUT_WINDOW; ++i)
        ++windows_[i].need_update_;

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
        // find which window was picked
        auto itp = pv.rbegin();
        for (; itp != pv.rend(); ++itp){
            // search for WindowPreview
            auto w = std::find_if(windows_.begin(), windows_.end(), WindowPreview::hasNode(itp->first));
            if (w != windows_.end()) {
                // set current
                current_window_ = (int) std::distance(windows_.begin(), w);
                ++windows_[current_window_].need_update_;
            }
        }
    }
}

void DisplaysView::initiate()
{
    // initiate pending action
    if (!current_action_ongoing_ && current_window_ > -1) {

        // store status current window
        // & make sure matrix transform of stored status is updated
        current_window_status_->copyTransform(windows_[current_window_].root_);
        current_window_status_->update(0.f);

        // store status current output frame in current window
        current_output_status_->copyTransform(windows_[current_window_].output_group_);
        current_output_status_->update(0.f);

        // initiated
        current_action_ = "";
        current_action_ongoing_ = true;
        ++windows_[current_window_].need_update_;
    }
}

void DisplaysView::terminate(bool force)
{
    // terminate pending action
    if (current_action_ongoing_ || force) {

        if (current_window_ > -1) {

            if (Settings::application.windows[current_window_+1].fullscreen) {
                // Apply change of fullscreen monitor
                if ( windows_[current_window_].monitor_.compare(Settings::application.windows[current_window_+1].monitor) != 0 )
                    Rendering::manager().outputWindow(current_window_).setFullscreen( windows_[current_window_].monitor_ );
            }
            else {
                // Apply coordinates to actual output window
                Rendering::manager().outputWindow(current_window_).setCoordinates( windowCoordinates(current_window_) );
            }

            // test if output area is inside the Window (with a margin of 10%)
            GlmToolkit::AxisAlignedBoundingBox _bb;
            _bb.extend(glm::vec3(-1.f, -1.f, 0.f));
            _bb.extend(glm::vec3(1.f, 1.f, 0.f));
            _bb = _bb.scaled(glm::vec3(0.9f));
            GlmToolkit::AxisAlignedBoundingBox output_bb;
            output_bb.extend(glm::vec3(-1.f, -1.f, 0.f)
                             + windows_[current_window_].output_handles_[0]->translation_);
            output_bb.extend(glm::vec3(-1.f, 1.f, 0.f)
                             + windows_[current_window_].output_handles_[1]->translation_);
            output_bb.extend(glm::vec3(1.f, -1.f, 0.f)
                             + windows_[current_window_].output_handles_[2]->translation_);
            output_bb.extend(glm::vec3(1.f, 1.f, 0.f)
                             + windows_[current_window_].output_handles_[3]->translation_);
            if (!_bb.intersect(output_bb) || output_bb.area() < 0.05f) {
                // No intersection of output bounding box with window area : revert to previous
                Settings::application.windows[current_window_ + 1].nodes = current_output_status_->data_;
                Log::Notify("Custom window output area outside window or too small");
            }

            // ensures update
            ++windows_[current_window_].need_update_;
        }

        // terminated
        current_action_ = "";
        current_action_ongoing_ = false;

        // prevent next draw
        draw_pending_ = true;
    }
}


glm::ivec4 DisplaysView::windowCoordinates(int index) const
{
    glm::ivec4 rect;

    rect.x = ceil( (windows_[index].root_->translation_.x - windows_[index].root_->scale_.x) / DISPLAYS_UNIT );
    rect.y = ceil( (windows_[index].root_->translation_.y + windows_[index].root_->scale_.y) / - DISPLAYS_UNIT );
    rect.p = 2.f * windows_[index].root_->scale_.x / DISPLAYS_UNIT;
    rect.q = 2.f * windows_[index].root_->scale_.y / DISPLAYS_UNIT;

    return rect;
}

std::string DisplaysView::fullscreenMonitor(int index) const
{
    return windows_[index].monitor_;
}

View::Cursor DisplaysView::grab (Source *, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2> pick)
{
    std::ostringstream info;
    View::Cursor ret = Cursor();

    // grab coordinates in scene-View reference frame
    glm::vec3 scene_from = Rendering::manager().unProject(from, scene.root()->transform_);
    glm::vec3 scene_to   = Rendering::manager().unProject(to, scene.root()->transform_);
    glm::vec3 scene_translation = scene_to - scene_from;

    // a window is currently selected
    if (current_window_ > -1) {

        // which window is grabbed ?
        Group *w = windows_[current_window_].root_;

        // which handle is grabbed ?
        size_t picked_handle_ = 4;
        if (pick.first == windows_[current_window_].output_handles_[0])
            picked_handle_ = 0;
        else if (pick.first == windows_[current_window_].output_handles_[1])
            picked_handle_ = 1;
        else if (pick.first == windows_[current_window_].output_handles_[2])
            picked_handle_ = 2;
        else if (pick.first == windows_[current_window_].output_handles_[3])
            picked_handle_ = 3;

        // Grab handles of the output frame to adjust
        if (picked_handle_ < 4) {
            // which corner was picked ?
            glm::vec2 corner = glm::round(pick.second);
            // transform from center to corner
            glm::mat4 T = GlmToolkit::transform(glm::vec3(corner.x, corner.y, 0.f), glm::vec3(0.f, 0.f, 0.f),
                                                glm::vec3(1.f, 1.f, 1.f));
            glm::mat4 root_to_corner_transform = T * glm::inverse(current_output_status_->transform_);
            glm::mat4 corner_to_root_transform = glm::inverse(root_to_corner_transform);
            // transformation from scene to corner:
            glm::mat4 scene_to_corner_transform = root_to_corner_transform * glm::inverse( current_window_status_->transform_);
            // compute cursor movement in corner reference frame
            glm::vec4 corner_from = scene_to_corner_transform * glm::vec4( scene_from,  1.f );
            glm::vec4 corner_to   = scene_to_corner_transform * glm::vec4( scene_to,  1.f );
            // get stored status
            glm::vec3 node_pos = glm::vec3(current_output_status_->data_[picked_handle_].x,
                                           current_output_status_->data_[picked_handle_].y,
                                           0.f);
            // Compute target coordinates of manipulated handle into CORNER reference frame
            node_pos = root_to_corner_transform * glm::vec4(node_pos, 1.f);
            // apply translation of target in CORNER
            node_pos = glm::translate(glm::identity<glm::mat4>(),  glm::vec3(corner_to - corner_from)) * glm::vec4(node_pos, 1.f);
            // snap handle coordinates to grid (if active)
            if ( grid->active() )
                node_pos = grid->snap(node_pos);
            // Diagonal SCALING with SHIFT
            if (UserInterface::manager().shiftModifier())
                node_pos.y = (corner.x * corner.y) * node_pos.x;
            // Compute handle coordinates back in ROOT reference frame
            node_pos = corner_to_root_transform * glm::vec4(node_pos, 1.f);

            // apply to output
            Settings::application.windows[current_window_+1].nodes[picked_handle_].x = node_pos.x;
            Settings::application.windows[current_window_+1].nodes[picked_handle_].y = node_pos.y;

            // show cursor hand
            ret.type = Cursor_Hand;
            // show info depending on corner picked
            if (picked_handle_ == 0)
                info << "Bottom-left";
            else if (picked_handle_ == 1)
                info << "Top-left";
            else if (picked_handle_ == 3)
                info << "Top-right";
            else
                info << "Bottom-right";
            node_pos.x *= (float) Settings::application.windows[current_window_ + 1].w;
            node_pos.y *= -1.f * (float) Settings::application.windows[current_window_ + 1].h;
            info << " +(" << std::fixed << std::setprecision(0) << node_pos.x << "," << node_pos.y << ")";
        }

        // grab window not fullscreen : move or resizes
        if (!Settings::application.windows[current_window_+1].fullscreen) {

            // grab surface to move
            if ( pick.first == windows_[current_window_].surface_ ){

                // apply translation
                w->translation_ = current_window_status_->translation_ + scene_translation;

                // snap coordinates to grid (if active)
                if (grid->active()) {
                    // get top left corner
                    glm::vec2 sc = glm::vec2(w->scale_) * glm::vec2(1.f, -1.f);
                    glm::vec2 top_left = glm::vec2(w->translation_) - sc;
                    top_left -= glm::vec2(gridroot_->translation_);
                    // snap to grid
                    top_left = grid->snap(top_left / glm::vec2(gridroot_->scale_))
                         * glm::vec2(gridroot_->scale_);
                    top_left += glm::vec2(gridroot_->translation_);
                    // revert to center coordinates
                    w->translation_ = glm::vec3(top_left, 0.f) + glm::vec3(sc, 0.f);
                }

                // Show move cursor
                ret.type = Cursor_ResizeAll;
                glm::ivec4 r = windowCoordinates(current_window_);
                info << "Window position " << r.x << ", " << r.y << " px";
            }
            // grab handle to resize
            else if ( pick.first == windows_[current_window_].resize_ ){

                // which corner was picked ?
                glm::vec2 corner = glm::round(pick.second);

                // transform from source center to corner
                glm::mat4 T = GlmToolkit::transform(glm::vec3(corner.x, corner.y, 0.f), glm::vec3(0.f, 0.f, 0.f),
                                                    glm::vec3(1.f, 1.f, 1.f));

                // transformation from scene to corner:
                glm::mat4 scene_to_corner_transform = T * glm::inverse(current_window_status_->transform_);
                glm::mat4 corner_to_scene_transform = glm::inverse(scene_to_corner_transform);

                // compute cursor movement in corner reference frame
                glm::vec4 corner_from = scene_to_corner_transform * glm::vec4( scene_from,  1.f );
                glm::vec4 corner_to = scene_to_corner_transform * glm::vec4(scene_to, 1.f);


                // operation of scaling in corner reference frame
                glm::vec3 corner_scaling = glm::vec3(corner_to) / glm::vec3(corner_from);
                glm::ivec4 rect;

                // RESIZE CORNER
                // proportional SCALING with SHIFT
                if (UserInterface::manager().shiftModifier()) {
                    // calculate proportional scaling factor
                    float factor = glm::length(glm::vec2(corner_to)) / glm::length(glm::vec2(corner_from));
                    w->scale_ = current_window_status_->scale_ * glm::vec3(factor, factor, 1.f);
                }
                // non-proportional CORNER RESIZE  (normal case)
                else {
                    w->scale_ = current_window_status_->scale_ * corner_scaling;
                }

                // update corner scaling to apply to center coordinates
                corner_scaling = w->scale_ / current_window_status_->scale_;

                // TRANSLATION CORNER
                // convert source position in corner reference frame
                glm::vec4 center = scene_to_corner_transform * glm::vec4( current_window_status_->translation_, 1.f);
                // transform source center (in corner reference frame)
                center = glm::scale(glm::identity<glm::mat4>(), corner_scaling) * center;
                // convert center back into scene reference frame
                center = corner_to_scene_transform * center;
                // apply to node
                w->translation_ = glm::vec3(center);

                // snap coordinates to grid (if active)
                if (grid->active()) {
                    // get bottom right corner
                    glm::vec2 sc = glm::vec2(w->scale_) * glm::vec2(1.f, -1.f);
                    glm::vec2 bottom_right = glm::vec2(w->translation_) + sc;
                    bottom_right -= glm::vec2(gridroot_->translation_);
                    // snap corner to grid
                    bottom_right = grid->snap(bottom_right / glm::vec2(gridroot_->scale_))
                                   * glm::vec2(gridroot_->scale_);
                    bottom_right += glm::vec2(gridroot_->translation_);
                    // recalculate center coordinates and scale
                    sc = glm::vec2(current_window_status_->scale_) * glm::vec2(1.f, -1.f);
                    glm::vec2 top_left = glm::vec2(current_window_status_->translation_) - sc;
                    glm::vec2 middle   = top_left + (bottom_right - top_left) * 0.5f;
                    w->translation_ = glm::vec3(middle, 0.f);
                    w->scale_ = glm::vec3((bottom_right - top_left) * glm::vec2(0.5f, -0.5f), 1.f);
                }

                // rescale title bar
                windows_[current_window_].title_->scale_.y = WINDOW_TITLEBAR_HEIGHT / w->scale_.y;
                windows_[current_window_].title_->translation_.y = 1.f + windows_[current_window_].title_->scale_.y;

                // show cursor
                ret.type = Cursor_ResizeNWSE;
                rect = windowCoordinates(current_window_);
                info << "Window size " << rect.p << " x "  << rect.q << " px";
            }
        }
        // grab fullscreen window : change monitor
        else {

            if ( pick.first == windows_[current_window_].surface_ ){

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
                        w->scale_.x = r.p * 0.5f * DISPLAYS_UNIT;
                        w->scale_.y = r.q * 0.5f * DISPLAYS_UNIT;
                        w->translation_.x = r.x * DISPLAYS_UNIT + w->scale_.x;
                        w->translation_.y = -r.y * DISPLAYS_UNIT - w->scale_.y;

                        // remember the output monitor selected
                        windows_[current_window_].monitor_ = monitor_iter->first;

                        // Show  cursor
                        ret.type = Cursor_Hand;
                        info << "Fullscreen Monitor " << index << " (" << windows_[current_window_].monitor_ << ")\n   "<<
                                r.p << " x "  << r.q << " px";
                    }
                }
            }
        }

        // request update
        ++windows_[current_window_].need_update_;
    }

    // update cursor
    ret.info = info.str();

    return ret;
}

//View::Cursor DisplaysView::over (glm::vec2 pos, bool)
//{
//    output_zoomarea_->visible_ = false;

//    if ( display_action_ == 2 ) {

//        // utility
//        bool _over_render = false;
//        glm::vec2 _uv_cursor_center(0.f, 0.f);
//        glm::vec3 scene_pos = Rendering::manager().unProject(pos);

//        // find if the output_render surface is under the cursor at scene_pos
//        _over_render = set_UV_output_render_from_pick(scene_pos, &_uv_cursor_center);

//        // set coordinates of zoom area indicator under cursor at position 'pos'
//        output_zoomarea_->translation_ = glm::inverse(scene.root()->transform_) * glm::vec4(scene_pos, 1.f);
//        output_zoomarea_->visible_ = _over_render;
//        output_zoomarea_->scale_ = glm::vec3(output_scale * output_->scale_.x, output_scale * output_->scale_.x, 1.f);

//        // mouse cursor is over the rendering surface
//        if (_over_render) {

//            // get the imgui screen area to render the zoom window into
//            const ImGuiIO& io = ImGui::GetIO();
//            const ImVec2 margin(100.f, 50.f);
//            ImVec2 screen = io.DisplaySize - margin * 2.f;
//            ImVec2 win_pos = margin;
//            ImVec2 win_size = screen * 0.5f;

//            // vimix window is horizontal
//            if ( screen.x > screen.y){
//                // set zoom window to square
//                win_size.y = win_size.x;
//                // show the zoom window left and right of window
//                win_pos.y += (screen.y - win_size.y) * 0.5f;
//                win_pos.x += (pos.x > io.DisplaySize.x * 0.5f) ? 0 : win_size.x;
//            }
//            // vimix window is vertical
//            else {
//                // set zoom window to square
//                win_size.x = win_size.y;
//                // show the zoom window top and bottom of window
//                win_pos.x += (screen.x - win_size.x) * 0.5f;
//                win_pos.y += (pos.y > io.DisplaySize.y * 0.5f) ? 0 : win_size.y;
//            }

//            // DRAW window without decoration, 100% opaque, with 2 pixels on the border
//            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.f);
//            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
//            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2.f, 2.f));

//            ImGui::SetNextWindowPos(win_pos, ImGuiCond_Always);
//            ImGui::SetNextWindowSize(win_size, ImGuiCond_Always);
//            if (ImGui::Begin("##DisplaysMacro", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration
//                             | ImGuiWindowFlags_NoSavedSettings ))
//            {
//                ImVec2 _zoom(zoom_factor,  zoom_factor * output_ar);
//                ImVec2 _uv1( _uv_cursor_center.x - _zoom.x * 0.5f, _uv_cursor_center.y - _zoom.y * 0.5f);
//                _uv1 = ImClamp( _uv1, ImVec2(0.f, 0.f), ImVec2(1.f, 1.f));

//                ImVec2 rect = _uv1 + _zoom;
//                ImVec2 _uv2 = ImClamp( rect, ImVec2(0.f, 0.f), ImVec2(1.f, 1.f));
//                _uv1 = _uv2 - _zoom;


//                ImGui::Image((void*)(intptr_t) output_render_->textureIndex(), win_size, _uv1, _uv2);

//                ImGui::End();
//            }
//            ImGui::PopStyleVar(3);

//        }

//    }

//    return Cursor();
//}

bool DisplaysView::doubleclic (glm::vec2 P)
{
    // bring window forward
    if ( pick(P).first != nullptr) {
        Rendering::manager().outputWindow(current_window_).show();
        return true;
    }

    return false;
}

#define TIME_STEP 500

void DisplaysView::arrow (glm::vec2 movement)
{
    static uint _time = 0;
    static glm::vec2 _from(0.f);
    static glm::vec2 _displacement(0.f);

    // grab only works on current window if not fullscreen
    if (current_window_ > -1 && !Settings::application.windows[current_window_+1].fullscreen) {

        // first time pressed: initialize and initiate
        if (!current_action_ongoing_) {
            if (UserInterface::manager().altModifier() || Settings::application.mouse_pointer_lock)
                MousePointer::manager().setActiveMode(Pointer::POINTER_GRID);
            else
                MousePointer::manager().setActiveMode(Pointer::POINTER_DEFAULT);

            // reset
            _time = 0;
            _displacement = glm::vec2(0.f);

            // initiate view action and store status
            initiate();

            // get coordinates of window and set this as start of mouse position
            _from = glm::vec2(
                Rendering::manager().project(windows_[current_window_].root_->translation_,
                                             scene.root()->transform_));
            // Initiate mouse pointer action
            MousePointer::manager().active()->initiate(_from);
        }

        // if initialized
        if (current_action_ongoing_) {
            // move on first press, and then every TIME_STEP milisecond
            if (_time < 1 || _time > TIME_STEP) {
                _time = 0;

                // move by step size if grid is active
                if (MousePointer::manager().activeMode() == Pointer::POINTER_GRID) {
                    // calculate step size in monitor coordinates (simulate mouse movement)
                    glm::vec2 step = grid->step() * glm::vec2(gridroot_->scale_);
                    step  = glm::vec2(Rendering::manager().project(glm::vec3(step.x, -step.y, 0.f),
                                                                  scene.root()->transform_));
                    step -= glm::vec2(Rendering::manager().project(glm::vec3(0.f),
                                                                   scene.root()->transform_));
                    // multiply movement by step size
                    movement *= step;
                }

                // increment displacement by movement
                _displacement += movement;

                // update mouse pointer action
                MousePointer::manager().active()->update(_from + _displacement, dt_ / 1000.f);

                // simulate mouse grab
                grab(nullptr, _from, MousePointer::manager().active()->target(),
                     std::make_pair(windows_[current_window_].surface_, glm::vec2(0.f) ) );
            }
            // draw mouse pointer effect
            MousePointer::manager().active()->draw();
            // increment time counter
            _time += static_cast<uint>(dt_);
        }
    }
    else {
        // reset
        _from = glm::vec2(0.f);
        _displacement = glm::vec2(0.f);
        terminate(true);
    }
}


bool WindowPreview::hasNode::operator()(const WindowPreview &elem) const
{
    if (_n)
    {
        if (_n == elem.fullscreen_ ||
            _n == elem.surface_ ||
            _n == elem.title_)
            return true;
    }

    return false;
}




//// from scene coordinates pos, get the UV of point in the output render
//// returns true if on output_render
//bool DisplaysView::get_UV_window_render_from_pick(const glm::vec3 &pos, glm::vec2 *uv)
//{
//    bool ret = false;

//    // perform picking at pos
//    PickingVisitor pv(pos);
//    scene.accept(pv);

//    // find if the output_render surface is at pos
//    for(std::vector< std::pair<Node *, glm::vec2> >::const_reverse_iterator p = pv.rbegin(); p != pv.rend(); ++p){
//        // if found the output rendering surface at pos
//        if ( p->first == window_render_) {
//            // set return value to true
//            ret = true;
//            // convert picking coordinates into UV
//            // This works only as we are sure that the output_render_ UV mapping is fixed
//           *uv = (p->second + glm::vec2(1.f, -1.f)) * glm::vec2(0.5f, -0.5f);
//           break;
//        }
//    }

//    return ret;
//}



// GLM conversion of GLSL code to generate white balance matrices

//// Parameters for transfer characteristics (gamma curves)
//struct transfer {
//    // Exponent used to linearize the signal
//    float power;

//    // Offset from 0.0 for the exponential curve
//    float off;

//    // Slope of linear segment near 0
//    float slope;

//    // Values below this are divided by slope during linearization
//    float cutoffToLinear;

//    // Values below this are multiplied by slope during gamma correction
//    float cutoffToGamma;

//    transfer(float p, float o, float s, float l, float g)  :
//        power(p), off(o), slope (s), cutoffToLinear(l), cutoffToGamma(g)
//    {}
//};

//// Parameters for a colorspace
//struct rgb_space {
//    // Chromaticity coordinates (xyz) for Red, Green, and Blue primaries
//    glm::mat4 primaries;

//    // Chromaticity coordinates (xyz) for white point
//    glm::vec4 white;

//    // Linearization and gamma correction parameters
//    transfer trc;

//    rgb_space(glm::mat4 p, glm::vec4 w, transfer t) :
//        primaries(p), white(w), trc(t)
//    {}
//};


//// Turns 6 chromaticity coordinates into a 3x3 matrix
//#define Primaries(r1, r2, g1, g2, b1, b2)\
//    glm::mat4(\
//    (r1), (r2), 1.0 - (r1) - (r2), 0,\
//    (g1), (g2), 1.0 - (g1) - (g2), 0,\
//    (b1), (b2), 1.0 - (b1) - (b2), 0,\
//    0, 0, 0, 1)

//// Creates a whitepoint's xyz chromaticity coordinates from the given xy coordinates
//#define White(x, y)\
//    glm::vec4( glm::vec3((x), (y), 1.0f - (x) - (y)) /(y), 1.0f)

//// Automatically calculate the slope and cutoffs for transfer characteristics
//#define Transfer(po, of)\
//transfer(\
//    (po),\
//    (of),\
//    (pow((po)*(of)/((po) - 1.0), 1.0 - (po))*pow(1.0 + (of), (po)))/(po),\
//    (of)/((po) - 1.0),\
//    (of)/((po) - 1.0)*(po)/(pow((po)*(of)/((po) - 1.0), 1.0 - (po))*pow(1.0 + (of), (po)))\
//)

//// Creates a scaling matrix using a vec4 to set the xyzw scalars
//#define diag(v)\
//    glm::mat4(\
//    (v).x, 0, 0, 0,\
//    0, (v).y, 0, 0,\
//    0, 0, (v).z, 0,\
//    0, 0, 0, (v).w)

//// Creates a conversion matrix that turns RGB colors into XYZ colors
//#define rgbToXyz(space)\
//    (space.primaries*diag(inverse((space).primaries)*(space).white))

//// Creates a conversion matrix that turns XYZ colors into RGB colors
//#define xyzToRgb(space)\
//    inverse(rgbToXyz(space))


///*
// * Chromaticities for RGB primaries
// */


//// Rec. 709 (HDTV) and sRGB primaries
//const glm::mat4 primaries709 = Primaries(
//    0.64f, 0.33f,
//    0.3f, 0.6f,
//    0.15f, 0.06f
//);


//// Same as above, but in fractional form
//const glm::mat4 primariesLms = Primaries(
//    194735469.0/263725741.0, 68990272.0/263725741.0,
//    141445123.0/106612934.0, -34832189.0/106612934.0,
//    36476327.0/229961670.0, 0.0
//);


///*
// * Chromaticities for white points
// */

//// Standard illuminant E (also known as the 'equal energy' white point)
//const glm::vec4 whiteE = White(1.f/3.f, 1.f/3.f);

//// Standard Illuminant D65 according to the Rec. 709 and sRGB standards
//const glm::vec4 whiteD65S = White(0.3127f, 0.3290f);

///*
// * Gamma curve parameters
// */
//// Linear gamma
//const transfer gam10 = transfer(1.0f, 0.0f, 1.0f, 0.0f, 0.0f);
//// Gamma for sRGB
//const transfer gamSrgb = transfer(2.4f, 0.055f, 12.92f, 0.04045f, 0.0031308f);

///*
// * RGB Colorspaces
// */
//// sRGB (mostly the same as Rec. 709, but different gamma and full range values)
//const rgb_space Srgb = rgb_space(primaries709, whiteD65S, gamSrgb);

//// Lms primaries, balanced against equal energy white point
//const rgb_space LmsRgb = rgb_space(primariesLms, whiteE, gam10);


//const glm::mat4 toXyz = rgbToXyz(Srgb);
//const glm::mat4 toRgb = xyzToRgb(Srgb);
//const glm::mat4 toLms = xyzToRgb(LmsRgb);
//const glm::mat4 frLms = rgbToXyz(LmsRgb);

//std::cerr << "toXyz" << std::endl;
//std::cerr << glm::to_string(toXyz) <<std::endl;
//std::cerr << "toRgb" << std::endl;
//std::cerr << glm::to_string(toRgb) <<std::endl;
//std::cerr << "toLms" << std::endl;
//std::cerr << glm::to_string(toLms) <<std::endl;
//std::cerr << "frLms" << std::endl;
//std::cerr << glm::to_string(frLms) <<std::endl;
