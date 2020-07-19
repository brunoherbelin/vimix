// Opengl
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/vector_angle.hpp>


#include "imgui.h"
#include "ImGuiToolkit.h"

// memmove
#include <string.h>
#include <sstream>
#include <iomanip>

#include "View.h"
#include "defines.h"
#include "Settings.h"
#include "Session.h"
#include "Source.h"
#include "SessionSource.h"
#include "PickingVisitor.h"
#include "BoundingBoxVisitor.h"
#include "DrawVisitor.h"
#include "Mesh.h"
#include "Mixer.h"
#include "UserInterfaceManager.h"
#include "UpdateCallback.h"
#include "Log.h"

#define CIRCLE_PIXELS 64
#define CIRCLE_PIXEL_RADIUS 1024.0

bool View::need_deep_update_ = true;

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
    if (View::need_deep_update_) {
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
    if ( !pv.picked().empty()) {

        // select top-most Node picked
        pick = pv.picked().back();

    }
    return pick;
}

void View::initiate()
{
    for (auto sit = Mixer::manager().session()->begin();
         sit != Mixer::manager().session()->end(); sit++){

        (*sit)->stored_status_->copyTransform((*sit)->group(mode_));
    }
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
        glm::mat4 modelview = GlmToolkit::transform(scene.root()->translation_, scene.root()->rotation_, scene.root()->scale_);
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
    if ( !pv.picked().empty()) {

        // create a list of source matching the list of picked nodes
        SourceList selection;
        std::vector< std::pair<Node *, glm::vec2> > pick = pv.picked();
        // loop over the nodes and add all sources found.
        for(std::vector< std::pair<Node *, glm::vec2> >::iterator p = pick.begin(); p != pick.end(); p++){
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
        scene.root()->translation_ = glm::vec3(1.0f, 0.0f, 0.0f);
        saveSettings();
    }
    else
        restoreSettings();

    // Mixing scene background
    Mesh *disk = new Mesh("mesh/disk.ply");
    disk->scale_ = glm::vec3(limbo_scale_, limbo_scale_, 1.f);
    disk->shader()->color = glm::vec4( COLOR_LIMBO_CIRCLE, 0.6f );
    scene.bg()->attach(disk);

    disk = new Mesh("mesh/disk.ply");
    disk->setTexture(textureMixingQuadratic());
    scene.bg()->attach(disk);

    Mesh *circle = new Mesh("mesh/circle.ply");
    circle->shader()->color = glm::vec4( COLOR_FRAME, 0.9f );
    scene.bg()->attach(circle);

}


void MixingView::zoom( float factor )
{
    float z = scene.root()->scale_.x;
    z = CLAMP( z + 0.1f * factor, MIXING_MIN_SCALE, MIXING_MAX_SCALE);
    scene.root()->scale_.x = z;
    scene.root()->scale_.y = z;
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

View::Cursor MixingView::grab (Source *s, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2>)
{
    if (!s)
        return Cursor();

    // unproject
    glm::vec3 gl_Position_from = Rendering::manager().unProject(from, scene.root()->transform_);
    glm::vec3 gl_Position_to   = Rendering::manager().unProject(to, scene.root()->transform_);

    // compute delta translation
    s->group(mode_)->translation_ = s->stored_status_->translation_ + gl_Position_to - gl_Position_from;

    // request update
    s->touch();

    std::ostringstream info;
    if (s->active())
        info << "Alpha " << std::fixed << std::setprecision(3) << s->blendingShader()->color.a;
    else
        info << "Inactive";

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
    glm::vec2 mix_pos = glm::vec2(sourceNode->translation_);

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

uint MixingView::textureMixingQuadratic()
{
    static GLuint texid = 0;
    if (texid == 0) {
        // generate the texture with alpha exactly as computed for sources
        glGenTextures(1, &texid);
        glBindTexture(GL_TEXTURE_2D, texid);
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
                distance = (GLfloat) ((c * c) + (l * l)) / CIRCLE_PIXEL_RADIUS;
//                distance = (GLfloat) sqrt( (GLfloat) ((c * c) + (l * l))) / (GLfloat) sqrt(CIRCLE_PIXEL_RADIUS); // linear
                // luminance
                luminance = 255.f * CLAMP( 0.95f - 0.8f * distance, 0.f, 1.f);
                color[0] = color[1] = color[2] = static_cast<GLubyte>(luminance);
                // alpha
                alpha = 255.f * CLAMP( 1.f - distance , 0.f, 1.f);
                color[3] = static_cast<GLubyte>(alpha);

                // 1st quadrant
                memmove(&matrix[ j * 4 + i * CIRCLE_PIXELS * 4 ], color, 4);
                // 4nd quadrant
                memmove(&matrix[ (CIRCLE_PIXELS -j -1)* 4 + i * CIRCLE_PIXELS * 4 ], color, 4);
                // 3rd quadrant
                memmove(&matrix[ j * 4 + (CIRCLE_PIXELS -i -1) * CIRCLE_PIXELS * 4 ], color, 4);
                // 4th quadrant
                memmove(&matrix[ (CIRCLE_PIXELS -j -1) * 4 + (CIRCLE_PIXELS -i -1) * CIRCLE_PIXELS * 4 ], color, 4);

                ++c;
            }
            ++l;
        }
        // two components texture : luminance and alpha
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, CIRCLE_PIXELS, CIRCLE_PIXELS, 0, GL_RGBA, GL_UNSIGNED_BYTE, (float *) matrix);

    }
    return texid;
}

