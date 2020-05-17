// Opengl
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/vector_angle.hpp>

// memmove
#include <string.h>

#include "defines.h"
#include "Settings.h"
#include "View.h"
#include "Source.h"
#include "Primitives.h"
#include "PickingVisitor.h"
#include "Mesh.h"
#include "Mixer.h"
#include "FrameBuffer.h"
#include "UserInterfaceManager.h"
#include "Log.h"

#define CIRCLE_PIXELS 64
#define CIRCLE_PIXEL_RADIUS 1024.0

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

void View::update(float dt)
{
    // recursive update from root of scene
    scene.update( dt );
}

MixingView::MixingView() : View(MIXING)
{
    // read default settings
    if ( Settings::application.views[View::MIXING].name.empty() ) {
        // no settings found: store application default
        Settings::application.views[View::MIXING].name = "Mixing";
        scene.root()->scale_ = glm::vec3(2.0f, 2.0f, 1.0f);
        saveSettings();
    }
    else
        restoreSettings();

    // Mixing scene background

    Mesh *disk = new Mesh("mesh/disk.ply");
    disk->setTexture(textureMixingQuadratic());
    scene.bg()->attach(disk);

    glm::vec4 pink( 0.8f, 0.f, 0.8f, 1.f );
    Mesh *circle = new Mesh("mesh/circle.ply");
    circle->shader()->color = pink;
    scene.bg()->attach(circle);

}

MixingView::~MixingView()
{

}

void MixingView::draw()
{
    // draw scene of this view
    scene.root()->draw(glm::identity<glm::mat4>(), Rendering::manager().Projection());
}

void MixingView::zoom( float factor )
{
    float z = scene.root()->scale_.x;
    z = CLAMP( z + 0.1f * factor, 0.2f, 10.f);
    scene.root()->scale_.x = z;
    scene.root()->scale_.y = z;
}

void MixingView::drag (glm::vec2 from, glm::vec2 to)
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
}


void MixingView::grab (glm::vec2 from, glm::vec2 to, Source *s, std::pair<Node *, glm::vec2>)
{
    if (!s)
        return;

    Group *sourceNode = s->group(View::MIXING);

    static glm::vec3 start_translation = glm::vec3(0.f);
    static glm::vec2 start_position = glm::vec2(0.f);

    if ( start_position != from ) {
        start_position = from;
        start_translation = sourceNode->translation_;
    }

    // unproject
    glm::vec3 gl_Position_from = Rendering::manager().unProject(from, scene.root()->transform_);
    glm::vec3 gl_Position_to   = Rendering::manager().unProject(to, scene.root()->transform_);

    // compute delta translation
    sourceNode->translation_ = start_translation + gl_Position_to - gl_Position_from;

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
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, CIRCLE_PIXELS, CIRCLE_PIXELS, 0, GL_RGBA, GL_UNSIGNED_BYTE, (float *) matrix);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    }
    return texid;
}

RenderView::RenderView() : View(RENDERING), frame_buffer_(nullptr)
{
    // set resolution to settings or default
    setResolution();
}

RenderView::~RenderView()
{
    if (frame_buffer_)
        delete frame_buffer_;
}


void RenderView::setResolution(glm::vec3 resolution)
{
    if (resolution.x < 128.f || resolution.y < 128.f)
        resolution = FrameBuffer::getResolutionFromParameters(Settings::application.framebuffer_ar, Settings::application.framebuffer_h);

    if (frame_buffer_)
        delete frame_buffer_;

    frame_buffer_ = new FrameBuffer(resolution);
    frame_buffer_->setClearColor(glm::vec4(0.f, 0.f, 0.f, 1.f));
}

void RenderView::draw()
{
    static glm::mat4 projection = glm::ortho(-1.f, 1.f, -1.f, 1.f, SCENE_DEPTH, 0.f);
    glm::mat4 P  = glm::scale( projection, glm::vec3(1.f / frame_buffer_->aspectRatio(), 1.f, 1.f));
    frame_buffer_->begin();
    scene.root()->draw(glm::identity<glm::mat4>(), P);
    frame_buffer_->end();
}


GeometryView::GeometryView() : View(GEOMETRY)
{
    // read default settings
    if ( Settings::application.views[View::GEOMETRY].name.empty() ) {
        // no settings found: store application default
        Settings::application.views[View::GEOMETRY].name = "Geometry";
        scene.root()->scale_ = glm::vec3(1.2f, 1.2f, 1.0f);
        saveSettings();
    }
    else
        restoreSettings();

    // Geometry Scene background
    Surface *rect = new Surface;
    scene.bg()->attach(rect);

    Frame *border = new Frame(Frame::SHARP_THIN);
    border->color = glm::vec4( 0.8f, 0.f, 0.8f, 1.f );
    scene.bg()->attach(border);

}

