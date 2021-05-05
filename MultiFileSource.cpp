#include <sstream>
#include <algorithm>

#include "defines.h"
#include "ImageShader.h"
#include "Resource.h"
#include "Decorations.h"
#include "Stream.h"
#include "Visitor.h"
#include "GstToolkit.h"
#include "BaseToolkit.h"
#include "SystemToolkit.h"
#include "MediaPlayer.h"
#include "Log.h"

#include "MultiFileSource.h"

// example test gstreamer pipelines
//
// multifile : sequence of numbered images
// gst-launch-1.0 multifilesrc location="/home/bhbn/Images/sequence/frames%03d.png" caps="image/png,framerate=\(fraction\)12/1" loop=1 ! decodebin ! videoconvert ! autovideosink
//
// imagesequencesrc : sequence of numbered images (cannot loop)
// gst-launch-1.0 imagesequencesrc location=frames%03d.png start-index=1 framerate=24/1 ! decodebin ! videoconvert ! autovideosink
//
// splitfilesrc
// gst-launch-1.0 splitfilesrc location="/home/bhbn/Videos/MOV01*.MOD" ! decodebin ! videoconvert ! autovideosink

MultiFileSequence::MultiFileSequence() : width(0), height(0), min(0), max(0), loop(1)
{
}

MultiFileSequence::MultiFileSequence(const std::list<std::string> &list_files) : loop(1)
{
    location = BaseToolkit::common_numbered_pattern(list_files, &min, &max);

    // sanity check: the location pattern looks like a filename and seems consecutive numbered
    if ( SystemToolkit::extension_filename(location).empty() ||
         SystemToolkit::path_filename(location) != SystemToolkit::path_filename(list_files.front()) ||
         list_files.size() != max - min + 1 ) {
        location.clear();
    }

    if ( !location.empty() ) {
        MediaInfo media = MediaPlayer::UriDiscoverer( GstToolkit::filename_to_uri( list_files.front() ) );
        if (media.valid && media.isimage) {
            codec.resize(media.codec_name.size());
            std::transform(media.codec_name.begin(), media.codec_name.end(), codec.begin(), ::tolower);
            width = media.width;
            height = media.height;
        }
    }
}

bool MultiFileSequence::valid() const
{
    return !( location.empty() || codec.empty() || width < 1 || height < 1 || max == min);
}

inline MultiFileSequence& MultiFileSequence::operator = (const MultiFileSequence& b)
{
    if (this != &b) {
        this->width = b.width;
        this->height = b.height;
        this->min = b.min;
        this->max = b.max;
        this->loop = b.loop;
        this->location = b.location;
        this->codec = b.codec;
    }
    return *this;
}


bool MultiFileSequence::operator != (const MultiFileSequence& b)
{
    return ( location != b.location || codec != b.codec || width != b.width ||
              height != b.height || min != b.min || max != b.max || loop != b.loop );
}

MultiFile::MultiFile() : Stream(), src_(nullptr)
{

}

void MultiFile::open(const MultiFileSequence &sequence, uint framerate )
{
    if (sequence.location.empty())
        return;

    std::ostringstream gstreamer_pipeline;
    gstreamer_pipeline << "multifilesrc name=src location=\"";
    gstreamer_pipeline << sequence.location;
    gstreamer_pipeline << "\" caps=\"image/";
    gstreamer_pipeline << sequence.codec;
    gstreamer_pipeline << ",framerate=(fraction)";
    gstreamer_pipeline << framerate;
    gstreamer_pipeline << "/1\" loop=";
    gstreamer_pipeline << sequence.loop;
    gstreamer_pipeline << " start-index=";
    gstreamer_pipeline << sequence.min;
    gstreamer_pipeline << " stop-index=";
    gstreamer_pipeline << sequence.max;
    gstreamer_pipeline << " ! decodebin ! videoconvert";

    // (private) open stream
    Stream::open(gstreamer_pipeline.str(), sequence.width, sequence.height);

    src_ = gst_bin_get_by_name (GST_BIN (pipeline_), "src");

}

void MultiFile::setLoop (int on)
{
    if (src_) {
        g_object_set (src_, "loop", on, NULL);
    }
}

void MultiFile::setRange (int begin, int end)
{
    if (src_) {
        g_object_set (src_, "start-index", MAX(begin, 0), NULL);
        g_object_set (src_, "stop-index", MAX(end, 0), NULL);
    }
}



MultiFileSource::MultiFileSource(uint64_t id) : StreamSource(id), framerate_(0), range_(glm::ivec2(0))
{
    // create stream
    stream_ = static_cast<Stream *>( new MultiFile );

    // set symbol
    symbol_ = new Symbol(Symbol::SEQUENCE, glm::vec3(0.75f, 0.75f, 0.01f));
    symbol_->scale_.y = 1.5f;
}


void MultiFileSource::setFiles (const std::list<std::string> &list_files, uint framerate)
{
    setSequence(MultiFileSequence(list_files), framerate);
}

void MultiFileSource::setSequence (const MultiFileSequence &sequence, uint framerate)
{
    framerate_ = CLAMP( framerate, 1, 30);
    sequence_ = sequence;

    if (sequence_.valid())
    {
        // open gstreamer
        multifile()->open( sequence_, framerate_ );
        stream_->play(true);

        // init range
        range_ = glm::ivec2(sequence_.min, sequence_.max);

        // will be ready after init and one frame rendered
        ready_ = false;
    }
}

void MultiFileSource::setFramerate (uint framerate)
{
    if (multifile()) {
        setSequence(sequence_, framerate);
    }
}

void MultiFileSource::setLoop (bool on)
{
    if (multifile()) {
        sequence_.loop = on ? 1 : 0;
        multifile()->setLoop( sequence_.loop );
    }
}

void MultiFileSource::setRange (glm::ivec2 range)
{
    range_.x = glm::clamp( range.x, sequence_.min, sequence_.max );
    range_.y = glm::clamp( range.y, sequence_.min, sequence_.max );
    range_.x = glm::min( range_.x, range_.y );
    range_.y = glm::max( range_.x, range_.y );

    if (multifile())
        multifile()->setRange( range_.x, range_.y );
}

void MultiFileSource::accept(Visitor& v)
{
    Source::accept(v);
    if (!failed())
        v.visit(*this);
}

MultiFile *MultiFileSource::multifile() const
{
    return dynamic_cast<MultiFile *>(stream_);
}


