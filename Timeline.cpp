
#include <algorithm>


#include "defines.h"
#include "Log.h"
#include "Timeline.h"

#define SEGMENT_ARRAY_MAX_SIZE 1000

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
    }
    return *this;
}

void Timeline::reset()
{
    // reset timing
    timing_.reset();
    step_ = GST_CLOCK_TIME_NONE;

    // clear gaps
    gaps_.clear();
}

bool Timeline::is_valid()
{
    return timing_.is_valid() && step_ != GST_CLOCK_TIME_NONE;
}

void Timeline::setStart(GstClockTime start)
{
    timing_.begin = start;
}

void Timeline::setEnd(GstClockTime end)
{
    timing_.end = end;
}

void Timeline::setStep(GstClockTime dt)
{
    step_ = dt;
}

void Timeline::updateGapsFromArray(float *array_, size_t array_size_)
{
    // reset gaps
    gaps_.clear();

    // loop over the array to detect gaps
    float status = 1.f;
    GstClockTime begin_gap = GST_CLOCK_TIME_NONE;
    for (size_t i = 0; i < array_size_; ++i) {
        // detect a change of value between two slots
        if ( array_[i] != status) {
            // compute time of the event in array
            GstClockTime t = (timing_.duration() * i) / array_size_;
            // change from 1.f to 0.f : begin of a gap
            if (status) {
                begin_gap = t;
            }
            // change from 0.f to 1.f : end of a gap
            else {
                addGap( begin_gap, t );
                begin_gap = GST_CLOCK_TIME_NONE;
            }
            // swap
            status = array_[i];
        }
    }
    // end a potentially pending gap if reached end of array with no end of gap
    if (begin_gap != GST_CLOCK_TIME_NONE)
        addGap( begin_gap, timing_.end );

}

void Timeline::fillArrayFromGaps(float *array_, size_t array_size_)
{
    if (array_ != nullptr && array_size_ > 0 && timing_.is_valid()) {

        // fill the array from gaps
        // NB: this is the less efficient algorithm possible! but we should not keep arrays anyway
        // TODO : implement an ImGui widget to plot a timeline instead of an array
        TimeInterval gap;
        for (size_t i = 0; i < array_size_; ++i) {
            GstClockTime t = (timing_.duration() * i) / array_size_;
            array_[i] = gapAt(t, gap) ? 0.f : 1.f;
        }

    }
}

size_t Timeline::numGaps()
{
    return gaps_.size();
}

bool Timeline::gapAt(const GstClockTime t, TimeInterval &gap)
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
    if ( s.is_valid() )
        return gaps_.insert(s).second;

    return false;
}

void Timeline::toggleGaps(GstClockTime from, GstClockTime to)
{
    TimeInterval interval(from, to);
    TimeInterval gap;
    bool is_a_gap_ = gapAt(from, gap);

    if (interval.is_valid())
    {
        // fill gap
        if (is_a_gap_) {

        }
        // else create gap (from time is not in a gap)
        else {

            TimeIntervalSet::iterator g = std::find_if(gaps_.begin(), gaps_.end(), includesTime(to));
            // if there is a gap overlap with [from to] (or [to from]), expand the gap
            if ( g != gaps_.end() ) {
                interval.begin = MIN( (*g).begin, interval.begin);
                interval.end   = MAX( (*g).end , interval.end);
                gaps_.erase(g);
            }
            //  add the new gap
            addGap(interval);
            Log::Info("add gap [ %ld  %ld ]", interval.begin, interval.end);
        }


    }
    Log::Info("%d gaps in timeline", numGaps());
}

bool Timeline::removeGaptAt(GstClockTime t)
{
    TimeIntervalSet::const_iterator s = std::find_if(gaps_.begin(), gaps_.end(), includesTime(t));

    if ( s != gaps_.end() ) {
        gaps_.erase(s);
        return true;
    }

    return false;
}

void Timeline::clearGaps()
{
    gaps_.clear();
}

std::list< std::pair<guint64, guint64> > Timeline::gaps() const
{
    std::list< std::pair<guint64, guint64> > ret;
    for (TimeIntervalSet::iterator it = gaps_.begin(); it != gaps_.end(); it++)
        ret.push_back( std::make_pair( it->begin, it->end ) );

    return ret;
}

