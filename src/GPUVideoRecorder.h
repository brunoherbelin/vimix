#ifndef GPUVIDEORECORDER_H
#define GPUVIDEORECORDER_H

#ifdef USE_GST_OPENGL_SYNC_HANDLER

#include <atomic>
#include <string>
#include <gst/gst.h>
#include <gst/gl/gl.h>
#include <gst/app/gstappsrc.h>

/**
 * @brief The GPUVideoRecorder class provides a full-GPU video recording pipeline
 * using GLMemory throughout, eliminating CPU-GPU copies.
 *
 * Unlike VideoRecorder (which uses readPixels/PBO to transfer frames to system memory),
 * GPUVideoRecorder keeps frames in GPU memory from capture through encoding.
 *
 * This class is designed specifically for hardware encoders (nvenc, vaapi)
 * and handles GL context threading constraints properly by:
 * - Performing GL operations in GStreamer's GL thread via callbacks
 * - Using FBO blits for texture transfer within the GPU
 * - Properly sharing GL context between vimix and GStreamer
 *
 * REQUIREMENTS:
 * - USE_GST_OPENGL_SYNC_HANDLER must be defined
 * - OpenGL context sharing properly initialized in RenderingManager
 * - Hardware encoder available (nvenc or vaapi)
 *
 * SPECIFICITIES:
 * - Does NOT inherit from FrameGrabber (different threading model)
 * - All GL operations execute in GStreamer's thread
 * - Uses appsrc with GLMemory caps directly
 */
class GPUVideoRecorder
{
public:

    // Hardware encoder profiles
    typedef enum {
        NVENC_H264_REALTIME = 0,
        NVENC_H264_HQ,
        NVENC_H265_REALTIME,
        NVENC_H265_HQ,
        VAAPI_H264_REALTIME,
        VAAPI_H264_HQ,
        VAAPI_H265_REALTIME,
        VAAPI_H265_HQ,
        PROFILE_COUNT
    } Profile;

    static const char* profile_name[PROFILE_COUNT];
    static const char* profile_encoder[PROFILE_COUNT];
    static const int   framerate_preset[3];

    GPUVideoRecorder(const std::string &basename = std::string());
    virtual ~GPUVideoRecorder();


    /**
     * Add a frame from vimix's OpenGL texture
     * This is called from vimix's render thread.
     * The actual GL work happens in GStreamer's thread via callback.
     *
     * @param texture_id OpenGL texture ID from vimix's FrameBuffer
     */
    void addFrame(guint texture_id, GstCaps *caps);

    /**
     * Stop recording and finalize file
     */
    void stop();

    /**
     * Check if recording is initialized
     */
    bool initialized() const { return initialized_; }

    /**
     * Check if recording is active
     */
    bool active() const { return active_; }

    /**
     * Check if recording has finished
     */
    bool finished() const { return finished_; }

    /**
     * Get current recording duration in milliseconds
     */
    uint64_t duration() const;

    /**
     * Get total frame count
     */
    guint64 frames() const { return frame_count_; }

    /**
     * Get output filename
     */
    std::string filename() const { return filename_; }

    /**
     * Get status information
     */
    std::string info() const;

protected:

    /**
     * Initialize the GPU recording pipeline
     * @param width Frame width
     * @param height Frame height
     * @param fps Target framerate
     * @return True on success, false on failure
     */
    bool init(GstCaps *caps);

private:
    // GStreamer pipeline elements
    GstElement *pipeline_;
    GstAppSrc *src_;
    GstCaps *caps_;
    GstGLContext *gl_context_;
    GstGLDisplay *gl_display_;

    // Timing
    GstClock *timer_;
    GstClockTime timer_start_;
    GstClockTime timestamp_;
    GstClockTime frame_duration_;
    guint64 frame_count_;

    // Recording state
    std::atomic<bool> initialized_;
    std::atomic<bool> active_;
    std::atomic<bool> finished_;
    std::atomic<bool> accept_buffer_;

    // Configuration
    gint width_;
    gint height_;
    // guint fps_;
    Profile profile_;
    std::string filename_;
    std::string basename_;

    // GL texture transfer
    // We need an FBO to blit from vimix's texture to GStreamer's GLMemory
    struct TextureTransferData {
        GPUVideoRecorder *grabber;
        guint vimix_texture_id;
        GstBuffer *buffer;  // Buffer with GLMemory to fill
    };

    static void perform_texture_transfer(GstGLContext *context, gpointer data);

    // GStreamer callbacks
    static void callback_need_data(GstAppSrc *src, guint length, gpointer user_data);
    static void callback_enough_data(GstAppSrc *src, gpointer user_data);
    static GstBusSyncReply bus_sync_handler(GstBus *bus, GstMessage *msg, gpointer user_data);

    // Helper to check encoder availability
    static bool isEncoderAvailable(Profile profile);

    // Build pipeline description
    std::string buildPipeline(Profile profile);
};

#endif // USE_GST_OPENGL_SYNC_HANDLER

#endif // GPUVIDEORECORDER_H
