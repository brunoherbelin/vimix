#ifndef SHADERSOURCE_H
#define SHADERSOURCE_H

#include "Source.h"
#include "ImageFilter.h"

class ShaderSource : public Source
{
    friend class Source;

public:
    // only Source class can create new ShaderSource via clone();
    ShaderSource(uint64_t id = 0);
    ~ShaderSource();

    // implementation of source API
    void update (float dt) override;
    void setActive (bool on) override;
    bool playing () const override { return !paused_; }
    void play (bool on) override;
    bool playable () const  override;
    void replay () override;
    void reload() override;
    guint64 playtime () const override;
    uint texture() const override;
    Failure failed() const override;
    void accept (Visitor& v) override;
    void render() override;
    glm::ivec2 icon() const override;
    std::string info() const override;
    bool texturePostProcessed() const override { return true; }

    // setup shader
    void setResolution(glm::vec3 resolution);
    void setProgram(const FilteringProgram &f);

    // access shader
    inline ImageFilter *filter() { return filter_; }

protected:

    void init() override;

    // control
    bool paused_;

    // Filter
    ImageFilter *filter_;
    FrameBuffer *background_;
};


#endif // CLONESOURCE_H
