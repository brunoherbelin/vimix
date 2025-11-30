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

#include "Source/SessionSource.h"
#include "imgui.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/vector_angle.hpp>

#include "Toolkit/ImGuiToolkit.h"

#include <algorithm>
#include <string>
#include <sstream>
#include <iomanip>

#include "Mixer.h"
#include "defines.h"
#include "Source/Source.h"
#include "Source/CloneSource.h"
#include "Source/SourceCallback.h"
#include "Settings.h"
#include "Scene/Decorations.h"
#include "UserInterfaceManager.h"
#include "Visitor/BoundingBoxVisitor.h"
#include "ActionManager.h"
#include "MousePointer.h"

#include "LayerView.h"

LayerView::LayerView() : View(LAYER), aspect_ratio(1.f)
{
    scene.root()->scale_ = glm::vec3(LAYER_DEFAULT_SCALE, LAYER_DEFAULT_SCALE, 1.0f);
    scene.root()->translation_ = glm::vec3(2.2f, 1.2f, 0.0f);
    // read default settings
    if ( Settings::application.views[mode_].name.empty() )
        // no settings found: store application default
        saveSettings();
    else
        restoreSettings();
    Settings::application.views[mode_].name = "Layers";

    // Geometry Scene background
    frame_ = new Group;
    Surface *rect = new Surface;
    rect->shader()->color.a = 0.3f;
    frame_->attach(rect);

    Frame *border = new Frame(Frame::ROUND, Frame::THIN, Frame::PERSPECTIVE);
    border->color = glm::vec4( COLOR_FRAME, 0.95f );
    frame_->attach(border);
    scene.bg()->attach(frame_);

    persp_left_ = new Mesh("mesh/perspective_axis_left.ply");
    persp_left_->shader()->color = glm::vec4( COLOR_FRAME_LIGHT, 1.f );
    persp_left_->scale_.x = LAYER_PERSPECTIVE;
    persp_left_->translation_.z = -0.1f;
    persp_left_->translation_.x = -1.f;
    scene.bg()->attach(persp_left_);

    persp_right_ = new Mesh("mesh/perspective_axis_right.ply");
    persp_right_->shader()->color = glm::vec4( COLOR_FRAME_LIGHT, 1.f );
    persp_right_->scale_.x = LAYER_PERSPECTIVE;
    persp_right_->translation_.z = -0.1f;
    persp_right_->translation_.x = 1.f;
    scene.bg()->attach(persp_right_);

    // replace grid with appropriate one
    if (grid) delete grid;
    grid = new LayerGrid(scene.root());
}


