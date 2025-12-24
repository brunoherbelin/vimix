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

#include <thread>
#include <sstream>
#include <cstring>
#include <algorithm>

// Desktop OpenGL function loader
#include <glad/glad.h>

// gstreamer
#include <gst/gstformat.h>
#include <gst/video/video.h>
#include <gst/app/gstappsrc.h>
#include <gst/pbutils/pbutils.h>

//osc
#if ((ULONG_MAX) != (UINT_MAX))
#define _M_X64
#endif
#include "osc/OscOutboundPacketStream.h"

#include "Settings.h"
#include "Toolkit/GstToolkit.h"
#include "Toolkit/SystemToolkit.h"
#include "Log.h"
#include "Connection.h"
#include "Toolkit/NetworkToolkit.h"
#include "FrameGrabbing.h"

#include "Streamer.h"

#ifndef NDEBUG
#define STREAMER_DEBUG
#endif

/// oscsend 127.0.0.1 71510 /vimix/request is 9000 "vimixclient"
/// oscdump -L 9000 | xargs -L1 -P1 sh -c 'gst-launch-1.0 udpsrc port=$3 caps="application/x-rtp,media=(string)video,encoding-name=(string)RAW,sampling=(string)RGB,width=(string)$5,height=(string)$6" ! rtpvrawdepay ! queue max-size-buffers=10 ! videoconvert ! autovideosink'

/// oscsend 127.0.0.1 71510 /vimix/request is 9000 "h264"
/// oscdump -L 9000 | xargs -L1 -P1 sh -c 'gst-launch-1.0 udpsrc port=$3 caps="application/x-rtp,media=(string)video,encoding-name=(string)H264" ! rtph264depay ! queue ! decodebin ! videoconvert ! autovideosink'

/// oscsend 127.0.0.1 71510 /vimix/request is 9000 "jpeg"
/// oscdump -L 9000 | xargs -L1 -P1 sh -c 'gst-launch-1.0 udpsrc port=$3 caps="application/x-rtp,media=(string)video,encoding-name=(string)JPEG" ! rtpjpegdepay ! queue ! decodebin ! videoconvert ! autovideosink'

void Streaming::RequestListener::ProcessMessage( const osc::ReceivedMessage& m,
                                               const IpEndpointName& remoteEndpoint )
{
    char sender[IpEndpointName::ADDRESS_AND_PORT_STRING_LENGTH];
    remoteEndpoint.AddressAndPortAsString(sender);

    try{

        if( std::strcmp( m.AddressPattern(), OSC_PREFIX OSC_STREAM_REQUEST) == 0 ){

#ifdef STREAMER_DEBUG
            Log::Info("%s wants a stream.", sender);
#endif
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            int reply_to_port = (arg++)->AsInt32();
            const char *client_name = (arg++)->AsString();
            if (Streaming::manager().enabled()) {
                // default proposed protocol to stream
                NetworkToolkit::StreamProtocol protocol = NetworkToolkit::DEFAULT;
                // exception: if client is black listed for SHM
                if ( std::find( Streaming::manager().shm_blacklist_.begin(),
                                Streaming::manager().shm_blacklist_.end(), client_name
                                ) != Streaming::manager().shm_blacklist_.end() )
                    // then enforce local UDP transfer
                    protocol = NetworkToolkit::UDP_RAW;
                // add stream answering to request
                Streaming::manager()._addStream(sender, reply_to_port, client_name, protocol);
            }
            else
                Streaming::manager()._refuseStream(sender, reply_to_port);
        }
        else if( std::strcmp( m.AddressPattern(), OSC_PREFIX OSC_STREAM_DISCONNECT) == 0 ){
            // receive info on disconnection
            osc::ReceivedMessage::const_iterator arg = m.ArgumentsBegin();
            int port = (arg++)->AsInt32();
            int failed = arg != m.ArgumentsEnd() && (arg++)->AsBool();
            // remove the stream
            NetworkToolkit::StreamConfig removed = Streaming::manager().removeStream(sender, port);
            // if disconnection is due to failure of Shared Memory
            if (failed && removed.protocol == NetworkToolkit::SHM_RAW) {
                Log::Info("%s failed to connect shared memory.", removed.client_name.c_str());
                // put this client in black-list
                Streaming::manager().shm_blacklist_.push_back(removed.client_name);
            }
#ifdef STREAMER_DEBUG
            else
                Log::Info("%s:%d does not need streaming anymore.", sender, port);
#endif
        }
    }
    catch( osc::Exception& e ){
        // any parsing errors such as unexpected argument types, or
        // missing arguments get thrown as exceptions.
        Log::Info("error while parsing message '%s' from %s : %s", m.AddressPattern(), sender, e.what());
    }
}

