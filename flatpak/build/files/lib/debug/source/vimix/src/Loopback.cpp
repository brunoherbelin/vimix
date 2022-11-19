/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2022 Bruno Herbelin <bruno.herbelin@gmail.com>
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

//  Desktop OpenGL function loader
#include <glad/glad.h>

// gstreamer
#include <gst/gstformat.h>
#include <gst/video/video.h>

#include "defines.h"
#include "Settings.h"
#include "GstToolkit.h"
#include "SystemToolkit.h"
#include "FrameBuffer.h"
#include "Log.h"

#include "Loopback.h"

bool Loopback::system_loopback_initialized = false;

#if defined(LINUX)

/**
 *
 * Linux video 4 linux loopback device
 *
 * 1) Linux system has to have the v4l2loopback package
 * See documentation at https://github.com/umlaeute/v4l2loopback
 *
 * $ sudo -A apt install v4l2loopback-dkms
 *
 * 2) User (sudo) has to install a v4l2loopback
 *
 * $ sudo -A modprobe v4l2loopback exclusive_caps=1 video_nr=10
 *
 * 3) But to do that, the user has to enter sudo passwd
 *
 * The command line above should be preceeded by
 *   export SUDO_ASKPASS="/tmp/mysudo.sh"
 *
 * where mysudo.sh contains the following:
 *   #!/bin/bash
 *   zenity --password --title=Authentication
 *
 * 4) Optionaly, we can set the dynamic properties of the stream
 *
 * $ sudo v4l2loopback-ctl set-caps "RGBA:640x480" /dev/video10
 * $ sudo v4l2loopback-ctl set-fps 30 /dev/video10
 *
 * 5) Finally, the gstreamer pipeline can write into v4l2sink
 *
 * gst-launch-1.0 videotestsrc ! v4l2sink device=/dev/video10
 *
 *
 * Useful command lines for debug
 * $ v4l2-ctl --all -d 10
 * $ gst-launch-1.0 v4l2src device=/dev/video10 ! videoconvert ! autovideosink
 * $ gst-launch-1.0 videotestsrc ! v4l2sink device=/dev/video10
 */

#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

std::string Loopback::system_loopback_name = "/dev/video10";
std::string Loopback::system_loopback_pipeline = "appsrc name=src ! videoconvert ! videorate ! video/x-raw,framerate=30/1 ! v4l2sink sync=false name=sink";

bool Loopback::initializeSystemLoopback()
{
    if (!Loopback::systemLoopbackInitialized()) {

        // create script for asking sudo password
        std::string sudoscript = SystemToolkit::full_filename(SystemToolkit::settings_path(), "sudo.sh");
        FILE *file = fopen(sudoscript.c_str(), "w");
        if (file) {
            fprintf(file, "#!/bin/bash\n");
            fprintf(file, "zenity --password --title=Authentication\n");
            fclose(file);

            // make script executable
            int fildes = 0;
            fildes = open(sudoscript.c_str(), O_RDWR);
            fchmod(fildes, S_IRWXU | S_IRWXG | S_IROTH | S_IWOTH);
            close(fildes);

            // create command line for installing v4l2loopback
            std::string cmdline = "export SUDO_ASKPASS=\"" + sudoscript + "\"\n";
            cmdline += "sudo -A apt install v4l2loopback-dkms 2>&1\n";
            cmdline += "sudo -A modprobe -r v4l2loopback 2>&1\n";
            cmdline += "sudo -A modprobe v4l2loopback exclusive_caps=1 video_nr=10 card_label=\"vimix loopback\" 2>&1\n";

            // execute v4l2 command line
            std::string report;
            FILE *fp = popen(cmdline.c_str(), "r");
            if (fp != NULL) {

                // get stdout content from command line
                char linestdout[PATH_MAX];
                while (fgets(linestdout, PATH_MAX, fp) != NULL)
                    report += linestdout;

                // error reported by pclose?
                if (pclose(fp) != 0 )
                    Log::Warning("Failed to initialize system v4l2loopback\n%s", report.c_str());
                // okay, probaly all good...
                else
                    system_loopback_initialized = true;
            }
            else
                Log::Warning("Failed to initialize system v4l2loopback\nCannot execute command line");

        }
        else
            Log::Warning("Failed to initialize system v4l2loopback\nCannot create script", sudoscript.c_str());
    }

    return system_loopback_initialized;
}

