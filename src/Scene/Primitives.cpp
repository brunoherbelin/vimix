/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2023 Bruno Herbelin <bruno.herbelin@gmail.com>
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
#include <glm/gtc/constants.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/vector_angle.hpp>

#include "Resource.h"
#include "FrameBuffer.h"
#include "Visitor/Visitor.h"

#include "Primitives.h"

#define MESH_SURFACE_DENSITY 32

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


MeshSurface::MeshSurface(Shader *s) : Surface(s)
{
}

void MeshSurface::generate_mesh(size_t rows, size_t columns)
{
    // Set up vertices
    points_.resize(rows * columns);
    texCoords_.resize(rows * columns);
    colors_.resize(rows * columns);
    for (size_t c = 0; c < columns; ++c) {
        for (size_t r = 0; r < rows; ++r) {
            size_t index = c * columns + r;
            points_[index] = glm::vec3(-1.f, -1.f, 0.f)
                             + 2.f * glm::vec3((float) c / (float) (columns - 1),
                                               (float) r / (float) (rows - 1),
                                               0.f);
            texCoords_[index] = glm::vec2(0.f, 1.f)
                                + glm::vec2((float) c / (float) (columns - 1),
                                            -1.f * (float) r / (float) (rows - 1));
            colors_[index] = glm::vec4(1.f, 1.f, 1.f, 1.f);
        }
    }

    // Set up indices
    indices_.resize( (rows * 2 + 2) * (columns - 1) - 2 );
    size_t i = 0;
    size_t height = columns - 1;
    for (size_t y = 0; y < height; y++) {
        size_t base = y * rows;
        //indices[i++] = (uint16)base;
        for (size_t x = 0; x < rows; x++) {
            indices_[i++] = (base + x);
            indices_[i++] = (base + rows + x);
        }
        // add a degenerate triangle (except in a last row)
        if (y < height - 1) {
            indices_[i++] = ((y + 1) * rows + (rows - 1));
            indices_[i++] = ((y + 1) * rows);
        }
    }

    drawMode_ = GL_TRIANGLE_STRIP;

//    g_printerr("generated_mesh %lu x %lu :\n", rows, columns);
//    g_printerr("%lu points\n", points_.size());
//    for(size_t ind=0; ind < points_.size(); ind++) {
//        g_printerr("point[%lu] : %f, %f, %f \n", ind, points_[ind].x, points_[ind].y, points_[ind].z);
//        g_printerr("         : %f, %f \n", texCoords_[ind].x, texCoords_[ind].y);
//    }
//    g_printerr("%lu indices\n", indices_.size());
//    for(size_t ind=0; ind < indices_.size(); ind++) {
//        g_printerr("index[%lu] = %u \n", ind, indices_[ind]);
//    }
}

void MeshSurface::init()
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
        // 0. generate mesh
        generate_mesh(MESH_SURFACE_DENSITY, MESH_SURFACE_DENSITY);
        // 1. init the Primitive (only once)
        Primitive::init();
        // 2. remember global vertex array object
        unique_vao_ = vao_;
        unique_drawCount = drawCount_;
        // 3. unique_vao_ will NOT be deleted
    }

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

FrameBufferMeshSurface::FrameBufferMeshSurface(FrameBuffer *fb, Shader *s) : MeshSurface(s), frame_buffer_(fb)
{
}

void FrameBufferMeshSurface::draw(glm::mat4 modelview, glm::mat4 projection)
{
    if ( !initialized() )
        init();

    glBindTexture(GL_TEXTURE_2D, frame_buffer_->texture());

    Primitive::draw(modelview, projection);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void FrameBufferMeshSurface::accept(Visitor& v)
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



HLine::HLine(float linewidth, Shader *s): Primitive(s), width(linewidth)
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

    Primitive::draw(modelview, projection);
}

VLine::VLine(float linewidth, Shader *s): Primitive(s), width(linewidth)
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

    Primitive::draw(modelview, projection);
}

LineSquare::LineSquare(float linewidth) : Group()
{
    shader__ = new Shader;

    top_    = new HLine(linewidth, shader__);
    top_->translation_ = glm::vec3(0.f, 1.f, 0.f);
    attach(top_);
    bottom_ = new HLine(linewidth, shader__);
    bottom_->translation_ = glm::vec3(0.f, -1.f, 0.f);
    attach(bottom_);
    left_   = new VLine(linewidth, shader__);
    left_->translation_ = glm::vec3(-1.f, 0.f, 0.f);
    attach(left_);
    right_  = new VLine(linewidth, shader__);
    right_->translation_ = glm::vec3(1.f, 0.f, 0.f);
    attach(right_);
}

