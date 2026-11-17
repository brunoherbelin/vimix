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

#include <cmath>
#include <cstring>
#include <vector>
#include <set>

// ImGui
#include "Toolkit/ImGuiToolkit.h"
#include "imgui_internal.h"

#include "defines.h"
#include "IconsVimixImage.h"
#include "Settings.h"
#include "Metronome.h"
#include "Mixer.h"
#include "Draft.h"
#include "Session.h"
#include "ActionManager.h"
#include "Source/Source.h"
#include "Automaton/AutomatonMath.h"
#include "Automaton/AutomatonRig.h"
#include "Automaton/PathAutomaton.h"

#include "AutomatonControlWindow.h"

#define AUTOMATON_IMGUI_COLOR(a) ImVec4(COLOR_AUTOMATON, a)

AutomatonControlWindow::AutomatonControlWindow () : WorkspaceWindow("Automatons"), name_id_(0)
{
    name_buffer_[0] = '\0';
}

void AutomatonControlWindow::setVisible (bool on)
{
    // restore workspace to show the window
    if (WorkspaceWindow::clear_workspace_enabled) {
        WorkspaceWindow::restoreWorkspace(on);
        // do not change status if ask to hide (consider user asked to toggle because the window was not visible)
        if (!on)  return;
    }

    if (Settings::application.widget.automatons_view > 0 &&
        Settings::application.widget.automatons_view != Settings::application.current_view) {
        Settings::application.widget.automatons_view = -1;
        on = true;
    }

    Settings::application.widget.automatons = on;
}

bool AutomatonControlWindow::Visible () const
{
    return ( Settings::application.widget.automatons
             && (Settings::application.widget.automatons_view < 0 ||
                 Settings::application.widget.automatons_view == Settings::application.current_view )
             );
}

void AutomatonControlWindow::Render ()
{
    const float h = ImGui::GetTextLineHeight();
    ImGui::SetNextWindowPos(ImVec2(520, 420), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(34.f * h, 20.f * h), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(24.f * h, 13.f * h), ImVec2(FLT_MAX, FLT_MAX));

    if ( !ImGui::Begin(name_, &Settings::application.widget.automatons,
                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar |
                       ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNavInputs ))
    {
        ImGui::End();
        return;
    }

    AutomatonRig *rig = Mixer::manager().session()->automatons();
    View *view = Mixer::manager().view();
    const bool can_insert = !Draft::manager().active() && !Mixer::manager().editingDraft();

    // menu (no title bar)
    if (ImGui::BeginMenuBar())
    {
        if (ImGuiToolkit::IconButton(ICON_VI_CLOSE_WIDGET))
            Settings::application.widget.automatons = false;

        if (ImGui::BeginMenu(IMGUI_TITLE_AUTOMATONS))
        {
            if ( ImGui::MenuItem( ICON_FA_PLAY "  Play all", nullptr, false, rig->anyPaused()) ) {
                rig->setPaused(false);
                Action::manager().store("Automatons: Play all");
            }
            if ( ImGui::MenuItem( ICON_FA_PAUSE "  Pause all", nullptr, false, !rig->empty() && !rig->allPaused()) ) {
                rig->setPaused(true);
                Action::manager().store("Automatons: Pause all");
            }

            ImGui::Separator();
            bool pinned = Settings::application.widget.automatons_view == Settings::application.current_view;
            std::string menutext = std::string( ICON_FA_MAP_PIN "    Stick to ") +
                                   Settings::application.views[Settings::application.current_view].name + " view";
            if ( ImGui::MenuItem( menutext.c_str(), nullptr, &pinned) ){
                if (pinned)
                    Settings::application.widget.automatons_view = Settings::application.current_view;
                else
                    Settings::application.widget.automatons_view = -1;
            }
            if ( ImGui::MenuItem( MENU_CLOSE ) )
                Settings::application.widget.automatons = false;

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(ICON_FA_PLUS " Insert", can_insert))
        {
            for (size_t t = 0; t < Automaton::Types.size(); ++t) {
                const std::string label = std::string(Automaton::Types[t].first) + "  " + Automaton::Types[t].second;
                if ( ImGui::MenuItem( label.c_str(), nullptr, false,
                                      Automaton::accepts( (Automaton::Type) t, view->mode()) ) ) {
                    Automaton *a = rig->add( (Automaton::Type) t, view );
                    if (a)
                        Action::manager().store(a->name() + ": Insert");
                }
            }
            ImGui::Separator();
            ImGui::TextDisabled("in %s view", Settings::application.views[view->mode()].name.c_str());
            ImGui::EndMenu();
        }

        // metronome
        char info[64];
        const double q = std::max(1.0, Metronome::manager().quantum());
        const double b = rig->beats();
        snprintf(info, 64, "%.1f BPM   %d / %d",
                 Metronome::manager().tempo(), (int) std::floor(b - q * std::floor(b / q)) + 1, (int) q);
        const float w = ImGui::CalcTextSize(info).x + ImGui::GetStyle().ItemSpacing.x;
        ImGui::SameLine(ImGui::GetWindowWidth() - w - ImGui::GetStyle().WindowPadding.x);
        ImGui::TextDisabled("%s", info);

        ImGui::EndMenuBar();
    }

    // timelines on top, details of selected automaton below
    // timeline as high as needed by rows (at least 2 rows), leaving space for details
    const ImGuiStyle &style = ImGui::GetStyle();
    const float row_h = ImGui::GetFrameHeight() * 1.5f;
    const float needed = ImGui::GetTextLineHeightWithSpacing() + row_h * float(std::max<size_t>(2, rig->size()))
                         + 2.f * style.WindowPadding.y + style.ItemSpacing.y;
    const float details_min = 5.5f * ImGui::GetFrameHeightWithSpacing();
    const float timeline_h = std::max(2.f * row_h,
                                      std::min(needed, ImGui::GetContentRegionAvail().y - details_min));
    ImGui::BeginChild("##AutomatonTimelines", ImVec2(0, timeline_h), true);
    RenderTimelines(rig);
    ImGui::EndChild();

    RenderDetails(rig, rig->selected());

    ImGui::End();
}

