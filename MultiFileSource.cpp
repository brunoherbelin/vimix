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

MultiFileSequence::MultiFileSequence() : width(0), height(0), min(0), max(0)
{
}

MultiFileSequence::MultiFileSequence(const std::list<std::string> &list_files)
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
        this->location = b.location;
        this->codec = b.codec;
    }
    return *this;
}


bool MultiFileSequence::operator != (const MultiFileSequence& b)
{
    return ( location != b.location || codec != b.codec || width != b.width ||
              height != b.height || min != b.min || max != b.max );
}

MultiFile::MultiFile() : Stream(), src_(nullptr)
{

}

void MultiFile::open (const MultiFileSequence &sequence, uint framerate )
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
    gstreamer_pipeline << "/1\" loop=1";
    gstreamer_pipeline << " start-index=";
    gstreamer_pipeline << sequence.min;
    gstreamer_pipeline << " stop-index=";
    gstreamer_pipeline << sequence.max;
    gstreamer_pipeline << " ! decodebin ! videoconvert";

    // (private) open stream
    Stream::open(gstreamer_pipeline.str(), sequence.width, sequence.height);

    // keep multifile source for dynamic properties change
    src_ = gst_bin_get_by_name (GST_BIN (pipeline_), "src");
}

void MultiFile::close ()
{
    if (src_ != nullptr) {
        gst_object_unref (src_);
        src_ = nullptr;
    }
    Stream::close();
}

void MultiFile::setProperties (int begin, int end, int loop)
{
    if (src_) {
        g_object_set (src_, "start-index", MAX(begin, 0), NULL);
        g_object_set (src_, "stop-index", MAX(end, 0), NULL);
        g_object_set (src_, "loop", MIN(loop, 1), NULL);
    }
}



MultiFileSource::MultiFileSource (uint64_t id) : StreamSource(id), framerate_(0), begin_(-1), end_(INT_MAX), loop_(1)
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

        // validate range and apply loop_
        setRange(begin_, end_);

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
        loop_ = on ? 1 : 0;
        multifile()->setProperties (begin_, end_, loop_);
    }
}

void MultiFileSource::setRange (int begin, int end)
{
    begin_ = glm::clamp( begin, sequence_.min, sequence_.max );
    end_   = glm::clamp( end  , sequence_.min, sequence_.max );
    begin_ = glm::min( begin_, end_ );
    end_   = glm::max( begin_, end_ );

    if (multifile())
        multifile()->setProperties (begin_, end_, loop_);
}

void MultiFileSource::accept (Visitor& v)
{
    Source::accept(v);
    if (!failed())
        v.visit(*this);
}

MultiFile *MultiFileSource::multifile () const
{
    return dynamic_cast<MultiFile *>(stream_);
}


