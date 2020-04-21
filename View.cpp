// Opengl
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "defines.h"
#include "View.h"
#include "Primitives.h"
#include "Mesh.h"
#include "FrameBuffer.h"
#include "Log.h"


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
    // Mixing scene
    Mesh *disk = new Mesh("mesh/disk.ply", "images/transparencygrid.png");
    backgound_.addChild(disk);

    glm::vec4 pink( 0.8f, 0.f, 0.8f, 1.f );
    LineCircle *circle = new LineCircle(pink, 5);
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


void MixingView::grab (glm::vec2 from, glm::vec2 to, Node *node)
{
    static glm::vec3 start_translation = glm::vec3(0.f);
    static glm::vec2 start_position = glm::vec2(0.f);

    if ( start_position != from ) {
        start_position = from;
        start_translation = node->translation_;
    }

    // unproject
    glm::vec3 gl_Position_from = Rendering::manager().unProject(from, node->transform_);
    glm::vec3 gl_Position_to = Rendering::manager().unProject(to, node->transform_);

    // compute delta translation
    node->translation_ = start_translation + gl_Position_to - gl_Position_from;


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

