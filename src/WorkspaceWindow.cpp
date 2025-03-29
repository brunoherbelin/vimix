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
    float progress;  // [0 1]
    bool  animation; // need animation
    bool  resizing_workspace;
    bool  hidden;
    bool  collapsed;
    ImVec2 user_pos;
    ImVec2 user_size;
    ImVec2 hidden_pos;
    ImVec2 resized_pos;

    ImGuiProperties ()
    {
        ptr = nullptr;
        progress = 0.f;
        animation = false;
        resizing_workspace = false;
        hidden = false;
        collapsed = false;
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

void WorkspaceWindow::setHidden(bool h, bool force)
{
    // hide if valid pointer and if status 'hidden' is changing (or forcing entry)
    if (impl_ && impl_->ptr && ( impl_->hidden != h || force )) {

        // set status
        impl_->hidden = h;

        // utility variables
        const ImVec2 display_size = ImGui::GetIO().DisplaySize;

        if (h) {
            // remember user position before animation
            impl_->user_pos = impl_->ptr->Pos;

            // distance to right side, top & bottom
            float right = display_size.x - (impl_->ptr->Pos.x + impl_->ptr->SizeFull.x * 0.7);
            float top = impl_->ptr->Pos.y;
            float bottom = display_size.y - (impl_->ptr->Pos.y + impl_->ptr->SizeFull.y);

            // hidden_pos starts from user position,
            // + moved to closest border, with a margin to keep visible
            impl_->hidden_pos = impl_->ptr->Pos;
            float margin = (impl_->ptr->MenuBarHeight() + impl_->ptr->TitleBarHeight()) * 1.5f;
            if (top < bottom && top < right)
                impl_->hidden_pos.y = margin - impl_->ptr->SizeFull.y;
            else if (right < top && right < bottom)
                impl_->hidden_pos.x = display_size.x - margin;
            else
                impl_->hidden_pos.y = display_size.y - margin;

            if (force) {
                impl_->animation = false;
                ImGui::SetWindowPos(impl_->ptr, impl_->hidden_pos);
            }
            else {
                // initialize animation
                impl_->progress = 0.f;
                impl_->animation = true;
            }

        }
        else {
            // remember outside position before animation
            impl_->hidden_pos = impl_->ptr->Pos;

            float margin = (impl_->ptr->MenuBarHeight() + impl_->ptr->TitleBarHeight()) * 3.f;
            impl_->user_pos.x = CLAMP(impl_->user_pos.x,
                                      -impl_->ptr->SizeFull.x + margin,
                                      display_size.x - margin);
            impl_->user_pos.y = CLAMP(impl_->user_pos.y,
                                      -impl_->ptr->SizeFull.y + margin,
                                      display_size.y - margin);

            if (force) {
                impl_->animation = false;
                // ImGui::SetWindowPos(impl_->ptr, impl_->user_pos);
            }
            else {
                // initialize animation
                impl_->progress = 1.f;
                impl_->animation = true;
            }
        }
    }
}

void WorkspaceWindow::setCollapsed(bool c)
{
    // hide if valid pointer and if status 'collapsed' is changing
    if (impl_ && impl_->ptr && impl_->collapsed != c && !impl_->hidden) {
        impl_->collapsed = c;

        ImVec2 s = impl_->ptr->SizeFull;
        if (c) {
            impl_->user_size = s;
            s.y = impl_->ptr->MenuBarHeight() * 2.3f;
        } else
            s.y = impl_->user_size.y;

        ImGui::SetWindowSize(impl_->ptr, s);
    }
}

void WorkspaceWindow::restoreWorkspace(bool force)
{
    if (clear_workspace_enabled || force) {
        // const ImVec2 display_size = ImGui::GetIO().DisplaySize;
        for(auto it = windows_.cbegin(); it != windows_.cend(); ++it) {
            if (force)
                (*it)->setCollapsed(false);
            (*it)->setHidden(false, force);
        }
    }
    clear_workspace_enabled = false;
}

void WorkspaceWindow::clearWorkspace()
{
    if (!clear_workspace_enabled) {
        // const ImVec2 display_size = ImGui::GetIO().DisplaySize;
        for(auto it = windows_.cbegin(); it != windows_.cend(); ++it) {
            (*it)->setHidden(true);
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
                const float target = impl_->hidden ? 1.f : 0.f;
                // increment animation progress by small steps
                impl_->progress += SIGN(target -impl_->progress) * 0.1f;
                // finished animation : apply full target position
                if (ABS_DIFF(target, impl_->progress) < 0.05f) {
                    impl_->animation = false;
                    ImVec2 pos = impl_->user_pos * (1.f -target) + impl_->hidden_pos * target;
                    ImGui::SetWindowPos(impl_->ptr, pos);
                }
                // move window by interpolation between user position and outside target position
                else {
                    ImVec2 pos = impl_->user_pos * (1.f -impl_->progress) + impl_->hidden_pos * impl_->progress;
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
                snprintf(nameoverlay, 64, "%sOverlay", name_);
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
