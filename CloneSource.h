#ifndef CLONESOURCE_H
#define CLONESOURCE_H

#include <queue>

#include "Source.h"

class CloneSource : public Source
{
    friend class Source;

public:
    ~CloneSource();

    // implementation of source API
    void update (float dt) override;
    void setActive (bool on) override;
    bool playing () const override { return !paused_; }
    void play (bool on) override;
    bool playable () const  override;
    void replay () override;
    guint64 playtime () const override;
    uint texture() const override;
    bool failed() const override  { return origin_ == nullptr; }
    void accept (Visitor& v) override;

    // implementation of cloning mechanism
    CloneSource *clone(uint64_t id = 0) override;
    inline void detach() { origin_ = nullptr; }
    inline Source *origin() const { return origin_; }

    // Clone properties
    typedef enum {
        CLONE_TEXTURE = 0,
        CLONE_RENDER
    } CloneSourceProvenance;
    static const char* cloning_provenance_label[2];

    void setCloningProvenance(CloneSourceProvenance m) { provenance_ = m; }
    CloneSourceProvenance cloningProvenance() const { return provenance_; }

    void setDelay(double second);
    double delay() const { return delay_; }

    glm::ivec2 icon() const override;
    std::string info() const override;

protected:
    // only Source class can create new CloneSource via clone();
    CloneSource(Source *origin, uint64_t id = 0);

    void init() override;
    Source *origin_;

    // cloning & queue of past frames
    std::queue<FrameBuffer *> images_;
    Surface *cloningsurface_;
    FrameBuffer *to_delete_;

    // time management
    GTimer *timer_;
    std::queue<double> elapsed_;
    std::queue<guint64> timestamps_;
    double delay_;

    // control
    bool paused_;
    CloneSourceProvenance provenance_;

    // connecting line
    class DotLine *connection_;
};


#endif // CLONESOURCE_H
