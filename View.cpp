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

#include "View.h"
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


uint View::need_deep_update_ = 1;

View::View(Mode m) : mode_(m)
{
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
        if ( (*sit)->active() )
            Mixer::selection().add(*sit);
    }
}

void View::select(glm::vec2 A, glm::vec2 B)
{
    // unproject mouse coordinate into scene coordinates
    glm::vec3 scene_point_A = Rendering::manager().unProject(A);
    glm::vec3 scene_point_B = Rendering::manager().unProject(B);

    // picking visitor traverses the scene
    PickingVisitor pv(scene_point_A, scene_point_B);
    scene.accept(pv);

    // reset selection
    Mixer::selection().clear();

    // picking visitor found nodes in the area?
    if ( !pv.empty()) {

        // create a list of source matching the list of picked nodes
        SourceList selection;
//        std::vector< std::pair<Node *, glm::vec2> > pick = pv.picked();
        // loop over the nodes and add all sources found.
//        for(std::vector< std::pair<Node *, glm::vec2> >::iterator p = pick.begin(); p != pick.end(); p++){
        for(std::vector< std::pair<Node *, glm::vec2> >::const_reverse_iterator p = pv.rbegin(); p != pv.rend(); p++){
            Source *s = Mixer::manager().findSource( p->first );
            if (s)
                selection.push_back( s );
        }
        // set the selection with list of picked (overlaped) sources
        Mixer::selection().set(selection);

    }

}


MixingView::MixingView() : View(MIXING), limbo_scale_(1.3f)
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

    tmp = new Mesh("mesh/circle.ply");
    tmp->shader()->color = glm::vec4( COLOR_FRAME, 0.9f );
    scene.bg()->attach(tmp);

    // Mixing scene foreground
    tmp = new Mesh("mesh/disk.ply");
    tmp->scale_ = glm::vec3(0.033f, 0.033f, 1.f);
    tmp->translation_ = glm::vec3(0.f, 1.f, 0.f);
    tmp->shader()->color = glm::vec4( COLOR_FRAME, 0.9f );
    scene.fg()->attach(tmp);

    button_white_ = new Disk();
    button_white_->scale_ = glm::vec3(0.026f, 0.026f, 1.f);
    button_white_->translation_ = glm::vec3(0.f, 1.f, 0.f);
    button_white_->color = glm::vec4( 0.85f, 0.85f, 0.85f, 1.0f );
    scene.fg()->attach(button_white_);

    tmp = new Mesh("mesh/disk.ply");
    tmp->scale_ = glm::vec3(0.033f, 0.033f, 1.f);
    tmp->translation_ = glm::vec3(0.f, -1.f, 0.f);
    tmp->shader()->color = glm::vec4( COLOR_FRAME, 0.9f );
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
    tmp->shader()->color = glm::vec4( COLOR_FRAME, 0.9f );
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
}

void MixingView::zoom( float factor )
{
    float z = scene.root()->scale_.x;
    z = CLAMP( z + 0.1f * factor, MIXING_MIN_SCALE, MIXING_MAX_SCALE);
    scene.root()->scale_.x = z;
    scene.root()->scale_.y = z;
}

void MixingView::resize ( int scale )
{
    float z = CLAMP(0.01f * (float) scale, 0.f, 1.f);
    z *= z;
    z *= MIXING_MAX_SCALE - MIXING_MIN_SCALE;
    z += MIXING_MIN_SCALE;
    scene.root()->scale_.x = z;
    scene.root()->scale_.y = z;
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

void MixingView::selectAll()
{
    for(auto sit = Mixer::manager().session()->begin();
        sit != Mixer::manager().session()->end(); sit++) {
        Mixer::selection().add(*sit);
    }
}

void MixingView::update(float dt)
{
    View::update(dt);

    // a more complete update is requested
    // for mixing, this means restore position of the fading slider
    if (View::need_deep_update_ > 0) {

        //
        // Set slider to match the actual fading of the session
        //
        float f = Mixer::manager().session()->fading();

        // reverse calculate angle from fading & move slider
        slider_root_->rotation_.z  = SIGN(slider_root_->rotation_.z) * asin(f) * 2.f;

        // visual feedback on mixing circle
        f = 1.f - f;
        mixingCircle_->shader()->color = glm::vec4(f, f, f, 1.f);

    }
    else {
        //
        // Set session fading to match the slider angle
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
    }

}


std::pair<Node *, glm::vec2> MixingView::pick(glm::vec2 P)
{
    // get picking from generic View
    std::pair<Node *, glm::vec2> pick = View::pick(P);

    // deal with internal interactive objects and do not forward
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

        // capture this pick
        pick = { nullptr, glm::vec2(0.f) };
    }

    return pick;
}



View::Cursor MixingView::grab (Source *s, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2> pick)
{
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

//    // diagonal translation with SHIFT
//    if (UserInterface::manager().shiftModifier()) {
//        s->group(mode_)->translation_.y = s->group(mode_)->translation_.x * s->stored_status_->translation_.y / s->stored_status_->translation_.x;
//    }


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

    // request update
    s->touch();

    std::ostringstream info;
    if (s->active()) {
        info << "Alpha " << std::fixed << std::setprecision(3) << s->blendingShader()->color.a;
//    else if ( Mixer::manager().concealed(s) )
//        info << "Stashed";q
        if (s->blendingShader()->color.a > 0.f)
            info << "  " << ICON_FA_EYE;
        else
            info << "  " << ICON_FA_EYE_SLASH;
    }
    else
        info << "Inactive  " << ICON_FA_SNOWFLAKE;

    // store action in history
    current_action_ = s->name() + ": " + info.str();
    current_id_ = s->id();

    return Cursor(Cursor_ResizeAll, info.str() );
}

View::Cursor MixingView::drag (glm::vec2 from, glm::vec2 to)
{
    Cursor ret = View::drag(from, to);

    // Clamp translation to acceptable area
    scene.root()->translation_ = glm::clamp(scene.root()->translation_, glm::vec3(-3.f, -2.f, 0.f), glm::vec3(3.f, 2.f, 0.f));

    return ret;
}

void MixingView::setAlpha(Source *s)
{
    if (!s)
        return;

    // move the layer node of the source
    Group *sourceNode = s->group(mode_);
    glm::vec2 mix_pos = glm::vec2(DEFAULT_MIXING_TRANSLATION);

    for(NodeSet::iterator it = scene.ws()->begin(); it != scene.ws()->end(); it++) {

        if ( glm::distance(glm::vec2((*it)->translation_), mix_pos) < 0.001) {
            mix_pos += glm::vec2(-0.03, 0.03);
        }
    }

    sourceNode->translation_.x = mix_pos.x;
    sourceNode->translation_.y = mix_pos.y;

    // request update
    s->touch();
}

#define CIRCLE_PIXELS 64
#define CIRCLE_PIXEL_RADIUS 1024.0
//#define CIRCLE_PIXELS 256
//#define CIRCLE_PIXEL_RADIUS 16384.0
//#define CIRCLE_PIXELS 1024
//#define CIRCLE_PIXEL_RADIUS 262144.0

float sin_quad_texture(float x, float y) {
    return 0.5f + 0.5f * cos( M_PI * CLAMP( ( ( x * x ) + ( y * y ) ) / CIRCLE_PIXEL_RADIUS, 0.f, 1.f ) );
}

uint MixingView::textureMixingQuadratic()
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

RenderView::RenderView() : View(RENDERING), frame_buffer_(nullptr), fading_overlay_(nullptr)
{
    // set resolution to settings default
    setResolution();
}

RenderView::~RenderView()
{
    if (frame_buffer_)
        delete frame_buffer_;
    if (fading_overlay_)
        delete fading_overlay_;
}

void RenderView::setFading(float f)
{
    if (fading_overlay_ == nullptr)
        fading_overlay_ = new Surface;

    fading_overlay_->shader()->color.a = CLAMP( f < EPSILON ? 0.f : f, 0.f, 1.f);
}

float RenderView::fading() const
{
    if (fading_overlay_)
        return fading_overlay_->shader()->color.a;
    else
        return 0.f;
}

void RenderView::setResolution(glm::vec3 resolution)
{
    // use default resolution if invalid resolution is given (default behavior)
    if (resolution.x < 2.f || resolution.y < 2.f)
        resolution = FrameBuffer::getResolutionFromParameters(Settings::application.render.ratio, Settings::application.render.res);

    // do we need to change resolution ?
    if (frame_buffer_ && frame_buffer_->resolution() != resolution)  {

        // new frame buffer
        delete frame_buffer_;
        frame_buffer_ = nullptr;
    }

    if (!frame_buffer_)
        // output frame is an RBG Multisamples FrameBuffer
        frame_buffer_ = new FrameBuffer(resolution, false, true);

    // reset fading
    setFading();
}

void RenderView::draw()
{
    static glm::mat4 projection = glm::ortho(-1.f, 1.f, 1.f, -1.f, -SCENE_DEPTH, 1.f);

    // draw in frame buffer
    glm::mat4 P  = glm::scale( projection, glm::vec3(1.f / frame_buffer_->aspectRatio(), 1.f, 1.f));
    frame_buffer_->begin();
    scene.root()->draw(glm::identity<glm::mat4>(), P);
    fading_overlay_->draw(glm::identity<glm::mat4>(), projection);
    frame_buffer_->end();
}

GeometryView::GeometryView() : View(GEOMETRY)
{
    // read default settings
    if ( Settings::application.views[mode_].name.empty() ) {
        // no settings found: store application default
        Settings::application.views[mode_].name = "Geometry";
        scene.root()->scale_ = glm::vec3(GEOMETRY_DEFAULT_SCALE, GEOMETRY_DEFAULT_SCALE, 1.0f);
        saveSettings();
    }
    else
        restoreSettings();

    // Geometry Scene background
    Surface *rect = new Surface;
    scene.bg()->attach(rect);

    // Geometry Scene foreground
    Frame *border = new Frame(Frame::SHARP, Frame::THIN, Frame::NONE);
    border->color = glm::vec4( COLOR_FRAME, 1.f );
    scene.fg()->attach(border);

    // User interface foreground
    //
    // point to show POSITION
    overlay_position_ = new Symbol(Symbol::SQUARE_POINT);
    overlay_position_->scale_ = glm::vec3(0.5f, 0.5f, 1.f);
    scene.fg()->attach(overlay_position_);
    overlay_position_->visible_ = false;
    // cross to show the axis for POSITION
    overlay_position_cross_ = new Symbol(Symbol::CROSS);
    overlay_position_cross_->rotation_ = glm::vec3(0.f, 0.f, M_PI_4);
    overlay_position_cross_->scale_ = glm::vec3(0.3f, 0.3f, 1.f);
    scene.fg()->attach(overlay_position_cross_);
    overlay_position_cross_->visible_ = false;
    // 'clock' : tic marks every 10 degrees for ROTATION
    // with dark background
    Group *g = new Group;
    Symbol *s = new Symbol(Symbol::CLOCK);
    g->attach(s);
    s = new Symbol(Symbol::CIRCLE_POINT);
    s->color = glm::vec4(0.f, 0.f, 0.f, 0.1f);
    s->scale_ = glm::vec3(28.f, 28.f, 1.f);
    s->translation_.z = -0.1;
    g->attach(s);
    overlay_rotation_clock_ = g;
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
    g = new Group;
    s = new Symbol(Symbol::GRID);
    g->attach(s);
    s = new Symbol(Symbol::SQUARE_POINT);
    s->color = glm::vec4(0.f, 0.f, 0.f, 0.1f);
    s->scale_ = glm::vec3(18.f, 18.f, 1.f);
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

    border = new Frame(Frame::SHARP, Frame::THIN, Frame::NONE);
    border->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 0.2f );
    overlay_crop_ = border;
    scene.fg()->attach(overlay_crop_);
    overlay_crop_->visible_ = false;

    show_context_menu_ = false;
}

