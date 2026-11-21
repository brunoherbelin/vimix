#ifndef METRONOME_H
#define METRONOME_H

#include <atomic>
#include <chrono>
#include <functional>
#include <string>

class Metronome
{
    // Private Constructor
    Metronome();
    Metronome(Metronome const& copy) = delete;
    Metronome& operator=(Metronome const& copy) = delete;

public:

    typedef enum {
        SYNC_NONE = 0,
        SYNC_BEAT,
        SYNC_PHASE
    } Synchronicity;

    static Metronome& manager ()
    {
        // The only instance
        static Metronome _instance;
        return _instance;
    }

    bool init ();
    void terminate ();

    void setEnabled (bool on);
    bool enabled () const;

    void setTempo (double t);
    double tempo () const;

    // Pause: set the tempo of the Link session to its minimum (20 BPM), and restore the
    // previous tempo when un-paused. If another value is set in the session (e.g. by a peer
    // or by the beat detection), or with setTempo(), the pause is cancelled.
    void setPaused (bool on);
    bool paused () const;

    void setQuantum (double q);
    double quantum () const;

    void setStartStopSync (bool on);
    bool startStopSync () const;
    void restart();

    // get beat and phase
    double beats () const;
    double phase () const;

    // mechanisms to delay execution to next beat
    std::chrono::microseconds timeToBeat();
    void executeAtBeat( std::function<void()> f );

    // mechanisms to delay execution to next phase
    std::chrono::microseconds timeToPhase();
    void executeAtPhase( std::function<void()> f );

    // get time to sync, in milisecond
    float timeToSync(Synchronicity sync);

    // get number of connected peers
    size_t peers () const;

    // Beat detection: an external program (vimix-beat) listens to an audio source,
    // detects the beat and sets the tempo and beat phase of the Link session.
    // It is searched next to the vimix executable, then in ../tools/vimix-beat/, then in PATH.
    // It requires audio to be accepted in the settings (and Audio to be initialized before init()),
    // and Ableton Link to be enabled (disabling Link stops the beat detection).
    bool beatDetectionAvailable ();
    void setBeatDetection (bool on);
    bool beatDetection ();                          // false if not started, or if the program has terminated
    void setBeatDevice (const std::string &name);   // audio source (restarts the detector if running; empty: what is currently played)

private:
    std::atomic<bool> paused_{false};
    double paused_tempo_ = 120.;
    int beat_pid_ = 0;
    std::string beat_binary_;
    bool beat_searched_ = false;

};

/// Example calls to executeAtBeat
///
/// With a Lamda function calling a member function of an object
///  - without parameter
///    Metronome::manager().executeAtBeat( std::bind([](MediaPlayer *p) { p->rewind(); }, mediaplayer_) );
///
///  - with parameter
///    Metronome::manager().executeAtBeat( std::bind([](MediaPlayer *p, bool o) { p->play(o); }, mediaplayer_, on) );
///


#endif // METRONOME_H
