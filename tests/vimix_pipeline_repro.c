/*
 * Standalone reproduction of vimix's actual MediaPlayer playback pipeline
 * (see MediaPlayer::execute_open(), src/MediaPlayer.cpp, USE_GST_PLAYBIN
 * branch): a "playbin" with an appsink installed as its "video-sink"
 * property (optionally wrapped in "glsinkbin" for the GL-memory zero-copy
 * path, which is vimix's default), driven by pulling samples from the
 * appsink callbacks exactly like MediaPlayer::callback_new_preroll /
 * callback_new_sample / callback_end_of_stream do, plus the same
 * GST_BUS_DROP sync bus handler vimix installs.
 *
 * Unlike vimix, this does NOT share an OpenGL context with GStreamer via
 * GST_MESSAGE_NEED_CONTEXT (Rendering::manager().global_display /
 * global_gl_context) -- glsinkbin/glupload just create their own private
 * context here. That's the main known gap versus the real app.
 *
 * Usage:
 *   gcc vimix_pipeline_repro.c -o vimix_pipeline_repro \
 *       $(pkg-config --cflags --libs gstreamer-1.0 gstreamer-app-1.0)
 *   ./vimix_pipeline_repro [--no-gl] /path/to/file.ext [width height]
 *
 * Prints periodic progress and a watchdog every 3s; after 40s with no
 * activity it reports a stall instead of hanging forever.
 */

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GMainLoop *loop;
static guint64 n = 0;
static GstClockTime last_pts = GST_CLOCK_TIME_NONE;
static gint64 t_start;

static GstFlowReturn on_preroll(GstAppSink *sink, gpointer p)
{
    GstSample *s = gst_app_sink_pull_preroll(sink);
    if (s) {
        GstBuffer *buf = gst_sample_get_buffer(s);
        printf("[preroll] pts=%" GST_TIME_FORMAT "\n", GST_TIME_ARGS(buf->pts));
        gst_sample_unref(s);
    }
    return GST_FLOW_OK;
}

static GstFlowReturn on_sample(GstAppSink *sink, gpointer p)
{
    GstSample *s = gst_app_sink_pull_sample(sink);
    if (s) {
        GstBuffer *buf = gst_sample_get_buffer(s);
        n++;
        last_pts = buf->pts;
        if (n % 25 == 0 || n < 5)
            printf("[sample #%lu] pts=%" GST_TIME_FORMAT " t=%.1fs\n", n,
                   GST_TIME_ARGS(buf->pts), (g_get_monotonic_time() - t_start) / 1e6);
        gst_sample_unref(s);
    }
    return GST_FLOW_OK;
}

static void on_eos(GstAppSink *sink, gpointer p)
{
    printf(">>> EOS callback fired: n=%lu last_pts=%" GST_TIME_FORMAT " t=%.1fs\n",
           n, GST_TIME_ARGS(last_pts), (g_get_monotonic_time() - t_start) / 1e6);
    g_main_loop_quit(loop);
}

static gboolean watchdog(gpointer p)
{
    printf("... watchdog: n=%lu last_pts=%" GST_TIME_FORMAT " t=%.1fs\n",
           n, GST_TIME_ARGS(last_pts), (g_get_monotonic_time() - t_start) / 1e6);
    return G_SOURCE_CONTINUE;
}

static gboolean give_up(gpointer p)
{
    printf("!!! TIMEOUT: n=%lu last_pts=%" GST_TIME_FORMAT " t=%.1fs -- pipeline stalled\n",
           n, GST_TIME_ARGS(last_pts), (g_get_monotonic_time() - t_start) / 1e6);
    g_main_loop_quit(loop);
    return G_SOURCE_REMOVE;
}

/* Mirrors MediaPlayer::signal_handler: drop everything but log errors.
 * Real vimix also handles GST_MESSAGE_NEED_CONTEXT here to share its own
 * GL display/context -- omitted, see file header. */
