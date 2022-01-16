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
//    void setActive (bool on) override;
    bool playing () const override { return !paused_; }
    void play (bool) override;
    void replay () override;
    bool playable () const  override { return true; }
    bool failed () const override;
    uint texture() const override;
    void accept (Visitor& v) override;

    inline Session *session () const { return session_; }
    inline void setSession (Session *se) { session_ = se; }

    glm::vec3 resolution() const;

    glm::ivec2 icon() const override;
    std::string info() const override;

    typedef enum {
        RENDER_TEXTURE = 0,
        RENDER_EXCLUSIVE
    } RenderSourceMode;
    static const char* render_mode_label[2];

    void setRenderMode(RenderSourceMode m) { render_mode_ = m; }
    RenderSourceMode renderMode() const { return render_mode_; }

protected:

    void init() override;
    Session *session_;

    FrameBuffer *rendered_output_;
    Surface *rendered_surface_;

    // control
    bool paused_;
    RenderSourceMode render_mode_;
};


#endif // RENDERSOURCE_H
