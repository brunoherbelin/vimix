
#include <algorithm>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>

#include <ableton/Link.hpp>

#include "Metronome.h"


namespace ableton
{

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

    void setTempo(double tempo)
    {
      auto sessionState = mLink.captureAppSessionState();
      sessionState.setTempo(tempo, now());
      mLink.commitAppSessionState(sessionState);
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

  private:
    std::chrono::microseconds now() const
    {
      return mLink.clock().micros();
    }

    Link& mLink;
    double mQuantum;
  };


  struct State
  {
    std::atomic<bool> running;
    Link link;
    Engine engine;
    double beats, phase, tempo;
    State()  : running(true), link(120.), engine(link), beats(0.), phase(4.), tempo(120.)
    {
    }
  };

  void update(State& state)
  {
      state.link.enable(true);

      while (state.running)
      {
          const auto time = state.link.clock().micros();
          auto sessionState = state.link.captureAppSessionState();

          state.beats = sessionState.beatAtTime(time, state.engine.quantum());
          state.phase = sessionState.phaseAtTime(time, state.engine.quantum());
          state.tempo = sessionState.tempo();

          std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }

      state.link.enable(false);
  }

} // namespace ableton


ableton::State link_state_;

Metronome::Metronome()
{

}

bool Metronome::init()
{
    std::thread(ableton::update, std::ref(link_state_)).detach();

    return link_state_.running;
}

void Metronome::terminate()
{
    link_state_.running = false;
}


double Metronome::beats() const
{
    return link_state_.beats;
}

double Metronome::phase() const
{
    return link_state_.phase;
}

double Metronome::tempo() const
{
    return link_state_.tempo;
}
