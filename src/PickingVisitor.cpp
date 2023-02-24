/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2022 Bruno Herbelin <bruno.herbelin@gmail.com>
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

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/vector_angle.hpp>

#include "Decorations.h"
#include "GlmToolkit.h"

#include "PickingVisitor.h"


PickingVisitor::PickingVisitor(glm::vec3 coordinates, bool force) : Visitor(),
    force_(force), modelview_(glm::mat4(1.f))
{
    points_.push_back( coordinates );
}

PickingVisitor::PickingVisitor(glm::vec3 selectionstart, glm::vec3 selection_end, bool force) : Visitor(),
    force_(force), modelview_(glm::mat4(1.f))
{
    points_.push_back( selectionstart );
    points_.push_back( selection_end );
}

void PickingVisitor::visit(Node &n)
{
    // use the transform modified during update
    modelview_ *= n.transform_;
}

void PickingVisitor::visit(Group &n)
{
    if (!n.visible_ && !force_)
        return;

    glm::mat4 mv = modelview_;
    for (NodeSet::iterator node = n.begin(); node != n.end(); ++node) {
        if ( (*node)->visible_ || force_)
            (*node)->accept(*this);
        modelview_ = mv;
    }
}

void PickingVisitor::visit(Switch &n)
{
    if ((!n.visible_ && !force_) || n.numChildren()<1)
        return;

    glm::mat4 mv = modelview_;
    n.activeChild()->accept(*this);
    modelview_ = mv;
}

void PickingVisitor::visit(Primitive &)
{
    // by default, a Primitive is not interactive
}

void PickingVisitor::visit(Surface &n)
{
    if (!n.visible_ && !force_)
        return;

    // if more than one point given for testing: test overlap
    if (points_.size() > 1) {
        // create bounding box for those points (2 in practice)
        GlmToolkit::AxisAlignedBoundingBox bb_points;
        bb_points.extend(points_);
        // update the coordinates of the Surface bounding box to match transform
        GlmToolkit::AxisAlignedBoundingBox surf;
        surf = n.bbox().transformed(modelview_);
        // Test inclusion of all four corners of the Surface inside the selection bounding box
        if ( bb_points.contains( surf) ) {
            // add this surface to the nodes picked
            nodes_.push_back( std::pair(&n, glm::vec2(0.f)) );
        }
//        // ALTERNATIVE BEHAVIOR : test bounding box for overlap only
//        // apply inverse transform
//        bb_points = bb_points.transformed(glm::inverse(modelview_)) ;
//        if ( bb_points.intersect( n.bbox() ) ) {
//            // add this surface to the nodes picked
//            nodes_.push_back( std::pair(&n, glm::vec2(0.f)) );
//        }

    }
    // only one point
    else if (points_.size() > 0) {
        // apply inverse transform to the point of interest
        glm::vec4 P = glm::inverse(modelview_) * glm::vec4( points_[0], 1.f );
        // test bounding box for picking from a single point
        if ( n.bbox().contains( glm::vec3(P)) ) {
            // add this surface to the nodes picked
            nodes_.push_back( std::pair(&n, glm::vec2(P)) );
        }
    }
}

void PickingVisitor::visit(Disk &n)
{
    // discard if not visible or if not exactly one point given for picking
    if ((!n.visible_ && !force_) || points_.size() != 1)
        return;

    // apply inverse transform to the point of interest
    glm::vec4 P = glm::inverse(modelview_) * glm::vec4( points_[0], 1.f );

    // test distance for picking from a single point
    if ( glm::length(glm::vec2(P)) < 1.f )
        // add this surface to the nodes picked
        nodes_.push_back( std::pair(&n, glm::vec2(P)) );

}

