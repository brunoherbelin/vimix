#ifndef METRONOME_H
#define METRONOME_H

#include <chrono>
#include <functional>

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
