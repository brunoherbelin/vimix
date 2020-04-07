#include "Primitives.h"
#include "ImageShader.h"
#include "Resource.h"
#include "MediaPlayer.h"
#include "Visitor.h"
#include "Log.h"
#include "ObjLoader.h"

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


ImageSurface::ImageSurface(const std::string& path) : Primitive(), textureindex_(0)
{
    // for image texture
    filename_ = path;

    // geometry
    points_ = std::vector<glm::vec3> { glm::vec3( -1.f, -1.f, 0.f ), glm::vec3( -1.f, 1.f, 0.f ),
            glm::vec3( 1.f, -1.f, 0.f ), glm::vec3( 1.f, 1.f, 0.f ) };
    colors_ = std::vector<glm::vec3> { glm::vec3( 1.f, 1.f, 1.f ), glm::vec3(  1.f, 1.f, 1.f ),
            glm::vec3( 1.f, 1.f, 1.f  ), glm::vec3( 1.f, 1.f, 1.f ) };
    texCoords_ = std::vector<glm::vec2> { glm::vec2( 0.f, 1.f ), glm::vec2( 0.f, 0.f ),
            glm::vec2( 1.f, 1.f ), glm::vec2( 1.f, 0.f ) };
    indices_ = std::vector<uint> { 0, 1, 2, 3 };
    drawingPrimitive_ = GL_TRIANGLE_STRIP;
}


void ImageSurface::init()
{
    // load image if specified
    if ( filename_.empty())
        textureindex_ = Resource::getTextureBlack();
    else {
        float ar = 1.0;
        textureindex_ = Resource::getTextureImage(filename_, &ar);
        scale_.x = ar;
    }
    // create shader for textured image
    shader_ = new ImageShader();

    // use static global vertex array object
    if (square_vao) {
        // only init Node (not the primitive vao
        Node::init();
        // if set, use the global vertex array object
        vao_ = square_vao;
    }
    else {
        // 1. init as usual (only once)
        Primitive::init();
        // 2. remember global vao
        square_vao = vao_;
        // 3. vao_ will NOT be deleted because deleteGLBuffers_() is empty
    }
}

void ImageSurface::draw(glm::mat4 modelview, glm::mat4 projection)
{
    if ( !initialized() )
        init();

    glBindTexture(GL_TEXTURE_2D, textureindex_);

    Primitive::draw(modelview, projection);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void ImageSurface::accept(Visitor& v)
{
    Primitive::accept(v);
    v.visit(*this);
}

MediaSurface::MediaSurface(const std::string& path) : ImageSurface()
{
    filename_ = path;
    mediaplayer_ = new MediaPlayer;
}

MediaSurface::~MediaSurface()
{
    delete mediaplayer_;
}

void MediaSurface::init()
{
    std::string tmp = filename_;
    filename_ = "";
    ImageSurface::init();
    filename_ = tmp;

    mediaplayer_->open(filename_);
    mediaplayer_->play(true);
}

void MediaSurface::draw(glm::mat4 modelview, glm::mat4 projection)
{
    if ( !initialized() )
        init();

    if ( mediaplayer_->isOpen() )
        mediaplayer_->bind();
    else
        glBindTexture(GL_TEXTURE_2D, textureindex_);

    Primitive::draw(modelview, projection);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void MediaSurface::update( float dt )
{
    if ( mediaplayer_->isOpen() ) {
        mediaplayer_->update();

        if (parent_ != nullptr) {
            parent_->transform_ = parent_->transform_ * glm::scale(glm::identity<glm::mat4>(), glm::vec3(mediaplayer_->aspectRatio(), 1.f, 1.f));
            scale_.x = 1.0;
        }
        else
            scale_.x = mediaplayer_->aspectRatio();
    }

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

    drawingPrimitive_ = GL_LINE_LOOP;
    linewidth_ = linewidth;
}

void LineStrip::init()
{
    Primitive::init();
    shader_ = new Shader();
}

void LineStrip::draw(glm::mat4 modelview, glm::mat4 projection)
{
    if ( !initialized() )
        init();

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
    if (circle_vao) {
        Node::init();
        // if set, use the global vertex array object
        vao_ = square_vao;
    }
    else {
        // 1. init as usual (only once)
        Primitive::init();
        // 2. remember global vao
        circle_vao = vao_;
        // 3. vao_ will not be deleted because deleteGLBuffers_() is empty
    }

    shader_ = new Shader();
}

void LineCircle::accept(Visitor& v)
{
    Primitive::accept(v);
    v.visit(*this);
}


ObjModel::ObjModel(const std::string& path) : Primitive(), textureindex_(0)
{
    // for obj model
    filename_ = path;

    // load geometry
    std::vector<glm::vec3> normals; // ignored
    std::string material_filename;
    bool okay = loadObject( Resource::getText(filename_), points_, normals, texCoords_, indices_, material_filename, 1.0 );
    if ( !okay ) {
        Log::Warning("Failed to load OBJ model %s", path.c_str());
    }

    // prepend path to the name of other files
    std::string rsc_path = filename_.substr(0, filename_.rfind('/')) + "/";

    // load materials
    std::map<std::string,Material*> material_library;
    okay = loadMaterialLibrary(Resource::getText( rsc_path + material_filename ), material_library);
    if (okay) {
        Material *material_ = material_library.begin()->second; // default use first material

        // fill colors
        for (int i = 0; i < points_.size(); i++)
            colors_.push_back(material_->diffuse);

        if (!material_->diffuseTexture.empty()) {
            texture_filename_ = rsc_path + material_->diffuseTexture;
        }
        delete material_;
    }

    drawingPrimitive_ = GL_TRIANGLES;
}

void ObjModel::init()
{
    Primitive::init();

    if (!texture_filename_.empty())
    {
        // create shader for textured image
        textureindex_ = Resource::getTextureImage(texture_filename_);
        shader_ = new ImageShader();
    }
    else {
        shader_ = new Shader();
    }

}

void ObjModel::draw(glm::mat4 modelview, glm::mat4 projection)
{
    if ( !initialized() )
        init();

    if (textureindex_)
        glBindTexture(GL_TEXTURE_2D, textureindex_);

    Primitive::draw(modelview, projection);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void ObjModel::accept(Visitor& v)
{
    Primitive::accept(v);
    v.visit(*this);
}
