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

#ifndef NAVIGATORINTERNAL_H
#define NAVIGATORINTERNAL_H

// Common preamble for all the files implementing the Navigator pannels:
// imgui with math operators and internals (ImVec2 arithmetic, ImRect, GImGui).
// NB: not included by Navigator.h, which is public (UserInterfaceManager.h).

#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnontrivial-memcall"
#endif
#include "imgui_internal.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#include <string>
#include <vector>
#include <utility>

// Icons and tooltips of the multistate button selecting the ordering of a
// list of files; shared by the New source pannel and the Playlist pannel.
extern std::vector< std::pair<int, int> > icons_ordering_files;
extern std::vector< std::string > tooltips_ordering_files;

#endif /* NAVIGATORINTERNAL_H */
