#ifndef TRANSCODER_H
#define TRANSCODER_H

#include <string>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

/**
 * @brief Psycho-visual tuning options for x264 encoder
 */
enum class PsyTuning {
    NONE = 0,      ///< No specific tuning
    FILM = 1,      ///< Optimize for film content
    ANIMATION = 2, ///< Optimize for animation/cartoon content
    GRAIN = 3,     ///< Preserve film grain
    STILL_IMAGE = 4 ///< Optimize for still image/slideshow content
};

/**
 * @brief Configuration options for transcoding
 */
struct TranscoderOptions {
    bool force_keyframes;    ///< Force keyframe at every second (for easier seeking/editing)
    PsyTuning psy_tune;      ///< Psycho-visual tuning preset
    int crf;                 ///< Constant Rate Factor (0-51, 0=lossless, 23=default, -1=use bitrate mode)
    bool force_no_audio;     ///< Force removal of audio stream (create video-only output)

    /**
     * @brief Default constructor with sensible defaults
     */
    TranscoderOptions(bool force_keyframes = true
                    , PsyTuning psy_tune = PsyTuning::NONE
                    , int crf = -1
                    , bool force_no_audio = false)
        : force_keyframes(force_keyframes)
        , psy_tune(psy_tune)
        , crf(crf)  // -1 = use bitrate mode (default)
        , force_no_audio(force_no_audio)
    {}
};

/**
 * @brief Video transcoder class using GStreamer
 *
 * Encodes video files to H.264/MP4 format using GStreamer's encoding capabilities.
 * Each instance handles transcoding of a single input file to an output file.
 */
class Transcoder
{
public:
    /**
     * @brief Construct a new Transcoder
     * @param input_filename Path to the input video file
     *
     * The output filename will be automatically generated in the same folder
     * with "_transcoded.mp4" suffix, ensuring it doesn't overwrite existing files.
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
    bool finished() const;

    /**
     * @brief Check if transcoding completed successfully
     * @return true if finished successfully, false if still running or failed
     */
    bool success() const;

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
    double progress() const;

    /**
     * @brief Get error message if transcoding failed
     * @return const std::string& Error message, empty if no error
     */
    const std::string& error() const { return error_message_; }

private:
    // Generate output filename from input filename and options
    std::string generateOutputFilename(const std::string& input, const TranscoderOptions& options);

    std::string input_filename_;
    std::string output_filename_;
    std::string error_message_;

    GObject *transcoder_;  // GstTranscoder object

    bool started_;
    bool finished_;
    bool success_;

    gint64 duration_;
    gint64 position_;
};

#endif // TRANSCODER_H
