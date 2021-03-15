// Opengl
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/vector_angle.hpp>

#include "imgui.h"
#include "ImGuiToolkit.h"

#include <string>
#include <sstream>
#include <iomanip>

#include "Mixer.h"
#include "defines.h"
#include "Settings.h"
#include "Decorations.h"
#include "UserInterfaceManager.h"
#include "BoundingBoxVisitor.h"
#include "ActionManager.h"
#include "MixingGroup.h"
#include "Log.h"

#include "MixingView.h"

// internal utility
float sin_quad_texture(float x, float y);
uint textureMixingQuadratic();



MixingView::MixingView() : View(MIXING), limbo_scale_(MIXING_LIMBO_SCALE)
{
    // read default settings
    if ( Settings::application.views[mode_].name.empty() ) {
        // no settings found: store application default
        Settings::application.views[mode_].name = "Mixing";
        scene.root()->scale_ = glm::vec3(MIXING_DEFAULT_SCALE, MIXING_DEFAULT_SCALE, 1.0f);
        scene.root()->translation_ = glm::vec3(0.0f, 0.0f, 0.0f);
        saveSettings();
    }
    else
        restoreSettings();

    // Mixing scene background
    Mesh *tmp = new Mesh("mesh/disk.ply");
    tmp->scale_ = glm::vec3(limbo_scale_, limbo_scale_, 1.f);
    tmp->shader()->color = glm::vec4( COLOR_LIMBO_CIRCLE, 0.6f );
    scene.bg()->attach(tmp);

    mixingCircle_ = new Mesh("mesh/disk.ply");
    mixingCircle_->setTexture(textureMixingQuadratic());
    mixingCircle_->shader()->color = glm::vec4( 1.f, 1.f, 1.f, 1.f );
    scene.bg()->attach(mixingCircle_);

    circle_ = new Mesh("mesh/circle.ply");
    circle_->shader()->color = glm::vec4( COLOR_CIRCLE, 1.0f );
    scene.bg()->attach(circle_);

    // Mixing scene foreground
    tmp = new Mesh("mesh/disk.ply");
    tmp->scale_ = glm::vec3(0.033f, 0.033f, 1.f);
    tmp->translation_ = glm::vec3(0.f, 1.f, 0.f);
    tmp->shader()->color = glm::vec4( COLOR_CIRCLE, 0.9f );
    scene.fg()->attach(tmp);

    button_white_ = new Disk();
    button_white_->scale_ = glm::vec3(0.026f, 0.026f, 1.f);
    button_white_->translation_ = glm::vec3(0.f, 1.f, 0.f);
    button_white_->color = glm::vec4( 0.85f, 0.85f, 0.85f, 1.0f );
    scene.fg()->attach(button_white_);

    tmp = new Mesh("mesh/disk.ply");
    tmp->scale_ = glm::vec3(0.033f, 0.033f, 1.f);
    tmp->translation_ = glm::vec3(0.f, -1.f, 0.f);
    tmp->shader()->color = glm::vec4( COLOR_CIRCLE, 0.9f );
    scene.fg()->attach(tmp);

    button_black_ = new Disk();
    button_black_->scale_ = glm::vec3(0.026f, 0.026f, 1.f);
    button_black_->translation_ = glm::vec3(0.f, -1.f, 0.f);
    button_black_->color = glm::vec4( 0.1f, 0.1f, 0.1f, 1.0f );
    scene.fg()->attach(button_black_);

    slider_root_ = new Group;
    scene.fg()->attach(slider_root_);

    tmp = new Mesh("mesh/disk.ply");
    tmp->scale_ = glm::vec3(0.08f, 0.08f, 1.f);
    tmp->translation_ = glm::vec3(0.0f, 1.0f, 0.f);
    tmp->shader()->color = glm::vec4( COLOR_CIRCLE, 0.9f );
    slider_root_->attach(tmp);

    slider_ = new Disk();
    slider_->scale_ = glm::vec3(0.075f, 0.075f, 1.f);
    slider_->translation_ = glm::vec3(0.0f, 1.0f, 0.f);
    slider_->color = glm::vec4( COLOR_SLIDER_CIRCLE, 1.0f );
    slider_root_->attach(slider_);

    stashCircle_ = new Disk();
    stashCircle_->scale_ = glm::vec3(0.5f, 0.5f, 1.f);
    stashCircle_->translation_ = glm::vec3(2.f, -1.0f, 0.f);
    stashCircle_->color = glm::vec4( COLOR_STASH_CIRCLE, 0.6f );
//    scene.bg()->attach(stashCircle_);


}


