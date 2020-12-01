#include <sstream>
#include <glm/gtc/matrix_transform.hpp>

#include "PatternSource.h"

#include "defines.h"
#include "ImageShader.h"
#include "Resource.h"
#include "Decorations.h"
#include "Stream.h"
#include "Visitor.h"
#include "Log.h"

#define MAX_PATTERN 23

//    smpte (0) – SMPTE 100%% color bars
//    snow (1) – Random (television snow)
//    black (2) – 100%% Black
//    white (3) – 100%% White
//    red (4) – Red
//    green (5) – Green
//    blue (6) – Blue
//    checkers-1 (7) – Checkers 1px
//    checkers-2 (8) – Checkers 2px
//    checkers-4 (9) – Checkers 4px
//    checkers-8 (10) – Checkers 8px
//    circular (11) – Circular
//    blink (12) – Blink
//    smpte75 (13) – SMPTE 75%% color bars
//    zone-plate (14) – Zone plate
//    gamut (15) – Gamut checkers
//    chroma-zone-plate (16) – Chroma zone plate
//    solid-color (17) – Solid color
//    ball (18) – Moving ball
//    smpte100 (19) – SMPTE 100%% color bars
//    bar (20) – Bar
//    pinwheel (21) – Pinwheel
//    spokes (22) – Spokes
//    gradient (23) – Gradient
//    colors (24) – Colors
const char* pattern_internal_[24] = { "videotestsrc pattern=black",
                                      "videotestsrc pattern=white",
                                      "videotestsrc pattern=gradient",
                                      "videotestsrc pattern=checkers-1 ! video/x-raw,format=GRAY8 ! videoconvert",
                                      "videotestsrc pattern=checkers-8 ! video/x-raw,format=GRAY8 ! videoconvert",
                                      "videotestsrc pattern=circular",
                                      "frei0r-src-lissajous0r ratiox=0.001 ratioy=0.999 ! videoconvert",
                                      "videotestsrc pattern=pinwheel",
                                      "videotestsrc pattern=spokes",
                                      "videotestsrc pattern=red",
                                      "videotestsrc pattern=green",
                                      "videotestsrc pattern=blue",
                                      "videotestsrc pattern=smpte100",
                                      "videotestsrc pattern=colors",
                                      "videotestsrc pattern=smpte",
                                      "videotestsrc pattern=snow",
                                      "videotestsrc pattern=blink",
                                      "videotestsrc pattern=zone-plate",
                                      "videotestsrc pattern=chroma-zone-plate",
                                      "videotestsrc pattern=bar horizontal-speed=5",
                                      "videotestsrc pattern=ball",
                                      "frei0r-src-ising0r",
                                      "videotestsrc pattern=black ! timeoverlay halignment=center valignment=center font-desc=\"Sans, 72\" ",
                                      "videotestsrc pattern=black ! clockoverlay halignment=center valignment=center font-desc=\"Sans, 72\" "
                                    };

std::vector<std::string> Pattern::pattern_types = { "Black",
                                         "White",
                                         "Gradient",
                                         "Checkers 1x1 px",
                                         "Checkers 8x8 px",
                                         "Circles",
                                         "Lissajous",
                                         "Pinwheel",
                                         "Spokes",
                                         "Red",
                                         "Green",
                                         "Blue",
                                         "Color bars",
                                         "RGB grid",
                                         "SMPTE test pattern",
                                         "Television snow",
                                         "Blink",
                                         "Fresnel zone plate",
                                         "Chroma zone plate",
                                         "Bar moving",
                                         "Ball bouncing",
                                         "Blob",
                                         "Timer",
                                         "Clock"
                                       };

Pattern::Pattern() : Stream(), type_(MAX_PATTERN+1) // invalid pattern
{

}

glm::ivec2 Pattern::resolution()
{
    return glm::ivec2( width_, height_);
}


void Pattern::open( uint pattern, glm::ivec2 res )
{
    type_ = MIN(pattern, MAX_PATTERN);
    std::string gstreamer_pattern = pattern_internal_[type_];

    // there is always a special case...
    switch(type_)
    {
    case 18: // zone plates
    case 17:
    {
        std::ostringstream oss;
        oss << " kx2=" << (int)(res.x * 10.f / res.y) << " ky2=10 kt=4";
        gstreamer_pattern += oss.str(); // Zone plate
    }
        break;
    default:
        break;
    }

    // all patterns before 'SMPTE test pattern' are single frames (not animated)
    single_frame_ = type_ < 14;

    // (private) open stream
    Stream::open(gstreamer_pattern, res.x, res.y);
}

PatternSource::PatternSource() : StreamSource()
{
    // create stream
    stream_ = (Stream *) new Pattern;

    // set symbol
    symbol_ = new Symbol(Symbol::PATTERN, glm::vec3(0.8f, 0.8f, 0.01f));
}

void PatternSource::setPattern(uint type, glm::ivec2 resolution)
{
    Log::Notify("Creating Source with pattern '%s'", Pattern::pattern_types[type].c_str());

    pattern()->open( (uint) type, resolution );
    stream_->play(true);
}

void PatternSource::accept(Visitor& v)
{
    Source::accept(v);
    if (!failed())
        v.visit(*this);
}

Pattern *PatternSource::pattern() const
{
    return dynamic_cast<Pattern *>(stream_);
}


