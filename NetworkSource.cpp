#include <algorithm>
#include <sstream>
#include <glm/gtc/matrix_transform.hpp>

#include <gst/pbutils/pbutils.h>
#include <gst/gst.h>

#include "defines.h"
#include "ImageShader.h"
#include "Resource.h"
#include "Decorations.h"
#include "Stream.h"
#include "Visitor.h"
#include "Log.h"

#include "NetworkSource.h"

#ifndef NDEBUG
#define NETWORK_DEBUG
#endif

NetworkStream::NetworkStream(): Stream(), protocol_(NetworkToolkit::DEFAULT), address_("127.0.0.1"), port_(5000)
{

}

glm::ivec2 NetworkStream::resolution()
{
    return glm::ivec2( width_, height_);
}


void NetworkStream::open( NetworkToolkit::Protocol protocol, const std::string &address, uint port )
{
    protocol_ = protocol;
    address_ = address;
    port_ = port;

    int w = 800;
    int h = 600;

    std::ostringstream pipeline;

    pipeline << "tcpclientsrc port=" << port_ << " ";

    pipeline << NetworkToolkit::protocol_receive_pipeline[protocol_];

    pipeline << " ! videoconvert";

    // (private) open stream
    Stream::open(pipeline.str(), w, h);
}


NetworkSource::NetworkSource() : StreamSource()
{
    // create stream
    stream_ = (Stream *) new NetworkStream;

    // set icons
    overlays_[View::MIXING]->attach( new Symbol(Symbol::EMPTY, glm::vec3(0.8f, 0.8f, 0.01f)) );
    overlays_[View::LAYER]->attach( new Symbol(Symbol::EMPTY, glm::vec3(0.8f, 0.8f, 0.01f)) );
}

void NetworkSource::connect(NetworkToolkit::Protocol protocol, const std::string &address, uint port)
{
    Log::Notify("Creating Network Source '%s:%d'", address.c_str(), port);

    networkstream()->open( protocol, address, port );
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

