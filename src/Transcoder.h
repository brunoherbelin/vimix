#ifndef TRANSCODER_H
#define TRANSCODER_H

#include <atomic>
#include <list>
#include <mutex>
#include <string>
#include <thread>
#include <variant>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include "Toolkit/GstToolkit.h"
#include "Upscaler.h"

/**
 * @brief What to encode the output with: a video profile or a still format
 *
 * A tagged union rather than two fields: the two are mutually exclusive, and
 * which one is meant is exactly what tells the Transcoder whether it handles
 * a stream of frames or a single image. std::holds_alternative() answers that
 * question, and a mistaken access throws instead of silently reinterpreting
 * one enum as the other.
 */
typedef std::variant<GstToolkit::Profile, GstToolkit::Image> TranscoderCodec;

/**
 * @brief Configuration options for transcoding
 *
 * The video-only options (keyframes, audio) are meaningless for a still image
 * and are left at their defaults by the image constructor, so that neither
 * caller can pass a setting which does not apply to what it is encoding.
 */
struct TranscoderOptions {
    TranscoderCodec codec;        ///< Video profile, or still image format
    bool force_keyframes;         ///< Force keyframe at every second (for easier seeking/editing)
    bool force_no_audio;          ///< Force removal of audio stream (create video-only output)
    std::string upscaler;         ///< Name of an UpscalerModel; Upscaler::NONE for no upscaling
    bool stream_copy;             ///< Copy video and audio streams without re-encoding (Matroska output)
    guint width, height;          ///< Output frame size, letterboxed (0 to keep the source size)
    guint framerate_n, framerate_d; ///< Output framerate (0 to keep the source framerate)
    GstClockTime begin, end;      ///< Range of the source to transcode (GST_CLOCK_TIME_NONE for start / end)

    /**
     * @brief Options to transcode a video, with sensible defaults
     */
    TranscoderOptions(GstToolkit::Profile profile = GstToolkit::H264_RT
                    , bool keyframes = false
                    , bool no_audio = false
                    , const std::string &upscaler = Upscaler::NONE)
        : codec(profile)
        , force_keyframes(keyframes)
        , force_no_audio(no_audio)
        , upscaler(upscaler)
        , stream_copy(false)
        , width(0), height(0)
        , framerate_n(0), framerate_d(1)
        , begin(GST_CLOCK_TIME_NONE), end(GST_CLOCK_TIME_NONE)
    {}

    /**
     * @brief Options to transcode a single still image
     */
    TranscoderOptions(GstToolkit::Image format
                    , const std::string &upscaler = Upscaler::NONE)
        : codec(format)
        , force_keyframes(false)
        , force_no_audio(true)
        , upscaler(upscaler)
        , stream_copy(false)
        , width(0), height(0)
        , framerate_n(0), framerate_d(1)
        , begin(GST_CLOCK_TIME_NONE), end(GST_CLOCK_TIME_NONE)
    {}

    /**
     * @brief True when the output is a single still image
     */
    bool isImage() const { return std::holds_alternative<GstToolkit::Image>(codec); }

    /**
     * @brief The video profile; only valid when isImage() is false
     */
    GstToolkit::Profile profile() const { return std::get<GstToolkit::Profile>(codec); }

    /**
     * @brief The still image format; only valid when isImage() is true
     */
    GstToolkit::Image format() const { return std::get<GstToolkit::Image>(codec); }
};