void GeometryView::update(float dt)
{
    View::update(dt);

    // a more complete update is requested
    if (View::need_deep_update_ > 0) {

        // update rendering of render frame
        FrameBuffer *output = Mixer::manager().session()->frame();
        if (output){
            float aspect_ratio = output->aspectRatio();
            for (NodeSet::iterator node = scene.bg()->begin(); node != scene.bg()->end(); node++) {
                (*node)->scale_.x = aspect_ratio;
            }
            for (NodeSet::iterator node = scene.fg()->begin(); node != scene.fg()->end(); node++) {
                (*node)->scale_.x = aspect_ratio;
            }
        }
    }
}

void GeometryView::zoom( float factor )
{
    float z = scene.root()->scale_.x;
    z = CLAMP( z + 0.1f * factor, GEOMETRY_MIN_SCALE, GEOMETRY_MAX_SCALE);
    scene.root()->scale_.x = z;
    scene.root()->scale_.y = z;
}

void GeometryView::resize ( int scale )
{
    float z = CLAMP(0.01f * (float) scale, 0.f, 1.f);
    z *= z;
    z *= GEOMETRY_MAX_SCALE - GEOMETRY_MIN_SCALE;
    z += GEOMETRY_MIN_SCALE;
    scene.root()->scale_.x = z;
    scene.root()->scale_.y = z;
}

int  GeometryView::size ()
{
    float z = (scene.root()->scale_.x - GEOMETRY_MIN_SCALE) / (GEOMETRY_MAX_SCALE - GEOMETRY_MIN_SCALE);
    return (int) ( sqrt(z) * 100.f);
}

void showContextMenu(View::Mode m, const char* label)
{
    if (ImGui::BeginPopup(label)) {
        Source *s = Mixer::manager().currentSource();
        if (s != nullptr) {
            if (ImGui::Selectable( ICON_FA_VECTOR_SQUARE "  Reset" )){
                s->group(m)->scale_ = glm::vec3(1.f);
                s->group(m)->rotation_.z = 0;
                s->group(m)->crop_ = glm::vec3(1.f);
                s->group(m)->translation_ = glm::vec3(0.f);
                s->touch();
            }
            else if (ImGui::Selectable( ICON_FA_EXPAND "  Fit" )){
                glm::vec3 scale = glm::vec3(1.f);
                if ( m == View::GEOMETRY) {
                    FrameBuffer *output = Mixer::manager().session()->frame();
                    if (output) scale.x = output->aspectRatio() / s->frame()->aspectRatio();
                }
                else if ( m == View::APPEARANCE ) {
                    glm::vec2 crop = s->frame()->projectionArea();
                    scale = glm::vec3( crop, 1.f);
                }
                s->group(m)->scale_ = scale;
                s->group(m)->rotation_.z = 0;
                s->group(m)->translation_ = glm::vec3(0.f);
                s->touch();
            }
            else  if (ImGui::Selectable( ICON_FA_CROSSHAIRS "  Center" )){
                s->group(m)->translation_ = glm::vec3(0.f);
                s->touch();
            }
            else if (ImGui::Selectable( ICON_FA_PERCENTAGE "   Original aspect ratio" )){ //ICON_FA_ARROWS_ALT_H
                s->group(m)->scale_.x = s->group(m)->scale_.y;
                s->group(m)->scale_ *= s->group(m)->crop_;
                s->touch();
            }
        }
        ImGui::EndPopup();
    }
}

void GeometryView::draw()
{
    // hack to prevent source manipulation (scale and rotate)
    // when multiple sources are selected: simply do not draw overlay in scene
    Source *s = Mixer::manager().currentSource();
    if (s != nullptr) {
        if ( Mixer::selection().size() > 1)   {
            s->setMode(Source::SELECTED);
            s = nullptr;
        }
    }

    // draw scene of this view    
    View::draw();

    // re-draw frames of all sources on top
    // (otherwise hidden in stack of sources)
    for (auto source_iter = Mixer::manager().session()->begin(); source_iter != Mixer::manager().session()->end(); source_iter++)
    {
        DrawVisitor dv((*source_iter)->frames_[mode_], Rendering::manager().Projection());
        scene.accept(dv);
    }

    // re-draw overlay of current source on top
    // (allows manipulation current source even when hidden below others)
    if (s != nullptr) {
        s->setMode(Source::CURRENT);
        DrawVisitor dv(s->overlays_[mode_], Rendering::manager().Projection());
        scene.accept(dv);
    }

    DrawVisitor dv(scene.fg(), Rendering::manager().Projection());
    scene.accept(dv);

    // display popup menu
    if (show_context_menu_) {
        ImGui::OpenPopup( "GeometryContextMenu" );
        show_context_menu_ = false;
    }
    showContextMenu(mode_,"GeometryContextMenu");

}


std::pair<Node *, glm::vec2> GeometryView::pick(glm::vec2 P)
{
    // prepare empty return value
    std::pair<Node *, glm::vec2> pick = { nullptr, glm::vec2(0.f) };

    // unproject mouse coordinate into scene coordinates
    glm::vec3 scene_point_ = Rendering::manager().unProject(P);

    // picking visitor traverses the scene
    PickingVisitor pv(scene_point_);
    scene.accept(pv);

    // picking visitor found nodes?
    if ( !pv.empty() ) {
        // keep current source active if it is clicked
        Source *s = Mixer::manager().currentSource();
        if (s != nullptr) {
            // find if the current source was picked
            auto itp = pv.rbegin();
            for (; itp != pv.rend(); itp++){
                // test if source contains this node
                Source::hasNode is_in_source((*itp).first );
                if ( is_in_source( s ) ){
                    // a node in the current source was clicked !
                    pick = *itp;
                    break;
                }
            }
            // not found: the current source was not clicked
            if (itp == pv.rend())
                s = nullptr;
            // picking on the menu handle
            else if ( pick.first == s->handles_[mode_][Handles::MENU] ) {
                // show context menu
                show_context_menu_ = true;
            }
        }
        // the clicked source changed (not the current source)
        if (s == nullptr) {
            // select top-most Node picked
            pick = pv.back();
        }
    }

    return pick;
}