void LayerView::draw()
{
    // Display grid
    grid->root()->visible_ = (grid->active() && current_action_ongoing_);

    View::draw();

    // initialize the verification of the selection
    static bool candidate_flatten_group = false;

    // display popup menu source
    if (show_context_menu_ == MENU_SOURCE) {
        ImGui::OpenPopup("LayerSourceContextMenu");
        show_context_menu_ = MENU_NONE;
    }
    if (ImGui::BeginPopup("LayerSourceContextMenu")) {
        // work on the current source
        Source *s = Mixer::manager().currentSource();
        if (s != nullptr && !s->failed() ) {

            if (ImGuiToolkit::BeginMenuIcon( 5, 6, "Blending" )) {
                for (auto bmode = Shader::blendingFunction.cbegin();
                    bmode != Shader::blendingFunction.cend();
                    ++bmode) {
                    int index = bmode - Shader::blendingFunction.cbegin();
                    if (ImGuiToolkit::MenuItemIcon(std::get<0>(*bmode),
                                                std::get<1>(*bmode),
                                                std::get<2>(*bmode).c_str(),
                                                nullptr,
                                                s->blendingShader()->blending == index)) {
                        s->blendingShader()->blending = Shader::BlendMode(index);
                        s->touch();
                        Action::manager().store(s->name() + ": Blending " + std::get<2>(*bmode));
                    }
                }
                ImGui::EndMenu();
            }

            if (s->icon() == glm::ivec2(ICON_SOURCE_GROUP) )
            {
                if (ImGuiToolkit::SelectableIcon( 7, 2, "Uncover bundle ", false )) {
                    Mixer::manager().import( dynamic_cast<SessionSource*>(s) );
                }
            }
            else {
                if ( s->cloned() || s->icon() == glm::ivec2(ICON_SOURCE_CLONE)){
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6, 0.6, 0.6, 0.9f));
                    ImGuiToolkit::SelectableIcon( 11, 2, "Bundle source", false );
                    ImGui::PopStyleColor(); 
                    if (ImGui::IsItemHovered()) {
                        ImGuiToolkit::ToolTip("Cannot create bundle; clones "
                            " cannot be separated from their origin source.");
                    }
                }
                else {
                    if (ImGuiToolkit::SelectableIcon( 11, 2, "Bundle source ", false )) {
                        Mixer::manager().groupCurrent();
                    }
                }
            }
        }

        ImGui::EndPopup();
    }

    // display popup menu selection
    if (show_context_menu_ == MENU_SELECTION) {

        // initialize the verification of the selection
        candidate_flatten_group = Mixer::manager().selectionCanBeGroupped();

        ImGui::OpenPopup( "LayerSelectionContextMenu" );
        show_context_menu_ = MENU_NONE;
    }
    if (ImGui::BeginPopup("LayerSelectionContextMenu")) {

        // colored context menu
        ImGui::PushStyleColor(ImGuiCol_Text, ImGuiToolkit::HighlightColor());
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(COLOR_MENU_HOVERED, 0.5f));

        // Blending all selection
        if (ImGuiToolkit::BeginMenuIcon( 5, 6, "Blending" )) {
            for (auto bmode = Shader::blendingFunction.cbegin();
                 bmode != Shader::blendingFunction.cend();
                 ++bmode) {
                int index = bmode - Shader::blendingFunction.cbegin();
                if (ImGuiToolkit::MenuItemIcon(std::get<0>(*bmode),
                                               std::get<1>(*bmode),
                                               std::get<2>(*bmode).c_str())) {
                    SourceList dsl = depth_sorted(Mixer::selection().getCopy());
                    for (SourceList::iterator  it = dsl.begin(); it != dsl.end(); ++it) {
                        (*it)->blendingShader()->blending = Shader::BlendMode(index);
                        (*it)->touch();
                    }
                    Action::manager().store(std::string("Blending selected " ICON_FA_LAYER_GROUP));
                }
            }
            ImGui::EndMenu();
        }

        // special action of Mixing view
        if (candidate_flatten_group){
            if (ImGuiToolkit::SelectableIcon( 11, 2, "Bundle selection", false )) {
                Mixer::manager().groupSelection();
            }
        }
        else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6, 0.6, 0.6, 0.9f));
            ImGuiToolkit::SelectableIcon( 11, 2, "Bundle selection", false );
            ImGui::PopStyleColor(); 
            if (ImGui::IsItemHovered()) {
                ImGuiToolkit::ToolTip("Cannot create bundle; selection must be "
                    "contiguous in layer, clones cannot be separated from their origin source.");
            }
        }

        ImGui::Separator();

        // manipulation of sources in Mixing view
        if (ImGui::Selectable( ICON_FA_GRIP_LINES_VERTICAL ICON_FA_GRIP_LINES_VERTICAL "  Distribute" )){
            SourceList dsl = depth_sorted(Mixer::selection().getCopy());
            SourceList::iterator  it = dsl.begin();
            float depth = (*it)->depth();
            float depth_inc   = (dsl.back()->depth() - depth) / static_cast<float>(Mixer::selection().size()-1);
            for (++it; it != dsl.end(); ++it) {
                depth += depth_inc;
                (*it)->call( new SetDepth(depth, 80.f) );
            }
            Action::manager().store(std::string("Distribute selected " ICON_FA_LAYER_GROUP));
        }
        if (ImGui::Selectable( ICON_FA_CARET_RIGHT ICON_FA_CARET_LEFT "   Compress" )){
            SourceList dsl = depth_sorted(Mixer::selection().getCopy());
            SourceList::iterator  it = dsl.begin();
            float depth = (*it)->depth();
            for (++it; it != dsl.end(); ++it) {
                depth += LAYER_STEP;
                (*it)->call( new SetDepth(depth, 80.f) );
            }
            Action::manager().store(std::string("Compress selected " ICON_FA_LAYER_GROUP));
        }
        if (ImGui::Selectable( ICON_FA_EXCHANGE_ALT "  Reverse order" )){
            SourceList dsl = depth_sorted(Mixer::selection().getCopy());
            SourceList::iterator  it = dsl.begin();
            SourceList::reverse_iterator  rit = dsl.rbegin();
            for (; it != dsl.end(); ++it, ++rit) {
                (*it)->call( new SetDepth((*rit)->depth(), 80.f) );
            }
            Action::manager().store(std::string("Reverse order selected " ICON_FA_LAYER_GROUP));
        }

        ImGui::PopStyleColor(2);
        ImGui::EndPopup();
    }
}


