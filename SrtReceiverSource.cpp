
#include "Log.h"
#include "Decorations.h"
#include "Visitor.h"

#include "SrtReceiverSource.h"

SrtReceiverSource::SrtReceiverSource(uint64_t id) : StreamSource(id)
{
    // create stream
    stream_ = new Stream;

    // set symbol
    symbol_ = new Symbol(Symbol::SHARE, glm::vec3(0.75f, 0.75f, 0.01f));
    symbol_->scale_.y = 1.5f;
}


void SrtReceiverSource::setConnection(const std::string &ip, const std::string &port)
{
    // TODO add check on wellformed IP and PORT
    ip_ = ip;
    port_ = port;
    Log::Notify("Creating Source SRT receiving from '%s'", uri().c_str());

    std::string description = "srtsrc uri=" + uri() + " ! tsdemux ! decodebin ! videoconvert";

    // open gstreamer
    stream_->open(description);
    stream_->play(true);

    // will be ready after init and one frame rendered
    ready_ = false;
}

std::string SrtReceiverSource::uri() const
{
    return std::string("srt://") + ip_ + ":" + port_;
}

void SrtReceiverSource::accept(Visitor& v)
{
    Source::accept(v);
    if (!failed())
        v.visit(*this);
}

glm::ivec2 SrtReceiverSource::icon() const
{
    return glm::ivec2(ICON_SOURCE_SRT);
}

std::string SrtReceiverSource::info() const
{
    return std::string("SRT receiver from '") + uri() + "'";
}
