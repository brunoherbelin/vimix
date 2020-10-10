
#include <algorithm>
#include <cmath>

#include "defines.h"
#include "Log.h"
#include "Timeline.h"

struct includesTime: public std::unary_function<TimeInterval, bool>
{
    inline bool operator()(const TimeInterval s) const
    {
       return s.includes(_t);
    }

    includesTime(GstClockTime t) : _t(t) { }

private:
    GstClockTime _t;
};

Timeline::Timeline()
{
    reset();
}

Timeline::~Timeline()
{
}

Timeline& Timeline::operator = (const Timeline& b)
{
    if (this != &b) {
        this->timing_ = b.timing_;
        this->step_ = b.step_;
        this->gaps_ = b.gaps_;
        this->gaps_array_need_update_ = b.gaps_array_need_update_;
        memcpy( this->gapsArray_, b.gapsArray_, MAX_TIMELINE_ARRAY * sizeof(float));
        memcpy( this->fadingArray_, b.fadingArray_, MAX_TIMELINE_ARRAY * sizeof(float));
    }
    return *this;
}

void Timeline::reset()
{
    // reset timing
    timing_.reset();
    timing_.begin = 0;
    first_ = 0;
    step_ = GST_CLOCK_TIME_NONE;

    clearGaps();
    clearFading();
}

bool Timeline::is_valid()
{
    return timing_.is_valid() && step_ != GST_CLOCK_TIME_NONE;
}

void Timeline::setFirst(GstClockTime first)
{
    first_ = first;
}

void Timeline::setEnd(GstClockTime end)
{
    timing_.end = end;
}

void Timeline::setStep(GstClockTime dt)
{
    step_ = dt;
}

void Timeline::setTiming(TimeInterval interval, GstClockTime step)
{
    timing_ = interval;
    if (step != GST_CLOCK_TIME_NONE)
        step_ = step;
}

GstClockTime Timeline::next(GstClockTime time) const
{
    GstClockTime next_time = time;

    TimeInterval gap;
    if (gapAt(time, gap) && gap.is_valid())
        next_time = gap.end;

    return next_time;
}

GstClockTime Timeline::previous(GstClockTime time) const
{
    GstClockTime prev_time = time;
    TimeInterval gap;
    if (gapAt(time, gap) && gap.is_valid())
        prev_time = gap.begin;

    return prev_time;
}

float *Timeline::gapsArray()
{
    if (gaps_array_need_update_) {
        fillArrayFromGaps(gapsArray_, MAX_TIMELINE_ARRAY);
    }
    return gapsArray_;
}

void Timeline::update()
{
    updateGapsFromArray(gapsArray_, MAX_TIMELINE_ARRAY);
    gaps_array_need_update_ = false;
}

bool Timeline::gapAt(const GstClockTime t, TimeInterval &gap) const
{
    TimeIntervalSet::const_iterator g = std::find_if(gaps_.begin(), gaps_.end(), includesTime(t));

    if ( g != gaps_.end() ) {
        gap = (*g);
        return true;
    }

    return false;
}

bool Timeline::addGap(GstClockTime begin, GstClockTime end)
{
    return addGap( TimeInterval(begin, end) );
}

bool Timeline::addGap(TimeInterval s)
{
    if ( s.is_valid() ) {
        gaps_array_need_update_ = true;
        return gaps_.insert(s).second;
    }

    return false;
}

void Timeline::setGaps(TimeIntervalSet g)
{
    gaps_array_need_update_ = true;
    gaps_ = g;
}

bool Timeline::removeGaptAt(GstClockTime t)
{
    TimeIntervalSet::const_iterator s = std::find_if(gaps_.begin(), gaps_.end(), includesTime(t));

    if ( s != gaps_.end() ) {
        gaps_.erase(s);
        gaps_array_need_update_ = true;
        return true;
    }

    return false;
}


TimeIntervalSet Timeline::sections() const
{
    TimeIntervalSet sec;

    GstClockTime begin_sec = timing_.begin;

    if (gaps_.size() > 0) {

        auto it = gaps_.begin();
        if ((*it).begin == begin_sec) {
            begin_sec = (*it).end;
            ++it;
        }

        for (; it != gaps_.end(); ++it)
        {
            sec.insert( TimeInterval(begin_sec, (*it).begin) );
            begin_sec = (*it).end;
        }

    }

    if (begin_sec != timing_.end)
        sec.insert( TimeInterval(begin_sec, timing_.end) );

    return sec;
}

void Timeline::clearGaps()
{
    gaps_.clear();

    for(int i=0;i<MAX_TIMELINE_ARRAY;++i)
        gapsArray_[i] = 0.f;

    gaps_array_need_update_ = true;
}