void LayerView::update(float dt)
{
    View::update(dt);

    // a more complete update is requested
    if (View::need_deep_update_ > 0) {

        // update rendering of render frame
        FrameBuffer *output = Mixer::manager().session()->frame();
        if (output){
            // correct with aspect ratio
            aspect_ratio = output->aspectRatio();
            frame_->scale_.x = aspect_ratio;
            persp_left_->translation_.x = -aspect_ratio;
            persp_right_->translation_.x = aspect_ratio + 0.06;
        }

        // prevent invalid scaling
        float s = CLAMP(scene.root()->scale_.x, LAYER_MIN_SCALE, LAYER_MAX_SCALE);
        scene.root()->scale_.x = s;
        scene.root()->scale_.y = s;

        // change grid color
        ImVec4 c = ImGuiToolkit::HighlightColor();
        grid->setColor( glm::vec4(c.x, c.y, c.z, 0.3) );
    }

    if (Mixer::manager().view() == this )
    {
        // update the selection overlay
        const ImVec4 c = ImGuiToolkit::HighlightColor();
        updateSelectionOverlay(glm::vec4(c.x, c.y, c.z, c.w));
    }

}

bool LayerView::canSelect(Source *s) {

    return ( View::canSelect(s) );
}

void LayerView::resize ( int scale )
{
    float z = CLAMP(0.01f * (float) scale, 0.f, 1.f);
    z *= z;
    z *= LAYER_MAX_SCALE - LAYER_MIN_SCALE;
    z += LAYER_MIN_SCALE;
    scene.root()->scale_.x = z;
    scene.root()->scale_.y = z;

    // Clamp translation to acceptable area
    glm::vec3 border(2.f, 1.f, 0.f);
    scene.root()->translation_ = glm::clamp(scene.root()->translation_, -border, border * 2.f);
}

int  LayerView::size ()
{
    float z = (scene.root()->scale_.x - LAYER_MIN_SCALE) / (LAYER_MAX_SCALE - LAYER_MIN_SCALE);
    return (int) ( sqrt(z) * 100.f);
}


std::pair<Node *, glm::vec2> LayerView::pick(glm::vec2 P)
{
    // get picking from generic View
    std::pair<Node *, glm::vec2> pick = View::pick(P);

    // deal with internal interactive objects
    if ( overlay_selection_icon_ != nullptr && pick.first == overlay_selection_icon_ ) {

        openContextMenu(MENU_SELECTION);
    }
    else {
        // get if a source was picked
        Source *s = Mixer::manager().findSource(pick.first);
        if (s != nullptr) {
            // pick on the lock icon; unlock source
            if ( UserInterface::manager().ctrlModifier() && pick.first == s->lock_) {
                lock(s, false);
                pick = { s->locker_, pick.second };
//                pick = { nullptr, glm::vec2(0.f) };
            }
            // pick on the open lock icon; lock source and cancel pick
            else if ( UserInterface::manager().ctrlModifier() && pick.first == s->unlock_ ) {
                lock(s, true);
                pick = { nullptr, glm::vec2(0.f) };
            }
            // pick a locked source; cancel pick
            else if ( !UserInterface::manager().ctrlModifier() && s->locked() ) {
                pick = { nullptr, glm::vec2(0.f) };
            }
            // pick the symbol: ask to show editor
            else if ( pick.first == s->symbol_ ) {
                UserInterface::manager().showSourceEditor(s);
            }
            // pick the initials: show in panel
            else if ( pick.first == s->initial_1_ ) {
                UserInterface::manager().showPannel(Mixer::manager().indexCurrentSource());
                UserInterface::manager().setSourceInPanel(s);
            }
            // pick blending icon
            else if (pick.first == s->blendmode_->activeChild()) {
                openContextMenu(MENU_SOURCE);
            }
        }
        else
            pick = { nullptr, glm::vec2(0.f) };
    }

    return pick;
}


