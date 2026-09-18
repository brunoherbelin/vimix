#ifndef TRANSCODER_H
#define TRANSCODER_H

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include "Toolkit/GstToolkit.h"
#include "Upscaler.h"

/**
 * @brief Configuration options for transcoding
 */
struct TranscoderOptions {
    GstToolkit::Profile profile;  ///< Encoding profile (quality/codec settings)
    bool force_keyframes;         ///< Force keyframe at every second (for easier seeking/editing)
    bool force_no_audio;          ///< Force removal of audio stream (create video-only output)
    std::string upscaler;         ///< Name of an UpscalerModel; Upscaler::NONE for no upscaling

    /**
     * @brief Default constructor with sensible defaults
     */
    TranscoderOptions(GstToolkit::Profile profile = GstToolkit::H264_RT
                    , bool keyframes = false
                    , bool no_audio = false
                    , const std::string &upscaler = Upscaler::NONE)
        : profile(profile)
        , force_keyframes(keyframes)
        , force_no_audio(no_audio)
        , upscaler(upscaler)
    {}
};

/**
 * @brief Video transcoder class using GStreamer
 *
 * Re-encodes a video file using one of GstToolkit's encoding profiles.
 * Each instance handles transcoding of a single input file to an output file
 * (or, for GstToolkit::JPEG_MULTI, a numbered sequence of still images).
 *
 * When TranscoderOptions names an upscaling model, every frame is enlarged
 * on the GPU with Real-ESRGAN (ncnn / Vulkan) before being encoded. That
 * inference cannot live inside a gstreamer pipeline, so this mode splits
 * decoding and encoding into two pipelines joined by a worker thread, and
 * the output is video only (any audio stream is dropped). Either way the
 * class is driven identically: start(), then poll finished() / progress().
 */
class Transcoder
{
public:
    // Safety cap on the number of images produced when transcoding to
    // GstToolkit::JPEG_MULTI, so a long source video can't silently create
    // thousands of files.
    static constexpr int MAX_JPEG_FRAMES = 600;

    /**
     * @brief Construct a new Transcoder
     * @param input_filename Path to the input video file
     *
     * The output filename (extension depending on the chosen profile's
     * container) will be automatically generated in the same folder with a
     * "_transcoded" suffix, ensuring it doesn't overwrite existing files.
     */
    Transcoder(const std::string& input_filename);

    /**
     * @brief Destroy the Transcoder and clean up resources
     */
    ~Transcoder();

    /**
     * @brief Start the transcoding process with optional configuration
     * @param options Transcoding options (profile, keyframes, upscaling, etc.)
     * @return true if transcoding started successfully, false otherwise
     *
     * When upscaling, the work happens in a background thread: a true here
     * only means the options are valid and the file could be opened, and a
     * later failure is reported through finished() / success() / error().
     */
    bool start(const TranscoderOptions& options = TranscoderOptions());

    /**
     * @brief Stop the transcoding process
     *
     * Cleanly stops an in-progress transcoding operation and removes the incomplete
     * output file. If transcoding has already finished or hasn't started, this method
     * does nothing.
     */
    void stop();

    /**
     * @brief Check if transcoding has finished
     * @return true if transcoding is complete (success or error), false if still running
     */
    bool finished();

    /**
     * @brief Check if transcoding completed successfully
     * @return true if finished successfully, false if still running or failed
     */
    bool success();

    /**
     * @brief Get the input filename
     * @return const std::string& Input file path
     */
    const std::string& inputFilename() const { return input_filename_; }

    /**
     * @brief Get the output filename
     * @return const std::string& Output file path
     */
    const std::string& outputFilename() const { return output_filename_; }

    /**
     * @brief Get transcoding progress (0.0 to 1.0)
     * @return double Progress percentage (0.0 = starting, 1.0 = complete)
     */
    double progress();

    /**
     * @brief Get error message if transcoding failed
     * @return const std::string& Error message, empty if no error
     */
    std::string error() const;

    /**
     * @brief Get a short description of what is going on right now
     * @return std::string Status message, empty when simply transcoding
     *
     * Used to tell the user about the long preliminary steps of upscaling
     * (downloading the model, initializing the GPU) before any frame is
     * produced and progress() starts to move.
     */
    std::string status() const;

    /**
     * @brief Check if the output is an image sequence
     * @return true if the output is a sequence of images, false otherwise
     */
    bool isImageSequence() const { return is_image_sequence_; }

private:
    // Generate output filename (or, for JPEG_MULTI, output folder + pattern)
    // from input filename and options
    std::string generateOutputFilename(const std::string& input, const TranscoderOptions& options);

    // Poll the pipeline bus (non-blocking) for EOS/ERROR and update
    // finished_/success_/error_message_ accordingly
    void pollBus();

    // Background upscaling: decode -> Real-ESRGAN -> encode, run in worker_
    void runUpscale(TranscoderOptions options);

    // Record the outcome of the worker (thread safe)
    void setError(const std::string &message);
    void setStatus(const std::string &message);

    std::string input_filename_;
    std::string output_filename_;

    // written by the worker thread, read by the UI thread
    mutable std::mutex message_mutex_;
    std::string error_message_;
    std::string status_message_;

    GstElement *pipeline_;
    GstBus *bus_;

    bool is_image_sequence_;  // true for GstToolkit::JPEG_MULTI (numbered images, not a muxed file)
    std::atomic<bool> started_;
    std::atomic<bool> finished_;
    std::atomic<bool> success_;

    // upscaling mode only: worker thread and the progress it publishes
    std::thread worker_;
    std::atomic<bool> abort_;
    std::atomic<gint64> duration_;
    std::atomic<gint64> position_;
    std::string decode_desc_;     // source -> RGB frames, built in start()
    std::string video_encoder_;   // encoder fragment for the profile, chosen in start()
    int upscale_factor_;          // 1 when not upscaling
};

#endif // TRANSCODER_H
