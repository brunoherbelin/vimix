#ifndef TIMELINE_H
#define TIMELINE_H

#include <string>
#include <sstream>
#include <set>
#include <list>

#include <gst/pbutils/pbutils.h>

struct TimeInterval
{
    GstClockTime begin;
    GstClockTime end;
    TimeInterval()
    {
        reset();
    }
    TimeInterval(GstClockTime a, GstClockTime b) : TimeInterval()
    {
        if ( a != GST_CLOCK_TIME_NONE && b != GST_CLOCK_TIME_NONE) {
            begin = MIN(a, b);
            end = MAX(a, b);
        }
    }
    inline void reset()
    {
        begin = GST_CLOCK_TIME_NONE;
        end = GST_CLOCK_TIME_NONE;
    }
    inline GstClockTime duration() const
    {
        return is_valid() ? (end - begin) : GST_CLOCK_TIME_NONE;
    }
    inline bool is_valid() const
    {
        return begin != GST_CLOCK_TIME_NONE && end != GST_CLOCK_TIME_NONE && begin < end;
    }
    inline bool operator < (const TimeInterval b) const
    {
        return (this->is_valid() && b.is_valid() && this->end < b.begin);
    }
    inline bool operator == (const TimeInterval b) const
    {
        return (this->begin == b.begin && this->end == b.end);
    }
    inline bool operator != (const TimeInterval b) const
    {
        return (this->begin != b.begin || this->end != b.end);
    }
    inline TimeInterval& operator = (const TimeInterval& b)
    {
        if (this != &b) {
            this->begin = b.begin;
            this->end = b.end;
        }
        return *this;
    }
    inline bool includes(const GstClockTime t) const
    {
        return (is_valid() && t != GST_CLOCK_TIME_NONE && t > this->begin && t < this->end);
    }
};


struct order_comparator
{
    inline bool operator () (const TimeInterval a, const TimeInterval b) const
    {
        return (a < b);
    }
};

typedef std::set<TimeInterval, order_comparator> TimeIntervalSet;


class Timeline
{
public:
    Timeline();
    ~Timeline();
    Timeline& operator = (const Timeline& b);

    void reset();
    bool is_valid();

    // global properties of the timeline
    // timeline is invalid untill all 3 are set
    void setStart(GstClockTime start);
    void setEnd(GstClockTime end);
    void setStep(GstClockTime dt);

    // get properties
    inline GstClockTime start() const { return timing_.begin; }
    inline GstClockTime end() const { return timing_.end; }
    inline GstClockTime step() const { return step_; }
    inline GstClockTime duration() const { return timing_.duration(); }
    inline size_t numFrames() const { return duration() / step_; }

    // Add / remove gaps in the timeline
    bool addGap(TimeInterval s);
    bool addGap(GstClockTime begin, GstClockTime end);
    bool removeGaptAt(GstClockTime t);
    void clearGaps();
    void toggleGaps(GstClockTime from, GstClockTime to);

    // get gaps
    size_t numGaps();
    bool gapAt(const GstClockTime t, TimeInterval &gap);
    std::list< std::pair<guint64, guint64> > gaps() const;

    // synchronize data structures
    void updateGapsFromArray(float *array_, size_t array_size_);
    void fillArrayFromGaps(float *array_, size_t array_size_);

private:

    // global information on the timeline
    TimeInterval timing_;
    GstClockTime step_;

    // main data structure containing list of gaps in the timeline
    TimeIntervalSet gaps_;

};

#endif // TIMELINE_H
