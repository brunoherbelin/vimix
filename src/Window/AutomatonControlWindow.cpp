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
#include "imgui.h"
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
#include "UserInterfaceManager.h"

#include "AutomatonControlWindow.h"

#define AUTOMATON_IMGUI_COLOR(a) ImVec4(COLOR_AUTOMATON, a)
#define AUTOMATON_ALPHA_CURVE          1.0f   // timeline curve
#define AUTOMATON_ALPHA_CURVE_PAUSED   0.4f   // timeline curve of a paused automaton
#define AUTOMATON_ALPHA_CURVE_DISABLED 0.3f  // timeline curve of a disabled automaton
#define AUTOMATON_ALPHA_MARK           0.8f   // timeline marks of events
#define AUTOMATON_ALPHA_HEAD           1.0f   // timeline head of a paused automaton
#define AUTOMATON_ALPHA_GRID_HEAD      0.5f   // play head of the metronome in the beats grid
#define AUTOMATON_ALPHA_MARK_DISABLED  0.3f   // timeline marks of events of a disabled automaton

AutomatonControlWindow::AutomatonControlWindow () : WorkspaceWindow("Kinetics"), name_id_(0), name_editing_(0), name_focus_(false), drag_offset_(0.0), drag_period_(4.0), drag_anchor_(0.0), drag_anchor_x_(0.f), drag_span_(0.0), drag_page_(0.0), drag_mode_(1), period_slider_(2.f), period_sliding_(false)
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
            // global disable (temporary: automatons do not act on sources)
            ImGui::MenuItem( ICON_FA_BAN "  Disable", nullptr, &Settings::application.automatons_disabled);

            ImGui::Separator();
            if ( ImGui::MenuItem( ICON_FA_PLAY "  Play all", nullptr, false, rig->anyPaused()) ) {
                rig->setPaused(false);
                Action::manager().store("Kinetics: Play all");
            }
            if ( ImGui::MenuItem( ICON_FA_PAUSE "  Pause all", nullptr, false, !rig->empty() && !rig->allPaused()) ) {
                rig->setPaused(true);
                Action::manager().store("Kinetics: Pause all");
            }
            if ( ImGui::MenuItem( ICON_FA_FAST_BACKWARD "  Restart all", nullptr, false, !rig->empty()) ) {
                rig->restart();
                Action::manager().store("Kinetics: Restart all");
            }
            if ( ImGui::MenuItem( ICON_FA_TRASH "  Delete all", nullptr, false, !rig->empty()) ) {
                // (linked sources stay where they are, see AutomatonRig::remove)
                const std::vector<Automaton *> all(rig->begin(), rig->end());
                for (auto a : all)
                    rig->remove(a);
                Action::manager().store("Kinetics: Delete all");
            }

            ImGui::Separator();
            if ( ImGui::MenuItem( MENU_CLOSE,  SHORTCUT_AUTOMATONS) )
                Settings::application.widget.automatons = false;

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(ICON_FA_PLUS "  Insert", can_insert))
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
        // followed by the icon of the Timer window (to open it)
        const float w_icon = ImGui::CalcTextSize(ICON_FA_CLOCK).x + ImGui::GetStyle().ItemSpacing.x;
        const float w = ImGui::CalcTextSize(info).x + ImGui::GetStyle().ItemSpacing.x + w_icon;
        ImGui::SameLine(ImGui::GetWindowWidth() - w - ImGui::GetStyle().WindowPadding.x);
        ImGui::TextDisabled("%s", info);
        ImGui::SameLine();
        if (ImGuiToolkit::IconButton(ICON_FA_CLOCK, "Timer", SHORTCUT_TIMER))
            UserInterface::manager().timercontrol.setVisible(true);

        ImGui::EndMenuBar();
    }

    // timelines on top, control bar of selected automaton at the bottom of the window
    const ImGuiContext& g = *GImGui;
    const float bar_height = g.FontSize + g.Style.FramePadding.y * 4.0f;
    const float timeline_h = std::max(bar_height, ImGui::GetContentRegionAvail().y - bar_height - g.Style.ItemSpacing.y - g.Style.FramePadding.y);
    ImGui::BeginChild("##AutomatonTimelines", ImVec2(0, timeline_h));
    RenderTimelines(rig);
    // clic in the background deselects
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered())
        rig->setSelected(nullptr);
    ImGui::EndChild();

    DrawControlBar(rig, rig->selected());

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
    // same height as the timelines of the media player (double line)
    const float row_h = (ImGui::GetFontSize() + style.FramePadding.y) * 2.f;
    const float header_h = ImGui::GetTextLineHeightWithSpacing();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float x0 = origin.x + label_w;
    // two columns of small stacked buttons on the right (minimal margin for the icon of last column)
    const float period_w = 0.5f * row_h;
    const float x1 = origin.x + width - 2.f * period_w - 4.f - std::max(0.f, ImGui::GetFrameHeight() - period_w);
    const float xb = x1 + 2.f;                                   // drag mode buttons
    const float xs = xb + period_w + style.ItemInnerSpacing.x;                        // reset phase and shape buttons
    const float bottom = origin.y + header_h + row_h * float(rig->size());
    // line of time in seconds below the rows (same height as the line of beats)
    const float footer_h = header_h;

    // the width of the timeline is the longest cycle
    double span = 1.0;
    for (auto it = rig->begin(); it != rig->end(); ++it)
        span = std::max(span, (*it)->period());
    // timeline shows the current page of time
    const double beats = rig->beats();
    double page = std::floor(beats / span) * span;
    // unchanged while dragging a timeline (to not rescale under the mouse)
    if (drag_span_ > 0.0 && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
        drag_span_ = 0.0;
    if (drag_span_ > 0.0) {
        span = drag_span_;
        page = drag_page_;
    }
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

    // play head of the metronome in the beats grid (short, semi transparent)
    if (beats >= page && beats <= page + span) {
        ImVec4 c = ImGuiToolkit::HighlightColor();
        c.w *= AUTOMATON_ALPHA_GRID_HEAD;
        const float x = X(beats);
        dl->AddLine(ImVec2(x, origin.y), ImVec2(x, origin.y + header_h), ImGui::GetColorU32(c), 2.f);
    }

    // marks of events across all rows (drawn faint, on top of rows)
    std::vector<float> marks;

    int i = 0;
    for (auto it = rig->begin(); it != rig->end(); ++it, ++i) {
        Automaton *a = *it;
        const float y = origin.y + header_h + row_h * float(i);
        const bool selected = rig->selected() == a;
        ImGui::PushID( (int) (a->id() & 0x7FFFFFFF) );

        // items of the row are vertically centered (placed one after the other on their own line,
        // to avoid the text baseline offset of SameLine)
        const float yc = y + 0.5f * row_h;
        float xc = origin.x;
        auto place = [&](float h) { ImGui::SetCursorScreenPos(ImVec2(xc, yc - 0.5f * h)); };
        auto next  = [&]() { xc = ImGui::GetItemRectMax().x + style.ItemSpacing.x; };

        // icon of the view of the automaton
        place(a->mode() == View::LAYER ? ImGui::GetTextLineHeightWithSpacing() : ImGui::GetTextLineHeight());
        // enabled if it is the current view
        const bool inview = Mixer::manager().view()->mode() == a->mode();
        if (a->mode() == View::LAYER)
            ImGuiToolkit::Icon(ICON_VI_LAYERS, inview);
        else if (inview)
            ImGui::Text("%s", a->mode() == View::MIXING ? ICON_FA_BULLSEYE : ICON_FA_OBJECT_UNGROUP);
        else
            ImGui::TextDisabled("%s", a->mode() == View::MIXING ? ICON_FA_BULLSEYE : ICON_FA_OBJECT_UNGROUP);
        if (ImGui::IsItemHovered())
            ImGuiToolkit::ToolTip( (Settings::application.views[a->mode()].name + " view").c_str() );
        next();

        // ViewSpace (same as for sources)
        static std::vector< std::pair<int,int> > ws_icon = { {ICON_VI_VIEWSPACE_1},
                                                            {ICON_VI_VIEWSPACE_2},
                                                            {ICON_VI_VIEWSPACE_3},
                                                            {ICON_VI_VIEWSPACE_4} };
        static std::vector< std::string > ws_tooltip = { "Workspace 1", "Workspace 2",
                                                         "Workspace 3", "Workspace 4" };
        // linked sources and automaton share the same ViewSpace:
        // with one source linked, both change; with several, the automaton follows its sources
        std::set<uint64_t> linked;
        if (PathAutomaton *pa = dynamic_cast<PathAutomaton *>(a))
            linked = pa->linkedSources();
        int ws_index = (int) a->viewSpace();
        const bool singleviewspace = Mixer::manager().viewSpaceOnlyShown((uint) ws_index);
        place(linked.size() > 1 ? ImGui::GetTextLineHeightWithSpacing() : ImGui::GetFrameHeight());
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
            if ( singleviewspace ){
                rig->setSelected(a);
                Mixer::manager().showViewSpace(a->viewSpace(), true, true);
                if (Mixer::manager().view()->mode() != a->mode())
                    Mixer::manager().setView(a->mode());
            };
        }
        next();

        const std::string label = std::string(Automaton::Types[a->type()].first) + " " + a->name();
        // dimmed if its workspace is hidden
        const bool shown = Mixer::manager().viewSpaceShown(a->viewSpace());
        if (!shown)
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        if (!a->active()) {
            ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));
        }
        // select, and show only its workspace (to interact with it in the view)
        auto select = [rig, a]() {
            rig->setSelected(a);
            Mixer::manager().showViewSpace(a->viewSpace(), true, true);
            if (Mixer::manager().view()->mode() != a->mode())
                Mixer::manager().setView(a->mode());
        };
        // selectable of the height of the row background, with text centered
        // (or input text to edit the name)
        const float w_label = x0 - xc - style.ItemSpacing.x;
        if (name_editing_ == a->id()) {
            ImGui::SetCursorScreenPos(ImVec2(xc, yc - 0.5f * ImGui::GetFrameHeight()));
            ImGui::SetNextItemWidth(w_label);
            if (name_focus_) {
                ImGui::SetKeyboardFocusHere();
                name_focus_ = false;
            }
            if (ImGui::InputText("##rowname", name_buffer_, 64, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)
                || ImGui::IsItemDeactivatedAfterEdit()) {
                if (strlen(name_buffer_) > 0 && a->name() != name_buffer_) {
                    a->setName(name_buffer_);
                    Action::manager().store(a->name() + ": Rename");
                }
                name_editing_ = 0;
            }
            // back to selectable when leaving the input
            if (ImGui::IsItemDeactivated())
                name_editing_ = 0;
        }
        else {
            ImGui::SetCursorScreenPos(ImVec2(xc, y + 2.f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, 0.f));
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
            if (ImGui::Selectable(label.c_str(), selected, 0,  ImVec2(w_label, row_h - 4.f)))
                select();
            ImGui::PopStyleVar(2);
            // edit name button, right aligned on the selectable of the selected automaton
            if (selected) {
                ImGui::SetItemAllowOverlap();
                ImGui::SetCursorScreenPos(ImVec2(xc + w_label - 0.3f * ImGui::GetFrameHeight(), yc - 0.3f * ImGui::GetFrameHeight()));
                if (ImGuiToolkit::IconButton(ICON_FA_I_CURSOR, "Rename")) {
                    name_editing_ = a->id();
                    name_focus_ = true;
                    name_id_ = a->id();
                    strncpy(name_buffer_, a->name().c_str(), 63);
                    name_buffer_[63] = '\0';
                }
            }
        }
        if (!a->active())
            ImGui::PopStyleColor(3);
        if (!shown)
            ImGui::PopStyleColor();


        // clic in the timeline also selects
        ImGui::SetCursorScreenPos(ImVec2(x0, y));
        if (ImGui::InvisibleButton("##row", ImVec2(std::max(1.f, x1 - x0), row_h)))
            select();
        // drag the timeline of the selected automaton to
        // - shift its phase (by quarters of beat)
        // - or scale its cycle duration, stretching from the start of the cycle
        //   at the left of the timeline (fixed when starting to drag)
        if (selected) {
            if (ImGui::IsItemActivated()) {
                drag_offset_ = a->offset();
                drag_period_ = a->period();
                drag_anchor_ = drag_offset_ + std::floor((page - drag_offset_) / drag_period_) * drag_period_;
                drag_anchor_x_ = X(drag_anchor_);
                drag_span_ = span;
                drag_page_ = page;
            }
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                const ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
                if (drag_mode_ == 0) {
                    const double d = double(delta.x / (x1 - x0)) * span;
                    a->setOffset( std::round((drag_offset_ + d) * 4.0) / 4.0 );
                }
                else {
                    // ratio of distances to the anchor (grab point follows the mouse)
                    const float xg = std::max(ImGui::GetIO().MouseClickedPos[ImGuiMouseButton_Left].x - drag_anchor_x_, 10.f);
                    const double k = double(std::max(xg + delta.x, 1.f) / xg);
                    // whole beats (quarters of beat below one beat)
                    double p = drag_period_ * k;
                    p = p < 1.0 ? std::round(p * 4.0) / 4.0 : std::round(p);
                    p = std::min(64.0, std::max(0.25, p));
                    // a cycle still starts at the anchor (exact: no rounding needed)
                    a->setPeriod(p);
                    a->setOffset( drag_anchor_ - p * std::floor(drag_anchor_ / p) );
                }
            }
            if (ImGui::IsItemDeactivated()) {
                drag_span_ = 0.0;
                if (a->period() != drag_period_)
                    Action::manager().store(a->name() + ": Period");
                else if (a->offset() != drag_offset_)
                    Action::manager().store(a->name() + ": Phase");
            }
            if (ImGui::IsItemHovered() || ImGui::IsItemActive())
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
        if (ImGui::IsItemHovered()) {
            char info[64];
            if (std::abs(a->offset()) < 1e-6)
                snprintf(info, 64, "%g beat%s / cycle, no phase shift", a->period(), a->period() == 1.0 ? "" : "s");
            else
                snprintf(info, 64, "%g beat%s / cycle, shift by %+g", a->period(), a->period() == 1.0 ? "" : "s", a->offset());
            ImGuiToolkit::ToolTip(info);
        }

        // mode of drag on timeline (selected automaton only): toggle up / down
        if (selected) {
            // filled rect on the active mode
            const float ym = y + 0.5f * row_h;
            dl->AddRectFilled(ImVec2(xb, drag_mode_ == 0 ? y + 2.f : ym), ImVec2(xb + period_w, drag_mode_ == 0 ? ym : y + row_h - 2.f),
                              ImGui::GetColorU32(ImGuiCol_FrameBg), style.FrameRounding);
            ImGui::SetCursorScreenPos(ImVec2(xb, y + 1.f));
            if (ImGuiToolkit::IconButton(ICON_VI_PHASE_SHIFT, "Phase shift\nDrag timeline to shift cycle in time"))
                drag_mode_ = 0;
            ImGui::SetCursorScreenPos(ImVec2(xb, ym));
            if (ImGuiToolkit::IconButton(ICON_VI_PHASE_SCALE, "Scale cycle\nDrag timeline to change cycle duration"))
                drag_mode_ = 1;

            // reset phase shift (no background)
            ImGui::SetCursorScreenPos(ImVec2(xs, y + 1.f));
            if (ImGuiToolkit::IconButton(ICON_VI_PHASE_RESET, "Reset phase shift") && a->offset() != 0.0) {
                a->setOffset(0.0);
                Action::manager().store(a->name() + ": Phase");
            }
            // shape of the motion curve
            static std::vector< std::pair<int, int> > icons_shape = { {ICON_VI_MOTION_LINEAR}, {ICON_VI_MOTION_SMOOTH}, {ICON_VI_MOTION_PULSE} };
            static std::vector< std::string > tooltips_shape = { "Linear curve", "Smooth curve", "Pulse curve" };
            ImGui::SetCursorScreenPos(ImVec2(xs, ym));
            int shape = a->shape();
            if (ImGuiToolkit::IconMultistate(icons_shape, &shape, tooltips_shape)) {
                a->setShape(shape);
                Action::manager().store(a->name() + ": Curve " + Automaton::Shapes[shape]);
            }

        }

        ImGui::PopID();

        // row background
        const ImU32 col_row = ImGui::GetColorU32(ImGuiCol_FrameBgActive, selected ? 1.3f : a->active() ? 0.5f : 0.1f);
        dl->AddRectFilled(ImVec2(x0, y + 2.f), ImVec2(x1, y + row_h - 2.f), col_row, style.FrameRounding);

        const ImU32 col_curve = ImGui::GetColorU32(AUTOMATON_IMGUI_COLOR(!a->active() ? AUTOMATON_ALPHA_CURVE_DISABLED : (a->paused() ? AUTOMATON_ALPHA_CURVE_PAUSED : AUTOMATON_ALPHA_CURVE)));
        const ImU32 col_mark  = ImGui::GetColorU32(AUTOMATON_IMGUI_COLOR(a->active() ? AUTOMATON_ALPHA_MARK : AUTOMATON_ALPHA_MARK_DISABLED));
        const float width_curve = selected ? 2.5f : 1.5f;

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
        dl->AddPolyline(points.data(), (int) points.size(), col_curve, false, width_curve);

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

        // play head of the automaton: moving with beats when playing,
        // otherwise frozen at the location of pause (first occurrence in this page)
        // (gray if disabled, like the selectable of the row)
        const ImU32 col_head = a->active() ? ImGui::GetColorU32(ImGuiToolkit::HighlightColor())
                                            : ImGui::GetColorU32(ImGuiCol_FrameBg);
        const bool still = a->paused() || a->resuming();
        double th = beats;
        if (still)
            th = a->time() - a->period() * std::floor((a->time() - page) / a->period());
        if (th >= page - 1e-6 && th <= page + span + 1e-6) {
            const float x = X(th);
            if (!a->resuming())
                dl->AddLine(ImVec2(x, y + 2.f), ImVec2(x, y + row_h - 2.f), col_head, 2.f);
            dl->AddCircleFilled(ImVec2(x, low - (low - top) * a->curve(float(a->progress()))), 4.f, col_head);
        }
        // moment when the motion will resume
        if (a->resuming() && a->resumeTime() != Automaton::RESUME_UNSET) {
            const double tr = a->resumeTime();
            if (tr >= page && tr <= page + span) {
                const float x = X(tr);
                dl->AddLine(ImVec2(x, y + 2.f), ImVec2(x, y + row_h - 2.f), col_head, 2.f);
                dl->AddTriangleFilled(ImVec2(x - 6.f, y + 2.f), ImVec2(x + 6.f, y + 2.f),
                                      ImVec2(x, y + 11.f), col_head);
            }
        }
    }

    // events of selected automaton across all rows
    const ImU32 col_faint = ImGui::GetColorU32(AUTOMATON_IMGUI_COLOR(0.25f));
    for (float x : marks)
        dl->AddLine(ImVec2(x, origin.y + header_h), ImVec2(x, bottom), col_faint);

    // time in seconds from the start of the page, below the rows
    const double tempo = Metronome::manager().tempo();
    if (tempo > 0.0) {
        const double seconds = span * 60.0 / tempo;
        // step between ticks: enough space for their labels
        const float min_dx = ImGui::CalcTextSize("00.00s").x + 6.f;
        static const double steps[] = { 0.1, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 15.0, 30.0, 60.0, 120.0, 300.0 };
        double step = steps[0];
        for (double s : steps) {
            step = s;
            if ((x1 - x0) * float(s / seconds) >= min_dx)
                break;
        }
        auto XS = [&](double s) { return x0 + (x1 - x0) * float(s / seconds); };
        const float yt = bottom + 2.f;
        // total duration of the timeline, right aligned after the timeline
        char total[32];
        if (seconds >= 60.0)
            snprintf(total, 32, "%d:%04.1f", int(seconds) / 60, std::fmod(seconds, 60.0));
        else
            snprintf(total, 32, "%.2fs", seconds);
        const float x_total = origin.x + width - ImGui::CalcTextSize(total).x;
        dl->AddText(ImVec2(x_total, yt), col_bar, total);
        for (int k = 0; k * step <= seconds + 1e-6; ++k) {
            const double s = k * step;
            const float x = XS(s);
            dl->AddLine(ImVec2(x, bottom), ImVec2(x, bottom + footer_h * 0.5f), col_grid, 2.f);
            char txt[16];
            if (s >= 60.0)
                snprintf(txt, 16, "%d:%02d", int(s) / 60, int(s) % 60);
            else
                snprintf(txt, 16, "%gs", s);
            // no label overflowing the timeline
            if (x + 5.f + ImGui::CalcTextSize(txt).x <= std::min(x1, x_total - 3.f))
                dl->AddText(ImVec2(x + 5.f, yt), col_text, txt);
        }
    }

    // reserve space in window
    ImGui::SetCursorScreenPos(ImVec2(origin.x, bottom + footer_h));
    ImGui::Dummy(ImVec2(width, style.ItemSpacing.y));
}

