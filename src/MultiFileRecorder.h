#ifndef MULTIFILERECORDER_H
#define MULTIFILERECORDER_H

#include <list>
#include <string>
#include <atomic>
#include <vector>
#include <future>

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#include "Recorder.h"

class MultiFileRecorder
{

public:
    MultiFileRecorder();
    virtual ~MultiFileRecorder();

    void setFramerate (int fps);
    inline int framerate () const { return fps_; }

    void setProfile (VideoRecorder::Profile p);
    inline VideoRecorder::Profile profile () const { return profile_; }

    inline void setFiles (std::list<std::string> list) { files_ = list; }
    inline std::list<std::string> files () const { return files_; }

    // process control
    void start ();
    void cancel ();
    bool finished ();

    // result
    inline std::string filename () const { return filename_; }
    inline int width () const { return width_; }
    inline int height () const { return height_; }
    inline float progress () const { return progress_; }
    inline guint64 numFrames () const { return frame_count_; }

protected:
    // gstreamer functions
    static std::string assemble (MultiFileRecorder *rec);
    bool start_record (const std::string &video_filename);
    bool add_image    (const std::string &image_filename, GstCaps *caps);
    bool end_record();

    // gstreamer callbacks
    static void callback_need_data (GstAppSrc *, guint, gpointer user_data);
    static void callback_enough_data (GstAppSrc *, gpointer user_data);

private:

    // video properties
    std::string filename_;
    VideoRecorder::Profile profile_;
    int fps_;
    int width_;
    int height_;

    // encoder
    std::list<std::string> files_;
    GstElement   *pipeline_;
    GstAppSrc    *src_;
    guint64      frame_count_;
    GstClockTime timestamp_;
    GstClockTime frame_duration_;
    std::atomic<bool> cancel_;
    std::atomic<bool> endofstream_;
    std::atomic<bool> accept_buffer_;

    // progress and result
    float progress_;
    std::vector< std::future<std::string> >promises_;
};

#endif // MULTIFILERECORDER_H
