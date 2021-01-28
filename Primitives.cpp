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
#include <glm/gtx/vector_angle.hpp>
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



HLine::HLine(float linewidth): Primitive(new Shader), width(linewidth)
{
    //                      1       3
    //                      +-------+        ^
    //                    / |     / | \      |
    //    +-----+   => 0 +  |   /   |  + 5   | linewidth
    //   -1     1         \ | /     | /      |
    //                      +-------+        v
    //                      2       4
    //
    points_ = std::vector<glm::vec3> { glm::vec3( -1.f, 0.f, 0.f ),
            glm::vec3( -0.999f, 0.001f, 0.f ),
            glm::vec3( -0.999f, -0.001f, 0.f ),
            glm::vec3( 0.999f, 0.001f, 0.f ),
            glm::vec3( 0.999f, -0.001f, 0.f ),
            glm::vec3( 1.f, 0.f, 0.f ) };
    colors_ = std::vector<glm::vec4> { glm::vec4( 1.f, 1.f, 1.f , 1.f ), glm::vec4(  1.f, 1.f, 1.f, 1.f  ),
            glm::vec4( 1.f, 1.f, 1.f, 1.f ), glm::vec4( 1.f, 1.f, 1.f, 1.f ),
            glm::vec4( 1.f, 1.f, 1.f, 1.f ), glm::vec4( 1.f, 1.f, 1.f, 1.f ) };
    indices_ = std::vector<uint> { 0, 1, 2, 3, 4, 5 };
    drawMode_ = GL_TRIANGLE_STRIP;

    // default scale
    scale_.y = width;

    //default color
    color = glm::vec4( 1.f, 1.f, 1.f, 1.f);
}

HLine::~HLine()
{
    // do NOT delete vao_ (unique)
    vao_ = 0;
}

void HLine::init()
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

void HLine::draw(glm::mat4 modelview, glm::mat4 projection)
{
    // extract pure scaling from modelview (without rotation)
    glm::mat4 ctm;
    glm::vec3 rot(0.f);
    glm::vec4 vec = modelview * glm::vec4(1.f, 0.f, 0.f, 0.f);
    rot.z = glm::orientedAngle( glm::vec3(1.f, 0.f, 0.f), glm::normalize(glm::vec3(vec)), glm::vec3(0.f, 0.f, 1.f) );
    ctm = glm::rotate(glm::identity<glm::mat4>(), -rot.z, glm::vec3(0.f, 0.f, 1.f)) * modelview ;
    vec = ctm * glm::vec4(1.f, 1.f, 0.f, 0.f);

    // Change transform to use linewidth independently of scale in Y (vertical)
    scale_.y = (float) width / vec.y;
    update(0);

    // change color
    shader_->color = color;

    Primitive::draw(modelview, projection);
}

VLine::VLine(float linewidth): Primitive(new Shader), width(linewidth)
{
    points_ = std::vector<glm::vec3> { glm::vec3( 0.f, -1.f, 0.f ),
            glm::vec3( 0.001f, -0.999f, 0.f ),
            glm::vec3( -0.001f, -0.999f, 0.f ),
            glm::vec3( 0.001f, 0.999f, 0.f ),
            glm::vec3( -0.001f, 0.999f, 0.f ),
            glm::vec3( 0.f, 1.f, 0.f )};
    colors_ = std::vector<glm::vec4> { glm::vec4( 1.f, 1.f, 1.f , 1.f ), glm::vec4(  1.f, 1.f, 1.f, 1.f  ),
            glm::vec4( 1.f, 1.f, 1.f, 1.f ), glm::vec4( 1.f, 1.f, 1.f, 1.f ),
            glm::vec4( 1.f, 1.f, 1.f, 1.f ), glm::vec4( 1.f, 1.f, 1.f, 1.f ) };
    indices_ = std::vector<uint> { 0, 1, 2, 3, 4, 5 };
    drawMode_ = GL_TRIANGLE_STRIP;

    // default scale
    scale_.x = width;

    // default color
    color = glm::vec4( 1.f, 1.f, 1.f, 1.f);
}

VLine::~VLine()
{
    // do NOT delete vao_ (unique)
    vao_ = 0;
}

void VLine::init()
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

void VLine::draw(glm::mat4 modelview, glm::mat4 projection)
{
    // extract pure scaling from modelview (without rotation)
    glm::mat4 ctm;
    glm::vec3 rot(0.f);
    glm::vec4 vec = modelview * glm::vec4(1.f, 0.f, 0.f, 0.f);
    rot.z = glm::orientedAngle( glm::vec3(1.f, 0.f, 0.f), glm::normalize(glm::vec3(vec)), glm::vec3(0.f, 0.f, 1.f) );
    ctm = glm::rotate(glm::identity<glm::mat4>(), -rot.z, glm::vec3(0.f, 0.f, 1.f)) * modelview ;
    vec = ctm * glm::vec4(1.f, 1.f, 0.f, 0.f);

    // Change transform to use linewidth independently of scale in X (horizontal)
    scale_.x = width / vec.x;
    update(0);

    // change color
    shader_->color = color;

    Primitive::draw(modelview, projection);
}

LineSquare::LineSquare(float linewidth) : Group()
{
    top_    = new HLine(linewidth);
    top_->translation_ = glm::vec3(0.f, 1.f, 0.f);
    attach(top_);
    bottom_ = new HLine(linewidth);
    bottom_->translation_ = glm::vec3(0.f, -1.f, 0.f);
    attach(bottom_);
    left_   = new VLine(linewidth);
    left_->translation_ = glm::vec3(-1.f, 0.f, 0.f);
    attach(left_);
    right_  = new VLine(linewidth);
    right_->translation_ = glm::vec3(1.f, 0.f, 0.f);
    attach(right_);
}


void LineSquare::setLineWidth(float v)
{
    top_->width = v;
    bottom_->width = v;
    left_->width = v;
    right_->width = v;
}

void LineSquare::setColor(glm::vec4 c)
{
    top_->color = c;
    bottom_->color = c;
    left_->color = c;
    right_->color = c;
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
