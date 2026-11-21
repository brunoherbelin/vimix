/*
 * vimix-beat : audio beat detector feeding the Ableton Link session
 *
 * Captures an audio source (same device enumeration as vimix's Audio.cpp, using
 * GstDeviceMonitor on "Audio/Source"), detects the tempo with the GStreamer
 * 'bpmdetect' element (gst-plugins-bad, SoundTouch) and sets the tempo of the
 * Ableton Link session, which vimix's Metronome follows.
 *
 * Usage:
 *    vimix-beat --list
 *    vimix-beat [--device NAME|INDEX] [--min 70] [--max 180] [--dry-run] [--verbose]
 *
 * By default, the first "monitor" device (i.e. what is currently played on the
 * speakers) is captured; otherwise the first available audio source.
 *
 * GPL-3.0-or-later, same license as vimix.
 */

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <glib-unix.h>

#include <ableton/Link.hpp>

// Optional second engine: aubio onset-based tempo tracker (CMake defines HAVE_AUBIO)
#ifdef HAVE_AUBIO
#include <aubio/aubio.h>
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

struct AudioHandle {
    std::string name;
    bool is_monitor;
    GstDevice *device;
};

struct Options {
    // --- audio source
    std::string device;             // --device NAME|N : audio source, by index or by part of its name (empty: first monitor)
    bool list = false;              // --list : print the audio sources and exit

    // --- tempo range
    double min_bpm = 40.0;          // --min BPM : lowest tempo; estimates outside the range are folded by octaves
    double max_bpm = 220.0;         // --max BPM : highest tempo (the range spans at least one octave)

    // --- detection engine
    std::string engine;             // --engine gst|aubio : 'gst' is the GStreamer bpmdetect element, 'aubio'
                                    //   is the default when compiled in (HAVE_AUBIO), otherwise 'gst'
    int window = 20;                // --window SEC : analysis window; a tempo change must be seen in two
                                    //   consecutive windows to be applied (gst: the detector restarts each window)
    int hop = 128;                  // --hop N : aubio hop size in samples (smaller: finer tempo resolution; aubio only)

    // --- detection of changes (aubio only)
    int silence_ms = 500;           // --silence MS : a silence of at least this duration (and 1.5 beats) restarts
                                    //   the detection, as it means the music is changing (0: off)
    double sensitivity = 1.0;       // --sensitivity X : reactivity to changes of the beat, 0.25 to 4 (higher:
                                    //   smaller confidence drop, shorter durations and smaller tempo difference)

    // --- Ableton Link
    bool align = true;              // --no-align : (default on) align the beat phase of Link on the beats of the
                                    //   music, in addition to the tempo (aubio only)
    bool dry_run = false;           // --dry-run : detect and print, but never change the Link session

    // --- output
    bool verbose = false;           // --verbose : print raw estimates and, every second, the detection status
};

// ---------------------------------------------------------------------------
// Device enumeration (as in vimix Audio.cpp)
// ---------------------------------------------------------------------------

static bool is_monitor_device(GstDevice *device)
{
    // PulseAudio / PipeWire monitors are flagged in the device properties
    bool monitor = false;
    GstStructure *props = gst_device_get_properties(device);
    if (props) {
        const gchar *cls = gst_structure_get_string(props, "device.class");
        if (cls && g_strcmp0(cls, "monitor") == 0)
            monitor = true;
        const gchar *dev = gst_structure_get_string(props, "device.api");
        (void) dev;
        gst_structure_free(props);
    }
    // fallback as vimix: the device name ends with 'monitor'
    if (!monitor) {
        gchar *name = gst_device_get_display_name(device);
        std::string n(name ? name : "");
        g_free(name);
        monitor = n.size() >= 7 && n.compare(n.size() - 7, 7, "monitor") == 0;
        // the 'device' property of pulsesrc (e.g. alsa_output...monitor)
        if (!monitor) {
            GstElement *e = gst_device_create_element(device, nullptr);
            if (e) {
                if (g_object_class_find_property(G_OBJECT_GET_CLASS(e), "device")) {
                    gchar *d = nullptr;
                    g_object_get(e, "device", &d, nullptr);
                    std::string s(d ? d : "");
                    g_free(d);
                    monitor = s.size() >= 7 && s.compare(s.size() - 7, 7, "monitor") == 0;
                }
                gst_object_unref(e);
            }
        }
    }
    return monitor;
}