void AutomatonControlWindow::RenderTimelines (AutomatonRig *rig)
{
    if (rig->empty()) {
        ImGui::Spacing();
        ImGui::TextDisabled("No automaton in this session.");
        ImGui::TextDisabled("Insert one from the menu (in Mixing, Geometry or Layer view).");
        return;
    }

    ImDrawList *dl = ImGui::GetWindowDrawList();
    const ImGuiStyle &style = ImGui::GetStyle();
    const float label_w = 11.f * ImGui::GetTextLineHeight();
    const float row_h = ImGui::GetFrameHeight() * 1.5f;
    const float header_h = ImGui::GetTextLineHeightWithSpacing();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float x0 = origin.x + label_w;
    const float x1 = origin.x + width - style.ItemSpacing.x;
    const float bottom = origin.y + header_h + row_h * float(rig->size());

    // the width of the timeline is the longest cycle
    double span = 1.0;
    for (auto it = rig->begin(); it != rig->end(); ++it)
        span = std::max(span, (*it)->period());
    // timeline shows the current page of time
    const double beats = rig->beats();
    const double page = std::floor(beats / span) * span;
    auto X = [&](double t) { return x0 + (x1 - x0) * float((t - page) / span); };

    const ImU32 col_grid = ImGui::GetColorU32(ImGuiCol_Text, 0.12f);
    const ImU32 col_bar  = ImGui::GetColorU32(ImGuiCol_Text, 0.35f);
    const ImU32 col_text = ImGui::GetColorU32(ImGuiCol_TextDisabled);

    // beats grid, with bars of the metronome quantum
    const double quantum = std::max(1.0, Metronome::manager().quantum());
    for (int k = 0; k <= int(std::ceil(span)); ++k) {
        const double t = page + k;
        const float x = X(t);
        if (x > x1 + 1.f)
            break;
        const double in_bar = t - quantum * std::floor(t / quantum);
        const bool bar = in_bar < 0.5;
        dl->AddLine(ImVec2(x, origin.y + header_h * 0.8f), ImVec2(x, bottom), bar ? col_bar : col_grid);
        if (k < int(std::ceil(span))) {
            char txt[16];
            snprintf(txt, 16, "%d", int(std::round(in_bar)) + 1);
            dl->AddText(ImVec2(x + 3.f, origin.y), bar ? col_bar : col_text, txt);
        }
    }

    // marks of events across all rows (drawn faint, on top of rows)
    std::vector<float> marks;

    int i = 0;
    for (auto it = rig->begin(); it != rig->end(); ++it, ++i) {
        Automaton *a = *it;
        const float y = origin.y + header_h + row_h * float(i);
        const bool selected = rig->selected() == a;
        ImGui::PushID( (int) (a->id() & 0x7FFFFFFF) );

        // enable and select
        ImGui::SetCursorScreenPos(ImVec2(origin.x, y + 0.5f * (row_h - ImGui::GetFrameHeight())));
        bool on = a->enabled();
        if (ImGui::Checkbox("##enabled", &on)) {
            rig->setEnabled(a, on);
            Action::manager().store(a->name() + (on ? ": Enable" : ": Disable"));
        }
        if (ImGui::IsItemHovered())
            ImGuiToolkit::ToolTip(on ? "Disable" : "Enable");
        ImGui::SameLine();
        const std::string label = std::string(Automaton::Types[a->type()].first) + " " + a->name()
                                  + (a->paused() ? "  " ICON_FA_PAUSE : (a->resuming() ? "  " ICON_FA_HOURGLASS_HALF : ""));
        // dimmed if its workspace is hidden
        const bool shown = Mixer::manager().viewSpaceShown(a->viewSpace());
        if (!shown)
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        // select, and show only its workspace (to interact with it in the view)
        auto select = [rig, a]() {
            rig->setSelected(a);
            Mixer::manager().showViewSpace(a->viewSpace(), true, true);
        };
        if (ImGui::Selectable(label.c_str(), selected, 0, ImVec2(x0 - ImGui::GetCursorScreenPos().x - style.ItemSpacing.x, 0)))
            select();
        if (!shown)
            ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            ImGuiToolkit::ToolTip( (Settings::application.views[a->mode()].name + " view, workspace "
                                    + std::to_string(a->viewSpace() + 1) + "\nClic to edit (show only workspace "
                                    + std::to_string(a->viewSpace() + 1) + ")").c_str() );

        // clic in the timeline also selects
        ImGui::SetCursorScreenPos(ImVec2(x0, y));
        if (ImGui::InvisibleButton("##row", ImVec2(std::max(1.f, x1 - x0), row_h)))
            select();

        ImGui::PopID();

        // row background
        const ImU32 col_row = ImGui::GetColorU32(selected ? ImGuiCol_FrameBgActive : ImGuiCol_FrameBg);
        dl->AddRectFilled(ImVec2(x0, y + 2.f), ImVec2(x1, y + row_h - 2.f), col_row, style.FrameRounding);

        const ImU32 col_curve = ImGui::GetColorU32(AUTOMATON_IMGUI_COLOR(a->enabled() ? 1.f : 0.35f));
        const ImU32 col_mark  = ImGui::GetColorU32(AUTOMATON_IMGUI_COLOR(a->enabled() ? 0.8f : 0.3f));

        // motion curve over the page of time
        const float top = y + 5.f, low = y + row_h - 5.f;
        const int N = std::max(8, int((x1 - x0) / 2.f));
        std::vector<ImVec2> points;
        points.reserve(N + 1);
        for (int k = 0; k <= N; ++k) {
            const double t = page + span * double(k) / double(N);
            const float u = float(AutomatonMath::progress(t, a->period(), a->offset()));
            points.push_back( ImVec2(X(t), low - (low - top) * a->curve(u)) );
        }
        dl->AddPolyline(points.data(), (int) points.size(), col_curve, false, 1.5f);

        // events: start of cycles and remarkable instants
        std::vector<float> events = a->events();
        events.insert(events.begin(), 0.f);
        const double c0 = AutomatonMath::cycle(page, a->period(), a->offset());
        for (double c = c0; c <= c0 + std::ceil(span / a->period()) + 1.0; c += 1.0) {
            for (size_t e = 0; e < events.size(); ++e) {
                const double t = a->offset() + (c + events[e]) * a->period();
                if (t < page - 1e-6 || t > page + span + 1e-6)
                    continue;
                const float x = X(t);
                dl->AddLine(ImVec2(x, y + 2.f), ImVec2(x, y + row_h - 2.f), col_mark, e == 0 ? 2.f : 1.f);
                if (selected)
                    marks.push_back(x);
            }
        }

        // paused: head of the automaton frozen at its progress, repeated on this page
        if (a->paused() || a->resuming()) {
            const double tp = a->time() - a->period() * std::floor((a->time() - page) / a->period());
            const ImU32 col_head = ImGui::GetColorU32(AUTOMATON_IMGUI_COLOR(1.f));
            for (double t = tp; t <= page + span + 1e-6; t += a->period()) {
                const float x = X(t);
                dl->AddLine(ImVec2(x, y + 2.f), ImVec2(x, y + row_h - 2.f), col_head, 2.f);
                const float u = float(AutomatonMath::progress(t, a->period(), a->offset()));
                dl->AddCircleFilled(ImVec2(x, low - (low - top) * a->curve(u)), 4.f, col_head);
            }
            // moment when the motion will resume
            if (a->resuming() && a->resumeTime() != Automaton::RESUME_UNSET) {
                const double tr = a->resumeTime();
                if (tr >= page && tr <= page + span) {
                    const float x = X(tr);
                    dl->AddTriangleFilled(ImVec2(x - 5.f, y + 2.f), ImVec2(x + 5.f, y + 2.f),
                                          ImVec2(x, y + 9.f), col_head);
                }
            }
        }
    }

    // events of selected automaton across all rows
    const ImU32 col_faint = ImGui::GetColorU32(AUTOMATON_IMGUI_COLOR(0.25f));
    for (float x : marks)
        dl->AddLine(ImVec2(x, origin.y + header_h), ImVec2(x, bottom), col_faint);

    // play head
    const float xp = X(beats);
    dl->AddLine(ImVec2(xp, origin.y + header_h * 0.5f), ImVec2(xp, bottom),
                ImGui::GetColorU32(ImGuiToolkit::HighlightColor()), 2.f);

    // reserve space in window
    ImGui::SetCursorScreenPos(ImVec2(origin.x, bottom));
    ImGui::Dummy(ImVec2(width, style.ItemSpacing.y));
}

