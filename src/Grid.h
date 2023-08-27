#ifndef GRID_H
#define GRID_H

#include <glm/glm.hpp>

#include "Scene.h"

class Grid
{

public:
    typedef enum {
        GRID_ORTHO = 0,
        GRID_POLAR
    } Shapes;

    Grid(Group *parent, Shapes s = GRID_ORTHO);
    virtual ~Grid() {}

    // if active, the view will use it to approximate coordinates
    inline bool active() const { return active_; }
    inline void setActive(bool on) { active_ = on; }

    // possible grid scales and shapes
    typedef enum {
        UNIT_PRECISE = 0,
        UNIT_SMALL = 1,
        UNIT_DEFAULT = 2,
        UNIT_LARGE = 3,
        UNIT_ONE = 4
    } Units;
    inline void setUnit(Units u) { unit_ = u; }

    inline float aspectRatio() const {
        return root_->scale_.x;
    }
    inline void setAspectRatio(float ar) {
        root_->scale_.x = ar;
    }

    // unit of the grid, i.e. fraction between lines
    glm::vec2 step() const;
    glm::vec2 snap(glm::vec2 in);

    inline glm::vec3 snap(glm::vec3 in) {
        return glm::vec3( snap(glm::vec2(in)), in.z);
    }
    inline glm::vec4 snap(glm::vec4 in) {
        return glm::vec4( snap(glm::vec2(in)), in.z, in.w);
    }

    // Node to render in scene
    virtual Group *root () { return root_; }
    virtual void setColor (const glm::vec4 &c);

//     get point on grid approximating the given coordinates
//    virtual glm::vec2 approx(const glm::vec2 &p) const { return p; }

    // get the center point of the grid
//    virtual glm::vec2 center() const { return glm::vec2(0.f); }

protected:
    bool  active_;
    Shapes shape_;
    Units unit_;
    Group *parent_;
    Group *root_;
    static float ortho_units_[5];
    static float polar_units_[5];
};

class TranslationGrid : public Grid
{
    Switch *ortho_grids_;
public:
    TranslationGrid(Group *parent);
    Group *root () override;
};

class RotationGrid : public Grid
{
    Switch *polar_grids_;
public:
    RotationGrid(Group *parent);
    Group *root () override;
};


#endif // GRID_H
