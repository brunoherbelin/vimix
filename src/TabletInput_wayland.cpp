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
 * Tablet input on native Wayland, with the tablet protocol (tablet-v2).
 *
 * Objects are created on the Wayland connection of GLFW, with a separate event
 * queue: GLFW reads the events of the connection (glfwPollEvents), and the events
 * of the tablet are dispatched from that queue in pollEvents().
 *
 * NB: once the application uses the tablet protocol, the compositor does not
 * emulate the mouse pointer with the stylus anymore: the position and buttons
 * of the stylus over the main window are given in TabletData (pointer, x, y,
 * buttons), and the cursor of the stylus is set by the application.
 */

#include <cstring>
#include <set>
#include <linux/input-event-codes.h>

#include <wayland-client.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WAYLAND
#include <GLFW/glfw3native.h>

#include "tablet-v2-client-protocol.h"
#ifdef HAVE_WAYLAND_CURSOR_SHAPE
#include "cursor-shape-v1-client-protocol.h"
#endif

#include "Log.h"
#include "RenderingManager.h"
#include "TabletInput.h"

// cursor values of ImGui (ImGuiMouseCursor)
#define CURSOR_UNSET -2
#define CURSOR_NONE  -1

namespace {

struct WaylandTablet
{
    struct wl_display *display = nullptr;
    struct wl_event_queue *queue = nullptr;
    struct wl_registry *registry = nullptr;
    struct wl_seat *seat = nullptr;
    struct zwp_tablet_manager_v2 *manager = nullptr;
    struct zwp_tablet_seat_v2 *tablet_seat = nullptr;
#ifdef HAVE_WAYLAND_CURSOR_SHAPE
    struct wp_cursor_shape_manager_v1 *shape_manager = nullptr;
#endif
    struct wl_surface *surface = nullptr;     // surface of the main window
    TabletInput::TabletData *data = nullptr;
    std::set<struct zwp_tablet_v2 *> tablets;
    std::set<struct zwp_tablet_tool_v2 *> tools;

