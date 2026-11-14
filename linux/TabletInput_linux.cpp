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


/*
 * Tablet input on Linux: under native Wayland with the tablet protocol
 * (TabletInput_wayland.cpp), otherwise under X11 with XInput2 (TabletInput_x11.cpp)
 */

#include "TabletInput.h"
#include "Log.h"
#include "RenderingManager.h"

TabletInput::TabletInput()
    : data_({0.0f, false, 0.0f, 0.0f, false, false})
    , active_(false)
    , wayland_(false)
#ifdef HAVE_X11TABLETINPUT
    , display_(nullptr)
    , xi_opcode_(-1)
#endif
{
}

TabletInput::~TabletInput()
{
    terminate();
}

bool TabletInput::init()
{
    wayland_ = Rendering::manager().wayland();

    // native Wayland: XInput2 would only receive events of XWayland windows
    if (wayland_) {
#ifdef HAVE_WAYLAND_TABLET
        return initWayland();
#else
        Log::Info("TabletInput: no Wayland tablet support - tablet support disabled");
        return false;
#endif
    }

#ifdef HAVE_X11TABLETINPUT
    return initX11();
#else
    Log::Info("TabletInput: XInput2 not available - tablet support disabled");
    return false;
#endif
}

void TabletInput::pollEvents()
{
#ifdef HAVE_WAYLAND_TABLET
    if (wayland_)
        pollWayland();
#endif
#ifdef HAVE_X11TABLETINPUT
    if (!wayland_)
        pollX11();
#endif
}

void TabletInput::terminate()
{
#ifdef HAVE_WAYLAND_TABLET
    if (wayland_)
        terminateWayland();
#endif
#ifdef HAVE_X11TABLETINPUT
    if (!wayland_)
        terminateX11();
#endif
    active_ = false;
}

void TabletInput::setCursor(int imgui_cursor)
{
#ifdef HAVE_WAYLAND_TABLET
    if (wayland_)
        setCursorWayland(imgui_cursor);
#else
    (void) imgui_cursor;
#endif
}