View::Cursor GeometryView::grab (Source *s, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2> pick)
{
    View::Cursor ret = Cursor();

    // work on the given source
    if (!s)
        return ret;
    Group *sourceNode = s->group(mode_); // groups_[View::GEOMETRY]

    // grab coordinates in scene-View reference frame
    glm::vec3 scene_from = Rendering::manager().unProject(from, scene.root()->transform_);
    glm::vec3 scene_to   = Rendering::manager().unProject(to, scene.root()->transform_);
    glm::vec3 scene_translation = scene_to - scene_from;

    // make sure matrix transform of stored status is updated
    s->stored_status_->update(0);
    // grab coordinates in source-root reference frame
    glm::vec4 source_from = glm::inverse(s->stored_status_->transform_) * glm::vec4( scene_from,  1.f );
    glm::vec4 source_to   = glm::inverse(s->stored_status_->transform_) * glm::vec4( scene_to,  1.f );
    glm::vec3 source_scaling     = glm::vec3(source_to) / glm::vec3(source_from);

    // which manipulation to perform?
    std::ostringstream info;
    if (pick.first)  {
        // which corner was picked ?
        glm::vec2 corner = glm::round(pick.second);

        // transform from source center to corner
        glm::mat4 T = GlmToolkit::transform(glm::vec3(corner.x, corner.y, 0.f), glm::vec3(0.f, 0.f, 0.f),
                                            glm::vec3(1.f / s->frame()->aspectRatio(), 1.f, 1.f));

        // transformation from scene to corner:
        glm::mat4 scene_to_corner_transform = T * glm::inverse(s->stored_status_->transform_);
        glm::mat4 corner_to_scene_transform = glm::inverse(scene_to_corner_transform);

        // compute cursor movement in corner reference frame
        glm::vec4 corner_from = scene_to_corner_transform * glm::vec4( scene_from,  1.f );
        glm::vec4 corner_to   = scene_to_corner_transform * glm::vec4( scene_to,  1.f );
        // operation of scaling in corner reference frame
        glm::vec3 corner_scaling = glm::vec3(corner_to) / glm::vec3(corner_from);

        // convert source position in corner reference frame
        glm::vec4 center = scene_to_corner_transform * glm::vec4( s->stored_status_->translation_, 1.f);

        // picking on the resizing handles in the corners
        if ( pick.first == s->handles_[mode_][Handles::RESIZE] ) {

            // hide all other grips
            s->handles_[mode_][Handles::SCALE]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_H]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_V]->visible_ = false;
            s->handles_[mode_][Handles::ROTATE]->visible_ = false;
            s->handles_[mode_][Handles::CROP]->visible_ = false;
            s->handles_[mode_][Handles::MENU]->visible_ = false;
            // inform on which corner should be overlayed (opposite)
            s->handles_[mode_][Handles::RESIZE]->overlayActiveCorner(-corner);
            // RESIZE CORNER
            // proportional SCALING with SHIFT
            if (UserInterface::manager().shiftModifier()) {
                // calculate proportional scaling factor
                float factor = glm::length( glm::vec2( corner_to ) ) / glm::length( glm::vec2( corner_from ) );
                // scale node
                sourceNode->scale_ = s->stored_status_->scale_ * glm::vec3(factor, factor, 1.f);
                // discretized scaling with ALT
                if (UserInterface::manager().altModifier()) {
                    sourceNode->scale_.x = ROUND(sourceNode->scale_.x, 10.f);
                    factor = sourceNode->scale_.x / s->stored_status_->scale_.x;
                    sourceNode->scale_.y = s->stored_status_->scale_.y * factor;
                }
                // update corner scaling to apply to center coordinates
                corner_scaling = sourceNode->scale_ / s->stored_status_->scale_;
            }
            // non-proportional CORNER RESIZE  (normal case)
            else {
                // scale node
                sourceNode->scale_ = s->stored_status_->scale_ * corner_scaling;
                // discretized scaling with ALT
                if (UserInterface::manager().altModifier()) {
                    sourceNode->scale_.x = ROUND(sourceNode->scale_.x, 10.f);
                    sourceNode->scale_.y = ROUND(sourceNode->scale_.y, 10.f);
                    corner_scaling = sourceNode->scale_ / s->stored_status_->scale_;
                }
            }
            // transform source center (in corner reference frame)
            center = glm::scale(glm::identity<glm::mat4>(), corner_scaling) * center;
            // convert center back into scene reference frame
            center = corner_to_scene_transform * center;
            // apply to node
            sourceNode->translation_ = glm::vec3(center);
            // show cursor depending on diagonal (corner picked)
            T = glm::rotate(glm::identity<glm::mat4>(), s->stored_status_->rotation_.z, glm::vec3(0.f, 0.f, 1.f));
            T = glm::scale(T, s->stored_status_->scale_);
            corner = T * glm::vec4( corner, 0.f, 0.f );
            ret.type = corner.x * corner.y > 0.f ? Cursor_ResizeNESW : Cursor_ResizeNWSE;
            info << "Size " << std::fixed << std::setprecision(3) << sourceNode->scale_.x;
            info << " x "  << sourceNode->scale_.y;

        }
        // picking on the BORDER RESIZING handles left or right
        else if ( pick.first == s->handles_[mode_][Handles::RESIZE_H] ) {

            // hide all other grips
            s->handles_[mode_][Handles::RESIZE]->visible_ = false;
            s->handles_[mode_][Handles::SCALE]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_V]->visible_ = false;
            s->handles_[mode_][Handles::ROTATE]->visible_ = false;
            s->handles_[mode_][Handles::CROP]->visible_ = false;
            s->handles_[mode_][Handles::MENU]->visible_ = false;
            // inform on which corner should be overlayed (opposite)
            s->handles_[mode_][Handles::RESIZE_H]->overlayActiveCorner(-corner);
            // SHIFT: HORIZONTAL SCALE to restore source aspect ratio
            if (UserInterface::manager().shiftModifier()) {
                sourceNode->scale_.x = ABS(sourceNode->scale_.y) * SIGN(sourceNode->scale_.x);
                corner_scaling = sourceNode->scale_ / s->stored_status_->scale_;
            }
            // HORIZONTAL RESIZE (normal case)
            else {
                // x scale only
                corner_scaling = glm::vec3(corner_scaling.x, 1.f, 1.f);
                // scale node
                sourceNode->scale_ = s->stored_status_->scale_ * corner_scaling;
                // POST-CORRECTION ; discretized scaling with ALT
                if (UserInterface::manager().altModifier()) {
                    sourceNode->scale_.x = ROUND(sourceNode->scale_.x, 10.f);
                    corner_scaling = sourceNode->scale_ / s->stored_status_->scale_;
                }
            }
            // transform source center (in corner reference frame)
            center = glm::scale(glm::identity<glm::mat4>(), corner_scaling) * center;
            // convert center back into scene reference frame
            center = corner_to_scene_transform * center;
            // apply to node
            sourceNode->translation_ = glm::vec3(center);
            // show cursor depending on angle
            float c = tan(sourceNode->rotation_.z);
            ret.type = ABS(c) > 1.f ? Cursor_ResizeNS : Cursor_ResizeEW;
            info << "Size " << std::fixed << std::setprecision(3) << sourceNode->scale_.x;
            info << " x "  << sourceNode->scale_.y;
        }
        // picking on the BORDER RESIZING handles top or bottom
        else if ( pick.first == s->handles_[mode_][Handles::RESIZE_V] ) {

            // hide all other grips
            s->handles_[mode_][Handles::RESIZE]->visible_ = false;
            s->handles_[mode_][Handles::SCALE]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_H]->visible_ = false;
            s->handles_[mode_][Handles::ROTATE]->visible_ = false;
            s->handles_[mode_][Handles::CROP]->visible_ = false;
            s->handles_[mode_][Handles::MENU]->visible_ = false;
            // inform on which corner should be overlayed (opposite)
            s->handles_[mode_][Handles::RESIZE_V]->overlayActiveCorner(-corner);
            // SHIFT: VERTICAL SCALE to restore source aspect ratio
            if (UserInterface::manager().shiftModifier()) {
                sourceNode->scale_.y = ABS(sourceNode->scale_.x) * SIGN(sourceNode->scale_.y);
                corner_scaling = sourceNode->scale_ / s->stored_status_->scale_;
            }
            // VERTICAL RESIZE (normal case)
            else {
                // y scale only
                corner_scaling = glm::vec3(1.f, corner_scaling.y, 1.f);
                // scale node
                sourceNode->scale_ = s->stored_status_->scale_ * corner_scaling;
                // POST-CORRECTION ; discretized scaling with ALT
                if (UserInterface::manager().altModifier()) {
                    sourceNode->scale_.y = ROUND(sourceNode->scale_.y, 10.f);
                    corner_scaling = sourceNode->scale_ / s->stored_status_->scale_;
                }
            }
            // transform source center (in corner reference frame)
            center = glm::scale(glm::identity<glm::mat4>(), corner_scaling) * center;
            // convert center back into scene reference frame
            center = corner_to_scene_transform * center;
            // apply to node
            sourceNode->translation_ = glm::vec3(center);
            // show cursor depending on angle
            float c = tan(sourceNode->rotation_.z);
            ret.type = ABS(c) > 1.f ? Cursor_ResizeEW : Cursor_ResizeNS;
            info << "Size " << std::fixed << std::setprecision(3) << sourceNode->scale_.x;
            info << " x "  << sourceNode->scale_.y;
        }
        // picking on the CENTRER SCALING handle
        else if ( pick.first == s->handles_[mode_][Handles::SCALE] ) {

            // hide all other grips
            s->handles_[mode_][Handles::RESIZE]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_H]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_V]->visible_ = false;
            s->handles_[mode_][Handles::ROTATE]->visible_ = false;
            s->handles_[mode_][Handles::CROP]->visible_ = false;
            s->handles_[mode_][Handles::MENU]->visible_ = false;
            // prepare overlay
            overlay_scaling_cross_->visible_ = false;
            overlay_scaling_grid_->visible_ = false;
            overlay_scaling_->visible_ = true;
            overlay_scaling_->translation_.x = s->stored_status_->translation_.x;
            overlay_scaling_->translation_.y = s->stored_status_->translation_.y;
            overlay_scaling_->rotation_.z = s->stored_status_->rotation_.z;
            overlay_scaling_->update(0);
            // PROPORTIONAL ONLY
            if (UserInterface::manager().shiftModifier()) {
                float factor = glm::length( glm::vec2( source_to ) ) / glm::length( glm::vec2( source_from ) );
                source_scaling = glm::vec3(factor, factor, 1.f);
                overlay_scaling_cross_->visible_ = true;
                overlay_scaling_cross_->copyTransform(overlay_scaling_);
            }
            // apply center scaling
            sourceNode->scale_ = s->stored_status_->scale_ * source_scaling;
            // POST-CORRECTION ; discretized scaling with ALT
            if (UserInterface::manager().altModifier()) {
                sourceNode->scale_.x = ROUND(sourceNode->scale_.x, 10.f);
                sourceNode->scale_.y = ROUND(sourceNode->scale_.y, 10.f);
                overlay_scaling_grid_->visible_ = true;
                overlay_scaling_grid_->copyTransform(overlay_scaling_);
            }
            // show cursor depending on diagonal
            corner = glm::sign(sourceNode->scale_);
            ret.type = (corner.x * corner.y) > 0.f ? Cursor_ResizeNWSE : Cursor_ResizeNESW;
            info << "Size " << std::fixed << std::setprecision(3) << sourceNode->scale_.x;
            info << " x "  << sourceNode->scale_.y;
        }
        // picking on the CROP
        else if ( pick.first == s->handles_[mode_][Handles::CROP] ) {

            // hide all other grips
            s->handles_[mode_][Handles::RESIZE]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_H]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_V]->visible_ = false;
            s->handles_[mode_][Handles::ROTATE]->visible_ = false;
            s->handles_[mode_][Handles::SCALE]->visible_ = false;
            s->handles_[mode_][Handles::MENU]->visible_ = false;

            // prepare overlay
            overlay_crop_->scale_ = s->stored_status_->scale_ / s->stored_status_->crop_;
            overlay_crop_->scale_.x  *= s->frame()->aspectRatio();
            overlay_crop_->translation_.x = s->stored_status_->translation_.x;
            overlay_crop_->translation_.y = s->stored_status_->translation_.y;
            overlay_crop_->rotation_.z = s->stored_status_->rotation_.z;
            overlay_crop_->update(0);
            overlay_crop_->visible_ = true;

            // PROPORTIONAL ONLY
            if (UserInterface::manager().shiftModifier()) {
                float factor = glm::length( glm::vec2( source_to ) ) / glm::length( glm::vec2( source_from ) );
                source_scaling = glm::vec3(factor, factor, 1.f);
            }
            // calculate crop of framebuffer
            sourceNode->crop_ = s->stored_status_->crop_ * source_scaling;
            // POST-CORRECTION ; discretized crop with ALT
            if (UserInterface::manager().altModifier()) {
                sourceNode->crop_.x = ROUND(sourceNode->crop_.x, 10.f);
                sourceNode->crop_.y = ROUND(sourceNode->crop_.y, 10.f);
            }
            // CLAMP crop values
            sourceNode->crop_.x = CLAMP(sourceNode->crop_.x, 0.1f, 1.f);
            sourceNode->crop_.y = CLAMP(sourceNode->crop_.y, 0.1f, 1.f);
            // apply center scaling
            s->frame()->setProjectionArea( glm::vec2(sourceNode->crop_) );
            sourceNode->scale_ = s->stored_status_->scale_ * (sourceNode->crop_ / s->stored_status_->crop_);
            // show cursor depending on diagonal
            corner = glm::sign(sourceNode->scale_);
            ret.type = (corner.x * corner.y) < 0.f ? Cursor_ResizeNWSE : Cursor_ResizeNESW;
            info << "Crop " << std::fixed << std::setprecision(3) << sourceNode->crop_.x;
            info << " x "  << sourceNode->crop_.y;
        }
        // picking on the rotating handle
        else if ( pick.first == s->handles_[mode_][Handles::ROTATE] ) {

            // hide all other grips
            s->handles_[mode_][Handles::RESIZE]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_H]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_V]->visible_ = false;
            s->handles_[mode_][Handles::SCALE]->visible_ = false;
            s->handles_[mode_][Handles::CROP]->visible_ = false;
            s->handles_[mode_][Handles::MENU]->visible_ = false;

            // ROTATION on CENTER
            overlay_rotation_->visible_ = true;
            overlay_rotation_->translation_.x = s->stored_status_->translation_.x;
            overlay_rotation_->translation_.y = s->stored_status_->translation_.y;
            overlay_rotation_->update(0);
            overlay_rotation_fix_->visible_ = true;
            overlay_rotation_fix_->copyTransform(overlay_rotation_);
            overlay_rotation_clock_->visible_ = false;

            // rotation center to center of source (disregarding scale)
            glm::mat4 T = glm::translate(glm::identity<glm::mat4>(), s->stored_status_->translation_);
            source_from = glm::inverse(T) * glm::vec4( scene_from,  1.f );
            source_to   = glm::inverse(T) * glm::vec4( scene_to,  1.f );
            // compute rotation angle
            float angle = glm::orientedAngle( glm::normalize(glm::vec2(source_from)), glm::normalize(glm::vec2(source_to)));
            // apply rotation on Z axis
            sourceNode->rotation_ = s->stored_status_->rotation_ + glm::vec3(0.f, 0.f, angle);

            // POST-CORRECTION ; discretized rotation with ALT
            int degrees = int(  glm::degrees(sourceNode->rotation_.z) );
            if (UserInterface::manager().altModifier()) {
                degrees = (degrees / 10) * 10;
                sourceNode->rotation_.z = glm::radians( float(degrees) );
                overlay_rotation_clock_->visible_ = true;
                overlay_rotation_clock_->copyTransform(overlay_rotation_);
                info << "Angle " << degrees << "\u00b0"; // degree symbol
            }
            else
                info << "Angle " << std::fixed << std::setprecision(1) << glm::degrees(sourceNode->rotation_.z) << "\u00b0"; // degree symbol

            overlay_rotation_clock_hand_->visible_ = true;
            overlay_rotation_clock_hand_->translation_.x = s->stored_status_->translation_.x;
            overlay_rotation_clock_hand_->translation_.y = s->stored_status_->translation_.y;
            overlay_rotation_clock_hand_->rotation_.z = sourceNode->rotation_.z;
            overlay_rotation_clock_hand_->update(0);

            // show cursor for rotation
            ret.type = Cursor_Hand;
            // + SHIFT = no scaling /  NORMAL = with scaling
            if (!UserInterface::manager().shiftModifier()) {
                // compute scaling to match cursor
                float factor = glm::length( glm::vec2( source_to ) ) / glm::length( glm::vec2( source_from ) );
                source_scaling = glm::vec3(factor, factor, 1.f);
                // apply center scaling
                sourceNode->scale_ = s->stored_status_->scale_ * source_scaling;
                info << std::endl << "   Size " << std::fixed << std::setprecision(3) << sourceNode->scale_.x;
                info << " x "  << sourceNode->scale_.y ;                
                overlay_rotation_fix_->visible_ = false;
            }
        }
        // picking anywhere but on a handle: user wants to move the source
        else {
            ret.type = Cursor_ResizeAll;
            sourceNode->translation_ = s->stored_status_->translation_ + scene_translation;
            // discretized translation with ALT
            if (UserInterface::manager().altModifier()) {
                sourceNode->translation_.x = ROUND(sourceNode->translation_.x, 10.f);
                sourceNode->translation_.y = ROUND(sourceNode->translation_.y, 10.f);
            }
            // ALT: single axis movement
            overlay_position_cross_->visible_ = false;
            if (UserInterface::manager().shiftModifier()) {
                overlay_position_cross_->visible_ = true;
                overlay_position_cross_->translation_.x = s->stored_status_->translation_.x;
                overlay_position_cross_->translation_.y = s->stored_status_->translation_.y;
                overlay_position_cross_->update(0);

                glm::vec3 dif = s->stored_status_->translation_ - sourceNode->translation_;
                if (ABS(dif.x) > ABS(dif.y) ) {
                    sourceNode->translation_.y = s->stored_status_->translation_.y;
                    ret.type = Cursor_ResizeEW;
                } else {
                    sourceNode->translation_.x = s->stored_status_->translation_.x;
                    ret.type = Cursor_ResizeNS;
                }
            }
            // Show center overlay for POSITION
            overlay_position_->visible_ = true;
            overlay_position_->translation_.x = sourceNode->translation_.x;
            overlay_position_->translation_.y = sourceNode->translation_.y;
            overlay_position_->update(0);
            // Show move cursor
            info << "Position " << std::fixed << std::setprecision(3) << sourceNode->translation_.x;
            info << ", "  << sourceNode->translation_.y ;
        }
    }

    // request update
    s->touch();

    // store action in history
    current_action_ = s->name() + ": " + info.str();
    current_id_ = s->id();

    // update cursor
    ret.info = info.str();
    return ret;
}