float Timeline::fadingAt(const GstClockTime t)
{
    double true_index = (static_cast<double>(MAX_TIMELINE_ARRAY) * static_cast<double>(t)) / static_cast<double>(timing_.end);
    double previous_index = floor(true_index);
    float percent = static_cast<float>(true_index - previous_index);
    size_t keyframe_index = CLAMP( static_cast<size_t>(previous_index), 0, MAX_TIMELINE_ARRAY-1);
    size_t keyframe_next_index = CLAMP( keyframe_index+1, 0, MAX_TIMELINE_ARRAY-1);
    float v = fadingArray_[keyframe_index];
    v += percent * (fadingArray_[keyframe_next_index] - fadingArray_[keyframe_index]);

    return v;
}

void Timeline::clearFading()
{
    for(int i=0;i<MAX_TIMELINE_ARRAY;++i)
        fadingArray_[i] = 1.f;
}

void Timeline::smoothFading(uint N)
{
    const float kernel[7] = { 2.f, 22.f, 97.f, 159.f, 97.f, 22.f, 2.f};
    float tmparray[MAX_TIMELINE_ARRAY];

    for (uint n = 0; n < N; ++n) {

        for (long i = 0; i < MAX_TIMELINE_ARRAY; ++i) {
            tmparray[i] = 0.f;
            float divider = 0.f;
            for( long j = 0; j < 7; ++j) {
                long k = i - 3 + j;
                if (k > -1 && k < MAX_TIMELINE_ARRAY-1) {
                    tmparray[i] += fadingArray_[k] * kernel[j];
                    divider += kernel[j];
                }
            }
            tmparray[i] *= 1.f / divider;
        }

        memcpy( fadingArray_, tmparray, MAX_TIMELINE_ARRAY * sizeof(float));
    }
}


void Timeline::autoFading(uint milisecond)
{

    GstClockTime stepduration = timing_.end / MAX_TIMELINE_ARRAY;
    stepduration = GST_TIME_AS_MSECONDS(stepduration);
    uint N = milisecond / stepduration;

    // reset all to zero
    for(int i=0;i<MAX_TIMELINE_ARRAY;++i)
        fadingArray_[i] = 0.f;

    // get sections (inverse of gaps)
    TimeIntervalSet sec = sections();

    // fading for each section
    // NB : there is at least one
    for (auto it = sec.begin(); it != sec.end(); ++it)
    {
        // get index of begining of section
        size_t s = ( (*it).begin * MAX_TIMELINE_ARRAY ) / timing_.end;
        // get index of ending of section
        size_t e = ( (*it).end * MAX_TIMELINE_ARRAY ) / timing_.end;

        // calculate size of the smooth transition in [s e] interval
        uint n = MIN( (e-s)/3, N );

        // linear fade in starting at s
        size_t i = s;
        for (; i < s+n; ++i)
            fadingArray_[i] = static_cast<float>(i-s) / static_cast<float>(n);
        // plateau
        for (; i < e-n; ++i)
            fadingArray_[i] = 1.f;
        // linear fade out ending at e
        for (; i < e; ++i)
            fadingArray_[i] = static_cast<float>(e-i) / static_cast<float>(n);
    }

}

void Timeline::updateGapsFromArray(float *array, size_t array_size)
{
    // reset gaps
    gaps_.clear();

    // fill the gaps from array
    if (array != nullptr && array_size > 0 && timing_.is_valid()) {

        // loop over the array to detect gaps
        float status = 0.f;
        GstClockTime begin_gap = GST_CLOCK_TIME_NONE;
        for (size_t i = 0; i < array_size; ++i) {
            // detect a change of value between two slots
            if ( array[i] != status) {
                // compute time of the event in array
                GstClockTime t = (timing_.duration() * i) / array_size;
                // change from 0.f to 1.f : begin of a gap
                if (array[i] > 0.f) {
                    begin_gap = t;
                }
                // change from 1.f to 0.f : end of a gap
                else {
                    addGap( begin_gap, t );
                    begin_gap = GST_CLOCK_TIME_NONE;
                }
                // swap
                status = array[i];
            }
        }
        // end a potentially pending gap if reached end of array with no end of gap
        if (begin_gap != GST_CLOCK_TIME_NONE)
            addGap( begin_gap, timing_.end );

    }
}

void Timeline::fillArrayFromGaps(float *array, size_t array_size)
{
    // fill the array from gaps
    if (array != nullptr && array_size > 0 && timing_.is_valid()) {

        for(int i=0;i<array_size;++i)
            gapsArray_[i] = 0.f;

        // for each gap
        for (auto it = gaps_.begin(); it != gaps_.end(); ++it)
        {
            size_t s = ( (*it).begin * array_size ) / timing_.end;
            size_t e = ( (*it).end * array_size ) / timing_.end;

            // fill with 1 where there is a gap
            for (size_t i = s; i < e; ++i) {
                gapsArray_[i] = 1.f;
            }
        }

        gaps_array_need_update_ = false;
    }

    //        // NB: less efficient algorithm :
    //        TimeInterval gap;
    //        for (size_t i = 0; i < array_size; ++i) {
    //            GstClockTime t = (timing_.duration() * i) / array_size;
    //            array[i] = gapAt(t, gap) ? 1.f : 0.f;
    //        }
}