LineSquare::LineSquare(const LineSquare &square)
{
    shader__ = new Shader;
    shader__->color = square.color();

    top_    = new HLine(square.top_->width, shader__);
    top_->translation_ = glm::vec3(0.f, 1.f, 0.f);
    attach(top_);
    bottom_ = new HLine(square.bottom_->width, shader__);
    bottom_->translation_ = glm::vec3(0.f, -1.f, 0.f);
    attach(bottom_);
    left_   = new VLine(square.left_->width, shader__);
    left_->translation_ = glm::vec3(-1.f, 0.f, 0.f);
    attach(left_);
    right_  = new VLine(square.right_->width, shader__);
    right_->translation_ = glm::vec3(1.f, 0.f, 0.f);
    attach(right_);
}

LineSquare::~LineSquare()
{
    top_->replaceShader(nullptr);
    bottom_->replaceShader(nullptr);
    left_->replaceShader(nullptr);
    right_->replaceShader(nullptr);
    delete shader__;
}

void LineSquare::setLineWidth(float v)
{
    top_->width = v;
    bottom_->width = v;
    left_->width = v;
    right_->width = v;
}

LineGrid::LineGrid(size_t N, float step, float linewidth)
{
    shader__ = new Shader;
    N = MAX(1, N);

    for (size_t n = 0; n < N ; ++n) {
        VLine *l = new VLine(linewidth, shader__);
        l->translation_.x = (float)n * step;
        l->scale_.y = (float)N * step;
        attach(l);
    }
    for (size_t n = 1; n < N ; ++n) {
        VLine *l = new VLine(linewidth, shader__);
        l->translation_.x = (float)n * -step;
        l->scale_.y = (float)N * step;
        attach(l);
    }
    for (size_t n = 0; n < N ; ++n) {
        HLine *l = new HLine(linewidth, shader__);
        l->translation_.y = (float)n * step;
        l->scale_.x = (float)N * step;
        attach(l);
    }
    for (size_t n = 1; n < N ; ++n) {
        HLine *l = new HLine(linewidth, shader__);
        l->translation_.y = (float)n * -step;
        l->scale_.x = (float)N * step;
        attach(l);
    }
}

LineGrid::~LineGrid()
{
    // prevent nodes from deleting the shader
    for (NodeSet::iterator node = begin(); node != end(); ++node) {
        Primitive *p = dynamic_cast<Primitive *>(*node);
        if (p)
            p->replaceShader(nullptr);
    }
    delete shader__;
}

void LineGrid::setLineWidth(float v)
{
    for (NodeSet::iterator node = begin(); node != end(); ++node) {
        VLine *vl = dynamic_cast<VLine *>(*node);
        if (vl) {
            vl->width = v;
            continue;
        }
        HLine *hl = dynamic_cast<HLine *>(*node);
        if (hl)
            hl->width = v;
    }
}

float LineGrid::lineWidth() const
{
    Node *n = front();
    if (n) {
        VLine *vl = dynamic_cast<VLine *>(n);
        if (vl)
            return vl->width;
        HLine *hl = dynamic_cast<HLine *>(n);
        if (hl)
            return hl->width;
    }
    return 0.f;
}

LineStrip::LineStrip(const std::vector<glm::vec2> &path, float linewidth, Shader *s) : Primitive(s),
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
    if (!vao_)
        return;

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


LineLoop::LineLoop(const std::vector<glm::vec2> &path, float linewidth, Shader *s) : LineStrip(path, linewidth, s)
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
    if (!vao_)
        return;

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

