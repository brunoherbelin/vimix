/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2024 Bruno Herbelin <bruno.herbelin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
**/

#include <thread>
#include <string>

#include <glib.h>
#ifdef __linux__
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#endif

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
#include "Audio.h"


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

    // restart the beat detector if it was running (needs the audio devices: call after Audio init)
    if (Settings::application.timer.beat_detection)
        setBeatDetection(true);

    // no reason for failure?
    return true;
}

void Metronome::terminate()
{
    // stop the beat detector, but remember that it was running: the setting is saved after this
    const bool beat_was_running = beatDetection();
    setBeatDetection(false);
    Settings::application.timer.beat_detection = beat_was_running;

    // save current tempo
    Settings::application.timer.link_tempo = tempo();

    // disconnect
    link_->enable(false);
}

void Metronome::setEnabled (bool on)
{
    // the beat detection feeds the Link session: useless without it
    if (!on)
        setBeatDetection(false);

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


//
// Beat detection with the external program 'vimix-beat'
//
#define BEAT_DETECTOR_NAME "vimix-beat"

// search the program, in this order:
//  1. in the same directory as the vimix executable (installation, flatpak, snap)
//  2. in ../tools/vimix-beat/ relative to it (build directory)
//  3. in the PATH
static std::string find_beat_detector()
{
    std::string found;

#ifdef __linux__
    char exe[4096];
    ssize_t len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (len > 0) {
        exe[len] = '\0';
        gchar *dir = g_path_get_dirname(exe);
        gchar *c1 = g_build_filename(dir, BEAT_DETECTOR_NAME, nullptr);
        gchar *c2 = g_build_filename(dir, "..", "tools", BEAT_DETECTOR_NAME, BEAT_DETECTOR_NAME, nullptr);
        if (g_file_test(c1, G_FILE_TEST_IS_EXECUTABLE) && !g_file_test(c1, G_FILE_TEST_IS_DIR))
            found = c1;
        else if (g_file_test(c2, G_FILE_TEST_IS_EXECUTABLE) && !g_file_test(c2, G_FILE_TEST_IS_DIR))
            found = c2;
        g_free(c1);
        g_free(c2);
        g_free(dir);
    }
    if (found.empty()) {
        gchar *p = g_find_program_in_path(BEAT_DETECTOR_NAME);
        if (p) {
            found = p;
            g_free(p);
        }
    }
#endif

    return found;
}

#ifdef __linux__
// the beat detector must not survive vimix, even if it crashes
static void beat_child_setup(gpointer)
{
    prctl(PR_SET_PDEATHSIG, SIGTERM);
}
#endif

bool Metronome::beatDetectionAvailable ()
{
    if (!beat_searched_) {
        beat_binary_ = find_beat_detector();
        beat_searched_ = true;
        if (beat_binary_.empty())
            Log::Info("Beat detection not available (program '%s' not found).", BEAT_DETECTOR_NAME);
        else
            Log::Info("Beat detection with '%s'.", beat_binary_.c_str());
    }
    return !beat_binary_.empty();
}

bool Metronome::beatDetection ()
{
#ifdef __linux__
    if (beat_pid_ > 0) {
        int status = 0;
        pid_t r = waitpid(beat_pid_, &status, WNOHANG);
        if (r == beat_pid_ || r < 0) {
            // the program terminated by itself
            Log::Warning("Beat detection stopped (program exited).");
            beat_pid_ = 0;
            Settings::application.timer.beat_detection = false;
        }
    }
#endif
    return beat_pid_ > 0;
}

void Metronome::setBeatDetection (bool on)
{
#ifdef __linux__
    // (re)start requires to stop first
    if (beat_pid_ > 0) {
        kill(beat_pid_, SIGTERM);
        // wait for a clean exit (it leaves the Link session), at most one second
        bool done = false;
        for (int i = 0; i < 100 && !done; ++i) {
            done = waitpid(beat_pid_, nullptr, WNOHANG) != 0;
            if (!done)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!done) {
            kill(beat_pid_, SIGKILL);
            waitpid(beat_pid_, nullptr, 0);
        }
        beat_pid_ = 0;
        Log::Info("Beat detection stopped.");
    }

    Settings::application.timer.beat_detection = false;

    // the beat detector listens to audio: only if audio is accepted (and thus initialized)
    if (on && !Settings::application.accept_audio) {
        Log::Warning("Beat detection requires audio: it is disabled in the settings.");
        on = false;
    }
    // and it sets the tempo of Ableton Link: only if enabled
    if (on && !link_->isEnabled()) {
        Log::Warning("Beat detection requires Ableton Link to be enabled.");
        on = false;
    }

    if (on && beatDetectionAvailable()) {
        std::string device = Settings::application.timer.beat_device;
        // only use the device if it is plugged in
        if (!device.empty() && !Audio::manager().exists(device)) {
            Log::Info("Beat detection: audio source '%s' not found; using the default.", device.c_str());
            device = "";
        }

        gchar *argv[5];
        int n = 0;
        argv[n++] = const_cast<gchar *>(beat_binary_.c_str());
        if (!device.empty()) {
            argv[n++] = const_cast<gchar *>("--device");
            argv[n++] = const_cast<gchar *>(device.c_str());
        }
        argv[n] = nullptr;

        GPid pid = 0;
        GError *error = nullptr;
        if (g_spawn_async(nullptr, argv, nullptr, G_SPAWN_DO_NOT_REAP_CHILD,
                          beat_child_setup, nullptr, &pid, &error)) {
            beat_pid_ = pid;
            Settings::application.timer.beat_detection = true;
            Log::Info("Beat detection started%s%s.", device.empty() ? "" : " on ", device.c_str());
        }
        else {
            Log::Warning("Beat detection could not start: %s", error ? error->message : "unknown error");
            if (error)
                g_error_free(error);
        }
    }
#else
    (void) on;
#endif
}

void Metronome::setBeatDevice (const std::string &name)
{
    Settings::application.timer.beat_device = name;
    // apply the new source
    if (beatDetection())
        setBeatDetection(true);
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
