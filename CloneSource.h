#ifndef CLONESOURCE_H
#define CLONESOURCE_H

#include "Source.h"

class CloneSource : public Source
{
    friend class Source;

public:
    ~CloneSource();

    // implementation of source API
    void setActive (bool on) override;
    bool playing () const override { return true; }
    void play (bool) override {}
    bool playable () const  override { return false; }
    void replay () override {}
    uint texture() const override;
    bool failed() const override  { return origin_ == nullptr; }
    void accept (Visitor& v) override;

    CloneSource *clone(uint64_t id = 0) override;
    inline void detach() { origin_ = nullptr; }
    inline Source *origin() const { return origin_; }

    glm::ivec2 icon() const override;
    std::string info() const override;

protected:
    // only Source class can create new CloneSource via clone();
    CloneSource(Source *origin, uint64_t id = 0);

    void init() override;
    Source *origin_;
};


#endif // CLONESOURCE_H
