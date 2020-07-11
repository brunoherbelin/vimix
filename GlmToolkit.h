#ifndef GLMTOOLKIT_H
#define GLMTOOLKIT_H


#include <vector>
#include <glm/glm.hpp>

namespace GlmToolkit
{

glm::mat4 transform(glm::vec3 translation, glm::vec3 rotation, glm::vec3 scale);


class AxisAlignedBoundingBox
{
    glm::vec3 mMin;
    glm::vec3 mMax;

public:
    AxisAlignedBoundingBox();

    void operator = (const AxisAlignedBoundingBox &D ) {
        mMin = D.mMin;
        mMax = D.mMax;
    }

    // test
    inline bool isNull() const { return mMin.x > mMax.x || mMin.y > mMax.y || mMin.z > mMax.z;}
    inline glm::vec3 min() const { return mMin; }
    inline glm::vec3 max() const { return mMax; }
    glm::vec3 center(bool ignore_z = true) const;
    glm::vec3 scale(bool ignore_z = true) const;
    bool intersect(const AxisAlignedBoundingBox& bb, bool ignore_z = true) const;
    bool contains(const AxisAlignedBoundingBox& bb, bool ignore_z = true) const;
    bool contains(glm::vec3 point, bool ignore_z = true) const;

    // build
    void extend(const glm::vec3& point);
    void extend(std::vector<glm::vec3> points);
    void extend(const AxisAlignedBoundingBox& bb);

    AxisAlignedBoundingBox translated(glm::vec3 t);
    AxisAlignedBoundingBox scaled(glm::vec3 s);
    AxisAlignedBoundingBox transformed(glm::mat4 m);
};

}


#endif // GLMTOOLKIT_H