/**
 * @brief Video transcoder class using GStreamer
 *
 * Re-encodes a video file using one of GstToolkit's encoding profiles.
 * Each instance handles transcoding of a single input file to an output file
 * (or, for GstToolkit::JPEG_MULTI, a numbered sequence of still images).
 *
 * A still image source is re-encoded instead to one of GstToolkit's image
 * formats, selected by giving TranscoderOptions a GstToolkit::Image: the
 * pipeline is the same one frame shorter, without muxer, audio branch or
 * keyframe tuning.
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
     * @param output_filename Path of the output file; empty to generate it
     *
     * Unless given, the output filename (extension depending on the chosen
     * profile's container) will be automatically generated in the same folder
     * with a suffix, ensuring it doesn't overwrite existing files.
     */
    Transcoder(const std::string& input_filename, const std::string& output_filename = "");

    /**
     * @brief Construct a new Transcoder of a sequence of videos
     * @param input_files Paths of compatible videos (as played by a SplitMediaPlayer),
     *        encoded one after the other into a single output video
     * @param output_filename Path of the output file; empty to generate it
     */
    Transcoder(const std::list<std::string>& input_files, const std::string& output_filename = "");

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

    // Delete the output file (or folder) left behind by a transcoding which
    // did not run to completion
    void removeIncompleteOutput();

    // Record the outcome of the worker (thread safe)
    void setError(const std::string &message);
    void setStatus(const std::string &message);

    // Stream copy: parsebin -> matroskamux, branches linked when pads appear
    bool startCopy(const std::string &src_filename, bool has_audio);
    static void callback_copy_pad_added(GstElement *, GstPad *pad, gpointer user_data);
    static GstPadProbeReturn callback_copy_check_pts(GstPad *, GstPadProbeInfo *info, gpointer user_data);

    // Range: buffers are dropped at the entry of each branch until the seek
    // to the range has flushed it, so muxer and encoders only see the range
    bool hasRange() const;
    void addRangeProbe(GstPad *sinkpad);
    bool applyRange(bool accurate);
    static GstPadProbeReturn callback_range_probe(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);

    std::string input_filename_;
    std::list<std::string> input_files_;  // sequence of videos (splitmux), empty otherwise
    std::string output_filename_;

    // written by the worker thread, read by the UI thread
    mutable std::mutex message_mutex_;
    std::string error_message_;
    std::string status_message_;

    GstElement *pipeline_;
    GstBus *bus_;

    bool is_image_sequence_;  // true for GstToolkit::JPEG_MULTI (numbered images, not a muxed file)
    bool is_still_image_;     // true when encoding one image to a GstToolkit::Image format
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

    // range of the source to transcode
    GstClockTime range_begin_;
    GstClockTime range_end_;
    std::atomic<bool> range_ready_;
    bool range_by_eos_;           // end of range by EOS in the branches (no clipping of last frame)
    GstPad *range_pad_;           // pad upstream of a branch, where to send the seek
    std::mutex range_mutex_;

    // stream copy
    GstElement *copy_mux_;
    bool copy_audio_;
    std::atomic<bool> copy_failed_;
};

/**
 * @brief Transcoder of a sequence of still images or videos
 *
 * With options for a still image, re-encodes every image of a list to one of
 * GstToolkit's image formats, optionally upscaled, into a new folder created
 * next to the images: the files keep their name, with the extension of the
 * new format.
 *
 * With options for a video, re-encodes (or copies) a list of videos, or a
 * list of time ranges of one video, into a folder of consecutively numbered
 * video files (name_00001.mov, ...), e.g. to be played by a SplitMediaSource.
 *
 * The items are transcoded one after the other by a Transcoder, in a worker
 * thread. Driven like a Transcoder: start(), then poll finished() / progress().
 */
class SequenceTranscoder
{
public:
    /**
     * @brief Construct a new SequenceTranscoder
     * @param input_files Paths of the images or videos, in the order of the sequence
     */
    SequenceTranscoder(const std::list<std::string>& input_files);
    /**
     * @brief Construct a new SequenceTranscoder of ranges of a video
     * @param input_file Path of the video
     * @param ranges List of [begin, end] time ranges, in the order of the sequence
     */
    SequenceTranscoder(const std::string& input_file,
                       const std::list< std::pair<GstClockTime, GstClockTime> >& ranges);
    ~SequenceTranscoder();

    /**
     * @brief Start transcoding
     * @param options Options for a still image, or for a video
     * @param output_folder Folder of video files (created if needed);
     *        empty for a new folder next to the input files
     * @return true if started, false if options or files are not valid
     */
    bool start(const TranscoderOptions& options, const std::string &output_folder = "");

    /**
     * @brief Stop transcoding, and remove the files produced
     */
    void stop();

    bool finished() const { return finished_; }
    bool success() const { return finished_ && success_; }
    double progress() const;
    std::string error() const;
    std::string status() const;

    /**
     * @brief Folder of the transcoded images or videos
     */
    const std::string& outputFolder() const { return output_folder_; }

    /**
     * @brief Paths of the transcoded images or videos; complete once success()
     */
    std::list<std::string> outputFiles() const;

private:
    void run(TranscoderOptions options);

    struct Item {
        std::string file;
        GstClockTime begin, end;
    };
    std::list<Item> input_;
    std::string output_folder_;

    // written by the worker thread, read by the UI thread
    mutable std::mutex mutex_;
    std::list<std::string> output_files_;
    std::string error_message_;
    std::string status_message_;
    std::atomic<double> progress_;

    std::thread worker_;
    std::atomic<bool> started_;
    std::atomic<bool> abort_;
    std::atomic<bool> finished_;
    std::atomic<bool> success_;
};

#endif // TRANSCODER_H