void MixingView::draw()
{
    // temporarily force shaders to use opacity blending for rendering icons
    Shader::force_blending_opacity = true;
    // draw scene of this view
    View::draw();
    // restore state
    Shader::force_blending_opacity = false;

    // display popup menu
    if (show_context_menu_ == MENU_SELECTION) {
        ImGui::OpenPopup( "MixingSelectionContextMenu" );
        show_context_menu_ = MENU_NONE;
    }
    if (ImGui::BeginPopup("MixingSelectionContextMenu")) {

        // colored context menu
        ImGui::PushStyleColor(ImGuiCol_Text, ImGuiToolkit::HighlightColor());
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.36f, 0.36f, 0.36f, 0.44f));

        // special action of Mixing view: link or unlink
        SourceList selected = Mixer::selection().getCopy();
        if ( Mixer::manager().session()->canlink( selected )) {
            // the selected sources can be linked
            if (ImGui::Selectable( ICON_FA_LINK "  Link" )){
                // assemble a MixingGroup
                Mixer::manager().session()->link(selected, scene.fg() );
                Action::manager().store(std::string("Sources linked."), selected.front()->id());
                // clear selection and select one of the sources of the group
                Source *cur = Mixer::selection().front();
                Mixer::manager().unsetCurrentSource();
                Mixer::selection().clear();
                Mixer::manager().setCurrentSource( cur );
            }
        }
        else {
            // the selected sources cannot be linked: offer to unlink!
            if (ImGui::Selectable( ICON_FA_UNLINK "  Unlink" )){
                // dismantle MixingGroup(s)
                Mixer::manager().session()->unlink(selected);
                Action::manager().store(std::string("Sources unlinked."), selected.front()->id());
            }
        }

        ImGui::Separator();

        // manipulation of sources in Mixing view
        if (ImGui::Selectable( ICON_FA_CROSSHAIRS "  Center" )){
            SourceList::iterator  it = Mixer::selection().begin();
            for (; it != Mixer::selection().end(); it++) {
                (*it)->group(View::MIXING)->translation_ -= overlay_selection_->translation_;
                (*it)->touch();
            }
        }
        if (ImGui::Selectable( ICON_FA_EXPAND_ARROWS_ALT "  Expand & Hide" )){
            SourceList::iterator  it = Mixer::selection().begin();
            for (; it != Mixer::selection().end(); it++) {
                (*it)->setAlpha(0.f);
            }
        }
        ImGui::PopStyleColor(2);
        ImGui::EndPopup();
    }
}

void MixingView::resize ( int scale )
{
    float z = CLAMP(0.01f * (float) scale, 0.f, 1.f);
    z *= z;
    z *= MIXING_MAX_SCALE - MIXING_MIN_SCALE;
    z += MIXING_MIN_SCALE;
    scene.root()->scale_.x = z;
    scene.root()->scale_.y = z;

    // Clamp translation to acceptable area
    glm::vec3 border(scene.root()->scale_.x * 1.f, scene.root()->scale_.y * 1.f, 0.f);
    scene.root()->translation_ = glm::clamp(scene.root()->translation_, -border, border);
}

int  MixingView::size ()
{
    float z = (scene.root()->scale_.x - MIXING_MIN_SCALE) / (MIXING_MAX_SCALE - MIXING_MIN_SCALE);
    return (int) ( sqrt(z) * 100.f);
}

void MixingView::centerSource(Source *s)
{
    // setup view so that the center of the source ends at screen coordinates (650, 150)
    // -> this is just next to the navigation pannel
    glm::vec2 screenpoint = glm::vec2(500.f, 20.f) * Rendering::manager().mainWindow().dpiScale();
    glm::vec3 pos_to = Rendering::manager().unProject(screenpoint, scene.root()->transform_);
    glm::vec3 pos_from( - s->group(View::MIXING)->scale_.x, s->group(View::MIXING)->scale_.y, 0.f);
    pos_from += s->group(View::MIXING)->translation_;
    glm::vec4 pos_delta = glm::vec4(pos_to.x, pos_to.y, 0.f, 0.f) - glm::vec4(pos_from.x, pos_from.y, 0.f, 0.f);
    pos_delta = scene.root()->transform_ * pos_delta;
    scene.root()->translation_ += glm::vec3(pos_delta);

}

