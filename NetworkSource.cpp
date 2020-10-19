#include <algorithm>
#include <sstream>
#include <glm/gtc/matrix_transform.hpp>

#include <gst/pbutils/pbutils.h>
#include <gst/gst.h>

#include "SystemToolkit.h"
#include "defines.h"
#include "Stream.h"
#include "Decorations.h"
#include "Visitor.h"
#include "Log.h"

#include "NetworkSource.h"

#ifndef NDEBUG
#define NETWORK_DEBUG
#endif

NetworkStream::NetworkStream(): Stream(), protocol_(NetworkToolkit::DEFAULT), host_("127.0.0.1"), port_(5000)
{

}

glm::ivec2 NetworkStream::resolution()
{
    return glm::ivec2( width_, height_);
}


void NetworkStream::open(NetworkToolkit::Protocol protocol, const std::string &host, uint port )
{
    protocol_ = protocol;
    host_ = host;
    port_ = port;

    int w = 1920;
    int h = 1080;

    std::ostringstream pipeline;
    if (protocol_ == NetworkToolkit::TCP_JPEG || protocol_ == NetworkToolkit::TCP_H264) {

        pipeline << "tcpclientsrc name=src timeout=1 host=" << host_ << " port=" << port_;
    }
    else if (protocol_ == NetworkToolkit::SHM_RAW) {

        std::string path = SystemToolkit::full_filename(SystemToolkit::settings_path(), "shm_socket");
        pipeline << "shmsrc name=src is-live=true socket-path=" << path;
        // TODO SUPPORT multiple sockets shared memory
    }

    pipeline << NetworkToolkit::protocol_receive_pipeline[protocol_];
    pipeline << " ! videoconvert";

//    if ( ping(&w, &h) )
        // (private) open stream
        Stream::open(pipeline.str(), w, h);
//    else {
//        Log::Notify("Failed to connect to %s:%d", host.c_str(), port);
//        failed_ = true;
//    }




}

bool NetworkStream::ping(int *w, int *h)
{
    bool ret = false;

    std::ostringstream pipeline_desc;
    if (protocol_ == NetworkToolkit::TCP_JPEG || protocol_ == NetworkToolkit::TCP_H264) {

        pipeline_desc << "tcpclientsrc is-live=true host=" << host_ << " port=" << port_;
    }
    else if (protocol_ == NetworkToolkit::SHM_RAW) {

        std::string path = SystemToolkit::full_filename(SystemToolkit::settings_path(), "shm_socket");
        pipeline_desc << "shmsrc is-live=true socket-path=" << path;
    }

    pipeline_desc << " ! appsink name=sink";

    GError *error = NULL;
    GstElement *pipeline = gst_parse_launch (pipeline_desc.str().c_str(), &error);
    if (error != NULL) return false;

    GstElement *sink = gst_bin_get_by_name (GST_BIN (pipeline), "sink");
    if (sink)   {

        GstAppSinkCallbacks callbacks;
        callbacks.new_preroll = callback_sample;
        callbacks.new_sample = callback_sample;
        callbacks.eos = NULL;
        gst_app_sink_set_callbacks (GST_APP_SINK(sink), &callbacks, this, NULL);
        gst_app_sink_set_emit_signals (GST_APP_SINK(sink), false);

        GstStateChangeReturn status = gst_element_set_state (pipeline, GST_STATE_PLAYING);
        if (status == GST_STATE_CHANGE_FAILURE) {
            ret = false;
        }
        GstState state;
        gst_element_get_state (pipeline, &state, NULL, GST_CLOCK_TIME_NONE);

        gst_element_set_state (pipeline, GST_STATE_PAUSED);

//        ret = true;
        // unref sink
        gst_object_unref (sink);
    }

    // free pipeline
    gst_object_unref (pipeline);

    *w = 1920;
    *h = 1080;

    return ret;
}


GstFlowReturn NetworkStream::callback_sample (GstAppSink *sink, gpointer p)
{
    GstFlowReturn ret = GST_FLOW_OK;

    Log::Info("callback_sample PING");

    // non-blocking read new sample
    GstSample *sample = gst_app_sink_pull_sample(sink);

    // if got a valid sample
    if (sample != NULL && !gst_app_sink_is_eos (sink)) {

        const GstStructure *truc = gst_sample_get_info(sample);
        GstCaps *cap = gst_sample_get_caps(sample);

        gchar *c = gst_caps_to_string(cap);

        // get buffer from sample (valid until sample is released)
//        GstBuffer *buf = gst_sample_get_buffer (sample) ;

//        // send frames to media player only if ready
//        Stream *m = (Stream *)p;
//        if (m && m->ready_) {
//            // fill frame with buffer
//            if ( !m->fill_frame(buf, Stream::SAMPLE) )
//                ret = GST_FLOW_ERROR;
//        }


    }
    else
        ret = GST_FLOW_FLUSHING;

    // release sample
    gst_sample_unref (sample);

    return ret;
}

NetworkSource::NetworkSource() : StreamSource()
{
    // create stream
    stream_ = (Stream *) new NetworkStream;

    // set icons
    overlays_[View::MIXING]->attach( new Symbol(Symbol::EMPTY, glm::vec3(0.8f, 0.8f, 0.01f)) );
    overlays_[View::LAYER]->attach( new Symbol(Symbol::EMPTY, glm::vec3(0.8f, 0.8f, 0.01f)) );
}

void NetworkSource::connect(NetworkToolkit::Protocol protocol, const std::string &address)
{
    Log::Notify("Creating Network Source '%s'", address.c_str());

    // extract host and port from "host:port"
    std::string host = address.substr(0, address.find_last_of(":"));
    std::string port_s = address.substr(address.find_last_of(":") + 1);
    uint port = std::stoi(port_s);

    // validate protocol
    if (protocol < NetworkToolkit::TCP_JPEG || protocol >= NetworkToolkit::DEFAULT)
        protocol = NetworkToolkit::TCP_JPEG;

    // open network stream
    networkstream()->open( protocol, host, port );
    stream_->play(true);
}

void NetworkSource::accept(Visitor& v)
{
    Source::accept(v);
    if (!failed())
        v.visit(*this);
}

NetworkStream *NetworkSource::networkstream() const
{
    return dynamic_cast<NetworkStream *>(stream_);
}

