#include <thread>

// gstreamer
#include <gst/gstformat.h>
#include <gst/video/video.h>

#include <stb_image.h>

#include "Log.h"
#include "GstToolkit.h"
#include "BaseToolkit.h"
#include "Settings.h"
#include "MediaPlayer.h"

#include "MultiFileRecorder.h"

MultiFileRecorder::MultiFileRecorder() :
    fps_(0), width_(0), height_(0), bpp_(3),
    pipeline_(nullptr), src_(nullptr), frame_count_(0), timestamp_(0), frame_duration_(0),
    cancel_(false), endofstream_(false), accept_buffer_(false), progress_(0.f)
{
    // default profile
    profile_ = VideoRecorder::H264_STANDARD;

    // default fps and frame duration
    setFramerate(15);
}

MultiFileRecorder::~MultiFileRecorder ()
{
    if (src_ != nullptr)
        gst_object_unref (src_);

    if (pipeline_ != nullptr)
        gst_object_unref (pipeline_);
}

void MultiFileRecorder::setFramerate (int fps)
{
    fps_ = fps;
    frame_duration_ = gst_util_uint64_scale_int (1, GST_SECOND, fps_);
}

void MultiFileRecorder::setProfile (VideoRecorder::Profile p)
{
    if (p < VideoRecorder::VP8)
        profile_ = p;
    else
        profile_ = VideoRecorder::H264_STANDARD;
}

// appsrc needs data and we should start sending
void MultiFileRecorder::callback_need_data (GstAppSrc *, guint , gpointer p)
{
    MultiFileRecorder *grabber = static_cast<MultiFileRecorder *>(p);
    if (grabber)
        grabber->accept_buffer_ = true;
}

// appsrc has enough data and we can stop sending
void MultiFileRecorder::callback_enough_data (GstAppSrc *, gpointer p)
{
    MultiFileRecorder *grabber = static_cast<MultiFileRecorder *>(p);
    if (grabber)
        grabber->accept_buffer_ = false;
}

bool MultiFileRecorder::add_image (const std::string &image_filename)
{
    if (image_filename.empty())
        return false;

    // read pix
    int c = 0;
    int w = 0;
    int h = 0;
    unsigned char* rgb = stbi_load(image_filename.c_str(), &w, &h, &c, bpp_);

    if ( rgb && w == width_ && h == height_ && c == bpp_) {

        // new buffer
        guint size = width_ * height_ * bpp_;
        GstBuffer *buffer = gst_buffer_new_and_alloc (size);

        // map gst buffer into a memory  WRITE target
        GstMapInfo map;
        gst_buffer_map (buffer, &map, GST_MAP_WRITE);

        // transfer pixels from memory to buffer memory
        memmove(map.data, rgb, size);

        // un-map
        gst_buffer_unmap (buffer, &map);

        // free stbi memory
        stbi_image_free( rgb );

        //g_print("frame_added @ timestamp = %ld\n", timestamp_);
        GST_BUFFER_DTS(buffer) = GST_BUFFER_PTS(buffer) = timestamp_;

        // set frame duration
        buffer->duration = frame_duration_;

        // monotonic time increment to keep fixed FPS
        timestamp_ += frame_duration_;

        // push frame
        if ( gst_app_src_push_buffer (src_, buffer) != GST_FLOW_OK )
            return false;

    }
    else
        return false;


    return true;
}