void GeometryView::terminate()
{
    View::terminate();

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
    for (auto sit = Mixer::manager().session()->begin();
         sit != Mixer::manager().session()->end(); sit++){

        (*sit)->handles_[mode_][Handles::RESIZE]->overlayActiveCorner(c);
        (*sit)->handles_[mode_][Handles::RESIZE_H]->overlayActiveCorner(c);
        (*sit)->handles_[mode_][Handles::RESIZE_V]->overlayActiveCorner(c);
        (*sit)->handles_[mode_][Handles::RESIZE]->visible_ = true;
        (*sit)->handles_[mode_][Handles::RESIZE_H]->visible_ = true;
        (*sit)->handles_[mode_][Handles::RESIZE_V]->visible_ = true;
        (*sit)->handles_[mode_][Handles::SCALE]->visible_ = true;
        (*sit)->handles_[mode_][Handles::ROTATE]->visible_ = true;
        (*sit)->handles_[mode_][Handles::CROP]->visible_ = true;
        (*sit)->handles_[mode_][Handles::MENU]->visible_ = true;
    }

}

//View::Cursor GeometryView::over (Source*, glm::vec2, std::pair<Node *, glm::vec2>)
//{
//    View::Cursor ret = Cursor_Arrow;

//    return ret;
//}

View::Cursor GeometryView::drag (glm::vec2 from, glm::vec2 to)
{
    Cursor ret = View::drag(from, to);

    // Clamp translation to acceptable area
    scene.root()->translation_ = glm::clamp(scene.root()->translation_, glm::vec3(-3.f, -1.5f, 0.f), glm::vec3(3.f, 1.5f, 0.f));

    return ret;
}

LayerView::LayerView() : View(LAYER), aspect_ratio(1.f)
{
    // read default settings
    if ( Settings::application.views[mode_].name.empty() ) {
        // no settings found: store application default
        Settings::application.views[mode_].name = "Layer";
        scene.root()->scale_ = glm::vec3(LAYER_DEFAULT_SCALE, LAYER_DEFAULT_SCALE, 1.0f);
        scene.root()->translation_ = glm::vec3(1.3f, 1.f, 0.0f);
        saveSettings();
    }
    else
        restoreSettings();

    // Geometry Scene background
    Surface *rect = new Surface;
    rect->shader()->color.a = 0.3f;
    scene.bg()->attach(rect);

    Mesh *persp = new Mesh("mesh/perspective_layer.ply");
    persp->translation_.z = -0.1f;
    scene.bg()->attach(persp);

    Frame *border = new Frame(Frame::ROUND, Frame::THIN, Frame::PERSPECTIVE);
    border->color = glm::vec4( COLOR_FRAME, 0.7f );
    scene.bg()->attach(border);
}

void LayerView::update(float dt)
{
    View::update(dt);

    // a more complete update is requested
    if (View::need_deep_update_ > 0) {

        // update rendering of render frame
        FrameBuffer *output = Mixer::manager().session()->frame();
        if (output){
            aspect_ratio = output->aspectRatio();
            for (NodeSet::iterator node = scene.bg()->begin(); node != scene.bg()->end(); node++) {
                (*node)->scale_.x = aspect_ratio;
            }
            for (NodeSet::iterator node = scene.fg()->begin(); node != scene.fg()->end(); node++) {
                (*node)->scale_.x = aspect_ratio;
            }
        }
    }
}

void LayerView::zoom (float factor)
{
    float z = scene.root()->scale_.x;
    z = CLAMP( z + 0.1f * factor, LAYER_MIN_SCALE, LAYER_MAX_SCALE);
    scene.root()->scale_.x = z;
    scene.root()->scale_.y = z;
}

void LayerView::resize ( int scale )
{
    float z = CLAMP(0.01f * (float) scale, 0.f, 1.f);
    z *= z;
    z *= LAYER_MAX_SCALE - LAYER_MIN_SCALE;
    z += LAYER_MIN_SCALE;
    scene.root()->scale_.x = z;
    scene.root()->scale_.y = z;
}

int  LayerView::size ()
{
    float z = (scene.root()->scale_.x - LAYER_MIN_SCALE) / (LAYER_MAX_SCALE - LAYER_MIN_SCALE);
    return (int) ( sqrt(z) * 100.f);
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
        Node *front = scene.ws()->front();
        if (front)
            depth = front->translation_.z + 0.5f;
        else
            depth = 0.5f;
    }

    // move on x
    sourceNode->translation_.x = CLAMP( -depth, -(SCENE_DEPTH - 2.f), 0.f);
    // discretized translation with ALT
    if (UserInterface::manager().altModifier()) {
        sourceNode->translation_.x = ROUND(sourceNode->translation_.x, 5.f);
    }
    // diagonal movement only
    sourceNode->translation_.y = sourceNode->translation_.x / aspect_ratio;
    // change depth
    sourceNode->translation_.z = -sourceNode->translation_.x;

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
    info << "Depth " << std::fixed << std::setprecision(2) << d;

    // store action in history
    current_action_ = s->name() + ": " + info.str();
    current_id_ = s->id();

    return Cursor(Cursor_ResizeNESW, info.str() );
}


View::Cursor LayerView::drag (glm::vec2 from, glm::vec2 to)
{
    Cursor ret = View::drag(from, to);

    // Clamp translation to acceptable area
    scene.root()->translation_ = glm::clamp(scene.root()->translation_, glm::vec3(0.f), glm::vec3(4.f, 2.f, 0.f));

    return ret;
}


// TRANSITION

TransitionView::TransitionView() : View(TRANSITION), transition_source_(nullptr)
{
    // read default settings
    if ( Settings::application.views[mode_].name.empty() )
    {
        // no settings found: store application default
        Settings::application.views[mode_].name = "Transition";
        scene.root()->scale_ = glm::vec3(TRANSITION_DEFAULT_SCALE, TRANSITION_DEFAULT_SCALE, 1.0f);
        scene.root()->translation_ = glm::vec3(1.5f, 0.f, 0.0f);
        saveSettings();
    }
    else
        restoreSettings();

    // Geometry Scene background
    gradient_ = new Switch;
    gradient_->attach(new ImageSurface("images/gradient_0_cross_linear.png"));
    gradient_->attach(new ImageSurface("images/gradient_1_black_linear.png"));
    gradient_->attach(new ImageSurface("images/gradient_2_cross_quad.png"));
    gradient_->attach(new ImageSurface("images/gradient_3_black_quad.png"));
    gradient_->scale_ = glm::vec3(0.501f, 0.006f, 1.f);
    gradient_->translation_ = glm::vec3(-0.5f, -0.005f, -0.01f);
    scene.fg()->attach(gradient_);

//    Mesh *horizontal_line = new Mesh("mesh/h_line.ply");
//    horizontal_line->shader()->color = glm::vec4( COLOR_TRANSITION_LINES, 0.9f );
//    scene.fg()->attach(horizontal_line);
    mark_1s_ = new Mesh("mesh/h_mark.ply");
    mark_1s_->translation_ = glm::vec3(-1.f, -0.01f, 0.0f);
    mark_1s_->shader()->color = glm::vec4( COLOR_TRANSITION_LINES, 0.9f );
    scene.fg()->attach(mark_1s_);

    mark_100ms_ = new Mesh("mesh/h_mark.ply");
    mark_100ms_->translation_ = glm::vec3(-1.f, -0.01f, 0.0f);
    mark_100ms_->scale_ = glm::vec3(0.5f, 0.5f, 0.0f);
    mark_100ms_->shader()->color = glm::vec4( COLOR_TRANSITION_LINES, 0.9f );
    scene.fg()->attach(mark_100ms_);

    // move the whole forground below the icons
    scene.fg()->translation_ = glm::vec3(0.f, -0.11f, 0.0f);

    output_surface_ = new Surface;
    output_surface_->shader()->color.a = 0.9f;
    scene.bg()->attach(output_surface_);

    Frame *border = new Frame(Frame::ROUND, Frame::THIN, Frame::GLOW);
    border->color = glm::vec4( COLOR_FRAME, 1.0f );
    scene.bg()->attach(border);

    scene.bg()->scale_ = glm::vec3(0.1f, 0.1f, 1.f);
    scene.bg()->translation_ = glm::vec3(0.4f, 0.f, 0.0f);

}

void TransitionView::update(float dt)
{
    // update scene
    View::update(dt);

    // a more complete update is requested
    if  (View::need_deep_update_ > 0) {

        // update rendering of render frame
        FrameBuffer *output = Mixer::manager().session()->frame();
        if (output){
            float aspect_ratio = output->aspectRatio();
            for (NodeSet::iterator node = scene.bg()->begin(); node != scene.bg()->end(); node++) {
                (*node)->scale_.x = aspect_ratio;
            }
            output_surface_->setTextureIndex( output->texture() );
        }
    }

    // Update transition source
    if ( transition_source_ != nullptr) {

        float d = transition_source_->group(View::TRANSITION)->translation_.x;

        // Transfer this movement to changes in mixing
        // cross fading
        if ( Settings::application.transition.cross_fade )
        {
            float f = 0.f;
            // change alpha of session:
            if (Settings::application.transition.profile == 0)
                // linear => identical coordinates in Mixing View
                f = d;
            else {
                // quadratic => square coordinates in Mixing View
                f = (d+1.f)*(d+1.f) -1.f;
            }
            transition_source_->group(View::MIXING)->translation_.x = CLAMP(f, -1.f, 0.f);
            transition_source_->group(View::MIXING)->translation_.y = 0.f;

        }
        // fade to black
        else
        {
            // change alpha of session ; hidden before -0.5, visible after
            transition_source_->group(View::MIXING)->translation_.x = d < -0.5f ? -1.f : 0.f;
            transition_source_->group(View::MIXING)->translation_.y = 0.f;

            // fade to black at 50% : fade-out [-1.0 -0.5], fade-in [-0.5 0.0]
            float f = 0.f;
            if (Settings::application.transition.profile == 0)
                f = ABS(2.f * d + 1.f);  // linear
            else {
                f = ( 2.f * d + 1.f);  // quadratic
                f *= f;
            }
            Mixer::manager().session()->setFading( 1.f - f );
        }

        // request update
        transition_source_->touch();

        if (d > 0.2f && Settings::application.transition.auto_open)
            Mixer::manager().setView(View::MIXING);

    }


}