bool AutomatonControlWindow::DrawControlBar (AutomatonRig *rig, Automaton *a)
{
    const ImGuiContext& g = *GImGui;
    const float h_space = g.Style.ItemInnerSpacing.x;
    const float v_space = g.Style.FramePadding.y;
    const float bar_height = g.FontSize + v_space * 4.0f;
    const float bar_width = ImGui::GetContentRegionAvail().x;

    // draw box
    const ImVec2 top = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(top, top + ImVec2(bar_width, bar_height), ImGui::GetColorU32(ImGuiCol_FrameBg), h_space);

    // buttons style
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.14f, 0.14f, 0.14f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.24f, 0.24f, 0.24f, 0.2f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.14f, 0.14f, 0.14f, 0.5f));

    ImGui::SetCursorScreenPos(top + ImVec2(h_space, v_space));

    // no automaton: disabled bar
    if (a == nullptr) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.f, 0.f, 0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.f, 0.f, 0.f, 0.f));
        ImGui::Button(ICON_FA_FAST_BACKWARD);
        ImGui::SameLine(0, h_space);
        ImGui::Button(ICON_FA_PLAY, ImVec2(ImGui::GetFrameHeightWithSpacing(), 0));
        ImGui::PopStyleColor(3 + 5);
        ImGui::SetCursorScreenPos(top + ImVec2(0.f, bar_height + v_space));
        return false;
    }

    // restart: back to start of cycle
    if (ImGui::Button(ICON_FA_FAST_BACKWARD)) {
        rig->lock();
        a->restart( Metronome::manager().beats() );
        rig->unlock();
        Action::manager().store(a->name() + ": Restart");
    }
    if (ImGui::IsItemHovered())
        ImGuiToolkit::ToolTip(a->paused() ? "Go to start of cycle"
                                          : "Restart cycle at the next beat");

    // play / pause toggle
    ImGui::SameLine(0, h_space);
    if (ImGui::Button(a->paused() ? ICON_FA_PLAY : ICON_FA_PAUSE,
                      ImVec2(ImGui::GetFrameHeightWithSpacing(), 0))) {
        rig->lock();
        a->setPaused( !a->paused() );
        rig->unlock();
        Action::manager().store(a->name() + (a->paused() ? ": Pause" : ": Play"));
    }
    if (ImGui::IsItemHovered())
        ImGuiToolkit::ToolTip(a->paused() ? (a->loop() ? "Play\nContinue motion from where it stopped, at the next beat"
                                                       : "Play\nPlay until the end of the cycle, from the next beat")
                                          : (a->resuming() ? "Pause\nWaiting for the next beat to continue"
                                                           : "Pause\nStop motion (still acting on sources)"));

    // waiting for the next beat to resume
    if (a->resuming()) {
        ImGui::SameLine(0, h_space);
        ImGui::AlignTextToFramePadding();
        ImGui::Text(ICON_FA_HOURGLASS_HALF);
        if (ImGui::IsItemHovered())
            ImGuiToolkit::ToolTip("Waiting for the next beat to continue");
    }

    // right aligned: loop mode
    static std::vector< std::pair<int, int> > icons_loop = { {ICON_VI_LOOP_STOP}, {ICON_VI_LOOP_RESTART} };
    static std::vector< std::string > tooltips_loop = { "One cycle\nPlay one cycle and pause",
                                                        "Loop\nRepeat cycles indefinitely" };
    ImGui::SameLine();
    const bool directional = a->type() < Automaton::AUTOMATON_HEART;  // linear, circular and free path
    // right part: [direction] loop | [linked sources] enable delete
    // the combo of linked sources takes the space left (if enough)
    PathAutomaton *pa = dynamic_cast<PathAutomaton *>(a);
    const float x_left  = ImGui::GetCursorScreenPos().x;
    const float x_right = top.x + bar_width;
    const float w_icons = bar_height * (directional ? 2.f : 1.f);
    const float w_sep   = ImGui::CalcTextSize("|").x + 2.f * h_space;
    float w_combo = 0.f;
    if (pa && !pa->links().empty()) {
        w_combo = std::min(8.f * ImGui::GetTextLineHeight(),
                           x_right - 2.f * bar_height - w_sep - w_icons - x_left - 2.f * h_space);
        if (w_combo < 4.f * ImGui::GetTextLineHeight())
            w_combo = 0.f;
    }
    ImGui::SetCursorScreenPos(ImVec2(x_right - 2.f * bar_height - (w_combo > 0.f ? w_combo + h_space : 0.f) - w_sep - w_icons,
                                     ImGui::GetCursorScreenPos().y));

    // direction (not reversed: clockwise, or starting from first point to second)
    if (directional) {
        static std::vector< std::pair<int, int> > icons_circular = { {ICON_VI_CLOCKWISE}, {ICON_VI_COUNTERCLOCKWISE} };
        static std::vector< std::string > tooltips_circular = { "Clockwise", "Counter-clockwise" };
        static std::vector< std::pair<int, int> > icons_linear = { {ICON_VI_RIGHT_LEFT}, {ICON_VI_LEFT_RIGHT} };
        static std::vector< std::string > tooltips_linear = { "Forward\nStart from first point", "Reverse\nStart from second point" };
        const bool circular = a->type() == Automaton::AUTOMATON_CIRCULAR;
        int current_direction = a->reverse() ? 1 : 0;
        if (ImGuiToolkit::IconMultistate(circular ? icons_circular : icons_linear, &current_direction,
                                         circular ? tooltips_circular : tooltips_linear)) {
            rig->lock();
            a->setReverse(current_direction > 0);
            rig->unlock();
            Action::manager().store(a->name() + ": Direction");
        }
        ImGui::SameLine(0, h_space);
    }

    int current_loop = a->loop() ? 1 : 0;
    if (ImGuiToolkit::IconMultistate(icons_loop, &current_loop, tooltips_loop)) {
        rig->lock();
        a->setLoop(current_loop > 0);
        rig->unlock();
        Action::manager().store(a->name() + (a->loop() ? ": Loop" : ": One cycle"));
    }

    // Separator
    ImGui::SameLine(0, h_space);
    ImGui::Text("|");

    // linked sources: select one to unlink it
    if (w_combo > 0.f) {
        ImGui::SameLine(0, h_space);
        ImGui::SetNextItemWidth(w_combo);
        const size_t n = pa->links().size();
        const std::string label = std::string(ICON_FA_LINK "  ") + std::to_string(n) + (n > 1 ? " sources" : " source");
        if (ImGui::BeginCombo("##AutomatonLinks", label.c_str())) {
            AutomatonLink to_remove;
            bool remove = false;
            int k = 0;
            for (auto &l : pa->links()) {
                ImGui::PushID(k++);
                Source *s = Mixer::manager().findSource(l.source);
                const std::string item = (s ? std::string(s->initials()) + " - " + s->name() : std::string("(missing source)"))
                                         + "  (" + AutomatonRig::linkLabel(l) + ")";
                if (ImGui::MenuItem(item.c_str(), nullptr, true)) {
                    to_remove = l;
                    remove = true;
                }
                if (ImGui::IsItemHovered())
                    ImGuiToolkit::ToolTip("Unlink");
                ImGui::PopID();
            }
            ImGui::EndCombo();
            if (remove) {
                rig->unlink(pa, to_remove);
                Action::manager().store(a->name() + ": Unlink");
            }
        }
        else if (ImGui::IsItemHovered())
            ImGuiToolkit::ToolTip("Linked sources\nSelect one to unlink it");
    }

    // enable
    ImGui::SameLine(0, h_space);
    ImGui::SetCursorScreenPos(ImVec2(x_right - 2.f * bar_height, ImGui::GetCursorScreenPos().y));
    bool on = a->enabled();

    // not available if all automatons are disabled
    const bool all_disabled = Settings::application.automatons_disabled;
    ImGuiToolkit::PushDisabled(all_disabled);
    const char *tooltip[2] = {"Disabled", "Enabled"};
    if ( ImGuiToolkit::IconToggle(ICON_VI_BANNED, ICON_VI_CHECKED, &on, tooltip ) && !all_disabled ){
        rig->setEnabled(a, on);
        Action::manager().store(a->name() + (on ? ": Enable" : ": Disable"));
    }
    ImGuiToolkit::PopDisabled(all_disabled);
    if (all_disabled && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGuiToolkit::ToolTip("All Kinetics are disabled\n(see menu)");


    // delete, at the right end
    ImGui::SameLine(0, h_space);
    ImGui::SetCursorScreenPos(ImVec2(x_right - bar_height, ImGui::GetCursorScreenPos().y));
    const bool del = ImGui::Button(ICON_FA_TRASH);
    if (ImGui::IsItemHovered())
        ImGuiToolkit::ToolTip("Delete automaton");

    ImGui::PopStyleColor(5);

    // move cursor below the bar
    ImGui::SetCursorScreenPos(top + ImVec2(0.f, bar_height + v_space));

    if (del) {
        const std::string label = a->name() + ": Delete";
        rig->remove(a);
        Action::manager().store(label);
    }
    return del;
}
