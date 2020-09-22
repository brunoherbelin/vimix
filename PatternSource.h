#ifndef PATTERNSOURCE_H
#define PATTERNSOURCE_H

#include <vector>

#include "StreamSource.h"

class Pattern : public Stream
{
public:
    static std::vector<std::string> pattern_types;

    Pattern(glm::ivec2 res);
    void open( uint pattern );

    glm::ivec2 resolution();
    inline uint type() const { return type_; }

private:
    uint type_;
};

class PatternSource : public StreamSource
{
public:
    PatternSource(glm::ivec2 resolution);

    // Source interface
    void accept (Visitor& v) override;

    // StreamSource interface
    Stream *stream() const override { return stream_; }

    // specific interface
    Pattern *pattern() const;
    void setPattern(int id);

};

#endif // PATTERNSOURCE_H
