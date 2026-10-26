/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2026 Bruno Herbelin <bruno.herbelin@gmail.com>
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

#include <algorithm>
#include <future>

#include "Log.h"
#include "Toolkit/GstToolkit.h"
#include "Toolkit/BaseToolkit.h"
#include "Toolkit/SystemToolkit.h"

#include "Player/MultiFilePlayer.h"

// example test gstreamer pipelines
//
// imagesequencesrc : sequence of numbered images, seekable (forward and backward)
// gst-launch-1.0 imagesequencesrc location=frames%03d.png start-index=1 stop-index=120 framerate=24/1 ! decodebin ! videoconvert ! autovideosink
// gst-launch-1.0 playbin uri="imagesequence:///path/frames%2503d.png?framerate=24/1&start-index=1&stop-index=120"
//


MultiFileSequence::MultiFileSequence() : width(0), height(0), min(0), max(0)
{
}

MultiFileSequence::MultiFileSequence(const std::list<std::string> &list_files)
{
    if (list_files.empty()) {
        location.clear();
        codec.clear();
        width = height = 0;
        min = max = 0;
        return;
    }

    // try to generate location pattern from the list of files (e.g. "frames%03d.png")
    location = BaseToolkit::common_numbered_pattern(list_files, &min, &max);

    // sanity check: does the location pattern looks like a filename and seems consecutive numbered
    if ( SystemToolkit::extension_filename(location).empty() ||
         SystemToolkit::path_filename(location) != SystemToolkit::path_filename(list_files.front()) ||
         list_files.size() != (size_t) (max - min) + 1 ) {
        location.clear();
    }

    // get codec and resolution from first file
    MediaInfo media = MediaPlayer::UriDiscoverer( GstToolkit::filename_to_uri( list_files.front() ) );
    if (media.valid && media.isimage) {
        if (location.empty())
            codec = "mixed files";
        else {
            codec.resize(media.codec_name.size());
            std::transform(media.codec_name.begin(), media.codec_name.end(), codec.begin(), ::tolower);
            // codecs without description are given as caps (e.g. 'image/webp')
            if (codec.rfind("image/", 0) == 0)
                codec.erase(0, 6);
        }
        width = media.width;
        height = media.height;
    }

}

MultiFileSequence::MultiFileSequence(const std::string &path)
    : MultiFileSequence( SystemToolkit::list_directory(path, {"*.jpg", "*.jpeg", "*.png", "*.webp"}) )
{
}

bool MultiFileSequence::valid() const
{
    return !( location.empty() || codec.empty() || width < 1 || height < 1 || max == min);
}

bool MultiFileSequence::operator != (const MultiFileSequence& b)
{
    return ( location != b.location || codec != b.codec || width != b.width ||
              height != b.height || min != b.min || max != b.max );
}


MultiFilePlayer::MultiFilePlayer() : MediaPlayer(), fps_(0)
{
    // images are decoded by software
    force_software_decoding_ = true;
}

bool MultiFilePlayer::available()
{
    GstElementFactory *factory = gst_element_factory_find("imagesequencesrc");
    if (factory == nullptr)
        return false;
    gst_object_unref(factory);
    return true;
}

std::string MultiFilePlayer::SequenceUri(const MultiFileSequence &sequence, uint framerate)
{
    // imagesequence uri from the file uri of the location pattern
    // (NB: '%' of the pattern is escaped by the conversion to uri)
    std::string uri = GstToolkit::filename_to_uri( sequence.location );
    if (uri.rfind("file://", 0) != 0)
        return std::string();

    uri = "imagesequence://" + uri.substr(7);
    uri += "?framerate=" + std::to_string(framerate) + "/1";
    uri += "&start-index=" + std::to_string(sequence.min);
    uri += "&stop-index=" + std::to_string(sequence.max);

    return uri;
}

MediaInfo MultiFilePlayer::SequenceInfo(const MultiFileSequence &sequence, uint framerate)
{
    MediaInfo info;

    // test the first and last images of the sequence
    char first[4096], last[4096];
    snprintf(first, sizeof(first), sequence.location.c_str(), sequence.min);
    snprintf(last, sizeof(last), sequence.location.c_str(), sequence.max);
    if ( !SystemToolkit::file_exists(first) || !SystemToolkit::file_exists(last) ) {
        info.log = "Invalid path or missing images.";
        return info;
    }

    if (framerate < 1 || sequence.max <= sequence.min) {
        info.log = "Invalid sequence.";
        return info;
    }

    const guint64 num_frames = (guint64) (sequence.max - sequence.min) + 1;

    info.width = sequence.width;
    info.par_width = sequence.width;
    info.height = sequence.height;
    info.codec_name = sequence.codec;
    info.framerate_n = framerate;
    info.framerate_d = 1;
    info.dt = GST_SECOND / framerate;
    info.end = num_frames * info.dt;
    info.seekable = true;
    info.isimage = false;
    info.valid = true;

    return info;
}

void MultiFilePlayer::open(const MultiFileSequence &sequence, uint framerate)
{
    // Cannot be opened with another sequence if already open or discovering
    if (isOpen() || discoverer_.valid())
        return;

    if ( !available() ) {
        Log::Warning("MultiFilePlayer: GStreamer element 'imagesequencesrc' is not available.");
        failed_ = true;
        return;
    }

    if ( !sequence.valid() || framerate < 1 ) {
        failed_ = true;
        return;
    }

    // set sequence
    sequence_ = sequence;
    fps_ = framerate;

    // set path
    filename_ = sequence_.location;

    // set uri
    uri_ = SequenceUri(sequence_, fps_);

    // start URI discovering thread:
    startDiscovery();
}

void MultiFilePlayer::startDiscovery()
{
    // all images are intra frames: backward playback is always possible
    evaluation_ = MediaEvaluation();
    evaluation_.frame_count = evaluation_.keyframe_count = (guint64) (sequence_.max - sequence_.min) + 1;
    evaluation_.gop_size_min = evaluation_.gop_size_max = 1;
    evaluation_.pts_first = 0;
    evaluation_.done = true;

    // info on the sequence is known: no need to discover the images
    discoverer_ = std::async( std::launch::async, MultiFilePlayer::SequenceInfo, sequence_, fps_);
}

bool MultiFilePlayer::setFramerate(uint framerate)
{
    if (framerate < 1 || !isOpen())
        return false;

    if (framerate == fps_)
        return true;

    fps_ = framerate;
    uri_ = SequenceUri(sequence_, fps_);

    // timing of the sequence at the new framerate
    const MediaInfo info = SequenceInfo(sequence_, fps_);
    media_.framerate_n = info.framerate_n;
    media_.dt = info.dt;
    media_.end = info.end;
    timeline_.setTiming( TimeInterval(0, media_.end), media_.dt);

    // restart the pipeline with the new uri
    reopen();

    return true;
}
