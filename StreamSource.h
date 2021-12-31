#ifndef STREAMSOURCE_H
#define STREAMSOURCE_H


#include "Stream.h"
#include "Source.h"

/**
 * @brief The StreamSource class
 *
 * StreamSource is a virtual base class
 * (because stream() = 0)
 * based on the virtual base class Source
 * that implements the update and display
 * of a Stream object (gstreamer generic)
 *
 * StreamSource does *not* create a stream
 * in its constructor to let this for the
 * specific implementation of the subclass.
 * Therefore it cannot be instanciated and
 * it cannot give access to its stream.
 *
 */
class StreamSource: public Source
{
public:
    StreamSource(uint64_t id = 0);
    virtual ~StreamSource();

    // implementation of source API
    void update (float dt) override;
    void setActive (bool on) override;
    bool playing () const override;
    void play (bool) override;
    bool playable () const  override;
    void replay () override;
    guint64 playtime () const override;
    bool failed() const override;
    uint texture() const override;

    // pure virtual interface
    virtual Stream *stream() const = 0;

protected:
    void init() override;

    Stream *stream_;
};

/**
 * @brief The GenericStreamSource class
 *
 * Implements the StreamSource
 * with an initialization
 * using a generic description
 * of the gstreamer pipeline.
 *
 * It can be instanciated.
 */
class GenericStreamSource : public StreamSource
{
    std::string gst_description_;
    std::list<std::string> gst_elements_;

public:
    GenericStreamSource();

    // Source interface
    void accept (Visitor& v) override;

    // StreamSource interface
    Stream *stream() const override { return stream_; }

    // specific interface
    void setDescription(const std::string &desc);
    std::string description() const;
    std::list<std::string> gstElements() const;

    glm::ivec2 icon() const override;
    std::string info() const override;
};

#endif // STREAMSOURCE_H
