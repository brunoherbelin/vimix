
#include <algorithm>


#include "defines.h"
#include "Log.h"
#include "Timeline.h"

#define SEGMENT_ARRAY_MAX_SIZE 1000


Timeline::Timeline() : array_(nullptr)
{
    reset();
}

Timeline::~Timeline()
{
    reset();
//    free(array_);
}

void Timeline::reset()
{
    // reset timing
    timing_.begin = GST_CLOCK_TIME_NONE;
    timing_.end = GST_CLOCK_TIME_NONE;
    step_ = GST_CLOCK_TIME_NONE;

//    // clear gaps
//    gaps_.clear();

//    // clear segment array
//    if (array_ != nullptr)
//        free(array_);

//    // avoid crash by providing a valid pointer
//    array_size_ = 1;
//    array_ =  (float *) malloc(sizeof(float));
//    array_[0] = 1.f;
//    need_update_ = true;
}

void Timeline::setStart(GstClockTime start)
{
    timing_.begin = start;
    need_update_ = true;
}

void Timeline::setEnd(GstClockTime end)
{
    timing_.end = end;
    need_update_ = true;
}

void Timeline::setStep(GstClockTime dt)
{
    step_ = dt;
    need_update_ = true;
}

void Timeline::updateGapsFromArray()
{
//    if (need_update_)
//        return;

//    // reset gaps
//    gaps_.clear();

//    // loop over the array to detect gaps
//    float status = 1.f;
//    GstClockTime begin_gap = GST_CLOCK_TIME_NONE;
//    for (size_t i = 0; i < array_size_; ++i) {
//        // detect a change of value between two slots
//        if ( array_[i] != status) {
//            // compute time of the event in array
//            GstClockTime t = (timing_.duration() * i) / array_size_;
//            // change from 1.f to 0.f : begin of a gap
//            if (status) {
//                begin_gap = t;
//            }
//            // change from 0.f to 1.f : end of a gap
//            else {
//                addGap( begin_gap, t );
//                begin_gap = GST_CLOCK_TIME_NONE;
//            }
//            // swap
//            status = array_[i];
//        }
//    }
//    // end a potentially pending gap if reached end of array with no end of gap
//    if (begin_gap != GST_CLOCK_TIME_NONE)
//        addGap( begin_gap, timing_.end );

}

void Timeline::updateArrayFromGaps()
{
//    if (step_ != GST_CLOCK_TIME_NONE && timing_.is_valid()) {

//        // clear segment array
//        if (array_ != nullptr)
//            free(array_);

//        array_size_ = MIN( SEGMENT_ARRAY_MAX_SIZE, duration() / step_);
//        array_ = (float *) malloc(array_size_ * sizeof(float));
//        for (int i = 0; i < array_size_; ++i)
//            array_[i] = 1.f;

//        Log::Info("%d frames in timeline", array_size_);

//        // fill the array from gaps
//        // NB: this is the less efficient algorithm possible! but we should not keep arrays anyway
//        // TODO : implement an ImGui widget to plot a timeline instead of an array
//        TimeInterval gap;
//        for (size_t i = 0; i < array_size_; ++i) {
//            GstClockTime t = (timing_.duration() * i) / array_size_;
//            array_[i] = gapAt(t, gap) ? 0.f : 1.f;
//        }

//        need_update_ = false;
//    }
}

size_t Timeline::numGaps()
{
    return gaps_.size();
}

bool Timeline::gapAt(const GstClockTime t, TimeInterval &gap)
{
//    TimeIntervalSet::const_iterator g = std::find_if(gaps_.begin(), gaps_.end(), includesTime(t));

//    if ( g != gaps_.end() ) {
//        gap = (*g);
//        return true;
//    }

    return false;
}

bool Timeline::addGap(GstClockTime begin, GstClockTime end)
{
    return addGap( TimeInterval(begin, end) );
}

bool Timeline::addGap(TimeInterval s)
{
//    if ( s.is_valid() ) {
//        need_update_ = true;
//        return gaps_.insert(s).second;
//    }

    return false;
}

bool Timeline::removeGaptAt(GstClockTime t)
{
//    TimeIntervalSet::const_iterator s = std::find_if(gaps_.begin(), gaps_.end(), includesTime(t));

//    if ( s != gaps_.end() ) {
//        gaps_.erase(s);
//        need_update_ = true;
//        return true;
//    }

    return false;
}

void Timeline::clearGaps()
{
    gaps_.clear();
    need_update_ = true;
}

std::list< std::pair<guint64, guint64> > Timeline::gaps() const
{
    std::list< std::pair<guint64, guint64> > ret;
    for (TimeIntervalSet::iterator it = gaps_.begin(); it != gaps_.end(); it++)
        ret.push_back( std::make_pair( it->begin, it->end ) );

    return ret;
}

float *Timeline::array()
{
//    if (need_update_)
//        updateArrayFromGaps();
    return array_;
}

size_t Timeline::arraySize()
{
//    if (need_update_)
//        updateArrayFromGaps();
    return array_size_;
}