// statically defined line loop drawing a circle (72 points)
std::vector<glm::vec2> _circle_loop = {
    {1.000000, 0.000000},
    {0.996087, 0.088380},
    {0.984378, 0.176069},
    {0.964965, 0.262379},
    {0.938000, 0.346636},
    {0.903694, 0.428180},
    {0.862315, 0.506373},
    {0.814187, 0.580603},
    {0.759687, 0.650289},
    {0.699242, 0.714885},
    {0.633324, 0.773887},
    {0.562449, 0.826832},
    {0.487173, 0.873306},
    {0.408084, 0.912945},
    {0.325801, 0.945439},
    {0.240968, 0.970533},
    {0.154249, 0.988032},
    {0.066323, 0.997798},
    {-0.022122, 0.999756},
    {-0.110394, 0.993888},
    {-0.197802, 0.980242},
    {-0.283662, 0.958925},
    {-0.367302, 0.930102},
    {-0.448067, 0.894000},
    {-0.525325, 0.850902},
    {-0.598472, 0.801144},
    {-0.666936, 0.745116},
    {-0.730179, 0.683256},
    {-0.787708, 0.616049},
    {-0.839072, 0.544021},
    {-0.883869, 0.467734},
    {-0.921749, 0.387788},
    {-0.952415, 0.304806},
    {-0.975627, 0.219439},
    {-0.991203, 0.132354},
    {-0.999022, 0.044233},
    {-0.999022, -0.044233},
    {-0.991203, -0.132354},
    {-0.975627, -0.219439},
    {-0.952415, -0.304806},
    {-0.921749, -0.387788},
    {-0.883870, -0.467734},
    {-0.839072, -0.544021},
    {-0.787708, -0.616049},
    {-0.730179, -0.683256},
    {-0.666936, -0.745116},
    {-0.598473, -0.801144},
    {-0.525325, -0.850902},
    {-0.448067, -0.894001},
    {-0.367302, -0.930102},
    {-0.283662, -0.958925},
    {-0.197802, -0.980243},
    {-0.110394, -0.993888},
    {-0.022122, -0.999756},
    {0.066323, -0.997799},
    {0.154249, -0.988033},
    {0.240968, -0.970534},
    {0.325801, -0.945439},
    {0.408084, -0.912945},
    {0.487173, -0.873306},
    {0.562450, -0.826832},
    {0.633324, -0.773887},
    {0.699242, -0.714886},
    {0.759688, -0.650289},
    {0.814188, -0.580603},
    {0.862315, -0.506373},
    {0.903694, -0.428180},
    {0.938001, -0.346636},
    {0.964966, -0.262379},
    {0.984379, -0.176068},
    {0.996088, -0.088380}
};

LineCircle::LineCircle(float linewidth, Shader *s) : LineLoop(_circle_loop, linewidth, s)
{
    //    static float a =  glm::two_pi<float>() / static_cast<float>(LINE_CIRCLE_DENSITY-1);
    //    // loop to build a circle
    //    glm::vec3 P(1.f, 0.f, 0.f);

    //    for (int i = 0; i < LINE_CIRCLE_DENSITY - 1; i++ ){
    //        path_[i] = glm::vec2(P);
    //        g_printerr("{%f, %f},\n", path_[i].x, path_[i].y);
    //        P = glm::rotateZ(P, a);
    //    }
}


LineCircleGrid::LineCircleGrid(float angle_step, size_t N, float step, float linewidth)
{
    shader__ = new Shader;
    N = MAX(1, N);
    step = MAX(0.01, step);

    // Draw N concentric circles
    for (size_t n = 1; n < N ; ++n) {
        float scale = (float) n * step;
        LineCircle *l = new LineCircle(linewidth / scale, shader__);
        l->scale_ = glm::vec3( scale, scale, 1.f);
        // add cirle to group
        attach(l);
    }

    // Draw radius lines every angle step
    glm::vec3 O(0.f, 0.f, 0.f);
    glm::vec3 P(1.f, 0.f, 0.f);
    std::vector<glm::vec2> points;
    for (int a = 0 ; a < (int)( (2.f * M_PI) / angle_step ) + 1 ; ++a ){
        points.push_back(O);
        P = glm::rotateZ(P, angle_step);
        points.push_back(P);
    }
    // add to group
    LineStrip *radius = new LineStrip(points, linewidth * 0.5f, shader__);
    radius->scale_ = glm::vec3( glm::vec2( (float) N * step ), 1.f);
    attach(radius);
}

LineCircleGrid::~LineCircleGrid()
{
    // prevent nodes from deleting the shader
    for (NodeSet::iterator node = begin(); node != end(); ++node) {
        Primitive *p = dynamic_cast<Primitive *>(*node);
        if (p)
            p->replaceShader(nullptr);
    }
    delete shader__;
}

void LineCircleGrid::setLineWidth(float v)
{
    for (NodeSet::iterator node = begin(); node != end(); ++node) {
        LineStrip *vl = dynamic_cast<LineStrip *>(*node);
        if (vl)
            vl->setLineWidth(v);
    }
}

float LineCircleGrid::lineWidth() const
{
    Node *n = front();

    if (n) {
        LineStrip *vl = dynamic_cast<LineStrip *>(n);
        if (vl)
            return vl->lineWidth();
    }
    return 0.f;
}

