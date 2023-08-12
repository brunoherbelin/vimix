/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2023 Bruno Herbelin <bruno.herbelin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
**/

// ImGui
#include "ImGuiToolkit.h"
#include "imgui_internal.h"

#include "defines.h"
#include "Settings.h"
#include "GstToolkit.h"
#include "Metronome.h"

#include "TimerMetronomeWindow.h"

#define PLOT_CIRCLE_SEGMENTS 64



TimerMetronomeWindow::TimerMetronomeWindow() : WorkspaceWindow("Timer")
{
    // timer modes : 0 Metronome, 1 Stopwatch
    timer_menu = { "Metronome", "Stopwatch" };
    // clock times
    start_time_      = gst_util_get_timestamp ();
    start_time_hand_ = gst_util_get_timestamp ();
    duration_hand_   = Settings::application.timer.stopwatch_duration *  GST_SECOND;
}


void TimerMetronomeWindow::setVisible(bool on)
{
    // restore workspace to show the window
    if (WorkspaceWindow::clear_workspace_enabled) {
        WorkspaceWindow::restoreWorkspace(on);
        // do not change status if ask to hide (consider user asked to toggle because the window was not visible)
        if (!on)  return;
    }

    if (Settings::application.widget.timer_view > 0 && Settings::application.widget.timer_view != Settings::application.current_view){
        Settings::application.widget.timer_view = -1;
        on = true;
    }

    Settings::application.widget.timer = on;
}

bool TimerMetronomeWindow::Visible() const
{
    return ( Settings::application.widget.timer
             && (Settings::application.widget.timer_view < 0 || Settings::application.widget.timer_view == Settings::application.current_view )
             );
}

