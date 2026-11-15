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

    void RenderTimelines (AutomatonRig *rig);
    void RenderDetails (AutomatonRig *rig, Automaton *a);

public:
    AutomatonControlWindow ();

    void Render ();
    void setVisible (bool on);

    // from WorkspaceWindow
    bool Visible () const override;
};

#endif // AUTOMATONCONTROLWINDOW_H
