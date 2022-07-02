#ifndef MULTIFILERECORDER_H
#define MULTIFILERECORDER_H

#include <list>
#include <string>
#include <atomic>

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#include "Recorder.h"

class MultiFileRecorder
{
    std::string filename_;
    VideoRecorder::Profile profile_;
    int fps_;
    int width_;
    int height_;
    int bpp_;

    GstElement   *pipeline_;
    GstAppSrc    *src_;
    guint64      frame_count_;
    GstClockTime timestamp_;
    GstClockTime frame_duration_;
    std::atomic<bool> endofstream_;
    std::atomic<bool> accept_buffer_;

    float progress_;
    std::list<std::string> files_;

public:
    MultiFileRecorder(const std::string &filename);
    virtual ~MultiFileRecorder();

    void setFramerate (int fps);
    inline int framerate () const { return fps_; }

    void setProfile (VideoRecorder::Profile profil);
    inline VideoRecorder::Profile profile () const { return profile_; }

    void assemble (std::list<std::string> list);

    inline float progress () const { return progress_; }
    inline std::list<std::string> files () const { return files_; }
    inline int width () const { return width_; }
    inline int height () const { return height_; }
    inline guint64 numFrames () const { return frame_count_; }

    //                std::thread(MultiFileRecorder::assembleImages, sourceSequenceFiles, "/home/bhbn/test.mov").detach();
    static void assembleImages(std::list<std::string> list, const std::string &filename);

protected:
    // gstreamer functions
    bool start_record (const std::string &video_filename);
    bool add_image    (const std::string &image_filename);
    bool end_record();

    // gstreamer callbacks
    static void callback_need_data (GstAppSrc *, guint, gpointer user_data);
    static void callback_enough_data (GstAppSrc *, gpointer user_data);
};

#endif // MULTIFILERECORDER_H