void wait_for_request_(UdpListeningReceiveSocket *receiver)
{
    receiver->Run();
}

Streaming::Streaming() : enabled_(false)
{
    int port = Connection::manager().info().port_stream_request;
    receiver_ = new UdpListeningReceiveSocket(IpEndpointName( IpEndpointName::ANY_ADDRESS, port ), &listener_ );

    std::thread(wait_for_request_, receiver_).detach();
}

Streaming::~Streaming()
{
    if (receiver_!=nullptr) {
        receiver_->Break();
        delete receiver_;
    }
}

bool Streaming::busy()
{
    bool b = false;

    if (streamers_lock_.try_lock()) {
        std::vector<VideoStreamer *>::const_iterator sit = streamers_.begin();
        for (; sit != streamers_.end() && !b; ++sit)
            b = (*sit)->busy() ;
        streamers_lock_.unlock();
    }

    return b;
}


std::vector<std::string> Streaming::listStreams()
{
    std::vector<std::string>  ls;

    if (streamers_lock_.try_lock()) {
        std::vector<VideoStreamer *>::const_iterator sit = streamers_.begin();
        for (; sit != streamers_.end(); ++sit)
            ls.push_back( (*sit)->info() );
        streamers_lock_.unlock();
    }

    return ls;
}

void Streaming::enable(bool on)
{
    if (on) {
        // accept streaming requests
        enabled_ = true;
        Log::Info("Accepting stream requests to %s.", Connection::manager().info().name.c_str());
    }
    else {
        // refuse streaming requests
        enabled_ = false;
        // ending and removing all streaming
        streamers_lock_.lock();
        for (auto sit = streamers_.begin(); sit != streamers_.end(); sit=streamers_.erase(sit))
            (*sit)->stop();
        streamers_lock_.unlock();
        Log::Info("Refusing stream requests to %s. No streaming ongoing.", Connection::manager().info().name.c_str());
    }
}

NetworkToolkit::StreamConfig Streaming::removeStream(const std::string &sender, int port)
{
    NetworkToolkit::StreamConfig removed;
    // get ip of sender
    std::string sender_ip = sender.substr(0, sender.find_last_of(":"));

    // parse the list for a streamers matching IP and port
    streamers_lock_.lock();
    std::vector<VideoStreamer *>::const_iterator sit = streamers_.begin();
    for (; sit != streamers_.end(); ++sit){
        NetworkToolkit::StreamConfig config = (*sit)->config_;
        if (config.client_address.compare(sender_ip) == 0 && config.port == port ) {
#ifdef STREAMER_DEBUG
            Log::Info("Ending streaming to %s:%d", config.client_address.c_str(), config.port);
#endif
            // match: stop this streamer
            removed = config;
            (*sit)->stop();
            // remove from list
            streamers_.erase(sit);
            break;
        }
    }
    streamers_lock_.unlock();
    return removed;
}

