#ifndef SESSIONSOURCE_H
#define SESSIONSOURCE_H

#include <atomic>
#include <future>
#include "Source.h"

class SessionSource : public Source
{
public:
    SessionSource();
    ~SessionSource();

    // implementation of source API
    void update (float dt) override;
    void setActive (bool on) override;
    bool failed() const override;
    uint texture() const override;
    void accept (Visitor& v) override;

    // Session Source specific interface
    void load(const std::string &p = "");
    Session *detach();

    inline std::string path() const { return path_; }
    inline Session *session() const { return session_; }

    glm::ivec2 icon() const override { return glm::ivec2(2, 16); }

protected:

    void init() override;

    std::string path_;
    Session *session_;

    std::atomic<bool> failed_;
    std::atomic<bool> wait_for_sources_;
    std::future<Session *> sessionLoader_;
};


class RenderSource : public Source
{
public:
    RenderSource();

    // implementation of source API
    bool failed () const override;
    uint texture() const override;
    void accept (Visitor& v) override;

    inline Session *session () const { return session_; }
    inline void setSession (Session *se) { session_ = se; }

    glm::ivec2 icon() const override { return glm::ivec2(0, 2); }

protected:

    void init() override;
    Session *session_;
};


#endif // SESSIONSOURCE_H
