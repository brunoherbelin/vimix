/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2022 Bruno Herbelin <bruno.herbelin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
**/


#include <glad/glad.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <glm/gtc/constants.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>

#include "ImageShader.h"
#include "Resource.h"
#include "FrameBuffer.h"
#include "MediaPlayer.h"
#include "Visitor.h"
#include "Log.h"

#include "Primitives.h"

Surface::Surface(Shader *s) : Primitive(s), textureindex_(0), mirror_(true)
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
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, mirror_ ? GL_MIRRORED_REPEAT : GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, mirror_ ? GL_MIRRORED_REPEAT : GL_REPEAT);
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


LineSquare::LineSquare(const LineSquare &square)
{
    top_    = new HLine(square.top_->width);
    top_->translation_ = glm::vec3(0.f, 1.f, 0.f);
    attach(top_);
    bottom_ = new HLine(square.bottom_->width);
    bottom_->translation_ = glm::vec3(0.f, -1.f, 0.f);
    attach(bottom_);
    left_   = new VLine(square.left_->width);
    left_->translation_ = glm::vec3(-1.f, 0.f, 0.f);
    attach(left_);
    right_  = new VLine(square.right_->width);
    right_->translation_ = glm::vec3(1.f, 0.f, 0.f);
    attach(right_);

    setColor(square.color());
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

LineStrip::LineStrip(const std::vector<glm::vec2> &path, float linewidth) : Primitive(new Shader),
    arrayBuffer_(0), path_(path)
{
    linewidth_ = 0.002f * linewidth;

    for(size_t i = 1; i < path_.size(); ++i)
    {
        glm::vec3 begin = glm::vec3(path_[i-1], 0.f);
        glm::vec3 end   = glm::vec3(path_[i], 0.f);
        glm::vec3 dir   = end - begin;
        glm::vec3 perp  = glm::normalize(glm::cross(dir, glm::vec3(0.f, 0.f, 1.f)));

        points_.push_back( begin + perp * linewidth_ );
        colors_.push_back( glm::vec4( 1.f, 1.f, 1.f, 1.f ) );
        indices_.push_back ( indices_.size() );

        points_.push_back( begin - perp * linewidth_ );
        colors_.push_back( glm::vec4( 1.f, 1.f, 1.f, 1.f ) );
        indices_.push_back ( indices_.size() );

        points_.push_back( end + perp * linewidth_ );
        colors_.push_back( glm::vec4( 1.f, 1.f, 1.f, 1.f ) );
        indices_.push_back ( indices_.size() );

        points_.push_back( end - perp * linewidth_ );
        colors_.push_back( glm::vec4( 1.f, 1.f, 1.f, 1.f ) );
        indices_.push_back ( indices_.size() );
    }

    drawMode_ = GL_TRIANGLE_STRIP;
}

LineStrip::~LineStrip()
{
    // delete buffer
    if ( arrayBuffer_ )
        glDeleteBuffers ( 1, &arrayBuffer_);
}

void LineStrip::init()
{
    if ( vao_ )
        glDeleteVertexArrays ( 1, &vao_);

    // Vertex Array
    glGenVertexArrays( 1, &vao_ );

    // Create and initialize buffer objects
    if ( arrayBuffer_ )
        glDeleteBuffers ( 1, &arrayBuffer_);
    glGenBuffers( 1, &arrayBuffer_ );
    uint elementBuffer_;
    glGenBuffers( 1, &elementBuffer_);
    glBindVertexArray( vao_ );

    // setup the array buffers for vertices
    std::size_t sizeofPoints = sizeof(glm::vec3) * points_.size();
    std::size_t sizeofColors = sizeof(glm::vec4) * colors_.size();
    glBindBuffer( GL_ARRAY_BUFFER, arrayBuffer_ );
    glBufferData( GL_ARRAY_BUFFER, sizeofPoints + sizeofColors, NULL, GL_DYNAMIC_DRAW);
    glBufferSubData( GL_ARRAY_BUFFER, 0, sizeofPoints, &points_[0] );
    glBufferSubData( GL_ARRAY_BUFFER, sizeofPoints, sizeofColors, &colors_[0] );

    // setup the element array for indices
    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, elementBuffer_);
    glBufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof(uint) * indices_.size(), &(indices_[0]), GL_STATIC_DRAW);

    // explain how to read attributes 0 and 1
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void *)0 );
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void *)(sizeofPoints) );
    glEnableVertexAttribArray(1);

    // done
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // drawing indications
    drawCount_ = indices_.size();

    if ( elementBuffer_ )
        glDeleteBuffers ( 1, &elementBuffer_);
    indices_.clear();

    // compute AxisAlignedBoundingBox
    bbox_.extend(points_);

    Node::init();
}

