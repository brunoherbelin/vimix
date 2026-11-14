/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2023 Bruno Herbelin <bruno.herbelin@gmail.com>
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

#include <atomic>
#include <deque>
#include <future>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include <gio/gio.h>
#include <gio/gunixfdlist.h>

#include "FreedesktopToolkit.h"

#ifndef NDEBUG
#define FREEDESKTOPTOOLKIT_DEBUG
#endif

#define PORTAL_BUS_NAME   "org.freedesktop.portal.Desktop"
#define PORTAL_PATH       "/org/freedesktop/portal/desktop"
#define PORTAL_SCREENCAST "org.freedesktop.portal.ScreenCast"
#define PORTAL_REQUEST    "org.freedesktop.portal.Request"
#define PORTAL_SESSION    "org.freedesktop.portal.Session"
#define PORTAL_SETTINGS   "org.freedesktop.portal.Settings"

#define MPRIS_BUS_NAME    "org.mpris.MediaPlayer2."
#define MPRIS_PATH        "/org/mpris/MediaPlayer2"
#define MPRIS_ROOT        "org.mpris.MediaPlayer2"
#define MPRIS_PLAYER      "org.mpris.MediaPlayer2.Player"
#define MPRIS_TRACK       "/io/github/brunoherbelin/vimix/session"

namespace FreedesktopToolkit
{

// steps of a request, in order
enum Step { CREATE_SESSION, SELECT_SOURCES, START, OPEN_REMOTE };

struct Job
{
    std::string restore_token;
    ScreenCastCallback callback;
    Step step = CREATE_SESSION;
    guint response_signal = 0;
    ScreenCastResult result;
};

// state of the portal; only accessed in the portal thread, except
// what is set before 'ready' is fulfilled, and the closed callback
struct Portal
{
    GMainContext *context = nullptr;
    GDBusConnection *bus = nullptr;
    std::string sender;
    uint32_t version = 0;
    uint32_t source_types = 0;
    uint32_t cursor_modes = 0;
    bool available = false;

    std::deque<Job *> queue;
    Job *current = nullptr;
    unsigned int token_counter = 0;
    std::map<std::string, guint> sessions;