void Streaming::removeStreams(const std::string &clientname)
{
    // remove all streamers matching given IP
    streamers_lock_.lock();
    std::vector<VideoStreamer *>::const_iterator sit = streamers_.begin();
    while ( sit != streamers_.end() ){
        NetworkToolkit::StreamConfig config = (*sit)->config_;
        if (config.client_name.compare(clientname) == 0) {
#ifdef STREAMER_DEBUG
            Log::Info("Ending streaming to %s:%d", config.client_address.c_str(), config.port);
#endif
            // match: stop this streamer
            (*sit)->stop();
            // remove from list
            sit = streamers_.erase(sit);
        }
        else
            ++sit;
    }
    streamers_lock_.unlock();
}

void Streaming::removeStream(const VideoStreamer *vs)
{
    if ( vs!= nullptr && streamers_lock_.try_lock()) {

        std::vector<VideoStreamer *>::const_iterator sit = streamers_.begin();
        while ( sit != streamers_.end() ){
            if ( *(sit) == vs) {
#ifdef STREAMER_DEBUG
                NetworkToolkit::StreamConfig config = vs->config_;
                Log::Info("Ending streaming to %s:%d", config.client_address.c_str(), config.port);
#endif
                // remove from list
                streamers_.erase(sit);
                break;
            }
            else
                ++sit;
        }
        streamers_lock_.unlock();
    }
}

void Streaming::_refuseStream(const std::string &sender, int reply_to)
{
    // get ip of client
    std::string sender_ip = sender.substr(0, sender.find_last_of(":"));
    // prepare to reply to client
    IpEndpointName host( sender_ip.c_str(), reply_to );
    UdpTransmitSocket socket( host );
    // build OSC message
    char buffer[IP_MTU_SIZE];
    osc::OutboundPacketStream p( buffer, IP_MTU_SIZE );
    p.Clear();
    p << osc::BeginMessage( OSC_PREFIX OSC_STREAM_REJECT );
    p << osc::EndMessage;
    // send OSC message to client
    socket.Send( p.Data(), p.Size() );
    // inform user
    Log::Warning("A connection request for streaming came in and was refused.\nYou can enable the Sharing on local network from the menu of the Output window.");
}

void Streaming::addStream(const std::string &sender, int port,
                          const std::string &clientname)
{
    // get ip of client
    std::string sender_ip = sender.substr(0, sender.find_last_of(":"));

    // prepare an offer
    NetworkToolkit::StreamConfig conf;
    conf.client_address = sender_ip;
    conf.client_name = clientname;
    conf.port = port;
    conf.width = FrameGrabbing::manager().width();
    conf.height = FrameGrabbing::manager().height();
    conf.protocol = Settings::application.stream_protocol > 0 ? NetworkToolkit::UDP_H264 : NetworkToolkit::UDP_JPEG;

    // create streamer & remember it
    VideoStreamer *streamer = new VideoStreamer(conf);
    streamers_lock_.lock();
    streamers_.push_back(streamer);
    streamers_lock_.unlock();

    // start streamer
    FrameGrabbing::manager().add(streamer);
}

