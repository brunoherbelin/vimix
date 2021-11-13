#include <atomic>
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

    std::chrono::microseconds timeNextBeat() const
    {
        auto sessionState = mLink.captureAppSessionState();
        double beat = ceil(sessionState.beatAtTime(now(), mQuantum));
        return sessionState.timeAtBeat(beat, mQuantum);
    }

    double phaseTime() const
    {
      auto sessionState = mLink.captureAppSessionState();
      return sessionState.phaseAtTime(now(), mQuantum);
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


ableton::Link link_(120.);
ableton::Engine engine_(link_);

Metronome::Metronome()
{

}

bool Metronome::init()
{
    // connect
    link_.enable(true);

    // enable sync
    link_.enableStartStopSync(true);

    // set parameters
    setTempo(Settings::application.metronome.tempo);
    setQuantum(Settings::application.metronome.quantum);

    // no reason for failure?
    return true;
}

void Metronome::terminate()
{
    // save current tempo
    Settings::application.metronome.tempo = tempo();

    // disconnect
    link_.enable(false);
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
    Settings::application.metronome.quantum = engine_.quantum();
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
    Settings::application.metronome.tempo = engine_.setTempo(t);
}

double Metronome::tempo() const
{
    return engine_.tempo();
}

std::chrono::microseconds Metronome::timeToBeat()
{
    return engine_.timeNextBeat() - engine_.now();
}

size_t Metronome::peers() const
{
    return link_.numPeers();
}
