#ifndef FRAMEGRABBING_H
#define FRAMEGRABBING_H

#include <list>
#include <map>
#include <string>

#include <gst/gst.h>

#include "FrameGrabber.h"

class FrameBuffer;

/**
 * @brief Manages all frame grabbers in the application
 *
 * FrameGrabbing is a singleton that coordinates all FrameGrabber instances,
 * handling frame capture from the rendered output and distributing frames
 * to active grabbers (recorders, broadcasters, etc.).
 *
 * The class uses PBO (Pixel Buffer Objects) for efficient GPU-to-CPU frame
 * transfers and maintains the pipeline state for all active grabbers.
 *
 * @note This is a singleton class - use FrameGrabbing::manager() to access
 * @note Session calls grabFrame() after each render cycle
 *
 * Thread Safety: Not thread-safe - should be accessed from render thread only
 */
class FrameGrabbing
{
    friend class Mixer;

    // Private Constructor (Singleton pattern)
    FrameGrabbing();
    FrameGrabbing(FrameGrabbing const& copy) = delete;
    FrameGrabbing& operator=(FrameGrabbing const& copy) = delete;

public:

    /**
     * @brief Get the singleton instance
     * @return Reference to the FrameGrabbing manager instance
     */
    static FrameGrabbing& manager()
    {
        // The only instance
        static FrameGrabbing _instance;
        return _instance;
    }

    /**
     * @brief Destructor - cleans up all grabbers and resources
     */
    ~FrameGrabbing();

    /**
     * @brief Get the width of captured frames
     * @return Frame width in pixels
     */
    inline uint width() const { return width_; }

    /**
     * @brief Get the height of captured frames
     * @return Frame height in pixels
     */
    inline uint height() const { return height_; }

    /**
     * @brief Add a new frame grabber to the active list
     * @param rec Pointer to the FrameGrabber to add (takes ownership)
     * @param duration Optional duration in seconds (0 = unlimited)
     *
     * The grabber will start receiving frames on the next render cycle.
     * When duration expires or grabber finishes, it will be automatically removed.
     */
    void add(FrameGrabber *rec, uint64_t duration = 0);

    /**
     * @brief Chain a new grabber to start after the current is interrupted
     * @param rec Current FrameGrabber that is running
     * @param new_rec New FrameGrabber to start 
     *
     * Useful for continuous recording - when one file finishes, another starts.
     */
    void chain(FrameGrabber *rec, FrameGrabber *new_rec);

    /**
     * @brief Check if any frame grabbers are currently active
     * @return true if at least one grabber is active, false otherwise
     */
    bool busy() const;

    /**
     * @brief Get a frame grabber by its unique ID
     * @param id The unique identifier of the grabber
     * @return Pointer to the FrameGrabber, or nullptr if not found
     */
    FrameGrabber *get(uint64_t id);

    /**
     * @brief Get a frame grabber by its type
     * @param t The FrameGrabber::Type to search for
     * @return Pointer to the first FrameGrabber of this type, or nullptr if none active
     */
    FrameGrabber *get(FrameGrabber::Type t);

    /**
     * @brief Stop all active frame grabbers
     *
     * Sends stop signal to all grabbers and waits for them to finish cleanly.
     */
    void stopAll();

    /**
     * @brief Clear all finished grabbers from the list
     *
     * Removes and deletes all grabbers that have completed or failed.
     */
    void clearAll();

protected:

    /**
     * @brief Capture current frame and distribute to all active grabbers
     * @param frame_buffer The FrameBuffer containing the rendered frame
     *
     * Called by Session after each render. Uses PBOs for efficient GPU readback.
     * Only accessible to friend class Mixer.
     */
    void grabFrame(FrameBuffer *frame_buffer);

private:
    std::list<FrameGrabber *> grabbers_;
    std::map<FrameGrabber *, FrameGrabber *> grabbers_chain_;
    std::map<FrameGrabber *, uint64_t> grabbers_duration_;
    guint pbo_[2];
    guint pbo_index_;
    guint pbo_next_index_;
    guint size_;
    guint width_;
    guint height_;
    bool  use_alpha_;
    GstCaps *caps_;
};

