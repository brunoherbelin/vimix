/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2023 Bruno Herbelin <bruno.herbelin@gmail.com>
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
#include <cmath>

#include "defines.h"
#include "Timeline.h"

float clamp(float x, float lowerlimit = 0.0f, float upperlimit = 1.0f) {
    if (x < lowerlimit) return lowerlimit;
    if (x > upperlimit) return upperlimit;
    return x;
}

float smootherstep(size_t edge0, size_t edge1, size_t i) {
    float x = clamp( static_cast<float>(i - edge0) / static_cast<float>(edge1 - edge0));
    return x * x * x * (x * (6.0f * x - 15.0f) + 10.0f);
    // return x * x * (3.0f - 2.0f * x);
}

float linestep(size_t edge0, size_t edge1, size_t i) {
    float x = clamp( static_cast<float>(i - edge0) / static_cast<float>(edge1 - edge0));
    return x;
}




static float empty_zeros[MAX_TIMELINE_ARRAY] = {};
static float empty_ones[MAX_TIMELINE_ARRAY] = {};

struct includesTime
{
    inline bool operator()(const TimeInterval s) const
    {
       return s.includes(_t);
    }

    explicit includesTime(GstClockTime t) : _t(t) { }

private:
    GstClockTime _t;
};

Timeline::Timeline()
{
    reset();
}

Timeline::~Timeline()
{
    reset();
}

Timeline& Timeline::operator = (const Timeline& b)
{
    if (this != &b) {
        this->timing_ = b.timing_;
        if (b.step_ != GST_CLOCK_TIME_NONE)
            this->step_ = b.step_;
        if (b.first_ != GST_CLOCK_TIME_NONE)
            this->first_ = b.first_;
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
    first_ = GST_CLOCK_TIME_NONE;
    step_ = GST_CLOCK_TIME_NONE;

    clearGaps();
    clearFading();
}

bool Timeline::is_valid() const
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
    first_ = GST_CLOCK_TIME_NONE;
    if (step != GST_CLOCK_TIME_NONE)
        step_ = step;
}

GstClockTime Timeline::next(GstClockTime time) const
{
    GstClockTime next_time = time;

    TimeInterval gap;
    if (getGapAt(time, gap) && gap.is_valid())
        next_time = gap.end;

    if ( first_ != GST_CLOCK_TIME_NONE && next_time < first_ )
        next_time = first_;

    return next_time;
}