bool MultiFileRecorder::start_record (const std::string &video_filename)
{
    if ( video_filename.empty() ) {
        Log::Warning("MultiFileRecorder Invalid file name");
        return false;
    }

    if (width_ == 0 || height_ == 0 ) {
        Log::Warning("MultiFileRecorder Invalid resolution");
        return false;
    }

    // create a gstreamer pipeline
    std::string description = "appsrc name=src ! queue ! videoconvert ! ";
    description += VideoRecorder::profile_description[ profile_ ];
    description += "qtmux ! filesink name=sink";

    // parse pipeline descriptor
    GError *error = NULL;
    pipeline_ = gst_parse_launch (description.c_str(), &error);
    if (error != NULL) {
        Log::Warning("MultiFileRecorder Could not construct pipeline %s:\n%s", description.c_str(), error->message);
        g_clear_error (&error);
        return false;
    }

    // setup file sink
    GstElement *sink = gst_bin_get_by_name (GST_BIN (pipeline_), "sink");
    if (!sink) {
        Log::Warning("MultiFileRecorder Could not configure sink");
        return false;
    }

    g_object_set (G_OBJECT (sink),
                  "location", video_filename.c_str(),
                  "sync", FALSE,
                  NULL);

    // setup custom app source
    src_ = GST_APP_SRC( gst_bin_get_by_name (GST_BIN (pipeline_), "src") );
    if (!src_) {
        Log::Warning("MultiFileRecorder : Failed to configure frame grabber.");
        return false;
    }

    g_object_set (G_OBJECT (src_),
                  "is-live", TRUE,
                  "format", GST_FORMAT_TIME,
                  "do-timestamp", FALSE,
                  NULL);

    // configure stream
    gst_app_src_set_emit_signals( src_, FALSE);
    gst_app_src_set_stream_type ( src_, GST_APP_STREAM_TYPE_STREAM);
    gst_app_src_set_latency     ( src_, -1, 0);
    gst_app_src_set_leaky_type  ( src_, GST_APP_LEAKY_TYPE_NONE);

    // Set buffer size
    gst_app_src_set_max_bytes( src_, MIN_BUFFER_SIZE);

    // specify recorder resolution and framerate in the source caps
    GstCaps *caps  = gst_caps_new_simple ("video/x-raw",
                                          "format", G_TYPE_STRING, bpp_ < 4 ? "RGB" : "RGBA",
                                          "width",  G_TYPE_INT, width_,
                                          "height", G_TYPE_INT, height_,
                                          "framerate", GST_TYPE_FRACTION, fps_, 1,
                                          NULL);
    gst_app_src_set_caps (src_, caps );
    gst_caps_unref (caps);

    GstAppSrcCallbacks callbacks;
    callbacks.need_data = MultiFileRecorder::callback_need_data;
    callbacks.enough_data = MultiFileRecorder::callback_enough_data;
    callbacks.seek_data = NULL; // stream type is not seekable
    gst_app_src_set_callbacks (src_, &callbacks, this, NULL);

    // start recording
    GstStateChangeReturn ret = gst_element_set_state (pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        Log::Warning("MultiFileRecorder: Failed to start frame grabber.");
        return false;
    }

    // wait ready
    int max = 100;
    accept_buffer_ = false;
    while (!accept_buffer_ && --max > 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));


//    // send request key frame upstream
//    GstEvent* event = gst_video_event_new_upstream_force_key_unit (GST_CLOCK_TIME_NONE, TRUE, 1);
//    if (!gst_element_send_event( GST_ELEMENT(sink), event) )
//        Log::Warning("MultiFileRecorder: Failed to request key unit.");

//    GstPad *padsrc = gst_element_get_static_pad ( GST_ELEMENT (sink), "sink");
//    gst_pad_push_event(padsrc, gst_event_new_custom(GST_EVENT_CUSTOM_UPSTREAM, gst_structure_new("GstForceKeyUnit", "all-headers",
//                                                                                                 G_TYPE_BOOLEAN, TRUE, NULL)));
//    gst_object_unref (padsrc);

//    // send request key frame downstream
//    GstPad *padsrc = gst_element_get_static_pad ( GST_ELEMENT (src_), "src");
//    GstStructure *s = gst_structure_new("GstForceKeyUnit",
//                                        "timestamp", G_TYPE_UINT64, 0,
//                                        "stream-time", G_TYPE_UINT64, 0,
//                                        "running-time", G_TYPE_UINT64, 0,
//                                        "all-headers", G_TYPE_BOOLEAN, TRUE,
//                                        NULL);
//    if ( !gst_pad_push_event(padsrc, gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, s )) )
//        Log::Warning("MultiFileRecorder: Failed to force key unit.");
//    gst_object_unref (padsrc);

    GstEvent* event = gst_video_event_new_downstream_force_key_unit (GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE, TRUE, 1);
    if (!gst_element_send_event( GST_ELEMENT(src_), event) )
        Log::Warning("MultiFileRecorder: Failed to force key unit.");


    return true;
}

bool MultiFileRecorder::end_record ()
{
    bool ret = true;
    GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline_));

    // ensure duration is set to correct