static std::vector<AudioHandle> enumerate_devices()
{
    std::vector<AudioHandle> handles;

    GstDeviceMonitor *monitor = gst_device_monitor_new();
    gst_device_monitor_set_show_all_devices(monitor, true);
    GstCaps *caps = gst_caps_new_empty_simple("audio/x-raw");
    gst_device_monitor_add_filter(monitor, "Audio/Source", caps);
    gst_caps_unref(caps);

    GList *devices = gst_device_monitor_get_devices(monitor);
    for (GList *tmp = devices; tmp; tmp = tmp->next) {
        GstDevice *device = (GstDevice *) tmp->data;
        gchar *name = gst_device_get_display_name(device);
        AudioHandle h;
        h.name = name ? name : "";
        h.is_monitor = is_monitor_device(device);
        h.device = GST_DEVICE(gst_object_ref(device));
        g_free(name);
        // skip duplicates, like vimix
        auto dup = std::find_if(handles.begin(), handles.end(),
                                [&](const AudioHandle &o) { return o.name == h.name; });
        if (dup == handles.end())
            handles.push_back(h);
        else
            gst_object_unref(h.device);
    }
    g_list_free_full(devices, gst_object_unref);
    gst_object_unref(monitor);

    return handles;
}

static int select_device(const std::vector<AudioHandle> &handles, const std::string &spec)
{
    if (handles.empty())
        return -1;

    if (!spec.empty()) {
        // by index
        char *end = nullptr;
        long i = strtol(spec.c_str(), &end, 10);
        if (end && *end == '\0' && i >= 0 && i < (long) handles.size())
            return (int) i;
        // by (partial) name
        for (size_t k = 0; k < handles.size(); ++k)
            if (handles[k].name.find(spec) != std::string::npos)
                return (int) k;
        return -1;
    }

    // default: first monitor, else first device
    for (size_t k = 0; k < handles.size(); ++k)
        if (handles[k].is_monitor)
            return (int) k;
    return 0;
}

// ---------------------------------------------------------------------------
// Tempo filtering
// ---------------------------------------------------------------------------

class TempoFilter
{
public:
    TempoFilter(double lo, double hi) : lo_(lo), hi_(hi) {}

    // fold into [lo, hi) by octaves: 40 -> 80, 240 -> 120
    double fold(double bpm) const
    {
        if (bpm <= 0.0)
            return 0.0;
        while (bpm < lo_) bpm *= 2.0;
        while (bpm >= hi_ && bpm / 2.0 >= lo_) bpm /= 2.0;
        return bpm;
    }

    // Called with the estimate of each analysis window (0 if none).
    // The first estimate is applied at once; a change then requires two
    // consecutive windows to agree. Returns true when 'out' must be applied.
    bool update(double raw, double &out)
    {
        double bpm = fold(raw);
        if (bpm <= 0.0)
            return false;
        bpm = std::round(bpm * 10.0) / 10.0;

        if (current_ <= 0.0 || (pending_ > 0.0 && std::fabs(bpm - pending_) < 1.5 && std::fabs(bpm - current_) >= 0.5)) {
            current_ = out = bpm;
            pending_ = 0.0;
            return true;
        }
        pending_ = (std::fabs(bpm - current_) < 0.5) ? 0.0 : bpm;
        return false;
    }

    double current() const { return current_; }

    // apply an estimate without waiting for a confirmation
    double force(double raw)
    {
        double bpm = fold(raw);
        current_ = std::round(bpm * 10.0) / 10.0;
        pending_ = 0.0;
        return current_;
    }

    // forget the current tempo: the next estimate is applied without confirmation
    void reset()
    {
        current_ = 0.0;
        pending_ = 0.0;
    }

private:
    double lo_, hi_;
    double current_ = 0.0;
    double pending_ = 0.0;
};

// ---------------------------------------------------------------------------
// Application
// ---------------------------------------------------------------------------

