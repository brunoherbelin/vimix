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

#include "NavigatorInternal.h"

#include "NavigatorPanel.h"

NavigatorPanel::NavigatorPanel() : width_(0.f), pannel_width_(0.f), height_(0.f), padding_width_(0.f), pannel_alpha_(0.95f)
{
}

void NavigatorPanel::setGeometry(float bar_width, float pannel_width, float height, float padding, float alpha)
{
    width_ = bar_width;
    pannel_width_ = pannel_width;
    height_ = height;
    padding_width_ = padding;
    pannel_alpha_ = alpha;
}

bool NavigatorPanel::beginPannelWindow(const char *name)
{
    // Next window is a side pannel
    ImGui::SetNextWindowPos( ImVec2(width_, 0), ImGuiCond_Always );
    ImGui::SetNextWindowSize( ImVec2(pannel_width_, height_), ImGuiCond_Always );
    ImGui::SetNextWindowBgAlpha( pannel_alpha_ ); // Transparent background
    return ImGui::Begin(name, NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |  ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
}
