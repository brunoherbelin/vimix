#include <iomanip>
#include <iostream>
#include <thread>

/// Ableton Link is a technology that synchronizes musical beat, tempo,
/// and phase across multiple applications running on one or more devices.
/// Applications on devices connected to a local network discover each other
/// automatically and form a musical session in which each participant can
/// perform independently: anyone can start or stop while still staying in time.
/// Anyone can change the tempo, the others will follow.
/// Anyone can join or leave without disrupting the session.
///
/// https://ableton.github.io/link/
///
#include <ableton/Link.hpp>

#include "Settings.h"
#include "Metronome.h"
#include "Log.h"


namespace ableton
{

  /// Inspired from Dummy audio platform example
  /// https://github.com/Ableton/link/blob/master/examples/linkaudio/AudioPlatform_Dummy.hpp
  class Engine
  {
  public:
    Engine(Link& link)
      : mLink(link)
      , mQuantum(4.)
    {
    }

    void startPlaying()
    {
      auto sessionState = mLink.captureAppSessionState();
      sessionState.setIsPlayingAndRequestBeatAtTime(true, now(), 0., mQuantum);
      mLink.commitAppSessionState(sessionState);
    }

    void stopPlaying()
    {
      auto sessionState = mLink.captureAppSessionState();
      sessionState.setIsPlaying(false, now());
      mLink.commitAppSessionState(sessionState);
    }

    bool isPlaying() const
    {
      return mLink.captureAppSessionState().isPlaying();
    }

    double beatTime() const
    {
      auto sessionState = mLink.captureAppSessionState();
      return sessionState.beatAtTime(now(), mQuantum);
    }

    std::chrono::microseconds timeNextBeat(std::chrono::microseconds now) const
    {
        auto sessionState = mLink.captureAppSessionState();
        double beat = ceil(sessionState.beatAtTime(now, mQuantum));
        return sessionState.timeAtBeat(beat, mQuantum);
    }

    double phaseTime() const
    {
      auto sessionState = mLink.captureAppSessionState();
      return sessionState.phaseAtTime(now(), mQuantum);
    }

    std::chrono::microseconds timeNextPhase(std::chrono::microseconds now) const
    {
        auto sessionState = mLink.captureAppSessionState();
        double phase = ceil(sessionState.phaseAtTime(now, mQuantum));
        double beat = ceil(sessionState.beatAtTime(now, mQuantum));
        return sessionState.timeAtBeat(beat + (mQuantum-phase), mQuantum);
    }

    double tempo() const
    {
      auto sessionState = mLink.captureAppSessionState();
      return sessionState.tempo();
    }

    double setTempo(double tempo)
    {
      auto sessionState = mLink.captureAppSessionState();
      sessionState.setTempo(tempo, now());
      mLink.commitAppSessionState(sessionState);
      return sessionState.tempo();
    }

    double quantum() const
    {
      return mQuantum;
    }

    void setQuantum(double quantum)
    {
      mQuantum = quantum;
    }

    bool isStartStopSyncEnabled() const
    {
      return mLink.isStartStopSyncEnabled();
    }

    void setStartStopSyncEnabled(bool enabled)
    {
      mLink.enableStartStopSync(enabled);
    }

    std::chrono::microseconds now() const
    {
      return mLink.clock().micros();
    }

  private:
    Link& mLink;
    double mQuantum;
  };


} // namespace ableton


ableton::Link *link_ = new ableton::Link(120.);
ableton::Engine engine_(*link_);

Metronome::Metronome()
{

}

bool Metronome::init()
{
    // set parameters
    setEnabled(Settings::application.timer.link_enabled);
    setTempo(Settings::application.timer.link_tempo);
    setQuantum(Settings::application.timer.link_quantum);
    setStartStopSync(Settings::application.timer.link_start_stop_sync);

    // no reason for failure?
    return true;
}

void Metronome::terminate()
{
    // save current tempo
    Settings::application.timer.link_tempo = tempo();

    // disconnect
    link_->enable(false);
}

void Metronome::setEnabled (bool on)
{
    link_->enable(on);
    Settings::application.timer.link_enabled = link_->isEnabled();
    Log::Info("Metronome Ableton Link %s", Settings::application.timer.link_enabled ? "Enabled" : "Disabled");
}

bool Metronome::enabled () const
{
    return link_->isEnabled();
}

double Metronome::beats() const
{
    return engine_.beatTime();
}

double Metronome::phase() const
{
    return engine_.phaseTime();
}

void Metronome::setQuantum(double q)
{
    engine_.setQuantum(q);
    Settings::application.timer.link_quantum = engine_.quantum();
}

double Metronome::quantum() const
{
    return engine_.quantum();
}

void Metronome::setTempo(double t)
{
    // set the tempo to t
    // OR
    // adopt the last tempo value that have been proposed on the network
    Settings::application.timer.link_tempo = engine_.setTempo(t);
}

double Metronome::tempo() const
{
    return engine_.tempo();
}


void Metronome::setStartStopSync (bool on)
{
    engine_.setStartStopSyncEnabled(on);
    Settings::application.timer.link_start_stop_sync = engine_.isStartStopSyncEnabled();
    Log::Info("Metronome Ableton Link start & stop sync %s", Settings::application.timer.link_start_stop_sync ? "Enabled" : "Disabled");
}

bool Metronome::startStopSync () const
{
    return engine_.isStartStopSyncEnabled();
}

void Metronome::restart()
{
    engine_.startPlaying();
}

std::chrono::microseconds Metronome::timeToBeat()
{
    std::chrono::microseconds now = engine_.now();
    return engine_.timeNextBeat( now ) - now;
}

std::chrono::microseconds Metronome::timeToPhase()
{
    std::chrono::microseconds now = engine_.now();
    return engine_.timeNextPhase( now ) - now;
}

void delay(std::function<void()> f, std::chrono::microseconds us)
{
    std::this_thread::sleep_for(us);
    f();
}

void Metronome::executeAtBeat( std::function<void()> f )
{
    std::thread( delay, f, timeToBeat() ).detach();
}

void Metronome::executeAtPhase( std::function<void()> f )
{
    std::thread( delay, f, timeToPhase() ).detach();
}

float Metronome::timeToSync(Synchronicity sync)
{
    float ret = 0.f;
    if ( sync > Metronome::SYNC_BEAT ) {
        // SYNC TO PHASE
        std::chrono::duration<float, std::milli> fp_ms = Metronome::manager().timeToPhase();
        ret =  fp_ms.count();
    }
    else if ( sync > Metronome::SYNC_NONE ) {
        // SYNC TO BEAT
        std::chrono::duration<float, std::milli> fp_ms = Metronome::manager().timeToBeat();
        ret =  fp_ms.count();
    }
    // SYNC NONE
    return ret;
}

size_t Metronome::peers() const
{
    return link_->numPeers();
}
