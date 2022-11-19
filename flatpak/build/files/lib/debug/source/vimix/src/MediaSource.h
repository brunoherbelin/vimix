#ifndef MEDIASOURCE_H
#define MEDIASOURCE_H

#include "Source.h"

class MediaPlayer;

class MediaSource : public Source
{
public:
    MediaSource(uint64_t id = 0);
    ~MediaSource();

    // implementation of source API
    void update (float dt) override;
    void setActive (bool on) override;
    bool playing () const override;
    void play (bool) override;
    bool playable () const  override;
    void replay () override;
    guint64 playtime () const override;
    void render() override;
    bool failed() const override;
    uint texture() const override;
    void accept (Visitor& v) override;

    // Media specific interface
    void setPath(const std::string &p);
    std::string path() const;
    MediaPlayer *mediaplayer() const;

    glm::ivec2 icon() const override;
    std::string info() const override;

protected:

    void init() override;

    std::string path_;
    MediaPlayer *mediaplayer_;
};

#endif // MEDIASOURCE_H