    std::mutex closed_access;
    std::function<void(const std::string &)> closed_callback;
};

static Portal portal_;
static std::once_flag portal_init_;
static std::promise<bool> portal_ready_;

static void next();
static void run_step(Job *job);

//
// Helpers
//

static std::string new_token()
{
    return "vimix" + std::to_string(++portal_.token_counter);
}

// run a function in the portal thread
static void invoke(std::function<void()> f)
{
    g_main_context_invoke(portal_.context, [](gpointer data) -> gboolean {
        auto *fn = static_cast<std::function<void()> *>(data);
        (*fn)();
        delete fn;
        return G_SOURCE_REMOVE;
    }, new std::function<void()>(f));
}

static void session_closed(GDBusConnection *, const gchar *, const gchar *object_path,
                           const gchar *, const gchar *, GVariant *, gpointer)
{
    std::string session(object_path);
#ifdef FREEDESKTOPTOOLKIT_DEBUG
    g_printerr("ScreenCast session %s closed by desktop\n", session.c_str());
#endif
    auto s = portal_.sessions.find(session);
    if (s != portal_.sessions.end()) {
        g_dbus_connection_signal_unsubscribe(portal_.bus, s->second);
        portal_.sessions.erase(s);
    }

    std::lock_guard<std::mutex> lock(portal_.closed_access);
    if (portal_.closed_callback)
        portal_.closed_callback(session);
}

static void close_session(const std::string &session)
{
    auto s = portal_.sessions.find(session);
    if (s != portal_.sessions.end()) {
        g_dbus_connection_signal_unsubscribe(portal_.bus, s->second);
        portal_.sessions.erase(s);
    }

    g_dbus_connection_call(portal_.bus, PORTAL_BUS_NAME, session.c_str(), PORTAL_SESSION,
                           "Close", nullptr, nullptr, G_DBUS_CALL_FLAGS_NONE, -1,
                           nullptr, nullptr, nullptr);
}

// end of a request (success or failure); starts the next one
static void finish(Job *job, const std::string &error = "")
{
    if (job->response_signal > 0)
        g_dbus_connection_signal_unsubscribe(portal_.bus, job->response_signal);

    if (!error.empty())
        job->result.error = error;

    if (job->result.success) {
        // keep track of the session closed by the desktop
        portal_.sessions[job->result.session] = g_dbus_connection_signal_subscribe(
            portal_.bus, PORTAL_BUS_NAME, PORTAL_SESSION, "Closed",
            job->result.session.c_str(), nullptr, G_DBUS_SIGNAL_FLAGS_NONE,
            session_closed, nullptr, nullptr);
    }
    else if (!job->result.session.empty())
        close_session(job->result.session);

#ifdef FREEDESKTOPTOOLKIT_DEBUG
    if (job->result.success)
        g_printerr("ScreenCast session %s: fd %d node %u (%d x %d) type %u\n",
                   job->result.session.c_str(), job->result.fd, job->result.node,
                   job->result.width, job->result.height, job->result.source_type);
    else
        g_printerr("ScreenCast request failed: %s\n",
                   job->result.error.empty() ? "cancelled" : job->result.error.c_str());
#endif

    if (job->callback)
        job->callback(job->result);

    delete job;
    portal_.current = nullptr;
    next();
}

//
// Request steps
//

static void on_response(GDBusConnection *, const gchar *, const gchar *, const gchar *,
                        const gchar *, GVariant *parameters, gpointer data)
{
    Job *job = static_cast<Job *>(data);

    g_dbus_connection_signal_unsubscribe(portal_.bus, job->response_signal);
    job->response_signal = 0;

    guint32 response = 2;
    GVariant *results = nullptr;
    g_variant_get(parameters, "(u@a{sv})", &response, &results);

    if (response != 0) {
        g_variant_unref(results);
        // 1 is cancelled by user: not an error
        finish(job, response == 1 ? "" : "the desktop refused the screen cast");
        return;
    }

    switch (job->step) {
    case CREATE_SESSION: {
        const gchar *session = nullptr;
        if (g_variant_lookup(results, "session_handle", "&s", &session)) {
            job->result.session = session;
            job->step = SELECT_SOURCES;
        }
        break;
    }
    case SELECT_SOURCES:
        job->step = START;
        break;
    case START: {
        // take the first stream (only one is asked for)
        GVariant *streams = g_variant_lookup_value(results, "streams", G_VARIANT_TYPE("a(ua{sv})"));
        if (streams && g_variant_n_children(streams) > 0) {
            GVariant *properties = nullptr;
            g_variant_get_child(streams, 0, "(u@a{sv})", &job->result.node, &properties);
            g_variant_lookup(properties, "size", "(ii)", &job->result.width, &job->result.height);
            g_variant_lookup(properties, "source_type", "u", &job->result.source_type);
            g_variant_unref(properties);
            job->step = OPEN_REMOTE;
        }
        if (streams)
            g_variant_unref(streams);
        const gchar *token = nullptr;
        if (g_variant_lookup(results, "restore_token", "&s", &token))
            job->result.restore_token = token;
        break;
    }
    default:
        break;
    }
    g_variant_unref(results);

    if (job->step == OPEN_REMOTE && job->result.node == 0)
        finish(job, "the desktop provided no stream");
    else if (job->step == SELECT_SOURCES && job->result.session.empty())
        finish(job, "the desktop provided no session");
    else
        run_step(job);
}

static void on_call(GObject *source, GAsyncResult *res, gpointer data)
{
    Job *job = static_cast<Job *>(data);
    GError *error = nullptr;
    GVariant *ret = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), res, &error);
    if (ret)
        g_variant_unref(ret);
    else {
        std::string msg = error->message;
        g_error_free(error);
        finish(job, msg);
    }
    // otherwise wait for the Response signal
}

