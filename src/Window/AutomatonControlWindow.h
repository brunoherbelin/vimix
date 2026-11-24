#ifndef AUTOMATONCONTROLWINDOW_H
#define AUTOMATONCONTROLWINDOW_H

#include <cstdint>

#include "WorkspaceWindow.h"

class AutomatonRig;
class Automaton;

/**
 * @brief The AutomatonControlWindow lists the automatons of the session
 * and shows their motion in time as rows of a timeline, aligned on the beats
 * of the metronome; the duration of the longest cycle is the width of the timeline.
 * The selected automaton can be configured below the timeline.
 */
class AutomatonControlWindow : public WorkspaceWindow
{
    char name_buffer_[64];
    uint64_t name_id_;
    double drag_offset_;   // phase offset when starting to drag the timeline
    double drag_period_;   // period when starting to drag the timeline
    double drag_anchor_;   // beat time of the cycle start kept fixed when scaling
    float drag_anchor_x_;  // its position in the timeline when starting to drag
    double drag_span_;     // width and start of the timeline frozen while dragging (0 if not)
    double drag_page_;
    int drag_mode_;        // drag on timeline: 0 shifts phase, 1 scales cycle duration
    float period_slider_;  // log2 of period, while moving the period slider
    bool period_sliding_;

    void RenderTimelines (AutomatonRig *rig);
    bool DrawControlBar (AutomatonRig *rig, Automaton *a);
    void RenderDetails (AutomatonRig *rig, Automaton *a);

public:
    AutomatonControlWindow ();

    void Render ();
    void setVisible (bool on);

    // from WorkspaceWindow
    bool Visible () const override;
};

#endif // AUTOMATONCONTROLWINDOW_H
