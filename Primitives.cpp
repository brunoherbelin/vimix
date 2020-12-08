#include "Primitives.h"
#include "ImageShader.h"
#include "Resource.h"
#include "FrameBuffer.h"
#include "MediaPlayer.h"
#include "Visitor.h"
#include "Log.h"

#include <glad/glad.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>



Surface::Surface(Shader *s) : Primitive(s), textureindex_(0)
{
    // geometry for a trianglulated simple rectangle surface with UV
    //  (0,0) B +---+ D (1,0)
    //          |\  |
    //          | \ |
    //          |  \|
    //  (0,1) A +---+ C (1,1)

    points_ = std::vector<glm::vec3> { glm::vec3( -1.f, -1.f, 0.f ), glm::vec3( -1.f, 1.f, 0.f ),
            glm::vec3( 1.f, -1.f, 0.f ), glm::vec3( 1.f, 1.f, 0.f ) };
    colors_ = std::vector<glm::vec4> { glm::vec4( 1.f, 1.f, 1.f , 1.f ), glm::vec4(  1.f, 1.f, 1.f, 1.f  ),
            glm::vec4( 1.f, 1.f, 1.f, 1.f ), glm::vec4( 1.f, 1.f, 1.f, 1.f ) };
    texCoords_ = std::vector<glm::vec2> { glm::vec2( 0.f, 1.f ), glm::vec2( 0.f, 0.f ),
            glm::vec2( 1.f, 1.f ), glm::vec2( 1.f, 0.f ) };
    indices_ = std::vector<uint> { 0, 1, 2, 3 };
    drawMode_ = GL_TRIANGLE_STRIP;
}


Surface::~Surface()
{
    // do NOT delete vao_ (unique)
    vao_ = 0;
}

void Surface::init()
{
    // use static unique vertex array object
    static uint unique_vao_ = 0;
    static uint unique_drawCount = 0;
    if (unique_vao_) {
        // 1. only init Node (not the primitive vao)
        Node::init();
        // 2. use the global vertex array object
        vao_ = unique_vao_;
        drawCount_ = unique_drawCount;
        // compute AxisAlignedBoundingBox
        bbox_.extend(points_);
        // arrays of vertices are not needed anymore
        points_.clear();
        colors_.clear();
        texCoords_.clear();
        indices_.clear();
    }
    else {
        // 1. init the Primitive (only once)
        Primitive::init();
        // 2. remember global vertex array object
        unique_vao_ = vao_;
        unique_drawCount = drawCount_;
        // 3. unique_vao_ will NOT be deleted
    }

}

void Surface::accept(Visitor& v)
{
    Primitive::accept(v);
    v.visit(*this);
}

void Surface::draw(glm::mat4 modelview, glm::mat4 projection)
{
    if ( !initialized() )
        init();

    glActiveTexture(GL_TEXTURE0);
    if ( textureindex_ ) {
        glBindTexture(GL_TEXTURE_2D, textureindex_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,  GL_MIRRORED_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,  GL_MIRRORED_REPEAT);
//        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // TODO add user input to select mode
//        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
//        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,  GL_REPEAT);
//        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,  GL_REPEAT);
    }
    else
        glBindTexture(GL_TEXTURE_2D, Resource::getTextureBlack());

    Primitive::draw(modelview, projection);

    glBindTexture(GL_TEXTURE_2D, 0);
}

ImageSurface::ImageSurface(const std::string& path, Shader *s) : Surface(s), resource_(path)
{

}

void ImageSurface::init()
{
    Surface::init();

    // load image if specified (should always be the case)
    if ( !resource_.empty()) {
        textureindex_ = Resource::getTextureImage(resource_);
    }
}

void ImageSurface::accept(Visitor& v)
{
    Surface::accept(v);
    v.visit(*this);
}

MediaSurface::MediaSurface(const std::string& p, Shader *s) : Surface(s)
{
    path_ = p;
    mediaplayer_ = new MediaPlayer;
}

MediaSurface::~MediaSurface()
{
    delete mediaplayer_;
}

void MediaSurface::init()
{
    Surface::init();

    mediaplayer_->open(path_);
    mediaplayer_->play(true);
}

void MediaSurface::draw(glm::mat4 modelview, glm::mat4 projection)
{
    if ( !initialized() ) {
        init();
        // set the texture to the media player once openned
        if ( mediaplayer_->isOpen() )
            textureindex_ = mediaplayer_->texture();
    }

    Surface::draw(modelview, projection);
}

void MediaSurface::update( float dt )
{
    if ( mediaplayer_->isOpen() ) {
        mediaplayer_->update();
        scale_.x = mediaplayer_->aspectRatio();
    }

    Primitive::update( dt );
}

void MediaSurface::accept(Visitor& v)
{
    Surface::accept(v);
    v.visit(*this);
}


FrameBufferSurface::FrameBufferSurface(FrameBuffer *fb, Shader *s) : Surface(s), frame_buffer_(fb)
{

}

void FrameBufferSurface::init()
{
    Surface::init();

    // set aspect ratio
//    scale_.x = frame_buffer_->aspectRatio();

}