static void on_open_remote(GObject *source, GAsyncResult *res, gpointer data)
{
    Job *job = static_cast<Job *>(data);
    GError *error = nullptr;
    GUnixFDList *fds = nullptr;
    GVariant *ret = g_dbus_connection_call_with_unix_fd_list_finish(G_DBUS_CONNECTION(source),
                                                                    &fds, res, &error);
    if (ret) {
        gint32 index = 0;
        g_variant_get(ret, "(h)", &index);
        job->result.fd = g_unix_fd_list_get(fds, index, &error);
        g_variant_unref(ret);
    }
    if (fds)
        g_object_unref(fds);

    if (job->result.fd < 0) {
        std::string msg = error ? error->message : "no PipeWire remote";
        if (error)
            g_error_free(error);
        finish(job, msg);
    }
    else {
        job->result.success = true;
        finish(job);
    }
}

// call a portal method creating a Request, and wait for its Response signal
static void call_request(Job *job, const char *method, GVariant *parameters, const std::string &token)
{
    std::string request = std::string(PORTAL_PATH) + "/request/" + portal_.sender + "/" + token;

    // subscribe before the call to never miss the response
    job->response_signal = g_dbus_connection_signal_subscribe(
        portal_.bus, PORTAL_BUS_NAME, PORTAL_REQUEST, "Response", request.c_str(), nullptr,
        G_DBUS_SIGNAL_FLAGS_NONE, on_response, job, nullptr);

    g_dbus_connection_call(portal_.bus, PORTAL_BUS_NAME, PORTAL_PATH, PORTAL_SCREENCAST, method,
                           parameters, G_VARIANT_TYPE("(o)"), G_DBUS_CALL_FLAGS_NONE, -1, nullptr,
                           on_call, job);
}

static void run_step(Job *job)
{
    std::string token = new_token();
    GVariantBuilder options;
    g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);

    switch (job->step) {
    case CREATE_SESSION:
        g_variant_builder_add(&options, "{sv}", "handle_token", g_variant_new_string(token.c_str()));
        g_variant_builder_add(&options, "{sv}", "session_handle_token", g_variant_new_string(new_token().c_str()));
        call_request(job, "CreateSession", g_variant_new("(a{sv})", &options), token);
        break;
    case SELECT_SOURCES: {
        uint32_t types = portal_.source_types & (SCREENCAST_MONITOR | SCREENCAST_WINDOW);
        g_variant_builder_add(&options, "{sv}", "handle_token", g_variant_new_string(token.c_str()));
        g_variant_builder_add(&options, "{sv}", "types", g_variant_new_uint32(types ? types : SCREENCAST_MONITOR));
        g_variant_builder_add(&options, "{sv}", "multiple", g_variant_new_boolean(FALSE));
        // hide the cursor (as ximagesrc show-pointer=false), if possible
        if (portal_.cursor_modes & 1)
            g_variant_builder_add(&options, "{sv}", "cursor_mode", g_variant_new_uint32(1));
        if (portal_.version >= 4) {
            // persistent until revoked; gives a restore token
            g_variant_builder_add(&options, "{sv}", "persist_mode", g_variant_new_uint32(2));
            if (!job->restore_token.empty())
                g_variant_builder_add(&options, "{sv}", "restore_token",
                                      g_variant_new_string(job->restore_token.c_str()));
        }
        call_request(job, "SelectSources",
                     g_variant_new("(oa{sv})", job->result.session.c_str(), &options), token);
        break;
    }
    case START:
        g_variant_builder_add(&options, "{sv}", "handle_token", g_variant_new_string(token.c_str()));
        call_request(job, "Start",
                     g_variant_new("(osa{sv})", job->result.session.c_str(), "", &options), token);
        break;
    case OPEN_REMOTE:
        g_dbus_connection_call_with_unix_fd_list(
            portal_.bus, PORTAL_BUS_NAME, PORTAL_PATH, PORTAL_SCREENCAST, "OpenPipeWireRemote",
            g_variant_new("(oa{sv})", job->result.session.c_str(), &options), G_VARIANT_TYPE("(h)"),
            G_DBUS_CALL_FLAGS_NONE, -1, nullptr, nullptr, on_open_remote, job);
        break;
    }
}

