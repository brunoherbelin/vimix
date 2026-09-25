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
#ifndef SOURCEPANEL_H
#define SOURCEPANEL_H

#include "NavigatorPanel.h"

class Source;
class Navigator;
struct ImVec2;

///
/// Side pannel to configure the current source:
/// name, properties (ImGuiVisitor), transcoding and actions on the source.
///
class SourcePanel : public NavigatorPanel
{
public:
    void Render(Navigator *navigator, Source *s, const ImVec2 &iconsize, bool reset = false);
};

#endif /* SOURCEPANEL_H */
