#include "Primitives.h"
#include "ImageShader.h"
#include "Resource.h"
#include "MediaPlayer.h"
#include "Visitor.h"
#include "Log.h"

#include <glad/glad.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>


static const std::vector<glm::vec3> square_points { glm::vec3( -1.f, -1.f, 0.f ), glm::vec3( -1.f, 1.f, 0.f ),
                                                   glm::vec3( 1.f, 1.f, 0.f ), glm::vec3( 1.f, -1.f, 0.f ) };
static uint square_vao = 0;
static uint circle_vao = 0;
static const std::string default_texture = "images/busy.png";


ImageSurface::ImageSurface(const std::string& path) : Primitive()
{
    filename_ = path;

    float ar = 1.0;
    textureindex_ = Resource::getTextureImage(filename_, &ar);
    transform_ = glm::scale(glm::identity<glm::mat4>(), glm::vec3(ar, 1.f, 1.f));

    // geometry
    points_ = std::vector<glm::vec3> { glm::vec3( -1.f, -1.f, 0.f ), glm::vec3( -1.f, 1.f, 0.f ),
            glm::vec3( 1.f, -1.f, 0.f ), glm::vec3( 1.f, 1.f, 0.f ) };
    colors_ = std::vector<glm::vec3> { glm::vec3( 1.f, 1.f, 1.f ), glm::vec3(  1.f, 1.f, 1.f ),
            glm::vec3( 1.f, 1.f, 1.f  ), glm::vec3( 1.f, 1.f, 1.f ) };
    texCoords_ = std::vector<glm::vec2> { glm::vec2( 0.f, 1.f ), glm::vec2( 0.f, 0.f ),
            glm::vec2( 1.f, 1.f ), glm::vec2( 1.f, 0.f ) };
    indices_ = std::vector<uint> { 0, 1, 2, 3 };

    // setup shader for textured image
    shader_ = new ImageShader();
    drawingPrimitive_ = GL_TRIANGLE_STRIP;
}


void ImageSurface::init()
{
    // use static global vertex array object
    if (square_vao)
        // if set, use the global vertex array object
        vao_ = square_vao;
    else {
        // 1. init as usual (only once)
        Primitive::init();
        // 2. remember global vao
        square_vao = vao_;
        // 3. vao_ will not be deleted because deleteGLBuffers_() is empty
    }

    visible_ = true;
}

void ImageSurface::draw(glm::mat4 modelview, glm::mat4 projection)
{
    glBindTexture(GL_TEXTURE_2D, textureindex_);

    Primitive::draw(modelview, projection);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void ImageSurface::accept(Visitor& v)
{
    Primitive::accept(v);
    v.visit(*this);
}

MediaSurface::MediaSurface(const std::string& path) : ImageSurface(default_texture)
{
    filename_ = path;
    mediaplayer_ = new MediaPlayer;
    mediaplayer_->open(filename_);
    mediaplayer_->play(true);
}

MediaSurface::~MediaSurface()
{
    delete mediaplayer_;
}

void MediaSurface::draw(glm::mat4 modelview, glm::mat4 projection)
{
    if ( mediaplayer_->isOpen() )
        mediaplayer_->bind();
    else
        glBindTexture(GL_TEXTURE_2D, textureindex_);

    Primitive::draw(modelview, projection);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void MediaSurface::update( float dt )
{
    if ( mediaplayer_->isOpen() )
        mediaplayer_->update();

    transform_ = glm::scale(glm::identity<glm::mat4>(), glm::vec3(mediaplayer_->aspectRatio(), 1.f, 1.f));

    Primitive::update( dt );
}

void MediaSurface::accept(Visitor& v)
{
    ImageSurface::accept(v);
    v.visit(*this);
}

LineStrip::LineStrip(std::vector<glm::vec3> points, glm::vec3 color, uint linewidth) : Primitive()
{
    for(size_t i = 0; i < points.size(); ++i)
    {
        points_.push_back( points[i] );
        colors_.push_back( color );
        indices_.push_back ( i );
    }

    shader_ = new Shader();

    drawingPrimitive_ = GL_LINE_LOOP;
    linewidth_ = linewidth;
}

void LineStrip::draw(glm::mat4 modelview, glm::mat4 projection)
{
    glLineWidth(linewidth_);
    Primitive::draw(modelview, projection);
}

void LineStrip::accept(Visitor& v)
{
    Primitive::accept(v);
    v.visit(*this);
}


LineSquare::LineSquare(glm::vec3 color, uint linewidth) : LineStrip(square_points, color, linewidth)
{
}

LineCircle::LineCircle(glm::vec3 color, uint linewidth) : LineStrip(square_points, color, linewidth)
{
    points_.clear();
    colors_.clear();
    indices_.clear();

    int N = 72;
    float a =  glm::two_pi<float>() / static_cast<float>(N);
    glm::vec3 P(1.f, 0.f, 0.f);
    for (int i = 0; i < N ; i++ ){
        points_.push_back( glm::vec3(P) );
        colors_.push_back( color );
        indices_.push_back ( i );

        P = glm::rotateZ(P, a);
    }
}

void LineCircle::init()
{
    // use static global vertex array object
    if (circle_vao)
        // if set, use the global vertex array object
        vao_ = square_vao;
    else {
        // 1. init as usual (only once)
        Primitive::init();
        // 2. remember global vao
        circle_vao = vao_;
        // 3. vao_ will not be deleted because deleteGLBuffers_() is empty
    }

    visible_ = true;
}

void LineCircle::accept(Visitor& v)
{
    Primitive::accept(v);
    v.visit(*this);
}
