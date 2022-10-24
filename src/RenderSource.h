#ifndef RENDERSOURCE_H
#define RENDERSOURCE_H

#include "Source.h"



class RenderSource : public Source
{
public:
    RenderSource(uint64_t id = 0);
    ~RenderSource();

    // implementation of source API
    void update (float dt) override;
    bool playing () const override { return !paused_; }
    void play (bool) override;
    void replay () override {}
    bool playable () const  override { return true; }
    guint64 playtime () const override { return runtime_; }
    bool failed () const override;
    uint texture() const override;
    void accept (Visitor& v) override;

    inline Session *session () const { return session_; }
    inline void setSession (Session *se) { session_ = se; }

    glm::vec3 resolution() const;

    typedef enum {
        RENDER_TEXTURE = 0,
        RENDER_EXCLUSIVE
    } RenderSourceProvenance;
    static const char* rendering_provenance_label[2];

    void setRenderingProvenance(RenderSourceProvenance m) { provenance_ = m; }
    RenderSourceProvenance renderingProvenance() const { return provenance_; }

    glm::ivec2 icon() const override;
    std::string info() const override;

protected:

    void init() override;
    Session *session_;
    uint64_t runtime_;

    FrameBuffer *rendered_output_;
    Surface *rendered_surface_;

    // control
    bool paused_;
    RenderSourceProvenance provenance_;
};


#endif // RENDERSOURCE_H
