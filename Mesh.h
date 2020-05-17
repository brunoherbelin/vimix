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
public:

    typedef enum { ROUND_THIN = 0, ROUND_LARGE, SHARP_THIN, SHARP_LARGE } Type;
    Frame(Type style);
    ~Frame();

    void draw (glm::mat4 modelview, glm::mat4 projection) override;
    void accept (Visitor& v) override;

    Type type() const { return type_; }
    Mesh *border() const { return border_; }
    glm::vec4 color;

protected:
    Mesh *border_;
    Mesh *shadow_;
    Type type_;

};

class Handles : public Node
{
public:
    typedef enum { RESIZE = 0, RESIZE_H, RESIZE_V, ROTATE } Type;
    Handles(Type type);
    ~Handles();

    void update (float dt) override;
    void draw (glm::mat4 modelview, glm::mat4 projection) override;
    void accept (Visitor& v) override;

    Type type() const { return type_; }
    Primitive *handle() const { return handle_; }
    glm::vec4 color;

protected:
    Primitive *handle_;
    Type type_;

};

// TODO Shadow mesh with unique vao
// TODO dedicated source .cpp .h files for Frame and Handles


#endif // MESH_H