void TimerMetronomeWindow::Render()
{
    // constraint square resizing
    static ImVec2 timer_window_size_min = ImVec2(11.f * ImGui::GetTextLineHeight(), 11.f * ImGui::GetTextLineHeight());
    ImGui::SetNextWindowSizeConstraints(timer_window_size_min, timer_window_size_min * 1.5f, ImGuiToolkit::CustomConstraints::Square);
    ImGui::SetNextWindowPos(ImVec2(600, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(timer_window_size_min, ImGuiCond_FirstUseEver);

    if ( !ImGui::Begin(name_, &Settings::application.widget.timer,
                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar |  ImGuiWindowFlags_NoCollapse ))
    {
        ImGui::End();
        return;
    }

    // menu (no title bar)
    if (ImGui::BeginMenuBar())
    {
        // Close and widget menu
        if (ImGuiToolkit::IconButton(4,16))
            Settings::application.widget.timer = false;
        if (ImGui::BeginMenu(IMGUI_TITLE_TIMER))
        {
            // Enable/Disable Ableton Link
            if ( ImGui::MenuItem( ICON_FA_USER_CLOCK " Ableton Link", nullptr, &Settings::application.timer.link_enabled) ) {
                Metronome::manager().setEnabled(Settings::application.timer.link_enabled);
            }

            // output manager menu
            ImGui::Separator();
            bool pinned = Settings::application.widget.timer_view == Settings::application.current_view;
            std::string menutext = std::string( ICON_FA_MAP_PIN "    Stick to ") + Settings::application.views[Settings::application.current_view].name + " view";
            if ( ImGui::MenuItem( menutext.c_str(), nullptr, &pinned) ){
                if (pinned)
                    Settings::application.widget.timer_view = Settings::application.current_view;
                else
                    Settings::application.widget.timer_view = -1;
            }
            if ( ImGui::MenuItem( MENU_CLOSE, SHORTCUT_TIMER) )
                Settings::application.widget.timer = false;

            ImGui::EndMenu();
        }
        // Selection of the timer mode
        if (ImGui::BeginMenu( timer_menu[Settings::application.timer.mode].c_str() ))
        {
            for (size_t i = 0; i < timer_menu.size(); ++i) {
                if (ImGui::MenuItem(timer_menu[i].c_str(), NULL, Settings::application.timer.mode==i))
                    Settings::application.timer.mode = i;
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    // current windowdraw parameters
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* draw_list = window->DrawList;
    // positions and size of GUI elements
    const float margin = window->MenuBarHeight();
    const float h = 0.4f * ImGui::GetFrameHeight();
    const ImVec2 circle_top_left = window->Pos + ImVec2(margin + h, margin + h);
    const ImVec2 circle_top_right = window->Pos + ImVec2(window->Size.y - margin - h, margin + h);
    const ImVec2 circle_botom_right = window->Pos + ImVec2(window->Size.y - margin - h, window->Size.x - margin - h);
    const ImVec2 circle_center = window->Pos + (window->Size + ImVec2(margin, margin) )/ 2.f;
    const float circle_radius = (window->Size.y - 2.f * margin) / 2.f;

    // color palette
    static ImU32 colorbg = ImGui::GetColorU32(ImGuiCol_FrameBgActive, 0.6f);
    static ImU32 colorfg = ImGui::GetColorU32(ImGuiCol_FrameBg, 2.5f);
    static ImU32 colorline = ImGui::GetColorU32(ImGuiCol_PlotHistogram);

    //
    // METRONOME
    //
    if (Settings::application.timer.mode < 1) {

        // Metronome info
        double t = Metronome::manager().tempo();
        double p = Metronome::manager().phase();
        double q = Metronome::manager().quantum();
        uint np  = (int) Metronome::manager().peers();

        // draw background ring
        draw_list->AddCircleFilled(circle_center, circle_radius, colorbg, PLOT_CIRCLE_SEGMENTS);

        // draw quarter
        static const float resolution = PLOT_CIRCLE_SEGMENTS / (2.f * M_PI);
        static ImVec2 buffer[PLOT_CIRCLE_SEGMENTS];
        float a0 = -M_PI_2 + (floor(p)/floor(q)) * (2.f * M_PI);
        float a1 = a0 + (1.f / floor(q)) * (2.f * M_PI);
        int n = ImMax(3, (int)((a1 - a0) * resolution));
        double da = (a1 - a0) / (n - 1);
        int index = 0;
        buffer[index++] = circle_center;
        for (int i = 0; i < n; ++i) {
            double a = a0 + i * da;
            buffer[index++] = ImVec2(circle_center.x + circle_radius * cos(a), circle_center.y + circle_radius * sin(a));
        }
        draw_list->AddConvexPolyFilled(buffer, index, colorfg);

        // draw clock hand
        a0 = -M_PI_2 + (p/q) * (2.f * M_PI);
        draw_list->AddLine(ImVec2(circle_center.x + margin * cos(a0), circle_center.y + margin * sin(a0)),
                           ImVec2(circle_center.x + circle_radius * cos(a0), circle_center.y + circle_radius * sin(a0)), colorline, 2.f);

        // centered indicator 'x / N'
        draw_list->AddCircleFilled(circle_center, margin, colorfg, PLOT_CIRCLE_SEGMENTS);
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_MONO);
        char text_buf[24];
        snprintf(text_buf, 24, "%d/%d", (int)(p)+1, (int)(q) );
        ImVec2 label_size = ImGui::CalcTextSize(text_buf, NULL);
        ImGui::SetCursorScreenPos(circle_center - label_size/2);
        ImGui::Text("%s", text_buf);
        ImGui::PopFont();

        // left slider : quantum
        float float_value = ceil(Metronome::manager().quantum());
        ImGui::SetCursorPos(ImVec2(0.5f * margin, 1.5f * margin));
        if ( ImGui::VSliderFloat("##quantum", ImVec2(0.5f * margin, 2.f * circle_radius ), &float_value, 2, 200, "", 2.f) ){
            Metronome::manager().setQuantum( ceil(float_value) );
        }
        if (ImGui::IsItemHovered() || ImGui::IsItemActive() )  {
            ImGui::BeginTooltip();
            guint64 time_phase = GST_SECOND * (60.0 * q / t) ;
            ImGui::Text("%d beats per phase\n= %s at %d BPM", (int) ceil(float_value),
                        GstToolkit::time_to_string(time_phase, GstToolkit::TIME_STRING_READABLE).c_str(), (int) t);
            ImGui::EndTooltip();
        }

        // Controls NOT operational if peer connected
        if (np >0 ) {
            // Tempo
            ImGui::SetCursorScreenPos(circle_top_right);
            ImGuiToolkit::PushFont(ImGuiToolkit::FONT_BOLD);
            ImGui::PushStyleColor(ImGuiCol_Text, colorfg);
            snprintf(text_buf, 24, "%d", (int) ceil(t) );
            ImGui::Text("%s", text_buf);
            ImGui::PopStyleColor();
            ImGui::PopFont();
            if (ImGui::IsItemHovered()){
                snprintf(text_buf, 24, "%d BPM\n(set by peer)", (int) ceil(t));
                ImGuiToolkit::ToolTip(text_buf);
            }
        }
        // Controls operational only if no peer
        else {
            // Tempo
            ImGui::SetCursorScreenPos(circle_top_right);
            ImGuiToolkit::PushFont(ImGuiToolkit::FONT_BOLD);
            snprintf(text_buf, 24, "%d", (int) ceil(t) );
            ImGui::Text("%s", text_buf);
            ImGui::PopFont();
            if (ImGui::IsItemClicked())
                ImGui::OpenPopup("bpm_popup");
            else if (ImGui::IsItemHovered()){
                snprintf(text_buf, 24, "%d BPM\n(clic to edit)", (int) ceil(t));
                ImGuiToolkit::ToolTip(text_buf);
            }
            if (ImGui::BeginPopup("bpm_popup", ImGuiWindowFlags_NoMove))
            {
                ImGui::SetNextItemWidth(80);
                ImGui::InputText("BPM", text_buf, 8, ImGuiInputTextFlags_CharsDecimal);
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    int t = 0;
                    sscanf(text_buf, "%d", &t);
                    t = CLAMP(t, 20, 2000);
                    Metronome::manager().setTempo((double) t);
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
            // restart icon
            ImGui::SetCursorScreenPos(circle_top_left);
            if (ImGuiToolkit::IconButton(9, 13)) {
                Metronome::manager().restart();
            }
        }

        // Network indicator, if link enabled
        if (Settings::application.timer.link_enabled) {
            ImGui::SetCursorScreenPos(circle_botom_right);
            ImGuiToolkit::Icon(16, 5, np > 0);
            if (ImGui::IsItemHovered()){
                snprintf(text_buf, 24, np < 1 ? "Ableton Link\nNo peer" : "Ableton Link\n%d peer%c", np, np < 2 ? ' ' : 's' );
                ImGuiToolkit::ToolTip(text_buf);
            }
        }

    }
    //
    // STOPWATCH
    //
    else {
        guint64 time_ = gst_util_get_timestamp ();

        // draw ring
        draw_list->AddCircle(circle_center, circle_radius, colorbg, PLOT_CIRCLE_SEGMENTS, 12 );
        draw_list->AddCircleFilled(ImVec2(circle_center.x, circle_center.y - circle_radius), 7, colorfg, PLOT_CIRCLE_SEGMENTS);
        // draw indicator time hand
        double da = -M_PI_2 + ( (double) (time_-start_time_hand_) / (double) duration_hand_) * (2.f * M_PI);
        draw_list->AddCircleFilled(ImVec2(circle_center.x + circle_radius * cos(da), circle_center.y + circle_radius * sin(da)), 7, colorline, PLOT_CIRCLE_SEGMENTS);

        // left slider : countdown
        float float_value = (float) Settings::application.timer.stopwatch_duration;
        ImGui::SetCursorPos(ImVec2(0.5f * margin, 1.5f * margin));
        if ( ImGui::VSliderFloat("##duration", ImVec2(0.5f * margin, 2.f * circle_radius ), &float_value, 1, 3600, "", 3.f) ){
            Settings::application.timer.stopwatch_duration = (uint64_t) float_value;
            duration_hand_ = Settings::application.timer.stopwatch_duration * GST_SECOND;
        }
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())  {
            ImGui::BeginTooltip();
            ImGui::Text("%s\ncountdown", GstToolkit::time_to_string(duration_hand_, GstToolkit::TIME_STRING_READABLE).c_str() );
            ImGui::EndTooltip();
        }

        // main text: elapsed time
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        char text_buf[24];
        snprintf(text_buf, 24, "%s", GstToolkit::time_to_string(time_-start_time_, GstToolkit::TIME_STRING_FIXED).c_str() );
        ImVec2 label_size = ImGui::CalcTextSize(text_buf, NULL);
        ImGui::SetCursorScreenPos(circle_center - label_size/2);
        ImGui::Text("%s", text_buf);
        ImGui::PopFont();

        // small text: remaining time
        ImGui::PushStyleColor(ImGuiCol_Text, colorfg);
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_BOLD);

        guint64 lap = (time_-start_time_hand_)/duration_hand_;
        snprintf(text_buf, 24, "%lu turn%s", (unsigned long) lap, lap > 1 ? "s" : " " );
        label_size = ImGui::CalcTextSize(text_buf, NULL);
        ImGui::SetCursorScreenPos(circle_center + ImVec2(0.f, circle_radius * -0.7f) - label_size/2);
        ImGui::Text("%s", text_buf);

        snprintf(text_buf, 24, "%s", GstToolkit::time_to_string(duration_hand_-(time_-start_time_hand_)%duration_hand_, GstToolkit::TIME_STRING_READABLE).c_str() );
        label_size = ImGui::CalcTextSize(text_buf, NULL);
        ImGui::SetCursorScreenPos(circle_center - ImVec2(0.f, circle_radius * -0.7f) - label_size/2);
        ImGui::Text("%s", text_buf);
        ImGui::PopFont();
        ImGui::PopStyleColor();

        // reset icon
        ImGui::SetCursorScreenPos(circle_top_left);
        if (ImGuiToolkit::IconButton(8, 13))
            start_time_ = start_time_hand_ = time_; // reset timers

        // TODO : pause ?
    }

    ImGui::End();
}