    // stylus over the main window
    struct zwp_tablet_tool_v2 *tool = nullptr;
    uint32_t proximity_serial = 0;
    int cursor = CURSOR_NONE + 1;             // requested cursor (arrow by default)
    int cursor_applied = CURSOR_UNSET;        // cursor shown for the stylus
};

WaylandTablet wl_;

void leave_proximity()
{
    wl_.tool = nullptr;
    wl_.cursor_applied = CURSOR_UNSET;
    wl_.data->pointer = false;
    wl_.data->in_proximity = false;
    wl_.data->tip_down = false;
    for (int i = 0; i < 3; ++i)
        wl_.data->buttons[i] = false;
}

void apply_cursor()
{
    if (wl_.tool == nullptr || wl_.cursor == wl_.cursor_applied)
        return;
    wl_.cursor_applied = wl_.cursor;

    // hide the cursor
    if (wl_.cursor == CURSOR_NONE) {
        zwp_tablet_tool_v2_set_cursor(wl_.tool, wl_.proximity_serial, nullptr, 0, 0);
        return;
    }

#ifdef HAVE_WAYLAND_CURSOR_SHAPE
    // otherwise the compositor shows its default cursor
    if (wl_.shape_manager == nullptr)
        return;

    // shapes corresponding to ImGuiMouseCursor values (Arrow, TextInput, ResizeAll,
    // ResizeNS, ResizeEW, ResizeNESW, ResizeNWSE, Hand, NotAllowed)
    static const uint32_t shapes[] = {
        WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT,
        WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_TEXT,
        WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_MOVE,
        WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NS_RESIZE,
        WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_EW_RESIZE,
        WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NESW_RESIZE,
        WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NWSE_RESIZE,
        WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_POINTER,
        WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NOT_ALLOWED,
    };
    const int n = sizeof(shapes) / sizeof(shapes[0]);
    const uint32_t shape = (wl_.cursor >= 0 && wl_.cursor < n) ? shapes[wl_.cursor] : shapes[0];

    struct wp_cursor_shape_device_v1 *device =
        wp_cursor_shape_manager_v1_get_tablet_tool_v2(wl_.shape_manager, wl_.tool);
    wp_cursor_shape_device_v1_set_shape(device, wl_.proximity_serial, shape);
    wp_cursor_shape_device_v1_destroy(device);
#endif
}

//
// Tool (stylus)
//
void tool_type(void *, struct zwp_tablet_tool_v2 *, uint32_t) {}
void tool_hardware_serial(void *, struct zwp_tablet_tool_v2 *, uint32_t, uint32_t) {}
void tool_hardware_id(void *, struct zwp_tablet_tool_v2 *, uint32_t, uint32_t) {}

void tool_capability(void *, struct zwp_tablet_tool_v2 *, uint32_t capability)
{
    if (capability == ZWP_TABLET_TOOL_V2_CAPABILITY_PRESSURE)
        wl_.data->has_pressure = true;
}

void tool_done(void *, struct zwp_tablet_tool_v2 *) {}

void tool_removed(void *, struct zwp_tablet_tool_v2 *tool)
{
    if (tool == wl_.tool)
        leave_proximity();
    wl_.tools.erase(tool);
    zwp_tablet_tool_v2_destroy(tool);
}

void tool_proximity_in(void *, struct zwp_tablet_tool_v2 *tool, uint32_t serial,
                       struct zwp_tablet_v2 *, struct wl_surface *surface)
{
    // only the main window uses the stylus (not the output windows)
    if (surface != wl_.surface)
        return;
    wl_.tool = tool;
    wl_.proximity_serial = serial;
    wl_.data->pointer = true;
    wl_.data->in_proximity = true;
    apply_cursor();
}

void tool_proximity_out(void *, struct zwp_tablet_tool_v2 *tool)
{
    if (tool == wl_.tool)
        leave_proximity();
}

void tool_down(void *, struct zwp_tablet_tool_v2 *tool, uint32_t)
{
    if (tool != wl_.tool)
        return;
    wl_.data->tip_down = true;
    wl_.data->buttons[0] = true;
    wl_.data->clicked[0] = true;
}

void tool_up(void *, struct zwp_tablet_tool_v2 *tool)
{
    if (tool != wl_.tool)
        return;
    wl_.data->tip_down = false;
    wl_.data->buttons[0] = false;
}

void tool_motion(void *, struct zwp_tablet_tool_v2 *tool, wl_fixed_t x, wl_fixed_t y)
{
    if (tool != wl_.tool)
        return;
    wl_.data->x = (float) wl_fixed_to_double(x);
    wl_.data->y = (float) wl_fixed_to_double(y);
}

void tool_pressure(void *, struct zwp_tablet_tool_v2 *tool, uint32_t pressure)
{
    if (tool == wl_.tool)
        wl_.data->pressure = (float) pressure / 65535.f;
}

void tool_distance(void *, struct zwp_tablet_tool_v2 *, uint32_t) {}

void tool_tilt(void *, struct zwp_tablet_tool_v2 *tool, wl_fixed_t tilt_x, wl_fixed_t tilt_y)
{
    if (tool != wl_.tool)
        return;
    // degrees (-90 to 90) to -1.0 to 1.0
    wl_.data->tilt_x = (float) wl_fixed_to_double(tilt_x) / 90.f;
    wl_.data->tilt_y = (float) wl_fixed_to_double(tilt_y) / 90.f;
}

void tool_rotation(void *, struct zwp_tablet_tool_v2 *, wl_fixed_t) {}
void tool_slider(void *, struct zwp_tablet_tool_v2 *, int32_t) {}
void tool_wheel(void *, struct zwp_tablet_tool_v2 *, wl_fixed_t, int32_t) {}

void tool_button(void *, struct zwp_tablet_tool_v2 *tool, uint32_t, uint32_t button, uint32_t state)
{
    if (tool != wl_.tool)
        return;
    // first stylus button as right mouse button, second as middle
    const int index = button == BTN_STYLUS ? 1 : button == BTN_STYLUS2 ? 2 : -1;
    if (index < 0)
        return;
    const bool pressed = state == ZWP_TABLET_TOOL_V2_BUTTON_STATE_PRESSED;
    wl_.data->buttons[index] = pressed;
    if (pressed)
        wl_.data->clicked[index] = true;
}

void tool_frame(void *, struct zwp_tablet_tool_v2 *, uint32_t) {}

const struct zwp_tablet_tool_v2_listener tool_listener = {
    tool_type, tool_hardware_serial, tool_hardware_id, tool_capability, tool_done,
    tool_removed, tool_proximity_in, tool_proximity_out, tool_down, tool_up,
    tool_motion, tool_pressure, tool_distance, tool_tilt, tool_rotation,
    tool_slider, tool_wheel, tool_button, tool_frame
};

//
// Tablet (device)
//
void tablet_name(void *, struct zwp_tablet_v2 *, const char *name)
{
    Log::Info("Tablet '%s' connected", name);
}

void tablet_id(void *, struct zwp_tablet_v2 *, uint32_t, uint32_t) {}
void tablet_path(void *, struct zwp_tablet_v2 *, const char *) {}
void tablet_done(void *, struct zwp_tablet_v2 *) {}

void tablet_removed(void *, struct zwp_tablet_v2 *tablet)
{
    Log::Info("Tablet disconnected");
    wl_.tablets.erase(tablet);
    zwp_tablet_v2_destroy(tablet);
}

void tablet_bustype(void *, struct zwp_tablet_v2 *, uint32_t) {}

const struct zwp_tablet_v2_listener tablet_listener = {
    tablet_name, tablet_id, tablet_path, tablet_done, tablet_removed, tablet_bustype
};

//
// Tablet seat: tablets and tools, connected now or later
//
void seat_tablet_added(void *, struct zwp_tablet_seat_v2 *, struct zwp_tablet_v2 *tablet)
{
    wl_.tablets.insert(tablet);
    zwp_tablet_v2_add_listener(tablet, &tablet_listener, nullptr);
}

void seat_tool_added(void *, struct zwp_tablet_seat_v2 *, struct zwp_tablet_tool_v2 *tool)
{
    wl_.tools.insert(tool);
    zwp_tablet_tool_v2_add_listener(tool, &tool_listener, nullptr);
}

void seat_pad_added(void *, struct zwp_tablet_seat_v2 *, struct zwp_tablet_pad_v2 *pad)
{
    // pads (buttons, rings, strips of the tablet) are not used
    zwp_tablet_pad_v2_destroy(pad);
}

const struct zwp_tablet_seat_v2_listener seat_listener = {
    seat_tablet_added, seat_tool_added, seat_pad_added
};

//
// Registry
//
void registry_global(void *, struct wl_registry *registry, uint32_t name,
                     const char *interface, uint32_t)
{
    if (strcmp(interface, zwp_tablet_manager_v2_interface.name) == 0)
        wl_.manager = (struct zwp_tablet_manager_v2 *)
            wl_registry_bind(registry, name, &zwp_tablet_manager_v2_interface, 1);
#ifdef HAVE_WAYLAND_CURSOR_SHAPE
    else if (strcmp(interface, wp_cursor_shape_manager_v1_interface.name) == 0)
        wl_.shape_manager = (struct wp_cursor_shape_manager_v1 *)
            wl_registry_bind(registry, name, &wp_cursor_shape_manager_v1_interface, 1);
#endif
    else if (strcmp(interface, wl_seat_interface.name) == 0 && wl_.seat == nullptr)
        wl_.seat = (struct wl_seat *) wl_registry_bind(registry, name, &wl_seat_interface, 1);
}

void registry_global_remove(void *, struct wl_registry *, uint32_t) {}

const struct wl_registry_listener registry_listener = {
    registry_global, registry_global_remove
};

} // namespace

