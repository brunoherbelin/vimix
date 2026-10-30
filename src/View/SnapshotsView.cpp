/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2026 Bruno Herbelin <bruno.herbelin@gmail.com>
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

#include "IconsFontAwesome5.h"
#include "Toolkit/ImGuiToolkit.h"

#include <string>

#include "defines.h"
#include "Log.h"
#include "Mixer.h"
#include "Draft.h"
#include "Settings.h"
#include "ActionManager.h"
#include "Source/SessionThumbnailSource.h"
#include "Toolkit/SystemToolkit.h"
#include "Window/WorkspaceWindow.h"

#include "imgui.h"
#include "SnapshotsView.h"

std::string readable_date_time_string(std::string date);

SnapshotsView::SnapshotsView() : StripView(SNAPSHOTS, "version"), session_(nullptr), date_id_(0)
{
    // fixed layout: no settings to restore
    Settings::application.views[mode_].name = "Versions";
}

int SnapshotsView::indexOf(uint64_t id) const
{
    for (size_t i = 0; i < ids_.size(); ++i) {
        if (ids_[i] == id)
            return static_cast<int>(i);
    }
    return -1;
}

bool SnapshotsView::busy() const
{
    return Action::manager().animating() || Mixer::manager().busy();
}

bool SnapshotsView::isOpened(int i) const
{
    return ids_[i] == Action::manager().restoredSnapshot();
}

std::string SnapshotsView::label(int i) const
{
    return labels_[i];
}

void SnapshotsView::setSnapshots()
{
    build();
    restart();
}

void SnapshotsView::build()
{
    clearItems();
    ids_.clear();
    labels_.clear();

    // versions of the live session, in chronological order
    session_ = Mixer::manager().liveSession();
    snapshots_ = Action::manager().snapshots();
    for (auto id = snapshots_.begin(); id != snapshots_.end(); ++id) {
        const std::string l = Action::manager().label(*id);
        ids_.push_back(*id);
        labels_.push_back(l);
        addItem( new SessionThumbnailSource(Action::manager().thumbnail(*id), l) );
    }

    // name to display
    const std::string filename = session_->filename();
    title_ = filename.empty() ? "Untitled session" : SystemToolkit::base_filename(filename);

    // current item is the version restored last, if in the list
    setCurrent( indexOf(Action::manager().restoredSnapshot()), false );
}

void SnapshotsView::sync()
{
    // follow changes of live session and of its versions
    bool changed = session_ != Mixer::manager().liveSession()
                   || snapshots_ != Action::manager().snapshots();

    // version was being captured when the list was set
    for (size_t i = 0; !changed && i < ids_.size(); ++i)
        changed = labels_[i].empty() && !Action::manager().label(ids_[i]).empty();

    if (changed) {
        const uint64_t target = leave_target_ > -1 ? ids_[leave_target_] : 0;
        build();
        if (target > 0)
            leave_target_ = indexOf(target);
    }

    // follow version restored (e.g. from session panel)
    const int i = indexOf(Action::manager().restoredSnapshot());
    if (i > -1 && i != current_)
        setCurrent(i, true);
}

void SnapshotsView::open(int i)
{
    if (i < 0 || i >= static_cast<int>(items_.size()) || busy())
        return;

    if (Draft::manager().active()) {
        Log::Notify("Not available in Draft mode.");
        return;
    }

    // restore with an animation (performed in Action update)
    Action::manager().restore(ids_[i], Mixer::manager().liveSession()->snapshots()->duration_);

    setCurrent(i, true);
}

void SnapshotsView::leave(bool restore_workspace)
{
    // finish animation
    Action::manager().finish();

    // free thumbnails
    clearItems();
    snapshots_.clear();
    ids_.clear();
    labels_.clear();
    session_ = nullptr;
    title_.clear();
    date_.clear();
    date_id_ = 0;

    // presentation started with a clear workspace
    if (restore_workspace)
        WorkspaceWindow::restoreWorkspace();
}

void SnapshotsView::drawInformation(bool is_busy)
{
    if (current_ > -1) {

        // right aligned label of version
        ImGuiToolkit::PushDisabled(is_busy);
        ImGui::SameLine();
        char text[256];
        snprintf(text, sizeof(text), "%s  %s", is_busy ? ICON_FA_HOURGLASS_HALF : " ", labels_[current_].c_str());
        const ImVec2 text_size = ImGui::CalcTextSize(text, NULL);
        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionWidth() - text_size.x);
        ImGui::Text("%s", text);
        ImGui::NewLine();
        ImGuiToolkit::PopDisabled(is_busy);

        // date of current version
        if (date_id_ != ids_[current_]) {
            date_ = "Version of " + readable_date_time_string(Action::manager().date(ids_[current_]));
            date_id_ = ids_[current_];
        }
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ImGui::TextWrapped("%s", date_.c_str());
        ImGui::PopFont();

    }
    else {
        ImGui::NewLine();
        ImGui::TextDisabled("Ready to start...");
        ImGui::NewLine();
    }
}

void SnapshotsView::drawButtons()
{
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetColorU32(ImGuiCol_MenuBarBg));
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImGui::GetColorU32(ImGuiCol_HeaderActive));
    // progress of the animation to current version
    if (Action::manager().animating()) {
        std::string _progress = std::to_string((int)(Action::manager().progress() * 100.f)) + "%";
        ImGui::ProgressBar(Action::manager().progress(),
                        ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 0.f), _progress.c_str());
    }
    else {
        // duration of animation between versions
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
        ImGui::SliderFloat("##snapshot_duration", &Mixer::manager().liveSession()->snapshots()->duration_, 50.f, 5000.f,
                         ICON_FA_STOPWATCH " %.0f ms");
    }
    ImGui::PopStyleColor(2);
}
