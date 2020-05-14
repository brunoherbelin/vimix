// Opengl
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// memmove
#include <string.h>

#include "defines.h"
#include "Settings.h"
#include "View.h"
#include "Source.h"
#include "Primitives.h"
#include "PickingVisitor.h"
#include "Resource.h"
#include "Mesh.h"
#include "Mixer.h"
#include "FrameBuffer.h"
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


void MixingView::grab (glm::vec2 from, glm::vec2 to, Source *s)
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
    if (resolution.x < 100.f || resolution.y < 100)
        resolution = FrameBuffer::getResolutionFromParameters(Settings::application.framebuffer_ar, Settings::application.framebuffer_h);

    if (frame_buffer_)
        delete frame_buffer_;

    frame_buffer_ = new FrameBuffer(resolution);
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
    border->overlay_ = new Mesh("mesh/border_vertical_overlay.ply");
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


void GeometryView::grab (glm::vec2 from, glm::vec2 to, Source *s)
{
    if (!s)
        return;

    Group *sourceNode = s->group(View::GEOMETRY);

    static glm::vec3 start_translation = glm::vec3(0.f);
    static glm::vec3 start_scale = glm::vec3(0.f);
    static glm::vec2 start_position = glm::vec2(0.f);


    if ( start_position != from ) {
        start_position = from;
        start_translation = sourceNode->translation_;
        start_scale = sourceNode->scale_;
    }

    // unproject in scene coordinates
    glm::vec3 gl_Position_from = Rendering::manager().unProject(from, scene.root()->transform_);
    glm::vec3 gl_Position_to = Rendering::manager().unProject(to, scene.root()->transform_);

    // coordinate in source
    glm::mat4 modelview;
    glm::vec2 clicpos(0.f);
    PickingVisitor pv(gl_Position_to);
    sourceNode->accept(pv);
    if (!pv.picked().empty()){
        clicpos = pv.picked().back().second;
        modelview = pv.picked().back().first->transform_;
//        Log::Info("clic pos source %.2f. %.2f", clicpos.x, clicpos.y);
    }

//    glm::vec4 P = glm::inverse(sourceNode->transform_ * modelview) * glm::vec4( gl_Position_to,  1.f );


    if ( ABS(clicpos.x)>0.9f && ABS(clicpos.y)>0.9f )
    {
        // clic inside corner
        Log::Info("corner %.2f. %.2f", clicpos.x, clicpos.y);
//        Log::Info("       %.2f. %.2f", P.x, P.y);

//        glm::vec2 topos = clicpos;
//        PickingVisitor pv(gl_Position_from);
//        sourceNode->accept(pv);

//        if (!pv.picked().empty()){
//            topos = pv.picked().back().second;
////            Log::Info("scale %.2f. %.2f", topos.x, topos.y);
//        }

        glm::vec4 P = glm::inverse(sourceNode->transform_ * modelview) * glm::vec4( gl_Position_from,  1.f );


        sourceNode->scale_ = start_scale * (glm::vec3(clicpos, 1.f) / glm::vec3(P) );
        Log::Info("scale %.2f. %.2f", sourceNode->scale_.x, sourceNode->scale_.y);
        Log::Info("      %.2f. %.2f", start_scale.x, start_scale.y);
    }
    else if ( ABS(clicpos.x)>0.95f && ABS(clicpos.y)<0.05f )
    {
        // clic resize horizontal
        Log::Info("H resize %.2f. %.2f", clicpos.x, clicpos.y);
    }
    else if ( ABS(clicpos.x)<0.05f && ABS(clicpos.y)>0.95f )
    {
        // clic resize vertical
        Log::Info("V resize %.2f. %.2f", clicpos.x, clicpos.y);
    }
    else
        // clic inside source: compute delta translation
        sourceNode->translation_ = start_translation + gl_Position_to - gl_Position_from;

}

