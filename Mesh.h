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
    Mesh(const std::string& path, const std::string& texture = "");
    ~Mesh();

    void init () override;
    void draw (glm::mat4 modelview, glm::mat4 projection) override;
    void accept (Visitor& v) override;

    inline std::string getResource() const { return resource_; }
    inline std::string getTexture() const { return texturefilename_; }

protected:
    std::string resource_;
    std::string texturefilename_;
    uint textureindex_;

};

#endif // MESH_H
