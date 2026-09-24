#ifndef SCREENCAPTURESOURCE_H
#define SCREENCAPTURESOURCE_H


#include <memory>
#include <string>
#include <vector>

#include "Toolkit/GstToolkit.h"
#include "Toolkit/FreedesktopToolkit.h"
#include "StreamSource.h"

#define SCREEN_CAPTURE_NAME    "Screen Capture"
#define SCREEN_CAPTURE_SELECT  "Select screen or window"

class ScreenCaptureSource : public StreamSource
{
    friend class ScreenCapture;

public:
    ScreenCaptureSource(uint64_t id = 0);
    ~ScreenCaptureSource();

    // Source interface
    Failure failed() const override;
    void accept (Visitor& v) override;
    void setActive (bool on) override;
    void update (float dt) override;
    void play (bool on) override;
    bool playable () const override;

    // StreamSource interface
    Stream *stream() const override { return stream_; }

    // specific interface
    void setWindow(const std::string &windowname = "");
    inline std::string window() const { return window_; }
    void reconnect();

    // Wayland: waiting for the user to select a screen or window
    inline bool pending() const { return request_ != nullptr; }
    // Wayland: token to restore the selection without asking the user
    inline std::string restoreToken() const { return restore_token_; }
    inline void setRestoreToken(const std::string &token) { restore_token_ = token; }

    glm::ivec2 icon() const override;
    inline std::string info() const override;

protected:

    void unplug() { failure_ = FAIL_CRITICAL; }
    void trash()  { failure_ = FAIL_FATAL; }

private:
    std::string window_;
    std::atomic<Source::Failure> failure_;
    void unsetWindow();

    std::shared_ptr<struct ScreenCaptureRequest> request_;
    std::string restore_token_;
    // pipewiresrc fails to resume from pause: never pause it
    bool pipewire_;
};

struct ScreenCaptureHandle {

    std::string name;
    std::string pipeline;
    unsigned long id;

    GstToolkit::PipelineConfigSet configs;

    Stream *stream;
    std::list<ScreenCaptureSource *> associated_sources;

    // Wayland: screen cast portal session and PipeWire remote
    std::string session;
    int fd;
    std::string restore_token;

    ScreenCaptureHandle() : id(0), stream(nullptr), fd(-1) {}
    void update(const std::string &newname);
};

class ScreenCapture
{
    friend class ScreenCaptureSource;

    ScreenCapture();
    ~ScreenCapture();
    ScreenCapture(ScreenCapture const& copy) = delete;
    ScreenCapture& operator=(ScreenCapture const& copy) = delete;

public:

    static ScreenCapture& manager()
    {
        // The only instance
        static ScreenCapture _instance;
        return _instance;
    }

    // false under Wayland, where the user selects in the dialog of the desktop
    inline bool hasWindowList () const { return !portal_; }

    int numWindow () ;
    std::string name (int index) ;
    std::string description (int index) ;
    GstToolkit::PipelineConfigSet config (int index) ;

    int  index  (const std::string &window);
    bool exists (const std::string &window);
    void reload ();

    void add   (const std::string &windowname, const std::string &pipeline, unsigned long id=0);
    void remove(const std::string &windowname, unsigned long id=0);

private:

    // Wayland: capture through the screen cast portal and pipewiresrc
    bool portal_;
    std::vector<std::string> closed_sessions_;
    std::string add (const FreedesktopToolkit::ScreenCastResult &result, const std::string &windowname);
    void release (const std::string &windowname);
    void update ();

    static void launchMonitoring(ScreenCapture *d);
    static bool initialized();
    static bool need_refresh();

    std::mutex access_;
    std::vector< ScreenCaptureHandle > handles_;

    std::condition_variable monitor_initialization_;
    bool monitor_initialized_;
    std::mutex monitor_access_;

};

#endif // SCREENCAPTURESOURCE_H
