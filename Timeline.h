#ifndef TIMELINE_H
#define TIMELINE_H

#include <string>
#include <sstream>
#include <set>
#include <list>

#include <gst/pbutils/pbutils.h>

#define MAX_TIMELINE_ARRAY 2000

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
        return (is_valid() && t != GST_CLOCK_TIME_NONE && !(t < this->begin) && !(t > this->end) );
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

    bool is_valid();
    void update();

    // global properties of the timeline
    // timeline is valid only if all 3 are set
    void setFirst(GstClockTime first);  // TODO : do we really use FIRST ?
    void setEnd(GstClockTime end);
    void setStep(GstClockTime dt);
    void setTiming(TimeInterval interval, GstClockTime step = GST_CLOCK_TIME_NONE);

    // Timing manipulation
    inline GstClockTime begin() const { return timing_.begin; }
    inline GstClockTime end() const { return timing_.end; }
    inline GstClockTime first() const { return first_; }
    inline GstClockTime last() const { return timing_.end - step_; }
    inline GstClockTime step() const { return step_; }
    inline GstClockTime duration() const { return timing_.duration(); }
    inline size_t numFrames() const { return duration() / step_; }
    inline TimeInterval interval() const { return timing_; }
    GstClockTime next(GstClockTime time) const;
    GstClockTime previous(GstClockTime time) const;

    // Manipulation of gaps in the timeline
    inline TimeIntervalSet gaps() const { return gaps_; }
    inline TimeIntervalSet sections() const;
    inline size_t numGaps() const { return gaps_.size(); }
    float *gapsArray();
    void clearGaps();
    void setGaps(const TimeIntervalSet &g);
    bool addGap(TimeInterval s);
    bool addGap(GstClockTime begin, GstClockTime end);
    bool removeGaptAt(GstClockTime t);
    bool gapAt(const GstClockTime t, TimeInterval &gap) const;

    float fadingAt(const GstClockTime t);
    inline float *fadingArray() { return fadingArray_; }
    void clearFading();
    void smoothFading(uint N = 1);
    void autoFading(uint milisecond = 100);

private:

    void reset();

    // global information on the timeline
    TimeInterval timing_;
    GstClockTime first_;
    GstClockTime step_;

    // main data structure containing list of gaps in the timeline
    TimeIntervalSet gaps_;    
    float gapsArray_[MAX_TIMELINE_ARRAY];
    bool gaps_array_need_update_;
    // synchronize data structures
    void updateGapsFromArray(float *array, size_t array_size);
    void fillArrayFromGaps(float *array, size_t array_size);

    float fadingArray_[MAX_TIMELINE_ARRAY];

};

#endif // TIMELINE_H