void TransitionView::draw()
{
    // update the GUI depending on changes in settings
    gradient_->setActive( 2*Settings::application.transition.profile + (Settings::application.transition.cross_fade ? 0 : 1) );

    // draw scene of this view
    scene.root()->draw(glm::identity<glm::mat4>(), Rendering::manager().Projection());

    // 100ms tic marks
    int n = static_cast<int>( Settings::application.transition.duration / 0.1f );
    glm::mat4 T = glm::translate(glm::identity<glm::mat4>(), glm::vec3( 1.f / n, 0.f, 0.f));
    DrawVisitor dv(mark_100ms_, Rendering::manager().Projection());
    dv.loop(n+1, T);
    scene.accept(dv);

    // 1s tic marks
    int N = static_cast<int>( Settings::application.transition.duration );
    T = glm::translate(glm::identity<glm::mat4>(), glm::vec3( 10.f / n, 0.f, 0.f));
    DrawVisitor dv2(mark_1s_, Rendering::manager().Projection());
    dv2.loop(N+1, T);
    scene.accept(dv2);

    // display interface duration
    glm::vec2 P = Rendering::manager().project(glm::vec3(-0.11f, -0.14f, 0.f), scene.root()->transform_, false);
    ImGui::SetNextWindowPos(ImVec2(P.x, P.y), ImGuiCond_Always);
    if (ImGui::Begin("##Transition", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground
                     | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings
                     | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
    {
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ImGui::SetNextItemWidth(100.f);
        ImGui::DragFloat("##transitionduration", &Settings::application.transition.duration,
                         0.1f, TRANSITION_MIN_DURATION, TRANSITION_MAX_DURATION, "%.1f s");
        ImGui::SameLine();
        if ( ImGui::Button(ICON_FA_STEP_FORWARD) )
            play(false);
        ImGui::PopFont();
        ImGui::End();
    }

}

void TransitionView::selectAll()
{
    Mixer::selection().clear();
    Mixer::selection().add(transition_source_);
}

void TransitionView::attach(SessionSource *ts)
{
    // store source for later (detatch & interaction)
    transition_source_ = ts;

    if ( transition_source_ != nullptr) {
        // insert in scene
        Group *tg = transition_source_->group(View::TRANSITION);
        tg->visible_ = true;
        scene.ws()->attach(tg);

        // in fade to black transition, start transition from current fading value
        if ( !Settings::application.transition.cross_fade) {

            // reverse calculate x position to match actual vading of session
            float d = 0.f;
            if (Settings::application.transition.profile == 0)
                d = -1.f + 0.5f * Mixer::manager().session()->fading();  // linear
            else {
                d = -1.f - 0.5f * ( sqrt(1.f - Mixer::manager().session()->fading()) - 1.f);  // quadratic
            }

            transition_source_->group(View::TRANSITION)->translation_.x = d;
        }
    }
}

Session *TransitionView::detach()
{
    // by default, nothing to return
    Session *ret = nullptr;

    if ( transition_source_ != nullptr) {

        // get and detatch the group node from the view workspace
        Group *tg = transition_source_->group(View::TRANSITION);
        scene.ws()->detach( tg );

        // test if the icon of the transition source is "Ready"
        if ( tg->translation_.x > 0.f )
            // detatch the session and return it
            ret = transition_source_->detach();

        // done with transition
        transition_source_ = nullptr;
    }

    return ret;
}

void TransitionView::zoom (float factor)
{
    if (transition_source_ != nullptr) {
        float d = transition_source_->group(View::TRANSITION)->translation_.x;
        d += 0.1f * factor;
        transition_source_->group(View::TRANSITION)->translation_.x = CLAMP(d, -1.f, 0.f);
    }
}

std::pair<Node *, glm::vec2> TransitionView::pick(glm::vec2 P)
{
    std::pair<Node *, glm::vec2> pick = View::pick(P);

    if (transition_source_ != nullptr) {
        // start animation when clic on target
        if (pick.first == output_surface_)
            play(true);
        // otherwise cancel animation
        else
            transition_source_->group(View::TRANSITION)->clearCallbacks();
    }

    return pick;
}


void TransitionView::play(bool open)
{
    if (transition_source_ != nullptr) {

        // if want to open session after play, target  movement till end position, otherwise stop at 0
        float target_x = open ? 0.4f : 0.f;

        // calculate how far to reach target
        float time = CLAMP(- transition_source_->group(View::TRANSITION)->translation_.x, 0.f, 1.f);
        // extra distance to reach transition if want to open
        time += open ? 0.2f : 0.f;
        // calculate remaining time on the total duration, in ms
        time *= Settings::application.transition.duration  * 1000.f;

        // cancel previous animation
        transition_source_->group(View::TRANSITION)->update_callbacks_.clear();

        // if remaining time is more than 50ms
        if (time > 50.f) {
            // start animation
            MoveToCallback *anim = new MoveToCallback(glm::vec3(target_x, 0.0, 0.0), time);
            transition_source_->group(View::TRANSITION)->update_callbacks_.push_back(anim);
        }
        // otherwise finish animation
        else
            transition_source_->group(View::TRANSITION)->translation_.x = target_x;
    }
}

View::Cursor TransitionView::grab (Source *s, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2>)
{
    if (!s)
        return Cursor();

    // unproject
    glm::vec3 gl_Position_from = Rendering::manager().unProject(from, scene.root()->transform_);
    glm::vec3 gl_Position_to   = Rendering::manager().unProject(to, scene.root()->transform_);

    // compute delta translation
    float d = s->stored_status_->translation_.x + gl_Position_to.x - gl_Position_from.x;
    std::ostringstream info;
    if (d > 0.2) {
        s->group(View::TRANSITION)->translation_.x = 0.4;
        info << "Open session";
    }
    else {
        s->group(View::TRANSITION)->translation_.x = CLAMP(d, -1.f, 0.f);
        info << "Transition " <<  int( 100.f * (1.f + s->group(View::TRANSITION)->translation_.x)) << "%";
    }

    return Cursor(Cursor_ResizeEW, info.str() );
}

View::Cursor TransitionView::drag (glm::vec2 from, glm::vec2 to)
{
    Cursor ret = View::drag(from, to);

    // Clamp translation to acceptable area
    scene.root()->translation_ = glm::clamp(scene.root()->translation_, glm::vec3(1.f, -1.7f, 0.f), glm::vec3(2.f, 1.7f, 0.f));

    return ret;
}


AppearanceView::AppearanceView() : View(APPEARANCE), edit_source_(nullptr), need_edit_update_(true)
{
    // read default settings
    if ( Settings::application.views[mode_].name.empty() ) {
        // no settings found: store application default
        Settings::application.views[mode_].name = "Appearance";
        scene.root()->scale_ = glm::vec3(APPEARANCE_DEFAULT_SCALE, APPEARANCE_DEFAULT_SCALE, 1.0f);
        scene.root()->translation_ = glm::vec3(0.8f, 0.f, 0.0f);
        saveSettings();
    }
    else
        restoreSettings();

    //
    // Scene background
    //
    // global dark
    Surface *tmp = new Surface( new Shader);
    tmp->scale_ = glm::vec3(20.f, 20.f, 1.f);
    tmp->shader()->color = glm::vec4( 0.1f, 0.1f, 0.1f, 0.6f );
    scene.bg()->attach(tmp);
    // frame showing the source original shape    
    background_surface_= new Surface( new Shader);
    background_surface_->scale_ = glm::vec3(20.f, 20.f, 1.f);
    background_surface_->shader()->color = glm::vec4( COLOR_BGROUND, 1.0f );
    scene.bg()->attach(background_surface_);
    background_frame_ = new Frame(Frame::SHARP, Frame::THIN, Frame::NONE);
    background_frame_->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 0.6f );
    scene.bg()->attach(background_frame_);
    // frame with checkerboard background to show cropped preview
    preview_checker_ = new ImageSurface("images/checker.dds");
    static glm::mat4 Tra = glm::scale(glm::translate(glm::identity<glm::mat4>(), glm::vec3( -32.f, -32.f, 0.f)), glm::vec3( 64.f, 64.f, 1.f));
    preview_checker_->shader()->iTransform = Tra;
    scene.bg()->attach(preview_checker_);
    preview_frame_ = new Frame(Frame::SHARP, Frame::THIN, Frame::GLOW);
    preview_frame_->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 1.f );
    scene.bg()->attach(preview_frame_);
    // marks on the frame to show scale
    show_scale_ = false;
    horizontal_mark_ = new Mesh("mesh/h_mark.ply");
    horizontal_mark_->translation_ = glm::vec3(0.f, -1.f, 0.0f);
    horizontal_mark_->scale_ = glm::vec3(2.5f, -2.5f, 0.0f);
    horizontal_mark_->rotation_.z = M_PI;
    horizontal_mark_->shader()->color = glm::vec4( COLOR_TRANSITION_LINES, 0.9f );
    scene.bg()->attach(horizontal_mark_);
    vertical_mark_  = new Mesh("mesh/h_mark.ply");
    vertical_mark_->translation_ = glm::vec3(-1.0f, 0.0f, 0.0f);
    vertical_mark_->scale_ = glm::vec3(2.5f, -2.5f, 0.0f);
    vertical_mark_->rotation_.z = M_PI_2;
    vertical_mark_->shader()->color = glm::vec4( COLOR_TRANSITION_LINES, 0.9f );
    scene.bg()->attach(vertical_mark_);

    //
    // surface to show the texture of the source
    //
    preview_shader_ = new ImageShader;
    preview_surface_ = new Surface(preview_shader_); // to attach source preview
    preview_surface_->translation_.z = 0.002f;
    scene.bg()->attach(preview_surface_);

    //
    // User interface foreground
    //
    // Mask manipulation
    mask_node_ = new Group;
    mask_square_ = new Frame(Frame::SHARP, Frame::LARGE, Frame::NONE);
    mask_square_->color = glm::vec4( COLOR_APPEARANCE_MASK, 1.0f );  //BLUE
    mask_node_->attach(mask_square_);
    mask_handle_ = new Handles(Handles::CROP);
    mask_handle_->color = glm::vec4( COLOR_APPEARANCE_MASK, 1.0f );  //BLUE
    mask_node_->attach(mask_handle_);
    mask_circle_ = new Mesh("mesh/circle.ply");
    mask_circle_->shader()->color = glm::vec4( COLOR_APPEARANCE_MASK, 1.0f );
    mask_node_->attach(mask_circle_);
    mask_corner_  = new Mesh("mesh/corner.ply");
    mask_corner_->shader()->color = glm::vec4( COLOR_APPEARANCE_MASK, 0.9f );
    mask_corner_->translation_ = glm::vec3(-1.f, -1.0f, 0.0f);
    mask_corner_->scale_ = glm::vec3(0.5f, 0.5f, 1.f);
    mask_node_->attach(mask_corner_);
    scene.fg()->attach(mask_node_);

    // Source manipulation (texture coordinates)
    //
    // point to show POSITION
    overlay_position_ = new Symbol(Symbol::SQUARE_POINT);
    overlay_position_->color = glm::vec4( COLOR_APPEARANCE_SOURCE, 1.f );
    overlay_position_->scale_ = glm::vec3(0.5f, 0.5f, 1.f);
    scene.fg()->attach(overlay_position_);
    overlay_position_->visible_ = false;
    // cross to show the axis for POSITION
    overlay_position_cross_ = new Symbol(Symbol::CROSS);
    overlay_position_cross_->color = glm::vec4( COLOR_APPEARANCE_SOURCE, 1.f );
    overlay_position_cross_->rotation_ = glm::vec3(0.f, 0.f, M_PI_4);
    overlay_position_cross_->scale_ = glm::vec3(0.3f, 0.3f, 1.f);
    scene.fg()->attach(overlay_position_cross_);
    overlay_position_cross_->visible_ = false;
    // 'grid' : tic marks every 0.1 step for SCALING
    // with dark background
    Group *g = new Group;
    Symbol *s = new Symbol(Symbol::GRID);
    s->color = glm::vec4( COLOR_APPEARANCE_SOURCE, 1.f );
    g->attach(s);
    s = new Symbol(Symbol::SQUARE_POINT);
    s->color = glm::vec4(0.f, 0.f, 0.f, 0.2f);
    s->scale_ = glm::vec3(18.f, 18.f, 1.f);
    s->translation_.z = -0.1;
    g->attach(s);
    overlay_scaling_grid_ = g;
    overlay_scaling_grid_->scale_ = glm::vec3(0.3f, 0.3f, 1.f);
    scene.fg()->attach(overlay_scaling_grid_);
    overlay_scaling_grid_->visible_ = false;
    // cross in the square for proportional SCALING
    overlay_scaling_cross_ = new Symbol(Symbol::CROSS);
    overlay_scaling_cross_->color = glm::vec4( COLOR_APPEARANCE_SOURCE, 1.f );
    overlay_scaling_cross_->scale_ = glm::vec3(0.3f, 0.3f, 1.f);
    scene.fg()->attach(overlay_scaling_cross_);
    overlay_scaling_cross_->visible_ = false;
    // square to show the center of SCALING
    overlay_scaling_ = new Symbol(Symbol::SQUARE);
    overlay_scaling_->color = glm::vec4( COLOR_APPEARANCE_SOURCE, 1.f );
    overlay_scaling_->scale_ = glm::vec3(0.3f, 0.3f, 1.f);
    scene.fg()->attach(overlay_scaling_);
    overlay_scaling_->visible_ = false;
    // 'clock' : tic marks every 10 degrees for ROTATION
    // with dark background
    g = new Group;
    s = new Symbol(Symbol::CLOCK);
    g->attach(s);
    s = new Symbol(Symbol::CIRCLE_POINT);
    s->color = glm::vec4(0.f, 0.f, 0.f, 0.25f);
    s->scale_ = glm::vec3(28.f, 28.f, 1.f);
    s->translation_.z = -0.1;
    g->attach(s);
    overlay_rotation_clock_ = g;
    overlay_rotation_clock_->scale_ = glm::vec3(0.25f, 0.25f, 1.f);
    scene.fg()->attach(overlay_rotation_clock_);
    overlay_rotation_clock_->visible_ = false;
    // circle to show fixed-size  ROTATION
    overlay_rotation_clock_hand_ = new Symbol(Symbol::CLOCK_H);
    overlay_rotation_clock_hand_->color = glm::vec4( COLOR_APPEARANCE_SOURCE, 1.f );
    overlay_rotation_clock_hand_->scale_ = glm::vec3(0.25f, 0.25f, 1.f);
    scene.fg()->attach(overlay_rotation_clock_hand_);
    overlay_rotation_clock_hand_->visible_ = false;
    overlay_rotation_fix_ = new Symbol(Symbol::SQUARE);
    overlay_rotation_fix_->color = glm::vec4( COLOR_APPEARANCE_SOURCE, 1.f );
    overlay_rotation_fix_->scale_ = glm::vec3(0.25f, 0.25f, 1.f);
    scene.fg()->attach(overlay_rotation_fix_);
    overlay_rotation_fix_->visible_ = false;
    // circle to show the center of ROTATION
    overlay_rotation_ = new Symbol(Symbol::CIRCLE);
    overlay_rotation_->color = glm::vec4( COLOR_APPEARANCE_SOURCE, 1.f );
    overlay_rotation_->scale_ = glm::vec3(0.25f, 0.25f, 1.f);
    scene.fg()->attach(overlay_rotation_);
    overlay_rotation_->visible_ = false;


    /// Tests
    //    test_buffer = new FrameBuffer(800, 450);
    //    Log::Info("test_buffer %s", test_buffer->info().c_str());
    //    test_shader = new MaskShader;
    //    test_shader->type = 0;
    //    test_shader->blur = 0.0;
    //    test_surface = new Surface(test_shader);
    //    preview_mask_ = new FrameBufferSurface(test_buffer); // to attach source preview
    //    preview_mask_->translation_.z = 0.002f;
    ////    scene.bg()->attach(preview_mask_);

}