static GstBusSyncReply sync_handler(GstBus *bus, GstMessage *msg, gpointer ptr)
{
    if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
        GError *error;
        gst_message_parse_error(msg, &error, NULL);
        printf("!!! ERROR: %s\n", error->message);
        g_error_free(error);
    }
    gst_message_unref(msg);
    return GST_BUS_DROP;
}

int main(int argc, char **argv)
{
    gst_init(&argc, &argv);

    gboolean use_gl = TRUE;
    const char *path = NULL;
    int width = 720, height = 576;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-gl") == 0)
            use_gl = FALSE;
        else if (!path)
            path = argv[i];
        else if (width == 720 && height == 576 && i + 1 < argc) {
            width = atoi(argv[i]);
            height = atoi(argv[++i]);
        }
    }
    if (!path) {
        fprintf(stderr, "usage: %s [--no-gl] /path/to/file [width height]\n", argv[0]);
        return 1;
    }

    GstElement *pipeline = gst_element_factory_make("playbin", "0");
    gchar *uri = gst_filename_to_uri(path, NULL);
    g_object_set(G_OBJECT(pipeline), "uri", uri, NULL);
    g_free(uri);

    gint flags = 0x00000001; /* GST_PLAY_FLAG_VIDEO only, matches vimix (no audio/text) */
    g_object_set(G_OBJECT(pipeline), "flags", flags, NULL);

    GstElement *sink = gst_element_factory_make("appsink", "appsink");

    if (use_gl) {
        GstElement *glsinkbin = gst_element_factory_make("glsinkbin", "glsink");
        if (!glsinkbin) {
            printf("glsinkbin unavailable, falling back to --no-gl path\n");
            use_gl = FALSE;
        } else {
            GstCaps *caps = gst_caps_new_simple("video/x-raw",
                "format", G_TYPE_STRING, "RGBA",
                "width", G_TYPE_INT, width, "height", G_TYPE_INT, height, NULL);
            GstCaps *gl_caps = gst_caps_new_simple("video/x-raw",
                "format", G_TYPE_STRING, "RGBA",
                "width", G_TYPE_INT, width, "height", G_TYPE_INT, height, NULL);
            gst_caps_set_features(gl_caps, 0, gst_caps_features_new("memory:GLMemory", NULL));
            gst_caps_append(gl_caps, caps);
            gst_app_sink_set_caps(GST_APP_SINK(sink), gl_caps);
            gst_caps_unref(gl_caps);

            g_object_set(G_OBJECT(glsinkbin), "sink", sink, NULL);
            g_object_set(G_OBJECT(pipeline), "video-sink", glsinkbin, NULL);
        }
    }
    if (!use_gl) {
        GstCaps *caps = gst_caps_new_simple("video/x-raw",
            "format", G_TYPE_STRING, "RGBA",
            "width", G_TYPE_INT, width, "height", G_TYPE_INT, height, NULL);
        gst_app_sink_set_caps(GST_APP_SINK(sink), caps);
        gst_caps_unref(caps);
        g_object_set(G_OBJECT(pipeline), "video-sink", sink, NULL);
    }

    /* NB: vimix leaves max-buffers/drop unset (commented out in source) */
    gst_base_sink_set_sync(GST_BASE_SINK(sink), TRUE);

    GstBus *bus = gst_element_get_bus(pipeline);
    gst_bus_set_sync_handler(bus, sync_handler, NULL, NULL);

    GstAppSinkCallbacks cb = {0};
    cb.eos = on_eos;
    cb.new_preroll = on_preroll;
    cb.new_sample = on_sample;
    gst_app_sink_set_callbacks(GST_APP_SINK(sink), &cb, NULL, NULL);
    gst_app_sink_set_emit_signals(GST_APP_SINK(sink), FALSE);

    printf("playing '%s' (%dx%d, GL memory path: %s)\n", path, width, height, use_gl ? "on" : "off");

    t_start = g_get_monotonic_time();
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    loop = g_main_loop_new(NULL, FALSE);
    g_timeout_add_seconds(3, watchdog, NULL);
    g_timeout_add_seconds(40, give_up, NULL);
    g_main_loop_run(loop);

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(bus);
    gst_object_unref(pipeline);
    return 0;
}