void MixingView::update(float dt)
{
    View::update(dt);

//    // always update the mixinggroups
//    for (auto g = groups_.begin(); g != groups_.end(); g++)
//        (*g)->update(dt);

    // a more complete update is requested
    // for mixing, this means restore position of the fading slider
    if (View::need_deep_update_ > 0) {

        //
        // Set slider to match the actual fading of the session
        //
        float f = Mixer::manager().session()->empty() ? 0.f : Mixer::manager().session()->fading();

        // reverse calculate angle from fading & move slider
        slider_root_->rotation_.z  = SIGN(slider_root_->rotation_.z) * asin(f) * 2.f;

        // visual feedback on mixing circle
        f = 1.f - f;
        mixingCircle_->shader()->color = glm::vec4(f, f, f, 1.f);

    }

    // the current view is the mixing view
    if (Mixer::manager().view() == this )
    {
        //
        // Set session fading to match the slider angle (during animation)
        //

        // calculate fading from angle
        float f = sin( ABS(slider_root_->rotation_.z) * 0.5f);

        // apply fading
        if ( ABS_DIFF( f, Mixer::manager().session()->fading()) > EPSILON )
        {
            // apply fading to session
            Mixer::manager().session()->setFading(f);

            // visual feedback on mixing circle
            f = 1.f - f;
            mixingCircle_->shader()->color = glm::vec4(f, f, f, 1.f);
        }

        // update the selection overlay
        updateSelectionOverlay();
    }

}


std::pair<Node *, glm::vec2> MixingView::pick(glm::vec2 P)
{
    // get picking from generic View
    std::pair<Node *, glm::vec2> pick = View::pick(P);

    // deal with internal interactive objects
    if ( pick.first == button_white_ || pick.first == button_black_ ) {

        RotateToCallback *anim = nullptr;
        if (pick.first == button_white_)
            anim = new RotateToCallback(0.f, 500.f);
        else
            anim = new RotateToCallback(SIGN(slider_root_->rotation_.z) * M_PI, 500.f);

        // animate clic
        pick.first->update_callbacks_.push_back(new BounceScaleCallback(0.3f));

        // reset & start animation
        slider_root_->update_callbacks_.clear();
        slider_root_->update_callbacks_.push_back(anim);

    }
    else if ( overlay_selection_icon_ != nullptr && pick.first == overlay_selection_icon_ ) {

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
            else if ( s->locked() && !UserInterface::manager().ctrlModifier() ) {
                pick = { nullptr, glm::vec2(0.f) };
            }
            // pick on the mixing group rotation icon
            else if ( pick.first == s->rotation_mixingroup_ ) {
                s->mixinggroup_->setAction( MixingGroup::ACTION_ROTATE_ALL );
            }
            // pick source of a mixing group
            else if ( s->mixinggroup_ != nullptr ) {
                if (UserInterface::manager().ctrlModifier()) {
                    SourceList linked = s->mixinggroup_->getCopy();
                    linked.remove(s);
                    if (Mixer::selection().empty())
                        Mixer::selection().add(linked);
                }
                else if (UserInterface::manager().shiftModifier())
                    s->mixinggroup_->setAction( MixingGroup::ACTION_GRAB_ONE );
                else
                    s->mixinggroup_->setAction( MixingGroup::ACTION_GRAB_ALL );
            }
        }
    }

    return pick;
}



