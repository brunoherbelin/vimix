#ifndef MEDIASOURCE_H
#define MEDIASOURCE_H

#include "Source.h"

class MediaSource : public Source
{
public:
    MediaSource();
    ~MediaSource();

    // implementation of source API
    FrameBuffer *frame() const override;
    void render() override;
    void accept (Visitor& v) override;
    bool failed() const override;

    // Media specific interface
    void setPath(const std::string &p);
    std::string path() const;
    MediaPlayer *mediaplayer() const;

protected:

    void init() override;

    Surface *mediasurface_;
    std::string path_;
    MediaPlayer *mediaplayer_;
};

#endif // MEDIASOURCE_H
