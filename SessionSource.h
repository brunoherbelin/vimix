#ifndef SESSIONSOURCE_H
#define SESSIONSOURCE_H

#include <atomic>
#include <future>
#include "Source.h"

class SessionSource : public Source
{
public:
    SessionSource(uint64_t id = 0);
    virtual ~SessionSource();

    // implementation of source API
    void update (float dt) override;
    void setActive (bool on) override;
    bool playing () const override { return !paused_; }
    void play (bool on) override { paused_ = !on; }
    bool playable () const  override { return true; }
    guint64 playtime () const override { return timer_; }
    void replay () override;
    bool failed () const override;
    uint texture () const override;

    Session *detach();
    inline Session *session() const { return session_; }

protected:

    Session *session_;
    std::atomic<bool> failed_;
    guint64 timer_;
    bool paused_;
};

class SessionFileSource : public SessionSource
{
public:
    SessionFileSource(uint64_t id = 0);

    // implementation of source API
    void accept (Visitor& v) override;
    void render() override;

    // SessionFile Source specific interface
    void load(const std::string &p = "", uint recursion = 0);

    inline std::string path() const { return path_; }
    glm::ivec2 icon() const override { return glm::ivec2(19, 6); }

protected:

    void init() override;

    std::string path_;
    bool initialized_;
    bool wait_for_sources_;
    std::future<Session *> sessionLoader_;
};

class SessionGroupSource : public SessionSource
{
public:
    SessionGroupSource(uint64_t id = 0);

    // implementation of source API
    void accept (Visitor& v) override;

    // SessionGroup Source specific interface
    inline void setResolution (glm::vec3 v) { resolution_ = v; }

    // import a source
    bool import(Source *source);

    glm::ivec2 icon() const override { return glm::ivec2(10, 6); }

protected:

    void init() override;
    glm::vec3 resolution_;
};


class RenderSource : public Source
{
public:
    RenderSource(uint64_t id = 0);

    // implementation of source API
    bool playing () const override { return true; }
    void play (bool) override {}
    bool playable () const  override { return false; }
    bool failed () const override;
    uint texture() const override;
    void accept (Visitor& v) override;

    inline Session *session () const { return session_; }
    inline void setSession (Session *se) { session_ = se; }

    glm::vec3 resolution() const;

    glm::ivec2 icon() const override { return glm::ivec2(0, 2); }

protected:

    void init() override;
    Session *session_;
};


#endif // SESSIONSOURCE_H
