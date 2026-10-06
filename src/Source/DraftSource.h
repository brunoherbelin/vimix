#ifndef DRAFTSOURCE_H
#define DRAFTSOURCE_H

#include "CloneSource.h"

/**
 * @brief The DraftSource class is a proxy of a source in the draft session
 *
 * A DraftSource does not produce content: it renders the content of its
 * origin (the source of the live session) with its own properties
 * (geometry, alpha, color correction, crop, etc.) edited in DRAFT mode.
 * Playing controls are forwarded to the origin.
 *
 * While its content properties are the same as its origin, it does not
 * render its content and shows the frame of its origin instead.
 */
class DraftSource : public CloneSource
{
public:
    DraftSource(Source *origin, uint64_t id = 0);

    // implementation of source API
    void update (float dt) override;
    void setActive (bool on) override;
    bool playing () const override;
    void play (bool on) override;
    bool playable () const  override;
    void replay () override;
    void reload () override;
    guint64 playtime () const override;
    uint texture() const override;
    Failure failed() const override;
    FrameBuffer *frame () const override;
    void accept (Visitor& v) override;
    void render() override;
    glm::ivec2 icon() const override;
    std::string info() const override;
    bool texturePostProcessed() const override;

protected:
    void init() override;

    // content is shared with origin
    bool shared_;
};

#endif // DRAFTSOURCE_H
