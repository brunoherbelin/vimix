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
#ifndef SETTINGSPANEL_H
#define SETTINGSPANEL_H

#include "NavigatorPanel.h"

///
/// Settings mode of the Navigator main pannel:
/// Appearance, Recording, Streaming, OSC, Gamepad and System preferences.
///
class SettingsPanel : public NavigatorPanel
{
public:
    void Render();
};

#endif /* SETTINGSPANEL_H */
