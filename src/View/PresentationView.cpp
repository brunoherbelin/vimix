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
#include "Source/SessionSource.h"
#include "Source/SessionThumbnailSource.h"
#include "Toolkit/SystemToolkit.h"
#include "Window/WorkspaceWindow.h"

#include "TransitionView.h"
#include "imgui.h"
#include "PresentationView.h"

PresentationView::PresentationView() : StripView(PRESENTATION, "session"), transition_source_(nullptr),
    transition_d_(0.f), transition_cross_fade_(true)
{
    // fixed layout: no settings to restore
    Settings::application.views[mode_].name = "Presentation";
}

int PresentationView::indexOf(const std::string &path) const
{
    for (size_t i = 0; i < items_.size(); ++i) {
        if (items_[i]->path() == path)
            return static_cast<int>(i);
    }
    return -1;
}

bool PresentationView::busy() const
{
    return transition_source_ != nullptr || Mixer::manager().busy();
}

bool PresentationView::isOpened(int i) const
{
    return items_[i]->path() == Mixer::manager().liveSession()->filename();
}

bool PresentationView::available(int i) const
{
    return !items_[i]->missing();
}

std::string PresentationView::label(int i) const
{
    return SystemToolkit::base_filename( items_[i]->path() );
}

void PresentationView::setPlaylist(const Playlist &playlist, const std::string &name)
{
    clearItems();
    playlist_ = playlist;

    // name to display (e.g. folder of a list without playlist file)
    playlist_name_ = name.empty() ? SystemToolkit::base_filename( playlist_.filename() ) : name;

    // one thumbnail per session file
    for (size_t i = 0; i < playlist_.size(); ++i)
        addItem( new SessionThumbnailSource( playlist_.at(i) ) );

    // current item is the live session, if in the list
    live_filename_ = Mixer::manager().liveSession()->filename();
    setCurrent( indexOf(live_filename_), false );

    restart();
}

void PresentationView::open(int i)
{
    if (i < 0 || i >= static_cast<int>(items_.size()) || busy() || items_[i]->missing())
        return;

    if (Draft::manager().active()) {
        Log::Notify("Not available in Draft mode.");
        return;
    }

    const std::string path = items_[i]->path();

    if (Settings::application.smooth_transition) {
        // create special SessionSource to be used for the smooth transition
        SessionFileSource *ts = new SessionFileSource;
        Log::Info("\nStarting transition to session %s", path.c_str());
        ts->load(path);
        ts->setName(SystemToolkit::base_filename(path));

        // insert source without changing view
        Mixer::manager().insertSource(ts, View::INVALID);

        // start transition (performed in update)
        transition_source_ = ts;
        transition_d_ = TransitionView::initialProgress(transition_cross_fade_);
        TransitionView::apply(transition_source_, transition_d_, transition_cross_fade_);
        transition_source_->touch();
    }
    else
        Mixer::manager().load(path);

    setCurrent(i, true);
}

bool PresentationView::validTransition()
{
    // the transition source is lost if the session was changed meanwhile
    if (transition_source_ != nullptr &&
        Mixer::manager().session()->find(transition_source_) == Mixer::manager().session()->end())
        transition_source_ = nullptr;

    return transition_source_ != nullptr;
}

void PresentationView::leave(bool restore_workspace)
{
    // finish transition
    if ( validTransition() ) {
        if (transition_source_->ready()) {
            TransitionView::apply(transition_source_, 0.f, transition_cross_fade_);
            Session *se = transition_source_->detach();
            if (se != nullptr)
                Mixer::manager().set(se);
        }
        else {
            // cannot finish a transition to a session not yet loaded
            Mixer::manager().deleteSource(transition_source_);
            Log::Info("Transition interrupted.");
        }
        transition_source_ = nullptr;
    }

    // free thumbnails
    clearItems();
    playlist_.clear();
    playlist_name_.clear();
    live_filename_.clear();

    // presentation started with a clear workspace
    if (restore_workspace)
        WorkspaceWindow::restoreWorkspace();
}

void PresentationView::sync()
{
    // follow changes of live session (e.g. loaded externally)
    const std::string live = Mixer::manager().liveSession()->filename();
    if (live != live_filename_) {
        live_filename_ = live;
        const int i = indexOf(live_filename_);
        if (transition_source_ == nullptr && i > -1 && i != current_)
            setCurrent(i, true);
    }
}

void PresentationView::animate(float dt)
{
    if ( !validTransition() )
        return;

    // cancel if transition source failed
    if (transition_source_->failed()) {
        Log::Warning("Failed to open session %s.", transition_source_->path().c_str());
        Mixer::manager().deleteSource(transition_source_);
        transition_source_ = nullptr;
        return;
    }

    // progress of transition once the session is loaded
    if (transition_source_->ready())
        transition_d_ += dt / (MAX(Settings::application.transition.duration, 0.01f) * 1000.f);

    // apply transition
    TransitionView::apply(transition_source_, MIN(transition_d_, 0.f), transition_cross_fade_);
    transition_source_->touch();

    // end of transition: open the session
    if (transition_d_ >= 0.f) {
        Session *se = transition_source_->detach();
        transition_source_ = nullptr;
        if (se != nullptr) {
            Mixer::manager().set(se);
            // leave view if requested (session is set at next update)
            if ( leave_target_ > -1 )
                Mixer::manager().setView(View::MIXING);
        }
    }
}

void PresentationView::drawInformation(bool is_busy)
{
    if (current_ > -1) {

        // right aligned filename
        ImGuiToolkit::PushDisabled(is_busy);
        ImGui::SameLine();
        char label[256];
        snprintf(label, sizeof(label), "%s  %s", is_busy ? ICON_FA_HOURGLASS_HALF : " ",
                SystemToolkit::filename( items_[current_]->path() ).c_str());
        const ImVec2 label_size = ImGui::CalcTextSize(label, NULL);
        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionWidth() - label_size.x);
        ImGui::Text("%s", label);
        ImGui::NewLine();
        ImGuiToolkit::PopDisabled(is_busy);

        // Session of current item: the one loaded for transition, or the live session
        Session *se = nullptr;
        if ( validTransition() ) {
            if ( transition_source_->ready() )
                se = transition_source_->session();
        }
        else if ( Mixer::manager().liveSession()->filename() == items_[current_]->path() )
            se = Mixer::manager().liveSession();

        // Notes from sticky notes
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        bool has_notes = false;
        if (se != nullptr) {
            for (auto note = se->beginNotes(); note != se->endNotes(); ++note) {
                if ( (*note).text.empty() )
                    continue;
                if (has_notes)
                    ImGui::Spacing();
                ImGui::TextWrapped("%s", (*note).text.c_str());
                has_notes = true;
            }
        }
        if (!has_notes)
            ImGui::TextDisabled("No note");
        ImGui::PopFont();

    }
    else {
        ImGui::NewLine();
        ImGui::TextDisabled("Ready to start...");
    }
}

void PresentationView::drawButtons()
{
    // disable outputs
    ImGuiToolkit::ButtonToggle(ICON_FA_EYE_SLASH, &Settings::application.render.disabled);
    if (ImGui::IsItemHovered())
        ImGuiToolkit::ToolTip(MENU_OUTPUTDISABLE, SHORTCUT_OUTPUTDISABLE);
}
