#ifndef MESH_H
#define MESH_H

#include <string>
#include <glm/glm.hpp>

#include "Scene.h"

/**
 * @brief The Mesh class creates a Primitive node from a PLY File
 *
 *  PLY - Polygon File Format
 *  Also known as the Stanford Triangle Format
 *  http://paulbourke.net/dataformats/ply/
 */
class Mesh : public Primitive {

public:
    Mesh(const std::string& ply_path, const std::string& tex_path = "");

    void setTexture(uint textureindex);

    void init () override;
    void draw (glm::mat4 modelview, glm::mat4 projection) override;
    void accept (Visitor& v) override;

    inline std::string meshPath() const { return mesh_resource_; }
    inline std::string texturePath() const { return texture_resource_; }

protected:
    std::string mesh_resource_;
    std::string texture_resource_;
    uint textureindex_;

};

class Frame : public Node
{
    Mesh *border_;
    Mesh *shadow_;

public:

    typedef enum { ROUND_THIN = 0, ROUND_LARGE, SHARP_THIN, SHARP_HANDLES } Style;
    Frame(Style style);
    ~Frame();

    void draw (glm::mat4 modelview, glm::mat4 projection) override;
    void accept (Visitor& v) override;

    Mesh *overlay_;
    glm::vec4 color;
};

#endif // MESH_H
