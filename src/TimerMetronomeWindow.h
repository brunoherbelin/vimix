#ifndef TIMERMETRONOMEWINDOW_H
#define TIMERMETRONOMEWINDOW_H

#include <string>
#include <array>

#include <gst/gstutils.h>

#include "WorkspaceWindow.h"



class TimerMetronomeWindow : public WorkspaceWindow
{
    std::array< std::string, 2 > timer_menu;
    // clock times
    guint64 start_time_;
    guint64 start_time_hand_;
    guint64 duration_hand_;

public:
    TimerMetronomeWindow();

    void Render();
    void setVisible(bool on);

    // from WorkspaceWindow
    bool Visible() const override;
};

#endif // TIMERMETRONOMEWINDOW_H
