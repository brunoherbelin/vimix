#ifndef PATTERNSOURCE_H
#define PATTERNSOURCE_H

#include <vector>

#include "Stream.h"
#include "Source.h"

class Pattern : public Stream
{
public:
    static std::vector<std::string> pattern_types;

    Pattern(glm::ivec2 res);
    void open( uint pattern );

    glm::ivec2 resolution();
    inline uint type() const { return type_; }

private:
    void open( std::string description ) override;
    uint type_;
};

class PatternSource : public Source
{
public:
    PatternSource(glm::ivec2 resolution);
    ~PatternSource();

    // implementation of source API
    void update (float dt) override;
    void setActive (bool on) override;
    void render() override;
    bool failed() const override;
    uint texture() const override;
    void accept (Visitor& v) override;

    // Pattern specific interface
    inline Pattern *pattern() const { return stream_; }
    void setPattern(int id);

protected:

    void init() override;
    void replaceRenderingShader() override;

    Surface *patternsurface_;
    Pattern *stream_;

};

#endif // PATTERNSOURCE_H