float LayerView::setDepth(Source *s, float d)
{
    if (!s)
        return -1.f;

    // move the layer node of the source
    Group *sourceNode = s->group(mode_);

    float depth = d < 0.f ? sourceNode->translation_.z : d;

    // negative or no  depth given; find the front most depth
    if ( depth < 0.f ) {
        // default to place visible in front of background
        depth = LAYER_BACKGROUND + LAYER_STEP;

        // find the front-most souce in the workspace (behind FOREGROUND)
        for (NodeSet::iterator node = scene.ws()->begin(); node != scene.ws()->end(); ++node) {
            // discard foreground
            if ((*node)->translation_.z > LAYER_FOREGROUND )
                break;
            // place in front of previous sources
            depth = MAX(depth, (*node)->translation_.z + LAYER_STEP);
            // in case node is already at max depth
            if ((*node)->translation_.z + DELTA_DEPTH > MAX_DEPTH )
                (*node)->translation_.z -= DELTA_DEPTH;
        }
    }

    // change depth
    sourceNode->translation_.z = CLAMP( depth, MIN_DEPTH, MAX_DEPTH);

    // request reordering of scene at next update
    ++View::need_deep_update_;

    // request update of source
    s->touch();

    return sourceNode->translation_.z;
}

View::Cursor LayerView::grab (Source *s, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2>)
{
    if (!s)
        return Cursor();

    // unproject
    glm::vec3 gl_Position_from = Rendering::manager().unProject(from, scene.root()->transform_);
    glm::vec3 gl_Position_to   = Rendering::manager().unProject(to, scene.root()->transform_);

    // compute delta translation
    glm::vec3 dest_translation = s->stored_status_->translation_ + gl_Position_to - gl_Position_from;

    // snap to grid (polar)
    if (grid->active())
        dest_translation = grid->snap(dest_translation * 0.5f) * 2.f;

    // apply change
    float d = setDepth( s,  MAX( -dest_translation.x, 0.f) );

    //
    // grab all others in selection
    //
    // compute effective depth translation of current source s
    float dp = s->group(mode_)->translation_.z - s->stored_status_->translation_.z;
    // loop over selection
    for (auto it = Mixer::selection().begin(); it != Mixer::selection().end(); ++it) {
        if ( *it != s && !(*it)->locked() ) {
            // set depth and request update
            setDepth( *it, (*it)->stored_status_->translation_.z + dp);
        }
    }

    // store action in history
    std::ostringstream info;
    info << "Depth " << std::fixed << std::setprecision(2) << d << "  ";
    current_action_ = s->name() + ": " + info.str();

    if ( d > LAYER_FOREGROUND )
        info << "\n   (Foreground layer)";
    else if ( d < LAYER_BACKGROUND )
        info << "\n   (Background layer)";
    else
        info << "\n   (Workspace layer)";

    return Cursor(Cursor_ResizeNESW, info.str() );
}

View::Cursor LayerView::over (glm::vec2 pos)
{
    View::Cursor ret = Cursor();
    std::pair<Node *, glm::vec2> pick = View::pick(pos);

    //
    // mouse over source
    //
    //    Source *s = Mixer::manager().findSource(pick.first);
    Source *s = Mixer::manager().currentSource();
    if (s != nullptr && s->ready()) {

        s->symbol_->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 1.f );
        s->initial_0_->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 1.f );
        s->initial_1_->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 1.f );
        const ImVec4 h = ImGuiToolkit::HighlightColor();

        // overlay symbol
        if ( pick.first == s->symbol_ )
            s->symbol_->color = glm::vec4( h.x, h.y, h.z, 1.f );
        // overlay initials
        else if ( pick.first == s->initial_1_ ) {
            s->initial_1_->color = glm::vec4( h.x, h.y, h.z, 1.f );
            s->initial_0_->color = glm::vec4( h.x, h.y, h.z, 1.f );
        }
    }

    return ret;
}