//    gst_app_src_set_duration (src_, frame_count_ * frame_duration_ );
    gst_app_src_set_duration (src_, timestamp_);

    // send end of stream
    gst_app_src_end_of_stream (src_);

    // wait to receive end of stream
    GstMessage *msg = gst_bus_timed_pop_filtered(bus, GST_SECOND, GST_MESSAGE_EOS);
    if ( msg == NULL ) {
        Log::Warning("MultiFileRecorder: Failed to close recording.");
        ret = false;
    }

    // stop the pipeline
    GstStateChangeReturn r = gst_element_set_state (pipeline_, GST_STATE_NULL);
    if (r == GST_STATE_CHANGE_ASYNC) {
        gst_element_get_state (pipeline_, NULL, NULL, GST_SECOND);
        g_print("... ASYNC END !\n");
    }
    else if (r == GST_STATE_CHANGE_FAILURE) {
        Log::Warning("MultiFileRecorder: Failed to end recording.");
        ret = false;
    }

    // clean
    gst_object_unref (src_);
    src_ = nullptr;
    gst_object_unref (pipeline_);
    pipeline_ = nullptr;

    return ret;
}



void MultiFileRecorder::start ()
{
    if ( promises_.empty() ) {
        filename_ = std::string();
        promises_.emplace_back( std::async(std::launch::async, assemble, this) );
    }
}

void MultiFileRecorder::cancel ()
{
    cancel_ = true;
}

bool MultiFileRecorder::finished ()
{
    if ( !promises_.empty() ) {
        // check that file dialog thread finished
        if (promises_.back().wait_for(std::chrono::milliseconds(4)) == std::future_status::ready ) {
            // get the filename from encoder
            filename_ = promises_.back().get();
            if (!filename_.empty()) {
                // save path location
                Settings::application.recentRecordings.push(filename_);
            }
            // done with this recoding
            promises_.pop_back();
            return true;
        }
    }
    return false;
}

std::string  MultiFileRecorder::assemble (MultiFileRecorder *rec)
{
    std::string filename;

    // reset
    rec->progress_ = 0.f;
    rec->width_ = 0;
    rec->height_ = 0;
    rec->bpp_ = 0;

    // input files
    if ( rec->files_.size() < 1 ) {
        Log::Warning("MultiFileRecorder: No image given.");
        return filename;
    }

    // set recorder resolution from first image
    stbi_info( rec->files_.front().c_str(), &rec->width_, &rec->height_, &rec->bpp_);

    if ( rec->width_ < 10 || rec->height_ < 10 || rec->bpp_ < 3 ) {
        Log::Warning("MultiFileRecorder: invalid image.");
        return filename;
    }

    Log::Info("MultiFileRecorder creating video %d x %d : %d.", rec->width_, rec->height_, rec->bpp_);

    // progress increment
    float inc_ = 1.f / ( (float) rec->files_.size() + 2.f);

    // initialize
    rec->frame_count_ = 0;
    filename = BaseToolkit::common_prefix (rec->files_);
    filename +=  "_sequence.mov";

    if ( rec->start_record( filename ) )
    {
        // progressing
        rec->progress_ += inc_;

        // loop over images to open
        for (auto file = rec->files_.cbegin(); file != rec->files_.cend(); ++file) {

            if ( rec->cancel_ )
                break;

            if ( rec->add_image( *file ) ) {

                // validate file
                rec->frame_count_++;
            }
            else {
                Log::Info("MultiFileRecorder could not include images %s.", file->c_str());
            }

            // pause in case appsrc buffer is full
            int max = 100;
            while (!rec->accept_buffer_ && --max > 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));

            // progressing
            rec->progress_ += inc_;
        }

        // close file properly
        if ( rec->end_record() ) {
            Log::Info("MultiFileRecorder %d images encoded (%ld s), saved in %s.", rec->frame_count_, rec->timestamp_, filename.c_str());
        }
    }
    else
        filename = std::string();

    // finished
    rec->progress_ = 1.f;

    return filename;
}

// alternative https://gstreamer.freedesktop.org/documentation/application-development/advanced/pipeline-manipulation.html?gi-language=c#changing-elements-in-a-pipeline

//    // initialize recorder with first image
//    std::string uri = GstToolkit::filename_to_uri( list.front() );
//    MediaInfo media = MediaPlayer::UriDiscoverer( uri );

