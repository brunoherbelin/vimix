// Opengl
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/vector_angle.hpp>


#include "imgui.h"
#include "ImGuiToolkit.h"

// memmove
#include <string.h>
#include <sstream>
#include <regex>
#include <iomanip>

#include "Mixer.h"
#include "defines.h"
#include "Settings.h"
#include "Session.h"
#include "Resource.h"
#include "Source.h"
#include "SessionSource.h"
#include "PickingVisitor.h"
#include "BoundingBoxVisitor.h"
#include "DrawVisitor.h"
#include "Decorations.h"
#include "Mixer.h"
#include "UserInterfaceManager.h"
#include "UpdateCallback.h"
#include "ActionManager.h"
#include "Log.h"

#include "View.h"

uint View::need_deep_update_ = 1;

View::View(Mode m) : mode_(m)
{
    show_context_menu_ = MENU_NONE;

    overlay_selection_ = nullptr;
    overlay_selection_frame_ = nullptr;
    overlay_selection_icon_ = nullptr;
}

void View::restoreSettings()
{
    scene.root()->scale_ = Settings::application.views[mode_].default_scale;
    scene.root()->translation_ = Settings::application.views[mode_].default_translation;
}

void View::saveSettings()
{
    Settings::application.views[mode_].default_scale = scene.root()->scale_;
    Settings::application.views[mode_].default_translation = scene.root()->translation_;
}

void View::draw()
{
    // draw scene of this view
    scene.root()->draw(glm::identity<glm::mat4>(), Rendering::manager().Projection());
}

void View::update(float dt)
{
    // recursive update from root of scene
    scene.update( dt );

    // a more complete update is requested
    if (View::need_deep_update_ > 0) {
        // reorder sources
        scene.ws()->sort();
    }
}


View::Cursor View::drag (glm::vec2 from, glm::vec2 to)
{
    static glm::vec3 start_translation = glm::vec3(0.f);
    static glm::vec2 start_position = glm::vec2(0.f);

    if ( start_position != from ) {
        start_position = from;
        start_translation = scene.root()->translation_;
    }

    // unproject
    glm::vec3 gl_Position_from = Rendering::manager().unProject(from);
    glm::vec3 gl_Position_to = Rendering::manager().unProject(to);

    // compute delta translation
    scene.root()->translation_ = start_translation + gl_Position_to - gl_Position_from;

    // apply and clamp
    zoom(0.f);

    return Cursor(Cursor_ResizeAll);
}

std::pair<Node *, glm::vec2> View::pick(glm::vec2 P)
{
    // prepare empty return value
    std::pair<Node *, glm::vec2> pick = { nullptr, glm::vec2(0.f) };

    // unproject mouse coordinate into scene coordinates
    glm::vec3 scene_point_ = Rendering::manager().unProject(P);

    // picking visitor traverses the scene
    PickingVisitor pv(scene_point_);
    scene.accept(pv);

    // picking visitor found nodes?
    if ( !pv.empty()) {
        // select top-most Node picked
        pick = pv.back();
    }

    return pick;
}

void View::initiate()
{
    current_action_ = "";
    current_id_ = 0;
    for (auto sit = Mixer::manager().session()->begin();
         sit != Mixer::manager().session()->end(); sit++){

        (*sit)->stored_status_->copyTransform((*sit)->group(mode_));
    }
}

void View::terminate()
{
    std::regex r("\\n");
    current_action_ = std::regex_replace(current_action_, r, " ");
    Action::manager().store(current_action_, current_id_);
    current_action_ = "";
    current_id_ = 0;
}

void View::zoom( float factor )
{
    resize( size() + int(factor * 2.f));
}