View::Cursor MixingView::grab (Source *s, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2> pick)
{
    View::Cursor ret = Cursor();
    ret.type = Cursor_ResizeAll;

    // unproject
    glm::vec3 gl_Position_from = Rendering::manager().unProject(from, scene.root()->transform_);
    glm::vec3 gl_Position_to   = Rendering::manager().unProject(to, scene.root()->transform_);

    // No source is given
    if (!s) {

        // if interaction with slider
        if (pick.first == slider_) {

            // apply rotation to match angle with mouse cursor
            float angle = glm::orientedAngle( glm::normalize(glm::vec2(0.f, 1.0)), glm::normalize(glm::vec2(gl_Position_to)));

            // snap on 0 and PI angles
            if ( ABS_DIFF(angle, 0.f) < 0.05)
                angle = 0.f;
            else if ( ABS_DIFF(angle, M_PI) < 0.05)
                angle = M_PI;

            // animate slider (rotation angle on its parent)
            slider_root_->rotation_.z = angle;

            // cursor feedback
            std::ostringstream info;
            info  << "Global opacity " << 100 - int(Mixer::manager().session()->fading() * 100.0) << " %";
            return Cursor(Cursor_Hand, info.str() );
        }

        // nothing to do
        return Cursor();
    }
    //
    // Interaction with source
    //
    // compute delta translation
    s->group(mode_)->translation_ = s->stored_status_->translation_ + gl_Position_to - gl_Position_from;

    // manage mixing group
    if (s->mixinggroup_ != nullptr  ) {
        // inform mixing groups to follow the current source
        if (Source::isCurrent(s) && s->mixinggroup_->action() > MixingGroup::ACTION_UPDATE) {
            s->mixinggroup_->follow(s);
            if (s->mixinggroup_->action() == MixingGroup::ACTION_ROTATE_ALL)
                ret.type = Cursor_Hand;
        }
        else
            s->mixingGroup()->setAction(MixingGroup::ACTION_NONE);
    }

    // request update
    s->touch();

//    // trying to enter stash
//    if ( glm::distance( glm::vec2(s->group(mode_)->translation_), glm::vec2(stashCircle_->translation_)) < stashCircle_->scale_.x) {

//        // refuse to put an active source in stash
//        if (s->active())
//            s->group(mode_)->translation_ = s->stored_status_->translation_;
//        else {
//            Mixer::manager().conceal(s);
//            s->group(mode_)->scale_ = glm::vec3(MIXING_ICON_SCALE) - glm::vec3(0.1f, 0.1f, 0.f);
//        }
//    }
//    else if ( Mixer::manager().concealed(s) ) {
//        Mixer::manager().uncover(s);
//        s->group(mode_)->scale_ = glm::vec3(MIXING_ICON_SCALE);
//    }


    std::ostringstream info;
    if (s->active()) {
        info << "Alpha " << std::fixed << std::setprecision(3) << s->blendingShader()->color.a << "  ";
        info << ( (s->blendingShader()->color.a > 0.f) ? ICON_FA_EYE : ICON_FA_EYE_SLASH);
    }
    else
        info << "Inactive  " << ICON_FA_SNOWFLAKE;

    // store action in history
    current_action_ = s->name() + ": " + info.str();
    current_id_ = s->id();

    // update cursor
    ret.info = info.str();
    return ret;
}

void MixingView::terminate()
{
    View::terminate();

    // terminate all mixing group actions
    for (auto g = Mixer::manager().session()->beginMixingGroup();
         g != Mixer::manager().session()->endMixingGroup(); g++)
        (*g)->setAction( MixingGroup::ACTION_NONE );

}

void MixingView::arrow (glm::vec2 movement)
{
    Source *s = Mixer::manager().currentSource();
    if (s) {

        glm::vec3 gl_Position_from = Rendering::manager().unProject(glm::vec2(0.f), scene.root()->transform_);
        glm::vec3 gl_Position_to   = Rendering::manager().unProject(movement, scene.root()->transform_);
        glm::vec3 gl_delta = gl_Position_to - gl_Position_from;

        Group *sourceNode = s->group(mode_);
        if (UserInterface::manager().altModifier()) {
            sourceNode->translation_ += glm::vec3(movement.x, -movement.y, 0.f) * 0.1f;
            sourceNode->translation_.x = ROUND(sourceNode->translation_.x, 10.f);
            sourceNode->translation_.y = ROUND(sourceNode->translation_.y, 10.f);
        }
        else
            sourceNode->translation_ += gl_delta * ARROWS_MOVEMENT_FACTOR;

        // request update
        s->touch();
    }
}

void MixingView::setAlpha(Source *s)
{
    if (!s)
        return;

    // move the layer node of the source
    Group *sourceNode = s->group(mode_);
    glm::vec2 mix_pos = glm::vec2(DEFAULT_MIXING_TRANSLATION);

    for(NodeSet::iterator it = scene.ws()->begin(); it != scene.ws()->end(); it++) {
        // avoid superposing icons: distribute equally
        if ( glm::distance(glm::vec2((*it)->translation_), mix_pos) < DELTA_ALPHA) {
            mix_pos += glm::vec2(-0.03f, 0.03f);
        }
    }

    sourceNode->translation_.x = mix_pos.x;
    sourceNode->translation_.y = mix_pos.y;

    // request update
    s->touch();
}


