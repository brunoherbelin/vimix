#ifndef TRANSCODER_H
#define TRANSCODER_H

#include <string>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include "Toolkit/GstToolkit.h"

/**
 * @brief Configuration options for transcoding
 */
struct TranscoderOptions {
    bool force_keyframes;         ///< Force keyframe at every second (for easier seeking/editing)
    GstToolkit::Profile profile;  ///< Encoding profile (quality/codec settings)
    bool force_no_audio;          ///< Force removal of audio stream (create video-only output)

    /**
     * @brief Default constructor with sensible defaults
     */
    TranscoderOptions(bool force_keyframes = true
                    , GstToolkit::Profile profile = GstToolkit::H264_RT
                    , bool force_no_audio = false)
        : force_keyframes(force_keyframes)
        , profile(profile)
        , force_no_audio(force_no_audio)
    {}
};

/**
 * @brief Video transcoder class using GStreamer
 *
 * Re-encodes a video file using one of GstToolkit's encoding profiles.
 * Each instance handles transcoding of a single input file to an output file
 * (or, for GstToolkit::JPEG_MULTI, a numbered sequence of still images).
 */
class Transcoder
{
public:
    // Safety cap on the number of images produced when transcoding to
    // GstToolkit::JPEG_MULTI, so a long source video can't silently create
    // thousands of files.
    static constexpr int MAX_JPEG_FRAMES = 200;

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
     * @param options Transcoding options (keyframes, tuning, etc.)
     * @return true if transcoding started successfully, false otherwise
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
    const std::string& error() const { return error_message_; }

private:
    // Generate output filename (or, for JPEG_MULTI, output folder + pattern)
    // from input filename and options
    std::string generateOutputFilename(const std::string& input, const TranscoderOptions& options);

    // Poll the pipeline bus (non-blocking) for EOS/ERROR and update
    // finished_/success_/error_message_ accordingly
    void pollBus();

    std::string input_filename_;
    std::string output_filename_;
    std::string error_message_;

    GstElement *pipeline_;
    GstBus *bus_;

    bool started_;
    bool is_image_sequence_;  // true for GstToolkit::JPEG_MULTI (numbered images, not a muxed file)
    bool finished_;
    bool success_;
};

#endif // TRANSCODER_H
