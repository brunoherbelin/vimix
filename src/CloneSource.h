#ifndef CLONESOURCE_H
#define CLONESOURCE_H

#include <queue>

#include "Source.h"
#include "FrameBufferFilter.h"

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
    void reload() override;
    guint64 playtime () const override;
    uint texture() const override;
    Failure failed() const override;
    void accept (Visitor& v) override;
    void render() override;
    glm::ivec2 icon() const override;
    std::string info() const override;

    // implementation of cloning mechanism
    void detach();
    inline Source *origin() const { return origin_; }
    std::string origin_name() const;

    // Filtering
    void setFilter(FrameBufferFilter::Type T);
    inline FrameBufferFilter *filter() { return filter_; }


protected:
    // only Source class can create new CloneSource via clone();
    CloneSource(Source *origin, uint64_t id = 0);

    void init() override;
    Source *origin_;
    std::string origin_name_;

    // control
    bool paused_;

    // connecting line
    class DotLine *connection_;

    // Filter
    FrameBufferFilter *filter_;

};


#endif // CLONESOURCE_H
