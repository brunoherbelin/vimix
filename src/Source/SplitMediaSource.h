#ifndef SPLITMEDIASOURCE_H
#define SPLITMEDIASOURCE_H

#include <list>
#include <string>

#include "MediaSource.h"

class SplitMediaPlayer;

/**
 * @brief The SplitMediaSource class is a MediaSource playing
 * an ordered list of compatible video files as one media.
 */
class SplitMediaSource : public MediaSource
{
public:
    SplitMediaSource(uint64_t id = 0);

    // Source interface
    void accept (Visitor& v) override;
    glm::ivec2 icon() const override;
    std::string info() const override;

    // specific interface
    void setFiles (const std::list<std::string> &list_files);
    std::list<std::string> files () const;

    SplitMediaPlayer *splitmediaplayer () const;
};

#endif // SPLITMEDIASOURCE_H
