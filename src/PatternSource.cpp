/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2022 Bruno Herbelin <bruno.herbelin@gmail.com>
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

#include <sstream>
#include <glm/gtc/matrix_transform.hpp>

#include "defines.h"
#include "ImageShader.h"
#include "Resource.h"
#include "Decorations.h"
#include "Stream.h"
#include "Visitor.h"
#include "Log.h"
#include "GstToolkit.h"

#include "PatternSource.h"

//
//   Fill the list of patterns videotestsrc
//
//    Label (for display), feature (for test), pipeline (for gstreamer), animated (true/false), available (false by default)
std::vector<pattern_descriptor> Pattern::patterns_ = {
    { "Black", "videotestsrc", "videotestsrc pattern=black", false, false },
    { "White", "videotestsrc", "videotestsrc pattern=white", false, false },
    { "Gradient", "videotestsrc", "videotestsrc pattern=gradient", false, false },
    { "Checkers 1x1 px", "videotestsrc", "videotestsrc pattern=checkers-1 ! videobalance saturation=0 contrast=1.5", false, false },
    { "Checkers 8x8 px", "videotestsrc", "videotestsrc pattern=checkers-8 ! videobalance saturation=0 contrast=1.5", false, false },
    { "Circles", "videotestsrc", "videotestsrc pattern=circular", false, false },
    { "Lissajous", "frei0r-src-lissajous0r", "frei0r-src-lissajous0r ratiox=0.001 ratioy=0.999 ! videoconvert", false, false },
    { "Pinwheel", "videotestsrc", "videotestsrc pattern=pinwheel", false, false },
    { "Spokes", "videotestsrc", "videotestsrc pattern=spokes", false, false },
    { "Red", "videotestsrc", "videotestsrc pattern=red", false, false },
    { "Green", "videotestsrc", "videotestsrc pattern=green", false, false },
    { "Blue", "videotestsrc", "videotestsrc pattern=blue", false, false },
    { "Color bars", "videotestsrc", "videotestsrc pattern=smpte100", false, false },
    { "RGB grid", "videotestsrc", "videotestsrc pattern=colors", false, false },
    { "SMPTE test", "videotestsrc", "videotestsrc pattern=smpte", true, false },
    { "Television snow", "videotestsrc", "videotestsrc pattern=snow", true, false },
    { "Blink", "videotestsrc", "videotestsrc pattern=blink", true, false },
    { "Fresnel zone plate", "videotestsrc", "videotestsrc pattern=zone-plate kx2=XXX ky2=YYY kt=4", true, false },
    { "Chroma zone plate", "videotestsrc", "videotestsrc pattern=chroma-zone-plate kx2=XXX ky2=YYY kt=4", true, false },
    { "Bar moving", "videotestsrc", "videotestsrc pattern=bar horizontal-speed=5", true, false },
    { "Ball bouncing", "videotestsrc", "videotestsrc pattern=ball", true, false },
    { "Blob", "frei0r-src-ising0r", "frei0r-src-ising0r", true, false },
    { "Timer", "timeoverlay",  "videotestsrc pattern=solid-color foreground-color=0 ! timeoverlay halignment=center valignment=center font-desc=\"Sans, 72\" ", true, false },
    { "Clock", "clockoverlay", "videotestsrc pattern=solid-color foreground-color=0 ! clockoverlay halignment=center valignment=center font-desc=\"Sans, 72\" ", true, false },
    { "Resolution", "textoverlay", "videotestsrc pattern=solid-color foreground-color=0 ! textoverlay text=\"XXXX x YYYY px\" halignment=center valignment=center font-desc=\"Sans, 52\" ", false, false },
    { "Frame", "videobox", "videotestsrc pattern=solid-color foreground-color=0 ! videobox fill=white top=-10 bottom=-10 left=-10 right=-10", false, false },
    { "Cross", "textoverlay", "videotestsrc pattern=solid-color foreground-color=0 ! textoverlay text=\"+\" halignment=center valignment=center font-desc=\"Sans, 22\" ", false, false },
    { "Grid", "frei0r-src-test-pat-g", "frei0r-src-test-pat-g type=0.35", false, false },
    { "Point Grid", "frei0r-src-test-pat-g", "frei0r-src-test-pat-g type=0.4", false, false },
    { "Ruler", "frei0r-src-test-pat-g", "frei0r-src-test-pat-g type=0.9", false, false },
    { "RGB noise", "frei0r-filter-rgbnoise", "videotestsrc pattern=black ! frei0r-filter-rgbnoise noise=0.6", true, false },
    { "Philips test", "frei0r-src-test-pat-b", "frei0r-src-test-pat-b type=0.7 ", false, false }
};


