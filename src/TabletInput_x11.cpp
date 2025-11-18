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

#include <glib.h>
#ifdef LINUX

#include "TabletInput.h"
#include "Log.h"

#ifdef HAVE_X11TABLETINPUT
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <cstring>
#endif

TabletInput::TabletInput()
    : data_({0.0f, false, 0.0f, 0.0f, false, false})
    , active_(false)
#ifdef HAVE_X11TABLETINPUT
    , display_(nullptr)
    , xi_opcode_(-1)
    , pressure_valuator_(-1)
    , tilt_x_valuator_(-1)
    , tilt_y_valuator_(-1)
#endif
{
}

TabletInput::~TabletInput()
{
    terminate();
}

bool TabletInput::init()
{
#ifdef HAVE_X11TABLETINPUT
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

    // Select events for all devices
    XIEventMask eventmask;
    unsigned char mask[XIMaskLen(XI_LASTEVENT)] = {0};

    XISetMask(mask, XI_Motion);
    XISetMask(mask, XI_ButtonPress);
    XISetMask(mask, XI_ButtonRelease);

    eventmask.deviceid = XIAllDevices;
    eventmask.mask_len = sizeof(mask);
    eventmask.mask = mask;

    Window root = DefaultRootWindow(display_);
    XISelectEvents(display_, root, &eventmask, 1);

    // Find tablet devices and their pressure valuators
    int ndevices;
    XIDeviceInfo *devices = XIQueryDevice(display_, XIAllDevices, &ndevices);

    for (int i = 0; i < ndevices; i++) {
        XIDeviceInfo *device = &devices[i];

        // Look for devices that have valuators (tablets, styluses)
        if (device->use == XISlavePointer || device->use == XIFloatingSlave) {
            // Check valuator classes for pressure
            for (int j = 0; j < device->num_classes; j++) {
                if (device->classes[j]->type == XIValuatorClass) {
                    XIValuatorClassInfo *v = (XIValuatorClassInfo*)device->classes[j];

                    // Try to identify pressure axis
                    // Pressure is usually valuator 2, but we check the label
                    Atom pressure_atom = XInternAtom(display_, "Abs Pressure", True);
                    Atom tilt_x_atom = XInternAtom(display_, "Abs Tilt X", True);
                    Atom tilt_y_atom = XInternAtom(display_, "Abs Tilt Y", True);

                    if (v->label == pressure_atom) {
                        pressure_valuator_ = v->number;
                        Log::Info("TabletInput: Found pressure valuator %d on device '%s'",
                                 pressure_valuator_, device->name);
                    }
                    else if (v->label == tilt_x_atom) {
                        tilt_x_valuator_ = v->number;
                    }
                    else if (v->label == tilt_y_atom) {
                        tilt_y_valuator_ = v->number;
                    }
                    // Fallback: assume valuator 2 is pressure if we haven't found it
                    else if (pressure_valuator_ == -1 && v->number == 2) {
                        pressure_valuator_ = v->number;
                        Log::Info("TabletInput: Using valuator 2 as pressure (fallback)");
                    }
                }
            }
        }
    }

    XIFreeDeviceInfo(devices);

    if (pressure_valuator_ == -1) {
        Log::Info("TabletInput: No pressure valuator found - tablet may not be connected");
        // Don't fail init, just continue without pressure detection
    }
    else {
        data_.has_pressure = true;
    }

    XFlush(display_);
    active_ = true;
    Log::Info("TabletInput: X11/XInput2 tablet input initialized");
    return true;
#else
    Log::Info("TabletInput: XInput2 not available - tablet support disabled");
    return false;
#endif
}

void TabletInput::pollEvents()
{
#ifdef HAVE_X11TABLETINPUT
    if (!display_) return;

    // Process all pending X11 events
    while (XPending(display_) > 0) {
        XEvent ev;
        XNextEvent(display_, &ev);

        // Check for XInput2 events
        if (ev.type == GenericEvent && ev.xcookie.extension == xi_opcode_) {
            if (XGetEventData(display_, &ev.xcookie)) {
                XIDeviceEvent *device_event = (XIDeviceEvent*)ev.xcookie.data;

                switch (ev.xcookie.evtype) {
                    case XI_Motion:
                    case XI_ButtonPress:
                    case XI_ButtonRelease: {
                        // Extract pressure from valuators
                        if (pressure_valuator_ >= 0) {
                            double *values = device_event->valuators.values;
                            unsigned char *mask = device_event->valuators.mask;

                            int val_index = 0;
                            for (int i = 0; i <= pressure_valuator_; i++) {
                                if (XIMaskIsSet(mask, i)) {
                                    if (i == pressure_valuator_) {
                                        // Normalize pressure (typically 0-65535 range)
                                        data_.pressure = values[val_index] / 65535.0f;
                                        if (data_.pressure > 1.0f) data_.pressure = 1.0f;
                                        if (data_.pressure < 0.0f) data_.pressure = 0.0f;
                                    }
                                    val_index++;
                                }
                            }
                        }

                        // Extract tilt
                        if (tilt_x_valuator_ >= 0 || tilt_y_valuator_ >= 0) {
                            double *values = device_event->valuators.values;
                            unsigned char *mask = device_event->valuators.mask;

                            int val_index = 0;
                            int max_valuator = tilt_x_valuator_ > tilt_y_valuator_ ?
                                              tilt_x_valuator_ : tilt_y_valuator_;

                            for (int i = 0; i <= max_valuator; i++) {
                                if (XIMaskIsSet(mask, i)) {
                                    if (i == tilt_x_valuator_) {
                                        data_.tilt_x = (values[val_index] - 32767.5f) / 32767.5f;
                                    }
                                    if (i == tilt_y_valuator_) {
                                        data_.tilt_y = (values[val_index] - 32767.5f) / 32767.5f;
                                    }
                                    val_index++;
                                }
                            }
                        }

                        // Update button state
                        if (ev.xcookie.evtype == XI_ButtonPress) {
                            data_.tip_down = true;
                            data_.in_proximity = data_.pressure > 0.005f;
                        }
                        else if (ev.xcookie.evtype == XI_ButtonRelease) {
                            data_.tip_down = false;
                            data_.in_proximity = false;
                        }
                        else if (ev.xcookie.evtype == XI_Motion) {
                            data_.in_proximity = data_.pressure > 0.005f;
                        }
                        break;
                    }
                }

                XFreeEventData(display_, &ev.xcookie);
            }
        }
    }
#endif
}

void TabletInput::terminate()
{
#ifdef HAVE_X11TABLETINPUT
    if (display_) {
        XCloseDisplay(display_);
        display_ = nullptr;
    }
#endif
    active_ = false;
}

#endif // LINUX
