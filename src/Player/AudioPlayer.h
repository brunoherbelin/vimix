#ifndef AUDIOPLAYER_H
#define AUDIOPLAYER_H

#include <string>

#include "Player/MediaPlayer.h"

#define AUDIO_VISUALIZATION_WIDTH 320
#define AUDIO_VISUALIZATION_HEIGHT 180
#define AUDIO_VISUALIZATION_FPS 25

/**
 * @brief The AudioPlayer class plays an audio file as a media,
 * using the playbin audio visualization (wavescope, or monoscope for GStreamer <= 1.24) as video.
 *
 * The timeline, seeking, speed, loop and volume are those of a MediaPlayer;
 * the frames are images of the audio waveform.
 * Requires audio to be enabled in Settings.
 */
class AudioPlayer : public MediaPlayer
{
public:
    AudioPlayer();

    /**
     * Audio is always enabled (mute is volume 0)
     * */
    void setAudioEnabled(bool) override {}

    /**
     * Discover audio file and get media info
     * (invalid if no audio stream, or if audio is disabled in Settings)
     * NB: blocking; call in a separate thread
     * */
    static MediaInfo AudioDiscoverer(const std::string &uri);

protected:
    void startDiscovery() override;
    void setupPipeline() override;
};

#endif // AUDIOPLAYER_H
