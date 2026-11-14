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
#ifdef LINUX
#include <map>
#endif

// Platform-specific forward declarations
#ifdef LINUX
// X11 forward declarations (for XInput2)
typedef struct _XDisplay Display;
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
 * across Linux (XInput2 under X11, tablet protocol under Wayland),
 * macOS (NSEvent) and Windows
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
        // Wayland: the stylus does not move the mouse pointer of the application,
        // it gives its position and buttons when it is over the main window
        bool pointer;        // stylus is over the main window (x, y and buttons are valid)
        float x, y;          // position in main window coordinates
        // X11 (XWayland): the stylus moves the mouse pointer, but its buttons may not
        // be given as mouse buttons: they are given here
        bool clicks;         // buttons are valid (but not x, y)
        bool buttons[3];     // tip, first and second stylus buttons are pressed
        bool clicked[3];     // were pressed since the previous poll (even if released)
    };

    static TabletInput& instance();

    // Initialize tablet input system
    bool init();

    // Poll for new tablet events (call once per frame)
    void pollEvents();

    // Clean up resources
    void terminate();

    // Set the cursor shown for the stylus, as an ImGuiMouseCursor value
    // (Wayland, where the application shows the cursor of the stylus)
    void setCursor(int imgui_cursor);

    // Get current tablet data
    const TabletData& getData() const { return data_; }

    // Quick accessors
    float getPressure() const { return data_.pressure; }
    bool isPressed() const { return data_.tip_down && data_.in_proximity; }
    
    // status
    bool isEnabled() const { return active_; }
    bool hasPressure() const { return isEnabled() && data_.has_pressure; }

private:
    TabletInput();
    ~TabletInput();

    TabletData data_;
    bool active_;

#ifdef LINUX
    // native Wayland or X11 (see linux/TabletInput_linux.cpp)
    bool wayland_;
#if defined(HAVE_WAYLAND_TABLET)
    // Wayland tablet protocol (see linux/TabletInput_wayland.cpp)
    bool initWayland();
    void pollWayland();
    void terminateWayland();
    void setCursorWayland(int imgui_cursor);
#endif
#if defined(HAVE_X11TABLETINPUT)
    // X11/XInput2 (see linux/TabletInput_x11.cpp)
    bool initX11();
    void pollX11();
    void terminateX11();
    Display *display_;
    int xi_opcode_;
    struct X11Axis {
        int number = -1;
        double min = 0.0, max = 1.0;
    };
    struct X11Pen {
        X11Axis pressure, tilt_x, tilt_y;
    };
    std::map<int, X11Pen> pens_;    // XInput2 devices with a pressure axis
    void scanX11Devices();
#endif
#elif defined(APPLE)
    void* monitor_; // Event monitor handle
#endif
};

#endif // TABLETINPUT_H
