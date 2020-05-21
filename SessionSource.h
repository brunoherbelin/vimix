#ifndef SESSIONSOURCE_H
#define SESSIONSOURCE_H

#include <atomic>
#include "Source.h"

class SessionSource : public Source
{
public:
    SessionSource();
    ~SessionSource();

    // implementation of source API
    FrameBuffer *frame() const override;
    void render() override;
    void accept (Visitor& v) override;
    bool failed() const override;

    // Session Source specific interface
    void load(const std::string &p);
    Session *detach();

    inline std::string path() const { return path_; }
    inline Session *session() const { return session_; }

protected:

    void init() override;
    static void loadSession(const std::string& filename, SessionSource *source);

    Surface *sessionsurface_;
    std::string path_;
    Session *session_;

    std::atomic<bool> loadFailed_;
    std::atomic<bool> loadFinished_;
};
#endif // SESSIONSOURCE_H
