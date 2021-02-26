// Opengl
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/vector_angle.hpp>

#include "imgui.h"
#include "ImGuiToolkit.h"

// memmove
#include <string>
#include <iomanip>

#include "Mixer.h"
#include "defines.h"
#include "Settings.h"
#include "Decorations.h"
#include "UserInterfaceManager.h"
#include "Log.h"

#include "LayerView.h"

LayerView::LayerView() : View(LAYER), aspect_ratio(1.f)
{
    // read default settings
    if ( Settings::application.views[mode_].name.empty() ) {
        // no settings found: store application default
        Settings::application.views[mode_].name = "Layer";
        scene.root()->scale_ = glm::vec3(LAYER_DEFAULT_SCALE, LAYER_DEFAULT_SCALE, 1.0f);
        scene.root()->translation_ = glm::vec3(2.2f, 1.2f, 0.0f);
        saveSettings();
    }
    else
        restoreSettings();

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
    scene.bg()->attach(persp_left_);

    persp_right_ = new Mesh("mesh/perspective_axis_right.ply");
    persp_right_->shader()->color = glm::vec4( COLOR_FRAME_LIGHT, 1.f );
    persp_right_->scale_.x = LAYER_PERSPECTIVE;
    persp_right_->translation_.z = -0.1f;
    scene.bg()->attach(persp_right_);

}


void LayerView::draw()
{
    View::draw();

    // initialize the verification of the selection
    static bool candidate_flatten_group = false;

    // display popup menu
    if (show_context_menu_ == MENU_SELECTION) {

        // initialize the verification of the selection
        candidate_flatten_group = true;
        // start loop on selection
        SourceList::iterator  it = Mixer::selection().begin();
        float depth_first = (*it)->depth();
        for (; it != Mixer::selection().end(); it++) {
            // test if selection is contiguous in layer (i.e. not interrupted)
            SourceList::iterator inter = Mixer::manager().session()->find(depth_first, (*it)->depth());
            if ( inter != Mixer::manager().session()->end() && !Mixer::selection().contains(*inter)){
                // NOT a group: there is a source in the session that
                // - is between two selected sources (in depth)
                // - is not part of the selection
                candidate_flatten_group = false;
                break;
            }
        }

        ImGui::OpenPopup( "LayerSelectionContextMenu" );
        show_context_menu_ = MENU_NONE;
    }
    if (ImGui::BeginPopup("LayerSelectionContextMenu")) {

        ImGui::PushStyleColor(ImGuiCol_Text, ImGuiToolkit::HighlightColor());
        if (candidate_flatten_group){
            if (ImGui::Selectable( ICON_FA_DOWNLOAD "  Flatten" )){
                Mixer::manager().groupSelection();
            }
        }
        else {
            ImGui::TextDisabled( ICON_FA_DOWNLOAD "  Flatten" );
        }
        if (ImGui::Selectable( ICON_FA_ALIGN_CENTER "  Distribute" )){
            SourceList::iterator  it = Mixer::selection().begin();
            float depth = (*it)->depth();
            float depth_inc   = (Mixer::selection().back()->depth() - depth) / static_cast<float>(Mixer::selection().size()-1);
            for (it++; it != Mixer::selection().end(); it++) {
                depth += depth_inc;
                (*it)->setDepth(depth);
            }
            View::need_deep_update_++;
        }
        ImGui::PopStyleColor();
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
    }

    if (Mixer::manager().view() == this )
    {
        // update the selection overlay
        updateSelectionOverlay();
    }

}

bool LayerView::canSelect(Source *s) {

    return ( View::canSelect(s) && s->active() );
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
    glm::vec3 border_left(scene.root()->scale_.x * -2.f, scene.root()->scale_.y * -1.f, 0.f);
    glm::vec3 border_right(scene.root()->scale_.x * 8.f, scene.root()->scale_.y * 8.f, 0.f);
    scene.root()->translation_ = glm::clamp(scene.root()->translation_, border_left, border_right);
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
            if ( pick.first == s->lock_) {
                s->setLocked(false);
                pick = { s->locker_, pick.second };
            }
            // pick on the open lock icon; lock source and cancel pick
            else if ( pick.first == s->unlock_ ) {
                s->setLocked(true);
                pick = { nullptr, glm::vec2(0.f) };
            }
            // pick a locked source without CTRL key; cancel pick
            else if ( s->locked() && !UserInterface::manager().ctrlModifier() )
                pick = { nullptr, glm::vec2(0.f) };
        }
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
        for (NodeSet::iterator node = scene.ws()->begin(); node != scene.ws()->end(); node++) {
            // place in front of previous sources
            depth = MAX(depth, (*node)->translation_.z + LAYER_STEP);

            // in case node is already at max depth
            if ((*node)->translation_.z + DELTA_DEPTH > MAX_DEPTH )
                (*node)->translation_.z -= DELTA_DEPTH;
        }
    }

    // change depth
    sourceNode->translation_.z = CLAMP( depth, MIN_DEPTH, MAX_DEPTH);

    // discretized translation with ALT
    if (UserInterface::manager().altModifier())
        sourceNode->translation_.z = ROUND(sourceNode->translation_.z, 5.f);

    // request reordering of scene at next update
    View::need_deep_update_++;

    // request update of source
    s->touch();

    return sourceNode->translation_.z;
}


View::Cursor LayerView::grab (Source *s, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2> pick)
{
    if (!s)
        return Cursor();

    // unproject
    glm::vec3 gl_Position_from = Rendering::manager().unProject(from, scene.root()->transform_);
    glm::vec3 gl_Position_to   = Rendering::manager().unProject(to, scene.root()->transform_);

    // compute delta translation
    glm::vec3 dest_translation = s->stored_status_->translation_ + gl_Position_to - gl_Position_from;

    // apply change
    float d = setDepth( s,  MAX( -dest_translation.x, 0.f) );

    std::ostringstream info;
    info << "Depth " << std::fixed << std::setprecision(2) << d << "  ";
//    info << (s->locked() ? ICON_FA_LOCK : ICON_FA_LOCK_OPEN); // TODO static not locked

    // store action in history
    current_action_ = s->name() + ": " + info.str();
    current_id_ = s->id();

    return Cursor(Cursor_ResizeNESW, info.str() );
}

void LayerView::arrow (glm::vec2 movement)
{
    Source *s = Mixer::manager().currentSource();
    if (s) {

        glm::vec3 gl_Position_from = Rendering::manager().unProject(glm::vec2(0.f), scene.root()->transform_);
        glm::vec3 gl_Position_to   = Rendering::manager().unProject(glm::vec2(movement.x-movement.y, 0.f), scene.root()->transform_);
        glm::vec3 gl_delta = gl_Position_to - gl_Position_from;
        if (UserInterface::manager().altModifier())
            gl_delta *= 10.f;

        Group *sourceNode = s->group(mode_);
        glm::vec3 dest_translation = sourceNode->translation_ + gl_delta * ARROWS_MOVEMENT_FACTOR;
        setDepth( s,  MAX( -dest_translation.x, 0.f) );

        // request update
        s->touch();
    }
}