void LayerView::arrow (glm::vec2 movement)
{
    static glm::vec2 _from(0.f);
    static glm::vec2 _displacement(0.f);

    Source *current = Mixer::manager().currentSource();

    if (!current && !Mixer::selection().empty())
        Mixer::manager().setCurrentSource( Mixer::selection().back() );

    if (current) {

        if (current_action_ongoing_) {

            // add movement to displacement
            movement.x += movement.y * -0.5f;
            _displacement += glm::vec2(movement.x, -0.5f * movement.x) * dt_ * 0.2f;

            // set coordinates of target
            glm::vec2 _to  = _from + _displacement;

            // update mouse pointer action
            MousePointer::manager().active()->update(_to, dt_ / 1000.f);

            // simulate mouse grab
            grab(current, _from, MousePointer::manager().active()->target(),
                 std::make_pair(current->group(mode_), glm::vec2(0.f) ) );

            // draw mouse pointer effect
            MousePointer::manager().active()->draw();
        }
        else {

            if (UserInterface::manager().altModifier() || Settings::application.mouse_pointer_lock)
                MousePointer::manager().setActiveMode( (Pointer::Mode) Settings::application.mouse_pointer );
            else
                MousePointer::manager().setActiveMode( Pointer::POINTER_DEFAULT );

            // initiate view action and store status of source
            initiate();

            // get coordinates of source and set this as start of mouse position
            _from = glm::vec2( Rendering::manager().project(current->group(mode_)->translation_, scene.root()->transform_) );
            _displacement = glm::vec2(0.f);

            // Initiate mouse pointer action
            MousePointer::manager().active()->initiate(_from);
        }

    }
    else {

        terminate(true);

        _from = glm::vec2(0.f);
        _displacement = glm::vec2(0.f);

    }

}



void LayerView::updateSelectionOverlay(glm::vec4 color)
{
    View::updateSelectionOverlay(color);

    if (overlay_selection_->visible_) {
        // calculate bbox on selection
        GlmToolkit::AxisAlignedBoundingBox selection_box = BoundingBoxVisitor::AABB(Mixer::selection().getCopy(), this);
        overlay_selection_->scale_ = selection_box.scale();
        overlay_selection_->translation_ = selection_box.center();

        // slightly extend the boundary of the selection
        overlay_selection_frame_->scale_ = glm::vec3(1.f) + glm::vec3(0.07f, 0.07f, 1.f) / overlay_selection_->scale_;
    }
}

LayerGrid::LayerGrid(Group *parent) : Grid(parent)
{
    root_ = new Group;
    root_->visible_ = false;
    parent_->attach(root_);

    // create custom grids specific for layers in diagonal
    perspective_grids_ = new Switch;
    root_->attach(perspective_grids_);

    // Generate groups for all units
    for (uint u = UNIT_PRECISE; u <= UNIT_ONE; u = u + 1) {
        Group *g = new Group;
        float d = MIN_DEPTH;
        // Fill background
        for (; d < LAYER_BACKGROUND ; d += Grid::ortho_units_[u] * 2.f) {
            HLine *l = new HLine(3.f);
            l->translation_.x = -d +1.f;
            l->translation_.y = -d / LAYER_PERSPECTIVE - 1.f;
            l->scale_.x = 3.5f;
            g->attach(l);
        }
        // Fill workspace
        for (; d < LAYER_FOREGROUND ; d += Grid::ortho_units_[u] * 2.f) {
            HLine *l = new HLine(3.f);
            l->translation_.x = -d +1.f;
            l->translation_.y = -d / LAYER_PERSPECTIVE - 1.15f;
            l->scale_.x = 3.5f;
            g->attach(l);
        }
        // Fill foreground
        for (; d < MAX_DEPTH ; d += Grid::ortho_units_[u] * 2.f) {
            HLine *l = new HLine(3.f);
            l->translation_.x = -d +1.f;
            l->translation_.y = -d / LAYER_PERSPECTIVE - 1.3f;
            l->scale_.x = 3.5f;
            g->attach(l);
        }
        // add this group to the grids
        perspective_grids_->attach(g);
    }

    // not visible at init
//    setColor( glm::vec4(0.f) );
}

Group *LayerGrid::root ()
{
    //adjust the grid to the unit scale
    perspective_grids_->setActive(unit_);

    // return the node to draw
    return root_;
}

