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

#ifdef APPLE

#include "TabletInput.h"
#include "Log.h"
#import <Cocoa/Cocoa.h>

TabletInput::TabletInput()
    : data_({0.0f, false, 0.0f, 0.0f, false, false})
    , active_(false)
    , monitor_(nullptr)
{
}

TabletInput::~TabletInput()
{
    terminate();
}

bool TabletInput::init()
{
    // Create event monitor for tablet events
    NSEventMask mask = NSEventMaskTabletPoint |
                       NSEventMaskTabletProximity |
                       NSEventMaskLeftMouseDown |
                       NSEventMaskLeftMouseUp |
                       NSEventMaskLeftMouseDragged;

    // Capture reference to data_ for the block
    TabletData *dataPtr = &data_;

    id monitor = [NSEvent addLocalMonitorForEventsMatchingMask:mask
        handler:^NSEvent *(NSEvent *event) {

            // Check for tablet point events
            if (event.type == NSEventTypeTabletPoint ||
                (event.subtype == NSEventSubtypeTabletPoint)) {

                dataPtr->has_pressure = true;
                dataPtr->pressure = event.pressure;
                dataPtr->tilt_x = event.tilt.x;
                dataPtr->tilt_y = event.tilt.y;
                dataPtr->tip_down = (event.pressure > 0.0f);
                dataPtr->in_proximity = true;
            }
            // Handle proximity events
            else if (event.type == NSEventTypeTabletProximity) {
                dataPtr->in_proximity = event.isEnteringProximity;
                if (!dataPtr->in_proximity) {
                    dataPtr->pressure = 0.0f;
                    dataPtr->tilt_x = 0.0f;
                    dataPtr->tilt_y = 0.0f;
                    dataPtr->tip_down = false;
                }
            }
            // Fallback to regular mouse events if tablet is in proximity
            else if (event.type == NSEventTypeLeftMouseDown) {
                if (dataPtr->in_proximity && dataPtr->pressure == 0.0f) {
                    // Set minimal pressure if tablet is near but not reporting pressure
                    dataPtr->pressure = 0.1f;
                    dataPtr->tip_down = true;
                }
            }
            else if (event.type == NSEventTypeLeftMouseUp) {
                if (dataPtr->in_proximity) {
                    dataPtr->tip_down = false;
                    if (!dataPtr->in_proximity) {
                        dataPtr->pressure = 0.0f;
                    }
                }
            }

            return event;
        }];

    monitor_ = (__bridge_retained void*)monitor;
    active_ = true;

    Log::Info("TabletInput: macOS tablet input initialized (NSEvent)");
    return true;
}

void TabletInput::pollEvents()
{
    // Events are handled automatically by the monitor callback
    // Nothing to do here for macOS
}

void TabletInput::terminate()
{
    if (monitor_) {
        id monitor = (__bridge_transfer id)monitor_;
        [NSEvent removeMonitor:monitor];
        monitor_ = nullptr;
    }
    data_.pressure = 0.0f;
    data_.tilt_x = 0.0f;
    data_.tilt_y = 0.0f;
    data_.in_proximity = false;
    data_.tip_down = false;
    active_ = false;
}

#endif // APPLE
