// Freely inspired from https://github.com/alter-rokuz/glm-aabb.git

#include "GlmToolkit.h"

#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/random.hpp>

glm::mat4 GlmToolkit::transform(glm::vec3 translation, glm::vec3 rotation, glm::vec3 scale)
{
    glm::mat4 View = glm::translate(glm::identity<glm::mat4>(), translation);
    View = glm::rotate(View, rotation.x, glm::vec3(1.f, 0.f, 0.f));
    View = glm::rotate(View, rotation.y, glm::vec3(0.f, 1.f, 0.f));
    View = glm::rotate(View, rotation.z, glm::vec3(0.f, 0.f, 1.f));
    glm::mat4 Model = glm::scale(glm::identity<glm::mat4>(), scale);
    return View * Model;
}


GlmToolkit::AxisAlignedBoundingBox::AxisAlignedBoundingBox() {
    mMin = glm::vec3(1.f);
    mMax = glm::vec3(-1.f);
}

void GlmToolkit::AxisAlignedBoundingBox::extend(const glm::vec3& point) // TODO why ref to point?
{
    if (isNull()) {
        mMin = point;
        mMax = point;
    }
    // general case
    else  {
        mMin = glm::min(point, mMin);
        mMax = glm::max(point, mMax);
    }
}

void GlmToolkit::AxisAlignedBoundingBox::extend(std::vector<glm::vec3> points)
{
    for (auto p = points.begin(); p != points.end(); p++)
        extend(*p);
}


void GlmToolkit::AxisAlignedBoundingBox::extend(const AxisAlignedBoundingBox& bb)
{
    if (bb.isNull())
        return;

    if (isNull()) {
        mMin = bb.mMin;
        mMax = bb.mMax;
    }
    // general case
    else {
        mMin = glm::min(bb.mMin, mMin);
        mMax = glm::max(bb.mMax, mMax);
    }
}

glm::vec3 GlmToolkit::AxisAlignedBoundingBox::center(bool ignore_z) const
{
    glm::vec3 ret = glm::vec3(0.f);

    if (!isNull())
    {
      glm::vec3 d = mMax - mMin;
      ret = mMin + (d * 0.5f);
    }

    if (ignore_z)
        ret.z = 0.f;

    return ret;
}

glm::vec3 GlmToolkit::AxisAlignedBoundingBox::scale(bool ignore_z) const
{
    glm::vec3 ret = glm::vec3(1.f);

    if (!isNull())
    {
      glm::vec3 d = mMax - mMin;
      ret = d * 0.5f;
    }

    if (ignore_z)
        ret.z = 1.f;

    return ret;
}

bool GlmToolkit::AxisAlignedBoundingBox::intersect(const AxisAlignedBoundingBox& bb, bool ignore_z) const
{
    if (isNull() || bb.isNull())
        return false;

    if (    (mMax.x < bb.mMin.x) || (mMin.x > bb.mMax.x) ||
            (mMax.y < bb.mMin.y) || (mMin.y > bb.mMax.y) ||
            ( !ignore_z && ((mMax.z < bb.mMin.z) || (mMin.z > bb.mMax.z)) ) )
    {
        return false;
    }

    return true;
}

bool GlmToolkit::AxisAlignedBoundingBox::contains(const AxisAlignedBoundingBox& bb, bool ignore_z) const
{
    if ( !intersect(bb, ignore_z))
        return false;

    if (    (mMin.x < bb.mMin.x) && (mMax.x > bb.mMax.x) &&
            (mMin.y < bb.mMin.y) && (mMax.y > bb.mMax.y)
            && ( ignore_z || ((mMin.z < bb.mMin.z) && (mMax.z > bb.mMax.z)) )
            )
    {
        return true;
    }

    return false;
}

bool GlmToolkit::AxisAlignedBoundingBox::contains(glm::vec3 point, bool ignore_z) const
{
    if (    (mMax.x < point.x) || (mMin.x > point.x) ||
            (mMax.y < point.y) || (mMin.y > point.y) ||
            ( !ignore_z && ((mMax.z < point.z) || (mMin.z > point.z)) ) )
        return false;

    return true;
}


GlmToolkit::AxisAlignedBoundingBox GlmToolkit::AxisAlignedBoundingBox::translated(glm::vec3 t) const
{
    GlmToolkit::AxisAlignedBoundingBox bb;
    bb = *this;

    bb.mMin += t;
    bb.mMax += t;

    return bb;
}

GlmToolkit::AxisAlignedBoundingBox GlmToolkit::AxisAlignedBoundingBox::scaled(glm::vec3 s) const
{
    GlmToolkit::AxisAlignedBoundingBox bb;    
    glm::vec3 vec;

    // Apply scaling to min & max corners (can be inverted) and update bbox accordingly
    vec = mMin * s;
    bb.extend(vec);

    vec = mMax * s;
    bb.extend(vec);

    return bb;
}

GlmToolkit::AxisAlignedBoundingBox GlmToolkit::AxisAlignedBoundingBox::transformed(glm::mat4 m) const
{
    GlmToolkit::AxisAlignedBoundingBox bb;
    glm::vec4 vec;

    // Apply transform to all four corners (can be rotated) and update bbox accordingly
    vec = m * glm::vec4(mMin, 1.f);
    bb.extend(glm::vec3(vec));

    vec = m * glm::vec4(mMax, 1.f);
    bb.extend(glm::vec3(vec));

    vec = m * glm::vec4(mMin.x, mMax.y, 0.f, 1.f);
    bb.extend(glm::vec3(vec));

    vec = m * glm::vec4(mMax.x, mMin.y, 0.f, 1.f);
    bb.extend(glm::vec3(vec));

    return bb;
}


glm::ivec2 GlmToolkit::resolutionFromDescription(int aspectratio, int height)
{
    int ar = glm::clamp(aspectratio, 0, 5);
    int h  = glm::clamp(height, 0, 8);

    static glm::vec2 aspect_ratio_size[6] = { glm::vec2(1.f,1.f), glm::vec2(4.f,3.f), glm::vec2(3.f,2.f), glm::vec2(16.f,10.f), glm::vec2(16.f,9.f), glm::vec2(21.f,9.f) };
    static float resolution_height[10] = { 16.f, 64.f, 200.f, 320.f, 480.f, 576.f, 720.f, 1080.f, 1440.f, 2160.f };

    float width = aspect_ratio_size[ar].x * resolution_height[h] / aspect_ratio_size[ar].y;
    glm::ivec2 res = glm::ivec2( width, resolution_height[h]);

    return res;
}
