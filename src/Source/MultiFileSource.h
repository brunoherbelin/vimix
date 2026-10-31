#ifndef MULTIFILESOURCE_H
#define MULTIFILESOURCE_H

#include <string>
#include <list>

#include "MediaSource.h"
#include "Player/MultiFilePlayer.h"

/**
 * @brief The MultiFileSource class is a MediaSource playing
 * a sequence of consecutively numbered images as a video.
 */
class MultiFileSource : public MediaSource
{
public:
    MultiFileSource (uint64_t id = 0);

    // Source interface
    void accept (Visitor& v) override;
    glm::ivec2 icon() const override;
    std::string_view type() const override;
    std::string info() const override;

    // specific interface
    void setFiles (const std::list<std::string> &list_files, uint framerate);

    void setSequence (const MultiFileSequence &sequence, uint framerate);
    MultiFileSequence sequence () const;

    void setFramerate (uint fps);
    uint framerate () const;

    MultiFilePlayer *multifileplayer () const;
};

#endif // MULTIFILESOURCE_H
