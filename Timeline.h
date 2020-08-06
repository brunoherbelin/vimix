#ifndef TIMELINE_H
#define TIMELINE_H

#include <string>
#include <sstream>
#include <set>
#include <list>

#include <gst/pbutils/pbutils.h>

struct MediaSegment
{
    GstClockTime begin;
    GstClockTime end;

    MediaSegment()
    {
        begin = GST_CLOCK_TIME_NONE;
        end = GST_CLOCK_TIME_NONE;
    }

    MediaSegment(GstClockTime b, GstClockTime e)
    {
        if ( b < e ) {
            begin = b;
            end = e;
        } else {
            begin = GST_CLOCK_TIME_NONE;
            end = GST_CLOCK_TIME_NONE;
        }
    }
    inline bool is_valid() const
    {
        return begin != GST_CLOCK_TIME_NONE && end != GST_CLOCK_TIME_NONE && begin < end;
    }
    inline bool operator < (const MediaSegment b) const
    {
        return (this->is_valid() && b.is_valid() && this->end < b.begin);
    }
    inline bool operator == (const MediaSegment b) const
    {
        return (this->begin == b.begin && this->end == b.end);
    }
    inline bool operator != (const MediaSegment b) const
    {
        return (this->begin != b.begin || this->end != b.end);
    }
};

struct containsTime: public std::unary_function<MediaSegment, bool>
{
    inline bool operator()(const MediaSegment s) const
    {
       return ( s.is_valid() && _t > s.begin && _t < s.end );
    }

    containsTime(GstClockTime t) : _t(t) { }

private:
    GstClockTime _t;
};


typedef std::set<MediaSegment> MediaSegmentSet;


class Timeline
{
public:
    Timeline();
    ~Timeline();

    void reset();
    void init(GstClockTime start, GstClockTime end, GstClockTime frame_duration);

    bool addPlaySegment(GstClockTime begin, GstClockTime end);
    bool addPlaySegment(MediaSegment s);
    bool removePlaySegmentAt(GstClockTime t);
    bool removeAllPlaySegmentOverlap(MediaSegment s);
    std::list< std::pair<guint64, guint64> > getPlaySegments() const;

    inline float *Array() { return array_; }
    inline size_t ArraySize() { return array_size_; }

private:

    GstClockTime start_;
    GstClockTime end_;
    size_t num_frames_;

    float *array_;
    size_t array_size_;
    MediaSegmentSet segments_;
    MediaSegmentSet::iterator current_segment_;

};

#endif // TIMELINE_H
