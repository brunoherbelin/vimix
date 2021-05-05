#ifndef MULTIFILESOURCE_H
#define MULTIFILESOURCE_H

#include <string>
#include <list>

#include "StreamSource.h"

struct MultiFileSequence {
    std::string location;
    std::string codec;
    uint width;
    uint height;
    int min;
    int max;
    int loop;

    MultiFileSequence ();
    MultiFileSequence (const std::list<std::string> &list_files);
    bool valid () const;
    MultiFileSequence& operator = (const MultiFileSequence& b);
    bool operator != (const MultiFileSequence& b);
};

class MultiFile : public Stream
{
public:
    MultiFile ();
    void open (const MultiFileSequence &sequence, uint framerate = 30);

    void setRange(int begin, int end);
    void setLoop (int on);

protected:
    GstElement *src_ ;
};

class MultiFileSource : public StreamSource
{
public:
    MultiFileSource (uint64_t id = 0);

    // Source interface
    void accept (Visitor& v) override;

    // StreamSource interface
    Stream *stream () const override { return stream_; }
    glm::ivec2 icon () const override { return glm::ivec2(3, 9); }

    // specific interface
    void setFiles (const std::list<std::string> &list_files, uint framerate);

    void setSequence (const MultiFileSequence &sequence, uint framerate);
    inline MultiFileSequence sequence () const { return sequence_; }

    void setFramerate (uint fps);
    inline uint framerate () const { return framerate_; }

    void setLoop (bool on);
    inline bool loop () const { return sequence_.loop; }

    void setRange (glm::ivec2 range);
    glm::ivec2 range () const { return range_; }

    MultiFile *multifile () const;

private:
    MultiFileSequence sequence_;
    uint framerate_;
    glm::ivec2 range_;
};


#endif // MULTIFILESOURCE_H
