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
#include <string.h>

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

    ///
    /// Parse arguments
    ///
    int versionRequested = 0;
    int testRequested = 0;
    int cleanRequested = 0;
    int headlessRequested = 0;
    int helpRequested = 0;
    int fontsizeRequested = 0;
    std::string settingsRequested;
    int ret = -1;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-V") == 0) {
            versionRequested = 1;
        } else if (strcmp(argv[i], "--test") == 0 || strcmp(argv[i], "-T") == 0) {
            testRequested = 1;
        } else if (strcmp(argv[i], "--clean") == 0 || strcmp(argv[i], "-C") == 0) {
            cleanRequested = 1;
        } else if (strcmp(argv[i], "--headless") == 0 || strcmp(argv[i], "-L") == 0) {
            headlessRequested = 1;
        } else if (strcmp(argv[i], "--settings") == 0 || strcmp(argv[i], "-S") == 0) {
            // get settings file argument
            if (i + 1 < argc) {
                settingsRequested = argv[i + 1]; // Parse next argument as integer
                i++; // Skip the next argument since it's already processed
            } else {
                fprintf(stderr, "Error: filename missing after --settings\n");
                helpRequested = 1;
            }
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-H") == 0) {
            helpRequested = 1;
        } else if (strcmp(argv[i], "--fontsize") == 0 || strcmp(argv[i], "-F") == 0) {
            // get font size argument
            if (i + 1 < argc) {
                fontsizeRequested = atoi(argv[i + 1]); // Parse next argument as integer
                i++; // Skip the next argument since it's already processed
            } else {
                fprintf(stderr, "Error: Integer value missing after --fontsize\n");
                helpRequested = 1;
            }
        } else if ( strchr(argv[i], '-')-argv[i] == 0 ) {
            fprintf(stderr, "Error: Invalid argument\n");
            helpRequested = 1;
        } else {
            _openfile = argv[i];
        }
    }

    if (versionRequested) {
#ifdef VIMIX_GIT
        printf(APP_NAME " " VIMIX_GIT " \n");
#else
#ifdef VIMIX_VERSION_MAJOR
        printf("%s: version %d.%d.%d\n",
               APP_NAME,
               VIMIX_VERSION_MAJOR,
               VIMIX_VERSION_MINOR,
               VIMIX_VERSION_PATCH);
#else
        printf("%s\n", APP_NAME);
#endif
#endif
        ret = 0;
    }

    if (testRequested) {
        if (!Rendering::manager().init()) {
            fprintf(stderr, "%s: test Failed\n", argv[0]);
            ret = 1;
        }
        else {
            fprintf(stderr, "%s: test OK\n", argv[0]);
            ret = 0;
        }
    }

    if (cleanRequested) {
        // clean settings : save settings before loading
        Settings::Save();
        printf("%s: clean OK\n", argv[0]);
        ret = 0;
    }

    if (helpRequested) {
        printf("Usage: %s [-H, --help] [-V, --version] [-F, --fontsize] [-L, --headless]\n"
               "               [-S, --settings] [-T, --test] [-C, --clean] [filename]\n",
               argv[0]);
        printf("Options:\n");
        printf("  --help       : Display usage information\n");
        printf("  --version    : Display version information\n");
        printf("  --fontsize   : Force rendering font size to specified value, e.g., '-F 25'\n");
        printf("  --settings   : Run with given settings file, e.g., '-S settingsfile.xml'\n");
        printf("  --headless   : Run without GUI (only if output windows configured)\n");
        printf("  --test       : Run rendering test and return\n");
        printf("  --clean      : Reset user settings\n");
        printf("Filename:\n");
        printf("  vimix session file (.mix extension)\n");
        ret = 0;
    }

    if (ret >= 0)
        return ret;

    if (!_openfile.empty())
        printf("Openning '%s' ...\n", _openfile.c_str());

    ///
    /// Settings
    ///
    Settings::Load( settingsRequested );
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
    if ( !UserInterface::manager().Init( fontsizeRequested ) )
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
    Rendering::manager().show(!headlessRequested);

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
    Mixer::manager().clear();

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