void AppearanceView::update(float dt)
{
    View::update(dt);

    // a more complete update is requested (e.g. after switching to view)
    if  (View::need_deep_update_ > 0 || edit_source_ != Mixer::manager().currentSource())
        need_edit_update_ = true;

}

void AppearanceView::zoom (float factor)
{
    float z = scene.root()->scale_.x;
    z = CLAMP( z + 0.1f * factor, APPEARANCE_MIN_SCALE, APPEARANCE_MAX_SCALE);
    scene.root()->scale_.x = z;
    scene.root()->scale_.y = z;
}

void AppearanceView::resize ( int scale )
{
    float z = CLAMP(0.01f * (float) scale, 0.f, 1.f);
    z *= z;
    z *= APPEARANCE_MAX_SCALE - APPEARANCE_MIN_SCALE;
    z += APPEARANCE_MIN_SCALE;
    scene.root()->scale_.x = z;
    scene.root()->scale_.y = z;
}

int AppearanceView::size ()
{
    float z = (scene.root()->scale_.x - APPEARANCE_MIN_SCALE) / (APPEARANCE_MAX_SCALE - APPEARANCE_MIN_SCALE);
    return (int) ( sqrt(z) * 100.f);
}


void AppearanceView::selectAll()
{
    Mixer::manager().setCurrentSource( getEditOrCurrentSource() );
}

void AppearanceView::select(glm::vec2 A, glm::vec2 B)
{
    // unproject mouse coordinate into scene coordinates
    glm::vec3 scene_point_A = Rendering::manager().unProject(A);
    glm::vec3 scene_point_B = Rendering::manager().unProject(B);

    // picking visitor traverses the scene
    PickingVisitor pv(scene_point_A, scene_point_B, true);
    scene.accept(pv);

    // picking visitor found nodes in the area?
    if ( !pv.empty()) {
        // loop over sources matching the list of picked nodes
        for(std::vector< std::pair<Node *, glm::vec2> >::const_reverse_iterator p = pv.rbegin(); p != pv.rend(); p++){
            Source *s = Mixer::manager().findSource( p->first );
            // set the edit source as current if selected
            if (s != nullptr && s == edit_source_)
                Mixer::manager().setCurrentSource( s );
        }
    }
}


std::pair<Node *, glm::vec2> AppearanceView::pick(glm::vec2 P)
{
    // prepare empty return value
    std::pair<Node *, glm::vec2> pick = { nullptr, glm::vec2(0.f) };

    // unproject mouse coordinate into scene coordinates
    glm::vec3 scene_point_ = Rendering::manager().unProject(P);

    // picking visitor traverses the scene
    PickingVisitor pv(scene_point_, true);
    scene.accept(pv);

    // picking visitor found nodes?
    if ( !pv.empty()) {
        // keep edit source active if it is clicked
        Source *s = edit_source_;
        if (s != nullptr) {

            // find if the edit source was picked
            auto itp = pv.rbegin();
            for (; itp != pv.rend(); itp++){
                // test if source contains this node
                Source::hasNode is_in_source( (*itp).first );
                if ( is_in_source( s ) ){
                    // a node in the current source was clicked !
                    pick = *itp;
                    break;
                }
                else if ( (*itp).first == mask_handle_ ) {
                    pick = *itp;
                    break;
                }
            }
            // not found: the edit source was not clicked
            if ( itp == pv.rend() )
                s = nullptr;
            // picking on the menu handle
            else if ( pick.first == s->handles_[mode_][Handles::MENU] ) {
                // show context menu
                show_context_menu_ = true;
            }
        }

        // not the edit source (or no edit source)
        if (s == nullptr) {
            // cancel pick
            pick = { nullptr, glm::vec2(0.f) };
        }

    }

    return pick;
}

void AppearanceView::adjustBackground()
{
    // by default consider edit source is null
    mask_node_->visible_ = false;
    float image_original_width = 1.f;
    glm::vec3 scale = glm::vec3(1.f);
    preview_surface_->setTextureIndex( Resource::getTextureTransparent() );

    // if its a valid index
    if (edit_source_ != nullptr) {
        // update rendering frame to match edit source AR
        image_original_width = edit_source_->frame()->aspectRatio();
        scale = edit_source_->mixingsurface_->scale_;
        preview_surface_->setTextureIndex( edit_source_->frame()->texture() );
        preview_shader_->mask_texture = edit_source_->blendingShader()->mask_texture;
        preview_surface_->scale_ = scale;
        // mask appearance
        mask_node_->visible_ = edit_source_->maskShader()->mode > 0;
        mask_circle_->visible_ = edit_source_->maskShader()->mode == 1;
        mask_square_->visible_ = edit_source_->maskShader()->mode >= 2;
        if (edit_source_->maskShader()->mode >= 4) {
            mask_node_->scale_ = scale;
            mask_node_->translation_ = scale - glm::vec3(edit_source_->maskShader()->size, 0.f) * scale ;
            mask_node_->translation_.z = 0.f;
        } else {
            mask_node_->scale_ = scale * glm::vec3(edit_source_->maskShader()->size, 1.f);
            mask_node_->translation_ = glm::vec3(0.f);
        }
        mask_corner_->scale_.y = mask_corner_->scale_.x * mask_node_->scale_.x / mask_node_->scale_.y;
    }

    // background scene
    background_surface_->scale_.x = image_original_width;
    background_surface_->scale_.y = 1.f;
    background_frame_->scale_.x = image_original_width;
    vertical_mark_->translation_.x = -image_original_width;
    preview_frame_->scale_ = scale;
    preview_checker_->scale_ = scale;
    glm::mat4 Ar  = glm::scale(glm::identity<glm::mat4>(), scale );
    static glm::mat4 Tra = glm::scale(glm::translate(glm::identity<glm::mat4>(), glm::vec3( -32.f, -32.f, 0.f)), glm::vec3( 64.f, 64.f, 1.f));
    preview_checker_->shader()->iTransform = Ar * Tra;

    //    /// Tests
    //    // update mask
    //    test_buffer->begin();
    //    test_surface->draw(glm::identity<glm::mat4>(), test_buffer->projection());
    //    test_buffer->end();
}

Source *AppearanceView::getEditOrCurrentSource()
{
    // cancel multiple selection
    if (Mixer::selection().size() > 1) {
        Source *s = Mixer::manager().currentSource();
        Mixer::manager().unsetCurrentSource();
        if ( s == nullptr )
            s = Mixer::selection().front();
        Mixer::selection().clear();
        Mixer::manager().setCurrentSource(s);
    }

    // get current source
    Source *_source = Mixer::manager().currentSource();

    // no current source?
    if (_source == nullptr) {
        // if something can be selected
        if ( !Mixer::manager().session()->empty()) {
            // return the edit source, if exists
            if (edit_source_ != nullptr)
                _source = Mixer::manager().findSource(edit_source_->id());
        }
    }

    return _source;
}

