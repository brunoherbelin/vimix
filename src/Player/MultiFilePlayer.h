#ifndef MULTIFILEPLAYER_H
#define MULTIFILEPLAYER_H

#include <list>
#include <string>

#include "Player/MediaPlayer.h"

/**
 * @brief Description of a sequence of consecutively numbered images,
 * e.g. 'frames%03d.png' for frames001.png ... frames120.png
 */
struct MultiFileSequence {
    std::string location;
    std::string codec;
    uint width;
    uint height;
    int min;
    int max;

    MultiFileSequence ();
    MultiFileSequence (const std::list<std::string> &list_files);
    MultiFileSequence (const std::string &path);
    bool valid () const;
    bool operator != (const MultiFileSequence& b);
};

/**
 * @brief The MultiFilePlayer class plays a sequence of numbered images
 * as a video, using the gstreamer imagesequencesrc element (through the
 * 'imagesequence://' uri of playbin).
 *
 * Every image is a frame; the timeline (duration, seek, speed, loop modes,
 * gaps, fading...) is the one of any MediaPlayer.
 */
class MultiFilePlayer : public MediaPlayer
{
public:
    MultiFilePlayer();

    /**
     * Open the sequence at the given framerate
     * */
    void open(const MultiFileSequence &sequence, uint framerate);
    inline MultiFileSequence sequence() const { return sequence_; }

    /**
     * Name representing all the images,
     * e.g. 'frames0[02-47].png' for frames002.png to frames047.png
     * */
    std::string filename() const override;
    inline uint framerate() const { return fps_; }

    /**
     * Change the framerate of an open sequence.
     * NB: this re-opens the sequence (playing from the start).
     * Returns false if not changed.
     * */
    bool setFramerate(uint framerate);

    /**
     * Images are always decoded by software
     * */
    bool softwareDecodingForced() override { return true; }
    void setSoftwareDecodingForced(bool) override {}

    /**
     * True if gstreamer provides the element reading sequences
     * */
    static bool available();
    /**
     * Gstreamer uri of imagesequencesrc for the sequence
     * */
    static std::string SequenceUri(const MultiFileSequence &sequence, uint framerate);
    /**
     * Info on the sequence played as a video (invalid if files are missing)
     * */
    static MediaInfo SequenceInfo(const MultiFileSequence &sequence, uint framerate);

protected:
    void startDiscovery() override;

private:
    MultiFileSequence sequence_;
    std::string sequence_name_;
    uint fps_;
};

#endif // MULTIFILEPLAYER_H
