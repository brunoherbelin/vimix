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
 * Tablet input under X11 (and XWayland), with the XInput2 extension
 * (selected by TabletInput_linux.cpp)
 */

#include <GLFW/glfw3.h>
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>

#include "TabletInput.h"
#include "Log.h"
#include "RenderingManager.h"

bool TabletInput::initX11()
{
    // Open X11 display connection
    display_ = XOpenDisplay(nullptr);
    if (!display_) {
        Log::Info("TabletInput: Failed to open X11 display");
        return false;
    }

    // Check for XInput2 extension
    int event, error;
    if (!XQueryExtension(display_, "XInputExtension", &xi_opcode_, &event, &error)) {
        Log::Info("TabletInput: XInput extension not available");
        XCloseDisplay(display_);
        display_ = nullptr;
        return false;
    }

    // Check XInput2 version
    int major = 2, minor = 2;
    if (XIQueryVersion(display_, &major, &minor) != Success) {
        Log::Info("TabletInput: XInput2 2.2 not available");
        XCloseDisplay(display_);
        display_ = nullptr;
        return false;
    }

    // Select events of all devices on the root window: motion and buttons of the
    // stylus, and changes of devices (tablet connected or disconnected).
    // NB: selecting on the root window keeps the delivery of mouse events to the
    // application windows (selecting on the window would take them over).
    unsigned char mask[XIMaskLen(XI_LASTEVENT)] = {0};
    XISetMask(mask, XI_Motion);
    XISetMask(mask, XI_ButtonPress);
    XISetMask(mask, XI_ButtonRelease);
    XISetMask(mask, XI_HierarchyChanged);
    XIEventMask eventmask;
    eventmask.deviceid = XIAllDevices;
    eventmask.mask_len = sizeof(mask);
    eventmask.mask = mask;
    XISelectEvents(display_, DefaultRootWindow(display_), &eventmask, 1);

    // find the tablets connected now
    scanX11Devices();

    XFlush(display_);
    active_ = true;
    Log::Info("TabletInput: X11/XInput2 tablet input initialized");
    return true;
}

void TabletInput::scanX11Devices()
{
    pens_.clear();

    Atom pressure_atom = XInternAtom(display_, "Abs Pressure", True);
    Atom tilt_x_atom = XInternAtom(display_, "Abs Tilt X", True);
    Atom tilt_y_atom = XInternAtom(display_, "Abs Tilt Y", True);

    // a stylus is a slave pointer device with a pressure axis
    int ndevices = 0;
    XIDeviceInfo *devices = XIQueryDevice(display_, XIAllDevices, &ndevices);
    for (int i = 0; i < ndevices; i++) {
        XIDeviceInfo *device = &devices[i];
        if (device->use != XISlavePointer && device->use != XIFloatingSlave)
            continue;

        X11Pen pen;
        for (int j = 0; j < device->num_classes; j++) {
            if (device->classes[j]->type != XIValuatorClass)
                continue;
            XIValuatorClassInfo *v = (XIValuatorClassInfo *) device->classes[j];
            X11Axis axis;
            axis.number = v->number;
            axis.min = v->min;
            axis.max = v->max > v->min ? v->max : v->min + 1.0;
            if (pressure_atom != None && v->label == pressure_atom)
                pen.pressure = axis;
            else if (tilt_x_atom != None && v->label == tilt_x_atom)
                pen.tilt_x = axis;
            else if (tilt_y_atom != None && v->label == tilt_y_atom)
                pen.tilt_y = axis;
        }

        if (pen.pressure.number >= 0) {
            pens_[device->deviceid] = pen;
            Log::Info("TabletInput: Tablet '%s' (pressure axis %d)", device->name, pen.pressure.number);
        }
    }
    XIFreeDeviceInfo(devices);

    data_.has_pressure = !pens_.empty();
    data_.clicks = !pens_.empty();
    if (pens_.empty()) {
        data_.in_proximity = false;
        data_.tip_down = false;
        for (int i = 0; i < 3; ++i)
            data_.buttons[i] = false;
    }
}

