#ifndef SESSIONTHUMBNAILSOURCE_H
#define SESSIONTHUMBNAILSOURCE_H

#include <future>

#include "SessionCreator.h"
#include "Source.h"

// Light Source showing the thumbnail of a session file
// (never inserted in a Session; owned and driven by the PresentationView)
class SessionThumbnailSource : public Source
{
public:
    SessionThumbnailSource(const std::string &path);
    ~SessionThumbnailSource();

    // implementation of source API
    void render() override;
    bool playable () const override { return false; }
    bool playing () const override { return false; }
    void play (bool) override {}
    Failure failed () const override { return FAIL_NONE; }
    uint texture () const override;

    glm::ivec2 icon() const override { return glm::ivec2(ICON_VI_SOURCE_SESSION); }
    std::string info() const override { return "Session thumbnail"; }

    // SessionThumbnail Source specific interface
    inline std::string path() const { return path_; }
    inline bool missing() const { return missing_; }

protected:

    void init() override;

    std::string path_;
    bool missing_;
    uint texture_;
    bool rendered_;
    std::future<SessionInformation> infoLoader_;
};

#endif // SESSIONTHUMBNAILSOURCE_H