struct App {
    Options opt;
    GMainLoop *loop = nullptr;
    ableton::Link *link = nullptr;
    TempoFilter *filter = nullptr;
    GstElement *pipeline = nullptr;
    double latest = 0.0;   // last raw estimate of the current window
    bool first = true;     // no tempo applied yet
    bool use_aubio = false;
#ifdef HAVE_AUBIO
    aubio_tempo_t *tempo = nullptr;
    fvec_t *in = nullptr;
    fvec_t *out = nullptr;
    uint_t hop = 512;
    uint_t filled = 0;
    std::atomic<double> aubio_bpm{0.0};
    std::atomic<double> latency_s{0.0};   // capture pipeline latency
    uint64_t total_samples = 0;           // processed by aubio (streaming thread only)
    std::mutex beat_mutex;
    std::vector<std::chrono::microseconds> beats;  // beat times in the Link clock
    std::deque<double> phase_errors;               // in beats, in [-0.5, 0.5]
    bool latency_known = false;
    // silence detection (streaming thread)
    double silent_s = 0.0;
    bool silence_fired = false;
    std::atomic<bool> silence_event{false};
    // restart of the detection (main thread)
    std::atomic<double> confidence{0.0};   // aubio confidence in the current tempo
    std::atomic<double> level{0.0};        // rms of the last chunk
    std::atomic<bool> loud{false};         // there is signal (not silence)
    std::atomic<bool> reset_request{false};  // ask the streaming thread to recreate the tracker
    // triggers watched by the main thread
    int poll_count = 0;
    double conf_ref = 0.0;   // recent peak of the confidence
    double lost_since = 0.0;   // beat lost: since when (s)
    bool lost_fired = false;
    double dis_since = 0.0;    // sustained disagreement with the applied tempo: since when (s)
    double dis_min = 0.0, dis_max = 0.0;
    int64_t settle_until = 0;   // monotonic time (us) before which estimates are ignored after a restart
#endif
    guint window_id = 0;
};

static void set_link_tempo(App *app, double bpm)
{
    bpm = std::min(std::max(bpm, 20.0), 999.0);
    auto state = app->link->captureAppSessionState();
    state.setTempo(bpm, app->link->clock().micros());
    app->link->commitAppSessionState(state);
#ifdef HAVE_AUBIO
    app->phase_errors.clear();   // measured against the previous tempo
#endif
}

static void commit_tempo(App *app, double out, const char *why = "")
{
    g_print("tempo: %.1f bpm%s%s (peers: %zu)\n", out, why,
            app->opt.dry_run ? " [dry-run]" : "", app->link->numPeers());
    fflush(stdout);
    if (!app->opt.dry_run)
        set_link_tempo(app, out);
    app->first = false;
}

static void apply_estimate(App *app)
{
    double out = 0.0;
    if (app->latest > 0.0 && app->filter->update(app->latest, out))
        commit_tempo(app, out);
}

static gboolean on_bus_message(GstBus *, GstMessage *msg, gpointer data)
{
    App *app = static_cast<App *>(data);

    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_TAG: {
        GstTagList *tags = nullptr;
        gst_message_parse_tag(msg, &tags);
        gdouble bpm = 0.0;
        if (tags && gst_tag_list_get_double(tags, GST_TAG_BEATS_PER_MINUTE, &bpm)) {
            if (app->opt.verbose)
                g_print("raw bpm: %.2f\n", bpm);
            app->latest = bpm;
            // do not wait for the end of the window to get started
            if (app->first)
                apply_estimate(app);
        }
        if (tags)
            gst_tag_list_unref(tags);
        break;
    }
    case GST_MESSAGE_ERROR: {
        GError *err = nullptr;
        gchar *dbg = nullptr;
        gst_message_parse_error(msg, &err, &dbg);
        g_printerr("Error: %s\n", err->message);
        g_error_free(err);
        g_free(dbg);
        g_main_loop_quit(app->loop);
        break;
    }
    default:
        break;
    }
    return TRUE;
}

static gboolean on_window(gpointer data);

