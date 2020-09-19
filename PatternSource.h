#ifndef PATTERNSOURCE_H
#define PATTERNSOURCE_H


#include "Source.h"

#include "Stream.h"

class Pattern : public Stream
{
public:
    static const char* pattern_names[25];

    Pattern(glm::ivec2 res);
    void open( uint pattern );

private:
    void open( std::string description ) override;
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
    inline uint pattern() const { return pattern_; }
    void setPattern(int id);

    glm::ivec2 resolution();

protected:

    void init() override;
    void replaceRenderingShader() override;

    Surface *patternsurface_;
    Pattern *stream_;

    uint pattern_;
};

#endif // PATTERNSOURCE_H