bool TabletInput::initWayland()
{
    wl_.display = glfwGetWaylandDisplay();
    wl_.surface = glfwGetWaylandWindow(Rendering::manager().mainWindow().window());
    if (wl_.display == nullptr || wl_.surface == nullptr)
        return false;
    wl_.data = &data_;

    // objects of the tablet receive their events in a separate queue
    wl_.queue = wl_display_create_queue(wl_.display);
    struct wl_display *wrapper = (struct wl_display *) wl_proxy_create_wrapper(wl_.display);
    wl_proxy_set_queue((struct wl_proxy *) wrapper, wl_.queue);
    wl_.registry = wl_display_get_registry(wrapper);
    wl_proxy_wrapper_destroy(wrapper);
    wl_registry_add_listener(wl_.registry, &registry_listener, nullptr);
    wl_display_roundtrip_queue(wl_.display, wl_.queue);

    if (wl_.manager == nullptr || wl_.seat == nullptr) {
        Log::Info("TabletInput: the compositor has no tablet support");
        terminateWayland();
        return false;
    }

    wl_.tablet_seat = zwp_tablet_manager_v2_get_tablet_seat(wl_.manager, wl_.seat);
    zwp_tablet_seat_v2_add_listener(wl_.tablet_seat, &seat_listener, nullptr);
    // receive the tablets connected now
    wl_display_roundtrip_queue(wl_.display, wl_.queue);

    active_ = true;
    Log::Info("TabletInput: Wayland tablet input initialized");
    return true;
}

void TabletInput::pollWayland()
{
    if (wl_.queue == nullptr)
        return;

    // clicks are given for one poll
    for (int i = 0; i < 3; ++i)
        data_.clicked[i] = false;

    // events were read from the connection by glfwPollEvents()
    wl_display_dispatch_queue_pending(wl_.display, wl_.queue);
}

void TabletInput::setCursorWayland(int imgui_cursor)
{
    if (wl_.queue == nullptr)
        return;
    wl_.cursor = imgui_cursor;
    if (wl_.tool != nullptr && wl_.cursor != wl_.cursor_applied) {
        apply_cursor();
        wl_display_flush(wl_.display);
    }
}

void TabletInput::terminateWayland()
{
    if (wl_.queue == nullptr)
        return;

    for (auto tool : wl_.tools)
        zwp_tablet_tool_v2_destroy(tool);
    for (auto tablet : wl_.tablets)
        zwp_tablet_v2_destroy(tablet);
    if (wl_.tablet_seat)
        zwp_tablet_seat_v2_destroy(wl_.tablet_seat);
    if (wl_.manager)
        zwp_tablet_manager_v2_destroy(wl_.manager);
#ifdef HAVE_WAYLAND_CURSOR_SHAPE
    if (wl_.shape_manager)
        wp_cursor_shape_manager_v1_destroy(wl_.shape_manager);
#endif
    if (wl_.seat)
        wl_seat_destroy(wl_.seat);
    if (wl_.registry)
        wl_registry_destroy(wl_.registry);
    wl_display_flush(wl_.display);
    wl_event_queue_destroy(wl_.queue);

    struct TabletInput::TabletData *data = wl_.data;
    wl_ = WaylandTablet();
    if (data) {
        data->pointer = false;
        data->in_proximity = false;
        data->tip_down = false;
    }
    active_ = false;
}
