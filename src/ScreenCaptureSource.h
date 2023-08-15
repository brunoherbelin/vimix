#ifndef SCREENCAPTURESOURCE_H
#define SCREENCAPTURESOURCE_H


#include <string>
#include <vector>
#include <set>
#include <map>

#include "GstToolkit.h"
#include "StreamSource.h"

#define SCREEN_CAPTURE_NAME    "Screen Capture"

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

    // StreamSource interface
    Stream *stream() const override { return stream_; }

    // specific interface
    void setWindow(const std::string &windowname);
    inline std::string window() const { return window_; }
    void reconnect();

    glm::ivec2 icon() const override;
    inline std::string info() const override;

protected:

    void unplug() { failure_ = FAIL_CRITICAL; }
    void trash()  { failure_ = FAIL_FATAL; }

private:
    std::string window_;
    std::atomic<Source::Failure> failure_;
    void unsetWindow();
};

struct ScreenCaptureHandle {

    std::string name;
    std::string pipeline;
    unsigned long id;

    GstToolkit::PipelineConfigSet configs;

    Stream *stream;
    std::list<ScreenCaptureSource *> associated_sources;

    ScreenCaptureHandle() : id(0), stream(nullptr) {}
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