void AppearanceView::draw()
{
    // edit view needs to be updated (source changed)
    if  ( need_edit_update_ )
    {
        need_edit_update_ = false;

        // now, follow the change of current source
        // & remember source to edit
        edit_source_ = getEditOrCurrentSource();

        // update background and frame to match editsource
        adjustBackground();
    }

    // draw marks in axis
    if (edit_source_ != nullptr && show_scale_){
        {
            glm::vec3 dT = glm::vec3( -0.2f * edit_source_->mixingsurface_->scale_.x, 0.f, 0.f);
            glm::mat4 T = glm::translate(glm::identity<glm::mat4>(), dT);
            DrawVisitor dv(horizontal_mark_, Rendering::manager().Projection());
            dv.loop(6, T);
            scene.accept(dv);
        }
        {
            glm::vec3 dT = glm::vec3( 0.f, -0.2f * edit_source_->mixingsurface_->scale_.y, 0.f);
            glm::mat4 T = glm::translate(glm::identity<glm::mat4>(), dT);
            DrawVisitor dv(vertical_mark_, Rendering::manager().Projection());
            dv.loop(6, T);
            scene.accept(dv);
        }
    }

    // draw general view
    Shader::force_blending_opacity = true;
    View::draw();
    Shader::force_blending_opacity = false;

    // if source active
    if (edit_source_ != nullptr){

        // force to redraw the frame of the edit source (even if source is not visible)
        DrawVisitor dv(edit_source_->frames_[mode_], Rendering::manager().Projection(), true);
        scene.accept(dv);

        // display interface
        glm::vec2 P = glm::vec2(-background_frame_->scale_.x - 0.03f, background_frame_->scale_.y );
        P = Rendering::manager().project(glm::vec3(P, 0.f), scene.root()->transform_, false);
        ImGui::SetNextWindowPos(ImVec2(P.x, P.y - 70.f ), ImGuiCond_Always);
        if (ImGui::Begin("##AppearanceMaskOptions", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground
                         | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings
                         | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
        {
            ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);

            int type = edit_source_->maskShader()->mode;
            ImGui::SetNextItemWidth(100.f);
            if ( ImGui::Combo("Mask    ", &type, MaskShader::mask_names, IM_ARRAYSIZE(MaskShader::mask_names) ) ) {
                edit_source_->maskShader()->mode = type;
                edit_source_->touch();
                need_edit_update_ = true;
                // store action history
                std::ostringstream oss;
                oss << edit_source_->name() << ": Texture Mask " << type;
                Action::manager().store(oss.str(), edit_source_->id());
            }

            if (edit_source_->maskShader()->mode > 0) {
                int val = int(edit_source_->maskShader()->blur * 100.f);
                static bool smoothchanged = false;
                ImGui::SameLine();
                ImGui::SetNextItemWidth(190.f);
                if (ImGui::DragInt("Smooth", &val, 1, 0, 100, "%d%%") ) {
                    edit_source_->maskShader()->blur = float(val) / 100.f;
                    edit_source_->touch();
                    need_edit_update_ = true;
                    smoothchanged = true;
                }
                else if (smoothchanged && ImGui::IsMouseReleased(ImGuiMouseButton_Left)){
                    // store action history
                    std::ostringstream oss;
                    oss << edit_source_->name() << ": Texture Smooth " << edit_source_->maskShader()->blur;
                    Action::manager().store(oss.str(), edit_source_->id());
                    smoothchanged = false;
                }
            }

            ImGui::PopFont();
            ImGui::End();
        }

    }

    // display popup menu
    if (show_context_menu_) {
        ImGui::OpenPopup( "AppearanceContextMenu" );
        show_context_menu_ = false;
    }
    showContextMenu(mode_,"AppearanceContextMenu");

    show_scale_ = false;
}

View::Cursor AppearanceView::grab (Source *s, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2> pick)
{
    std::ostringstream info;
    View::Cursor ret = Cursor();

    // grab coordinates in scene-View reference frame
    glm::vec3 scene_from = Rendering::manager().unProject(from, scene.root()->transform_);
    glm::vec3 scene_to   = Rendering::manager().unProject(to, scene.root()->transform_);
    glm::vec3 scene_translation = scene_to - scene_from;

    // Not grabbing a source
    if (!s) {
        // work on the edited source
        if ( edit_source_ != nullptr ) {

            // match edit source AR
            glm::vec3 scale = edit_source_->mixingsurface_->scale_;
            glm::vec3 delta = glm::vec3(0.1) / glm::vec3(scene.root()->scale_.x, scene.root()->scale_.y, 1.0);

            if ( pick.first == mask_handle_ ) {
                // compute scaling of mask
                glm::vec3 val = -scene_to + delta;
                val /= scale;
                // discretized scaling with ALT
                if (UserInterface::manager().altModifier()) {
                    val.x = ROUND(val.x, 5.f);
                    val.y = ROUND(val.y, 5.f);
                    show_scale_ = true;
                }
                // crop mask horizontally
                edit_source_->maskShader()->size.x = CLAMP(val.x, 0.2f, 2.f);
                edit_source_->maskShader()->size.y = CLAMP(val.y, 0.2f, 2.f);
                edit_source_->touch();
                // update
                need_edit_update_ = true;
                // cursor indication
                info << "Texture Mask " << std::fixed << std::setprecision(3) << edit_source_->maskShader()->size.x;
                info << " x "  << edit_source_->maskShader()->size.y;
                ret.type = Cursor_ResizeNESW;
            }

            // store action in history
            current_action_ = edit_source_->name() + ": " + info.str();
            current_id_ = edit_source_->id();

            // update cursor
            ret.info = info.str();
        }
        return ret;
    }

    Group *sourceNode = s->group(mode_); // groups_[View::APPEARANCE]

    // make sure matrix transform of stored status is updated
    s->stored_status_->update(0);

    // grab coordinates in source-root reference frame
    glm::vec4 source_from = glm::inverse(s->stored_status_->transform_) * glm::vec4( scene_from,  1.f );
    glm::vec4 source_to   = glm::inverse(s->stored_status_->transform_) * glm::vec4( scene_to,  1.f );
    glm::vec3 source_scaling     = glm::vec3(source_to) / glm::vec3(source_from);

    // which manipulation to perform?
    if (pick.first)  {
        // which corner was picked ?
        glm::vec2 corner = glm::round(pick.second);

        // transform from source center to corner
        glm::mat4 T = GlmToolkit::transform(glm::vec3(corner.x, corner.y, 0.f), glm::vec3(0.f, 0.f, 0.f),
                                            glm::vec3(1.f / s->frame()->aspectRatio(), 1.f, 1.f));

        // transformation from scene to corner:
        glm::mat4 scene_to_corner_transform = T * glm::inverse(s->stored_status_->transform_);
        glm::mat4 corner_to_scene_transform = glm::inverse(scene_to_corner_transform);

        // compute cursor movement in corner reference frame
        glm::vec4 corner_from = scene_to_corner_transform * glm::vec4( scene_from,  1.f );
        glm::vec4 corner_to   = scene_to_corner_transform * glm::vec4( scene_to,  1.f );
        // operation of scaling in corner reference frame
        glm::vec3 corner_scaling = glm::vec3(corner_to) / glm::vec3(corner_from);

        // convert source position in corner reference frame
        glm::vec4 center = scene_to_corner_transform * glm::vec4( s->stored_status_->translation_, 1.f);

        // picking on the resizing handles in the corners
        if ( pick.first == s->handles_[mode_][Handles::RESIZE] ) {

            // hide all other grips
            s->handles_[mode_][Handles::SCALE]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_H]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_V]->visible_ = false;
            s->handles_[mode_][Handles::ROTATE]->visible_ = false;
            s->handles_[mode_][Handles::MENU]->visible_ = false;

            // inform on which corner should be overlayed (opposite)
            s->handles_[mode_][Handles::RESIZE]->overlayActiveCorner(-corner);

            // RESIZE CORNER
            // proportional SCALING with SHIFT
            if (UserInterface::manager().shiftModifier()) {
                // calculate proportional scaling factor
                float factor = glm::length( glm::vec2( corner_to ) ) / glm::length( glm::vec2( corner_from ) );
                // scale node
                sourceNode->scale_ = s->stored_status_->scale_ * glm::vec3(factor, factor, 1.f);
                // discretized scaling with ALT
                if (UserInterface::manager().altModifier()) {
                    sourceNode->scale_.x = ROUND(sourceNode->scale_.x, 10.f);
                    factor = sourceNode->scale_.x / s->stored_status_->scale_.x;
                    sourceNode->scale_.y = s->stored_status_->scale_.y * factor;
                }
                // update corner scaling to apply to center coordinates
                corner_scaling = sourceNode->scale_ / s->stored_status_->scale_;
            }
            // non-proportional CORNER RESIZE  (normal case)
            else {
                // scale node
                sourceNode->scale_ = s->stored_status_->scale_ * corner_scaling;
                // discretized scaling with ALT
                if (UserInterface::manager().altModifier()) {
                    sourceNode->scale_.x = ROUND(sourceNode->scale_.x, 10.f);
                    sourceNode->scale_.y = ROUND(sourceNode->scale_.y, 10.f);
                    corner_scaling = sourceNode->scale_ / s->stored_status_->scale_;
                }
            }
            // transform source center (in corner reference frame)
            center = glm::scale(glm::identity<glm::mat4>(), corner_scaling) * center;
            // convert center back into scene reference frame
            center = corner_to_scene_transform * center;
            // apply to node
            sourceNode->translation_ = glm::vec3(center);
            // show cursor depending on diagonal (corner picked)
            T = glm::rotate(glm::identity<glm::mat4>(), s->stored_status_->rotation_.z, glm::vec3(0.f, 0.f, 1.f));
            T = glm::scale(T, s->stored_status_->scale_);
            corner = T * glm::vec4( corner, 0.f, 0.f );
            ret.type = corner.x * corner.y > 0.f ? Cursor_ResizeNESW : Cursor_ResizeNWSE;
            info << "Texture scale " << std::fixed << std::setprecision(3) << sourceNode->scale_.x;
            info << " x "  << sourceNode->scale_.y;

        }
        // picking on the BORDER RESIZING handles left or right
        else if ( pick.first == s->handles_[mode_][Handles::RESIZE_H] ) {

            // hide all other grips
            s->handles_[mode_][Handles::RESIZE]->visible_ = false;
            s->handles_[mode_][Handles::SCALE]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_V]->visible_ = false;
            s->handles_[mode_][Handles::ROTATE]->visible_ = false;
            s->handles_[mode_][Handles::MENU]->visible_ = false;

            // inform on which corner should be overlayed (opposite)
            s->handles_[mode_][Handles::RESIZE_H]->overlayActiveCorner(-corner);

            // SHIFT: HORIZONTAL SCALE to restore source aspect ratio
            if (UserInterface::manager().shiftModifier()) {
                sourceNode->scale_.x = ABS(sourceNode->scale_.y) * SIGN(sourceNode->scale_.x);
                corner_scaling = sourceNode->scale_ / s->stored_status_->scale_;
            }
            // HORIZONTAL RESIZE (normal case)
            else {
                // x scale only
                corner_scaling = glm::vec3(corner_scaling.x, 1.f, 1.f);
                // scale node
                sourceNode->scale_ = s->stored_status_->scale_ * corner_scaling;
                // POST-CORRECTION ; discretized scaling with ALT
                if (UserInterface::manager().altModifier()) {
                    sourceNode->scale_.x = ROUND(sourceNode->scale_.x, 10.f);
                    corner_scaling = sourceNode->scale_ / s->stored_status_->scale_;
                }
            }
            // transform source center (in corner reference frame)
            center = glm::scale(glm::identity<glm::mat4>(), corner_scaling) * center;
            // convert center back into scene reference frame
            center = corner_to_scene_transform * center;
            // apply to node
            sourceNode->translation_ = glm::vec3(center);
            // show cursor depending on angle
            float c = tan(sourceNode->rotation_.z);
            ret.type = ABS(c) > 1.f ? Cursor_ResizeNS : Cursor_ResizeEW;
            info << "Texture Scale " << std::fixed << std::setprecision(3) << sourceNode->scale_.x;
            info << " x "  << sourceNode->scale_.y;
        }
        // picking on the BORDER RESIZING handles top or bottom
        else if ( pick.first == s->handles_[mode_][Handles::RESIZE_V] ) {

            // hide all other grips
            s->handles_[mode_][Handles::RESIZE]->visible_ = false;
            s->handles_[mode_][Handles::SCALE]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_H]->visible_ = false;
            s->handles_[mode_][Handles::ROTATE]->visible_ = false;
            s->handles_[mode_][Handles::MENU]->visible_ = false;

            // inform on which corner should be overlayed (opposite)
            s->handles_[mode_][Handles::RESIZE_V]->overlayActiveCorner(-corner);

            // SHIFT: VERTICAL SCALE to restore source aspect ratio
            if (UserInterface::manager().shiftModifier()) {
                sourceNode->scale_.y = ABS(sourceNode->scale_.x) * SIGN(sourceNode->scale_.y);
                corner_scaling = sourceNode->scale_ / s->stored_status_->scale_;
            }
            // VERTICAL RESIZE (normal case)
            else {
                // y scale only
                corner_scaling = glm::vec3(1.f, corner_scaling.y, 1.f);
                // scale node
                sourceNode->scale_ = s->stored_status_->scale_ * corner_scaling;
                // POST-CORRECTION ; discretized scaling with ALT
                if (UserInterface::manager().altModifier()) {
                    sourceNode->scale_.y = ROUND(sourceNode->scale_.y, 10.f);
                    corner_scaling = sourceNode->scale_ / s->stored_status_->scale_;
                }
            }
            // transform source center (in corner reference frame)
            center = glm::scale(glm::identity<glm::mat4>(), corner_scaling) * center;
            // convert center back into scene reference frame
            center = corner_to_scene_transform * center;
            // apply to node
            sourceNode->translation_ = glm::vec3(center);
            // show cursor depending on angle
            float c = tan(sourceNode->rotation_.z);
            ret.type = ABS(c) > 1.f ? Cursor_ResizeEW : Cursor_ResizeNS;
            info << "Texture Scale " << std::fixed << std::setprecision(3) << sourceNode->scale_.x;
            info << " x "  << sourceNode->scale_.y;
        }
        // picking on the CENTRER SCALING handle
        else if ( pick.first == s->handles_[mode_][Handles::SCALE] ) {

            // hide all other grips
            s->handles_[mode_][Handles::RESIZE]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_H]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_V]->visible_ = false;
            s->handles_[mode_][Handles::ROTATE]->visible_ = false;
            s->handles_[mode_][Handles::MENU]->visible_ = false;

            // prepare overlay
            overlay_scaling_cross_->visible_ = false;
            overlay_scaling_grid_->visible_ = false;
            overlay_scaling_->visible_ = true;
            overlay_scaling_->translation_.x = s->stored_status_->translation_.x;
            overlay_scaling_->translation_.y = s->stored_status_->translation_.y;
            overlay_scaling_->rotation_.z = s->stored_status_->rotation_.z;
            overlay_scaling_->update(0);

            // PROPORTIONAL ONLY
            if (UserInterface::manager().shiftModifier()) {
                float factor = glm::length( glm::vec2( source_to ) ) / glm::length( glm::vec2( source_from ) );
                source_scaling = glm::vec3(factor, factor, 1.f);
                overlay_scaling_cross_->visible_ = true;
                overlay_scaling_cross_->copyTransform(overlay_scaling_);
            }
            // apply center scaling
            sourceNode->scale_ = s->stored_status_->scale_ * source_scaling;
            // POST-CORRECTION ; discretized scaling with ALT
            if (UserInterface::manager().altModifier()) {
                sourceNode->scale_.x = ROUND(sourceNode->scale_.x, 10.f);
                sourceNode->scale_.y = ROUND(sourceNode->scale_.y, 10.f);
                overlay_scaling_grid_->visible_ = true;
                overlay_scaling_grid_->copyTransform(overlay_scaling_);
            }
            // show cursor depending on diagonal
            corner = glm::sign(sourceNode->scale_);
            ret.type = (corner.x * corner.y) > 0.f ? Cursor_ResizeNWSE : Cursor_ResizeNESW;
            info << "Texture Scale " << std::fixed << std::setprecision(3) << sourceNode->scale_.x;
            info << " x "  << sourceNode->scale_.y;
        }
        // picking on the rotating handle
        else if ( pick.first == s->handles_[mode_][Handles::ROTATE] ) {

            // hide all other grips
            s->handles_[mode_][Handles::RESIZE]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_H]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_V]->visible_ = false;
            s->handles_[mode_][Handles::SCALE]->visible_ = false;
            s->handles_[mode_][Handles::MENU]->visible_ = false;
            // ROTATION on CENTER
            overlay_rotation_->visible_ = true;
            overlay_rotation_->translation_.x = s->stored_status_->translation_.x;
            overlay_rotation_->translation_.y = s->stored_status_->translation_.y;
            overlay_rotation_->update(0);
            overlay_rotation_fix_->visible_ = true;
            overlay_rotation_fix_->copyTransform(overlay_rotation_);
            overlay_rotation_clock_->visible_ = false;

            // rotation center to center of source (disregarding scale)
            glm::mat4 T = glm::translate(glm::identity<glm::mat4>(), s->stored_status_->translation_);
            source_from = glm::inverse(T) * glm::vec4( scene_from,  1.f );
            source_to   = glm::inverse(T) * glm::vec4( scene_to,  1.f );
            // compute rotation angle
            float angle = glm::orientedAngle( glm::normalize(glm::vec2(source_from)), glm::normalize(glm::vec2(source_to)));
            // apply rotation on Z axis
            sourceNode->rotation_ = s->stored_status_->rotation_ + glm::vec3(0.f, 0.f, angle);

            // POST-CORRECTION ; discretized rotation with ALT
            int degrees = int(  glm::degrees(sourceNode->rotation_.z) );
            if (UserInterface::manager().altModifier()) {
                degrees = (degrees / 10) * 10;
                sourceNode->rotation_.z = glm::radians( float(degrees) );
                overlay_rotation_clock_->visible_ = true;
                overlay_rotation_clock_->copyTransform(overlay_rotation_);
                info << "Texture Angle " << degrees << "\u00b0"; // degree symbol
            }
            else
                info << "Texture Angle " << std::fixed << std::setprecision(1) << glm::degrees(sourceNode->rotation_.z) << "\u00b0"; // degree symbol

            overlay_rotation_clock_hand_->visible_ = true;
            overlay_rotation_clock_hand_->translation_.x = s->stored_status_->translation_.x;
            overlay_rotation_clock_hand_->translation_.y = s->stored_status_->translation_.y;
            overlay_rotation_clock_hand_->rotation_.z = sourceNode->rotation_.z;
            overlay_rotation_clock_hand_->update(0);

            // show cursor for rotation
            ret.type = Cursor_Hand;
            // + SHIFT = no scaling /  NORMAL = with scaling
            if (!UserInterface::manager().shiftModifier()) {
                // compute scaling to match cursor
                float factor = glm::length( glm::vec2( source_to ) ) / glm::length( glm::vec2( source_from ) );
                source_scaling = glm::vec3(factor, factor, 1.f);
                // apply center scaling
                sourceNode->scale_ = s->stored_status_->scale_ * source_scaling;
                info << std::endl << "          Scale " << std::fixed << std::setprecision(3) << sourceNode->scale_.x;
                info << " x "  << sourceNode->scale_.y ;
                overlay_rotation_fix_->visible_ = false;
            }
        }
        // picking anywhere but on a handle: user wants to move the source
        else {
            ret.type = Cursor_ResizeAll;
            sourceNode->translation_ = s->stored_status_->translation_ + scene_translation;
            // discretized translation with ALT
            if (UserInterface::manager().altModifier()) {
                sourceNode->translation_.x = ROUND(sourceNode->translation_.x, 10.f);
                sourceNode->translation_.y = ROUND(sourceNode->translation_.y, 10.f);
            }
            // ALT: single axis movement
            overlay_position_cross_->visible_ = false;
            if (UserInterface::manager().shiftModifier()) {
                overlay_position_cross_->visible_ = true;
                overlay_position_cross_->translation_.x = s->stored_status_->translation_.x;
                overlay_position_cross_->translation_.y = s->stored_status_->translation_.y;
                overlay_position_cross_->update(0);

                glm::vec3 dif = s->stored_status_->translation_ - sourceNode->translation_;
                if (ABS(dif.x) > ABS(dif.y) ) {
                    sourceNode->translation_.y = s->stored_status_->translation_.y;
                    ret.type = Cursor_ResizeEW;
                } else {
                    sourceNode->translation_.x = s->stored_status_->translation_.x;
                    ret.type = Cursor_ResizeNS;
                }
            }
            // Show center overlay for POSITION
            overlay_position_->visible_ = true;
            overlay_position_->translation_.x = sourceNode->translation_.x;
            overlay_position_->translation_.y = sourceNode->translation_.y;
            overlay_position_->update(0);
            // Show move cursor
            info << "Texture Shift " << std::fixed << std::setprecision(3) << sourceNode->translation_.x;
            info << ", "  << sourceNode->translation_.y ;
        }
    }

    // request update
    s->touch();

    // store action in history
    current_action_ = s->name() + ": " + info.str();
    current_id_ = s->id();

    // update cursor
    ret.info = info.str();
    return ret;
}


