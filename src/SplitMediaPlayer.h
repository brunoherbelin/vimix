#ifndef SPLITMEDIAPLAYER_H
#define SPLITMEDIAPLAYER_H

#include <list>
#include <string>
#include <vector>

#include "MediaPlayer.h"

/**
 * @brief The SplitMediaPlayer class plays an ordered list of video files
 * as a single continuous media, using the gstreamer splitmuxsrc element.
 *
 * All files must be compatible (same codec, resolution, framerate and audio);
 * each file is a 'segment' of the timeline.
 */
class SplitMediaPlayer : public MediaPlayer
{
public:
    SplitMediaPlayer();

    /**
     * Open the ordered list of video files
     * */
    void open(const std::list<std::string> &files);
    inline std::list<std::string> files() const { return files_; }

    /**
     * Name representing all the files,
     * e.g. 'vb_000[1-3].mov' for vb_0001.mov, vb_0002.mov and vb_0003.mov
     * */
    std::string filename() const override;

    /**
     * Segments: start time of each file in the timeline
     * (empty until the media is discovered)
     * */
    inline const std::vector<GstClockTime> &segments() const { return media_.segments; }
    int currentSegment();
    bool go_to_segment(int index);
    bool nextSegment();
    bool previousSegment();
    // time where next / previous segment jumps to (GST_CLOCK_TIME_NONE if none)
    GstClockTime nextSegmentTime();
    GstClockTime previousSegmentTime();

    /**
     * Discover all files and test if they can be played together.
     * Returns info on the concatenated media, with its segments
     * (invalid if not compatible, with the reason in log).
     * NB: blocking; call in a separate thread
     * */
    static MediaInfo SplitDiscoverer(const std::list<std::string> &files);
    /**
     * Pattern representing a list of filenames, with the range of
     * numbers (or '*') in place of the part that differs
     * */
    static std::string FilesPattern(const std::list<std::string> &files);
    /**
     * Info on the concatenated media from the info of each file, with its
     * segments (invalid if not compatible, with the reason in log)
     * */
    static MediaInfo SplitMediaInfo(const std::list<std::string> &files,
                                    const std::vector<MediaInfo> &infos);

protected:
    void startDiscovery() override;
    void setupPipeline() override;

private:
    int nextSegmentIndex();
    int previousSegmentIndex();

    std::list<std::string> files_;
    std::string files_pattern_;

    // gst callbacks
    static void callback_source_setup (GstElement *, GstElement *source, gpointer user_data);
    static gchar **callback_format_location (GstElement *, gpointer user_data);
};

#endif // SPLITMEDIAPLAYER_H
