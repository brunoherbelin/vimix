#ifndef PATTERNSOURCE_H
#define PATTERNSOURCE_H

#include <vector>

#include "StreamSource.h"

class Pattern : public Stream
{
public:
    static std::vector<std::string> pattern_types;

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
    PatternSource();

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
