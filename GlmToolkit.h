#ifndef GLMTOOLKIT_H
#define GLMTOOLKIT_H


#include <vector>
#include <glm/glm.hpp>

namespace GlmToolkit
{

// get integer with unique id
int uniqueId();

// get Matrix for these transformation components
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

    AxisAlignedBoundingBox translated(glm::vec3 t) const;
    AxisAlignedBoundingBox scaled(glm::vec3 s) const;
    AxisAlignedBoundingBox transformed(glm::mat4 m) const;
};


static const char* aspect_ratio_names[6] = { "1:1", "4:3", "3:2", "16:10", "16:9", "21:9" };
static const char* height_names[10] = { "16", "64", "200", "320", "480", "576", "720p", "1080p", "1440", "4K" };

glm::ivec2 resolutionFromDescription(int aspectratio, int height);

}


#endif // GLMTOOLKIT_H