void Streaming::_addStream(const std::string &sender, int reply_to,
                          const std::string &clientname, NetworkToolkit::StreamProtocol protocol)
{
    // get ip of client
    std::string sender_ip = sender.substr(0, sender.find_last_of(":"));
    // get port used to send the request
    std::string sender_port = sender.substr(sender.find_last_of(":") + 1);

    // prepare to reply to client
    IpEndpointName host( sender_ip.c_str(), reply_to );
    UdpTransmitSocket socket( host );

    // prepare an offer
    NetworkToolkit::StreamConfig conf;
    conf.client_address = sender_ip;
    conf.client_name = clientname;
    conf.port = std::stoi(sender_port); // this port seems free, so re-use it!
    conf.width = FrameGrabbing::manager().width();
    conf.height = FrameGrabbing::manager().height();

    if (protocol == NetworkToolkit::DEFAULT) {
        // without indication, the JPEG stream is default
        conf.protocol = NetworkToolkit::UDP_JPEG;
        // on localhost sharing, use SHARED MEMORY RAW
        if ( NetworkToolkit::is_host_ip(conf.client_address) )
            conf.protocol = NetworkToolkit::SHM_RAW;
        // for non-localhost, if low bandwidth is requested, use H264 codec
        else if (Settings::application.stream_protocol > 0)
            conf.protocol = NetworkToolkit::UDP_H264;
    }
    else
        conf.protocol = protocol;

    // build OSC message
    char buffer[IP_MTU_SIZE];
    osc::OutboundPacketStream p( buffer, IP_MTU_SIZE );
    p.Clear();
    p << osc::BeginMessage( OSC_PREFIX OSC_STREAM_OFFER );
    p << conf.port;
    p << (int) conf.protocol;
    p << conf.width << conf.height;
    p << osc::EndMessage;

    // send OSC message to client
    socket.Send( p.Data(), p.Size() );

#ifdef STREAMER_DEBUG
    Log::Info("Replying to %s:%d", sender_ip.c_str(), reply_to);
    Log::Info("Starting streaming to %s:%d", sender_ip.c_str(), conf.port);
#endif

    // create streamer & remember it
    VideoStreamer *streamer = new VideoStreamer(conf);
    streamers_lock_.lock();
    streamers_.push_back(streamer);
    streamers_lock_.unlock();

    // start streamer
    FrameGrabbing::manager().add(streamer);
}


VideoStreamer::VideoStreamer(const NetworkToolkit::StreamConfig &conf): FrameGrabber(), config_(conf), stopped_(false)
{
    frame_duration_ = gst_util_uint64_scale_int (1, GST_SECOND, STREAMING_FPS);  // fixed 30 FPS
}