// value of an axis in the valuators of an event, normalized from 0 to 1 (or -1 if absent)
static double axis_value(const XIValuatorState &valuators, int number, double min, double max)
{
    if (number < 0 || number >= valuators.mask_len * 8 || !XIMaskIsSet(valuators.mask, number))
        return -1.0;
    // values are packed: count the valuators set before this one
    int index = 0;
    for (int i = 0; i < number; i++)
        if (XIMaskIsSet(valuators.mask, i))
            index++;
    const double v = (valuators.values[index] - min) / (max - min);
    return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v);
}

void TabletInput::pollX11()
{
    if (!display_) return;

    // clicks are given for one poll
    for (int i = 0; i < 3; ++i)
        data_.clicked[i] = false;

    // Process all pending X11 events
    while (XPending(display_) > 0) {
        XEvent ev;
        XNextEvent(display_, &ev);

        if (ev.type != GenericEvent || ev.xcookie.extension != xi_opcode_
            || !XGetEventData(display_, &ev.xcookie))
            continue;

        // a tablet was connected or disconnected
        if (ev.xcookie.evtype == XI_HierarchyChanged) {
            scanX11Devices();
        }
        else if (ev.xcookie.evtype == XI_Motion || ev.xcookie.evtype == XI_ButtonPress
                 || ev.xcookie.evtype == XI_ButtonRelease) {

            XIDeviceEvent *e = (XIDeviceEvent *) ev.xcookie.data;
            // events of a stylus (the same event is also received from the
            // master pointer device: only the one of the stylus device is used)
            auto pen = pens_.find(e->sourceid);
            if (e->deviceid == e->sourceid && pen != pens_.end()) {

                data_.in_proximity = true;

                // pressure and tilt (tilt from -1 to 1)
                const X11Pen &p = pen->second;
                double v = axis_value(e->valuators, p.pressure.number, p.pressure.min, p.pressure.max);
                if (v >= 0.0)
                    data_.pressure = (float) v;
                v = axis_value(e->valuators, p.tilt_x.number, p.tilt_x.min, p.tilt_x.max);
                if (v >= 0.0)
                    data_.tilt_x = (float) (v * 2.0 - 1.0);
                v = axis_value(e->valuators, p.tilt_y.number, p.tilt_y.min, p.tilt_y.max);
                if (v >= 0.0)
                    data_.tilt_y = (float) (v * 2.0 - 1.0);

                // buttons: 1 is the tip, 3 and 2 the stylus buttons (as right and middle)
                const int index = e->detail == 1 ? 0 : e->detail == 3 ? 1 : e->detail == 2 ? 2 : -1;
                if (index >= 0 && ev.xcookie.evtype == XI_ButtonPress) {
                    // only when pressed over the main window of vimix
                    // NB: GLFW_HOVERED is false with the stylus under XWayland,
                    // but the cursor position is correct
                    GLFWwindow *main = Rendering::manager().mainWindow().window();
                    double x = -1.0, y = -1.0;
                    int w = 0, h = 0;
                    if (main) {
                        glfwGetCursorPos(main, &x, &y);
                        glfwGetWindowSize(main, &w, &h);
                    }
                    if (x >= 0.0 && y >= 0.0 && x < (double) w && y < (double) h) {
                        // the window manager does not focus the window on a click of
                        // the stylus (not a mouse click), but the UI needs the focus
                        if (!glfwGetWindowAttrib(main, GLFW_FOCUSED))
                            glfwFocusWindow(main);
                        data_.buttons[index] = true;
                        data_.clicked[index] = true;
                        if (index == 0)
                            data_.tip_down = true;
                    }
                }
                else if (index >= 0 && ev.xcookie.evtype == XI_ButtonRelease) {
                    data_.buttons[index] = false;
                    if (index == 0)
                        data_.tip_down = false;
                }
            }
        }

        XFreeEventData(display_, &ev.xcookie);
    }
}

void TabletInput::terminateX11()
{
    if (display_) {
        XCloseDisplay(display_);
        display_ = nullptr;
    }
    active_ = false;
}
