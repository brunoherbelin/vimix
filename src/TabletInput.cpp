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

#include "TabletInput.h"

TabletInput& TabletInput::instance()
{
    static TabletInput instance;
    return instance;
}

void TabletInput::setCursor(int imgui_cursor)
{
#if defined(LINUX) && defined(HAVE_WAYLAND_TABLET)
    if (wayland_)
        setCursorWayland(imgui_cursor);
#else
    (void) imgui_cursor;
#endif
}

// Platform-specific implementations are in separate files:
// - TabletInput_linux.cpp (Linux: XInput2 under X11, or TabletInput_wayland.cpp)
// - osx/TabletInput_macos.mm (macOS with NSEvent)
// - TabletInput_win32.cpp (Windows)