GeometryView::~GeometryView()
{

}

void GeometryView::draw()
{
    // update rendering of render frame
    FrameBuffer *output = Mixer::manager().session()->frame();
    if (output){
        for (NodeSet::iterator node = scene.bg()->begin(); node != scene.bg()->end(); node++) {
            (*node)->scale_.x = output->aspectRatio();
        }
    }

    // draw scene of this view
    scene.root()->draw(glm::identity<glm::mat4>(), Rendering::manager().Projection());
}

void GeometryView::zoom( float factor )
{
    float z = scene.root()->scale_.x;
    z = CLAMP( z + 0.1f * factor, 0.2f, 10.f);
    scene.root()->scale_.x = z;
    scene.root()->scale_.y = z;
}

void GeometryView::drag (glm::vec2 from, glm::vec2 to)
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
}


void GeometryView::grab (glm::vec2 from, glm::vec2 to, Source *s, std::pair<Node *, glm::vec2> pick)
{
    // work on the given source
    if (!s)
        return;
    Group *sourceNode = s->group(View::GEOMETRY);

    // remember source transform at moment of clic at position 'from'
    static glm::vec2 start_clic_position = glm::vec2(0.f);
    static glm::vec3 start_translation = glm::vec3(0.f);
    static glm::vec3 start_scale = glm::vec3(1.f);
    static glm::vec3 start_rotation = glm::vec3(0.f);
    if ( start_clic_position != from ) {
        start_clic_position = from;
        start_translation = sourceNode->translation_;
        start_scale = sourceNode->scale_;
        start_rotation = sourceNode->rotation_;
    }

    // grab coordinates in scene-View reference frame
    glm::vec3 gl_Position_from = Rendering::manager().unProject(from, scene.root()->transform_);
    glm::vec3 gl_Position_to = Rendering::manager().unProject(to, scene.root()->transform_);

    // grab coordinates in source-root reference frame
    glm::vec4 S_from = glm::inverse(sourceNode->transform_) * glm::vec4( gl_Position_from,  1.f );
    glm::vec4 S_to = glm::inverse(sourceNode->transform_) * glm::vec4( gl_Position_to,  1.f );
    glm::vec3 S_resize = glm::vec3(S_to) / glm::vec3(S_from);

//    Log::Info(" screen coordinates ( %.1f, %.1f ) ", to.x, to.y);
//    Log::Info(" scene  coordinates ( %.1f, %.1f ) ", gl_Position_to.x, gl_Position_to.y);
//    Log::Info(" source coordinates ( %.1f, %.1f, %.1f ) ", S_from.x, S_from.y, S_from.z);
//    Log::Info("                    ( %.1f, %.1f, %.1f ) ", S_to.x, S_to.y, S_to.z);

    // which manipulation to perform?
    if (pick.first)  {
        // picking on the resizing handles in the corners
        if ( pick.first == s->handleNode(Handles::RESIZE) ) {
            if (UserInterface::manager().keyboardModifier())
                S_resize.y = S_resize.x;
            sourceNode->scale_ = start_scale * S_resize;
        }
        // picking on the resizing handles left or right
        else if ( pick.first == s->handleNode(Handles::RESIZE_H) ) {
            sourceNode->scale_ = start_scale * glm::vec3(S_resize.x, 1.f, 1.f);
        }
        // picking on the resizing handles top or bottom
        else if ( pick.first == s->handleNode(Handles::RESIZE_V) ) {
            sourceNode->scale_ = start_scale * glm::vec3(1.f, S_resize.y, 1.f);
        }
        // picking on the rotating handle
        else if ( pick.first == s->handleNode(Handles::ROTATE) ) {
            float angle = glm::orientedAngle( glm::normalize(glm::vec2(S_from)), glm::normalize(glm::vec2(S_to)));
            sourceNode->rotation_ = start_rotation + glm::vec3(0.f, 0.f, angle);
        }
        // picking anywhere but on a handle: user wants to move the source
        else {
            sourceNode->translation_ = start_translation + gl_Position_to - gl_Position_from;
        }
    }
    // don't have a handle, we can only move the source
    else {
        sourceNode->translation_ = start_translation + gl_Position_to - gl_Position_from;
    }

}


void LayersView::draw ()
{

}

void LayersView::zoom (float factor)
{

}

void LayersView::drag (glm::vec2 from, glm::vec2 to)
{

}

void LayersView::grab (glm::vec2 from, glm::vec2 to, Source *s, std::pair<Node *, glm::vec2> pick)
{

}

