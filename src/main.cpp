/*
 * vimix - video live mixer
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

#include <stdio.h>

//  GStreamer
#include <gst/gst.h>

// vmix
#include "Settings.h"
#include "Mixer.h"
#include "RenderingManager.h"
#include "UserInterfaceManager.h"
#include "ControlManager.h"
#include "Connection.h"
#include "Metronome.h"
#include "Audio.h"

#if defined(APPLE)
extern "C"{
    void forward_load_message(const char * filename){
        Mixer::manager().load(filename);
    }
}
#endif


void prepare()
{
    Control::manager().update();
    Mixer::manager().update();
    UserInterface::manager().NewFrame();
}

void drawScene()
{
    Mixer::manager().draw();
}

void renderGUI()
{
    UserInterface::manager().Render();
}

int main(int argc, char *argv[])
{
    std::string _openfile;

    // one extra argument is given
    if (argc == 2) {
        std::string argument(argv[1]);
        if (argument[0] == '-') {
            if (argument == "--clean" || argument == "-C") {
                // clean start if requested : Save empty settings before loading
                Settings::Save();
                fprintf(stderr, "%s: clean OK\n", APP_NAME);
                return 0;
            }
            else if (argument == "--version" || argument == "-V") {
#ifdef VIMIX_GIT
                fprintf(stderr, APP_NAME " " VIMIX_GIT " \n");
#else
#ifdef VIMIX_VERSION_MAJOR
                fprintf(stderr, "%s: version %d.%d.%d\n", APP_NAME, VIMIX_VERSION_MAJOR, VIMIX_VERSION_MINOR, VIMIX_VERSION_PATCH);
#else
                fprintf(stderr, "%s\n", APP_NAME);
#endif
#endif
                return 0;
            }
            else if (argument == "--test" || argument == "-T") {
                if ( !Rendering::manager().init() ) {
                    fprintf(stderr, "%s: test Failed\n", APP_NAME);
                    return 1;
                }
                fprintf(stderr, "%s: test OK\n", APP_NAME);
                return 0;
            }
            else {
                fprintf(stderr, "%s: unrecognized option '%s'\n"
                        "Usage: %s [-V, --version][-T, --test][-C, --clean][FILE]\n",
                        APP_NAME, argument.c_str(), APP_NAME);
                return 1;
            }
        }
        else {
            // try to open the file
            _openfile = argument;
            if (!_openfile.empty())
                fprintf(stderr, "Loading '%s' ...\n", _openfile.c_str());
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
    /// METRONOME INIT (Ableton Link)
    ///
    if ( !Metronome::manager().init() )
        return 1;

    ///
    /// RENDERING & GST INIT
    ///
    if ( !Rendering::manager().init() )
        return 1;

    ///
    /// CONTROLLER INIT (OSC)
    ///
    Control::manager().init();

    ///
    /// IMGUI INIT
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

    ///
    /// AUDIO INIT
    ///
    if ( Settings::application.accept_audio )
        Audio::manager().initialize();

    // callbacks to draw
    Rendering::manager().pushBackDrawCallback(prepare);
    Rendering::manager().pushBackDrawCallback(drawScene);
    Rendering::manager().pushBackDrawCallback(renderGUI);

    // show all windows
    Rendering::manager().draw();
    Rendering::manager().show();

    // try to load file given in argument
    Mixer::manager().load(_openfile);

    ///
    /// Main LOOP
    ///
    while ( Rendering::manager().isActive() )
        Rendering::manager().draw();

    ///
    /// UI TERMINATE
    ///
    UserInterface::manager().Terminate();

    ///
    /// MIXER TERMINATE
    ///
    while (Mixer::manager().busy())
        Mixer::manager().update();

    ///
    /// RENDERING TERMINATE
    ///
    Rendering::manager().terminate();

    ///
    /// METRONOME TERMINATE
    ///
    Metronome::manager().terminate();

    ///
    /// CONTROLLER TERMINATE
    ///
    Control::manager().terminate();

    ///
    /// CONNECTION TERMINATE
    ///
    Connection::manager().terminate();

    /// unlock on clean exit
    Settings::Unlock();

    ///
    /// Settings
    ///
    Settings::Save(UserInterface::manager().Runtime());

    /// ok
    return 0;
}
