/*
 * MultiFileRifeEncoder — turn a folder of images into an H.264/MP4 video,
 * generating intermediate frames with a pre-trained Real-Time Intermediate 
 * Flow Estimation frame-interpolation model on the GPU (ncnn/Vulkan) or 
 * CPU (ONNX Runtime).
 *
 * The class has one instance per input folder, asynchronous
 * start()/stop(), polled with finished(), success() and progress(). The
 * inference backends are an internal implementation detail — which ones
 * exist is a build-time decision (HAVE_NCNN / HAVE_ONNX, set by CMake from
 * library availability); with none, the images are encoded without
 * interpolation.
 */

#ifndef MULTIFILERIFEENCODER_H
#define MULTIFILERIFEENCODER_H

#include <list>
#include <atomic>
#include <string>
#include <thread>

#include "Toolkit/GstToolkit.h"

/**
 * @brief Configuration options for RIFE encoding
 */
struct RifeOptions {
    int mid;              ///< intermediate frames per image pair (0 = none)
    int fps;              ///< output framerate
    int loop;
    GstToolkit::Profile profile;  /// output profile (H.264/H.265/ProRes/VPX_RT)
    std::string backend;  ///< "auto" | "ncnn" | "onnx"

    RifeOptions(int mid = 1,
                int fps = 30,
                int loop = 0,
                GstToolkit::Profile profile = GstToolkit::H264_RT,
                const std::string &backend = "auto")
        : mid(mid)
        , fps(fps)
        , loop(loop)
        , profile(profile)
        , backend(backend)
    {}
};

/**
 * @brief Frame-interpolating video encoder using GStreamer and RIFE
 *
 * Reads a folder of images (JPEG/PNG) with GStreamer, generates intermediate
 * frames with RIFE, and encodes the result to H.264/MP4. Each instance
 * handles a single input folder to an output file.
 */
class MultiFileRifeEncoder
{
public:
    /**
     * @brief Construct a new encoder
     *
     * The output filename is generated in start() as "<input_dir>_rife.mp4",
     * ensuring it doesn't overwrite existing files.
     */
    MultiFileRifeEncoder();

    /**
     * @brief Destroy the encoder: stops any ongoing work and cleans up
     */
    ~MultiFileRifeEncoder();

    void reset();

    inline void setFiles (std::list<std::string> list) { files_ = list; }
    inline std::list<std::string> files () const { return files_; }

    /**
     * @brief Start the encoding process with optional configuration
     * @param options Encoding options (interpolation, framerate, backend)
     * @return true if it started successfully, false otherwise
     *
     * Processing runs in a background thread; poll finished()/progress().
     * The model files are downloaded on first use (needs network once).
     */
    bool start(const RifeOptions &options = RifeOptions());

    /**
     * @brief Stop the encoding process
     *
     * Cleanly stops an in-progress operation and removes the incomplete
     * output file. Does nothing if already finished or not started.
     */
    void stop();

    /**
     * @brief Check if encoding has finished (success or error)
     */
    bool finished() const;

    /**
     * @brief Check if encoding completed successfully
     */
    bool success() const;

    /**
     * @brief Get the input folder path
     */
    // const std::string &inputDir() const { return input_dir_; }

    /**
     * @brief Get the output filename
     */
    const std::string &filename() const { return output_filename_; }

    /**
     * @brief Get encoding progress (0.0 to 1.0)
     */
    double progress() const;

    /**
     * @brief Get the total number of frames
     */
    inline int numFrames () const { return total_frames_; }

    /**
     * @brief Get error message if encoding failed (empty if none)
     */
    const std::string &message() const { return message_; }

private:
    // Generate output filename from input dir (suffix "_rife.mp4")
    static std::string generateOutputFilename(const std::list<std::string> & files);

    // Background processing loop (decode -> RIFE -> encode)
    void run(RifeOptions options);

    // std::string input_dir_;
    std::list<std::string> files_;
    std::string output_filename_;
    std::string message_;

    std::thread worker_;

    std::atomic<bool> started_;
    std::atomic<bool> finished_;
    std::atomic<bool> success_;
    std::atomic<bool> abort_;

    std::atomic<int> total_frames_;
    std::atomic<int> done_frames_;
};

#endif // MULTIFILERIFEENCODER_H