void MixingView::updateSelectionOverlay()
{
    View::updateSelectionOverlay();

    if (overlay_selection_->visible_) {
        // calculate bbox on selection
        GlmToolkit::AxisAlignedBoundingBox selection_box = BoundingBoxVisitor::AABB(Mixer::selection().getCopy(), this);
        overlay_selection_->scale_ = selection_box.scale();
        overlay_selection_->translation_ = selection_box.center();

        // slightly extend the boundary of the selection
        overlay_selection_frame_->scale_ = glm::vec3(1.f) + glm::vec3(0.01f, 0.01f, 1.f) / overlay_selection_->scale_;
    }
}

#define CIRCLE_PIXELS 64
#define CIRCLE_PIXEL_RADIUS 1024.0
//#define CIRCLE_PIXELS 256
//#define CIRCLE_PIXEL_RADIUS 16384.0
//#define CIRCLE_PIXELS 1024
//#define CIRCLE_PIXEL_RADIUS 262144.0

float sin_quad_texture(float x, float y) {
//    return 0.5f + 0.5f * cos( M_PI * CLAMP( ( ( x * x ) + ( y * y ) ) / CIRCLE_PIXEL_RADIUS, 0.f, 1.f ) );
    float D = sqrt( ( x * x )/ CIRCLE_PIXEL_RADIUS + ( y * y )/ CIRCLE_PIXEL_RADIUS );
    return 0.5f + 0.5f * cos( M_PI * CLAMP( D * sqrt(D), 0.f, 1.f ) );
}

uint textureMixingQuadratic()
{
    static GLuint texid = 0;
    if (texid == 0) {
        // generate the texture with alpha exactly as computed for sources
        GLubyte matrix[CIRCLE_PIXELS*CIRCLE_PIXELS * 4];
        GLubyte color[4] = {0,0,0,0};
        GLfloat luminance = 1.f;
        GLfloat alpha = 0.f;
        GLfloat distance = 0.f;
        int l = -CIRCLE_PIXELS / 2 + 1, c = 0;

        for (int i = 0; i < CIRCLE_PIXELS / 2; ++i) {
            c = -CIRCLE_PIXELS / 2 + 1;
            for (int j=0; j < CIRCLE_PIXELS / 2; ++j) {
                // distance to the center
                distance = sin_quad_texture( (float) c , (float) l  );
//                distance = 1.f - (GLfloat) ((c * c) + (l * l)) / CIRCLE_PIXEL_RADIUS;  // quadratic
//                distance = 1.f - (GLfloat) sqrt( (GLfloat) ((c * c) + (l * l))) / (GLfloat) sqrt(CIRCLE_PIXEL_RADIUS); // linear

                // transparency
                alpha = 255.f * CLAMP( distance , 0.f, 1.f);
                color[3] = static_cast<GLubyte>(alpha);

                // luminance adjustment
                luminance = 255.f * CLAMP( 0.2f + 0.75f * distance, 0.f, 1.f);
                color[0] = color[1] = color[2] = static_cast<GLubyte>(luminance);

                // 1st quadrant
                memmove(&matrix[ j * 4 + i * CIRCLE_PIXELS * 4 ], color, 4 * sizeof(GLubyte));
                // 4nd quadrant
                memmove(&matrix[ (CIRCLE_PIXELS -j -1)* 4 + i * CIRCLE_PIXELS * 4 ], color, 4 * sizeof(GLubyte));
                // 3rd quadrant
                memmove(&matrix[ j * 4 + (CIRCLE_PIXELS -i -1) * CIRCLE_PIXELS * 4 ], color, 4 * sizeof(GLubyte));
                // 4th quadrant
                memmove(&matrix[ (CIRCLE_PIXELS -j -1) * 4 + (CIRCLE_PIXELS -i -1) * CIRCLE_PIXELS * 4 ], color, 4 * sizeof(GLubyte));

                ++c;
            }
            ++l;
        }
        // setup texture
        glGenTextures(1, &texid);
        glBindTexture(GL_TEXTURE_2D, texid);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, CIRCLE_PIXELS, CIRCLE_PIXELS);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, CIRCLE_PIXELS, CIRCLE_PIXELS, GL_BGRA, GL_UNSIGNED_BYTE, matrix);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    }
    return texid;
}