void View::recenter()
{
    // restore default view
    restoreSettings();

    // nothing else if scene is empty
    if (scene.ws()->numChildren() < 1)
        return;

    // calculate screen area visible in the default view
    GlmToolkit::AxisAlignedBoundingBox view_box;
    glm::mat4 modelview = GlmToolkit::transform(scene.root()->translation_, scene.root()->rotation_, scene.root()->scale_);
    view_box.extend( Rendering::manager().unProject(glm::vec2(0.f, Rendering::manager().mainWindow().height()), modelview) );
    view_box.extend( Rendering::manager().unProject(glm::vec2(Rendering::manager().mainWindow().width(), 0.f), modelview) );

    // calculate screen area required to see the entire scene
    BoundingBoxVisitor scene_visitor_bbox;
    scene.accept(scene_visitor_bbox);
    GlmToolkit::AxisAlignedBoundingBox scene_box = scene_visitor_bbox.bbox();

    // if the default view does not contains the entire scene
    // we shall adjust the view to fit the scene
    if ( !view_box.contains(scene_box)) {

        // drag view to move towards scene_box center (while remaining in limits of the view)
        glm::vec2 from = Rendering::manager().project(-view_box.center(), modelview);
        glm::vec2 to = Rendering::manager().project(-scene_box.center(), modelview);
        drag(from, to);

        // recalculate the view bounding box
        GlmToolkit::AxisAlignedBoundingBox updated_view_box;
        modelview = GlmToolkit::transform(scene.root()->translation_, scene.root()->rotation_, scene.root()->scale_);
        updated_view_box.extend( Rendering::manager().unProject(glm::vec2(0.f, Rendering::manager().mainWindow().height()), modelview) );
        updated_view_box.extend( Rendering::manager().unProject(glm::vec2(Rendering::manager().mainWindow().width(), 0.f), modelview) );

        // if the updated (translated) view does not contains the entire scene
        // we shall scale the view to fit the scene
        if ( !updated_view_box.contains(scene_box)) {

            glm::vec3 view_extend = updated_view_box.max() - updated_view_box.min();
            updated_view_box.extend(scene_box);
            glm::vec3 scene_extend = scene_box.max() - scene_box.min();
            glm::vec3 scale = view_extend  / scene_extend  ;
            float z = scene.root()->scale_.x;
            z = CLAMP( z * MIN(scale.x, scale.y), MIXING_MIN_SCALE, MIXING_MAX_SCALE);
            scene.root()->scale_.x = z;
            scene.root()->scale_.y = z;

        }
    }
}

void View::selectAll()
{
    Mixer::selection().clear();
    for(auto sit = Mixer::manager().session()->begin();
        sit != Mixer::manager().session()->end(); sit++) {
        if (canSelect(*sit))
            Mixer::selection().add(*sit);
    }
    // special case of one single source in selection : make current after release
    if (Mixer::selection().size() == 1)
        Mixer::manager().setCurrentSource( Mixer::selection().front() );
}

void View::select(glm::vec2 A, glm::vec2 B)
{
    // unproject mouse coordinate into scene coordinates
    glm::vec3 scene_point_A = Rendering::manager().unProject(A);
    glm::vec3 scene_point_B = Rendering::manager().unProject(B);

    // picking visitor traverses the scene
    PickingVisitor pv(scene_point_A, scene_point_B);
    scene.accept(pv);

    // picking visitor found nodes in the area?
    if ( !pv.empty()) {

        // create a list of source matching the list of picked nodes
        SourceList selection;
        // loop over the nodes and add all sources found.
         for(std::vector< std::pair<Node *, glm::vec2> >::const_reverse_iterator p = pv.rbegin(); p != pv.rend(); p++){
            Source *s = Mixer::manager().findSource( p->first );
            if (canSelect(s))
                selection.push_back( s );
        }
        // set the selection with list of picked (overlaped) sources
        Mixer::selection().set(selection);
    }
    else
        // reset selection
        Mixer::selection().clear();
}

bool View::canSelect(Source *s) {

    return ( s!=nullptr && !s->locked() );
}

void View::updateSelectionOverlay()
{
    // create first
    if (overlay_selection_ == nullptr) {
        overlay_selection_ = new Group;
        overlay_selection_icon_ = new Handles(Handles::MENU);
        overlay_selection_->attach(overlay_selection_icon_);
        overlay_selection_frame_ = new Frame(Frame::SHARP, Frame::LARGE, Frame::NONE);
//        overlay_selection_frame_->scale_ = glm::vec3(1.05f, 1.05f, 1.f);
        overlay_selection_->attach(overlay_selection_frame_);
        scene.fg()->attach(overlay_selection_);
    }

    // no overlay by default
    overlay_selection_->visible_ = false;

    // potential selection if more than 1 source selected
    if (Mixer::selection().size() > 1) {
        // calculate bbox on selection
        BoundingBoxVisitor selection_visitor_bbox;
        SourceList::iterator  it = Mixer::selection().begin();
        for (; it != Mixer::selection().end(); it++) {
            // calculate bounding box of area covered by selection
            selection_visitor_bbox.setModelview( scene.ws()->transform_ );
            (*it)->group(mode_)->accept(selection_visitor_bbox);
        }
        // adjust group overlay to selection
        GlmToolkit::AxisAlignedBoundingBox selection_box = selection_visitor_bbox.bbox();
        overlay_selection_->scale_ = selection_box.scale();
        overlay_selection_->translation_ = selection_box.center();
        // show group overlay
        overlay_selection_->visible_ = true;
        ImVec4 c = ImGuiToolkit::HighlightColor();
        overlay_selection_frame_->color = glm::vec4(c.x, c.y, c.z, c.w * 0.8f);
        overlay_selection_icon_->color = glm::vec4(c.x, c.y, c.z, c.w);
    }
    // no selection: reset drawing selection overlay
    else
        overlay_selection_->scale_ = glm::vec3(0.f, 0.f, 1.f);
}

