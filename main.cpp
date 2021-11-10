/*
 * vimix - video live mixer
 *
 * **Copyright** (C) 2020-2021 Bruno Herbelin <bruno.herbelin@gmail.com>
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

#include <stdio.h>

//  GStreamer
#include <gst/gst.h>

// vmix
#include "Settings.h"
#include "Mixer.h"
#include "RenderingManager.h"
#include "UserInterfaceManager.h"
#include "Connection.h"


#if defined(APPLE)
extern "C"{
    void forward_load_message(const char * filename){
        Mixer::manager().load(filename);
    }
}
#endif


void drawScene()
{
    Mixer::manager().draw();
}

int main(int argc, char *argv[])
{
    // one extra argument is given
    if (argc == 2) {
        std::string argument(argv[1]);
        if (argument == "--clean" || argument == "-c") {
            // clean start if requested : Save empty settings before loading
            Settings::Save();
            return 0;
        }
        else if (argument == "--version" || argument == "-v") {
#ifdef VIMIX_VERSION_MAJOR
            fprintf(stderr, "%s %d.%d.%d\n", APP_NAME, VIMIX_VERSION_MAJOR, VIMIX_VERSION_MINOR, VIMIX_VERSION_PATCH);
#else
            fprintf(stderr, "%s\n", APP_NAME);
#endif
            return 0;
        }
        else if (argument == "--test" || argument == "-t") {
            if ( !Rendering::manager().init() ) {
                fprintf(stderr, "%s Failed\n", APP_NAME);
                return 1;
            }
            fprintf(stderr, "%s OK\n", APP_NAME);
            return 0;
        }
        else {
            // try to open the file
            Mixer::manager().load(argument);
        }
    }

    ///
    /// Settings
    ///
    Settings::Load();
    Settings::application.executable = std::string(argv[0]);

    /// lock to inform an instance is running
    Settings::Lock();

    ///
    /// CONNECTION INIT
    ///
    if ( !Connection::manager().init() )
        return 1;

    ///
    /// RENDERING INIT
    ///
    if ( !Rendering::manager().init() )
        return 1;

    ///
    /// UI INIT
    ///
    if ( !UserInterface::manager().Init() )
        return 1;

    ///
    /// GStreamer
    ///
#ifndef NDEBUG
    gst_debug_set_default_threshold (GST_LEVEL_WARNING);
    gst_debug_set_active(TRUE);
#else
    gst_debug_set_default_threshold (GST_LEVEL_ERROR);
    gst_debug_set_active(FALSE);
#endif

    // draw the scene
    Rendering::manager().pushFrontDrawCallback(drawScene);

    // show all windows
    Rendering::manager().show();

    ///
    /// Main LOOP
    ///
    while ( Rendering::manager().isActive() )
    {
        Mixer::manager().update();

        Rendering::manager().draw();
    }

    ///
    /// UI TERMINATE
    ///
    UserInterface::manager().Terminate();

    ///
    /// RENDERING TERMINATE
    ///
    Rendering::manager().terminate();

    ///
    /// CONNECTION TERMINATE
    ///
    Connection::manager().terminate();

    /// unlock on clean exit
    Settings::Unlock();

    ///
    /// Settings
    ///
    Settings::Save();

    /// ok
    return 0;
}
