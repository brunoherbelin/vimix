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

#include <string>
#include <list>
#include <mutex>
using namespace std;

#include "defines.h"
#include "ImGuiToolkit.h"
#include "DialogToolkit.h"
#include "Log.h"


static std::mutex mtx;

struct AppLog
{
    ImGuiTextBuffer     Buf;
    ImGuiTextFilter     Filter;
    ImVector<int>       LineOffsets; 
    bool                LogInTitle;

    AppLog()
    {
        Clear();
    }

    void Clear()
    {
        mtx.lock();
        Buf.clear();
        LineOffsets.clear();
        LineOffsets.push_back(0);
        LogInTitle = false;
        mtx.unlock();
    }

    void AddLog(const char* fmt, va_list args)
    {
        mtx.lock();
        int old_size = Buf.size();
        Buf.appendf("%04d  ", LineOffsets.size()); // this adds 6 characters to show line number
        Buf.appendfv(fmt, args);
        Buf.append("\n");

        for (int new_size = Buf.size(); old_size < new_size; old_size++)
            if (Buf[old_size] == '\n')
                LineOffsets.push_back(old_size + 1);
        mtx.unlock();
    }

    void Draw(const char* title, bool* p_open = NULL)
    {

        ImGui::SetNextWindowPos(ImVec2(430, 660), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(1150, 220), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(600, 180), ImVec2(FLT_MAX, FLT_MAX));

        // normal title
        char window_title[1024];
        snprintf(window_title, 1024, "%s ###LOGVIMIX", title);

        if (*p_open) {
            // if open but Collapsed, create title of window with last line of logs
            if (LogInTitle) {
                char lastlogline[128];
                size_t lenght =  LineOffsets[LineOffsets.Size-1] - LineOffsets[LineOffsets.Size-2] - 1;
                memset(lastlogline, '\0', 128);
                memcpy(lastlogline, Buf.begin() + LineOffsets[LineOffsets.Size-2], MIN(127, lenght));
                snprintf(window_title, 1024, "%s - %s ###LOGVIMIX", title, lastlogline);
            }
        }

        if ( !ImGui::Begin(window_title, p_open))
        {
            LogInTitle = true;
            ImGui::End();
            return;
        }

        LogInTitle = false;

        //  window
        ImGui::SameLine(0, 0);
        static bool numbering = true;
        ImGuiToolkit::ButtonIconToggle(4, 12, 4, 12, &numbering );
        ImGui::SameLine();
        bool clear = ImGui::Button( ICON_FA_BACKSPACE " Clear");
        ImGui::SameLine();
        bool copy = ImGui::Button( ICON_FA_COPY " Copy");
        ImGui::SameLine();
        Filter.Draw("Filter", -60.0f);

        ImGui::Separator();
        if ( !ImGui::BeginChild("scrolling", ImVec2(0,0), false, ImGuiWindowFlags_AlwaysHorizontalScrollbar) )
        {
            ImGui::EndChild();
            ImGui::End();
            return;
        }

        if (clear)
            Clear();
        if (copy)
            ImGui::LogToClipboard();

        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_MONO);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

        mtx.lock();

        const char* buf = Buf.begin();
        const char* buf_end = Buf.end();
        if (Filter.IsActive())
        {
            // In this example we don't use the clipper when Filter is enabled.
            // This is because we don't have a random access on the result on our filter.
            // A real application processing logs with ten of thousands of entries may want to store the result of search/filter.
            // especially if the filtering function is not trivial (e.g. reg-exp).
            for (int line_no = 0; line_no < LineOffsets.Size; line_no++)
            {
                const char* line_start = buf + LineOffsets[line_no];
                const char* line_end = (line_no + 1 < LineOffsets.Size) ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;
                if (Filter.PassFilter(line_start, line_end))
                    ImGui::TextUnformatted(line_start, line_end);
            }
        }
        else
        {
            // The simplest and easy way to display the entire buffer:
            //   ImGui::TextUnformatted(buf_begin, buf_end);
            // And it'll just work. TextUnformatted() has specialization for large blob of text and will fast-forward to skip non-visible lines.
            // Here we instead demonstrate using the clipper to only process lines that are within the visible area.
            // If you have tens of thousands of items and their processing cost is non-negligible, coarse clipping them on your side is recommended.
            // Using ImGuiListClipper requires A) random access into your data, and B) items all being the  same height,
            // both of which we can handle since we an array pointing to the beginning of each line of text.
            // When using the filter (in the block of code above) we don't have random access into the data to display anymore, which is why we don't use the clipper.
            // Storing or skimming through the search result would make it possible (and would be recommended if you want to search through tens of thousands of entries)
            ImGuiListClipper clipper;
            clipper.Begin(LineOffsets.Size);
            while (clipper.Step())
            {
                for (int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; line_no++)
                {
                    const char* line_start = buf + LineOffsets[line_no] + (numbering?0:6);
                    const char* line_end = (line_no + 1 < LineOffsets.Size) ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;
                    ImGui::TextUnformatted(line_start, line_end);
                }
            }
            clipper.End();
        }

