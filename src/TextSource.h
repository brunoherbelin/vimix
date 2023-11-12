#ifndef TEXTSOURCE_H
#define TEXTSOURCE_H

#include "StreamSource.h"
#include <gst/app/gstappsrc.h>

class TextContents : public Stream
{
public:
    TextContents();
    void open(const std::string &contents, glm::ivec2 res);

    void setText(const std::string &t);
    inline std::string text() const { return text_; }

    inline bool isSubtitle() const { return src_ != nullptr; }
    static bool SubtitleDiscoverer(const std::string &path);

    void setFontDescriptor(const std::string &fd);
    inline std::string fontDescriptor() const { return fontdesc_; }
    /* (0): left             - left
     * (1): center           - center
     * (2): right            - right
     * (4): Absolute position clamped to canvas - position
     * (5): Absolute position - absolute
     * */
    void setHalignment(uint h);
    inline uint halignment() const { return halignment_; }
    /* (0): baseline         - baseline
     * (1): bottom           - bottom
     * (2): top              - top
     * (3): Absolute position clamped to canvas - position
     * (4): center           - center
     * (5): Absolute position - absolute
     * */
    void setValignment(uint v);
    inline uint valignment() const { return valignment_; }

private:
    GstElement *src_;
    GstElement *txt_;
    void execute_open() override;
    void push_data();

    std::string text_;
    std::string fontdesc_;
    uint halignment_;
    uint valignment_;
};


class TextSource : public StreamSource
{

public:
    TextSource(uint64_t id = 0);

    // Source interface
    void accept (Visitor& v) override;
    glm::ivec2 icon() const override;
    std::string info() const override;

    // StreamSource interface
    Stream *stream() const override { return stream_; }

    // Text specific interface
    void setContents(const std::string &p, glm::ivec2 resolution);
    TextContents *contents() const;

};



#endif // TEXTSOURCE_H
