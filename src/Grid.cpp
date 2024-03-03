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

#include <glm/gtx/vector_angle.hpp>

#include "Primitives.h"
#include "DrawVisitor.h"

#include "Grid.h"


float Grid::ortho_units_[] = { 1.f / 20.f, 1.f / 10.f, 1.f / 5.f, 1.f / 2.f, 1.f };
float Grid::polar_units_[] = { M_PI / 18.f, M_PI / 12.f, M_PI / 6.f,  M_PI / 4.f, M_PI / 2.f };

Grid::Grid(Group *parent, Shapes s) : active_(false), shape_(s), unit_(UNIT_DEFAULT),
    parent_(parent), root_(new Group)
{

}

Grid::~Grid()
{
    if (root_)
        delete root_;
}

glm::vec2 Grid::step() const {

    if ( shape_ != GRID_POLAR )
        return glm::vec2(root_->scale_.x, 1.f) * glm::vec2(ortho_units_[unit_]) ;
    else
        return glm::vec2(ortho_units_[unit_] * root_->scale_.x, polar_units_[unit_]);
}

glm::vec2 Grid::snap(glm::vec2 in) {

    if ( shape_ != GRID_POLAR ) {
        // convert to grid coordinate frame
        glm::vec3 coord = glm::vec3(in, 0.f);
        const glm::mat4 G = GlmToolkit::transform(root_->translation_,
                                                  root_->rotation_,
                                                  glm::vec3(1.f));
        coord = glm::inverse(G) * glm::vec4(coord, 1.f);
        coord = glm::vec3( glm::round( glm::vec2(coord) / step() ) * step(), 0.f);
        coord = G * glm::vec4(coord, 1.f);
        return glm::vec2(coord);
    }
    else {
        // convert orthographic to polar coordinates
        glm::vec2 ortho = in;
        glm::vec2 polar;
        polar.x = glm::length( glm::vec2(ortho) );
        polar.y = -glm::orientedAngle( glm::normalize(in), glm::normalize(glm::vec2(1.f, 0.f)));

        // snap polar to polar grid
        polar = glm::round( polar / step() ) * step();;

        // convert back to ortho coordinates
        ortho.x = polar.x * cos(polar.y);
        ortho.y = polar.x * sin(polar.y);
        return ortho;
    }
}

void Grid::setColor (const glm::vec4 &c)
{
    ColorVisitor color(c);
    root_->accept(color);
}

TranslationGrid::TranslationGrid(Group *parent) : Grid(parent)
{
    root_ = new Group;
    parent_->attach(root_);

    HLine *xaxis= new HLine(12.f);
    xaxis->scale_.x = 0.1f;
    root_->attach(xaxis);

    VLine *yaxis= new VLine(12.f);
    yaxis->scale_.y = 0.1f;
    root_->attach(yaxis);

    ortho_grids_ = new Switch;
    ortho_grids_->attach(new LineGrid(224, Grid::ortho_units_[UNIT_PRECISE], 2.f));
    ortho_grids_->attach(new LineGrid(112, Grid::ortho_units_[UNIT_SMALL], 2.f));
    ortho_grids_->attach(new LineGrid( 56, Grid::ortho_units_[UNIT_DEFAULT], 2.f));
    ortho_grids_->attach(new LineGrid( 28, Grid::ortho_units_[UNIT_LARGE], 2.f));
    ortho_grids_->attach(new LineGrid( 14, Grid::ortho_units_[UNIT_ONE], 2.f));
    root_->attach(ortho_grids_);

    // not visible at init
    setColor( glm::vec4(0.f) );
}

Group *TranslationGrid::root ()
{
    //adjust the gridnodethe unit scale
    ortho_grids_->setActive(unit_);

    // return the node to draw
    return root_;
}

RotationGrid::RotationGrid(Group *parent) : Grid(parent, Grid::GRID_POLAR)
{
    root_ = new Group;
    parent_->attach(root_);

    polar_grids_ = new Switch;
    polar_grids_->attach(new LineCircleGrid(Grid::polar_units_[UNIT_PRECISE], 50,
                                            Grid::ortho_units_[UNIT_PRECISE], 0.5f));
    polar_grids_->attach(new LineCircleGrid(Grid::polar_units_[UNIT_SMALL], 30,
                                            Grid::ortho_units_[UNIT_SMALL], 0.5f));
    polar_grids_->attach(new LineCircleGrid(Grid::polar_units_[UNIT_DEFAULT], 15,
                                            Grid::ortho_units_[UNIT_DEFAULT], 0.5f));
    polar_grids_->attach(new LineCircleGrid(Grid::polar_units_[UNIT_LARGE],  6,
                                            Grid::ortho_units_[UNIT_LARGE], 0.5f));
    polar_grids_->attach(new LineCircleGrid(Grid::polar_units_[UNIT_ONE],  3,
                                            Grid::ortho_units_[UNIT_ONE], 0.5f));
    root_->attach(polar_grids_);

    // not visible at init
    setColor( glm::vec4(0.f) );
}

Group *RotationGrid::root ()
{
    polar_grids_->setActive(unit_);

    // return the node to draw
    return root_;
}

//glm::vec2 GeometryGrid::approx(const glm::vec2 &in ) const
//{
//    glm::vec2 out;

//    glm::vec3 coord = Rendering::manager().unProject( in, root_->transform_);
//    coord = glm::round( coord / unit()) * unit();
//    out = Rendering::manager().project( coord, root_->transform_, false);

//    return out;
//}
