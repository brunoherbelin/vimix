#include <iostream>
#include <sstream>
#include <istream>
#include <iterator>
#include <vector>
#include <map>
#include <utility>

#include <glad/glad.h>

#include "Resource.h"
#include "ImageShader.h"
#include "Visitor.h"
#include "Log.h"
#include "Mesh.h"

using namespace std;
using namespace glm;



typedef std::vector< std::pair< std::string, int> > plyElement;

typedef struct prop {
    std::string name;
    bool is_float;
    bool is_list;
    prop(std::string n, bool t, bool l = false){
        name = n;
        is_float = t;
        is_list = l;
    }
} plyProperty;

typedef std::map<std::string, std::vector<plyProperty> > plyElementProperties;

//float parseValue()
template <typename T>
T parseValue(std::istream& istream) {

    T v;
    char space = ' ';
    istream >> v;
    if (!istream.eof()) {
        istream >> space >> std::ws;
    }

    return v;
}

/**
 * @brief parsePLY
 *
 * Loosely inspired from libply
 * https://web.archive.org/web/20151202190005/http://people.cs.kuleuven.be/~ares.lagae/libply/
 *
 * @param ascii content of an ascii PLY file
 * @param outPositions vertices
 * @param outColors colors
 * @param outUV texture coordinates
 * @param outIndices indices of faces
 * @param outPrimitive type of faces (triangles, etc.)
 * @return true on success read
 */