// start the next request in queue, if not busy
static void next()
{
    if (portal_.current != nullptr || portal_.queue.empty())
        return;

    portal_.current = portal_.queue.front();
    portal_.queue.pop_front();
    run_step(portal_.current);
}

//
// Portal thread
//

static uint32_t get_property(const char *name)
{
    uint32_t value = 0;
    GVariant *ret = g_dbus_connection_call_sync(
        portal_.bus, PORTAL_BUS_NAME, PORTAL_PATH, "org.freedesktop.DBus.Properties", "Get",
        g_variant_new("(ss)", PORTAL_SCREENCAST, name), G_VARIANT_TYPE("(v)"),
        G_DBUS_CALL_FLAGS_NONE, 2000, nullptr, nullptr);
    if (ret) {
        GVariant *v = nullptr;
        g_variant_get(ret, "(v)", &v);
        if (g_variant_is_of_type(v, G_VARIANT_TYPE_UINT32))
            value = g_variant_get_uint32(v);
        g_variant_unref(v);
        g_variant_unref(ret);
    }
    return value;
}

static void portal_thread()
{
    portal_.context = g_main_context_new();
    g_main_context_push_thread_default(portal_.context);

    GError *error = nullptr;
    portal_.bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    if (portal_.bus) {
        // sender name as used in request paths: ':1.42' becomes '1_42'
        std::string name = g_dbus_connection_get_unique_name(portal_.bus);
        name.erase(0, 1);
        for (auto &c : name)
            if (c == '.')
                c = '_';
        portal_.sender = name;

        portal_.version = get_property("version");
        portal_.source_types = get_property("AvailableSourceTypes");
        portal_.cursor_modes = get_property("AvailableCursorModes");
        portal_.available = portal_.version > 0 && portal_.source_types > 0;
    }
    else {
        g_printerr("ScreenCast: no D-Bus session (%s)\n", error->message);
        g_error_free(error);
    }

#ifdef FREEDESKTOPTOOLKIT_DEBUG
    g_printerr("ScreenCast version %u, source types %u, cursor modes %u\n",
               portal_.version, portal_.source_types, portal_.cursor_modes);
#endif

    portal_ready_.set_value(portal_.available);

    // run forever, even if not available, to handle and fail requests
    GMainLoop *loop = g_main_loop_new(portal_.context, FALSE);
    g_main_loop_run(loop);
}

static void init()
{
    std::call_once(portal_init_, []() {
        std::thread(portal_thread).detach();
        portal_ready_.get_future().wait();
    });
}

//
// Public interface
//

bool screencastAvailable()
{
    init();
    return portal_.available;
}

void screencastRequest(const std::string &restore_token, ScreenCastCallback callback)
{
    init();

    Job *job = new Job;
    job->restore_token = restore_token;
    job->callback = callback;

    invoke([job]() {
        if (!portal_.available) {
            job->result.error = "the desktop has no screen cast portal";
            if (job->callback)
                job->callback(job->result);
            delete job;
            return;
        }
        portal_.queue.push_back(job);
        next();
    });
}

void screencastClose(const std::string &session)
{
    if (session.empty())
        return;
    init();
    invoke([session]() { close_session(session); });
}

void setScreencastClosedCallback(std::function<void(const std::string &)> callback)
{
    std::lock_guard<std::mutex> lock(portal_.closed_access);
    portal_.closed_callback = callback;
}

// read a value from the settings portal (caller unrefs; nullptr if not available)
static GVariant *read_setting(const std::string &ns, const std::string &key)
{
    GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
    if (bus == nullptr)
        return nullptr;

    GVariant *value = nullptr;
    // ReadOne (portal version 2) returns the value; deprecated Read returns it
    // wrapped in another variant
    for (const char *method : { "ReadOne", "Read" }) {
        GVariant *ret = g_dbus_connection_call_sync(bus, PORTAL_BUS_NAME, PORTAL_PATH,
                                                    PORTAL_SETTINGS, method,
                                                    g_variant_new("(ss)", ns.c_str(), key.c_str()),
                                                    G_VARIANT_TYPE("(v)"), G_DBUS_CALL_FLAGS_NONE,
                                                    500, nullptr, nullptr);
        if (ret != nullptr) {
            g_variant_get(ret, "(v)", &value);
            g_variant_unref(ret);
            while (value != nullptr && g_variant_is_of_type(value, G_VARIANT_TYPE_VARIANT)) {
                GVariant *inner = g_variant_get_variant(value);
                g_variant_unref(value);
                value = inner;
            }
            break;
        }
    }

    g_object_unref(bus);
    return value;
}

