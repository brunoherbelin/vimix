#ifndef AUDIOSOURCE_H
#define AUDIOSOURCE_H

#include "MediaSource.h"

class AudioPlayer;

/**
 * @brief The AudioSource class is a MediaSource playing an audio file.
 *
 * It has the timeline of a media, but no image in the mix: it is shown
 * only in the Mixing view and in the Player, with a visualization
 * of the audio waveform. Its volume follows its alpha in the Mixing view.
 */
class AudioSource : public MediaSource
{
public:
    AudioSource(uint64_t id = 0);

    // Source interface
    uint texture() const override;
    bool hasImage() const override { return false; }
    bool texturePostProcessed () const override { return false; }
    void updateAudio() override;
    void accept (Visitor& v) override;
    glm::ivec2 icon() const override;
    std::string_view type() const override;
    std::string info() const override;

    // specific interface
    AudioPlayer *audioplayer () const;
};

#endif // AUDIOSOURCE_H