bool parsePLY(string ascii,
              vector<vec3> &outPositions,
              vector<vec4> &outColors,
              vector<vec2> &outUV,
              vector<uint> &outIndices,
              uint &outPrimitive)
{
    stringstream istream(ascii);

    std::string line;
    std::size_t line_number_ = 0;

    // magic
    char magic[3];
    istream.read(magic, 3);
    istream.ignore(1);
    ++line_number_;
    if (!istream) {
        Log::Warning("Parse error line %d: not ASCII?", line_number_);
        return false;
    }
    if ((magic[0] != 'p') || (magic[1] != 'l') || (magic[2] != 'y')){
        Log::Warning("Parse error line %d: not PLY format", line_number_);
        return false;
    }

    plyElement elements;
    plyElementProperties elementsProperties;
    std::string current_element = "";

    // parse header
    while (std::getline(istream, line)) {

        ++line_number_;
        std::istringstream stringstream(line);
        stringstream.unsetf(std::ios_base::skipws);

        stringstream >> std::ws;
        if (stringstream.eof()) {
            Log::Warning("Ignoring line %d: '%s'", line_number_, line.c_str());
        }

        else {
            std::string keyword;
            stringstream >> keyword;

            // format
            if (keyword == "format") {
                std::string format_string, version;
                char space_format_format_string, space_format_string_version;
                stringstream >> space_format_format_string >> std::ws >> format_string >> space_format_string_version >> std::ws >> version >> std::ws;
                if (!stringstream.eof() ||
                        !std::isspace(space_format_format_string) ||
                        !std::isspace(space_format_string_version)) {
                    Log::Warning("Parse error line %d: '%s'", line_number_, line.c_str());
                    return false;
                }
                if (format_string != "ascii") {
                    Log::Warning("Not PLY file format %s", format_string.c_str());
                }
                if (version != "1.0") {
                    Log::Warning("Unsupported PLY version %s", version.c_str());
                    return false;
                }
            }

            // element
            else if (keyword == "element") {
                std::string name;
                std::size_t count;
                char space_element_name, space_name_count;
                stringstream >> space_element_name >> std::ws >> name >> space_name_count >> std::ws >> count >> std::ws;
                if (!stringstream.eof() ||
                        !std::isspace(space_element_name) ||
                        !std::isspace(space_name_count)) {
                    Log::Warning("Parse error line %d: '%s'", line_number_, line.c_str());
                    return false;
                }
                current_element = name;
                elements.push_back( pair<string, int>{current_element, count} );
            }

            // property
            else if (keyword == "property") {
                std::string type_or_list;
                char space_property_type_or_list;
                stringstream >> space_property_type_or_list >> std::ws >> type_or_list;
                if (!std::isspace(space_property_type_or_list)) {
                    Log::Warning("Parse error line %d: '%s'", line_number_, line.c_str());
                  return false;
                }

                // NOT A LIST property : i.e. a type & a name (e.g. 'property float x' )
                if (type_or_list != "list") {
                    std::string name;
                    std::string& type = type_or_list;
                    char space_type_name;
                    stringstream >> space_type_name >> std::ws >> name >> std::ws;
                    if (!std::isspace(space_type_name)) {
                        Log::Warning("Parse error line %d: '%s'", line_number_, line.c_str());
                        return false;
                    }
                    bool float_value =  ( type == "float" ) | ( type == "double" );
                    elementsProperties[current_element].push_back(plyProperty(name, float_value));
                }
                // list property : several types & a name (e.g. 'property list uchar uint vertex_indices')
                else {
                    std::string name;
                    std::string size_type_string, scalar_type_string;
                    char space_list_size_type, space_size_type_scalar_type, space_scalar_type_name;
                    stringstream >> space_list_size_type >> std::ws >> size_type_string >> space_size_type_scalar_type >> std::ws >> scalar_type_string >> space_scalar_type_name >> std::ws >> name >> std::ws;
                    if (!std::isspace(space_list_size_type) ||
                            !std::isspace(space_size_type_scalar_type) ||
                            !std::isspace(space_scalar_type_name)) {
                      Log::Warning("Parse error line %d: '%s'", line_number_, line.c_str());
                      return false;
                    }
                    elementsProperties[current_element].push_back(plyProperty(name, false, true));
                }

            }

            // end_header
            else if (keyword == "end_header") {
                break;
            }
        }
    } // end while readline header

    uint num_vertex_per_face = 0;
    // loop over elements
    for (uint i=0; i< elements.size(); ++i)
    {
        std::string elem = elements[i].first;
        int num_data = elements[i].second;

        // loop over lines of properties of the element
        for (int n = 0; n < num_data; ++n )
        {
            if (!std::getline(istream, line)) {
                Log::Warning("Parse error line %d: '%s'", line_number_, line.c_str());
                return false;
            }
            ++line_number_;
            std::istringstream stringstream(line);
            stringstream.unsetf(std::ios_base::skipws);
            stringstream >> std::ws;

            vec3 point = vec3(0.f, 0.f, 0.f);
            vec4 color = vec4(1.f, 1.f, 1.f, 1.f);
            vec2 uv = vec2(0.f, 0.f);
            bool has_point = false;
            bool has_uv = false;

            // read each property of the element
            for (uint j = 0; j < elementsProperties[elem].size(); ++j)
            {
                plyProperty prop = elementsProperties[elem][j];

                // a numerical property
                if ( ! prop.is_list ) {

                    float value;
                    switch ( prop.name[0] ) {
                    case 'x':
                        point.x = parseValue<float>(stringstream);
                        has_point = true;
                        break;
                    case 'y':
                        point.y = parseValue<float>(stringstream);
                        has_point = true;
                        break;
                    case 'z':
                        point.z = parseValue<float>(stringstream);
                        has_point = true;
                        break;
                    case 's':
                        uv.x = parseValue<float>(stringstream);
                        has_uv = true;
                        break;
                    case 't':
                        uv.y = parseValue<float>(stringstream);
                        has_uv = true;
                        break;
                    case 'r':
                        color.r = (float) parseValue<int>(stringstream) / 255.f;
                        break;
                    case 'g':
                        color.g = (float) parseValue<int>(stringstream) / 255.f;
                        break;
                    case 'b':
                        color.b = (float) parseValue<int>(stringstream) / 255.f;
                        break;
                    case 'a':
                        color.a = (float) parseValue<int>(stringstream) / 255.f;
                        break;
                    default:
                        // ignore normals or other types
                        value = parseValue<float>(stringstream);
                        break;
                    }
                }
                // a list property
                else {
                    // how many values in the list of index ?
                    uint num_index = parseValue<int>(stringstream);

                    // check that the number of vertex per face is consistent
                    if (num_vertex_per_face == 0)
                        num_vertex_per_face = num_index;
                    else {
                        if (num_vertex_per_face != num_index) {
                            Log::Warning("Variable number of vertices per face not supported");
                            return false;
                        }
                    }

                    // safely append those indices
                    for (uint k = 0; k < num_vertex_per_face; ++k ){
                        uint index = parseValue<int>(stringstream);
                        outIndices.push_back( index );
                    }
                }
            }

            // ok, we filled some values
            if (has_point) {
                outPositions.push_back(point);
                outColors.push_back(color);
                if (has_uv)
                    outUV.push_back(uv);
            }
        }

    }

    switch (num_vertex_per_face) {
    case 1:
        outPrimitive = GL_POINTS;
        break;
    case 2:
        outPrimitive = GL_LINES;
        break;
    case 3:
        outPrimitive = GL_TRIANGLES;
        break;
    case 4:
        outPrimitive = GL_QUADS;
        break;
    default:
        Log::Warning("Invalid number of vertices per face. Please triangulate your mesh.");
        return false;
        break;
    }

    return true;
}