std::string VideoStreamer::init(GstCaps *caps)
{
    // ignore
    if (caps == nullptr)
        return std::string("Invalid caps");

    // check that config matches the given buffer properties
    gint w = 0, h = 0;
    GstStructure *capstruct = gst_caps_get_structure (caps, 0);
    if ( gst_structure_has_field (capstruct, "width"))
        gst_structure_get_int (capstruct, "width", &w);
    if ( gst_structure_has_field (capstruct, "height"))
        gst_structure_get_int (capstruct, "height", &h);
    if ( config_.width != w || config_.height != h) {
        return std::string("Video Streamer cannot start: given frames (") + std::to_string(w) + " x " + std::to_string(h) +
                ") are incompatible with stream (" + std::to_string(config_.width) + " x " + std::to_string(config_.height) + ")";
    }

    // create a gstreamer pipeline
    std::string description = "appsrc name=src ! ";

    // prevent eroneous protocol values
    if (config_.protocol < 0 || config_.protocol >= NetworkToolkit::DEFAULT)
        config_.protocol = NetworkToolkit::UDP_RAW;

    // special case H264: can be Hardware accelerated
    bool found_harware_acceleration = false;
    if (config_.protocol == NetworkToolkit::UDP_H264 && Settings::application.render.gpu_decoding) {
        for (auto config = NetworkToolkit::stream_h264_send_pipeline.cbegin();
             config != NetworkToolkit::stream_h264_send_pipeline.cend() && !found_harware_acceleration; ++config) {
            if ( GstToolkit::has_feature(config->first) ) {
#ifdef USE_GST_OPENGL_SYNC_HANDLER
                if ( GstToolkit::has_feature("glupload") &&
                     GstToolkit::has_feature("glcolorconvert") ) {
                    description += "glupload ! glcolorconvert ! ";
                    Log::Info("Video Streamer with glupload & GPU color conversion");
                }
                else 
#endif
                {
                    description += config->second;
                    found_harware_acceleration = true;
                    Log::Info("Video Streamer using hardware accelerated encoder (%s)", config->first.c_str());
                }
            }
        }
    }
    // general case: use defined protocols
    if (!found_harware_acceleration) {
        description += "videoconvert ! ";
        description += NetworkToolkit::stream_send_pipeline[config_.protocol];
    }

    // parse pipeline descriptor
    GError *error = NULL;
    pipeline_ = gst_parse_launch (description.c_str(), &error);
    if (error != NULL) {
        std::string msg = std::string("Video Streamer : Could not construct pipeline ") + description + "\n" + std::string(error->message);
        g_clear_error (&error);
        return msg;
    }

    // setup streaming sink
    if (config_.protocol == NetworkToolkit::SHM_RAW) {
        std::string path = SystemToolkit::full_filename(SystemToolkit::temp_path(), "shm");
        path += std::to_string(config_.port);
        g_object_set (G_OBJECT (gst_bin_get_by_name (GST_BIN (pipeline_), "sink")),
                      "sync", FALSE,
                      "socket-path", path.c_str(),  NULL);
    }
    else {
        g_object_set (G_OBJECT (gst_bin_get_by_name (GST_BIN (pipeline_), "sink")),
                      "sync", FALSE,
                      "host", config_.client_address.c_str(),
                      "port", config_.port,  NULL);
    }

    // setup custom app source
    src_ = GST_APP_SRC( gst_bin_get_by_name (GST_BIN (pipeline_), "src") );
    if (src_) {

        g_object_set (G_OBJECT (src_),
                      "is-live", TRUE,
                      "format", GST_FORMAT_TIME,
                      //                     "do-timestamp", TRUE,
                      NULL);

        // configure stream
        gst_app_src_set_stream_type( src_, GST_APP_STREAM_TYPE_STREAM);
        gst_app_src_set_latency( src_, -1, 0);

        // Set buffer size
        gst_app_src_set_max_bytes( src_, buffering_size_ );

        // specify streaming framerate in the given caps
        GstCaps *tmp = gst_caps_copy( caps );
        GValue v = G_VALUE_INIT;
        g_value_init (&v, GST_TYPE_FRACTION);
        gst_value_set_fraction (&v, STREAMING_FPS, 1);  // fixed 30 FPS
        gst_caps_set_value(tmp, "framerate", &v);
        g_value_unset (&v);

        // instruct src to use the caps
        caps_ = gst_caps_copy( tmp );
        gst_app_src_set_caps (src_, caps_);
        gst_caps_unref (tmp);

        // setup callbacks
        GstAppSrcCallbacks callbacks;
        callbacks.need_data = FrameGrabber::callback_need_data;
        callbacks.enough_data = FrameGrabber::callback_enough_data;
        callbacks.seek_data = NULL; // stream type is not seekable
        gst_app_src_set_callbacks (src_, &callbacks, this, NULL);

    }
    else {
        return std::string("Video Streamer : Failed to configure frame grabber.");
    }

    // all good
    initialized_ = true;

    return std::string("Streaming to ") + config_.client_name + " started.";
}

void VideoStreamer::terminate()
{
    // send EOS
    gst_app_src_end_of_stream (src_);

    // force finished
    endofstream_ = true;
    active_ = false;

    // make sure the shared memory socket is deleted
    if (config_.protocol == NetworkToolkit::SHM_RAW) {
        std::string path = SystemToolkit::full_filename(SystemToolkit::temp_path(), "shm");
        path += std::to_string(config_.port);
        SystemToolkit::remove_file(path);
    }

    Log::Notify("Streaming to %s finished after %s s.", config_.client_name.c_str(),
                GstToolkit::time_to_string(duration_).c_str());
}

void VideoStreamer::stop ()
{
    // stop recording
    FrameGrabber::stop ();

    // inform streaming manager to remove myself
    // NB: will not be effective if called inside a locked streamers_lock_
    Streaming::manager().removeStream(this);
}

std::string VideoStreamer::info(bool extended) const
{
    std::ostringstream ret;

    if (extended) {
        ret << NetworkToolkit::stream_send_pipeline[config_.protocol];
    }
    else {    
        if (!initialized_)
            ret << "Connecting";
        else if (active_) {
            ret << NetworkToolkit::stream_protocol_label[config_.protocol];
            ret << " to ";
            ret << config_.client_name;
        }
        else
            ret <<  "Streaming terminated.";
    }

    return ret.str();
}
