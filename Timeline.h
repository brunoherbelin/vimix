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
        begin = GST_CLOCK_TIME_NONE;
        end = GST_CLOCK_TIME_NONE;
    }

    TimeInterval(GstClockTime b, GstClockTime e)
    {
        if ( b < e ) {
            begin = b;
            end = e;
        } else {
            begin = GST_CLOCK_TIME_NONE;
            end = GST_CLOCK_TIME_NONE;
        }
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


typedef std::set<TimeInterval> TimeIntervalSet;


class Timeline
{
public:
    Timeline();
    ~Timeline();

    void reset();

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

    // Add / remove gaps in the timeline
    bool addGap(TimeInterval s);
    bool addGap(GstClockTime begin, GstClockTime end);
    bool removeGaptAt(GstClockTime t);
    void clearGaps();

    // get gaps
    size_t numGaps();
    bool gapAt(const GstClockTime t, TimeInterval &gap);
    std::list< std::pair<guint64, guint64> > gaps() const;

    // direct access to the array representation of the timeline
    // TODO : implement an ImGui widget to plot a timeline instead of an array
    float *array();
    size_t arraySize();
    // synchronize data structures
    void updateGapsFromArray();
    void updateArrayFromGaps();

private:

    // global information on the timeline
    TimeInterval timing_;
    GstClockTime step_;

    // main data structure containing list of gaps in the timeline
    TimeIntervalSet gaps_;

    // supplementary data structure needed to display and edit the timeline
    bool need_update_;
    void init_array();
    float *array_;
    size_t array_size_;

};

#endif // TIMELINE_H
