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

#include "Toolkit/GstToolkit.h"
#include "Toolkit/SystemToolkit.h"

#include "SplitMediaPlayer.h"

// example test gstreamer pipelines
//
// gst-launch-1.0 splitmuxsrc location=video*.mov ! decodebin ! autovideosink
// gst-launch-1.0 playbin uri="splitmux:///path/to/video*.mp4"
//
// An arbitrary list of files is given to splitmuxsrc with its 'format-location'
// signal, connected when playbin creates the source ('source-setup' signal).

SplitMediaPlayer::SplitMediaPlayer() : MediaPlayer()
{
}

void SplitMediaPlayer::open(const std::list<std::string> &files)
{
    // Cannot be opened with other files if already open or discovering
    if (isOpen() || discoverer_.valid())
        return;

    // nothing to open
    if (files.empty()) {
        failed_ = true;
        return;
    }

    files_ = files;
    files_pattern_ = FilesPattern(files_);

    // splitmux uri on first file (actual list of files given by format-location)
    std::string uri = GstToolkit::filename_to_uri( files_.front() );
    if (uri.rfind("file://", 0) == 0)
        uri = "splitmux://" + uri.substr(7);
    else
        uri.clear();

    MediaPlayer::open(files_.front(), uri);
}

std::string SplitMediaPlayer::filename() const
{
    return files_pattern_.empty() ? MediaPlayer::filename() : files_pattern_;
}

std::string SplitMediaPlayer::FilesPattern(const std::list<std::string> &files)
{
    if (files.empty())
        return std::string();
    if (files.size() == 1)
        return files.front();

    // length of the shortest name
    size_t len = files.front().size();
    for (const std::string &f : files)
        len = std::min(len, f.size());

    // length of the common prefix
    const std::string &s0 = files.front();
    size_t prefix = len;
    for (const std::string &f : files)
        prefix = std::min(prefix, (size_t) std::distance(s0.cbegin(),
                          std::mismatch(s0.cbegin(), s0.cbegin() + prefix, f.cbegin()).first));

    // length of the common suffix (not overlapping with prefix)
    size_t suffix = len - prefix;
    for (const std::string &f : files)
        suffix = std::min(suffix, (size_t) std::distance(s0.crbegin(),
                          std::mismatch(s0.crbegin(), s0.crbegin() + suffix, f.crbegin()).first));

    // all the same name
    if (prefix == len && std::all_of(files.cbegin(), files.cend(), [&s0](const std::string &f) { return f == s0; }))
        return s0;

    // range [first-last] of the middle parts, if they are all numbers
    std::string first, last;
    auto numbered = [&files, &first, &last](size_t pre, size_t suf) {
        first.clear();
        last.clear();
        for (const std::string &f : files) {
            const std::string middle = f.substr(pre, f.size() - pre - suf);
            if (middle.empty() || !std::all_of(middle.cbegin(), middle.cend(), ::isdigit))
                return false;
            // compare numbers by length, then by digits
            auto less = [](const std::string &a, const std::string &b) {
                return a.size() < b.size() || (a.size() == b.size() && a < b); };
            if (first.empty() || less(middle, first))
                first = middle;
            if (last.empty() || less(last, middle))
                last = middle;
        }
        return true;
    };

    // digits common to all could be part of the numbers (e.g. 'x1.mov' and 'x11.mov'):
    // otherwise, try again with the digits around the middle part
    bool is_numbered = numbered(prefix, suffix);
    if (!is_numbered) {
        size_t pre = prefix, suf = suffix;
        while (pre > 0 && ::isdigit(s0[pre - 1]))
            --pre;
        while (suf > 0 && ::isdigit(s0[s0.size() - suf]))
            --suf;
        if ( (pre != prefix || suf != suffix) && numbered(pre, suf) ) {
            prefix = pre;
            suffix = suf;
            is_numbered = true;
        }
    }

    const std::string middle = is_numbered ? "[" + first + "-" + last + "]" : "*";
    return s0.substr(0, prefix) + middle + s0.substr(s0.size() - suffix);
}

void SplitMediaPlayer::startDiscovery()
{
    // discover all files, filling the segments
    // (no evaluation of the concatenated media)
    discoverer_ = std::async( SplitMediaPlayer::SplitDiscoverer, files_);
}

MediaInfo SplitMediaPlayer::SplitDiscoverer(const std::list<std::string> &files)
{
    // discover every file
    std::vector<MediaInfo> infos;
    for (const std::string &f : files)
        infos.push_back( MediaPlayer::UriDiscoverer( GstToolkit::filename_to_uri(f) ) );

    return SplitMediaInfo(files, infos);
}