GstClockTime Timeline::previous(GstClockTime time) const
{
    GstClockTime prev_time = time;
    TimeInterval gap;
    if (getGapAt(time, gap) && gap.is_valid())
        prev_time = gap.begin;

    if ( first_ != GST_CLOCK_TIME_NONE && prev_time < first_ )
        prev_time = first_;

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

void Timeline::refresh()
{
    fillArrayFromGaps(gapsArray_, MAX_TIMELINE_ARRAY);
}

bool Timeline::gapAt(const GstClockTime t) const
{
    TimeIntervalSet::const_iterator g = std::find_if(gaps_.begin(), gaps_.end(), includesTime(t));
    return ( g != gaps_.end() );
}

bool Timeline::getGapAt(const GstClockTime t, TimeInterval &gap) const
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

bool Timeline::cut(GstClockTime t, bool left, bool join_extremity)
{
    bool ret = false;

    if (timing_.includes(t))
    {
        TimeIntervalSet::iterator gap = std::find_if(gaps_.begin(), gaps_.end(), includesTime(t));

        // cut left part
        if (left) {
            // cut a gap
            if ( gap != gaps_.end() )
            {
                GstClockTime b = gap->begin;
                gaps_.erase(gap);
                ret = addGap(b, t);
            }
            // create a gap
            else {
                auto previous = gaps_.end();
                for (auto g = gaps_.begin(); g != gaps_.end(); previous = g++) {
                    if ( g->begin > t)
                        break;
                }
                if (join_extremity) {
                    if (previous != gaps_.end()) {
                        for (auto g = gaps_.begin(); g != gaps_.find(*previous); ) {
                            g = gaps_.erase(g);
                        }
                    }
                    ret = addGap( TimeInterval(t, timing_.begin) );
                }
                else {
                    if (previous == gaps_.end())
                        ret = addGap( TimeInterval(timing_.begin, t) );
                    else {
                        GstClockTime b = previous->begin;
                        gaps_.erase(previous);
                        ret = addGap( TimeInterval(b, t) );
                    }
                }
            }
        }
        // cut right part
        else {
            // cut a gap
            if ( gap != gaps_.end() )
            {
                GstClockTime e = gap->end;
                gaps_.erase(gap);
                ret = addGap(t, e);
            }
            // create a gap
            else {
                auto suivant = gaps_.rend();
                for (auto g = gaps_.rbegin(); g != gaps_.rend(); suivant = g++) {
                    if ( g->end < t)
                        break;
                }
                if (join_extremity) {
                    if (suivant != gaps_.rend()) {
                        for (auto g = gaps_.find(*suivant); g != gaps_.end(); ) {
                            g = gaps_.erase(g);
                        }
                    }
                    ret = addGap( TimeInterval(t, timing_.end) );
                }
                else {
                    if (suivant == gaps_.rend())
                        ret = addGap( TimeInterval(t, timing_.end) );
                    else {
                        GstClockTime e = suivant->end;
                        gaps_.erase( gaps_.find(*suivant));
                        ret = addGap( TimeInterval(t, e) );
                    }
                }
            }
        }
    }

    return ret;
}

bool Timeline::addGap(TimeInterval s)
{
    if ( s.is_valid() ) {
        gaps_array_need_update_ = true;
        return gaps_.insert(s).second;
    }

    return false;
}

void Timeline::setGaps(const TimeIntervalSet &g)
{
    gaps_array_need_update_ = true;
    gaps_ = g;
}

bool Timeline::removeGaptAt(GstClockTime t)
{
    if (gaps_.empty())
        return false;

    TimeIntervalSet::const_iterator s = std::find_if(gaps_.begin(), gaps_.end(), includesTime(t));

    if ( s != gaps_.end() ) {
        gaps_.erase(s);
        gaps_array_need_update_ = true;
        return true;
    }

    return false;
}

bool Timeline::mergeGapstAt(GstClockTime t)
{
    if (gaps_.empty())
        return false;

    auto first = gaps_.end();
    auto second = gaps_.end();

    for (auto g = gaps_.begin(); g != gaps_.end(); g++) {
        if (g->includes(t)) {
            return false;
        } else if (t > g->end) {
            first = g;
        } else if (t < g->begin) {
            second = g;
            break;
        }
    }

    if (first != gaps_.end() && second != gaps_.end()) {
        gaps_.erase(first);
        gaps_.erase(second);
        addGap(first->begin, second->end);
    }
    else if (second != gaps_.end()) {
        gaps_.erase(second);
        addGap(timing_.begin, second->end);
    }
    else if (first != gaps_.end()) {
        gaps_.erase(first);
        addGap(first->begin, timing_.end);
    }

    return gaps_array_need_update_;
}

GstClockTime Timeline::sectionsDuration() const
{
    // compute the sum of durations of gaps
    GstClockTime d = 0;
    for (auto g = gaps_.begin(); g != gaps_.end(); ++g)
        d+= g->duration();

    // remove sum of gaps from actual duration
    return duration() - d;
}


GstClockTime Timeline::sectionsTimeAt(GstClockTime t) const
{
    // loop over gaps
    GstClockTime d = t;
    for (auto g = gaps_.begin(); g != gaps_.end(); ++g) {
        // gap before target?
        if ( g->begin < d ) {
            // increment target
            d += g->duration();
        }
        else
            // done
            break;
    }

    // return updated target
    return d;
}

GstClockTime Timeline::timeFromPercent(const float p) const
{
    if (!is_valid())
        return 0;

    // compute time at p % of actural section duration
    GstClockTime percent = (double) p * 100000;
    GstClockTime d = (sectionsDuration() * percent ) / 100000;

    // loop over gaps
    for (auto g = gaps_.begin(); g != gaps_.end(); ++g) {
        // gap before target?
        if ( g->begin < d ) {
            // jump over gap
            d += g->duration();
        }
        else
            // done
            break;
    }

    // return time corresponding to p% of play duration
    return d;
}


size_t Timeline::fillSectionsArrays( float* const gaps, float* const fading)
{
    size_t arraysize = MAX_TIMELINE_ARRAY;
    float* gapsptr = gaps;
    float* fadingptr = fading;

    if (gaps_array_need_update_)
        fillArrayFromGaps(gapsArray_, MAX_TIMELINE_ARRAY);

    if (gaps_.size() > 0) {

        // indices to define [s e[] sections
        size_t s = 0, e;
        arraysize = 0;

        auto it = gaps_.begin();

        // cut at the beginning
        if ((*it).begin == timing_.begin) {
            for (size_t i = 0; i < 5; ++i)
                gapsptr[ MAX(i, 0) ] = 1.f;

            s = ( (*it).end * MAX_TIMELINE_ARRAY ) / timing_.end;
            ++it;
        }

        // loop
        for (; it != gaps_.end(); ++it) {

            // [s e] section
            e = ( (*it).begin * MAX_TIMELINE_ARRAY ) / timing_.end;

            size_t n = e - s;
            memcpy( gapsptr, gapsArray_ + s, n  * sizeof(float));
            memcpy( fadingptr, fadingArray_ + s, n * sizeof(float));

            for (size_t i = -5; i > 0; ++i)
                gapsptr[ MAX(n+i, 0) ] = 1.f;

            gapsptr += n;
            fadingptr += n;
            arraysize += n;

            // next section
            s = ( (*it).end * MAX_TIMELINE_ARRAY ) / timing_.end;
        }

        // close last [s e] section
        if (s != MAX_TIMELINE_ARRAY) {

            // [s e] section
            e = MAX_TIMELINE_ARRAY;

            size_t n = e - s;
            memcpy( gapsptr, gapsArray_ + s, n * sizeof(float));
            memcpy( fadingptr, fadingArray_ + s, n * sizeof(float));
            arraysize += n;
        }

    }
    else {

        memcpy( gaps, gapsArray_, MAX_TIMELINE_ARRAY * sizeof(float));
        memcpy( fading, fadingArray_, MAX_TIMELINE_ARRAY * sizeof(float));
    }

    return arraysize;
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

        for (; it != gaps_.end(); ++it) {
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

float Timeline::fadingAt(const GstClockTime t) const
{
    double true_index = (static_cast<double>(MAX_TIMELINE_ARRAY) * static_cast<double>(t)) / static_cast<double>(timing_.end);
    double previous_index = floor(true_index);
    float percent = static_cast<float>(true_index - previous_index);
    size_t keyframe_index = MINI( static_cast<size_t>(previous_index), MAX_TIMELINE_ARRAY-1);
    size_t keyframe_next_index = MINI( keyframe_index+1, MAX_TIMELINE_ARRAY-1);
    float v = fadingArray_[keyframe_index];
    v += percent * (fadingArray_[keyframe_next_index] - fadingArray_[keyframe_index]);

    return v;
}

size_t Timeline::fadingIndexAt(const GstClockTime t) const
{
    double true_index = (static_cast<double>(MAX_TIMELINE_ARRAY) * static_cast<double>(t)) / static_cast<double>(timing_.end);
    double previous_index = floor(true_index);
    return  MINI( static_cast<size_t>(previous_index), MAX_TIMELINE_ARRAY-1);
}

void Timeline::clearFading()
{
    // fill static with 1 (only once)
    if (empty_ones[0] < 1.f){
        for(int i=0;i<MAX_TIMELINE_ARRAY;++i)
            empty_ones[i] = 1.f;
    }
    // clear with static array
    memcpy(fadingArray_, empty_ones, MAX_TIMELINE_ARRAY * sizeof(float));
}

void Timeline::smoothFading(uint N, TimeInterval interval)
{
    const float kernel[7] = { 2.f, 22.f, 97.f, 159.f, 97.f, 22.f, 2.f};
    float tmparray[MAX_TIMELINE_ARRAY];

    // default to cover entire array
    long s = 0;
    long e = MAX_TIMELINE_ARRAY;

    // if a valid interval is given in argument
    if (interval.is_valid()) {

        // get index of begining of interval
        s = (interval.begin * MAX_TIMELINE_ARRAY) / timing_.end;
        // get index of ending of interval
        e = (interval.end * MAX_TIMELINE_ARRAY) / timing_.end;

        // iterate a given amount of times
        for (uint n = 0; n < N; ++n) {
            // copy to tmp array
            memcpy(tmparray, fadingArray_, MAX_TIMELINE_ARRAY * sizeof(float));
            // apply gaussian filter on the interval
            for (long i = s; i < e; ++i) {
                tmparray[i] = 0.f;
                float divider = 0.f;
                for (long j = 0; j < 7; ++j) {
                    long k = i - 3 + j;
                    if (k > -1 && k < MAX_TIMELINE_ARRAY - 1) {
                        tmparray[i] += fadingArray_[k] * kernel[j];
                        divider += kernel[j];
                    }
                }
                tmparray[i] *= 1.f / divider;
            }
            // copy back to array
            memcpy(fadingArray_, tmparray, MAX_TIMELINE_ARRAY * sizeof(float));
        }
    }
    // in absence of interval given, loop over all sections
    else {
        TimeIntervalSet _intervals = sections();
        for (auto inter = _intervals.begin(); inter != _intervals.end(); inter++) {
            smoothFading(N, *inter);
        }
    }
}


void Timeline::autoFading(const GstClockTime duration, FadingCurve curve)
{
    // mow many index values of timeline array for this duration?
    size_t N = MAX_TIMELINE_ARRAY -1;
    if ( duration < timing_.end )
        N = duration / (timing_.end / MAX_TIMELINE_ARRAY);

    // clear with static array
    memcpy(fadingArray_, empty_zeros, MAX_TIMELINE_ARRAY * sizeof(float));

    // get sections (inverse of gaps)
    TimeIntervalSet sec = sections();

    // fading for each section
    // NB : there is at least one
    for (auto it = sec.begin(); it != sec.end(); ++it)
    {
        // get index of begining of section
        const size_t s = ( (*it).begin * MAX_TIMELINE_ARRAY ) / timing_.end;

        // get index of ending of section
        const size_t e = ( ( (*it).end * MAX_TIMELINE_ARRAY ) / timing_.end ) -1;

        // calculate size of the smooth transition in [s e] interval
        const size_t n = MIN( (e-s)/2, N );

        // linear fade in starting at s
        size_t i = s;
        for (; i < s+n; ++i){
            const float x = static_cast<float>(i-s) / static_cast<float>(n);
            if (curve==FADING_LINEAR)
                fadingArray_[i] = x * x;
            else if (curve==FADING_SMOOTH)
                fadingArray_[i] = 1.f - ((x- 1.f) * (x - 1.f));
            else
                fadingArray_[i] = x;
        }
        // plateau
        for (; i < e-n; ++i)
            fadingArray_[i] = 1.f;
        // linear fade out ending at e
        for (; i < e; ++i) {
            const float x = static_cast<float>(e-i) / static_cast<float>(n);
            if (curve==FADING_LINEAR)
                fadingArray_[i] = x * x;
            else if (curve==FADING_SMOOTH)
                fadingArray_[i] = 1.f - ((x- 1.f) * (x - 1.f));
            else
                fadingArray_[i] = x;
        }
    }
}

void Timeline::fadeOut(const GstClockTime from, const GstClockTime duration, FadingCurve curve)
{
    GstClockTime to = from + duration;

    if (duration > timing_.end) {
        to = timing_.end;
        for (auto g = gaps_.begin(); g != gaps_.end(); ++g) {
            // gap after target?
            if (g->begin > from) {
                to = g->begin;
                break;
            }
        }
    }

    // get index of begining of fading curve
    const size_t s = ( from * MAX_TIMELINE_ARRAY ) / timing_.end;

    // get index of ending of fading curve
    const size_t e = ( to * MAX_TIMELINE_ARRAY ) / timing_.end;

    // how many index values of timeline array for this duration?
    size_t N = MAX_TIMELINE_ARRAY - 1;
    N = MIN( ( MIN( duration, timing_.end - from ) * MAX_TIMELINE_ARRAY )/ timing_.end, N );

    // calculate size of the smooth transition in [s e] interval
    const size_t n = MIN( e-s, N );

    // if transition too short for a linear or smooth
    if (N < 3) {
        curve = FADING_SHARP;
        N = 2;
    }

    // linear fade out starts at s
    size_t i = s;
    // float val = fadingArray_[s];
    for (; i <= s+n; ++i) {
        const float x = static_cast<float>(e-i) / static_cast<float>(n);
        // const float x = 1.f - static_cast<float>(i-s) / static_cast<float>(n);
        if (curve==FADING_LINEAR)
            fadingArray_[i] = x;
        else if (curve==FADING_SMOOTH)
            fadingArray_[i] = 1.f - smootherstep(s, s+n, i);
        else
            fadingArray_[i] = 0.f;
        // fadingArray_[i] *= val;
    }

}

void Timeline::fadeIn(const GstClockTime to, const GstClockTime duration, FadingCurve curve)
{
    GstClockTime from = to - duration;

    if (duration > timing_.end) {
        for (auto g = gaps_.begin(); g != gaps_.end(); ++g) {
            // gap before target?
            if ( g->end < to )
                from = g->end;
            else
                break;
        }
    }

    // get index of begining of fading curve
    const size_t s = ( from * MAX_TIMELINE_ARRAY ) / timing_.end;

    // get index of ending of fading curve
    const size_t e = ( to * MAX_TIMELINE_ARRAY ) / timing_.end;

    // how many index values of timeline array for this duration?
    size_t N = MAX_TIMELINE_ARRAY - 1;
    N = MIN( ( MIN( duration, to - timing_.begin ) * MAX_TIMELINE_ARRAY )/ timing_.end, N );

    // if transition too short for a linear or smooth
    if (N < 3) {
        curve = FADING_SHARP;
        N = 2;
    }

    // calculate size of the smooth transition in [s e] interval
    const size_t n = MIN( e-s, N );

    // linear fade in ends at e
    size_t i = e-n;
    // float val = fadingArray_[e];
    for (; i < e; ++i) {
        // const float x = static_cast<float>(i-s) / static_cast<float>(n);
        const float x = 1.f - static_cast<float>(e - i) / static_cast<float>(n);
        if (curve==FADING_LINEAR)
            fadingArray_[i] = x;
        else if (curve==FADING_SMOOTH)
            fadingArray_[i] = smootherstep(e-n, e, i);
        else
            fadingArray_[i] = 0.f;
        // fadingArray_[i] *= val;
    }

}

void Timeline::fadeInOutRange(const GstClockTime t, const GstClockTime duration, bool in_and_out, FadingCurve curve)
{
    // init range to whole timeline
    TimeInterval range = timing_;

    // find cuts at left and right of given time
    for (auto g = gaps_.begin(); g != gaps_.end(); g++) {
        if (g->begin < t) {
            if (g->end > t) {
                // inside a gap
                range.begin = g->begin;
                range.end = g->end;
                break;
            } else {
                // after a gap
                range.begin = g->end;
            }
        } else {
            // before a gap
            range.end = g->begin;
            break;
        }
    }

    // get index of begining of section
    const size_t s = (range.begin * MAX_TIMELINE_ARRAY) / timing_.end;

    // get index of ending of section
    const size_t e = (range.end * MAX_TIMELINE_ARRAY) / timing_.end;

    // get index of time in section
    const size_t m = (t * MAX_TIMELINE_ARRAY) / timing_.end;

    size_t l = m;
    size_t r = m;

    // if duration too short for a linear or smooth
    if (duration < 2 * step_) {
        curve = FADING_SHARP;
    }
    // if duration allows to fade in and out
    else if (2 * duration < range.duration()) {
        l = ( (range.begin + duration) * MAX_TIMELINE_ARRAY) / timing_.end;
        r = ( (range.end - duration) * MAX_TIMELINE_ARRAY) / timing_.end;
    }

    // fill values inside range
    for (size_t k = s; k < e; ++k) {

        if (curve == FADING_LINEAR){
            if (k<l)
                fadingArray_[k] = in_and_out ? linestep(s, l, k) : 1.f - linestep(s, l, k);
            else if (k<r)
                fadingArray_[k] = in_and_out  ? 1.f : 0.f;
            else
                fadingArray_[k] = in_and_out  ? 1.f - linestep(r, e, k) : linestep(r, e, k);
        }
        else if (curve == FADING_SMOOTH) {
            if (k<l)
                fadingArray_[k] = in_and_out ? smootherstep(s, l, k) : 1.f - smootherstep(s, l, k);
            else if (k<r)
                fadingArray_[k] = in_and_out  ? 1.f : 0.f;
            else
                fadingArray_[k] = in_and_out ? 1.f - smootherstep(r, e, k) : smootherstep(r, e, k);
        }
        else // curve == FADING_SHARP
            fadingArray_[k] = in_and_out;
    }

}


bool Timeline::autoCut()
{
    bool changed = false;
    for (long i = 0; i < MAX_TIMELINE_ARRAY; ++i) {
        if (fadingArray_[i] < EPSILON) {
            if (gapsArray_[i] != 1.f)
                changed = true;
            gapsArray_[i] = 1.f;
        }
    }

    updateGapsFromArray(gapsArray_, MAX_TIMELINE_ARRAY);
    gaps_array_need_update_ = false;

    return changed;
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
                GstClockTime t = (timing_.duration() * i) / (array_size-1);
                // change from 0.f to 1.f : begin of a gap
                if (array[i] > 0.f) {
                    begin_gap = t;
                }
                // change from 1.f to 0.f : end of a gap
                else {
                    TimeInterval d (begin_gap, t) ;
                    if (d.is_valid() && d.duration() > step_)
                        addGap(d);
                    begin_gap = GST_CLOCK_TIME_NONE;
                }
                // swap
                status = array[i];
            }
        }
        // end a potentially pending gap if reached end of array with no end of gap
        if (begin_gap != GST_CLOCK_TIME_NONE) {
            TimeInterval d (begin_gap, timing_.end) ;
            if (d.is_valid() && d.duration() > step_)
                addGap(d);
        }

    }

}

void Timeline::fillArrayFromGaps(float *array, size_t array_size)
{
    // fill the array from gaps
    if (array != nullptr && array_size > 0 && timing_.is_valid()) {

        // clear with static array
        memcpy(gapsArray_, empty_zeros, MAX_TIMELINE_ARRAY * sizeof(float));

        // for each gap
        GstClockTime d = timing_.duration();
        for (auto it = gaps_.begin(); it != gaps_.end(); ++it)
        {
            size_t s = ( (*it).begin * array_size ) / d;
            size_t e = ( (*it).end * array_size ) / d;

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
