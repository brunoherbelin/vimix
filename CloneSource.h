#ifndef CLONESOURCE_H
#define CLONESOURCE_H

#include <array>

#define DELAY_ARRAY_SIZE 70

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
    bool playable () const  override { return true; }
    void replay () override;
    uint texture() const override;
    bool failed() const override  { return origin_ == nullptr; }
    void accept (Visitor& v) override;

    CloneSource *clone(uint64_t id = 0) override;
    inline void detach() { origin_ = nullptr; }
    inline Source *origin() const { return origin_; }

    typedef enum {
        CLONE_TEXTURE = 0,
        CLONE_RENDER
    } CloneImageMode;

    void setImageMode(CloneImageMode m) { image_mode_ = m; }
    CloneImageMode imageMode() const { return image_mode_; }

    void setDelay(double second);
    double delay() const { return delay_; }

    glm::ivec2 icon() const override;
    std::string info() const override;

protected:
    // only Source class can create new CloneSource via clone();
    CloneSource(Source *origin, uint64_t id = 0);

    void init() override;
    Source *origin_;

    // cloning
    std::array<FrameBuffer *, DELAY_ARRAY_SIZE> stack_;
    Surface *cloningsurface_;
    size_t read_index_, write_index_;

    GTimer *timer_;
    std::array<double, DELAY_ARRAY_SIZE> timestamp_;
    double delay_;

    // control
    bool paused_;
    CloneImageMode image_mode_;
};


#endif // CLONESOURCE_H
