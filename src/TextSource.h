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

    /*
     * ARGB integer color
     * */
    void setColor(uint c);
    inline uint color() const { return color_; }

    /* (0): None
     * (1): outline
     * (2): outline and shadow
     * */
    void setOutline(uint v);
    inline uint outline() const { return outline_; }
    void setOutlineColor(uint c);
    inline uint outlineColor() const { return outline_color_; }

    /* (0): left
     * (1): center
     * (2): right
     * (3): Absolute position
     * */
    void setHorizontalAlignment(uint h);
    inline uint horizontalAlignment() const { return halignment_; }

    void setHorizontalPadding(float x);
    inline float horizontalPadding() const { return xalignment_; }

    /* (0): bottom
     * (1): top
     * (2): center
     * (3): Absolute position
     * */
    void setVerticalAlignment(uint v);
    inline uint verticalAlignment() const { return valignment_; }

    void setVerticalPadding(float y);
    inline float verticalPadding() const { return yalignment_; }


private:
    GstElement *src_;
    GstElement *txt_;
    void execute_open() override;
    void push_data();

    std::string text_;
    std::string fontdesc_;
    uint color_;
    uint outline_;
    uint outline_color_;
    uint halignment_;
    uint valignment_;
    float xalignment_;
    float yalignment_;
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
