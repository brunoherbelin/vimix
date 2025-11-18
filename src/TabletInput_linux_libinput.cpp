/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2025 Bruno Herbelin <bruno.herbelin@gmail.com>
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

#include <glib.h>
#ifdef LINUX

#include "TabletInput.h"
#include "Log.h"

#ifdef HAVE_LIBINPUT
#include <libinput.h>
#include <libudev.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>

static int open_restricted(const char *path, int flags, void *user_data)
{
    int fd = open(path, flags);
    return fd < 0 ? -errno : fd;
}

static void close_restricted(int fd, void *user_data)
{
    close(fd);
}

static const struct libinput_interface interface = {
    .open_restricted = open_restricted,
    .close_restricted = close_restricted,
};

TabletInput::TabletInput()
    : data_({0.0f, false, 0.0f, 0.0f, false, false})
    , active_(false)
    , udev_(nullptr)
    , li_(nullptr)
    , fd_(-1)
{
}

TabletInput::~TabletInput()
{
    terminate();
}

bool TabletInput::init()
{
    udev_ = udev_new();
    if (!udev_) {
        Log::Info("TabletInput: Failed to initialize udev");
        return false;
    }

    li_ = libinput_udev_create_context(&interface, nullptr, udev_);
    if (!li_) {
        Log::Info("TabletInput: Failed to create libinput context");
        udev_unref(udev_);
        udev_ = nullptr;
        return false;
    }

    if (libinput_udev_assign_seat(li_, "seat0") != 0) {
        Log::Info("TabletInput: Failed to assign seat");
        libinput_unref(li_);
        li_ = nullptr;
        udev_unref(udev_);
        udev_ = nullptr;
        return false;
    }

    fd_ = libinput_get_fd(li_);

    // Set non-blocking
    int flags = fcntl(fd_, F_GETFL, 0);
    fcntl(fd_, F_SETFL, flags | O_NONBLOCK);

    active_ = true;
    Log::Info("TabletInput: Linux tablet input initialized (libinput)");
    return true;
}

void TabletInput::pollEvents()
{
    if (!li_) return;

    libinput_dispatch(li_);

    struct libinput_event *event;
    while ((event = libinput_get_event(li_)) != nullptr) {
        enum libinput_event_type type = libinput_event_get_type(event);

        switch (type) {
            case LIBINPUT_EVENT_TABLET_TOOL_AXIS:
            case LIBINPUT_EVENT_TABLET_TOOL_TIP:
            case LIBINPUT_EVENT_TABLET_TOOL_PROXIMITY: {
                struct libinput_event_tablet_tool *tablet_event =
                    libinput_event_get_tablet_tool_event(event);

                // Update pressure
                if (libinput_event_tablet_tool_pressure_has_changed(tablet_event)) {
                    data_.has_pressure = true;
                    data_.pressure = libinput_event_tablet_tool_get_pressure(tablet_event);
                }

                // Update tilt (if available)
                if (libinput_event_tablet_tool_tilt_x_has_changed(tablet_event)) {
                    data_.tilt_x = libinput_event_tablet_tool_get_tilt_x(tablet_event) / 90.0f;
                }
                if (libinput_event_tablet_tool_tilt_y_has_changed(tablet_event)) {
                    data_.tilt_y = libinput_event_tablet_tool_get_tilt_y(tablet_event) / 90.0f;
                }

                // Update proximity
                if (type == LIBINPUT_EVENT_TABLET_TOOL_PROXIMITY) {
                    data_.in_proximity = (libinput_event_tablet_tool_get_proximity_state(tablet_event)
                                         == LIBINPUT_TABLET_TOOL_PROXIMITY_STATE_IN);
                    if (!data_.in_proximity) {
                        data_.has_pressure = true;
                        data_.pressure = 0.0f;
                        data_.tip_down = false;
                    }
                }

                // Update tip state
                if (type == LIBINPUT_EVENT_TABLET_TOOL_TIP) {
                    data_.tip_down = (libinput_event_tablet_tool_get_tip_state(tablet_event)
                                     == LIBINPUT_TABLET_TOOL_TIP_DOWN);
                    if (!data_.tip_down) {
                        data_.has_pressure = true;
                        data_.pressure = 0.0f;
                    }
                }
                break;
            }
            default:
                break;
        }

        libinput_event_destroy(event);
    }
}

void TabletInput::terminate()
{
    if (li_) {
        libinput_unref(li_);
        li_ = nullptr;
    }
    if (udev_) {
        udev_unref(udev_);
        udev_ = nullptr;
    }
    active_ = false;
}

#else // !HAVE_LIBINPUT

// Stub implementation when libinput is not available

TabletInput::TabletInput()
    : data_({0.0f, false, 0.0f, 0.0f, false, false})
    , active_(false)
{
}

TabletInput::~TabletInput()
{
}

bool TabletInput::init()
{
    Log::Info("TabletInput: libinput not available - tablet pressure support disabled");
    active_ = false;
    return false;
}

void TabletInput::pollEvents()
{
    // No-op when libinput is not available
}

void TabletInput::terminate()
{
    active_ = false;
}

#endif // HAVE_LIBINPUT

#endif // LINUX