MediaInfo SplitMediaPlayer::SplitMediaInfo(const std::list<std::string> &files,
                                           const std::vector<MediaInfo> &infos)
{
    MediaInfo info;
    std::vector<GstClockTime> starts;

    if (files.size() < 2 || infos.size() != files.size()) {
        info.log = "Less than two videos";
        return info;
    }

    GstClockTime end = 0;
    auto media = infos.cbegin();
    for (auto f = files.cbegin(); f != files.cend(); ++f, ++media) {

        const std::string name = SystemToolkit::filename(*f);

        if (!media->valid) {
            info = MediaInfo();
            info.log = name + ": " + media->log;
            return info;
        }
        if (media->isimage || media->end == GST_CLOCK_TIME_NONE || !media->seekable) {
            info = MediaInfo();
            info.log = name + ": not a seekable video";
            return info;
        }

        // first file gives the reference
        if (f == files.cbegin())
            info = *media;
        // all other files must be compatible
        else {
            std::string mismatch;
            if (media->width != info.width || media->height != info.height)
                mismatch = "resolution (" + std::to_string(media->width) + " x " + std::to_string(media->height) + ")";
            else if (media->codec_name != info.codec_name)
                mismatch = "codec (" + media->codec_name + ")";
            else if (media->framerate_n * info.framerate_d != info.framerate_n * media->framerate_d)
                mismatch = "framerate (" + std::to_string(media->framerate_n) + "/" + std::to_string(media->framerate_d) + ")";
            else if (media->hasaudio != info.hasaudio)
                mismatch = media->hasaudio ? "audio (has audio)" : "audio (no audio)";
            else if (media->interlaced != info.interlaced)
                mismatch = "interlacing";

            if (!mismatch.empty()) {
                info = MediaInfo();
                info.log = name + ": " + mismatch + " differs from " + SystemToolkit::filename(files.front());
                return info;
            }
            info.bitrate = MAX(info.bitrate, media->bitrate);
        }

        // start of segment and accumulated duration
        starts.push_back(end);
        end += media->end;
    }

    info.end = end;
    info.log.clear();
    info.segments = starts;

    return info;
}

void SplitMediaPlayer::setupPipeline()
{
    // playbin (or uridecodebin) creates the splitmuxsrc source from uri
    GstElement *bin = pipeline_;
    GstElement *decoder = gst_bin_get_by_name (GST_BIN (pipeline_), "decoder");
    if (decoder)
        bin = decoder;

    g_signal_connect (G_OBJECT (bin), "source-setup", G_CALLBACK (callback_source_setup), this);

    if (decoder)
        gst_object_unref (decoder);
}

void SplitMediaPlayer::callback_source_setup (GstElement *, GstElement *source, gpointer user_data)
{
    GstElementFactory *factory = gst_element_get_factory (source);
    if (factory && g_strcmp0 (GST_OBJECT_NAME (factory), "splitmuxsrc") == 0)
        g_signal_connect (G_OBJECT (source), "format-location", G_CALLBACK (callback_format_location), user_data);
}

gchar **SplitMediaPlayer::callback_format_location (GstElement *, gpointer user_data)
{
    SplitMediaPlayer *mp = static_cast<SplitMediaPlayer *>(user_data);

    // NULL-terminated array of filenames, freed by splitmuxsrc
    gchar **list = g_new0 (gchar *, mp->files_.size() + 1);
    size_t i = 0;
    for (const std::string &f : mp->files_)
        list[i++] = g_strdup (f.c_str());

    return list;
}

int SplitMediaPlayer::currentSegment()
{
    const std::vector<GstClockTime> &s = segments();
    GstClockTime pos = position();
    if (s.empty() || pos == GST_CLOCK_TIME_NONE)
        return -1;

    int index = 0;
    while ( index + 1 < (int) s.size() && s[index + 1] <= pos )
        ++index;

    return index;
}

bool SplitMediaPlayer::go_to_segment(int index)
{
    const std::vector<GstClockTime> &s = segments();
    if (index < 0 || index >= (int) s.size())
        return false;

    return go_to( s[index] );
}

int SplitMediaPlayer::nextSegmentIndex()
{
    int index = currentSegment();
    if (index < 0)
        return -1;

    ++index;
    if (index >= (int) segments().size())
        index = 0; // loop to first segment
    return index;
}

int SplitMediaPlayer::previousSegmentIndex()
{
    int index = currentSegment();
    if (index < 0)
        return -1;

    // beginning of current segment, unless already close to it
    if ( position() > segments()[index] + GST_SECOND / 5 )
        return index;

    return index - 1;
}

GstClockTime SplitMediaPlayer::nextSegmentTime()
{
    int index = nextSegmentIndex();
    return index < 0 ? GST_CLOCK_TIME_NONE : segments()[index];
}

GstClockTime SplitMediaPlayer::previousSegmentTime()
{
    int index = previousSegmentIndex();
    return index < 0 ? GST_CLOCK_TIME_NONE : segments()[index];
}

bool SplitMediaPlayer::nextSegment()
{
    return go_to_segment( nextSegmentIndex() );
}

bool SplitMediaPlayer::previousSegment()
{
    return go_to_segment( previousSegmentIndex() );
}