/**
 * @brief Global manager for output FrameGrabbers (recordings, broadcasts, loopback, etc.)
 *
 * Outputs provides a safe interface to launch and stop output FrameGrabbers.
 * Since FrameGrabbers can terminate automatically (on completion or failure),
 * direct pointers become invalid. This class provides access by FrameGrabber::Type
 * instead, ensuring only one active instance per type exists.
 *
 * Supports all output types:
 * - Video recording (GRABBER_VIDEO, GRABBER_GPU)
 * - PNG snapshots (GRABBER_PNG)
 * - SRT broadcast (GRABBER_BROADCAST)
 * - Shared memory (GRABBER_SHM)
 * - Loopback camera (GRABBER_LOOPBACK)
 *
 * @note This is a singleton class - use Outputs::manager() to access
 * @note Singleton mechanism ensures only one active instance per FrameGrabber::Type
 *
 * @example
 * @code
 * // Start a video recorder
 * Outputs::manager().start(new VideoRecorder("output.mp4"));
 *
 * // Check if any output is active
 * if (Outputs::manager().enabled(FrameGrabber::GRABBER_VIDEO,
 *                                 FrameGrabber::GRABBER_BROADCAST)) {
 *     // At least one is running
 * }
 *
 * // Stop recording
 * Outputs::manager().stop(FrameGrabber::GRABBER_VIDEO);
 * @endcode
 *
 * Thread Safety: Not thread-safe - should be accessed from main/UI thread only
 */
class Outputs
{
    // Private Constructor (Singleton pattern)
    Outputs() {};
    Outputs(Outputs const& copy) = delete;
    Outputs& operator=(Outputs const& copy) = delete;

    bool delayed[FrameGrabber::GRABBER_INVALID] = { false };

public:

    /**
     * @brief Get the singleton instance
     * @return Reference to the Outputs manager instance
     */
    static Outputs& manager ()
    {
        // The only instance
        static Outputs _instance;
        return _instance;
    }

    /**
     * @brief Start a new output FrameGrabber
     * @param ptr Pointer to the FrameGrabber to start (takes ownership)
     * @param delay Optional delay before starting (default: immediate)
     * @param timeout Optional timeout in seconds (0 = unlimited)
     *
     * The FrameGrabber is registered with FrameGrabbing and will start
     * receiving frames. If delay > 0, a countdown timer is shown before starting.
     *
     * @note Only one instance per FrameGrabber::Type can be active
     * @see chain() for starting a new grabber when current one finishes
     */
    void start(FrameGrabber *ptr,
        std::chrono::seconds delay = std::chrono::seconds(0),
        uint64_t timeout = 0);

    /**
     * @brief Chain a new grabber to start when current one is interrupted
     * @param new_rec New FrameGrabber to start after current one completes
     *
     * Useful for continuous recording - seamlessly starts a new file
     * when the current recording is interrupted.
     *
     * @note The current grabber is determined by new_rec->type()
     */
    void chain(FrameGrabber *new_rec);

    /**
     * @brief Check if a grabber is pending (delayed start)
     * @param type The FrameGrabber::Type to check
     * @return true if grabber is in delayed countdown, false otherwise
     */
    bool pending(FrameGrabber::Type type);

    /**
     * @brief Check if any of the given grabber types are pending
     * @tparam Types Variadic template for multiple FrameGrabber::Type arguments
     * @param first First FrameGrabber::Type to check
     * @param rest Additional FrameGrabber::Type arguments (0 or more)
     * @return true if ANY of the given types are pending, false otherwise
     *
     * Uses variadic template recursion to check multiple types efficiently.
     *
     * @example
     * @code
     * // Check if any output is pending
     * if (Outputs::manager().pending(FrameGrabber::GRABBER_VIDEO,
     *                                 FrameGrabber::GRABBER_BROADCAST)) {
     *     // At least one is in countdown
     * }
     * @endcode
     */
    template<typename... Types>
    bool pending(FrameGrabber::Type first, Types... rest) {
        return pending(first) || pending(rest...);
    }

    /**
     * @brief Check if a grabber is currently active
     * @param type The FrameGrabber::Type to check
     * @return true if grabber is running, false otherwise
     */
    bool enabled(FrameGrabber::Type type);

    /**
     * @brief Check if any of the given grabber types are active
     * @tparam Types Variadic template for multiple FrameGrabber::Type arguments
     * @param first First FrameGrabber::Type to check
     * @param rest Additional FrameGrabber::Type arguments (0 or more)
     * @return true if ANY of the given types are enabled, false otherwise
     *
     * Uses variadic template recursion for efficient multi-type checking.
     * Compiles to equivalent of: enabled(first) || enabled(arg2) || enabled(arg3) || ...
     *
     * @example
     * @code
     * // Check if recording or broadcasting
     * if (Outputs::manager().enabled(FrameGrabber::GRABBER_VIDEO,
     *                                 FrameGrabber::GRABBER_BROADCAST,
     *                                 FrameGrabber::GRABBER_SHM)) {
     *     // Show "output active" indicator
     * }
     * @endcode
     */
    template<typename... Types>
    bool enabled(FrameGrabber::Type first, Types... rest) {
        return enabled(first) || enabled(rest...);
    }