void LineStrip::updatePath()
{
    // redo points_ array
    points_.clear();
    for(size_t i = 1; i < path_.size(); ++i)
    {
        glm::vec3 begin = glm::vec3(path_[i-1], 0.f);
        glm::vec3 end   = glm::vec3(path_[i], 0.f);
        glm::vec3 dir   = end - begin;
        glm::vec3 perp  = glm::normalize(glm::cross(dir, glm::vec3(0.f, 0.f, 1.f)));

        points_.push_back( begin + perp * linewidth_ );
        points_.push_back( begin - perp * linewidth_ );
        points_.push_back( end + perp * linewidth_ );
        points_.push_back( end - perp * linewidth_ );
    }

    // bind the vertex array and change the point coordinates
    glBindVertexArray( vao_ );
    glBindBuffer(GL_ARRAY_BUFFER, arrayBuffer_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::vec3) * points_.size(), &points_[0] );
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // reset and compute AxisAlignedBoundingBox
    GlmToolkit::AxisAlignedBoundingBox b;
    bbox_ = b;
    bbox_.extend(points_);
}

void LineStrip::editPath(uint index, glm::vec2 position)
{
    if (index < path_.size()) {
        path_[index] = position;
        updatePath();
    }
}

void LineStrip::changePath(std::vector<glm::vec2> path)
{
    // invalid if not enough points given
    size_t N = path_.size();
    if (path.size() < N)
        return;

    // replace path but keep number of points
    path_ = path;
    path_.resize(N);

    updatePath();
}

void LineStrip::setLineWidth(float linewidth) {

    linewidth_ = 0.002f * linewidth;
    updatePath();
}

void LineStrip::accept(Visitor& v)
{
    Primitive::accept(v);
    v.visit(*this);
}


LineLoop::LineLoop(const std::vector<glm::vec2> &path, float linewidth) : LineStrip(path, linewidth)
{
    // close linestrip loop
    glm::vec3 begin = glm::vec3(path_[path_.size()-1], 0.f);
    glm::vec3 end   = glm::vec3(path_[0], 0.f);
    glm::vec3 dir   = end - begin;
    glm::vec3 perp  = glm::normalize(glm::cross(dir, glm::vec3(0.f, 0.f, 1.f)));

    points_.push_back( begin + perp * linewidth_ );
    colors_.push_back( glm::vec4( 1.f, 1.f, 1.f, 1.f ) );
    indices_.push_back ( indices_.size()  );

    points_.push_back( begin - perp * linewidth_ );
    colors_.push_back( glm::vec4( 1.f, 1.f, 1.f, 1.f ) );
    indices_.push_back ( indices_.size() );

    points_.push_back( end + perp * linewidth_ );
    colors_.push_back( glm::vec4( 1.f, 1.f, 1.f, 1.f ) );
    indices_.push_back ( indices_.size()  );

    points_.push_back( end - perp * linewidth_ );
    colors_.push_back( glm::vec4( 1.f, 1.f, 1.f, 1.f ) );
    indices_.push_back ( indices_.size() );
}

void LineLoop::updatePath()
{
    glm::vec3 begin;
    glm::vec3 end;
    glm::vec3 dir;
    glm::vec3 perp;

    // redo points_ array
    points_.clear();
    size_t i = 1;
    for(; i < path_.size(); ++i)
    {
        begin = glm::vec3(path_[i-1], 0.f);
        end   = glm::vec3(path_[i], 0.f);
        dir   = end - begin;
        perp  = glm::normalize(glm::cross(dir, glm::vec3(0.f, 0.f, 1.f)));

        points_.push_back( begin + perp * linewidth_ );
        points_.push_back( begin - perp * linewidth_ );
        points_.push_back( end + perp * linewidth_ );
        points_.push_back( end - perp * linewidth_ );
    }

    // close linestrip loop
    begin = glm::vec3(path_[i-1], 0.f);
    end   = glm::vec3(path_[0], 0.f);
    dir   = end - begin;
    perp  = glm::normalize(glm::cross(dir, glm::vec3(0.f, 0.f, 1.f)));
    points_.push_back( begin + perp * linewidth_ );
    points_.push_back( begin - perp * linewidth_ );
    points_.push_back( end + perp * linewidth_ );
    points_.push_back( end - perp * linewidth_ );

    // bind the vertex array and change the point coordinates
    glBindVertexArray( vao_ );
    glBindBuffer(GL_ARRAY_BUFFER, arrayBuffer_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::vec3) * points_.size(), &points_[0] );
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // re-compute AxisAlignedBoundingBox
    bbox_.extend(points_);
}

#define LINE_CIRCLE_DENSITY 72

LineCircle::LineCircle(float linewidth) : LineLoop(std::vector<glm::vec2>(LINE_CIRCLE_DENSITY), linewidth)
{
    static float a =  glm::two_pi<float>() / static_cast<float>(LINE_CIRCLE_DENSITY-1);
    // loop to build a circle
    glm::vec3 P(1.f, 0.f, 0.f);

    for (int i = 0; i < LINE_CIRCLE_DENSITY - 1; i++ ){
        path_[i] = glm::vec2(P);
        P = glm::rotateZ(P, a);
    }
    updatePath();
}