RenderView::RenderView() : View(RENDERING), frame_buffer_(nullptr), fading_overlay_(nullptr)
{
    // set resolution to settings or default
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
    if (resolution.x < 128.f || resolution.y < 128.f)
        resolution = FrameBuffer::getResolutionFromParameters(Settings::application.render.ratio, Settings::application.render.res);

    // new frame buffer
    if (frame_buffer_)
        delete frame_buffer_;

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

    Frame *border = new Frame(Frame::SHARP, Frame::THIN, Frame::NONE);
    border->color = glm::vec4( COLOR_FRAME, 1.f );
    scene.fg()->attach(border);

    // selection box
//    selection_box_ = new Box;
//    selection_box_->visible_ = false;
//    scene.ws()->attach(selection_box_);
}

void GeometryView::update(float dt)
{
    View::update(dt);

    // a more complete update is requested
    if (View::need_deep_update_) {

        // update rendering of render frame
        FrameBuffer *output = Mixer::manager().session()->frame();
        if (output){
            for (NodeSet::iterator node = scene.bg()->begin(); node != scene.bg()->end(); node++) {
                (*node)->scale_.x = output->aspectRatio();
            }
            for (NodeSet::iterator node = scene.fg()->begin(); node != scene.fg()->end(); node++) {
                (*node)->scale_.x = output->aspectRatio();
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
    scene.root()->draw(glm::identity<glm::mat4>(), Rendering::manager().Projection());

    // re-draw overlay of current source on top
    // (allows manipulation current source even when hidden below others)
    if (s != nullptr) {
        s->setMode(Source::CURRENT);
        DrawVisitor dv(s->overlays_[mode_], Rendering::manager().Projection());
        scene.accept(dv);
    }
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
    if ( pv.picked().size() > 0) {

        Source *s = Mixer::manager().currentSource();
        if (s != nullptr) {

            // find if the current source was picked
            auto itp = pv.picked().rbegin();
            for (; itp != pv.picked().rend(); itp++){
                if ( s->contains( (*itp).first ) ){
                    pick = *itp;
                    break;
                }
            }
            // not found: the current source was not clicked
            if (itp == pv.picked().rend())
                s = nullptr;
        }
        // maybe the source changed
        if (s == nullptr)
        {
            // select top-most Node picked
            pick = pv.picked().back();
        }
    }

    return pick;
}

View::Cursor GeometryView::grab (Source *s, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2> pick)
{
    View::Cursor ret = Cursor();

    std::ostringstream info;

    // work on the given source
    if (!s)
        return ret;
    Group *sourceNode = s->group(mode_);

    // grab coordinates in scene-View reference frame
    glm::vec3 gl_Position_from = Rendering::manager().unProject(from, scene.root()->transform_);
    glm::vec3 gl_Position_to   = Rendering::manager().unProject(to, scene.root()->transform_);

    // grab coordinates in source-root reference frame
    glm::vec4 S_from = glm::inverse(sourceNode->transform_) * glm::vec4( gl_Position_from,  1.f );
    glm::vec4 S_to   = glm::inverse(sourceNode->transform_) * glm::vec4( gl_Position_to,  1.f );
    glm::vec3 S_resize = glm::vec3(S_to) / glm::vec3(S_from);

//    Log::Info(" screen coordinates ( %.1f, %.1f ) ", to.x, to.y);
//    Log::Info(" scene  coordinates ( %.1f, %.1f ) ", gl_Position_to.x, gl_Position_to.y);
//    Log::Info(" source coordinates ( %.1f, %.1f, %.1f ) ", S_from.x, S_from.y, S_from.z);
//    Log::Info("                    ( %.1f, %.1f, %.1f ) ", S_to.x, S_to.y, S_to.z);

    // which manipulation to perform?
    if (pick.first)  {
        // picking on the resizing handles in the corners
        if ( pick.first == s->handle_[Handles::RESIZE] ) {
            if (UserInterface::manager().keyboardModifier())
                S_resize.y = S_resize.x;
            sourceNode->scale_ = s->stored_status_->scale_ * S_resize;

//            Log::Info(" resize            ( %.1f, %.1f ) ", S_resize.x, S_resize.y);
//            glm::vec3 factor = S_resize * glm::vec3(0.5f, 0.5f, 1.f);
////            glm::vec3 factor = S_resize * glm::vec3(1.f, 1.f, 1.f);
////            factor *= glm::sign( glm::vec3(pick.second, 1.f) );
//            sourceNode->scale_ = start_scale + factor;

//            sourceNode->translation_ = start_translation + factor;
////            sourceNode->translation_ = start_translation + S_resize * factor;

            // select cursor depending on diagonal
            glm::vec2 axis = glm::sign(pick.second);
            ret.type = axis.x * axis.y > 0.f ? Cursor_ResizeNESW : Cursor_ResizeNWSE;
            info << "Size " << std::fixed << std::setprecision(3) << sourceNode->scale_.x;
            info << " x "  << sourceNode->scale_.y;
        }
        // picking on the resizing handles left or right
        else if ( pick.first == s->handle_[Handles::RESIZE_H] ) {
            sourceNode->scale_ = s->stored_status_->scale_ * glm::vec3(S_resize.x, 1.f, 1.f);
            if (UserInterface::manager().keyboardModifier())
                sourceNode->scale_.x = float( int( sourceNode->scale_.x * 10.f ) ) / 10.f;

            ret.type = Cursor_ResizeEW;
            info << "Size " << std::fixed << std::setprecision(3) << sourceNode->scale_.x;
            info << " x "  << sourceNode->scale_.y;
        }
        // picking on the resizing handles top or bottom
        else if ( pick.first == s->handle_[Handles::RESIZE_V] ) {
            sourceNode->scale_ = s->stored_status_->scale_ * glm::vec3(1.f, S_resize.y, 1.f);
            if (UserInterface::manager().keyboardModifier())
                sourceNode->scale_.y = float( int( sourceNode->scale_.y * 10.f ) ) / 10.f;

            ret.type = Cursor_ResizeNS;
            info << "Size " << std::fixed << std::setprecision(3) << sourceNode->scale_.x;
            info << " x "  << sourceNode->scale_.y;
        }
        // picking on the rotating handle
        else if ( pick.first == s->handle_[Handles::ROTATE] ) {
            // rotation center to center of source
            glm::mat4 T = glm::translate(glm::identity<glm::mat4>(), s->stored_status_->translation_);
            S_from = glm::inverse(T) * glm::vec4( gl_Position_from,  1.f );
            S_to   = glm::inverse(T) * glm::vec4( gl_Position_to,  1.f );
            // angle
            float angle = glm::orientedAngle( glm::normalize(glm::vec2(S_from)), glm::normalize(glm::vec2(S_to)));
            // apply rotation on Z axis
            sourceNode->rotation_ = s->stored_status_->rotation_ + glm::vec3(0.f, 0.f, angle);

            int degrees = int(  glm::degrees(sourceNode->rotation_.z) );
            if (UserInterface::manager().keyboardModifier()) {
                degrees = (degrees / 10) * 10;
                sourceNode->rotation_.z = glm::radians( float(degrees) );
            }

            ret.type = Cursor_Hand;
            info << "Angle " << degrees << "\u00b0"; // degree symbol
        }
        // picking anywhere but on a handle: user wants to move the source
        else {
            sourceNode->translation_ = s->stored_status_->translation_ + gl_Position_to - gl_Position_from;

            if (UserInterface::manager().keyboardModifier()) {
                sourceNode->translation_.x = float( int( sourceNode->translation_.x * 10.f ) ) / 10.f;
                sourceNode->translation_.y = float( int( sourceNode->translation_.y * 10.f ) ) / 10.f;
            }

            ret.type = Cursor_ResizeAll;
            info << "Position (" << std::fixed << std::setprecision(3) << sourceNode->translation_.x;
            info << ", "  << sourceNode->translation_.y << ")";
        }
    }
//    // don't have a handle, we can only move the source
//    else {
//        sourceNode->translation_ = start_translation + gl_Position_to - gl_Position_from;
//        ret.type = Cursor_ResizeAll;
//        info << "Position (" << std::fixed << std::setprecision(3) << sourceNode->translation_.x;
//        info << ", "  << sourceNode->translation_.y << ")";
//    }

    // request update
    s->touch();

    ret.info = info.str();
    return ret;
}

View::Cursor GeometryView::over (Source*, glm::vec2, std::pair<Node *, glm::vec2>)
{    
    View::Cursor ret = Cursor_Arrow;

    return ret;
}

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
    if (View::need_deep_update_) {

        // update rendering of render frame
        FrameBuffer *output = Mixer::manager().session()->frame();
        if (output){
            aspect_ratio = output->aspectRatio();
            for (NodeSet::iterator node = scene.bg()->begin(); node != scene.bg()->end(); node++) {
                (*node)->scale_.x = aspect_ratio;
            }
            for (NodeSet::iterator node = scene.ws()->begin(); node != scene.ws()->end(); node++) {
                (*node)->translation_.y = (*node)->translation_.x / aspect_ratio;
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


float LayerView::setDepth(Source *s, float d)
{
    if (!s)
        return -1.f;

    float depth = d;

    // negative depth given; find the front most depth
    if ( depth < 0.f ) {
        Node *front = scene.ws()->front();
        if (front)
            depth = front->translation_.z + 0.5f;
        else
            depth = 0.5f;
    }

    // move the layer node of the source
    Group *sourceNode = s->group(mode_);

    // diagonal movement only
    sourceNode->translation_.x = CLAMP( -depth, -(SCENE_DEPTH - 2.f), 0.f);
    sourceNode->translation_.y = sourceNode->translation_.x / aspect_ratio;

    // change depth
    sourceNode->translation_.z = -sourceNode->translation_.x;

    // request reordering of scene at next update
    View::need_deep_update_ = true;

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
    return Cursor(Cursor_ResizeAll, info.str() );
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
        scene.root()->scale_ = glm::vec3(5.f, 5.f, 1.0f);
        scene.root()->translation_ = glm::vec3(1.8f, 0.f, 0.0f);
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

            // reset / no fading
            Mixer::manager().session()->setFading( 0.f );
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
                f = ( 2 * d + 1.f);  // quadratic
                f *= f;
            }
            Mixer::manager().session()->setFading( 1.f - f );
        }

        // request update
        transition_source_->touch();

        if (d > 0.2f && Settings::application.transition.auto_open)
            Mixer::manager().setView(View::MIXING);

    }

    // update scene
    View::update(dt);

    // a more complete update is requested
    if  (View::need_deep_update_) {

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
        ImGui::DragFloat("##nolabel", &Settings::application.transition.duration,
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
    }
}

Session *TransitionView::detach()
{
    // by default, nothing to return
    Session *ret = nullptr;

    if ( transition_source_ != nullptr) {

        // get and detatch the group node from the view workspace
        Group *tg = transition_source_->group(View::TRANSITION);
        scene.ws()->detatch( tg );

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

    std::ostringstream info;

    // unproject
    glm::vec3 gl_Position_from = Rendering::manager().unProject(from, scene.root()->transform_);
    glm::vec3 gl_Position_to   = Rendering::manager().unProject(to, scene.root()->transform_);

    // compute delta translation
    float d = s->stored_status_->translation_.x + gl_Position_to.x - gl_Position_from.x;
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