void AppearanceView::terminate()
{
    View::terminate();

    // hide all overlays
    overlay_position_->visible_       = false;
    overlay_position_cross_->visible_ = false;
    overlay_scaling_grid_->visible_   = false;
    overlay_scaling_cross_->visible_  = false;
    overlay_scaling_->visible_        = false;
    overlay_rotation_clock_->visible_ = false;
    overlay_rotation_clock_hand_->visible_ = false;
    overlay_rotation_fix_->visible_   = false;
    overlay_rotation_->visible_       = false;

    // cancel of all handles overlays
    glm::vec2 c(0.f, 0.f);
    for (auto sit = Mixer::manager().session()->begin();
         sit != Mixer::manager().session()->end(); sit++){

        (*sit)->handles_[mode_][Handles::RESIZE]->overlayActiveCorner(c);
        (*sit)->handles_[mode_][Handles::RESIZE_H]->overlayActiveCorner(c);
        (*sit)->handles_[mode_][Handles::RESIZE_V]->overlayActiveCorner(c);
        (*sit)->handles_[mode_][Handles::RESIZE]->visible_ = true;
        (*sit)->handles_[mode_][Handles::RESIZE_H]->visible_ = true;
        (*sit)->handles_[mode_][Handles::RESIZE_V]->visible_ = true;
        (*sit)->handles_[mode_][Handles::SCALE]->visible_ = true;
        (*sit)->handles_[mode_][Handles::ROTATE]->visible_ = true;
        (*sit)->handles_[mode_][Handles::MENU]->visible_ = true;
    }

}

View::Cursor AppearanceView::drag (glm::vec2 from, glm::vec2 to)
{
    Cursor ret = View::drag(from, to);

    // Clamp translation to acceptable area
    scene.root()->translation_ = glm::clamp(scene.root()->translation_, glm::vec3(-3.f, -1.5f, 0.f), glm::vec3(3.f, 1.5f, 0.f));

    return ret;
}
