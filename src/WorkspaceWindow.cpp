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


#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

#include "defines.h"
#include "WorkspaceWindow.h"

bool WorkspaceWindow::clear_workspace_enabled = false;
std::list<WorkspaceWindow *> WorkspaceWindow::windows_;

struct ImGuiProperties
{
    ImGuiWindow *ptr;
    ImVec2 user_pos;
    ImVec2 outside_pos;
    float progress;  // [0 1]
    float target;    //  1 go to outside, 0 go to user pos
    bool  animation;     // need animation
    bool  resizing_workspace;
    ImVec2 resized_pos;

    ImGuiProperties ()
    {
        ptr = nullptr;
        progress = 0.f;
        target = 0.f;
        animation = false;
        resizing_workspace = false;
    }
};

WorkspaceWindow::WorkspaceWindow(const char* name): name_(name), impl_(nullptr)
{
    WorkspaceWindow::windows_.push_back(this);
}

void WorkspaceWindow::toggleClearRestoreWorkspace()
{
    // stop animations that are ongoing
    for(auto it = windows_.cbegin(); it != windows_.cend(); ++it) {
        if ( (*it)->impl_ && (*it)->impl_->animation )
            (*it)->impl_->animation = false;
    }

    // toggle
    if (clear_workspace_enabled)
        restoreWorkspace();
    else
        clearWorkspace();
}

void WorkspaceWindow::restoreWorkspace(bool instantaneous)
{
    if (clear_workspace_enabled) {
        const ImVec2 display_size = ImGui::GetIO().DisplaySize;
        for(auto it = windows_.cbegin(); it != windows_.cend(); ++it) {
            ImGuiProperties *impl = (*it)->impl_;
            if (impl && impl->ptr)
            {
                float margin = (impl->ptr->MenuBarHeight() + impl->ptr->TitleBarHeight()) * 3.f;
                impl->user_pos.x = CLAMP(impl->user_pos.x, -impl->ptr->SizeFull.x +margin, display_size.x -margin);
                impl->user_pos.y = CLAMP(impl->user_pos.y, -impl->ptr->SizeFull.y +margin, display_size.y -margin);

                if (instantaneous) {
                    impl->animation = false;
                    ImGui::SetWindowPos(impl->ptr, impl->user_pos);
                }
                else {
                    // remember outside position before move
                    impl->outside_pos = impl->ptr->Pos;

                    // initialize animation
                    impl->progress = 1.f;
                    impl->target = 0.f;
                    impl->animation = true;
                }
            }
        }
    }
    clear_workspace_enabled = false;
}

void WorkspaceWindow::clearWorkspace()
{
    if (!clear_workspace_enabled) {
        const ImVec2 display_size = ImGui::GetIO().DisplaySize;
        for(auto it = windows_.cbegin(); it != windows_.cend(); ++it) {
            ImGuiProperties *impl = (*it)->impl_;
            if (impl && impl->ptr)
            {
                // remember user position before move
                impl->user_pos = impl->ptr->Pos;

                // init before decision
                impl->outside_pos = impl->ptr->Pos;

                // distance to right side, top & bottom
                float right = display_size.x - (impl->ptr->Pos.x + impl->ptr->SizeFull.x * 0.7);
                float top = impl->ptr->Pos.y;
                float bottom = display_size.y - (impl->ptr->Pos.y + impl->ptr->SizeFull.y);

                // move to closest border, with a margin to keep visible
                float margin = (impl->ptr->MenuBarHeight() + impl->ptr->TitleBarHeight()) * 1.5f;
                if (top < bottom && top < right)
                    impl->outside_pos.y = margin - impl->ptr->SizeFull.y;
                else if (right < top && right < bottom)
                    impl->outside_pos.x = display_size.x - margin;
                else
                    impl->outside_pos.y = display_size.y - margin;

                // initialize animation
                impl->progress = 0.f;
                impl->target = 1.f;
                impl->animation = true;
            }
        }
    }
    clear_workspace_enabled = true;
}

void WorkspaceWindow::notifyWorkspaceSizeChanged(int prev_width, int prev_height, int curr_width, int curr_height)
{
    // restore windows pos before rescale
    restoreWorkspace(true);

    for(auto it = windows_.cbegin(); it != windows_.cend(); ++it) {
        ImGuiProperties *impl = (*it)->impl_;
        if ( impl && impl->ptr)
        {
            ImVec2 distance_to_corner = ImVec2(prev_width, prev_height) - impl->ptr->Pos - impl->ptr->SizeFull;

            impl->resized_pos = impl->ptr->Pos;

            if ( ABS(distance_to_corner.x) < 100.f ) {
                impl->resized_pos.x += curr_width - prev_width;
                impl->resizing_workspace = true;
            }

            if ( ABS(distance_to_corner.y) < 100.f ) {
                impl->resized_pos.y += curr_height -prev_height;
                impl->resizing_workspace = true;
            }
        }
    }
}

void WorkspaceWindow::Update()
{
    // get ImGui pointer to window (available only after first run)
    if (!impl_) {
        ImGuiWindow *w = ImGui::FindWindowByName(name_);
        if (w != NULL) {
            impl_ = new ImGuiProperties;
            impl_->ptr = w;
            impl_->user_pos = w->Pos;
        }
    }
    else
    {
        if ( Visible() ) {
            // perform animation for clear/restore workspace
            if (impl_->animation) {
                // increment animation progress by small steps
                impl_->progress += SIGN(impl_->target -impl_->progress) * 0.1f;
                // finished animation : apply full target position
                if (ABS_DIFF(impl_->target, impl_->progress) < 0.05f) {
                    impl_->animation = false;
                    ImVec2 pos = impl_->user_pos * (1.f -impl_->target) + impl_->outside_pos * impl_->target;
                    ImGui::SetWindowPos(impl_->ptr, pos);
                }
                // move window by interpolation between user position and outside target position
                else {
                    ImVec2 pos = impl_->user_pos * (1.f -impl_->progress) + impl_->outside_pos * impl_->progress;
                    ImGui::SetWindowPos(impl_->ptr, pos);
                }
            }
            // Restore if clic on overlay
            if (clear_workspace_enabled)
            {
                // draw another window on top of the WorkspaceWindow, at exact same position and size
                ImGuiWindow* window = impl_->ptr;
                ImGui::SetNextWindowPos(window->Pos, ImGuiCond_Always);
                ImGui::SetNextWindowSize(window->Size, ImGuiCond_Always);
                char nameoverlay[64];
                sprintf(nameoverlay, "%sOverlay", name_);
                if (ImGui::Begin(nameoverlay, NULL, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings ))
                {
                    // exit workspace clear mode if user clics on the window
                    ImGui::InvisibleButton("##dummy", window->Size);
                    if (ImGui::IsItemClicked())
                        WorkspaceWindow::restoreWorkspace();
                    ImGui::End();
                }
            }
        }
        // move windows because workspace was resized
        if (impl_->resizing_workspace) {
            // how far from the target ?
            ImVec2 delta = impl_->resized_pos - impl_->ptr->Pos;
            // got close enough to stop workspace resize
            if (ABS(delta.x) < 2.f && ABS(delta.y) < 2.f)
                impl_->resizing_workspace = false;
            // dichotomic reach of target position
            ImVec2 pos = impl_->ptr->Pos + delta * 0.5f;
            ImGui::SetWindowPos(impl_->ptr, pos);
        }
    }
}
