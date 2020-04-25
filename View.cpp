// Opengl
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// memmove
#include <string.h>

#include "defines.h"
#include "View.h"
#include "Source.h"
#include "Primitives.h"
#include "Resource.h"
#include "Mesh.h"
#include "FrameBuffer.h"
#include "Log.h"

#define CIRCLE_PIXELS 64
#define CIRCLE_PIXEL_RADIUS 1024.0

View::View()
{
}

void View::update(float dt)
{
    // recursive update from root of scene
    scene.root()->update( dt );
}

MixingView::MixingView() : View()
{
    // default settings
    scene.root()->scale_ = glm::vec3(1.4, 1.4, 1.0);

    // Mixing scene
    Mesh *disk = new Mesh("mesh/disk.ply");
    disk->setTexture(textureMixingQuadratic());
    backgound_.addChild(disk);

    glm::vec4 pink( 0.8f, 0.f, 0.8f, 1.f );
//    LineCircle *circle = new LineCircle(pink, 5);
    Mesh *circle = new Mesh("mesh/circle.ply");
    circle->shader()->color = pink;
    backgound_.addChild(circle);

    scene.root()->addChild(&backgound_);
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
    glm::vec3 gl_Position_from = Rendering::manager().unProject(from, sourceNode->parent_->transform_);
    glm::vec3 gl_Position_to = Rendering::manager().unProject(to, sourceNode->parent_->transform_);

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

RenderView::RenderView() : View(), frame_buffer_(nullptr)
{
    setResolution(1280, 720);
}

RenderView::~RenderView()
{
    if (frame_buffer_)
        delete frame_buffer_;
}


void RenderView::setResolution(uint width, uint height)
{
    if (frame_buffer_)
        delete frame_buffer_;

    frame_buffer_ = new FrameBuffer(width, height);
}

void RenderView::draw()
{
    static glm::mat4 projection = glm::ortho(-SCENE_UNIT, SCENE_UNIT, -SCENE_UNIT, SCENE_UNIT, SCENE_DEPTH, 0.f);
    glm::mat4 P  = glm::scale( projection, glm::vec3(1.f / frame_buffer_->aspectRatio(), 1.f, 1.f));
    frame_buffer_->begin();
    scene.root()->draw(glm::identity<glm::mat4>(), P);
    frame_buffer_->end();
}