std::string settingString(const std::string &ns, const std::string &key)
{
    std::string result;
    GVariant *value = read_setting(ns, key);
    if (value != nullptr) {
        if (g_variant_is_of_type(value, G_VARIANT_TYPE_STRING))
            result = g_variant_get_string(value, nullptr);
        g_variant_unref(value);
    }
    return result;
}

int settingInt(const std::string &ns, const std::string &key, int default_value)
{
    int result = default_value;
    GVariant *value = read_setting(ns, key);
    if (value != nullptr) {
        if (g_variant_is_of_type(value, G_VARIANT_TYPE_INT32))
            result = g_variant_get_int32(value);
        else if (g_variant_is_of_type(value, G_VARIANT_TYPE_UINT32))
            result = (int) g_variant_get_uint32(value);
        g_variant_unref(value);
    }
    return result;
}

//
// MPRIS (org.mpris.MediaPlayer2), served in the portal thread
//

static const char mpris_xml[] =
    "<node>"
    "  <interface name='" MPRIS_ROOT "'>"
    "    <method name='Raise'/>"
    "    <method name='Quit'/>"
    "    <property name='CanQuit' type='b' access='read'/>"
    "    <property name='CanRaise' type='b' access='read'/>"
    "    <property name='HasTrackList' type='b' access='read'/>"
    "    <property name='Identity' type='s' access='read'/>"
    "    <property name='DesktopEntry' type='s' access='read'/>"
    "    <property name='SupportedUriSchemes' type='as' access='read'/>"
    "    <property name='SupportedMimeTypes' type='as' access='read'/>"
    "  </interface>"
    "  <interface name='" MPRIS_PLAYER "'>"
    "    <method name='Next'/>"
    "    <method name='Previous'/>"
    "    <method name='Pause'/>"
    "    <method name='PlayPause'/>"
    "    <method name='Stop'/>"
    "    <method name='Play'/>"
    "    <method name='Seek'><arg direction='in' name='Offset' type='x'/></method>"
    "    <method name='SetPosition'>"
    "      <arg direction='in' name='TrackId' type='o'/>"
    "      <arg direction='in' name='Position' type='x'/>"
    "    </method>"
    "    <method name='OpenUri'><arg direction='in' name='Uri' type='s'/></method>"
    "    <signal name='Seeked'><arg name='Position' type='x'/></signal>"
    "    <property name='PlaybackStatus' type='s' access='read'/>"
    "    <property name='Rate' type='d' access='read'/>"
    "    <property name='Metadata' type='a{sv}' access='read'/>"
    "    <property name='Volume' type='d' access='read'/>"
    "    <property name='Position' type='x' access='read'/>"
    "    <property name='MinimumRate' type='d' access='read'/>"
    "    <property name='MaximumRate' type='d' access='read'/>"
    "    <property name='CanGoNext' type='b' access='read'/>"
    "    <property name='CanGoPrevious' type='b' access='read'/>"
    "    <property name='CanPlay' type='b' access='read'/>"
    "    <property name='CanPause' type='b' access='read'/>"
    "    <property name='CanSeek' type='b' access='read'/>"
    "    <property name='CanControl' type='b' access='read'/>"
    "  </interface>"
    "</node>";

// state of the MPRIS service; identity and desktop entry are set before start,
// commands and state are accessed by both threads (under lock)
struct Mpris
{
    std::string identity;
    std::string desktop_entry;
    std::atomic<bool> started{false};

    std::mutex access;
    std::deque<MprisCommand> commands;
    MprisState state;
};