    /**
     * @brief Check if a grabber is currently processing
     * @param type The FrameGrabber::Type to check
     * @return true if grabber is actively processing frames, false otherwise
     */
    bool busy(FrameGrabber::Type type);

    /**
     * @brief Check if any of the given grabber types are busy
     * @tparam Types Variadic template for multiple FrameGrabber::Type arguments
     * @param first First FrameGrabber::Type to check
     * @param rest Additional FrameGrabber::Type arguments (0 or more)
     * @return true if ANY of the given types are busy, false otherwise
     *
     * @example
     * @code
     * // Check if any output is busy processing
     * if (Outputs::manager().busy(FrameGrabber::GRABBER_VIDEO,
     *                             FrameGrabber::GRABBER_BROADCAST)) {
     *     // At least one is actively processing frames
     * }
     * @endcode
     */
    template<typename... Types>
    bool busy(FrameGrabber::Type first, Types... rest) {
        return busy(first) || busy(rest...);
    }

    /**
     * @brief Get information string about a grabber
     * @param type The FrameGrabber::Type to query
     * @param extended If true, return detailed info; if false, return brief info
     * @return String describing the grabber state (filename, URL, device, etc.)
     *
     * @example For GRABBER_VIDEO: "Recording: output.mp4"
     * @example For GRABBER_BROADCAST: "srt://localhost:9998"
     */
    std::string info(bool extended, FrameGrabber::Type type);

    /**
     * @brief Get concatenated information strings for multiple grabber types
     * @tparam Types Variadic template for multiple FrameGrabber::Type arguments
     * @param extended If true, return detailed info; if false, return brief info
     * @param first First FrameGrabber::Type to query
     * @param rest Additional FrameGrabber::Type arguments (0 or more)
     * @return Concatenated string with info for all types, separated by newlines
     *
     * Each enabled grabber's info is on a separate line. Disabled grabbers are skipped.
     *
     * @example
     * @code
     * // Get info for all active outputs
     * std::string all_info = Outputs::manager().info(false,
     *     FrameGrabber::GRABBER_VIDEO,
     *     FrameGrabber::GRABBER_BROADCAST,
     *     FrameGrabber::GRABBER_SHM);
     * // Returns: "Recording: file.mp4\nsrt://localhost:9998\nShared Memory"
     * @endcode
     */
    template<typename... Types>
    std::string info(bool extended, FrameGrabber::Type first, Types... rest) {
        std::string result = info(extended, first);
        if constexpr (sizeof...(rest) > 0) {
            std::string rest_info = info(extended, rest...);
            // If current result is "Disabled", replace with rest_info
            if (result == "Disabled") {
                result = rest_info;
            }
            // Otherwise, append rest_info if it's not "Disabled" or empty
            else if (rest_info != "Disabled" && !rest_info.empty()) {
                result += "\n" + rest_info;
            }
        }
        return result;
    }

    /**
     * @brief Stop a running grabber
     * @param type The FrameGrabber::Type to stop
     *
     * Sends stop signal to the grabber, allowing it to finish cleanly
     * and finalize output (close file, disconnect stream, etc.).
     */
    void stop(FrameGrabber::Type type);
    template<typename... Types>
    void stop(FrameGrabber::Type first, Types... rest) {
        stop(first);
        stop(rest...);
    }

    /**
     * @brief Check if a grabber is paused
     * @param type The FrameGrabber::Type to check
     * @return true if grabber is paused, false if running or not active
     */
    bool paused(FrameGrabber::Type type);
    template<typename... Types>
    bool paused(FrameGrabber::Type first, Types... rest) {
        return paused(first) || paused(rest...);
    }

    /**
     * @brief Pause a running grabber
     * @param type The FrameGrabber::Type to pause
     *
     * Temporarily stops frame capture while keeping the grabber active.
     * Useful for video recording - pause without stopping the file.
     */
    void pause(FrameGrabber::Type type);
    template<typename... Types>
    void pause(FrameGrabber::Type first, Types... rest) {
        pause(first);
        pause(rest...);
    }

    /**
     * @brief Resume a paused grabber
     * @param type The FrameGrabber::Type to resume
     *
     * Continues frame capture after pause().
     */
    void unpause(FrameGrabber::Type type);
    template<typename... Types>
    void unpause(FrameGrabber::Type first, Types... rest) {
        unpause(first);
        unpause(rest...);
    }
};


#endif // FRAMEGRABBING_H
