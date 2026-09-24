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

#include <deque>
#include <future>
#include <map>
#include <mutex>
#include <thread>

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

}