void PickingVisitor::visit(Handles &n)
{
    // discard if not visible or if not exactly one point given for picking
    if ((!n.visible_ && !force_) || points_.size() != 1)
        return;

    // apply inverse transform to the point of interest
    glm::vec4 P = glm::inverse(modelview_) * glm::vec4( points_[0], 1.f );

    // inverse transform to check the scale
    glm::vec4 S = glm::inverse(modelview_) * glm::vec4( 0.05f, 0.05f, 0.f, 0.f );
    float scale = glm::length( glm::vec2(S) );

    // extract rotation from modelview
    glm::mat4 ctm;
    glm::vec3 rot(0.f);
    glm::vec4 vec = modelview_ * glm::vec4(1.f, 0.f, 0.f, 0.f);
    rot.z = glm::orientedAngle( glm::vec3(1.f, 0.f, 0.f), glm::normalize(glm::vec3(vec)), glm::vec3(0.f, 0.f, 1.f) );
    ctm = glm::rotate(glm::identity<glm::mat4>(), -rot.z, glm::vec3(0.f, 0.f, 1.f)) * modelview_ ;
    vec = ctm *  glm::vec4(1.f, 1.f, 0.f, 0.f);
    glm::vec4 mirror = glm::sign(vec);

    bool picked = false;
    if ( n.type() == Handles::RESIZE ) {
        // 4 corners
        picked = ( glm::length(glm::vec2(+1.f, +1.f)- glm::vec2(P)) < scale ||
                   glm::length(glm::vec2(+1.f, -1.f)- glm::vec2(P)) < scale ||
                   glm::length(glm::vec2(-1.f, +1.f)- glm::vec2(P)) < scale ||
                   glm::length(glm::vec2(-1.f, -1.f)- glm::vec2(P)) < scale );
    }
    else if ( n.type() == Handles::RESIZE_H ){
        // left & right
        picked = ( glm::length(glm::vec2(+1.f, 0.f)- glm::vec2(P)) < scale ||
                   glm::length(glm::vec2(-1.f, 0.f)- glm::vec2(P)) < scale );
    }
    else if ( n.type() == Handles::RESIZE_V ){
        // top & bottom
        picked = ( glm::length(glm::vec2(0.f, +1.f)- glm::vec2(P)) < scale ||
                   glm::length(glm::vec2(0.f, -1.f)- glm::vec2(P)) < scale );
    }
    else if ( n.type() == Handles::ROTATE ){
        // the icon for rotation is on the right top corner at (0.12, 0.12) in scene coordinates
        glm::vec4 pos = glm::inverse(ctm) * ( mirror * glm::vec4( 0.12f, 0.12f, 0.f, 0.f ) );
        picked  = glm::length( glm::vec2( 1.f, 1.f) + glm::vec2(pos) - glm::vec2(P) ) < 1.5f * scale;
    }
    else if ( n.type() == Handles::SCALE ){
        // the icon for scaling is on the right bottom corner at (0.12, -0.12) in scene coordinates
        glm::vec4 pos = glm::inverse(ctm) * ( mirror * glm::vec4( 0.12f, -0.12f, 0.f, 0.f ) );
        picked  = glm::length( glm::vec2( 1.f, -1.f) + glm::vec2(pos) - glm::vec2(P) ) < 1.5f * scale;
    }
    else if ( n.type() == Handles::CROP ){
        // the icon for cropping is on the left bottom corner at (0.12, 0.12) in scene coordinates
        glm::vec4 pos = glm::inverse(ctm) * ( mirror * glm::vec4( 0.12f, 0.12f, 0.f, 0.f ) );
        picked  = glm::length( glm::vec2( -1.f, -1.f) + glm::vec2(pos) - glm::vec2(P) ) < 1.5f * scale;
    }
    else if ( n.type() == Handles::MENU ){
        // the icon for menu is on the left top corner at (-0.12, 0.12) in scene coordinates
        glm::vec4 pos = glm::inverse(ctm) * ( mirror * glm::vec4( -0.12f, 0.12f, 0.f, 0.f ) );
        picked  = glm::length( glm::vec2( -1.f, 1.f) + glm::vec2(pos) - glm::vec2(P) ) < 1.5f * scale;
    }
    else if ( n.type() == Handles::LOCKED || n.type()  == Handles::UNLOCKED  ){
        // the icon for lock is on the right bottom corner at (-0.12, 0.12) in scene coordinates
        glm::vec4 pos = glm::inverse(ctm) * ( mirror * glm::vec4( -0.12f, 0.12f, 0.f, 0.f ) );
        picked  = glm::length( glm::vec2( 1.f, -1.f) + glm::vec2(pos) - glm::vec2(P) ) < 1.5f * scale;
    }

    if ( picked )
        // add this to the nodes picked
        nodes_.push_back( std::pair(&n, glm::vec2(P)) );

}


void PickingVisitor::visit(Symbol& n)
{
    // discard if not visible or if not exactly one point given for picking
    if ((!n.visible_ && !force_) || points_.size() != 1)
        return;

    // apply inverse transform to the point of interest
    glm::vec4 P = glm::inverse(modelview_) * glm::vec4( points_[0], 1.f );

    // test bounding box for picking from a single point
    if ( n.bbox().contains( glm::vec3(P)) ) {
        // add this to the nodes picked
        nodes_.push_back( std::pair(&n, glm::vec2(P)) );
    }

}

void PickingVisitor::visit(Scene &n)
{
    n.root()->accept(*this);
}