Mesh::Mesh(const std::string& ply_path, const std::string& tex_path) : Primitive(), mesh_resource_(ply_path), texture_resource_(tex_path), textureindex_(0)
{
    if ( !parsePLY( Resource::getText(mesh_resource_), points_, colors_, texCoords_, indices_, drawMode_) )
    {
        points_.clear();
        colors_.clear();
        texCoords_.clear();
        indices_.clear();
        Log::Warning("Mesh could not be created from %s", ply_path.c_str());
    }

    // default non texture shader (deleted in Primitive)
    shader_ = new Shader;
}


void Mesh::setTexture(uint textureindex)
{
    if (textureindex) {
        // replace previous shader with a new Image Shader
        replaceShader( new ImageShader );
        // set texture
        textureindex_ = textureindex;
    }
}

void Mesh::init()
{
    Primitive::init();

    if (!texture_resource_.empty())
        setTexture(Resource::getTextureImage(texture_resource_));

}

void Mesh::draw(glm::mat4 modelview, glm::mat4 projection)
{
    if ( !initialized() )
        init();

    if (textureindex_)
        glBindTexture(GL_TEXTURE_2D, textureindex_);

    Primitive::draw(modelview, projection);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void Mesh::accept(Visitor& v)
{
    Primitive::accept(v);
    v.visit(*this);
}

Frame::Frame(Style style) : Node()
{
    overlay_ = nullptr;
    shadow_  = nullptr;
    color   = glm::vec4( 1.f, 1.f, 1.f, 1.f);
    switch (style) {
    case SHARP_HANDLES:
        border_  = new Mesh("mesh/border_handles_sharp.ply");
        overlay_ = new Mesh("mesh/border_handles_overlay.ply");
        shadow_  = new Mesh("mesh/shadow.ply", "images/shadow.png");
        break;
    case SHARP_THIN:
        border_  = new Mesh("mesh/border_sharp.ply");
        break;
    case ROUND_LARGE:
        border_  = new Mesh("mesh/border_large_round.ply");
        shadow_  = new Mesh("mesh/shadow.ply", "images/shadow.png");
        break;
    default:
    case ROUND_THIN:
        border_  = new Mesh("mesh/border_round.ply");
        shadow_  = new Mesh("mesh/shadow.ply", "images/shadow.png");
        break;
    }
}

Frame::~Frame()
{
    if(overlay_) delete overlay_;
    if(border_)  delete border_;
    if(shadow_)  delete shadow_;
}

void Frame::draw(glm::mat4 modelview, glm::mat4 projection)
{
    if ( !initialized() ) {
        if(overlay_) overlay_->init();
        if(border_)  border_->init();
        if(shadow_)  shadow_->init();
        init();
    }

    if ( visible_ ) { // not absolutely necessary but saves some CPU time..

        // shadow
        if(shadow_)
            shadow_->draw( modelview * transform_, projection);

        if (overlay_) {
            // overlat is not altered
            overlay_->shader()->color = color;
            overlay_->draw( modelview, projection );
        }

        // right side
        float ar = scale_.x / scale_.y;
        glm::vec3 s(scale_.y, scale_.y, 1.0);
        glm::vec3 t(translation_.x - 1.0 +ar, translation_.y, translation_.z);
        glm::mat4 ctm = modelview * transform(t, rotation_, s);

        if(border_) {
            // right side
            border_->shader()->color = color;
            border_->draw( ctm, projection );
            // left side
            t.x = -t.x;
            s.x = -s.x;
            ctm = modelview * transform(t, rotation_, s);
            border_->draw( ctm, projection );
        }

    }
}


void Frame::accept(Visitor& v)
{
    Node::accept(v);
    v.visit(*this);
}
