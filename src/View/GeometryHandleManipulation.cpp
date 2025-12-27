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

#include <iomanip>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <glm/gtx/component_wise.hpp>

#include "IconsFontAwesome5.h"
#include "Scene/Scene.h"
#include "Scene/Primitives.h"
#include "Scene/Decorations.h"
#include "FrameBuffer.h"
#include "UserInterfaceManager.h"

#include "GeometryHandleManipulation.h"


namespace GeometryHandleManipulation {

// Handle NODE_LOWER_LEFT manipulation
void handleNodeLowerLeft(HandleGrabContext& ctx)
{
    // hide other grips
    ctx.handles[Handles::CROP_H]->visible_ = false;
    ctx.handles[Handles::CROP_V]->visible_ = false;
    ctx.handles[Handles::ROUNDING]->visible_ = false;
    ctx.handles[Handles::MENU]->visible_ = false;
    ctx.handles[Handles::EDIT_CROP]->visible_ = false;
    // get stored status
    glm::vec3 node_pos = glm::vec3(ctx.stored_status->data_[0].x,
                                   ctx.stored_status->data_[0].y,
                                   0.f);
    // Compute target coordinates of manipulated handle into SCENE reference frame
    node_pos = ctx.target_to_scene_transform * glm::vec4(node_pos, 1.f);
    // apply translation of target in SCENE
    node_pos = glm::translate(glm::identity<glm::mat4>(), ctx.scene_to - ctx.scene_from)  * glm::vec4(node_pos, 1.f);
    // snap handle coordinates to grid (if active)
    if ( ctx.grid->active() )
        node_pos = ctx.grid->snap(node_pos);
    // Compute handle coordinates back in TARGET reference frame
    node_pos = ctx.scene_to_target_transform * glm::vec4(node_pos, 1.f);
    // Diagonal SCALING with SHIFT
    if (UserInterface::manager().shiftModifier())
        node_pos.y = node_pos.x;
    // apply to target Node and to handles
    ctx.targetNode->data_[0].x = CLAMP( node_pos.x, 0.f, 0.96f );
    ctx.targetNode->data_[0].y = CLAMP( node_pos.y, 0.f, 0.96f );
    ctx.info << "Corner low-left " << std::fixed << std::setprecision(3) << ctx.targetNode->data_[0].x;
    ctx.info << " x "  << ctx.targetNode->data_[0].y;
}

// Handle NODE_UPPER_LEFT manipulation
void handleNodeUpperLeft(HandleGrabContext& ctx)
{
    // hide other grips
    ctx.handles[Handles::CROP_H]->visible_ = false;
    ctx.handles[Handles::CROP_V]->visible_ = false;
    ctx.handles[Handles::ROUNDING]->visible_ = false;
    ctx.handles[Handles::MENU]->visible_ = false;
    ctx.handles[Handles::EDIT_CROP]->visible_ = false;
    // get stored status
    glm::vec3 node_pos = glm::vec3(ctx.stored_status->data_[1].x,
                                   ctx.stored_status->data_[1].y,
                                   0.f);
    // Compute target coordinates of manipulated handle into SCENE reference frame
    node_pos = ctx.target_to_scene_transform * glm::vec4(node_pos, 1.f);
    // apply translation of target in SCENE
    node_pos = glm::translate(glm::identity<glm::mat4>(), ctx.scene_to - ctx.scene_from)  * glm::vec4(node_pos, 1.f);
    // snap handle coordinates to grid (if active)
    if ( ctx.grid->active() )
        node_pos = ctx.grid->snap(node_pos);
    // Compute handle coordinates back in TARGET reference frame
    node_pos = ctx.scene_to_target_transform * glm::vec4(node_pos, 1.f);
    // Diagonal SCALING with SHIFT
    if (UserInterface::manager().shiftModifier())
        node_pos.y = -node_pos.x;
    // apply to target Node and to handles
    ctx.targetNode->data_[1].x = CLAMP( node_pos.x, 0.f, 0.96f );
    ctx.targetNode->data_[1].y = CLAMP( node_pos.y, -0.96f, 0.f );
    ctx.info << "Corner up-left " << std::fixed << std::setprecision(3) << ctx.targetNode->data_[1].x;
    ctx.info << " x "  << ctx.targetNode->data_[1].y;
}

// Handle NODE_LOWER_RIGHT manipulation
void handleNodeLowerRight(HandleGrabContext& ctx)
{
    // hide other grips
    ctx.handles[Handles::CROP_H]->visible_ = false;
    ctx.handles[Handles::CROP_V]->visible_ = false;
    ctx.handles[Handles::ROUNDING]->visible_ = false;
    ctx.handles[Handles::MENU]->visible_ = false;
    ctx.handles[Handles::EDIT_CROP]->visible_ = false;
    // get stored status
    glm::vec3 node_pos = glm::vec3(ctx.stored_status->data_[2].x,
                                   ctx.stored_status->data_[2].y,
                                   0.f);
    // Compute target coordinates of manipulated handle into SCENE reference frame
    node_pos = ctx.target_to_scene_transform * glm::vec4(node_pos, 1.f);
    // apply translation of target in SCENE
    node_pos = glm::translate(glm::identity<glm::mat4>(), ctx.scene_to - ctx.scene_from)  * glm::vec4(node_pos, 1.f);
    // snap handle coordinates to grid (if active)
    if ( ctx.grid->active() )
        node_pos = ctx.grid->snap(node_pos);
    // Compute handle coordinates back in TARGET reference frame
    node_pos = ctx.scene_to_target_transform * glm::vec4(node_pos, 1.f);
    // Diagonal SCALING with SHIFT
    if (UserInterface::manager().shiftModifier())
        node_pos.y = -node_pos.x;
    // apply to target Node and to handles
    ctx.targetNode->data_[2].x = CLAMP( node_pos.x, -0.96f, 0.f );
    ctx.targetNode->data_[2].y = CLAMP( node_pos.y, 0.f, 0.96f );
    ctx.info << "Corner low-right " << std::fixed << std::setprecision(3) << ctx.targetNode->data_[2].x;
    ctx.info << " x "  << ctx.targetNode->data_[2].y;
}

// Handle NODE_UPPER_RIGHT manipulation
void handleNodeUpperRight(HandleGrabContext& ctx)
{
    // hide other grips
    ctx.handles[Handles::CROP_H]->visible_ = false;
    ctx.handles[Handles::CROP_V]->visible_ = false;
    ctx.handles[Handles::ROUNDING]->visible_ = false;
    ctx.handles[Handles::MENU]->visible_ = false;
    ctx.handles[Handles::EDIT_CROP]->visible_ = false;

    // get stored status
    glm::vec3 node_pos = glm::vec3(ctx.stored_status->data_[3].x,
                                   ctx.stored_status->data_[3].y,
                                   0.f);
    // Compute target coordinates of manipulated handle into SCENE reference frame
    node_pos = ctx.target_to_scene_transform * glm::vec4(node_pos, 1.f);
    // apply translation of target in SCENE
    node_pos = glm::translate(glm::identity<glm::mat4>(), ctx.scene_to - ctx.scene_from)  * glm::vec4(node_pos, 1.f);
    // snap handle coordinates to grid (if active)
    if ( ctx.grid->active() )
        node_pos = ctx.grid->snap(node_pos);
    // Compute handle coordinates back in TARGET reference frame
    node_pos = ctx.scene_to_target_transform * glm::vec4(node_pos, 1.f);
    // Diagonal SCALING with SHIFT
    if (UserInterface::manager().shiftModifier())
        node_pos.y = node_pos.x;
    // apply to target Node and to handles
    ctx.targetNode->data_[3].x = CLAMP( node_pos.x, -0.96f, 0.f );
    ctx.targetNode->data_[3].y = CLAMP( node_pos.y, -0.96f, 0.f );
    ctx.info << "Corner up-right " << std::fixed << std::setprecision(3) << ctx.targetNode->data_[3].x;
    ctx.info << " x "  << ctx.targetNode->data_[3].y;
}

// Handle CROP_H (horizontal crop) manipulation
void handleCropH(HandleGrabContext& ctx)
{
    // hide all other grips
    ctx.handles[Handles::NODE_LOWER_RIGHT]->visible_ = false;
    ctx.handles[Handles::NODE_LOWER_LEFT]->visible_ = false;
    ctx.handles[Handles::NODE_UPPER_RIGHT]->visible_ = false;
    ctx.handles[Handles::NODE_UPPER_LEFT]->visible_ = false;
    ctx.handles[Handles::ROUNDING]->visible_ = false;
    ctx.handles[Handles::CROP_V]->visible_ = false;
    ctx.handles[Handles::MENU]->visible_ = false;
    ctx.handles[Handles::EDIT_CROP]->visible_ = false;

    // prepare overlay frame showing full image size
    glm::vec3 _c_s = glm::vec3(ctx.stored_status->crop_[0] - ctx.stored_status->crop_[1],
                               ctx.stored_status->crop_[2] - ctx.stored_status->crop_[3],
                               2.f) * 0.5f;
    ctx.overlay_crop->scale_ = ctx.stored_status->scale_ / _c_s;
    ctx.overlay_crop->scale_.x *= ctx.frame->aspectRatio();
    ctx.overlay_crop->rotation_.z = ctx.stored_status->rotation_.z;
    ctx.overlay_crop->translation_ = ctx.stored_status->translation_;
    glm::vec3 _t((ctx.stored_status->crop_[1] + _c_s.x) * ctx.overlay_crop->scale_.x,
                 (-ctx.stored_status->crop_[2] + _c_s.y) * ctx.overlay_crop->scale_.y, 0.f);
    _t = glm::rotate(glm::identity<glm::mat4>(),
                     ctx.overlay_crop->rotation_.z,
                     glm::vec3(0.f, 0.f, 1.f)) * glm::vec4(_t, 1.f);
    ctx.overlay_crop->translation_ += _t;
    ctx.overlay_crop->translation_.z = 0.f;
    ctx.overlay_crop->update(0);
    ctx.overlay_crop->visible_ = true;

    // Manipulate the handle in the SCENE coordinates to apply grid snap
    glm::vec4 handle = ctx.corner_to_scene_transform * glm::vec4(ctx.corner * 2.f, 0.f, 1.f );
    handle = glm::translate(glm::identity<glm::mat4>(), ctx.scene_to - ctx.scene_from) * handle;
    if ( ctx.grid->active() )
        handle = ctx.grid->snap(handle);

    handle = ctx.scene_to_corner_transform * handle;
    glm::vec2 handle_scaling = glm::vec2(handle.x, 1.f) / glm::vec2(ctx.corner.x * 2.f, 1.f);

    // Apply transform to the CROP
    if (ctx.corner.x > 0.f) {
        ctx.targetNode->crop_[1] = CLAMP(ctx.stored_status->crop_[0]
                                         + (ctx.stored_status->crop_[1] - ctx.stored_status->crop_[0]) * handle_scaling.x,
                                         0.1f, 1.f);
    } else {
        ctx.targetNode->crop_[0] = CLAMP(ctx.stored_status->crop_[1]
                                         - (ctx.stored_status->crop_[1] - ctx.stored_status->crop_[0]) * handle_scaling.x,
                                         -1.f, -0.1f);
    }

    handle_scaling.x = (ctx.targetNode->crop_[1] - ctx.targetNode->crop_[0])
                       / (ctx.stored_status->crop_[1] - ctx.stored_status->crop_[0]);

    // Adjust scale and translation
    glm::vec4 corner_center = glm::vec4(ctx.corner, 0.f, 1.f);
    corner_center = glm::scale(glm::identity<glm::mat4>(), glm::vec3(handle_scaling, 1.f)) * corner_center;
    corner_center = ctx.corner_to_scene_transform * corner_center;

    ctx.targetNode->translation_ = glm::vec3(corner_center);
    ctx.targetNode->scale_ = ctx.stored_status->scale_ * glm::vec3(handle_scaling, 1.f);

    float c = tan(ctx.targetNode->rotation_.z);
    ctx.cursor.type = ABS(c) > 1.f ? View::Cursor_ResizeNS : View::Cursor_ResizeEW;
    ctx.info << "Crop H " << std::fixed << std::setprecision(3) << ctx.targetNode->crop_[0];
    ctx.info << " x " << ctx.targetNode->crop_[1];
}

// Handle CROP_V (vertical crop) manipulation
void handleCropV(HandleGrabContext& ctx)
{
    // hide all other grips
    ctx.handles[Handles::NODE_LOWER_RIGHT]->visible_ = false;
    ctx.handles[Handles::NODE_LOWER_LEFT]->visible_ = false;
    ctx.handles[Handles::NODE_UPPER_RIGHT]->visible_ = false;
    ctx.handles[Handles::NODE_UPPER_LEFT]->visible_ = false;
    ctx.handles[Handles::ROUNDING]->visible_ = false;
    ctx.handles[Handles::CROP_H]->visible_ = false;
    ctx.handles[Handles::MENU]->visible_ = false;
    ctx.handles[Handles::EDIT_CROP]->visible_ = false;

    // prepare overlay frame showing full image size
    glm::vec3 _c_s = glm::vec3(ctx.stored_status->crop_[0] - ctx.stored_status->crop_[1],
                               ctx.stored_status->crop_[2] - ctx.stored_status->crop_[3],
                               2.f) * 0.5f;
    ctx.overlay_crop->scale_ = ctx.stored_status->scale_ / _c_s;
    ctx.overlay_crop->scale_.x *= ctx.frame->aspectRatio();
    ctx.overlay_crop->rotation_.z = ctx.stored_status->rotation_.z;
    ctx.overlay_crop->translation_ = ctx.stored_status->translation_;
    glm::vec3 _t((ctx.stored_status->crop_[1] + _c_s.x) * ctx.overlay_crop->scale_.x,
                 (-ctx.stored_status->crop_[2] + _c_s.y) * ctx.overlay_crop->scale_.y, 0.f);
    _t = glm::rotate(glm::identity<glm::mat4>(),
                     ctx.overlay_crop->rotation_.z,
                     glm::vec3(0.f, 0.f, 1.f)) * glm::vec4(_t, 1.f);
    ctx.overlay_crop->translation_ += _t;
    ctx.overlay_crop->translation_.z = 0.f;
    ctx.overlay_crop->update(0);
    ctx.overlay_crop->visible_ = true;

    // Manipulate the handle in the SCENE coordinates to apply grid snap
    glm::vec4 handle = ctx.corner_to_scene_transform * glm::vec4(ctx.corner * 2.f, 0.f, 1.f );
    handle = glm::translate(glm::identity<glm::mat4>(), ctx.scene_to - ctx.scene_from) * handle;
    if ( ctx.grid->active() )
        handle = ctx.grid->snap(handle);

    handle = ctx.scene_to_corner_transform * handle;
    glm::vec2 handle_scaling = glm::vec2(1.f, handle.y) / glm::vec2(1.f, ctx.corner.y * 2.f);

    // Apply transform to the CROP
    if (ctx.corner.y > 0.f) {
        ctx.targetNode->crop_[2] = CLAMP(ctx.stored_status->crop_[3]
                                         + (ctx.stored_status->crop_[2] - ctx.stored_status->crop_[3]) * handle_scaling.y,
                                         0.1f, 1.f);
    } else {
        ctx.targetNode->crop_[3] = CLAMP(ctx.stored_status->crop_[2]
                                         - (ctx.stored_status->crop_[2] - ctx.stored_status->crop_[3]) * handle_scaling.y,
                                         -1.f, -0.1f);
    }

    handle_scaling.y = (ctx.targetNode->crop_[2] - ctx.targetNode->crop_[3])
                       / (ctx.stored_status->crop_[2] - ctx.stored_status->crop_[3]);

    // Adjust scale and translation
    glm::vec4 corner_center = glm::vec4(ctx.corner, 0.f, 1.f);
    corner_center = glm::scale(glm::identity<glm::mat4>(), glm::vec3(handle_scaling, 1.f)) * corner_center;
    corner_center = ctx.corner_to_scene_transform * corner_center;

    ctx.targetNode->translation_ = glm::vec3(corner_center);
    ctx.targetNode->scale_ = ctx.stored_status->scale_ * glm::vec3(handle_scaling, 1.f);

    float c = tan(ctx.targetNode->rotation_.z);
    ctx.cursor.type = ABS(c) > 1.f ? View::Cursor_ResizeEW : View::Cursor_ResizeNS;
    ctx.info << "Crop V " << std::fixed << std::setprecision(3) << ctx.targetNode->crop_[2];
    ctx.info << " x "  << ctx.targetNode->crop_[3];
}

// Handle ROUNDING manipulation
void handleRounding(HandleGrabContext& ctx)
{
    // hide other grips
    ctx.handles[Handles::CROP_H]->visible_ = false;
    ctx.handles[Handles::CROP_V]->visible_ = false;
    ctx.handles[Handles::MENU]->visible_ = false;
    ctx.handles[Handles::EDIT_CROP]->visible_ = false;
    // get stored status
    glm::vec3 node_pos = glm::vec3( -ctx.stored_status->data_[0].w, 0.f, 0.f);
    // Compute target coordinates of manipulated handle into SCENE reference frame
    node_pos = ctx.target_to_scene_transform * glm::vec4(node_pos, 1.f);
    // apply translation of target in SCENE
    node_pos = glm::translate(glm::identity<glm::mat4>(), ctx.scene_to - ctx.scene_from) * glm::vec4(node_pos, 1.f);
    // Compute handle coordinates back in TARGET reference frame
    node_pos = ctx.scene_to_target_transform * glm::vec4(node_pos, 1.f);

    // apply to target Node and to handles
    ctx.targetNode->data_[0].w = - CLAMP( node_pos.x, -1.f, 0.f );
    ctx.info << "Corner round " << std::fixed << std::setprecision(3) << ctx.targetNode->data_[0].w;
}

// Handle RESIZE (corner resize) manipulation
void handleResize(HandleGrabContext& ctx)
{
    // hide  other grips
    ctx.handles[Handles::SCALE]->visible_ = false;
    ctx.handles[Handles::RESIZE_H]->visible_ = false;
    ctx.handles[Handles::RESIZE_V]->visible_ = false;
    ctx.handles[Handles::ROTATE]->visible_ = false;
    ctx.handles[Handles::EDIT_SHAPE]->visible_ = false;
    ctx.handles[Handles::MENU]->visible_ = false;
    // inform on which corner should be overlayed (opposite)
    ctx.handles[Handles::RESIZE]->overlayActiveCorner(-ctx.corner);

    // Manipulate the handle in the SCENE coordinates to apply grid snap
    glm::vec4 handle = ctx.corner_to_scene_transform * glm::vec4(ctx.corner * 2.f, 0.f, 1.f );
    handle = glm::translate(glm::identity<glm::mat4>(), ctx.scene_to - ctx.scene_from) * handle;
    if ( ctx.grid->active() )
        handle = ctx.grid->snap(handle);
    handle = ctx.scene_to_corner_transform * handle;
    glm::vec2 corner_scaling = glm::vec2(handle) / glm::vec2(ctx.corner * 2.f);

    // proportional SCALING with SHIFT
    if (UserInterface::manager().shiftModifier()) {
        corner_scaling = glm::vec2(glm::compMax(corner_scaling));
    }

    // Apply scaling to the target
    ctx.targetNode->scale_ = ctx.stored_status->scale_ * glm::vec3(corner_scaling, 1.f);

    // Adjust translation
    glm::vec4 corner_center = glm::vec4( ctx.corner, 0.f, 1.f);
    corner_center = glm::scale(glm::identity<glm::mat4>(), glm::vec3(corner_scaling, 1.f)) * corner_center;
    corner_center = ctx.corner_to_scene_transform * corner_center;
    ctx.targetNode->translation_ = glm::vec3(corner_center);

    // show cursor depending on diagonal (corner picked)
    glm::mat4 T = glm::rotate(glm::identity<glm::mat4>(), ctx.stored_status->rotation_.z, glm::vec3(0.f, 0.f, 1.f));
    T = glm::scale(T, ctx.stored_status->scale_);
    glm::vec2 corner = T * glm::vec4( ctx.corner, 0.f, 0.f );
    ctx.cursor.type = corner.x * corner.y > 0.f ? View::Cursor_ResizeNESW : View::Cursor_ResizeNWSE;
    ctx.info << "Size " << std::fixed << std::setprecision(3) << ctx.targetNode->scale_.x;
    ctx.info << " x "  << ctx.targetNode->scale_.y;
}

// Handle RESIZE_H (horizontal resize) manipulation
void handleResizeH(HandleGrabContext& ctx)
{
    // hide all other grips
    ctx.handles[Handles::RESIZE]->visible_ = false;
    ctx.handles[Handles::SCALE]->visible_ = false;
    ctx.handles[Handles::RESIZE_V]->visible_ = false;
    ctx.handles[Handles::ROTATE]->visible_ = false;
    ctx.handles[Handles::EDIT_SHAPE]->visible_ = false;
    ctx.handles[Handles::MENU]->visible_ = false;
    // inform on which corner should be overlayed (opposite)
    ctx.handles[Handles::RESIZE_H]->overlayActiveCorner(-ctx.corner);

    // Manipulate the handle in the SCENE coordinates to apply grid snap
    glm::vec4 handle = ctx.corner_to_scene_transform * glm::vec4(ctx.corner * 2.f, 0.f, 1.f );
    handle = glm::translate(glm::identity<glm::mat4>(), ctx.scene_to - ctx.scene_from) * handle;
    if ( ctx.grid->active() )
        handle = ctx.grid->snap(handle);
    handle = ctx.scene_to_corner_transform * handle;
    glm::vec2 corner_scaling =  glm::vec2(handle.x, 1.f) / glm::vec2(ctx.corner.x * 2.f, 1.f);

    // Apply scaling to the target
    ctx.targetNode->scale_ = ctx.stored_status->scale_ * glm::vec3(corner_scaling, 1.f);

    // SHIFT: restore previous aspect ratio
    if (UserInterface::manager().shiftModifier()) {
        float ar = ctx.stored_status->scale_.y / ctx.stored_status->scale_.x;
        ctx.targetNode->scale_.y = ar * ctx.targetNode->scale_.x;
    }

    // Adjust translation
    glm::vec4 corner_center = glm::vec4( ctx.corner, 0.f, 1.f);
    corner_center = glm::scale(glm::identity<glm::mat4>(), glm::vec3(corner_scaling, 1.f)) * corner_center;
    corner_center = ctx.corner_to_scene_transform * corner_center;
    ctx.targetNode->translation_ = glm::vec3(corner_center);

    // show cursor depending on angle
    float c = tan(ctx.targetNode->rotation_.z);
    ctx.cursor.type = ABS(c) > 1.f ? View::Cursor_ResizeNS : View::Cursor_ResizeEW;
    ctx.info << "Size " << std::fixed << std::setprecision(3) << ctx.targetNode->scale_.x;
    ctx.info << " x "  << ctx.targetNode->scale_.y;
}

// Handle RESIZE_V (vertical resize) manipulation
void handleResizeV(HandleGrabContext& ctx)
{
    // hide all other grips
    ctx.handles[Handles::RESIZE]->visible_ = false;
    ctx.handles[Handles::SCALE]->visible_ = false;
    ctx.handles[Handles::RESIZE_H]->visible_ = false;
    ctx.handles[Handles::ROTATE]->visible_ = false;
    ctx.handles[Handles::EDIT_SHAPE]->visible_ = false;
    ctx.handles[Handles::MENU]->visible_ = false;
    // inform on which corner should be overlayed (opposite)
    ctx.handles[Handles::RESIZE_V]->overlayActiveCorner(-ctx.corner);

    // Manipulate the handle in the SCENE coordinates to apply grid snap
    glm::vec4 handle = ctx.corner_to_scene_transform * glm::vec4(ctx.corner * 2.f, 0.f, 1.f );
    handle = glm::translate(glm::identity<glm::mat4>(), ctx.scene_to - ctx.scene_from) * handle;
    if ( ctx.grid->active() )
        handle = ctx.grid->snap(handle);
    handle = ctx.scene_to_corner_transform * handle;
    glm::vec2 corner_scaling =  glm::vec2(1.f, handle.y) / glm::vec2(1.f, ctx.corner.y * 2.f);

    // Apply scaling to the target
    ctx.targetNode->scale_ = ctx.stored_status->scale_ * glm::vec3(corner_scaling, 1.f);

    // SHIFT: restore previous aspect ratio
    if (UserInterface::manager().shiftModifier()) {
        float ar = ctx.stored_status->scale_.x / ctx.stored_status->scale_.y;
        ctx.targetNode->scale_.x = ar * ctx.targetNode->scale_.y;
    }

    // Adjust translation
    glm::vec4 corner_center = glm::vec4( ctx.corner, 0.f, 1.f);
    corner_center = glm::scale(glm::identity<glm::mat4>(), glm::vec3(corner_scaling, 1.f)) * corner_center;
    corner_center = ctx.corner_to_scene_transform * corner_center;
    ctx.targetNode->translation_ = glm::vec3(corner_center);

    // show cursor depending on angle
    float c = tan(ctx.targetNode->rotation_.z);
    ctx.cursor.type = ABS(c) > 1.f ? View::Cursor_ResizeEW : View::Cursor_ResizeNS;
    ctx.info << "Size " << std::fixed << std::setprecision(3) << ctx.targetNode->scale_.x;
    ctx.info << " x "  << ctx.targetNode->scale_.y;
}

// Handle SCALE (center scaling) manipulation
void handleScale(HandleGrabContext& ctx)
{
    // hide all other grips
    ctx.handles[Handles::RESIZE]->visible_ = false;
    ctx.handles[Handles::RESIZE_H]->visible_ = false;
    ctx.handles[Handles::RESIZE_V]->visible_ = false;
    ctx.handles[Handles::ROTATE]->visible_ = false;
    ctx.handles[Handles::EDIT_SHAPE]->visible_ = false;
    ctx.handles[Handles::MENU]->visible_ = false;
    // prepare overlay
    ctx.overlay_scaling_cross->visible_ = false;
    ctx.overlay_scaling->visible_ = true;
    ctx.overlay_scaling->translation_.x = ctx.stored_status->translation_.x;
    ctx.overlay_scaling->translation_.y = ctx.stored_status->translation_.y;
    ctx.overlay_scaling->rotation_.z = ctx.stored_status->rotation_.z;
    ctx.overlay_scaling->update(0);

    // Manipulate the scaling handle in the SCENE coordinates to apply grid snap
    glm::vec3 handle = glm::vec3( glm::round(ctx.pick.second), 0.f);
    handle = ctx.target_to_scene_transform * glm::vec4( handle, 1.f );
    handle = glm::translate(glm::identity<glm::mat4>(), ctx.scene_to - ctx.scene_from) * glm::vec4( handle, 1.f );
    if ( ctx.grid->active() )
        handle = ctx.grid->snap(handle);
    handle = ctx.scene_to_target_transform * glm::vec4( handle,  1.f );
    glm::vec2 handle_scaling = glm::vec2(handle) / glm::round(ctx.pick.second);

    // proportional SCALING with SHIFT
    if (UserInterface::manager().shiftModifier()) {
        handle_scaling = glm::vec2(glm::compMax(handle_scaling));
        ctx.overlay_scaling_cross->visible_ = true;
        ctx.overlay_scaling_cross->copyTransform(ctx.overlay_scaling);
    }
    // Apply scaling to the target
    ctx.targetNode->scale_ = ctx.stored_status->scale_ * glm::vec3(handle_scaling, 1.f);

    // show cursor depending on diagonal
    glm::vec2 corner = glm::sign(ctx.targetNode->scale_);
    ctx.cursor.type = (corner.x * corner.y) > 0.f ? View::Cursor_ResizeNWSE : View::Cursor_ResizeNESW;
    ctx.info << "Size " << std::fixed << std::setprecision(3) << ctx.targetNode->scale_.x;
    ctx.info << " x "  << ctx.targetNode->scale_.y;
}

// Handle ROTATE manipulation
void handleRotate(HandleGrabContext& ctx)
{
    // hide all other grips
    ctx.handles[Handles::RESIZE]->visible_ = false;
    ctx.handles[Handles::RESIZE_H]->visible_ = false;
    ctx.handles[Handles::RESIZE_V]->visible_ = false;
    ctx.handles[Handles::SCALE]->visible_ = false;
    ctx.handles[Handles::EDIT_SHAPE]->visible_ = false;
    ctx.handles[Handles::MENU]->visible_ = false;
    // ROTATION on CENTER
    ctx.overlay_rotation->visible_ = true;
    ctx.overlay_rotation->translation_.x = ctx.stored_status->translation_.x;
    ctx.overlay_rotation->translation_.y = ctx.stored_status->translation_.y;
    ctx.overlay_rotation->update(0);
    ctx.overlay_rotation_fix->visible_ = false;
    ctx.overlay_rotation_fix->copyTransform(ctx.overlay_rotation);
    ctx.overlay_rotation_clock_hand->visible_ = true;
    ctx.overlay_rotation_clock_hand->translation_.x = ctx.stored_status->translation_.x;
    ctx.overlay_rotation_clock_hand->translation_.y = ctx.stored_status->translation_.y;

    // prepare variables
    const float diagonal = glm::length( glm::vec2(ctx.frame->aspectRatio() * ctx.stored_status->scale_.x, ctx.stored_status->scale_.y));
    glm::vec2 handle_polar = glm::vec2(diagonal, 0.f);

    // rotation center to center of target (disregarding scale)
    glm::mat4 M = glm::translate(glm::identity<glm::mat4>(), ctx.stored_status->translation_);
    glm::vec3 target_from = glm::inverse(M) * glm::vec4( ctx.scene_from,  1.f );
    glm::vec3 target_to   = glm::inverse(M) * glm::vec4( ctx.scene_to,  1.f );

    // compute rotation angle on Z axis
    float angle = glm::orientedAngle( glm::normalize(glm::vec2(target_from)), glm::normalize(glm::vec2(target_to)));
    handle_polar.y = ctx.stored_status->rotation_.z + angle;
    ctx.info << "Angle " << std::fixed << std::setprecision(1) << glm::degrees(ctx.targetNode->rotation_.z) << UNICODE_DEGREE;

    // compute scaling of diagonal to reach new coordinates
    handle_polar.x *= glm::length( glm::vec2(target_to) ) / glm::length( glm::vec2(target_from) );

    // snap polar coordiantes (diagonal lenght, angle)
    if ( ctx.grid->active() ) {
        handle_polar = glm::round( handle_polar / ctx.grid->step() ) * ctx.grid->step();
        // prevent null size
        handle_polar.x = glm::max( ctx.grid->step().x,  handle_polar.x );
    }

    // Cancel scaling diagonal with SHIFT
    if (UserInterface::manager().shiftModifier()) {
        handle_polar.x = diagonal;
        ctx.overlay_rotation_fix->visible_ = true;
    } else {
        ctx.info << std::endl << "   Size " << std::fixed << std::setprecision(3) << ctx.targetNode->scale_.x;
        ctx.info << " x "  << ctx.targetNode->scale_.y ;
    }

    // apply after snap
    ctx.targetNode->rotation_ = glm::vec3(0.f, 0.f, handle_polar.y);
    handle_polar.x /= diagonal ;
    ctx.targetNode->scale_ = ctx.stored_status->scale_ * glm::vec3(handle_polar.x, handle_polar.x, 1.f);

    // update overlay
    ctx.overlay_rotation_clock_hand->rotation_.z = ctx.targetNode->rotation_.z;
    ctx.overlay_rotation_clock_hand->update(0);

    // show cursor for rotation
    ctx.cursor.type = View::Cursor_Hand;
}

} // namespace GeometryHandleManipulation
