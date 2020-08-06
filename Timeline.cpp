
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
}

void Timeline::reset()
{
    start_ = GST_CLOCK_TIME_NONE;
    end_ = GST_CLOCK_TIME_NONE;
    num_frames_ = 0;
    array_size_ = 0;
    // clear segment array
    if (array_ != nullptr)
        free(array_);
}

void Timeline::init(GstClockTime start, GstClockTime end, GstClockTime frame_duration)
{
    reset();

    start_ = start;
    end_   = end;
    num_frames_ = (size_t) end_ / (size_t) frame_duration;

    array_size_ = MIN( SEGMENT_ARRAY_MAX_SIZE, num_frames_);
    array_ = (float *) malloc(array_size_ * sizeof(float));
    for (int i = 0; i < array_size_; ++i)
        array_[i] = 1.f;

    Log::Info("%d frames in timeline", array_size_);
}

bool Timeline::addPlaySegment(GstClockTime begin, GstClockTime end)
{
    return addPlaySegment( MediaSegment(begin, end) );
}

bool Timeline::addPlaySegment(MediaSegment s)
{
    if ( s.is_valid() )
        return segments_.insert(s).second;

    return false;
}

bool Timeline::removeAllPlaySegmentOverlap(MediaSegment s)
{
    bool ret = removePlaySegmentAt(s.begin);
    return removePlaySegmentAt(s.end) || ret;
}

bool Timeline::removePlaySegmentAt(GstClockTime t)
{
    MediaSegmentSet::const_iterator s = std::find_if(segments_.begin(), segments_.end(), containsTime(t));

    if ( s != segments_.end() ) {
        segments_.erase(s);
        return true;
    }

    return false;
}

std::list< std::pair<guint64, guint64> > Timeline::getPlaySegments() const
{
    std::list< std::pair<guint64, guint64> > ret;
    for (MediaSegmentSet::iterator it = segments_.begin(); it != segments_.end(); it++)
        ret.push_back( std::make_pair( it->begin, it->end ) );

    return ret;
}
