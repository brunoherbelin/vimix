#include <sstream>
#include <glm/gtc/matrix_transform.hpp>

#include "PatternSource.h"

#include "defines.h"
#include "ImageShader.h"
#include "ImageProcessingShader.h"
#include "Resource.h"
#include "Primitives.h"
#include "Stream.h"
#include "Visitor.h"
#include "Log.h"

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
const char* pattern_internal_[25] = { "videotestsrc pattern=black",
                                      "videotestsrc pattern=white",
                                      "frei0r-src-test-pat-l",
                                      "videotestsrc pattern=gradient",
                                      "videotestsrc pattern=checkers-1 ! video/x-raw,format=GRAY8 ! videoconvert",
                                      "frei0r-src-test-pat-g",
                                      "videotestsrc pattern=circular",
                                      "videotestsrc pattern=pinwheel",
                                      "videotestsrc pattern=spokes",
                                      "videotestsrc pattern=red",
                                      "videotestsrc pattern=green",
                                      "videotestsrc pattern=blue",
                                      "videotestsrc pattern=smpte100",
                                      "frei0r-src-test-pat-c",
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

std::vector<std::string> Pattern::pattern_types = { "100% Black",
                                         "100% White",
                                         "Gray bars",
                                         "Gradient",
                                         "Checkers 1x1 px",
                                         "Checkerboard",
                                         "Circles",
                                         "Pinwheel",
                                         "Spokes",
                                         "100% Red",
                                         "100% Green",
                                         "100% Blue",
                                         "Color bars",
                                         "Color Gradient",
                                         "Color grid",
                                         "SMPTE test pattern",
                                         "Television snow",
                                         "Blink",
                                         "Fresnel zone plate",
                                         "Chroma zone plate",
                                         "Moving bar",
                                         "Moving ball",
                                         "Blob",
                                         "Timer",
                                         "Clock"
                                       };

Pattern::Pattern(glm::ivec2 res) : Stream()
{

    width_ = res.x;
    height_ = res.y;
}

glm::ivec2 Pattern::resolution()
{
    return glm::ivec2( width_, height_);
}


void Pattern::open( uint pattern )
{
    type_ = CLAMP(pattern, 0, 25);
    std::string gstreamer_pattern = pattern_internal_[type_];

    // always some special cases...
    switch(type_)
    {
    case 18:
    case 19:
    {
        std::ostringstream oss;
        oss << " kx2=" << (int)(aspectRatio() * 10.f) << " ky2=10 kt=4";
        gstreamer_pattern += oss.str(); // Zone plate
        single_frame_ = false;
    }
        break;
    default:
        break;
    }

    // all patterns before index are single frames (not animated)
    single_frame_ = type_ < 15;

    // (private) open stream
    open(gstreamer_pattern);
}

void Pattern::open(std::string gstreamer_pattern)
{
    // set gstreamer pipeline source
    description_ = gstreamer_pattern;

    // close before re-openning
    if (isOpen())
        close();

    execute_open();
}


PatternSource::PatternSource(glm::ivec2 resolution) : Source()
{
    // create stream
    stream_ = new Pattern(resolution);

    // create surface
    patternsurface_ = new Surface(renderingshader_);
}

PatternSource::~PatternSource()
{
    // delete media surface & stream
    delete patternsurface_;
    delete stream_;
}

bool PatternSource::failed() const
{
    return stream_->failed();
}

uint PatternSource::texture() const
{
    return stream_->texture();
}

void PatternSource::replaceRenderingShader()
{
    patternsurface_->replaceShader(renderingshader_);
}

void PatternSource::setPattern(int id)
{
    stream_->open(id);
    stream_->play(true);

    Log::Notify("Creating pattern %s", Pattern::pattern_types[id].c_str());
}


void PatternSource::init()
{
    if ( stream_->isOpen() ) {

        // update video
        stream_->update();

        // once the texture of media player is created
        if (stream_->texture() != Resource::getTextureBlack()) {

            // get the texture index from media player, apply it to the media surface
            patternsurface_->setTextureIndex( stream_->texture() );

            // create Frame buffer matching size of media player
            float height = float(stream_->width()) / stream_->aspectRatio();
            FrameBuffer *renderbuffer = new FrameBuffer(stream_->width(), (uint)height, true);

            // set the renderbuffer of the source and attach rendering nodes
            attach(renderbuffer);

            // icon in mixing view
            overlays_[View::MIXING]->attach( new Symbol(Symbol::PATTERN, glm::vec3(0.8f, 0.8f, 0.01f)) );
            overlays_[View::LAYER]->attach( new Symbol(Symbol::PATTERN, glm::vec3(0.8f, 0.8f, 0.01f)) );

            // done init
            initialized_ = true;
            Log::Info("Source Pattern linked to Stream %d.", stream_->description().c_str());

            // force update of activation mode
            active_ = true;
            touch();
        }
    }

}

void PatternSource::setActive (bool on)
{
    bool was_active = active_;

    Source::setActive(on);

    // change status of media player (only if status changed)
    if ( active_ != was_active ) {
        stream_->enable(active_);
    }
}

void PatternSource::update(float dt)
{
    Source::update(dt);

    // update stream
    // TODO : update only if animated pattern
    stream_->update();
}

void PatternSource::render()
{
    if (!initialized_)
        init();
    else {
        // render the media player into frame buffer
        static glm::mat4 projection = glm::ortho(-1.f, 1.f, 1.f, -1.f, -1.f, 1.f);
        renderbuffer_->begin();
        patternsurface_->draw(glm::identity<glm::mat4>(), projection);
        renderbuffer_->end();
    }
}

void PatternSource::accept(Visitor& v)
{
    Source::accept(v);
    v.visit(*this);
}
