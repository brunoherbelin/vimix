#ifndef PATTERNSOURCE_H
#define PATTERNSOURCE_H

#include <vector>

#include "StreamSource.h"

typedef struct pattern_ {
    std::string label;
    std::string feature;
    std::string pipeline;
    bool animated;
    bool available;
} pattern_descriptor;


class Pattern : public Stream
{
    static std::vector<pattern_descriptor> patterns_;

public:
    static pattern_descriptor get(uint type);
    static uint count();

    Pattern();
    void open( uint pattern, glm::ivec2 res);

    glm::ivec2 resolution();
    inline uint type() const { return type_; }

private:
    uint type_;
};

class PatternSource : public StreamSource
{
public:
    PatternSource(uint64_t id = 0);

    // Source interface
    void accept (Visitor& v) override;

    // StreamSource interface
    Stream *stream() const override { return stream_; }

    // specific interface
    Pattern *pattern() const;
    void setPattern(uint type, glm::ivec2 resolution);

    glm::ivec2 icon() const override { return glm::ivec2(11, 5); }

};

#endif // PATTERNSOURCE_H
