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

#ifndef TABLETINPUT_H
#define TABLETINPUT_H

#include <glm/glm.hpp>

// Platform-specific forward declarations
#if defined(LINUX) && defined(HAVE_LIBINPUT)
struct libinput;
struct udev;
#elif defined(APPLE)
#ifdef __OBJC__
@class NSEvent;
#else
class NSEvent;
#endif
#endif

/**
 * @brief Cross-platform tablet/stylus input manager
 *
 * Provides normalized pressure values (0.0-1.0) from pen/stylus devices
 * across Linux (libinput) and macOS (NSEvent)
 */
class TabletInput {
public:
    struct TabletData {
        float pressure;      // 0.0 - 1.0
        bool has_pressure;   // Stylius has pressure
        float tilt_x;        // -1.0 to 1.0 (optional)
        float tilt_y;        // -1.0 to 1.0 (optional)
        bool in_proximity;   // Is stylus near/touching surface
        bool tip_down;       // Is stylus tip pressed
    };

    static TabletInput& instance();

    // Initialize tablet input system
    bool init(void* platform_handle = nullptr);

    // Poll for new tablet events (call once per frame)
    void pollEvents();

    // Clean up resources
    void terminate();

    // Get current tablet data
    const TabletData& getData() const { return data_; }

    // Quick accessors
    float getPressure() const { return data_.pressure; }
    bool isPressed() const { return data_.tip_down || data_.in_proximity; }
    
    // status
    bool isEnabled() const { return active_; }
    bool hasPressure() const { return isEnabled() && data_.has_pressure; }

private:
    TabletInput();
    ~TabletInput();

    TabletData data_;
    bool active_;

#if defined(LINUX) && defined(HAVE_LIBINPUT)
    struct udev *udev_;
    struct libinput *li_;
    int fd_;
#elif defined(APPLE)
    void* monitor_; // Event monitor handle
#endif
};

#endif // TABLETINPUT_H