        mtx.unlock();

        ImGui::PopStyleVar();
        ImGui::PopFont();

        // Auto scroll
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndChild();
        ImGui::End();
    }
};

AppLog logs;
list<string> notifications;
list<string> warnings;
float notifications_timeout = 0.f;

void Log::Info(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    logs.AddLog(fmt, args);
    va_end(args);
}

void Log::ShowLogWindow(bool* p_open)
{
    ImGui::SetNextWindowSize(ImVec2(700, 600), ImGuiCond_FirstUseEver);
    logs.Draw( IMGUI_TITLE_LOGS, p_open);
}

void Log::Notify(const char* fmt, ...)
{
    ImGuiTextBuffer buf;

    va_list args;
    va_start(args, fmt);
    buf.appendfv(fmt, args);
    va_end(args);

    // will display a notification
    notifications.push_back(buf.c_str());
    notifications_timeout = 0.f;

    // always log
    Log::Info(ICON_FA_INFO_CIRCLE " %s", buf.c_str());
}

void Log::Warning(const char* fmt, ...)
{
    ImGuiTextBuffer buf;

    va_list args;
    va_start(args, fmt);
    buf.appendfv(fmt, args);
    va_end(args);

    // will display a warning dialog
    warnings.push_back(buf.c_str());

    // always log
    Log::Info(ICON_FA_EXCLAMATION_TRIANGLE " Warning - %s", buf.c_str());
}

void Log::Render(bool *showWarnings)
{
    bool show_warnings = !warnings.empty();
    bool show_notification = !notifications.empty();

    if (!show_notification && !show_warnings)
        return;

    const ImGuiIO& io = ImGui::GetIO();
    float width = io.DisplaySize.x * 0.4f;
    float pos = io.DisplaySize.x * 0.3f;

    if (show_notification){
        notifications_timeout += io.DeltaTime;
        float height = ImGui::GetTextLineHeightWithSpacing() * notifications.size();
        float y = -height + MIN( notifications_timeout * height * 10.f, height );

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 3.0);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(COLOR_NAVIGATOR, 1.f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(COLOR_NAVIGATOR, 1.f));

        ImGui::SetNextWindowPos( ImVec2(pos, y), ImGuiCond_Always );
        ImGui::SetNextWindowSize( ImVec2(width, height), ImGuiCond_Always );
        ImGui::SetNextWindowBgAlpha(0.9f); // opaque background
        if (ImGui::Begin("##notification", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav ))
        {
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + width);
            for (list<string>::iterator it=notifications.begin(); it != notifications.end(); ++it) {
                ImGui::Text( ICON_FA_INFO "  %s\n", (*it).c_str());
            }
            ImGui::PopTextWrapPos();

        }
        ImGui::End();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();

        // stop showing after timeout
        if ( notifications_timeout > IMGUI_NOTIFICATION_DURATION )
            notifications.clear();
    }


    if (show_warnings) {
        if (!ImGui::IsPopupOpen("Warning") )
            ImGui::OpenPopup("Warning");
        if (ImGui::BeginPopupModal("Warning", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGuiToolkit::Icon(7, 14);
            ImGui::SameLine(0, IMGUI_SAME_LINE);
            ImGui::SetNextItemWidth(width);
            ImGui::TextColored(ImVec4(1.0f,0.6f,0.0f,1.0f), "%ld problem(s) occurred.\n\n", warnings.size());
            ImGui::Dummy(ImVec2(width, 0));

            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + width);
            for (list<string>::iterator it=warnings.begin(); it != warnings.end(); ++it) {
                ImGui::Text("%s \n", (*it).c_str());
                ImGui::Separator();
            }
            ImGui::PopTextWrapPos();

            bool close = false;
            ImGui::Spacing();
            if (ImGui::Button("Show logs", ImVec2(width * 0.2f, 0))) {
                close = true;
                if (showWarnings!= nullptr)
                    *showWarnings = true;
            }

            ImGui::SameLine();
            ImGui::Dummy(ImVec2(width * 0.6f, 0)); // right align
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Tab));
            if (ImGui::Button(" Ok ", ImVec2(width * 0.2f, 0))
                    || ImGui::IsKeyPressed(257) || ImGui::IsKeyPressed(335) ) // GLFW_KEY_ENTER or GLFW_KEY_KP_ENTER
                close = true;
            ImGui::PopStyleColor(1);

            if (close) {
                ImGui::CloseCurrentPopup();
                // messages have been seen
                warnings.clear();
            }

            ImGui::EndPopup();
        }
    }

}

void Log::Error(const char* fmt, ...)
{
    ImGuiTextBuffer buf;

    va_list args;
    va_start(args, fmt);
    buf.appendfv(fmt, args);
    va_end(args);

    DialogToolkit::ErrorDialog(buf.c_str());

    Log::Info("Error - %s\n", buf.c_str());
}