static Mpris mpris_;

static void mpris_push(MprisAction action, int64_t position = 0)
{
    MprisCommand command;
    command.action = action;
    command.position = position;
    std::lock_guard<std::mutex> lock(mpris_.access);
    mpris_.commands.push_back(command);
}

static void mpris_method(GDBusConnection *, const gchar *, const gchar *, const gchar *,
                         const gchar *method_name, GVariant *parameters,
                         GDBusMethodInvocation *invocation, gpointer)
{
    const std::string method(method_name);
    if (method == "Raise")
        mpris_push(MPRIS_RAISE);
    else if (method == "PlayPause")
        mpris_push(MPRIS_PLAY_PAUSE);
    else if (method == "Play")
        mpris_push(MPRIS_PLAY);
    else if (method == "Pause" || method == "Stop")
        mpris_push(MPRIS_PAUSE);
    else if (method == "Next")
        mpris_push(MPRIS_NEXT);
    else if (method == "Previous")
        mpris_push(MPRIS_PREVIOUS);
    else if (method == "Seek") {
        gint64 offset = 0;
        g_variant_get(parameters, "(x)", &offset);
        mpris_push(MPRIS_SEEK, offset);
    }
    else if (method == "SetPosition") {
        const gchar *track = nullptr;
        gint64 position = 0;
        g_variant_get(parameters, "(&ox)", &track, &position);
        mpris_push(MPRIS_SET_POSITION, position);
    }
    else if (method == "OpenUri") {
        g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED,
                                              "Opening a URI is not supported");
        return;
    }
    // 'Quit' is ignored (CanQuit is false)

    g_dbus_method_invocation_return_value(invocation, nullptr);
}

// value of a property (floating reference), nullptr if unknown
static GVariant *mpris_property(const std::string &interface, const std::string &name)
{
    if (interface == MPRIS_ROOT) {
        if (name == "CanQuit" || name == "HasTrackList")
            return g_variant_new_boolean(FALSE);
        if (name == "CanRaise")
            return g_variant_new_boolean(TRUE);
        if (name == "Identity")
            return g_variant_new_string(mpris_.identity.c_str());
        if (name == "DesktopEntry")
            return g_variant_new_string(mpris_.desktop_entry.c_str());
        if (name == "SupportedUriSchemes" || name == "SupportedMimeTypes")
            return g_variant_new_strv(nullptr, 0);
        return nullptr;
    }

    if (name == "Rate" || name == "Volume" || name == "MinimumRate" || name == "MaximumRate")
        return g_variant_new_double(1.0);
    if (name == "CanGoNext" || name == "CanGoPrevious" || name == "CanPlay" ||
        name == "CanPause" || name == "CanControl")
        return g_variant_new_boolean(TRUE);

    std::lock_guard<std::mutex> lock(mpris_.access);
    if (name == "PlaybackStatus")
        return g_variant_new_string(mpris_.state.playing ? "Playing" : "Paused");
    if (name == "CanSeek")
        return g_variant_new_boolean(mpris_.state.seekable);
    if (name == "Position")
        return g_variant_new_int64(mpris_.state.seekable ? mpris_.state.position : 0);
    if (name == "Metadata") {
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
        g_variant_builder_add(&builder, "{sv}", "mpris:trackid", g_variant_new_object_path(MPRIS_TRACK));
        g_variant_builder_add(&builder, "{sv}", "xesam:title", g_variant_new_string(mpris_.state.title.c_str()));
        if (!mpris_.state.artist.empty()) {
            const gchar *artists[] = { mpris_.state.artist.c_str(), nullptr };
            g_variant_builder_add(&builder, "{sv}", "xesam:artist", g_variant_new_strv(artists, 1));
        }
        if (mpris_.state.seekable && mpris_.state.duration > 0)
            g_variant_builder_add(&builder, "{sv}", "mpris:length", g_variant_new_int64(mpris_.state.duration));
        return g_variant_builder_end(&builder);
    }
    return nullptr;
}