void AutomatonControlWindow::RenderDetails (AutomatonRig *rig, Automaton *a)
{
    if (a == nullptr) {
        ImGui::TextDisabled("Select an automaton to configure it.");
        return;
    }

    // name
    if (name_id_ != a->id()) {
        name_id_ = a->id();
        strncpy(name_buffer_, a->name().c_str(), 63);
        name_buffer_[63] = '\0';
    }
    ImGui::Text("%s", Automaton::Types[a->type()].first);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(12.f * ImGui::GetTextLineHeight());
    if (ImGui::InputText("##name", name_buffer_, 64, ImGuiInputTextFlags_EnterReturnsTrue) || ImGui::IsItemDeactivatedAfterEdit()) {
        if (strlen(name_buffer_) > 0 && a->name() != name_buffer_) {
            a->setName(name_buffer_);
            Action::manager().store(a->name() + ": Rename");
        }
    }
    ImGui::SameLine();

    // ViewSpace (same as for sources)
    static std::vector< std::pair<int,int> > ws_icon = { {ICON_VI_VIEWSPACE_1},
                                                        {ICON_VI_VIEWSPACE_2},
                                                        {ICON_VI_VIEWSPACE_3},
                                                        {ICON_VI_VIEWSPACE_4} };
    static std::vector< std::string > ws_tooltip = { "Workspace 1\nActs on sources of workspace 1",
                                                     "Workspace 2\nActs on sources of workspace 2",
                                                     "Workspace 3\nActs on sources of workspace 3",
                                                     "Workspace 4\nActs on sources of workspace 4" };
    // linked sources and automaton share the same ViewSpace:
    // with one source linked, both change; with several, the automaton follows its sources
    std::set<uint64_t> linked;
    if (PathAutomaton *pa = dynamic_cast<PathAutomaton *>(a))
        linked = pa->linkedSources();
    int ws_index = (int) a->viewSpace();
    const bool singleviewspace = Mixer::manager().viewSpaceOnlyShown((uint) ws_index);
    if (linked.size() > 1) {
        // cannot change
        ImGuiToolkit::Icon(ws_icon[ws_index].first, ws_icon[ws_index].second, false);
        if (ImGui::IsItemHovered())
            ImGuiToolkit::ToolTip( (std::string("Workspace ") + std::to_string(ws_index + 1) +
                                    "\nSame as its linked sources").c_str() );
    }
    else if (ImGuiToolkit::IconMultistate(ws_icon, &ws_index, ws_tooltip)) {
        rig->lock();
        a->setViewSpace((uint) ws_index);
        rig->unlock();
        // the source linked goes too (and other automatons follow it)
        Source *s = linked.empty() ? nullptr : Mixer::manager().findSource(*linked.begin());
        if (s)
            Mixer::manager().setViewSpace(s, (uint) ws_index);
        Action::manager().store(a->name() + ": Workspace " + std::to_string(ws_index + 1));
        // keep the automaton visible if its workspace was the only one shown
        if ( singleviewspace )
            Mixer::manager().showViewSpace((uint) ws_index, true, true);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s in %s view", Automaton::Types[a->type()].second.c_str(),
                        Settings::application.views[a->mode()].name.c_str());

    ImGui::Columns(2, "##AutomatonDetails", false);
    const float item_w = 9.f * ImGui::GetTextLineHeight();

    // period in beats (steps of quarters of beat)
    float p = (float) a->period();
    ImGui::SetNextItemWidth(item_w);
    if (ImGui::DragFloat("##period", &p, 0.02f, 0.25f, 64.f, "%.2f beats"))
        a->setPeriod( std::max(0.25, std::round(double(p) * 4.0) / 4.0) );
    if (ImGui::IsItemDeactivatedAfterEdit())
        Action::manager().store(a->name() + ": Period");
    ImGui::SameLine(0, 2);
    if (ImGui::Button("/2") && a->period() > 0.25) {
        a->setPeriod( std::max(0.25, a->period() * 0.5) );
        Action::manager().store(a->name() + ": Period");
    }
    ImGui::SameLine(0, 2);
    if (ImGui::Button("x2") && a->period() < 64.0) {
        a->setPeriod( std::min(64.0, a->period() * 2.0) );
        Action::manager().store(a->name() + ": Period");
    }
    ImGui::SameLine();
    ImGui::Text("Cycle");

    // phase offset in beats
    float o = (float) a->offset();
    ImGui::SetNextItemWidth(item_w);
    if (ImGui::DragFloat("##offset", &o, 0.02f, -64.f, 64.f, "%+.2f beats"))
        a->setOffset( std::round(double(o) * 4.0) / 4.0 );
    if (ImGui::IsItemDeactivatedAfterEdit())
        Action::manager().store(a->name() + ": Phase");
    ImGui::SameLine();
    ImGui::Text("Phase");

    // shape of motion
    int shape = a->shape();
    ImGui::SetNextItemWidth(item_w);
    if (ImGui::Combo("Motion", &shape, "Linear\0Smooth\0Pulse\0")) {
        a->setShape(shape);
        Action::manager().store(a->name() + ": Motion " + Automaton::Shapes[shape]);
    }

    // loop or play one cycle
    bool loop = a->loop();
    if (ImGui::Checkbox("Loop", &loop)) {
        a->setLoop(loop);
        Action::manager().store(a->name() + (loop ? ": Loop" : ": One cycle"));
    }
    if (ImGui::IsItemHovered())
        ImGuiToolkit::ToolTip(loop ? "Repeat cycles indefinitely" : "Play one cycle and pause");
    if (a->type() < Automaton::AUTOMATON_HEART)
        ImGui::SameLine();

    // direction
    if (a->type() < Automaton::AUTOMATON_HEART) {
        bool r = a->reverse();
        if (ImGui::Checkbox(a->type() == Automaton::AUTOMATON_CIRCULAR ? "Counter-clockwise" : "Reverse", &r)) {
            a->setReverse(r);
            Action::manager().store(a->name() + ": Direction");
        }
    }
    // key points
    if (FreePathAutomaton *fp = dynamic_cast<FreePathAutomaton *>(a)) {
        ImGui::SameLine();
        if (ImGui::SmallButton(ICON_FA_PLUS " Key")) {
            rig->lock();
            fp->addKey();
            rig->unlock();
            Action::manager().store(a->name() + ": Add key");
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(ICON_FA_MINUS " Key") && fp->controls().size() > 2) {
            rig->lock();
            fp->removeKey();
            rig->unlock();
            Action::manager().store(a->name() + ": Remove key");
        }
    }

    // play / pause
    if (ImGui::Button(a->paused() ? ICON_FA_PLAY "  Play " : ICON_FA_PAUSE "  Pause")) {
        a->setPaused( !a->paused() );
        Action::manager().store(a->name() + (a->paused() ? ": Pause" : ": Play"));
    }
    if (ImGui::IsItemHovered())
        ImGuiToolkit::ToolTip(a->paused() ? (a->loop() ? "Continue motion from where it stopped, at the next beat"
                                                       : "Play until the end of the cycle, from the next beat")
                                          : (a->resuming() ? "Waiting for the next beat to continue"
                                                           : "Stop motion (still acting on sources)"));
    ImGui::SameLine();

    // delete
    if (ImGui::Button(ICON_FA_TRASH "  Delete")) {
        const std::string label = a->name() + ": Delete";
        rig->remove(a);
        Action::manager().store(label);
        ImGui::Columns(1);
        return;
    }

    ImGui::NextColumn();

    // links
    if (PathAutomaton *pa = dynamic_cast<PathAutomaton *>(a)) {
        if (pa->links().empty()) {
            ImGui::TextDisabled("No link.");
            ImGui::TextWrapped("Drag the moving part of the automaton in the view, and release on a source "
                               "(or on a handle of the current source in Geometry view).");
        }
        else {
            ImGui::BeginChild("##AutomatonLinks", ImVec2(0, 0), false);
            AutomatonLink to_remove;
            bool remove = false;
            int k = 0;
            for (auto &l : pa->links()) {
                ImGui::PushID(k++);
                if (ImGuiToolkit::IconButton(ICON_FA_UNLINK, "Unlink")) {
                    to_remove = l;
                    remove = true;
                }
                ImGui::SameLine();
                Source *s = Mixer::manager().findSource(l.source);
                if (s)
                    ImGui::Text("%s  %s", s->name().c_str(), AutomatonRig::linkLabel(l).c_str());
                else
                    ImGui::TextDisabled("(missing source)  %s", AutomatonRig::linkLabel(l).c_str());
                ImGui::PopID();
            }
            ImGui::EndChild();
            if (remove) {
                rig->unlink(pa, to_remove);
                Action::manager().store(a->name() + ": Unlink");
            }
        }
    }
    else {
        ImGui::TextDisabled("Acts on all sources");
        ImGui::TextDisabled("of the %s view.", Settings::application.views[a->mode()].name.c_str());
        ImGui::TextWrapped("Move it with the cross in the center; adjust its size with the grips.");
    }

    ImGui::Columns(1);
}
