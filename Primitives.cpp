#include "Primitives.h"
#include "ImageShader.h"
#include "Resource.h"
#include "MediaPlayer.h"
#include "Log.h"

#include <glad/glad.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>



TexturedRectangle::TexturedRectangle(const std::string& resourcepath)
{
    texturepath_ = resourcepath;

    float ar = 1.0;
    textureindex_ = Resource::getTextureImage(texturepath_, &ar);
    transform_ = glm::scale(glm::identity<glm::mat4>(), glm::vec3(ar, 1.f, 1.f));

    // point 0
    points_.push_back( glm::vec3( -ar, -1.f, 0.f ) );
    colors_.push_back( glm::vec3( 1.f, 1.f, 1.f ) );
    texCoords_.push_back( glm::vec2( 0.f, 1.f ) );
    // point 1
    points_.push_back( glm::vec3( -ar, 1.f, 0.f ) );
    colors_.push_back( glm::vec3( 1.f, 1.f, 1.f ) );
    texCoords_.push_back( glm::vec2( 0.f, 0.f ) );
    // point 2
    points_.push_back( glm::vec3( ar, -1.f, 0.f ) );
    colors_.push_back( glm::vec3( 1.f, 1.f, 1.f ) );
    texCoords_.push_back( glm::vec2( 1.f, 1.f ) );
    // point 3
    points_.push_back( glm::vec3( ar, 1.f, 0.f ) );
    colors_.push_back( glm::vec3( 1.f, 1.f, 1.f ) );
    texCoords_.push_back( glm::vec2( 1.f, 0.f ) );
    // indices
    indices_.push_back ( 0 );
    indices_.push_back ( 1 );
    indices_.push_back ( 2 );
    indices_.push_back ( 3 );

    // setup shader for textured image
    shader_ = new ImageShader();
}

void TexturedRectangle::draw(glm::mat4 modelview, glm::mat4 projection)
{
    glBindTexture(GL_TEXTURE_2D, textureindex_);

    Primitive::draw(modelview, projection);

    glBindTexture(GL_TEXTURE_2D, 0);
}



MediaRectangle::MediaRectangle(const std::string& mediapath)
{
    mediapath_ = mediapath;

    mediaplayer_ = new MediaPlayer;
    mediaplayer_->Open(mediapath_);
    mediaplayer_->Play(true);

    // point 0
    points_.push_back( glm::vec3( -1.f, -1.f, 0.f ) );
    colors_.push_back( glm::vec3( 1.f, 1.f, 1.f ) );
    texCoords_.push_back( glm::vec2( 0.f, 1.f ) );
    // point 1
    points_.push_back( glm::vec3( -1.f, 1.f, 0.f ) );
    colors_.push_back( glm::vec3( 1.f, 1.f, 1.f ) );
    texCoords_.push_back( glm::vec2( 0.f, 0.f ) );
    // point 2
    points_.push_back( glm::vec3( 1.f, -1.f, 0.f ) );
    colors_.push_back( glm::vec3( 1.f, 1.f, 1.f ) );
    texCoords_.push_back( glm::vec2( 1.f, 1.f ) );
    // point 3
    points_.push_back( glm::vec3( 1.f, 1.f, 0.f ) );
    colors_.push_back( glm::vec3( 1.f, 1.f, 1.f ) );
    texCoords_.push_back( glm::vec2( 1.f, 0.f ) );
    // indices
    indices_.push_back ( 0 );
    indices_.push_back ( 1 );
    indices_.push_back ( 2 );
    indices_.push_back ( 3 );

    // setup shader for textured image
    shader_ = new ImageShader();
}

MediaRectangle::~MediaRectangle()
{
    delete mediaplayer_;
}

void MediaRectangle::draw(glm::mat4 modelview, glm::mat4 projection)
{
    mediaplayer_->Bind();

    Primitive::draw(modelview, projection);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void MediaRectangle::update( float dt )
{
    if ( mediaplayer_->isOpen() )
        mediaplayer_->Update();

    transform_ = glm::scale(glm::identity<glm::mat4>(), glm::vec3(mediaplayer_->AspectRatio(), 1.f, 1.f));

    Primitive::update( dt );
}


LineStrip::LineStrip(std::vector<glm::vec3> points, glm::vec3 color)
{
    for(size_t i = 0; i < points.size(); ++i)
    {
        points_.push_back( points[i] );
        colors_.push_back( color );
        indices_.push_back ( i );
    }

    shader_ = new Shader();
    drawingPrimitive_ = GL_LINE_STRIP;
}