static GVariant *mpris_get_property(GDBusConnection *, const gchar *, const gchar *,
                                    const gchar *interface_name, const gchar *property_name,
                                    GError **error, gpointer)
{
    GVariant *value = mpris_property(interface_name, property_name);
    if (value == nullptr)
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_PROPERTY,
                    "Unknown property %s", property_name);
    return value;
}

static const GDBusInterfaceVTable mpris_vtable = { mpris_method, mpris_get_property, nullptr, {} };

// notify the desktop that properties of the player changed
static void mpris_emit_changed(const std::vector<std::string> &names)
{
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
    for (const auto &name : names)
        g_variant_builder_add(&builder, "{sv}", name.c_str(), mpris_property(MPRIS_PLAYER, name));
    g_dbus_connection_emit_signal(portal_.bus, nullptr, MPRIS_PATH, "org.freedesktop.DBus.Properties",
                                  "PropertiesChanged",
                                  g_variant_new("(sa{sv}as)", MPRIS_PLAYER, &builder, nullptr),
                                  nullptr);
}

void mprisStart(const std::string &identity, const std::string &desktop_entry)
{
    if (mpris_.started)
        return;
    init();
    if (portal_.bus == nullptr)
        return;

    mpris_.identity = identity;
    mpris_.desktop_entry = desktop_entry;
    mpris_.started = true;

    invoke([]() {
        GError *error = nullptr;
        GDBusNodeInfo *node = g_dbus_node_info_new_for_xml(mpris_xml, &error);
        if (node == nullptr) {
            g_printerr("MPRIS: %s\n", error->message);
            g_error_free(error);
            return;
        }
        for (int i = 0; node->interfaces[i] != nullptr; ++i) {
            if (g_dbus_connection_register_object(portal_.bus, MPRIS_PATH, node->interfaces[i],
                                                  &mpris_vtable, nullptr, nullptr, &error) == 0) {
                g_printerr("MPRIS: %s\n", error->message);
                g_clear_error(&error);
            }
        }
        g_dbus_node_info_unref(node);

        const std::string name = MPRIS_BUS_NAME + mpris_.identity;
        g_bus_own_name_on_connection(portal_.bus, name.c_str(), G_BUS_NAME_OWNER_FLAGS_DO_NOT_QUEUE,
            [](GDBusConnection *, const gchar *name, gpointer) {
#ifdef FREEDESKTOPTOOLKIT_DEBUG
                g_printerr("MPRIS: serving %s\n", name);
#else
                (void) name;
#endif
            },
            [](GDBusConnection *, const gchar *name, gpointer) {
                // e.g. owned by another instance of vimix, or not allowed in a sandbox
                g_printerr("MPRIS: could not own %s; no remote control by the desktop\n", name);
            },
            nullptr, nullptr);
    });
}

bool mprisCommand(MprisCommand &command)
{
    std::lock_guard<std::mutex> lock(mpris_.access);
    if (mpris_.commands.empty())
        return false;
    command = mpris_.commands.front();
    mpris_.commands.pop_front();
    return true;
}

void mprisSetState(const MprisState &state)
{
    if (!mpris_.started)
        return;

    std::vector<std::string> changed;
    {
        std::lock_guard<std::mutex> lock(mpris_.access);
        if (state.playing != mpris_.state.playing)
            changed.push_back("PlaybackStatus");
        if (state.seekable != mpris_.state.seekable)
            changed.push_back("CanSeek");
        if (state.title != mpris_.state.title || state.artist != mpris_.state.artist ||
            state.seekable != mpris_.state.seekable ||
            state.duration != mpris_.state.duration)
            changed.push_back("Metadata");
        mpris_.state = state;
    }

    // NB: changes of position are not notified (as specified by MPRIS)
    if (!changed.empty())
        invoke([changed]() { mpris_emit_changed(changed); });
}

void mprisSeeked(int64_t position)
{
    if (!mpris_.started)
        return;

    invoke([position]() {
        g_dbus_connection_emit_signal(portal_.bus, nullptr, MPRIS_PATH, MPRIS_PLAYER, "Seeked",
                                      g_variant_new("(x)", (gint64) position), nullptr);
    });
}

}