// (re)start the countdown of the analysis window
static void restart_window(App *app)
{
    if (app->window_id)
        g_source_remove(app->window_id);
    app->window_id = g_timeout_add_seconds(app->opt.window, on_window, app);
}

// end of an analysis window: evaluate, then restart the detector (it keeps
// the whole history otherwise, and would be slow to follow tempo changes)
static gboolean on_window(gpointer data)
{
    App *app = static_cast<App *>(data);
    apply_estimate(app);
    if (app->use_aubio)
        return G_SOURCE_CONTINUE;   // aubio adapts by itself, nothing to restart
    app->latest = 0.0;
    gst_element_set_state(app->pipeline, GST_STATE_NULL);
    gst_element_set_state(app->pipeline, GST_STATE_PLAYING);
    return G_SOURCE_CONTINUE;
}

#ifdef HAVE_AUBIO
// appsink callback (streaming thread): feed aubio with mono float samples
static GstFlowReturn on_new_sample(GstAppSink *sink, gpointer data)
{
    App *app = static_cast<App *>(data);
    GstSample *sample = gst_app_sink_pull_sample(sink);
    if (!sample)
        return GST_FLOW_ERROR;

    GstBuffer *buffer = gst_sample_get_buffer(sample);
    GstMapInfo map;
    if (buffer && gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        const float *samples = reinterpret_cast<const float *>(map.data);
        size_t n = map.size / sizeof(float);
        // the end of this chunk is 'now' (minus capture latency) on the Link clock
        const auto now = app->link->clock().micros();
        const double chunk_end_s = double(app->total_samples + n) / 44100.0;

        // level of this chunk (about -55 dBFS or less is silence)
        double rms = 0.0;
        if (n > 0) {
            double sum = 0.0;
            for (size_t i = 0; i < n; ++i)
                sum += double(samples[i]) * double(samples[i]);
            rms = std::sqrt(sum / double(n));
            app->level.store(rms);
            // signal is present unless it was quiet for the last second (gaps between kicks are not silence)
            if (rms >= 0.0018)
                app->silent_s = 0.0;
            else
                app->silent_s += double(n) / 44100.0;
            app->loud.store(app->silent_s < 1.0);
        }

        // a restart was requested by the main thread (beat lost)
        if (app->reset_request.exchange(false)) {
            del_aubio_tempo(app->tempo);
            app->tempo = new_aubio_tempo("default", 1024, app->hop, 44100);
            app->filled = 0;
            app->aubio_bpm.store(0.0);
            app->confidence.store(0.0);
            std::lock_guard<std::mutex> lock(app->beat_mutex);
            app->beats.clear();
        }

        // silence detection
        if (app->opt.silence_ms > 0 && n > 0) {
            if (rms < 0.0018) {
                // long enough: at least 1.5 beats, so that gaps between kicks are not silences
                double bpm = app->aubio_bpm.load();
                double min_s = std::max(app->opt.silence_ms / 1000.0, bpm > 0.0 ? 1.5 * 60.0 / bpm : 0.0);
                if (!app->silence_fired && app->silent_s >= min_s) {
                    app->silence_fired = true;
                    // forget what was learned: new tempo detector, no pending beats
                    del_aubio_tempo(app->tempo);
                    app->tempo = new_aubio_tempo("default", 1024, app->hop, 44100);
                    app->filled = 0;
                    app->aubio_bpm.store(0.0);
                    app->confidence.store(0.0);
                    {
                        std::lock_guard<std::mutex> lock(app->beat_mutex);
                        app->beats.clear();
                    }
                    app->silence_event.store(true);
                }
            } else {
                app->silence_fired = false;
            }
        }
        for (size_t i = 0; i < n; ++i) {
            app->in->data[app->filled++] = samples[i];
            ++app->total_samples;
            if (app->filled == app->hop) {
                app->filled = 0;
                aubio_tempo_do(app->tempo, app->in, app->out);
                app->confidence.store(double(aubio_tempo_get_confidence(app->tempo)));
                if (app->out->data[0] != 0.f) {   // a beat was found
                    double bpm = aubio_tempo_get_bpm(app->tempo);
                    if (bpm > 0.0)
                        app->aubio_bpm.store(bpm);
                    // when did this beat happen? (in stream time, compensated for detection delay)
                    double age = chunk_end_s - double(aubio_tempo_get_last_s(app->tempo)) + app->latency_s.load();
                    if (age >= 0.0 && age < 2.0) {
                        std::lock_guard<std::mutex> lock(app->beat_mutex);
                        if (app->beats.size() < 64)
                            app->beats.push_back(now - std::chrono::microseconds(int64_t(age * 1e6)));
                    }
                }
            }
        }
        gst_buffer_unmap(buffer, &map);
    }
    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

// aubio's confidence has no absolute scale (about 1.8 on a synthetic kick, 0.17 on real
// music), so it is compared to its recent peak: aubio is not following a beat when the
// confidence falls below a fraction of that peak, or under a small absolute floor.
static const double kConfidenceFloor = 0.05;
static const double kConfidenceRatio = 0.4;

static bool following_beat(const App *app)
{
    const double c = app->confidence.load();
    return c >= kConfidenceFloor && c >= std::min(0.95, kConfidenceRatio * app->opt.sensitivity) * app->conf_ref;
}

// Align the beat phase of Link on the beats of the music.
//
// requestBeatAtTime() cannot do this: with other peers (vimix) it only schedules
// a quantized launch and never moves the running beat grid. forceBeatAtTime()
// is the documented way to bridge an external clock into a session, so it is used
// sparingly: the error is measured on several beats (circular mean, so that it is
// robust to outliers), only a correction above 25 ms is applied, and only three quarters of it
// at once. The correction is under a quarter of a beat, so the position in the
// bar (set by vimix with the start/stop sync) is never changed.
static gboolean on_align(gpointer data)
{
    App *app = static_cast<App *>(data);
    const double quantum = 4.0;

    // capture pipeline latency, once it is known
    if (!app->latency_known) {
        GstQuery *q = gst_query_new_latency();
        if (gst_element_query(app->pipeline, q)) {
            gboolean live = FALSE;
            GstClockTime min = 0, max = 0;
            gst_query_parse_latency(q, &live, &min, &max);
            if (live) {
                app->latency_s.store(double(min) / 1e9);
                app->latency_known = true;
                if (app->opt.verbose)
                    g_print("capture latency: %.0f ms\n", double(min) / 1e6);
            }
        }
        gst_query_unref(q);
    }

    std::vector<std::chrono::microseconds> beats;
    {
        std::lock_guard<std::mutex> lock(app->beat_mutex);
        beats.swap(app->beats);
    }
    if (!app->opt.align || app->first || beats.empty() || !following_beat(app))
        return G_SOURCE_CONTINUE;

    for (auto t : beats) {
        auto state = app->link->captureAppSessionState();
        double b = state.beatAtTime(t, quantum);
        app->phase_errors.push_back(b - std::round(b));   // > 0: Link is ahead of the music
        if (app->phase_errors.size() > 12)
            app->phase_errors.pop_front();
    }
    if (app->phase_errors.size() < 8)
        return G_SOURCE_CONTINUE;

    double S = 0.0, C = 0.0;
    for (double e : app->phase_errors) {
        S += std::sin(2.0 * M_PI * e);
        C += std::cos(2.0 * M_PI * e);
    }
    const double N = double(app->phase_errors.size());
    const double confidence = std::hypot(S, C) / N;   // 1: all beats agree
    const double err = std::atan2(S, C) / (2.0 * M_PI);   // beats
    if (confidence < 0.8)
        return G_SOURCE_CONTINUE;   // beats are not regular enough to be trusted

    auto state = app->link->captureAppSessionState();
    const double err_ms = err * 60000.0 / state.tempo();
    if (app->opt.verbose)
        g_print("beat phase error: %+.0f ms (confidence %.2f)\n", err_ms, confidence);
    if (std::fabs(err_ms) < 25.0)
        return G_SOURCE_CONTINUE;

    // the music beat happened when Link was at beat (k + err): move Link back by err
    const auto now = app->link->clock().micros();
    const double beat_now = state.beatAtTime(now, quantum);
    g_print("beat alignment: %+.0f ms%s\n", -0.75 * err_ms, app->opt.dry_run ? " [dry-run]" : "");
    if (!app->opt.dry_run) {
        state.forceBeatAtTime(beat_now - 0.75 * err, now, quantum);
        app->link->commitAppSessionState(state);
    }
    app->phase_errors.clear();   // measured with the previous grid
    return G_SOURCE_CONTINUE;
}

// quick poll of the aubio estimate: get started without waiting for a full window
// forget the current tempo and learn it again: the first new estimate is applied
// without confirmation, once the beat is established
static void restart_detection(App *app, const char *reason, bool reset_tracker)
{
    if (!app->first)
        g_print("%s: restarting the detection\n", reason);
    if (reset_tracker)
        app->reset_request.store(true);
    app->filter->reset();
    app->phase_errors.clear();
    app->latest = 0.0;
    app->first = true;
    app->settle_until = g_get_monotonic_time() + 4 * G_USEC_PER_SEC;
    if (!reset_tracker)
        app->conf_ref = 0.0;   // after a silence: a new track, with possibly another confidence scale
    app->lost_since = 0.0;
    app->dis_since = 0.0;
    restart_window(app);
}

// Moments when the tempo may change, watched every 250 ms (aubio engine):
//  - silence (flagged by the streaming thread)
//  - beat lost: signal but no tempo confidence for a while (breakdown, transition)
//  - sustained disagreement: the estimate stays away from the applied tempo
static const double kLostSeconds = 4.0;
static const double kDisagreement = 0.03;    // relative difference
static const double kDisagreementSeconds = 7.0;

static gboolean on_poll(gpointer data)
{
    App *app = static_cast<App *>(data);
    const double now_s = double(g_get_monotonic_time()) / 1e6;
    const double sens = app->opt.sensitivity;
    const double lost_seconds = kLostSeconds / sens;
    const double disagreement = std::max(0.01, kDisagreement / sens);
    const double disagreement_seconds = kDisagreementSeconds / sens;

    // recent peak of the confidence (half-life of about 60 s)
    app->conf_ref = std::max(app->confidence.load(), app->conf_ref * 0.9885);

    if (app->silence_event.exchange(false)) {
        restart_detection(app, "silence", false);   // tracker already recreated
        return G_SOURCE_CONTINUE;
    }
    if (app->first && g_get_monotonic_time() < app->settle_until)
        return G_SOURCE_CONTINUE;

    double bpm = app->aubio_bpm.load();
    const bool following = following_beat(app);

    // 1. beat lost
    if (app->opt.verbose && ++app->poll_count % 4 == 0)
        g_print("status: level %.0f dBFS, confidence %.2f (recent peak %.2f, %s), aubio bpm %.1f%s\n",
                20.0 * std::log10(std::max(app->level.load(), 1e-6)), app->confidence.load(),
                app->conf_ref, following ? "following" : "not following", bpm, app->first ? ", waiting for a first estimate" : "");
    if (!app->first && bpm > 0.0 && app->loud.load() && !following) {
        if (app->lost_since == 0.0)
            app->lost_since = now_s;
        else if (now_s - app->lost_since >= lost_seconds) {
            restart_detection(app, "beat lost", true);
            return G_SOURCE_CONTINUE;
        }
    } else {
        app->lost_since = 0.0;
    }
    if (!following)
        app->dis_since = 0.0;

    if (bpm > 0.0 && following) {
        app->latest = bpm;
        if (app->opt.verbose)
            g_print("raw bpm: %.2f\n", bpm);
        if (app->first) {
            apply_estimate(app);
        } else {
            // 2. sustained disagreement with the applied tempo
            const double cur = app->filter->current();
            const double f = app->filter->fold(bpm);
            if (cur > 0.0 && std::fabs(f - cur) / cur > disagreement) {
                if (app->dis_since == 0.0) {
                    app->dis_since = now_s;
                    app->dis_min = app->dis_max = f;
                } else {
                    app->dis_min = std::min(app->dis_min, f);
                    app->dis_max = std::max(app->dis_max, f);
                    if ((app->dis_max - app->dis_min) / cur > disagreement) {
                        // the estimates do not agree with each other either: start over
                        app->dis_since = now_s;
                        app->dis_min = app->dis_max = f;
                    } else if (now_s - app->dis_since >= disagreement_seconds) {
                        commit_tempo(app, app->filter->force(f), " (sustained change)");
                        app->phase_errors.clear();
                        app->dis_since = 0.0;
                        restart_window(app);
                    }
                }
            } else {
                app->dis_since = 0.0;
            }
        }
    }
    return G_SOURCE_CONTINUE;
}
#endif

static gboolean on_signal(gpointer data)
{
    g_main_loop_quit(static_cast<App *>(data)->loop);
    return G_SOURCE_REMOVE;
}

static void usage()
{
    g_print("Usage: vimix-beat [options]\n"
            "  --list              list audio sources and exit\n"
            "  --device NAME|N     audio source (name substring or index); default: first monitor\n"
            "  --min BPM           lowest tempo (default 40); estimates are folded by octaves\n"
            "  --max BPM           highest tempo (default 220)\n"
            "  --engine NAME       tempo engine: 'gst' (bpmdetect element)"
#ifdef HAVE_AUBIO
            " or 'aubio' (default)"
#endif
            "\n"
            "  --hop N             aubio hop size in samples (default 128, smaller: finer tempo resolution)\n"
            "  --window SEC        analysis window in seconds (default 20)\n"
            "  --sensitivity X     reactivity to changes of the beat, 0.25 to 4 (default 1; higher: reacts to smaller\n"
            "                      changes and faster, lower: more stable; aubio)\n"
            "  --silence MS        a silence of at least MS restarts the detection (default 500, 0: off; aubio)\n"
            "  --no-align          only set the tempo (aubio: by default, also align the beat phase)\n"
            "  --dry-run           detect and print, do not touch Link\n"
            "  --verbose           print raw estimates and the detection status\n");
}

int main(int argc, char **argv)
{
    App app;
    Options &opt = app.opt;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> const char * { return (i + 1 < argc) ? argv[++i] : ""; };
        if (a == "--list") opt.list = true;
        else if (a == "--device") opt.device = next();
        else if (a == "--min") opt.min_bpm = atof(next());
        else if (a == "--max") opt.max_bpm = atof(next());
        else if (a == "--engine") opt.engine = next();
        else if (a == "--no-align") opt.align = false;
        else if (a == "--silence") opt.silence_ms = atoi(next());
        else if (a == "--sensitivity") opt.sensitivity = std::min(std::max(atof(next()), 0.25), 4.0);
        else if (a == "--hop") opt.hop = atoi(next());
        else if (a == "--window") opt.window = atoi(next());
        else if (a == "--dry-run") opt.dry_run = true;
        else if (a == "--verbose") opt.verbose = true;
        else { usage(); return (a == "-h" || a == "--help") ? 0 : 1; }
    }
    if (opt.min_bpm <= 0 || opt.max_bpm < 2 * opt.min_bpm - 1e-9) {
        // the folding range needs to span at least an octave
        opt.max_bpm = std::max(opt.max_bpm, 2.0 * opt.min_bpm);
    }

    gst_init(&argc, &argv);

#ifdef HAVE_AUBIO
    const char *default_engine = "aubio";
#else
    const char *default_engine = "gst";
#endif
    if (opt.engine.empty())
        opt.engine = default_engine;
    app.use_aubio = (opt.engine == "aubio");
    if (opt.engine != "gst" && opt.engine != "aubio") {
        g_printerr("Unknown engine '%s'\n", opt.engine.c_str());
        return 1;
    }
#ifndef HAVE_AUBIO
    if (app.use_aubio) {
        g_printerr("This build has no aubio support.\n");
        return 1;
    }
#endif
    if (opt.window < 1)
        opt.window = 20;

    std::vector<AudioHandle> handles = enumerate_devices();

    if (opt.list) {
        for (size_t k = 0; k < handles.size(); ++k)
            g_print("%zu: %s%s\n", k, handles[k].name.c_str(), handles[k].is_monitor ? "  [monitor]" : "");
        return 0;
    }

    if (!app.use_aubio && !gst_element_factory_find("bpmdetect")) {
        g_printerr("GStreamer element 'bpmdetect' not found (package gst-plugins-bad / soundtouch).\n");
        return 2;
    }

    int sel = select_device(handles, opt.device);
    if (sel < 0) {
        g_printerr("No audio source found%s%s.\n", opt.device.empty() ? "" : " matching ", opt.device.c_str());
        return 2;
    }
    g_print("Capturing '%s' (engine: %s)\n", handles[sel].name.c_str(), opt.engine.c_str());

    GstElement *src = gst_device_create_element(handles[sel].device, nullptr);
    GstElement *pipeline = gst_pipeline_new("beat");
    GstElement *conv = gst_element_factory_make("audioconvert", nullptr);
    GstElement *resample = gst_element_factory_make("audioresample", nullptr);
    GstElement *filter = gst_element_factory_make("capsfilter", nullptr);
    GstElement *bpm = app.use_aubio ? nullptr : gst_element_factory_make("bpmdetect", nullptr);
    GstElement *sink = gst_element_factory_make(app.use_aubio ? "appsink" : "fakesink", nullptr);
    if (!src || !conv || !resample || !filter || (!app.use_aubio && !bpm) || !sink) {
        g_printerr("Could not create the GStreamer pipeline.\n");
        return 2;
    }
    GstCaps *caps = gst_caps_from_string("audio/x-raw,format=F32LE,channels=1,rate=44100");
    g_object_set(filter, "caps", caps, nullptr);
    gst_caps_unref(caps);
    g_object_set(sink, "sync", FALSE, "async", FALSE, nullptr);

    gst_bin_add_many(GST_BIN(pipeline), src, conv, resample, filter, sink, nullptr);
    if (bpm)
        gst_bin_add(GST_BIN(pipeline), bpm);
    gboolean linked = bpm ? gst_element_link_many(src, conv, resample, filter, bpm, sink, nullptr)
                          : gst_element_link_many(src, conv, resample, filter, sink, nullptr);
    if (!linked) {
        g_printerr("Could not link the GStreamer pipeline.\n");
        return 2;
    }

#ifdef HAVE_AUBIO
    if (app.use_aubio) {
        const uint_t win = 1024, rate = 44100;
        app.hop = std::min(std::max(opt.hop, 64), 1024);
        app.tempo = new_aubio_tempo("default", win, app.hop, rate);
        app.in = new_fvec(app.hop);
        app.out = new_fvec(2);
        if (!app.tempo || !app.in || !app.out) {
            g_printerr("Could not initialize aubio.\n");
            return 2;
        }
        GstAppSinkCallbacks cb = {};
        cb.new_sample = on_new_sample;
        gst_app_sink_set_callbacks(GST_APP_SINK(sink), &cb, &app, nullptr);
        g_object_set(sink, "sync", FALSE, "max-buffers", 8, "drop", TRUE, nullptr);
        g_timeout_add(250, on_poll, &app);
        g_timeout_add(100, on_align, &app);
    }
#endif

    ableton::Link link(120.0);
    link.enable(true);
    TempoFilter tf(opt.min_bpm, opt.max_bpm);
    app.link = &link;
    app.filter = &tf;
    app.loop = g_main_loop_new(nullptr, FALSE);
    app.pipeline = pipeline;
    restart_window(&app);

    GstBus *bus = gst_element_get_bus(pipeline);
    gst_bus_add_watch(bus, on_bus_message, &app);
    gst_object_unref(bus);
    g_unix_signal_add(SIGINT, on_signal, &app);
    g_unix_signal_add(SIGTERM, on_signal, &app);

    if (gst_element_set_state(pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Could not start capturing.\n");
        return 2;
    }

    g_main_loop_run(app.loop);

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    link.enable(false);
#ifdef HAVE_AUBIO
    if (app.use_aubio) {
        del_aubio_tempo(app.tempo);
        del_fvec(app.in);
        del_fvec(app.out);
        aubio_cleanup();
    }
#endif
    for (auto &h : handles)
        gst_object_unref(h.device);
    g_main_loop_unref(app.loop);
    return 0;
}