bool Loopback::systemLoopbackInitialized()
{
    // test if already initialized
    if (!system_loopback_initialized) {
        // check the existence of loopback device
        if ( SystemToolkit::file_exists(system_loopback_name) )
            system_loopback_initialized = true;
    }

    return system_loopback_initialized;
}

#else

std::string Loopback::system_loopback_name = "undefined";
std::string Loopback::system_loopback_pipeline = "";


bool Loopback::initializeSystemLoopback()
{
    system_loopback_initialized = false;
    return false;
}

bool Loopback::systemLoopbackInitialized()
{
    return false;
}
#endif


Loopback::Loopback() : FrameGrabber()
{
    frame_duration_ = gst_util_uint64_scale_int (1, GST_SECOND, 30);  // fixed 30 FPS
}

std::string Loopback::init(GstCaps *caps)
{
    // ignore
    if (caps == nullptr)
        return std::string("Invalid caps");

    if (!Loopback::systemLoopbackInitialized()){
        return std::string("Loopback system shall be initialized first.");
    }

    // create a gstreamer pipeline
    std::string description = Loopback::system_loopback_pipeline;

    // parse pipeline descriptor
    GError *error = NULL;
    pipeline_ = gst_parse_launch (description.c_str(), &error);
    if (error != NULL) {
        std::string msg = std::string("Loopback : Could not construct pipeline ") + description + "\n" + std::string(error->message);
        g_clear_error (&error);
        return msg;
    }

    // setup device sink
    g_object_set (G_OBJECT (gst_bin_get_by_name (GST_BIN (pipeline_), "sink")),
                  "device", Loopback::system_loopback_name.c_str(),
                  NULL);

    // setup custom app source
    src_ = GST_APP_SRC( gst_bin_get_by_name (GST_BIN (pipeline_), "src") );
    if (src_) {

        g_object_set (G_OBJECT (src_),
                      "is-live", TRUE,
                      NULL);

        // configure stream
        gst_app_src_set_stream_type( src_, GST_APP_STREAM_TYPE_STREAM);
        gst_app_src_set_latency( src_, -1, 0);

        // Set buffer size
        gst_app_src_set_max_bytes( src_, buffering_size_ );

        // specify streaming framerate in the given caps
        GstCaps *tmp = gst_caps_copy( caps );
        GValue v = { 0, };
        g_value_init (&v, GST_TYPE_FRACTION);
        gst_value_set_fraction (&v, 30, 1); // fixed 30 FPS
        gst_caps_set_value(tmp, "framerate", &v);
        g_value_unset (&v);

        // instruct src to use the caps
        caps_ = gst_caps_copy( tmp );
        gst_app_src_set_caps (src_, caps_);
        gst_caps_unref (tmp);

        // setup callbacks
        GstAppSrcCallbacks callbacks;
        callbacks.need_data = FrameGrabber::callback_need_data;
        callbacks.enough_data = FrameGrabber::callback_enough_data;
        callbacks.seek_data = NULL; // stream type is not seekable
        gst_app_src_set_callbacks( src_, &callbacks, this, NULL);

    }
    else {
        return std::string("Loopback : Could not configure source.");
    }

    // start recording
    GstStateChangeReturn ret = gst_element_set_state (pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        return std::string("Loopback : Could not open ") + Loopback::system_loopback_name;
    }

    // all good
    initialized_ = true;

    return std::string("Loopback started on ") + Loopback::system_loopback_name;
}

void Loopback::terminate()
{
    Log::Notify("Loopback to %s terminated.", Loopback::system_loopback_name.c_str());
}