Pattern::Pattern() : Stream(), type_(UINT_MAX) // invalid pattern
{

}

pattern_descriptor Pattern::get(uint type)
{
    type = CLAMP(type, 0, patterns_.size()-1);

    // check availability of feature to use this pattern
    if (!patterns_[type].available)
        patterns_[type].available = GstToolkit::has_feature(patterns_[type].feature);

    // return struct
    return patterns_[type];
}

uint Pattern::count()
{
    return patterns_.size();
}

glm::ivec2 Pattern::resolution()
{
    return glm::ivec2( width_, height_);
}


void Pattern::open( uint pattern, glm::ivec2 res )
{
    // clamp type to be sure
    type_ = MIN(pattern, Pattern::patterns_.size()-1);
    std::string gstreamer_pattern = Pattern::patterns_[type_].pipeline;

    //
    // pattern string post-processing: replace placeholders by resolution values
    // XXXX, YYYY = resolution x and y
    // XXX,  YYY  = resolution x and y / 10
    //
    // if there is a XXXX parameter to enter
    std::string::size_type xxxx = gstreamer_pattern.find("XXXX");
    if (xxxx != std::string::npos)
        gstreamer_pattern.replace(xxxx, 4, std::to_string(res.x));
    // if there is a YYYY parameter to enter
    std::string::size_type yyyy = gstreamer_pattern.find("YYYY");
    if (yyyy != std::string::npos)
        gstreamer_pattern.replace(yyyy, 4, std::to_string(res.y));
    // if there is a XXX parameter to enter
    std::string::size_type xxx = gstreamer_pattern.find("XXX");
    if (xxx != std::string::npos)
        gstreamer_pattern.replace(xxx, 3, std::to_string(res.x/10));
    // if there is a YYY parameter to enter
    std::string::size_type yyy = gstreamer_pattern.find("YYY");
    if (yyy != std::string::npos)
        gstreamer_pattern.replace(yyy, 3, std::to_string(res.y/10));

    // remember if the pattern is to be updated once or animated
    single_frame_ = !Pattern::patterns_[type_].animated;

    // (private) open stream
    Stream::open(gstreamer_pattern, res.x, res.y);
}

PatternSource::PatternSource(uint64_t id) : StreamSource(id)
{
    // create stream
    stream_ = static_cast<Stream *>( new Pattern );

    // set symbol
    symbol_ = new Symbol(Symbol::PATTERN, glm::vec3(0.75f, 0.75f, 0.01f));
    symbol_->scale_.y = 1.5f;
}

void PatternSource::setPattern(uint type, glm::ivec2 resolution)
{
    // open gstreamer with pattern
    if ( Pattern::get(type).available)      {
        pattern()->open( (uint) type, resolution );
    }
    // revert to pattern Black if not available
    else {
        pattern()->open( 0, resolution );
        Log::Warning("Pattern '%s' is not available in this version of vimix.", Pattern::get(type).label.c_str());
    }

    // play gstreamer
    stream_->play(true);

    // will be ready after init and one frame rendered
    ready_ = false;
}

void PatternSource::accept(Visitor& v)
{
    Source::accept(v);
    v.visit(*this);
}

Pattern *PatternSource::pattern() const
{
    return dynamic_cast<Pattern *>(stream_);
}


glm::ivec2 PatternSource::icon() const
{
    return glm::ivec2(ICON_SOURCE_PATTERN);
}

std::string PatternSource::info() const
{
    return "Pattern";
}