void FrameBufferSurface::draw(glm::mat4 modelview, glm::mat4 projection)
{
    if ( !initialized() )
        init();

    glBindTexture(GL_TEXTURE_2D, frame_buffer_->texture());

    Primitive::draw(modelview, projection);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void FrameBufferSurface::accept(Visitor& v)
{
    Surface::accept(v);
    v.visit(*this);
}


Points::Points(std::vector<glm::vec3> points, glm::vec4 color, uint pointsize) : Primitive(new Shader)
{
    for(size_t i = 0; i < points.size(); ++i)
    {
        points_.push_back( points[i] );
        colors_.push_back( color );
        indices_.push_back ( i );
    }

    drawMode_ = GL_POINTS; // TODO implement drawing of points as Mesh
    pointsize_ = pointsize;
}



void Points::draw(glm::mat4 modelview, glm::mat4 projection)
{
    if ( !initialized() )
        init();

    glPointSize(pointsize_);

    Primitive::draw(modelview, projection);

    glPointSize(1);
}

void Points::accept(Visitor& v)
{
    Primitive::accept(v);
    v.visit(*this);
}

LineStrip::LineStrip(std::vector<glm::vec3> points, std::vector<glm::vec4> colors, uint linewidth) : Primitive(new Shader), linewidth_(linewidth)
{
    for(size_t i = 0; i < points.size(); ++i)
    {
        points_.push_back( points[i] );
        colors_.push_back( colors[i] );
        indices_.push_back ( i );
    }

    drawMode_ = GL_LINE_STRIP;
}

void LineStrip::draw(glm::mat4 modelview, glm::mat4 projection)
{
    if ( !initialized() )
        init();

   // glLineWidth(linewidth_ * 2.f * Rendering::manager().mainWindow().dpiScale());

    glm::mat4 mv = modelview;
    glm::mat4 scale = glm::scale(glm::identity<glm::mat4>(), glm::vec3(1.001f, 1.001f, 1.f));

    // TODO FIXME drawing multiple times is not correct to draw lines of different width
    // TODO Draw LineStrip using polygons
    for (uint i = 0 ; i < linewidth_ ; ++i ) {
        Primitive::draw(mv, projection);
        mv *= scale;
    }

   // glLineWidth(1);
}

void LineStrip::accept(Visitor& v)
{
    Primitive::accept(v);
    v.visit(*this);
}



static const std::vector<glm::vec3> square_points {
            glm::vec3( -1.f, -1.f, 0.f ), glm::vec3( -1.f, 1.f, 0.f ),
            glm::vec3( 1.f, 1.f, 0.f ), glm::vec3( 1.f, -1.f, 0.f ),
            glm::vec3( -1.f, -1.f, 0.f )
};

static const std::vector<glm::vec4> square_colors {
            glm::vec4( 1.f, 1.f, 1.f, 1.f ), glm::vec4( 1.f, 1.f, 1.f, 1.f ),
            glm::vec4( 1.f, 1.f, 1.f, 1.f ), glm::vec4( 1.f, 1.f, 1.f, 1.f ),
            glm::vec4( 1.f, 1.f, 1.f, 1.f )
};

LineSquare::LineSquare(uint linewidth) : LineStrip(square_points, square_colors, linewidth)
{
}

void LineSquare::init()
{
    // use static unique vertex array object
    static uint unique_vao_ = 0;
    static uint unique_drawCount = 0;
    if (unique_vao_) {
        // 1. only init Node (not the primitive vao)
        Node::init();
        // 2. use the global vertex array object
        vao_ = unique_vao_;
        drawCount_ = unique_drawCount;
        // compute AxisAlignedBoundingBox
        bbox_.extend(points_);
        // arrays of vertices are not needed anymore
        points_.clear();
        colors_.clear();
        texCoords_.clear();
        indices_.clear();
    }
    else {
        // 1. init the Primitive (only once)
        Primitive::init();
        // 2. remember global vertex array object
        unique_vao_ = vao_;
        unique_drawCount = drawCount_;
    }
}

void LineSquare::accept(Visitor& v)
{
    Primitive::accept(v);
    v.visit(*this);
}

LineSquare::~LineSquare()
{
    // do NOT delete vao_ (unique)
    vao_ = 0;
}


LineCircle::LineCircle(uint linewidth) : LineStrip(std::vector<glm::vec3>(), std::vector<glm::vec4>(), linewidth)
{
    static int N = 72;
    static float a =  glm::two_pi<float>() / static_cast<float>(N);
    static glm::vec4 circle_color_points = glm::vec4(1.f, 1.f, 1.f, 1.f);
    // loop to build a circle
    glm::vec3 P(1.f, 0.f, 0.f);
    for (int i = 0; i < N ; i++ ){
        points_.push_back( glm::vec3(P) );
        colors_.push_back( circle_color_points );
        indices_.push_back ( i );

        P = glm::rotateZ(P, a);
    }
    // close loop
    points_.push_back( glm::vec3(1.f, 0.f, 0.f) );
    colors_.push_back( circle_color_points );
    indices_.push_back ( N );
}

void LineCircle::init()
{
    // use static unique vertex array object
    static uint unique_vao_ = 0;
    static uint unique_drawCount = 0;
    if (unique_vao_) {
        // 1. only init Node (not the primitive vao)
        Node::init();
        // 2. use the global vertex array object
        vao_ = unique_vao_;
        drawCount_ = unique_drawCount;        
        // replace AxisAlignedBoundingBox
        bbox_.extend(points_);
        // arrays of vertices are not needed anymore (STATIC DRAW of vertex object)
        points_.clear();
        colors_.clear();
        texCoords_.clear();
        indices_.clear();
    }
    else {
        // 1. init the Primitive (only once)
        Primitive::init();
        // 2. remember global vertex array object
        unique_vao_ = vao_;
        unique_drawCount = drawCount_;
        // 3. unique_vao_ will NOT be deleted because LineCircle::deleteGLBuffers_() is empty
    }
}

void LineCircle::accept(Visitor& v)
{
    Primitive::accept(v);
    v.visit(*this);
}


LineCircle::~LineCircle()
{
    // do NOT delete vao_ (unique)
    vao_ = 0;
}
