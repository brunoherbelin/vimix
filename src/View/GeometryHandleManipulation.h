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

#ifndef GEOMETRYHANDLEMANIPULATION_H
#define GEOMETRYHANDLEMANIPULATION_H

#include <sstream>
#include <utility>

#include <glm/glm.hpp>

#include "View.h"

// Forward declarations
class Source;
class Group;
class Node;
class Symbol;
class Grid;
class Handles;
class FrameBuffer;

namespace GeometryHandleManipulation {

// Generic context structure for handle manipulation
// Works for both Source and Canvas manipulation
struct HandleGrabContext {
    // Target being manipulated (source or canvas root)
    Group* targetNode;
    Group* stored_status;
    FrameBuffer* frame;

    // Scene transformation data
    glm::vec3 scene_from;
    glm::vec3 scene_to;
    glm::mat4 scene_to_target_transform;
    glm::mat4 target_to_scene_transform;
    glm::mat4 scene_to_corner_transform;
    glm::mat4 corner_to_scene_transform;
    glm::vec2 corner;

    // Picking and interaction
    std::pair<Node *, glm::vec2> pick;
    Grid* grid;
    std::ostringstream& info;
    View::Cursor& cursor;

    // Handles array (for hiding/showing other handles)
    Handles** handles;

    // Overlays (for visual feedback during manipulation)
    Node* overlay_crop;
    Symbol* overlay_scaling;
    Symbol* overlay_scaling_cross;
    Symbol* overlay_rotation;
    Symbol* overlay_rotation_fix;
    Node* overlay_rotation_clock_hand;
};

// Handle manipulation functions for different handle types
void handleNodeLowerLeft(HandleGrabContext& ctx);
void handleNodeUpperLeft(HandleGrabContext& ctx);
void handleNodeLowerRight(HandleGrabContext& ctx);
void handleNodeUpperRight(HandleGrabContext& ctx);
void handleCropH(HandleGrabContext& ctx);
void handleCropV(HandleGrabContext& ctx);
void handleRounding(HandleGrabContext& ctx);
void handleResize(HandleGrabContext& ctx);
void handleResizeH(HandleGrabContext& ctx);
void handleResizeV(HandleGrabContext& ctx);
void handleScale(HandleGrabContext& ctx);
void handleRotate(HandleGrabContext& ctx);

} // namespace GeometryHandleManipulation

#endif // GEOMETRYHANDLEMANIPULATION_H