//    // if media is valid image
//    if (!media.valid || !media.isimage || media.width < 1 || media.height < 1) {
//        Log::Warning("MultiFileRecorder: Invalid image.");
//        return;
//    }

//    // set recorder resolution from first image
//    width_ = media.width;
//    height_ = media.height;

//GstFlowReturn MultiFileRecorder::callback_new_preroll (GstAppSink *sink, gpointer p)
//{
//    GstFlowReturn ret = GST_FLOW_OK;

//    // blocking read pre-roll samples
//    GstSample *sample = gst_app_sink_pull_preroll(sink);

//    // if got a valid sample
//    if (sample != NULL) {

//        MultiFileRecorder *grabber = static_cast<MultiFileRecorder *>(p);
//        if (grabber) {

//            GstBuffer *buf = gst_sample_get_buffer (sample);

//            // store frame to recorder
//            grabber->addFrame(buf);
//        }
//    }
//    else
//        ret = GST_FLOW_FLUSHING;

//    // release sample
//    gst_sample_unref (sample);

//    return ret;
//}

//bool MultiFileRecorder::get_frame_from_image (const std::string &file)
//{
//    std::string description = "uridecodebin name=decoder ! queue ! videoconvert ! appsink name=sink";

//    // parse pipeline descriptor
//    GError *error = NULL;
//    GstElement *pipeline = gst_parse_launch (description.c_str(), &error);
//    if (error != NULL) {
//        Log::Warning("MultiFileRecorder Could not construct pipeline %s:\n%s", description.c_str(), error->message);
//        g_clear_error (&error);
//        return false;
//    }

//    // setup decoder
//    GstElement *dec = gst_bin_get_by_name (GST_BIN (pipeline), "decoder");
//    if (!dec) {
//        Log::Warning("MultiFileRecorder Could not configure decoder");
//        return false;
//    }
//    g_object_set (dec, "uri", GstToolkit::filename_to_uri( file ).c_str(), NULL);

//    // setup appsink
//    GstElement *sink = gst_bin_get_by_name (GST_BIN (pipeline), "sink");
//    if (!sink) {
//        Log::Warning("MultiFileRecorder Could not configure sink");
//        return false;
//    }

//    // instruct the sink to send samples synched in time
//    gst_base_sink_set_sync (GST_BASE_SINK(sink), true);

//    // instruct sink to use the required caps
//    GstCaps *caps  = gst_caps_new_simple ("video/x-raw",
//                                 "format", G_TYPE_STRING, "RGB",
//                                 "width",  G_TYPE_INT, width_,
//                                 "height", G_TYPE_INT, height_,
//                                 NULL);
//    gst_app_sink_set_caps (GST_APP_SINK(sink), caps);

//    // set the callback to receive image preroll
//    GstAppSinkCallbacks callbacks;
//    callbacks.eos = NULL;
//    callbacks.new_sample = NULL;
//#if GST_VERSION_MINOR > 18 && GST_VERSION_MAJOR > 0
//    callbacks.new_event = NULL;
//#endif
//    callbacks.new_preroll = MultiFileRecorder::callback_new_preroll;
//    gst_app_sink_set_callbacks (GST_APP_SINK(sink), &callbacks, this, NULL);
//    gst_app_sink_set_emit_signals (GST_APP_SINK(sink), false);

//    // done with refs
//    gst_object_unref (sink);
//    gst_object_unref (dec);
//    gst_caps_unref   (caps);

//    // set to PAUSE to trigger preroll
//    GstStateChangeReturn ret = gst_element_set_state (pipeline, GST_STATE_PAUSED);
//    if (ret == GST_STATE_CHANGE_FAILURE) {
//        Log::Warning("MultiFileRecorder Could not open '%s'", file.c_str());
//        return false;
//    }

//    // wait to accept buffer
//    int max = 100;
//    frame_added_ = false;
//    while (!frame_added_ && --max > 0)
//        std::this_thread::sleep_for(std::chrono::milliseconds(10));

//    GstState state = GST_STATE_NULL;
//    gst_element_set_state (pipeline_, state);
//    gst_element_get_state (pipeline_, &state, NULL, GST_CLOCK_TIME_NONE);
//    g_print("pipeline stoped\n");

//    // end pipeline
//    gst_object_unref (pipeline);

//    return true;
//}
