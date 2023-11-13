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

#include <iostream>
#include <iomanip>

#include <gst/gst.h>

// ImGui
#include "ImGuiToolkit.h"
#include "imgui_internal.h"

#include "defines.h"
#include "Log.h"
#include "Settings.h"
#include "SystemToolkit.h"
#include "DialogToolkit.h"
#include "GstToolkit.h"

#include "Mixer.h"
#include "CloneSource.h"
#include "MediaSource.h"
#include "StreamSource.h"
#include "MediaPlayer.h"
#include "ActionManager.h"
#include "UserInterfaceManager.h"

#include "SourceControlWindow.h"

void DrawSource(Source *s, ImVec2 framesize, ImVec2 top_image, bool withslider = false, bool withinspector = false);
ImRect DrawSourceWithSlider(Source *s, ImVec2 top, ImVec2 rendersize, bool with_inspector);


SourceControlWindow::SourceControlWindow() : WorkspaceWindow("SourceController"),
    min_width_(0.f), h_space_(0.f), v_space_(0.f), scrollbar_(0.f),
    timeline_height_(0.f),  mediaplayer_height_(0.f), buttons_width_(0.f), buttons_height_(0.f),
    play_toggle_request_(false), replay_request_(false), pending_(false),
    active_label_(LABEL_AUTO_MEDIA_PLAYER), active_selection_(-1),
    selection_context_menu_(false), selection_mediaplayer_(nullptr), selection_target_slower_(0), selection_target_faster_(0),
    mediaplayer_active_(nullptr), mediaplayer_edit_fading_(false), mediaplayer_edit_pipeline_(false), mediaplayer_mode_(false), mediaplayer_slider_pressed_(false), mediaplayer_timeline_zoom_(1.f),
    magnifying_glass(false)
{
    info_.setExtendedStringMode();

    captureFolderDialog = new DialogToolkit::OpenFolderDialog("Capture frame Location");
}


void SourceControlWindow::resetActiveSelection()
{
    info_.reset();
    active_selection_ = -1;
    active_label_ = LABEL_AUTO_MEDIA_PLAYER;
    play_toggle_request_ = false;
    replay_request_ = false;
    capture_request_ = false;
}

void SourceControlWindow::setVisible(bool on)
{
    magnifying_glass = false;

    // restore workspace to show the window
    if (WorkspaceWindow::clear_workspace_enabled) {
        WorkspaceWindow::restoreWorkspace(on);
        // do not change status if ask to hide (consider user asked to toggle because the window was not visible)
        if (!on)  return;
    }

    if (Settings::application.widget.media_player_view > 0 && Settings::application.widget.media_player_view != Settings::application.current_view) {
        Settings::application.widget.media_player_view = -1;
        on = true;
    }

    // if no selection in the player and in the source selection, show all sources
    if (on && selection_.empty() && Mixer::selection().empty() )
        selection_ = valid_only( Mixer::manager().session()->getDepthSortedList() );

    Settings::application.widget.media_player = on;
}

bool SourceControlWindow::Visible() const
{
    return ( Settings::application.widget.media_player
             && (Settings::application.widget.media_player_view < 0 || Settings::application.widget.media_player_view == Settings::application.current_view )
             );
}

void SourceControlWindow::Update()
{
    WorkspaceWindow::Update();

    SourceList selectedsources;

    if (Settings::application.widget.media_player == false)
        selection_.clear();

    // get new selection or validate previous list if selection was not updated
    selectedsources = Mixer::manager().validate(selection_);
    if (selectedsources.empty() && !Mixer::selection().empty())
        selectedsources = valid_only(Mixer::selection().getCopy());

    // compute number of source selected and playable
    size_t n_source = selectedsources.size();
    size_t n_play = 0;
    for (auto source = selectedsources.begin(); source != selectedsources.end(); ++source){
        if ( (*source)->active() && (*source)->playing() )
            n_play++;
    }

    //
    // Play button or keyboard [Space] was pressed
    //
    if ( play_toggle_request_ ) {

        for (auto source = selectedsources.begin(); source != selectedsources.end(); ++source)
            (*source)->play( n_play < n_source );
        Action::manager().store( n_play < n_source ? "Sources Play" : "Sources Pause" );

        play_toggle_request_ = false;
    }

    //
    // Replay / rewind button or keyboard [CTRL+Space] was pressed
    //
    if ( replay_request_ ) {

        for (auto source = selectedsources.begin(); source != selectedsources.end(); ++source)
            (*source)->replay();

        replay_request_ = false;
    }

    //
    // return from thread for selecting capture folder
    //
    if (captureFolderDialog->closed() && !captureFolderDialog->path().empty())
        // get the folder from this file dialog
        Settings::application.source.capture_path = captureFolderDialog->path();

    //
    // Capture frame on current selection
    //
    Source *s = nullptr;
    if ( selection_.size() == 1 )
        s = selection_.front();
    if ( s != nullptr) {
        // back from capture of FBO: can save file
        if ( capture.isFull() ){
            std::string filename;
            // if sequencial naming of file is selected
            if (Settings::application.source.capture_naming == 0 )
                filename = SystemToolkit::filename_sequential(Settings::application.source.capture_path, s->name(), "png");
            else
                filename = SystemToolkit::filename_dateprefix(Settings::application.source.capture_path, s->name(), "png");
            // save capture and inform user
            capture.save( filename );
            Log::Notify("Frame saved in %s", filename.c_str() );
        }
        // request capture : initiate capture of FBO
        if ( capture_request_ ) {
            capture.captureFramebuffer( s->frame() );
            capture_request_ = false;
        }
    }

    // reset on session change
    static Session *__session = nullptr;
    if ( Mixer::manager().session() != __session ) {
        __session = Mixer::manager().session();
        resetActiveSelection();
    }
}

void SourceControlWindow::Render()
{
    // estimate window size
    const ImGuiContext& g = *GImGui;
    h_space_ = g.Style.ItemInnerSpacing.x;
    v_space_ = g.Style.FramePadding.y;
    buttons_height_  = g.FontSize + v_space_ * 4.0f ;
    buttons_width_  = g.FontSize * 8.0f ;
    min_width_ = 6.f * buttons_height_;
    timeline_height_ = (g.FontSize + v_space_) * 2.0f ; // double line for each timeline
    scrollbar_ = g.Style.ScrollbarSize;
    // all together: 1 title bar + spacing + 1 toolbar + spacing + 2 timelines + scrollbar
    mediaplayer_height_ = buttons_height_ + 2.f * timeline_height_ + 2.f * scrollbar_ +  v_space_;

    // constraint size
    ImGui::SetNextWindowSizeConstraints(ImVec2(min_width_, 2.f * mediaplayer_height_), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::SetNextWindowPos(ImVec2(1180, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);

    // Window named "SourceController" at instanciation
    if ( !ImGui::Begin(name_, &Settings::application.widget.media_player,
                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar |  ImGuiWindowFlags_NoCollapse ))
    {
        ImGui::End();
        return;
    }

    // menu (no title bar)
    if (ImGui::BeginMenuBar())
    {
        if (ImGuiToolkit::IconButton(4,16)){
            Settings::application.widget.media_player = false;
        }
        if (ImGui::BeginMenu(IMGUI_TITLE_MEDIAPLAYER))
        {
            //
            // Menu section for play control
            //
            if (ImGui::MenuItem( ICON_FA_FAST_BACKWARD "  Restart", CTRL_MOD "Space", nullptr, !selection_.empty()))
                replay_request_ = true;
            if (ImGui::MenuItem( ICON_FA_PLAY "  Play | Pause", "Space", nullptr, !selection_.empty()))
                play_toggle_request_ = true;

            ImGui::Separator();
            //
            // Menu section for display
            //
            if (ImGui::BeginMenu( ICON_FA_IMAGE "  Displayed image"))
            {
                if (ImGuiToolkit::MenuItemIcon(8, 9, " Render"))
                    Settings::application.widget.media_player_slider = 0.0;
                if (ImGuiToolkit::MenuItemIcon(6, 9, " Split"))
                    Settings::application.widget.media_player_slider = 0.5;
                if (ImGuiToolkit::MenuItemIcon(7, 9, " Input"))
                    Settings::application.widget.media_player_slider = 1.0;
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem( ICON_FA_TH "  List all")) {
                selection_.clear();
                resetActiveSelection();
                Mixer::manager().unsetCurrentSource();
                Mixer::selection().clear();
                selection_ = valid_only(Mixer::manager().session()->getDepthSortedList());
            }
            if (ImGui::MenuItem( ICON_FA_MINUS "  Clear")) {
                selection_.clear();
                resetActiveSelection();
                Mixer::manager().unsetCurrentSource();
                Mixer::selection().clear();
            }
            //
            // Menu section for window management
            //
            ImGui::Separator();
            bool pinned = Settings::application.widget.media_player_view == Settings::application.current_view;
            std::string menutext = std::string( ICON_FA_MAP_PIN "    Stick to ") + Settings::application.views[Settings::application.current_view].name + " view";
            if ( ImGui::MenuItem( menutext.c_str(), nullptr, &pinned) ){
                if (pinned)
                    Settings::application.widget.media_player_view = Settings::application.current_view;
                else
                    Settings::application.widget.media_player_view = -1;
            }
            if ( ImGui::MenuItem( MENU_CLOSE, SHORTCUT_PLAYER) ) {
                Settings::application.widget.media_player = false;
                selection_.clear();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(active_label_.c_str()))
        {
            // info on selection status
            size_t N = Mixer::manager().session()->numBatch();
            bool enabled = !selection_.empty() && active_selection_ < 0;

            // Menu : Dynamic selection
            if (ImGui::MenuItem(LABEL_AUTO_MEDIA_PLAYER))
                resetActiveSelection();
            // Menu : store selection
            if (ImGui::MenuItem(ICON_FA_PLUS_CIRCLE LABEL_STORE_SELECTION, NULL, false, enabled))
            {
                active_selection_ = N;
                active_label_ = std::string(ICON_FA_CHECK_CIRCLE "  Batch #") + std::to_string(active_selection_);
                Mixer::manager().session()->addBatch( ids(selection_) );
                info_.reset();
            }
            // Menu : list of selections
            if (N>0) {
                ImGui::Separator();
                for (size_t i = 0 ; i < N; ++i)
                {
                    std::string label = std::string(ICON_FA_CHECK_CIRCLE "  Batch #") + std::to_string(i);
                    if (ImGui::MenuItem( label.c_str() ))
                    {
                        active_selection_ = i;
                        active_label_ = label;
                        info_.reset();
                    }
                }
            }

            ImGui::EndMenu();
        }

        //
        // Menu for capture frame
        //
        if ( ImGui::BeginMenu(ICON_FA_ARROW_ALT_CIRCLE_DOWN "  Capture", selection_.size() == 1 ) )
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_CAPTURE, 0.8f));
            if (ImGui::MenuItem( MENU_CAPTUREFRAME, "F10" ))
                capture_request_ = true;
            ImGui::PopStyleColor(1);

            // separator and hack for extending menu width
            ImGui::Separator();
            ImGui::MenuItem("Settings                            ", nullptr, false, false);

            // path menu selection
            static char* name_path[4] = { nullptr };
            if ( name_path[0] == nullptr ) {
                for (int i = 0; i < 4; ++i)
                    name_path[i] = (char *) malloc( 1024 * sizeof(char));
                snprintf( name_path[1], 1024, "%s", ICON_FA_HOME " Home");
                snprintf( name_path[2], 1024, "%s", ICON_FA_FOLDER " File location");
                snprintf( name_path[3], 1024, "%s", ICON_FA_FOLDER_PLUS " Select");
            }
            if (Settings::application.source.capture_path.empty())
                Settings::application.source.capture_path = SystemToolkit::home_path();
            snprintf( name_path[0], 1024, "%s", Settings::application.source.capture_path.c_str());
            int selected_path = 0;
            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            ImGui::Combo("Path", &selected_path, name_path, 4);
            if (selected_path > 2)
                captureFolderDialog->open();
            else if (selected_path > 1) {
                // file location of media player
                if (mediaplayer_active_)
                    Settings::application.source.capture_path = SystemToolkit::path_filename( mediaplayer_active_->filename() );
                // else file location of session
                else
                    Settings::application.source.capture_path = SystemToolkit::path_filename( Mixer::manager().session()->filename() );
            }
            else if (selected_path > 0)
                Settings::application.source.capture_path = SystemToolkit::home_path();

            // offer to open folder location
            ImVec2 draw_pos = ImGui::GetCursorPos();
            ImGui::SetCursorPos(draw_pos + ImVec2(ImGui::GetContentRegionAvailWidth() - 1.2 * ImGui::GetTextLineHeightWithSpacing(), -ImGui::GetFrameHeight()) );
            if (ImGuiToolkit::IconButton( ICON_FA_FOLDER_OPEN,  Settings::application.source.capture_path.c_str()))
                SystemToolkit::open(Settings::application.source.capture_path);
            ImGui::SetCursorPos(draw_pos);

            // Naming menu selection
            static const char* naming_style[2] = { ICON_FA_SORT_NUMERIC_DOWN "  Sequential", ICON_FA_CALENDAR "  Date prefix" };
            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            ImGui::Combo("Filename", &Settings::application.source.capture_naming, naming_style, IM_ARRAYSIZE(naming_style));

            ImGui::EndMenu();
        }

        //
        // Menu for video Mediaplayer control
        //
        if (ImGui::BeginMenu(ICON_FA_FILM " Timeline", mediaplayer_active_) )
        {
            if ( mediaplayer_active_->isImage()) {
                if ( ImGuiToolkit::MenuItemIcon(1, 14, "Remove")){
                    // set empty timeline
                    Timeline tl;
                    mediaplayer_active_->setTimeline(tl);
                    mediaplayer_active_->play(false);
                    // re-open the image with NO timeline
                    mediaplayer_active_->reopen();
                }

                if ( ImGui::MenuItem(ICON_FA_HOURGLASS_HALF "  Duration")){
                    mediaplayer_set_duration_ = true;
                }
            }

            if (ImGui::MenuItem(ICON_FA_WINDOW_CLOSE "  Reset")){
                mediaplayer_timeline_zoom_ = 1.f;
                mediaplayer_active_->timeline()->clearFading();
                mediaplayer_active_->timeline()->clearGaps();
                mediaplayer_active_->setVideoEffect("");
                std::ostringstream oss;
                oss << SystemToolkit::base_filename( mediaplayer_active_->filename() );
                oss << ": Reset timeline";
                Action::manager().store(oss.str());
            }

            if (ImGui::MenuItem(LABEL_EDIT_FADING))
                mediaplayer_edit_fading_ = true;

            if (ImGui::BeginMenu(ICON_FA_CLOCK "  Metronome"))
            {
                Metronome::Synchronicity sync = mediaplayer_active_->syncToMetronome();
                bool active = sync == Metronome::SYNC_NONE;
                if (ImGuiToolkit::MenuItemIcon(5, 13, " Not synchronized", NULL, active ))
                    mediaplayer_active_->setSyncToMetronome(Metronome::SYNC_NONE);
                active = sync == Metronome::SYNC_BEAT;
                if (ImGuiToolkit::MenuItemIcon(6, 13, " Sync to beat", NULL, active ))
                    mediaplayer_active_->setSyncToMetronome(Metronome::SYNC_BEAT);
                active = sync == Metronome::SYNC_PHASE;
                if (ImGuiToolkit::MenuItemIcon(7, 13, " Sync to phase", NULL, active ))
                    mediaplayer_active_->setSyncToMetronome(Metronome::SYNC_PHASE);
                ImGui::EndMenu();
            }

            ImGui::Separator();
            if (ImGuiToolkit::MenuItemIcon(16, 16, "Gstreamer effect", nullptr,
                                           !mediaplayer_active_->videoEffect().empty(),
                                           mediaplayer_active_->videoEffectAvailable()) )
                mediaplayer_edit_pipeline_ = true;

            ImGui::EndMenu();
        }

        // button to activate the magnifying glass at top right corner
        ImVec2 p = g.CurrentWindow->Pos;
        p.x += g.CurrentWindow->Size.x - 2.1f * g.FontSize;
        if (g.CurrentWindow->DC.CursorPos.x < p.x)
        {
            ImGui::SetCursorScreenPos(p);
            if (selection_.size() == 1) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
                ImGuiToolkit::ButtonToggle( ICON_FA_SEARCH, &magnifying_glass);
                ImGui::PopStyleColor();
            }
            else
                ImGui::TextDisabled(" " ICON_FA_SEARCH);
        }

        ImGui::EndMenuBar();
    }

    // disable magnifying glass if window is deactivated
    if ( g.NavWindow != g.CurrentWindow )
        magnifying_glass = false;

    // reset mediaplayer ptr
    mediaplayer_active_ = nullptr;

    // render with appropriate method
    if (active_selection_ > -1)
        RenderSelection(active_selection_);
    else
        RenderSelectedSources();

    ImGui::End();

}

void DrawTimeScale(const char* label, guint64 duration, double width_ratio)
{
    // get window
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return;

    // get style & id
    const ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    const ImVec2 timeline_size( static_cast<float>( static_cast<double>(duration) * width_ratio ), 2.f * g.FontSize);

    const ImVec2 pos = window->DC.CursorPos;
    const ImVec2 frame_size( timeline_size.x + 2.f * style.FramePadding.x, timeline_size.y + style.FramePadding.y);
    const ImRect bbox(pos, pos + frame_size);
    ImGui::ItemSize(frame_size, style.FramePadding.y);
    if (!ImGui::ItemAdd(bbox, id))
        return;

    const ImVec2 timescale_pos = pos + ImVec2(style.FramePadding.x, 0.f);

    ImGuiToolkit::RenderTimeline(timescale_pos, timescale_pos + timeline_size, 0, duration, 1000, true);

}

std::list< std::pair<float, guint64> > DrawTimeline(const char* label, Timeline *timeline, guint64 time,
                                                    double width_ratio, float height)
{
    std::list< std::pair<float, guint64> > ret;

    // get window
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return ret;

    // get style & id
    const ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const float fontsize = g.FontSize;
    const ImGuiID id = window->GetID(label);

    //
    // FIRST PREPARE ALL data structures
    //

    // fixed elements of timeline
    float *lines_array = timeline->fadingArray();
    const guint64 duration = timeline->sectionsDuration();
    TimeIntervalSet se = timeline->sections();
    const ImVec2 timeline_size( static_cast<float>( static_cast<double>(duration) * width_ratio ), 2.f * fontsize);

    // widget bounding box
    const ImVec2 frame_pos = window->DC.CursorPos;
    const ImVec2 frame_size( timeline_size.x + 2.f * style.FramePadding.x, height);
    const ImRect bbox(frame_pos, frame_pos + frame_size);
    ImGui::ItemSize(frame_size, style.FramePadding.y);
    if (!ImGui::ItemAdd(bbox, id))
        return ret;

    // capture hover to avoid tooltip on plotlines
    ImGui::ItemHoverable(bbox, id);

    // cursor size
    const float cursor_width = 0.5f * fontsize;

    // TIMELINE is inside the bbox, at the bottom
    const ImVec2 timeline_pos = frame_pos + ImVec2(style.FramePadding.x, frame_size.y - timeline_size.y -style.FramePadding.y);
    const ImRect timeline_bbox( timeline_pos, timeline_pos + timeline_size);

    // PLOT of opacity is inside the bbox, at the top
    const ImVec2 plot_pos = frame_pos + style.FramePadding;
    const ImRect plot_bbox( plot_pos, plot_pos + ImVec2(timeline_size.x, frame_size.y - 2.f * style.FramePadding.y - timeline_size.y));

    //
    // THIRD RENDER
    //

    // Render the bounding box frame
    ImGui::RenderFrame(bbox.Min, bbox.Max, ImGui::GetColorU32(ImGuiCol_FrameBgActive), true, style.FrameRounding);

    // loop over sections of sources' timelines
    guint64 d = 0;
    guint64 e = 0;
    ImVec2 section_bbox_min = timeline_bbox.Min;
    for (auto section = se.begin(); section != se.end(); ++section) {

        // increment duration to adjust horizontal position
        d += section->duration();
        e = section->end;
        const float percent = static_cast<float>(d) / static_cast<float>(duration) ;
        ImVec2 section_bbox_max = ImLerp(timeline_bbox.GetBL(), timeline_bbox.GetBR(), percent);

        // adjust bbox of section and render a timeline
        ImRect section_bbox(section_bbox_min, section_bbox_max);
        // render the timeline
        ImGuiToolkit::RenderTimeline(section_bbox_min, section_bbox_max, section->begin, section->end, timeline->step());

        // draw the cursor
        float time_ = static_cast<float> ( static_cast<double>(time - section->begin) / static_cast<double>(section->duration()) );
        if ( time_ > -FLT_EPSILON && time_ < 1.f ) {
            ImVec2 pos = ImLerp(section_bbox.GetTL(), section_bbox.GetTR(), time_) - ImVec2(cursor_width, 2.f);
            ImGui::RenderArrow(window->DrawList, pos, ImGui::GetColorU32(ImGuiCol_SliderGrab), ImGuiDir_Up);
        }

        // draw plot of lines
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0,0,0,0));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.f, 0.f));
        ImGui::SetCursorScreenPos(ImVec2(section_bbox_min.x, plot_bbox.Min.y));
        // find the index in timeline array of the section start time
        size_t i = timeline->fadingIndexAt(section->begin);
        // number of values is the index after end time of section (+1), minus the start index
        size_t values_count = 1 + timeline->fadingIndexAt(section->end) - i;
        ImGui::PlotLines("##linessection", lines_array + i, values_count, 0, NULL, 0.f, 1.f, ImVec2(section_bbox.GetWidth(), plot_bbox.GetHeight()));
        ImGui::PopStyleColor(1);
        ImGui::PopStyleVar(1);

        // detect if there was a gap before
        if (i > 0)
            window->DrawList->AddRectFilled(ImVec2(section_bbox_min.x -2.f, plot_bbox.Min.y), ImVec2(section_bbox_min.x + 2.f, plot_bbox.Max.y), ImGui::GetColorU32(ImGuiCol_TitleBg));

        ret.push_back( std::pair<float, guint64>(section_bbox_min.x,section->begin ) );
        ret.push_back( std::pair<float, guint64>(section_bbox_max.x,section->end ) );

        // iterate: next bbox of section starts at end of current
        section_bbox_min.x = section_bbox_max.x;
    }

    // detect if there is a gap after
    if (e < timeline->duration())
        window->DrawList->AddRectFilled(ImVec2(section_bbox_min.x -2.f, plot_bbox.Min.y), ImVec2(section_bbox_min.x + 2.f, plot_bbox.Max.y), ImGui::GetColorU32(ImGuiCol_TitleBg));

    return ret;
}

void SourceControlWindow::RenderSelection(size_t i)
{
    ImVec2 top = ImGui::GetCursorScreenPos();
    ImVec2 rendersize = ImGui::GetContentRegionAvail() - ImVec2(0, buttons_height_ + scrollbar_ + v_space_);
    ImVec2 bottom = ImVec2(top.x, top.y + rendersize.y + v_space_);

    selection_ = Mixer::manager().session()->getBatch(i);
    int numsources = selection_.size();

    // no source selected
    if (numsources < 1)
    {
        ///
        /// Centered text
        ///
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6, 0.6, 0.6, 0.5f));
        ImVec2 center = rendersize * ImVec2(0.5f, 0.5f);
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_ITALIC);
        center.x -= ImGui::GetTextLineHeight() * 2.f;
        ImGui::SetCursorScreenPos(top + center);
        ImGui::Text("Empty selection");
        ImGui::PopFont();
        ImGui::PopStyleColor(1);
    }
    else {
        ///
        /// Sources LIST
        ///
        ///

        // get max duration and max frame width
        GstClockTime maxduration = 0;
        std::list<guint64> durations;
        float maxframewidth = 0.f;
        for (auto source = selection_.begin(); source != selection_.end(); ++source) {
            // collect durations of all media sources
            MediaSource *ms = dynamic_cast<MediaSource *>(*source);
            if (ms != nullptr && !ms->mediaplayer()->singleFrame())
                durations.push_back(static_cast<guint64>(static_cast<double>(ms->mediaplayer()->timeline()->sectionsDuration()) / fabs(ms->mediaplayer()->playSpeed())));
            // compute the displayed width of frames of this source, and keep the max to align afterwards
            float w = 1.5f * timeline_height_ * (*source)->frame()->aspectRatio();
            if ( w > maxframewidth)
                maxframewidth = w;
        }
        if (durations.size()>0) {
            durations.sort();
            durations.unique();
            maxduration = durations.back();
        }

        // compute the ratio for timeline rendering : width (pixel) per time unit (ms)
        const float w = rendersize.x -maxframewidth - 3.f * h_space_ - scrollbar_;
        const double width_ratio = static_cast<double>(w) / static_cast<double>(maxduration);

        // draw list in a scroll area
        ImGui::BeginChild("##v_scroll2", rendersize, false);
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, v_space_));

            // draw play time scale if a duration is set
            if (maxduration > 0) {
                ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2( maxframewidth + h_space_, 0));
                DrawTimeScale("##timescale", maxduration, width_ratio);
            }

            // loop over all sources
            SourceList selection_sources = selection_;
            ///
            /// First pass: loop over media sources only (with timeline)
            ///
            for (auto source = selection_sources.begin(); source != selection_sources.end();  ) {

                // get media source
                MediaSource *ms = dynamic_cast<MediaSource *>(*source);
                if (ms == nullptr || ms->mediaplayer()->singleFrame()) {
                    // leave the source in the list and continue
                    ++source;
                    continue;
                }

                // ok, get the media player of the media source
                MediaPlayer *mp = ms->mediaplayer();

                ///
                /// Source Image Button
                ///
                ImVec2 image_top = ImGui::GetCursorPos();
                const ImVec2 framesize(1.5f * timeline_height_ * (*source)->frame()->aspectRatio(), 1.5f * timeline_height_);
                int action = SourceButton(*source, framesize);
                if (action > 1) {
                    (*source)->play( ! (*source)->playing() );
                    Action::manager().store((*source)->playing() ? "Source Play" : "Source Pause" );
                }
                else if (action > 0)
                    UserInterface::manager().showSourceEditor(*source);

                // icon and text below thumbnail
                ImGuiToolkit::PushFont(ImGuiToolkit::FONT_MONO);
                ImGuiToolkit::Icon( (*source)->icon().x, (*source)->icon().y);
                if ((*source)->playable()) {
                    ImGui::SameLine();
                    ImGui::Text(" %s", GstToolkit::time_to_string((*source)->playtime()).c_str() );
                }
                ImGui::PopFont();

                // start to draw timeline aligned at maximum frame width + horizontal space
                ImVec2 pos = image_top + ImVec2( maxframewidth + h_space_, 0);
                ImGui::SetCursorPos(pos);

                // draw the mediaplayer's timeline, with the indication of cursor position
                // NB: use the same width/time ratio for all to ensure timing vertical correspondance

                // TODO : if (mp->syncToMetronome() > Metronome::SYNC_NONE)
                DrawTimeline("##timeline_mediaplayer", mp->timeline(), mp->position(),
                             width_ratio / fabs(mp->playSpeed()), framesize.y);

                if ( w > maxframewidth ) {

                    // next icon buttons are small
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3.f, 3.f));

                    // next buttons sub id
                    ImGui::PushID( static_cast<int>(mp->id()));

                    // display play speed
                    ImGui::SetCursorPos(pos + ImVec2( 0.f, framesize.y + v_space_));
                    ImGui::Text(UNICODE_MULTIPLY " %.2f", mp->playSpeed());
                    // if not 1x speed, offer to reset
                    if ( fabs( fabs(mp->playSpeed()) - 1.0 ) > EPSILON ) {
                        ImGui::SameLine(0,h_space_);
                        if (ImGuiToolkit::ButtonIcon(19, 15, "Reset speed"))
                            mp->setPlaySpeed( 1.0 );
                    }

                    // if more than one duration of media players, add buttons to adjust
                    if (durations.size() > 1)
                    {
                        for (auto d = durations.crbegin(); d != durations.crend(); ++d) {

                            // next buttons sub id
                            ImGui::PushID( static_cast<int>(*d));

                            // calculate position of icons
                            double x = static_cast<double>(*d) * width_ratio;
                            ImGui::SetCursorPos(pos + ImVec2( static_cast<float>(x) - 2.f, framesize.y + v_space_) );
                            // depending on position relative to media play duration, offer corresponding action
                            double secdur = static_cast<double>(mp->timeline()->sectionsDuration());
                            guint64 playdur = static_cast<guint64>( secdur / fabs(mp->playSpeed()) );
                            // last icon in the timeline
                            if ( playdur == (*d) ) {
                                // not the minimum duration :
                                if (playdur > durations.front() ) {
                                    // offer to speed up or slow down [<>]
                                    if (playdur < durations.back() ) {
                                        if ( ImGuiToolkit::ButtonIcon(0, 12, "Adjust duration") ) {
                                            auto prev = d;
                                            prev--;
                                            selection_target_slower_ = SIGN(mp->playSpeed()) * secdur / static_cast<double>(*prev);
                                            auto next = d;
                                            next++;
                                            selection_target_faster_ = SIGN(mp->playSpeed()) * secdur / static_cast<double>(*next);
                                            selection_mediaplayer_ = mp;
                                            selection_context_menu_ = true;
                                        }
                                    }
                                    // offer to speed up [< ]
                                    else if ( ImGuiToolkit::ButtonIcon(8, 12, "Adjust duration") ) {
                                        auto next = d;
                                        next++;
                                        selection_target_faster_ = SIGN(mp->playSpeed()) * secdur / static_cast<double>(*next);
                                        selection_target_slower_ = 0.0;
                                        selection_mediaplayer_ = mp;
                                        selection_context_menu_ = true;
                                    }
                                }
                                // minimum duration : offer to slow down [ >]
                                else if ( ImGuiToolkit::ButtonIcon(9, 12, "Adjust duration") ) {
                                    selection_target_faster_ = 0.0;
                                    auto prev = d;
                                    prev--;
                                    selection_target_slower_ = SIGN(mp->playSpeed()) * secdur / static_cast<double>(*prev);
                                    selection_mediaplayer_ = mp;
                                    selection_context_menu_ = true;
                                }
                            }
                            // middle buttons : offer to cut at this position
                            else if ( playdur > (*d) ) {
                                char text_buf[256];
                                GstClockTime cutposition = mp->timeline()->sectionsTimeAt( (*d) * fabs(mp->playSpeed()) );
                                ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), "Cut at %s",
                                               GstToolkit::time_to_string(cutposition, GstToolkit::TIME_STRING_MINIMAL).c_str());

                                if ( ImGuiToolkit::ButtonIcon(9, 3, text_buf) ) {
                                    if ( mp->timeline()->cut(cutposition, false, true) ) {
                                        std::ostringstream info;
                                        info << SystemToolkit::base_filename( mp->filename() ) << ": Timeline " <<text_buf;
                                        Action::manager().store(info.str());
                                    }
                                }
                            }

                            ImGui::PopID();
                        }
                    }
                    // special case when all media players are (cut to) the same size
                    else if ( durations.size() > 0) {

                        // calculate position of icon
                        double x = static_cast<double>(durations.front()) * width_ratio;
                        ImGui::SetCursorPos(pos + ImVec2( static_cast<float>(x) - 2.f, framesize.y + v_space_) );

                        // offer only to adjust size by removing ending gap
                        if ( mp->timeline()->gapAt( mp->timeline()->end() ) ) {
                            if ( ImGuiToolkit::ButtonIcon(7, 0, "Remove end gap" )){
                                if ( mp->timeline()->removeGaptAt(mp->timeline()->end()) ) {
                                    std::ostringstream info;
                                    info << SystemToolkit::base_filename( mp->filename() ) << ": Timeline Remove end gap";
                                    Action::manager().store(info.str());
                                }
                            }
                        }
                    }

                    ImGui::PopStyleVar();
                    ImGui::PopID();
                }

                // next line position
                ImGui::SetCursorPos(image_top + ImVec2(0, 2.0f * timeline_height_ + 2.f * v_space_));

                // next source
                source = selection_sources.erase(source);
            }

            ImGui::Spacing();

            ///
            /// Second pass: loop over remaining sources (no timeline)
            ///
            ImGui::Columns( MAX( 1, MIN( int(ceil(w / 250.f)), (int)selection_sources.size()) ), "##selectioncolumns", false);
            for (auto source = selection_sources.begin(); source != selection_sources.end(); ++source) {
                ///
                /// Source Image Button
                ///
                const ImVec2 framesize(1.5f * timeline_height_ * (*source)->frame()->aspectRatio(), 1.5f * timeline_height_);
                int action = SourceButton(*source, framesize);
                if (action > 1) {
                    (*source)->play( ! (*source)->playing() );                    
                    Action::manager().store((*source)->playing() ? "Source Play" : "Source Pause" );
                }
                else if (action > 0)
                    UserInterface::manager().showSourceEditor(*source);

                // icon and text below thumbnail
                ImGuiToolkit::PushFont(ImGuiToolkit::FONT_MONO);
                ImGuiToolkit::Icon( (*source)->icon().x, (*source)->icon().y);
                if ((*source)->playable()) {
                    ImGui::SameLine();
                    ImGui::Text(" %s", GstToolkit::time_to_string((*source)->playtime()).c_str() );
                }
                ImGui::PopFont();

                // next line position
                ImGui::Spacing();
                ImGui::NextColumn();
            }
            ImGui::Columns(1);

            ImGui::PopStyleVar();
        }
        ImGui::EndChild();

    }

    ///
    /// context menu from actions above
    ///
    RenderSelectionContextMenu();

    ///
    /// Play button bar
    ///
    DrawButtonBar(bottom, rendersize.x);

    ///
    /// Selection of sources
    ///
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.14f, 0.14f, 0.14f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.14f, 0.14f, 0.14f, 0.7f));

    float width_combo = ImGui::GetContentRegionAvail().x - buttons_height_;
    if (width_combo > buttons_width_)
    {
        ImGui::SameLine(0, width_combo -buttons_width_ );
        ImGui::SetNextItemWidth(buttons_width_);
        std::string label = std::string(ICON_FA_CHECK_CIRCLE "  ") + std::to_string(numsources) + ( numsources > 1 ? " sources" : " source");
        if (ImGui::BeginCombo("##SelectionImport", label.c_str()))
        {
            // select all playable sources
            for (auto s = Mixer::manager().session()->begin(); s != Mixer::manager().session()->end(); ++s) {
                if ( !(*s)->failed() ) {
                    std::string label = std::string((*s)->initials()) + " - " + (*s)->name();
                    if (std::find(selection_.begin(),selection_.end(),*s) == selection_.end()) {
                        if (ImGui::MenuItem( label.c_str() , 0, false ))
                            Mixer::manager().session()->addSourceToBatch(i, *s);
                    }
                    else {
                        if (ImGui::MenuItem( label.c_str(), 0, true ))
                            Mixer::manager().session()->removeSourceFromBatch(i, *s);
                    }
                }
            }
            ImGui::EndCombo();
        }
    }

    ImGui::SameLine();
    ImGui::SetCursorPosX(rendersize.x - buttons_height_ / 1.3f);
    if (ImGui::Button(ICON_FA_TIMES_CIRCLE)) {
        resetActiveSelection();
        Mixer::manager().session()->deleteBatch(i);
    }
    if (ImGui::IsItemHovered())
        ImGuiToolkit::ToolTip("Delete batch");

    ImGui::PopStyleColor(4);
}

void SourceControlWindow::RenderSelectionContextMenu()
{
    if (selection_mediaplayer_ == nullptr)
        return;

    if (selection_context_menu_) {
        ImGui::OpenPopup("source_controller_selection_context_menu");
        selection_context_menu_ = false;
    }
    if (ImGui::BeginPopup("source_controller_selection_context_menu"))
    {
        std::ostringstream info;
        info << SystemToolkit::base_filename( selection_mediaplayer_->filename() );

        if ( ImGuiToolkit::MenuItemIcon(14, 16,  ICON_FA_CARET_LEFT " Accelerate", NULL, false, fabs(selection_target_faster_) > 0 )){
            selection_mediaplayer_->setPlaySpeed( selection_target_faster_ );
            info << ": Speed x" << std::setprecision(3) << selection_target_faster_;
            Action::manager().store(info.str());
        }
        if ( ImGuiToolkit::MenuItemIcon(15, 16,  "Slow down " ICON_FA_CARET_RIGHT, NULL, false, fabs(selection_target_slower_) > 0 )){
            selection_mediaplayer_->setPlaySpeed( selection_target_slower_ );
            info << ": Speed x" << std::setprecision(3) << selection_target_slower_;
            Action::manager().store(info.str());
        }
        if ( selection_mediaplayer_->timeline()->gapAt( selection_mediaplayer_->timeline()->end()) ) {

            if ( ImGuiToolkit::MenuItemIcon(7, 0, "Restore ending" )){
                info << ": Restore ending";
                if ( selection_mediaplayer_->timeline()->removeGaptAt(selection_mediaplayer_->timeline()->end()) )
                    Action::manager().store(info.str());
            }

        }
        ImGui::EndPopup();
    }

}

void DrawInspector(uint texture, ImVec2 texturesize, ImVec2 cropsize, ImVec2 origin)
{
    if (Settings::application.source.inspector_zoom > 0 && ImGui::IsWindowFocused())
    {
        // region size is computed with zoom factor from settings
        float region_sz = texturesize.x / Settings::application.source.inspector_zoom;

        // get coordinates of area to zoom at mouse position
        const ImGuiIO& io = ImGui::GetIO();
        float region_x = io.MousePos.x - origin.x - region_sz * 0.5f;
        if (region_x < 0.f)
            region_x = 0.f;
        else if (region_x > texturesize.x - region_sz)
            region_x = texturesize.x - region_sz;

        float region_y = io.MousePos.y - origin.y - region_sz * 0.5f;
        if (region_y < 0.f)
            region_y = 0.f;
        else if (region_y > texturesize.y - region_sz)
            region_y = texturesize.y - region_sz;

        // Tooltip without border and 100% opaque
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f) );
        ImGui::BeginTooltip();

        // compute UV and display image in tooltip
        ImVec2 uv0 = ImVec2((region_x) / cropsize.x, (region_y) / cropsize.y);
        ImVec2 uv1 = ImVec2((region_x + region_sz) / cropsize.x, (region_y + region_sz) / cropsize.y);
        ImVec2 uv2 = ImClamp( uv1, ImVec2(0.f, 0.f), ImVec2(1.f, 1.f));
        uv0 += (uv2-uv1);
        ImGui::Image((void*)(intptr_t)texture, ImVec2(texturesize.x / 3.f, texturesize.x / 3.f), uv0, uv2, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), ImVec4(1.0f, 1.0f, 1.0f, 0.5f));

        ImGui::EndTooltip();
        ImGui::PopStyleVar(3);
    }
}

void DrawSource(Source *s, ImVec2 framesize, ImVec2 top_image, bool withslider, bool withinspector)
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // info on source
    CloneSource *cloned = dynamic_cast<CloneSource *>(s);

    // draw pre and post-processed parts if necessary
    if (s->imageProcessingEnabled() || s->textureTransformed() || cloned != nullptr) {

        //
        // LEFT of slider  : original texture
        //
        ImVec2 slider = framesize * ImVec2(Settings::application.widget.media_player_slider,1.f);
        ImGui::Image((void*)(uintptr_t) s->texture(), slider, ImVec2(0.f,0.f), ImVec2(Settings::application.widget.media_player_slider,1.f));
        if ( withinspector && ImGui::IsItemHovered() )
            DrawInspector(s->texture(), framesize, framesize, top_image);

        //
        // RIGHT of slider : post-processed image (after crop and color correction)
        //
        ImVec2 cropsize = framesize * ImVec2 ( s->frame()->projectionArea().x, s->frame()->projectionArea().y);
        ImVec2 croptop  = (framesize - cropsize) * 0.5f;
        // no overlap of slider with cropped area
        if (slider.x < croptop.x) {
            // draw cropped area
            ImGui::SetCursorScreenPos( top_image + croptop );
            ImGui::Image((void*)(uintptr_t) s->frame()->texture(), cropsize, ImVec2(0.f, 0.f), ImVec2(1.f,1.f));
            if ( withinspector && ImGui::IsItemHovered() )
                DrawInspector(s->frame()->texture(), framesize, cropsize, top_image + croptop);
        }
        // overlap of slider with cropped area (horizontally)
        else if (slider.x < croptop.x + cropsize.x ) {
            // compute slider ratio of cropped area
            float cropped_slider = (slider.x - croptop.x) / cropsize.x;
            // top x moves with slider
            ImGui::SetCursorScreenPos( top_image + ImVec2(slider.x, croptop.y) );
            // size is reduced by slider
            ImGui::Image((void*)(uintptr_t) s->frame()->texture(), cropsize * ImVec2(1.f -cropped_slider, 1.f), ImVec2(cropped_slider, 0.f), ImVec2(1.f,1.f));
            if ( withinspector && ImGui::IsItemHovered() )
                DrawInspector(s->frame()->texture(), framesize, cropsize, top_image + croptop);
        }
        // else : no render of cropped area

        ImU32 slider_color = ImGui::GetColorU32(ImGuiCol_NavWindowingHighlight);
        if (withslider)
        {
            //
            // SLIDER
            //
            // user input : move slider horizontally
            ImGui::SetCursorScreenPos(top_image + ImVec2(- 20.f, 0.5f * framesize.y - 20.0f));
            ImGuiToolkit::InvisibleSliderFloat("#media_player_slider2", &Settings::application.widget.media_player_slider, 0.f, 1.f, ImVec2(framesize.x + 40.f, 40.0f) );
            // affordance: cursor change to horizontal arrows
            if (ImGui::IsItemHovered() || ImGui::IsItemFocused()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                slider_color = ImGui::GetColorU32(ImGuiCol_Text);
            }
            // graphical indication of slider
            draw_list->AddCircleFilled(top_image + slider * ImVec2(1.f, 0.5f), 20.f, slider_color, 26);
        }
        // graphical indication of separator (vertical line)
        draw_list->AddLine(top_image + slider * ImVec2(1.f,0.0f), top_image + slider, slider_color, 1);
    }
    // no post-processed to show: draw simple texture
    else {
        ImGui::Image((void*)(uintptr_t) s->texture(), framesize);
        if ( withinspector && ImGui::IsItemHovered() )
            DrawInspector(s->texture(), framesize, framesize, top_image);
    }
}

ImRect DrawSourceWithSlider(Source *s, ImVec2 top, ImVec2 rendersize, bool with_inspector)
{
    ///
    /// Centered frame
    ///
    FrameBuffer *frame = s->frame();
    ImVec2 framesize = rendersize;
    ImVec2 corner(0.f, 0.f);
    ImVec2 tmp = ImVec2(framesize.y * frame->aspectRatio(), framesize.x / frame->aspectRatio());
    if (tmp.x > framesize.x) {
        //vertically centered, modulo height of font for display of info under frame
        corner.y = MAX( (framesize.y - tmp.y) / 2.f - ImGui::GetStyle().IndentSpacing, 0.f);
        framesize.y = tmp.y;
    }
    else {
        // horizontally centered
        //        corner.x = (framesize.x - tmp.x) - MAX( (framesize.x - tmp.x) / 2.f - ImGui::GetStyle().IndentSpacing, 0.f);
        corner.x = (framesize.x - tmp.x) / 2.f;
        framesize.x = tmp.x;
    }

    ///
    /// Image
    ///
    const ImVec2 top_image = top + corner;
    ImGui::SetCursorScreenPos(top_image);

    if (s->ready()) {
        // 100% opacity for the image (ensure true colors)
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.f);
        DrawSource(s, framesize, top_image, true, with_inspector);
        ImGui::PopStyleVar();
    }

    return ImRect( top_image, top_image + framesize);
}


int SourceControlWindow::SourceButton(Source *s, ImVec2 framesize)
{
    // returns > 0 if clicked, >1 if clicked on center play/pause button
    int ret = 0;

    // all subsequent buttons are identified under a unique source id
    ImGui::PushID(s->id());

    // Adjust size of font to frame size
    ImGuiToolkit::PushFont(framesize.x > 350.f ? ImGuiToolkit::FONT_LARGE : ImGuiToolkit::FONT_MONO);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const float H = ImGui::GetTextLineHeight();
    const ImVec2 frame_top = ImGui::GetCursorScreenPos();
    const ImVec2 frame_center = frame_top +  ImVec2((framesize.x  - H) / 2.f, (framesize.y  - H) / 2.f);
    ImU32 frame_color = ImGui::GetColorU32(ImGuiCol_Text);
    ImU32 icon_color = ImGui::GetColorU32(ImGuiCol_NavWindowingHighlight);

    // 1. draw texture of source
    if (s->ready())
        DrawSource(s, framesize, frame_top);

    // 2. interactive centered button Play / Pause
    if (s->active() && s->playable()) {
        //draw_list->AddText(frame_center, ImGui::GetColorU32(ImGuiCol_Text), SourcePlayIcon(s));
        ImGui::SetCursorScreenPos(frame_center - ImVec2(H * 0.2f, H * 0.2f));
        ImGui::InvisibleButton("##sourcebutton_icon", ImVec2(H * 1.2f, H * 1.2f));
        if (ImGui::IsItemHovered() || ImGui::IsItemActive()){
            frame_color = ImGui::GetColorU32(ImGuiCol_NavWindowingHighlight);
            icon_color  = ImGui::GetColorU32(ImGuiCol_Text);
        }
        if (ImGui::IsItemClicked()) {
            ret = 2;
        }
    }

    // back to draw overlay
    ImGui::SetCursorScreenPos(frame_top + ImVec2(1.f, 0.f) );

    // 3. draw initials in up-left corner
    draw_list->AddText(frame_top + ImVec2(H * 0.2f, H * 0.1f), ImGui::GetColorU32(ImGuiCol_Text), s->initials());

    // 4. interactive surface on whole texture with frame overlay on mouse over
    ImGui::InvisibleButton("##sourcebutton", framesize);
    if (ImGui::IsItemHovered() || ImGui::IsItemClicked())
    {
        // border
        draw_list->AddRect(frame_top, frame_top + framesize - ImVec2(1.f, 0.f), frame_color, 0, 0, 3.f);
        // centered icon in front of dark background
        if (s->active() && s->playable()) {
            draw_list->AddRectFilled(frame_center - ImVec2(H * 0.2f, H * 0.2f),
                                     frame_center + ImVec2(H * 1.1f, H * 1.1f), ImGui::GetColorU32(ImGuiCol_TitleBgCollapsed), 6.f);
            draw_list->AddText(frame_center, icon_color, s->playing() ? ICON_FA_PAUSE : ICON_FA_PLAY);
        }
    }
    if (ImGui::IsItemClicked()) {
        ret = 1;
    }

    // pops
    ImGui::PopFont();
    ImGui::PopID();

    return ret;
}

void SourceControlWindow::RenderSelectedSources()
{
    ImVec2 top = ImGui::GetCursorScreenPos();
    ImVec2 rendersize = ImGui::GetContentRegionAvail() - ImVec2(0, buttons_height_ + scrollbar_ + v_space_);
    ImVec2 bottom = ImVec2(top.x, top.y + rendersize.y + v_space_);

    // get new selection or validate previous list if selection was not updated
    if (Mixer::selection().empty())
        selection_ = valid_only(Mixer::manager().validate(selection_));
    else
        selection_ = valid_only(Mixer::selection().getCopy());
    int numsources = selection_.size();

    // no source selected
    if (numsources < 1)
    {
        ///
        /// Centered text
        ///
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6, 0.6, 0.6, 0.5f));
        ImVec2 center = rendersize * ImVec2(0.5f, 0.5f);
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_ITALIC);
        center.x -= ImGui::GetTextLineHeight() * 2.f;
        ImGui::SetCursorScreenPos(top + center);
        ImGui::Text("Nothing to play");
        ImGui::PopFont();
        ImGui::PopStyleColor(1);

        ///
        /// Play button bar (automatically disabled)
        ///
        DrawButtonBar(bottom, rendersize.x);

    }
    // single source selected
    else if (numsources < 2)
    {
        ///
        /// Single Source display
        ///
        RenderSingleSource( selection_.front() );
    }
    // Several sources selected
    else {
        ///
        /// Sources grid
        ///
        ImGui::BeginChild("##v_scroll", rendersize, false);
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 2.f * v_space_));

            // area horizontal pack
            int numcolumns = CLAMP( int(ceil(1.0f * rendersize.x / rendersize.y)), 1, numsources );
            ImGui::Columns( numcolumns, "##selectiongrid", false);
            float widthcolumn = rendersize.x / static_cast<float>(numcolumns);
            widthcolumn -= scrollbar_;

            // loop over sources in grid
            for (auto source = selection_.begin(); source != selection_.end(); ++source) {
                ///
                /// Source Image Button
                ///
                ImVec2 image_top = ImGui::GetCursorPos();
                ImVec2 framesize(widthcolumn, widthcolumn / (*source)->frame()->aspectRatio());
                int action = SourceButton(*source, framesize);
                if (action > 1) {
                    (*source)->play( ! (*source)->playing() );                    
                    Action::manager().store((*source)->playing() ? "Source Play" : "Source Pause" );
                }
                else if (action > 0)
                    UserInterface::manager().showSourceEditor(*source);

                // source icon lower left corner
                ImGuiToolkit::PushFont(framesize.x > 350.f ? ImGuiToolkit::FONT_LARGE : ImGuiToolkit::FONT_MONO);
                float h = ImGui::GetTextLineHeightWithSpacing();
                ImGui::SetCursorPos(image_top + ImVec2( h_space_, framesize.y -v_space_ - h ));
                ImGuiToolkit::Icon( (*source)->icon().x, (*source)->icon().y);
                if ((*source)->playable()) {
                    ImGui::SameLine();
                    ImGui::Text(" %s", GstToolkit::time_to_string((*source)->playtime()).c_str() );
                }
                ImGui::PopFont();

                ImGui::Spacing();
                ImGui::NextColumn();
            }

            ImGui::Columns(1);
            ImGui::PopStyleVar();
        }
        ImGui::EndChild();

        ///
        /// Play button bar
        ///
        DrawButtonBar(bottom, rendersize.x);

        ///
        /// Menu to store Selection from current sources
        ///
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.14f, 0.14f, 0.14f, 0.5f));

        float space = ImGui::GetContentRegionAvail().x;
        float width = buttons_height_;
        std::string label(ICON_FA_PLUS_CIRCLE);
        if (space > buttons_width_) { // enough space to show full button with label text
            label += LABEL_STORE_SELECTION;
            width = buttons_width_ - ImGui::GetTextLineHeightWithSpacing();
        }
        ImGui::SameLine(0, space -width);
        ImGui::SetNextItemWidth(width);
        if (ImGui::Button( label.c_str() )) {
            active_selection_ = Mixer::manager().session()->numBatch();
            active_label_ = std::string("Batch #") + std::to_string(active_selection_);
            Mixer::manager().session()->addBatch( ids(selection_) );
        }
        if (space < buttons_width_ && ImGui::IsItemHovered())
            ImGuiToolkit::ToolTip(LABEL_STORE_SELECTION);

        ImGui::PopStyleColor(2);
    }

}

void SourceControlWindow::RenderSingleSource(Source *s)
{
    static bool show_overlay_info = false;

    if ( s == nullptr)
        return;

    // in case of a MediaSource
    MediaSource *ms = dynamic_cast<MediaSource *>(s);
    if ( ms != nullptr && ms->playable() ) {
        RenderMediaPlayer( ms );
    }
    else
    {
        ///
        /// Draw centered Image
        ///
        ImVec2 top = ImGui::GetCursorScreenPos();
        ImVec2 rendersize = ImGui::GetContentRegionAvail() - ImVec2(0, buttons_height_ + scrollbar_ + v_space_);
        ImVec2 bottom = ImVec2(top.x, top.y + rendersize.y + v_space_);

        ImRect imgarea = DrawSourceWithSlider(s, top, rendersize, magnifying_glass);

        ///
        /// Info overlays
        ///
        if (!show_overlay_info){
            ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.f, 0.f, 0.8f));
            ImGui::SetCursorScreenPos(imgarea.GetTL() + ImVec2(h_space_-1.f, v_space_-1.f));
            ImGui::Text("%s", s->initials());
            ImGui::SetCursorScreenPos(imgarea.GetTL() + ImVec2(h_space_+1.f, v_space_+1.f));
            ImGui::Text("%s", s->initials());
            ImGui::PopStyleColor(1);

            ImGui::SetCursorScreenPos(imgarea.GetTL() + ImVec2(h_space_, v_space_));
            ImGui::Text("%s", s->initials());
            ImGui::PopFont();
        }
        if (!magnifying_glass) {

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.f, 0.f, 0.8f));
            ImGui::SetCursorScreenPos(imgarea.GetTL() + ImVec2(imgarea.GetWidth() - ImGui::GetTextLineHeightWithSpacing(), v_space_));
            ImGui::Text(ICON_FA_CIRCLE);
            ImGui::PopStyleColor(1);

            ImGui::SetCursorScreenPos(imgarea.GetTL() + ImVec2(imgarea.GetWidth() - ImGui::GetTextLineHeightWithSpacing(), v_space_));
            ImGui::Text(ICON_FA_INFO_CIRCLE);
            show_overlay_info = ImGui::IsItemHovered();
            if (show_overlay_info){
                // fill info string
                s->accept(info_);
                // draw overlay frame and text
                float tooltip_height = 3.f * ImGui::GetTextLineHeightWithSpacing();
                ImGui::GetWindowDrawList()->AddRectFilled(imgarea.GetTL(), imgarea.GetTL() + ImVec2(imgarea.GetWidth(), tooltip_height), IMGUI_COLOR_OVERLAY);
                ImGui::SetCursorScreenPos(imgarea.GetTL() + ImVec2(h_space_, v_space_));
                ImGui::Text("%s", info_.str().c_str());
                // special case Streams: print framerate
                StreamSource *sts = dynamic_cast<StreamSource*>(s);
                if (sts && s->playing()) {
                    ImGui::SetCursorScreenPos(imgarea.GetTL() + ImVec2(imgarea.GetWidth() - 1.5f * buttons_height_, 0.5f * tooltip_height));
                    ImGui::Text("%.1f Hz", sts->stream()->updateFrameRate());
                }
            }
            else
                // make sure next ItemHovered refreshes the info_
                info_.reset();
        }

        ///
        /// icon & timing in lower left corner
        ///
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ImGui::SetCursorScreenPos(bottom + ImVec2(h_space_, -ImGui::GetTextLineHeightWithSpacing() - h_space_ ));
        ImGuiToolkit::Icon( s->icon().x, s->icon().y);
        ImGui::SameLine();
        ImGui::Text("%s", s->playable() ? GstToolkit::time_to_string(s->playtime()).c_str() : " " );
        ImGui::PopFont();

        ///
        /// Play button bar
        ///
        DrawButtonBar(bottom, rendersize.x);

        ///
        /// Special possibly : selected a media source that is not playable
        /// then offer to make it playable by adding a timeline
        ///
        if ( ms != nullptr )
        {
            if (ms->mediaplayer()->isImage()) {

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.14f, 0.14f, 0.14f, 0.5f));

                float space = ImGui::GetContentRegionAvail().x;
                float width = buttons_height_ ;
                if (space > buttons_width_)
                    width = buttons_width_ - ImGui::GetTextLineHeightWithSpacing();
                ImGui::SameLine(0, space -width);
                ImGui::SetNextItemWidth(width);
                if (ImGuiToolkit::ButtonIcon( 0, 14, LABEL_ADD_TIMELINE, space > buttons_width_ )) {

                    // activate mediaplayer
                    mediaplayer_active_ = ms->mediaplayer();

                    // set timeline to default 1 second
                    Timeline tl;
                    TimeInterval interval(0, GST_SECOND);
                    // set fixed framerate to 25 FPS
                    tl.setTiming( interval, 40 * GST_MSECOND);
                    mediaplayer_active_->setTimeline(tl);

                    // set to play
                    mediaplayer_active_->play(true);

                    // re-open the image with a timeline
                    mediaplayer_active_->reopen();

                    // open dialog to set duration
                    mediaplayer_set_duration_ = true;
                }

                ImGui::PopStyleColor(2);
            }
        }
        ///
        ///  Not a media source, but playable source
        ///  Offer context menu if it is a Stream source
        ///
        else if ( s->active() && s->playable() ) {
            StreamSource *ss = dynamic_cast<StreamSource *>(s);
            if ( ss != nullptr ) {

                static uint counter_menu_timeout = 0;

                ImGui::SameLine();
                ImGui::SetCursorPosX(rendersize.x - buttons_height_ / 1.4f);
                if (ImGuiToolkit::IconButton(5, 8) || ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup)) {
                    counter_menu_timeout=0;
                    ImGui::OpenPopup( "MenuStreamOptions" );
                }

                if (ImGui::BeginPopup( "MenuStreamOptions" ))
                {
                    if (ImGui::MenuItem(ICON_FA_REDO_ALT "  Reload"))
                        ss->reload();
                    // NB: ss is playable (tested above), and thus ss->stream() is not null
                    bool option = ss->stream()->rewindOnDisabled();
                    if (ImGui::MenuItem(ICON_FA_SNOWFLAKE "  Restart on deactivation", NULL, &option ))
                        ss->stream()->setRewindOnDisabled(option);

                    if (ImGui::IsWindowHovered())
                        counter_menu_timeout=0;
                    else if (++counter_menu_timeout > 10)
                        ImGui::CloseCurrentPopup();

                    ImGui::EndPopup();
                }
            }
        }
    }
}

void SourceControlWindow::RenderMediaPlayer(MediaSource *ms)
{
    static bool show_overlay_info = false;

    mediaplayer_active_ = ms->mediaplayer();

    // for action manager
    std::ostringstream oss;
    oss << SystemToolkit::base_filename( mediaplayer_active_->filename() );

    // for draw
    const float slider_zoom_width = timeline_height_ / 2.f;
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ///
    /// Draw centered Image
    ///
    const ImVec2 top = ImGui::GetCursorScreenPos();
    const ImVec2 rendersize = ImGui::GetContentRegionAvail() - ImVec2(0, mediaplayer_height_);
    ImVec2 bottom = ImVec2(top.x, top.y + rendersize.y + v_space_);

    ImRect imgarea = DrawSourceWithSlider(ms, top, rendersize, magnifying_glass);

    ///
    /// Info overlays
    ///
    if (!show_overlay_info){
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.f, 0.f, 0.8f));
        ImGui::SetCursorScreenPos(imgarea.GetTL() + ImVec2(h_space_-1.f, v_space_-1.f));
        ImGui::Text("%s", ms->initials());
        ImGui::SetCursorScreenPos(imgarea.GetTL() + ImVec2(h_space_+1.f, v_space_+1.f));
        ImGui::Text("%s", ms->initials());
        ImGui::PopStyleColor(1);

        ImGui::SetCursorScreenPos(imgarea.GetTL() + ImVec2(h_space_, v_space_));
        if ( mediaplayer_active_->audioEnabled())
            // Icon to inform audio decoding
            ImGui::Text("%s " ICON_FA_VOLUME_UP, ms->initials());
        else
            ImGui::Text("%s", ms->initials());
        ImGui::PopFont();
    }
    if (!magnifying_glass) {

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.f, 0.f, 0.8f));
        ImGui::SetCursorScreenPos(imgarea.GetTL() + ImVec2(imgarea.GetWidth() - ImGui::GetTextLineHeightWithSpacing(), v_space_));
        ImGui::Text(ICON_FA_CIRCLE);
        ImGui::PopStyleColor(1);

        ImGui::SetCursorScreenPos(imgarea.GetTL() + ImVec2(imgarea.GetWidth() - ImGui::GetTextLineHeightWithSpacing(), v_space_));
        ImGui::Text(ICON_FA_INFO_CIRCLE);
        show_overlay_info = ImGui::IsItemHovered();
        if (show_overlay_info){
            // information visitor
            mediaplayer_active_->accept(info_);
            float tooltip_height = 3.f * ImGui::GetTextLineHeightWithSpacing();
            draw_list->AddRectFilled(imgarea.GetTL(), imgarea.GetTL() + ImVec2(imgarea.GetWidth(), tooltip_height), IMGUI_COLOR_OVERLAY);
            ImGui::SetCursorScreenPos(imgarea.GetTL() + ImVec2(h_space_, v_space_));
            ImGui::Text("%s", info_.str().c_str());

            // Icon to inform audio decoding
            if ( mediaplayer_active_->audioEnabled()) {
                ImGui::SetCursorScreenPos(imgarea.GetTL() + ImVec2( imgarea.GetWidth() - 2.f * ImGui::GetTextLineHeightWithSpacing(), 0.35f * tooltip_height));
                ImGui::Text(ICON_FA_VOLUME_UP);
            }

            // Icon to inform hardware decoding
            if ( mediaplayer_active_->decoderName().compare("software") != 0) {
                ImGui::SetCursorScreenPos(imgarea.GetTL() + ImVec2( imgarea.GetWidth() - ImGui::GetTextLineHeightWithSpacing(), 0.35f * tooltip_height));
                ImGui::Text(ICON_FA_MICROCHIP);
            }

            // refresh frequency
            if ( mediaplayer_active_->isPlaying()) {
                ImGui::SetCursorScreenPos(imgarea.GetTL() + ImVec2( imgarea.GetWidth() - 1.5f * buttons_height_, 0.667f * tooltip_height));
                ImGui::Text("%.1f Hz", mediaplayer_active_->updateFrameRate());
            }
        }
    }

    ///
    /// icon & timing in lower left corner
    ///
    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
    ImVec2 S (h_space_, -ImGui::GetTextLineHeightWithSpacing() - h_space_ );
    ImGui::SetCursorScreenPos(bottom + S);
    ImGuiToolkit::Icon( ms->icon().x, ms->icon().y);
    ImGui::SameLine();
    ImGui::Text( "%s", GstToolkit::time_to_string(mediaplayer_active_->position()).c_str() );

    ///
    /// Sync info lower right corner
    ///
    Metronome::Synchronicity sync = mediaplayer_active_->syncToMetronome();
    if ( sync > Metronome::SYNC_NONE) {
        static bool show = true;
        if (mediaplayer_active_->pending())
            show = !show;
        else
            show = true;
        if (show) {
            S.x = rendersize.x + S.y;
            ImGui::SetCursorScreenPos(bottom + S);
            ImGuiToolkit::Icon( sync > Metronome::SYNC_BEAT ? 7 : 6, 13);
        }
    }

    ImGui::PopFont();

    ///
    /// media player timelines
    ///
    double current_play_speed = mediaplayer_active_->playSpeed();
    static uint counter_menu_timeout = 0;
    const ImVec2 scrollwindow = ImVec2(ImGui::GetContentRegionAvail().x - slider_zoom_width - 3.0,
                                 2.f * timeline_height_ + scrollbar_ );

    if ( mediaplayer_active_->isEnabled() ) {

        // ignore actual play status of mediaplayer when slider is pressed
        if (!mediaplayer_slider_pressed_)
            mediaplayer_mode_ = mediaplayer_active_->isPlaying();

        // seek position
        guint64 seek_t = mediaplayer_active_->position();

        // scrolling sub-window
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1.f, 1.f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 1.f);
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.f, 0.f, 0.f, 0.0f));

        ImGui::SetCursorScreenPos(bottom + ImVec2(1.f, 0.f));
        ImGui::BeginChild("##scrolling", scrollwindow,  false, ImGuiWindowFlags_HorizontalScrollbar);
        {
            ImVec2 size = ImGui::CalcItemSize(ImVec2(-FLT_MIN, 0.0f), ImGui::CalcItemWidth(), timeline_height_ -1);
            size.x *= mediaplayer_timeline_zoom_;

            Timeline *tl = mediaplayer_active_->timeline();
            if (tl->is_valid())
            {
                bool released = false;
                if ( ImGuiToolkit::EditPlotHistoLines("##TimelineArray", tl->gapsArray(), tl->fadingArray(),
                                                      MAX_TIMELINE_ARRAY, 0.f, 1.f, tl->begin(), tl->end(),
                                                      Settings::application.widget.media_player_timeline_editmode, &released, size) ) {
                    tl->update();
                }
                else if (released)
                {
                    tl->refresh();
                    if (Settings::application.widget.media_player_timeline_editmode)
                        oss << ": Timeline cut";
                    else
                        oss << ": Timeline opacity";
                    Action::manager().store(oss.str());
                }

                // custom timeline slider
                // TODO  : if (mediaplayer_active_->syncToMetronome() > Metronome::SYNC_NONE)
                mediaplayer_slider_pressed_ = ImGuiToolkit::TimelineSlider("##timeline", &seek_t, tl->begin(),
                                                                               tl->first(), tl->end(), tl->step(), size.x);

            }
        }
        ImGui::EndChild();

        // action mode
        bottom += ImVec2(scrollwindow.x + 2.f, 0.f);
        draw_list->AddRectFilled(bottom, bottom + ImVec2(slider_zoom_width, timeline_height_ -1.f), ImGui::GetColorU32(ImGuiCol_FrameBg));
        ImGui::SetCursorScreenPos(bottom + ImVec2(1.f, 0.f));
        const char *tooltip[2] = {"Draw opacity tool", "Cut tool"};
        ImGuiToolkit::IconToggle(7,4,8,3, &Settings::application.widget.media_player_timeline_editmode, tooltip);

        ImGui::SetCursorScreenPos(bottom + ImVec2(1.f, 0.5f * timeline_height_));
        if (Settings::application.widget.media_player_timeline_editmode) {
            // action cut
            if (mediaplayer_active_->isPlaying()) {
                ImGuiToolkit::Indication("Pause video to enable cut options", 9, 3);
            }
            else if (ImGuiToolkit::IconButton(9, 3, "Cut at cursor")) {
                ImGui::OpenPopup("timeline_cut_context_menu");
            }
            if (ImGui::BeginPopup("timeline_cut_context_menu")){
                if (ImGuiToolkit::MenuItemIcon(1,0,"Cut left")){
                    if (mediaplayer_active_->timeline()->cut(mediaplayer_active_->position(), true, false)) {
                        oss << ": Timeline cut";
                        Action::manager().store(oss.str());
                    }
                }
                if (ImGuiToolkit::MenuItemIcon(2,0,"Cut right")){
                    if (mediaplayer_active_->timeline()->cut(mediaplayer_active_->position(), false, false)){
                        oss << ": Timeline cut";
                        Action::manager().store(oss.str());
                    }
                }
                ImGui::EndPopup();
            }
        }
        else {
            static int _actionsmooth = 0;

            // action smooth
            ImGui::PushButtonRepeat(true);
            if (ImGuiToolkit::IconButton(13, 12, "Smooth")){
                mediaplayer_active_->timeline()->smoothFading( 5 );
                ++_actionsmooth;
            }
            ImGui::PopButtonRepeat();

            if (_actionsmooth > 0 && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                oss << ": Timeline opacity smooth";
                Action::manager().store(oss.str());
                _actionsmooth = 0;
            }
        }

        // zoom slider
        ImGui::SetCursorScreenPos(bottom + ImVec2(0.f, timeline_height_));
        ImGui::VSliderFloat("##TimelineZoom", ImVec2(slider_zoom_width, timeline_height_), &mediaplayer_timeline_zoom_, 1.0, 5.f, "");

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(1);

        ///
        /// media player buttons bar (custom)
        ///

        bottom.x = top.x;
        bottom.y += 2.f * timeline_height_ + scrollbar_;

        draw_list->AddRectFilled(bottom, bottom + ImVec2(rendersize.x, buttons_height_), ImGui::GetColorU32(ImGuiCol_FrameBg), h_space_);

        // buttons style
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.14f, 0.14f, 0.14f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.24f, 0.24f, 0.24f, 0.2f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.14f, 0.14f, 0.14f, 0.5f));

        ImGui::SetCursorScreenPos(bottom + ImVec2(h_space_, v_space_) );
        if (ImGui::Button(mediaplayer_active_->playSpeed() > 0 ? ICON_FA_FAST_BACKWARD :ICON_FA_FAST_FORWARD))
            mediaplayer_active_->rewind();

        // display buttons Play/Stop depending on current playing mode
        ImGui::SameLine(0, h_space_);
        if (mediaplayer_mode_) {
            if (ImGui::Button(ICON_FA_PAUSE)){
                mediaplayer_mode_ = false;
                oss << ": Pause";
                Action::manager().store(oss.str());
            }
            ImGui::SameLine(0, h_space_);

            ImGui::PushButtonRepeat(true);
            if (ImGui::Button( mediaplayer_active_->playSpeed() < 0 ? ICON_FA_BACKWARD :ICON_FA_FORWARD))
                mediaplayer_active_->jump ();
            ImGui::PopButtonRepeat();
        }
        else {
            if (ImGui::Button(ICON_FA_PLAY)) {
                mediaplayer_mode_ = true;
                oss << ": Play";
                Action::manager().store(oss.str());
            }
            ImGui::SameLine(0, h_space_);

            ImGui::PushButtonRepeat(true);
            if (ImGui::Button( mediaplayer_active_->playSpeed() < 0 ? ICON_FA_STEP_BACKWARD : ICON_FA_STEP_FORWARD))
                mediaplayer_active_->step();
            ImGui::PopButtonRepeat();
        }

        // loop modes button
        ImGui::SameLine(0, h_space_);
        static int current_loop = 0;
        static std::vector< std::pair<int, int> > icons_loop = { {0,15}, {1,15}, {19,14} };
        static std::vector< std::string > tooltips_loop = { "Stop at end", "Loop to start", "Bounce (reverse speed)" };
        current_loop = (int) mediaplayer_active_->loop();
        if ( ImGuiToolkit::IconMultistate(icons_loop, &current_loop, tooltips_loop) )
            mediaplayer_active_->setLoop( (MediaPlayer::LoopMode) current_loop );

        // speed slider (if enough space)
        if ( rendersize.x > min_width_ * 1.2f ) {
            ImGui::SameLine(0, MAX(h_space_ * 2.f, rendersize.x - min_width_ * 1.4f) );
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - buttons_height_ );
            // speed slider
            float s = fabs(static_cast<float>(current_play_speed));
            if (ImGui::DragFloat( "##Speed", &s, 0.01f, 0.1f, 10.f, UNICODE_MULTIPLY " %.2f"))
                mediaplayer_active_->setPlaySpeed( SIGN(current_play_speed) * static_cast<double>(s) );
            // store action on mouse release
            if (ImGui::IsItemDeactivatedAfterEdit()){
                oss << ": Speed x" << std::setprecision(3) << s;
                Action::manager().store(oss.str());
            }
            if (ImGui::IsItemHovered())
                ImGuiToolkit::ToolTip("Play speed");
        }

        ImGui::SameLine();
        ImGui::SetCursorPosX(rendersize.x - buttons_height_ / 1.4f);
        if (ImGuiToolkit::IconButton(5, 8) || ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup)) {
            counter_menu_timeout=0;
            ImGui::OpenPopup( "MenuMediaPlayerOptions" );
        }

        // restore buttons style
        ImGui::PopStyleColor(5);

        if (mediaplayer_active_->pending())
        {
            draw_list->AddRectFilled(bottom, bottom + ImVec2(rendersize.x, buttons_height_), ImGui::GetColorU32(ImGuiCol_ScrollbarBg), h_space_);
        }

        ///
        /// media player timeline actions
        ///

        // request seek (ASYNC)
        if ( mediaplayer_slider_pressed_ && mediaplayer_active_->go_to(seek_t) )
            mediaplayer_slider_pressed_ = false;

        // play/stop command should be following the playing mode (buttons)
        // AND force to stop when the slider is pressed
        bool media_play = mediaplayer_mode_ & (!mediaplayer_slider_pressed_);

        // apply play action to media only if status should change
        if ( mediaplayer_active_->isPlaying() != media_play ) {
            mediaplayer_active_->play( media_play );
        }

    }
    else {

        ImGui::SetCursorScreenPos(bottom + ImVec2(1.f, 0.f));

        const ImGuiContext& g = *GImGui;
        const double width_ratio = static_cast<double>(scrollwindow.x - slider_zoom_width + g.Style.FramePadding.x ) / static_cast<double>(mediaplayer_active_->timeline()->sectionsDuration());
        DrawTimeline("##timeline_mediaplayers", mediaplayer_active_->timeline(), mediaplayer_active_->position(), width_ratio, 2.f * timeline_height_);

        ///
        /// Play button bar
        ///
        bottom.y += 2.f * timeline_height_ + scrollbar_;
        DrawButtonBar(bottom, rendersize.x);
    }

    if (ImGui::BeginPopup( "MenuMediaPlayerOptions" ))
    {
        if (ImGuiToolkit::MenuItemIcon(8,0, "Play forward", nullptr, current_play_speed>0)) {
            mediaplayer_active_->setPlaySpeed( ABS(mediaplayer_active_->playSpeed()) );
            oss << ": Play forward";
            Action::manager().store(oss.str());
        }
        if (ImGuiToolkit::MenuItemIcon(9,0, "Play backward", nullptr, current_play_speed<0)) {
            mediaplayer_active_->setPlaySpeed( - ABS(mediaplayer_active_->playSpeed()) );
            oss << ": Play backward";
            Action::manager().store(oss.str());
        }
        if (ImGuiToolkit::MenuItemIcon(19,15, "Reset speed")) {
            mediaplayer_active_->setPlaySpeed(1.0);
            oss << ": Speed x 1.0";
            Action::manager().store(oss.str());
        }
        ImGui::Separator();

        if (ImGui::MenuItem( ICON_FA_REDO_ALT "  Reload" ))
            mediaplayer_active_->reopen();

        bool option = mediaplayer_active_->rewindOnDisabled();
        if (ImGui::MenuItem(ICON_FA_SNOWFLAKE "  Restart on deactivation", NULL, &option )) {
            mediaplayer_active_->setRewindOnDisabled(option);
        }

        if (ImGui::IsWindowHovered())
            counter_menu_timeout=0;
        else if (++counter_menu_timeout > 10)
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    ///
    /// Dialog to edit timeline fade in and out
    ///
    if (mediaplayer_edit_fading_) {
        ImGui::OpenPopup(LABEL_EDIT_FADING);
        mediaplayer_edit_fading_ = false;
    }
    const ImVec2 mp_dialog_size(buttons_width_ * 2.f, buttons_height_ * 6);
    ImGui::SetNextWindowSize(mp_dialog_size, ImGuiCond_Always);
    const ImVec2 mp_dialog_pos = top + rendersize * 0.5f  - mp_dialog_size * 0.5f;
    ImGui::SetNextWindowPos(mp_dialog_pos, ImGuiCond_Always);
    if (ImGui::BeginPopupModal(LABEL_EDIT_FADING, NULL, ImGuiWindowFlags_NoResize))
    {
        const ImVec2 pos = ImGui::GetCursorPos();
        const ImVec2 area = ImGui::GetContentRegionAvail();

        ImGui::Spacing();
        ImGui::Text("Set parameters and apply:");
        ImGui::Spacing();

        static int l = 0;
        static std::vector< std::tuple<int, int, std::string> > fading_options = {
            {19, 7, "Fade in"},
            {18, 7, "Fade out"},
            { 0, 8, "Auto fade in & out"}
        };
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ImGuiToolkit::ComboIcon("Fading", &l, fading_options);

        static int c = 0;
        static std::vector< std::tuple<int, int, std::string> > curve_options = {
            {18, 3, "Linear"},
            {19, 3, "Progressive"},
            {17, 3, "Abrupt"}
        };
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ImGuiToolkit::ComboIcon("Curve", &c, curve_options);

        static uint d = 1000;
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ImGuiToolkit::SliderTiming ("Duration", &d, 200, 5050, 50, "Maximum");
        if (d > 5000)
            d = UINT_MAX;

        bool close = false;
        ImGui::SetCursorPos(pos + ImVec2(0.f, area.y - buttons_height_));
        if (ImGui::Button(ICON_FA_TIMES "  Cancel", ImVec2(area.x * 0.3f, 0)))
            close = true;
        ImGui::SetCursorPos(pos + ImVec2(area.x * 0.7f, area.y - buttons_height_));
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Tab));
        if (ImGui::Button(ICON_FA_CHECK "  Apply", ImVec2(area.x * 0.3f, 0))
                || ImGui::IsKeyPressedMap(ImGuiKey_Enter)  || ImGui::IsKeyPressedMap(ImGuiKey_KeyPadEnter) ) {
            close = true;
            // timeline to edit
            Timeline *tl = mediaplayer_active_->timeline();
            switch (l) {
            case 0:
                tl->fadeIn(d, (Timeline::FadingCurve) c);
                oss << ": Timeline Fade in " << d;
                break;
            case 1:
                tl->fadeOut(d, (Timeline::FadingCurve) c);
                oss << ": Timeline Fade out " << d;
                break;
            case 2:
                tl->autoFading(d, (Timeline::FadingCurve) c);
                oss << ": Timeline Fade in&out " << d;
                break;
            default:
                break;
            }
            tl->smoothFading( 2 );
            Action::manager().store(oss.str());
        }
        ImGui::PopStyleColor(1);

        if (close)
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ///
    /// Dialog to set gstreamer video effect
    ///
    static std::string _effect_description = "";
    static bool _effect_description_changed = false;
    if (mediaplayer_edit_pipeline_) {
        // open dialog
        ImGui::OpenPopup(DIALOG_GST_EFFECT);
        mediaplayer_edit_pipeline_ = false;
        // initialize dialog
        _effect_description = mediaplayer_active_->videoEffect();
        _effect_description_changed = true;
    }
    const ImVec2 mpp_dialog_size(buttons_width_ * 3.f, buttons_height_ * 6.2f);
    ImGui::SetNextWindowSize(mpp_dialog_size, ImGuiCond_Always);
    const ImVec2 mpp_dialog_pos = top + rendersize * 0.5f  - mpp_dialog_size * 0.5f;
    ImGui::SetNextWindowPos(mpp_dialog_pos, ImGuiCond_Always);
    if (ImGui::BeginPopupModal(DIALOG_GST_EFFECT, NULL, ImGuiWindowFlags_NoResize))
    {
        const ImVec2 pos = ImGui::GetCursorPos();
        const ImVec2 area = ImGui::GetContentRegionAvail();
        static uint _status = 0;  // 0:unknown, 1:ok, 2:error
        static std::string _status_message = "";
        static std::vector< std::pair< std::string, std::string> > _examples = { {"Primary color", "frei0r-filter-primaries" },
                                                                                 {"Histogram", "frei0r-filter-rgb-parade mix=0.5"},
                                                                                 {"Emboss", "frei0r-filter-emboss"},
                                                                                 {"Denoise", "frei0r-filter-hqdn3d spatial=0.05 temporal=0.1"},
                                                                                 {"Thermal", "coloreffects preset=heat"},
                                                                                 {"Afterimage", "streaktv"}
                                                                               };
        static ImVec2 fieldsize(ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN, 70);
        static int numlines = 0;
        const ImGuiContext& g = *GImGui;
        fieldsize.y = MAX(2.5, numlines) * g.FontSize + g.Style.ItemSpacing.y + g.Style.FramePadding.y;

        ImGui::Spacing();
        ImGui::Text("Enter a gstreamer video effect description and apply.\nLeave empty for no effect.");
        ImGui::SameLine();
        ImGuiToolkit::HelpToolTip("Video effects are directly integrated in the gstreamer pipeline "
                                  "and performed on CPU (might be slow). Vimix recommends using "
                                  "GPU accelerated filters by cloning the source.");
        ImGui::Spacing();

        // Editor
        if ( ImGuiToolkit::InputCodeMultiline("Effect", &_effect_description, fieldsize, &numlines) )
            _effect_description_changed = true;
        if ( ImGui::IsItemActive() )
            _status = 0;

        // Local menu for list of examples
        ImVec2 pos_bot = ImGui::GetCursorPos();
        ImGui::SetCursorPos( pos_bot + ImVec2(fieldsize.x + IMGUI_SAME_LINE, -ImGui::GetFrameHeightWithSpacing()));
        if (ImGui::BeginCombo("##ExamplesVideoEffect", "ExamplesVideoEffect", ImGuiComboFlags_NoPreview | ImGuiComboFlags_HeightLarge))  {
            ImGui::TextDisabled("Examples");
            for (auto it = _examples.begin(); it != _examples.end(); ++it) {
                if (ImGui::Selectable( it->first.c_str() ) ) {
                    _effect_description = it->second;
                    _effect_description_changed = true;
                }
            }
            ImGui::Separator();
            ImGui::TextDisabled("Explore online");
            if (ImGui::Selectable( ICON_FA_EXTERNAL_LINK_ALT " Frei0r" ) )
                SystemToolkit::open("https://gstreamer.freedesktop.org/documentation/frei0r");
            if (ImGui::Selectable( ICON_FA_EXTERNAL_LINK_ALT " Effectv" ) )
                SystemToolkit::open("https://gstreamer.freedesktop.org/documentation/effectv");
            if (ImGui::Selectable( ICON_FA_EXTERNAL_LINK_ALT " Gaudi" ) )
                SystemToolkit::open("https://gstreamer.freedesktop.org/documentation/gaudieffects");
            if (ImGui::Selectable( ICON_FA_EXTERNAL_LINK_ALT " Geometric" ) )
                SystemToolkit::open("https://gstreamer.freedesktop.org/documentation/geometrictransform");
            ImGui::EndCombo();
        }
        // Local menu for clearing
        ImGui::SameLine();
        if ( ImGuiToolkit::ButtonIcon(11,13,"Clear") ) {
            _effect_description = "";
            _effect_description_changed = true;
        }

        // if desciption changed, start a timeout to test the pipeline
        if ( _effect_description_changed ){
            // reset to status 'unknown' and no message
            _status = 0;
            _status_message.clear();

            // No description, no effect
            if ( _effect_description.empty() ) {
                _status = 1;
                _status_message = "(no video effect)";
            }
            // has a description, we test it
            else {
                GError *error = NULL;
                GstElement *effect = gst_parse_launch( _effect_description.c_str(), &error);
                // on error
                if (effect == NULL || error != NULL) {
                    _status = 2;
                    if (error != NULL)
                        _status_message = error->message;
                    g_clear_error (&error);
                    if (effect)
                        gst_object_unref(effect);
                }
                // on success
                else
                    _status = 1;
            }
            // done
            _effect_description_changed = false;
        }

        // display message line
        if (_status > 1) {
            // On Error
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0, 0.2, 0.2, 0.95f));
            ImGui::TextWrapped("Error - %s", _status_message.c_str());
            ImGui::PopStyleColor(1);
        }
        else if (_status > 0) {
            // All ok
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2, 1.0, 0.2, 0.85f));
            ImGui::Text("Ok %s", _status_message.c_str());
            ImGui::PopStyleColor(1);
        }

        bool close = false;
        ImGui::SetCursorPos(pos + ImVec2(0.f, area.y - buttons_height_));
        if (ImGui::Button(ICON_FA_TIMES "  Cancel", ImVec2(area.x * 0.3f, 0)))
            close = true;
        ImGui::SetCursorPos(pos + ImVec2(area.x * 0.7f, area.y - buttons_height_));
        // on success status, offer to apply
        if (_status == 1) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Tab));
            if (ImGui::Button(ICON_FA_CHECK "  Apply", ImVec2(area.x * 0.3f, 0))
                    || ImGui::IsKeyPressedMap(ImGuiKey_Enter)  || ImGui::IsKeyPressedMap(ImGuiKey_KeyPadEnter) ) {
                close = true;

                // apply to pipeline
                mediaplayer_active_->setVideoEffect(_effect_description);
                oss << " gst effect";
                Action::manager().store(oss.str());
            }
            ImGui::PopStyleColor(1);
        }
        else
            ImGuiToolkit::ButtonDisabled(ICON_FA_CHECK "  Apply", ImVec2(area.x * 0.3f, 0));

        if (close)
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ////
    ///  Dialog to set timeline duration
    ///
    static double timeline_duration_ = 0.0;
    if (mediaplayer_set_duration_) {
        mediaplayer_set_duration_ = false;
        // open dialog
        if (mediaplayer_active_) {
            // get current duration of mediaplayer
            GstClockTime end = mediaplayer_active_->timeline()->end();
            timeline_duration_ = (double) ( GST_TIME_AS_MSECONDS(end) ) / 1000.f;
            // open dialog to change duration
            ImGui::OpenPopup(DIALOG_TIMELINE_DURATION);
        }
    }
    const ImVec2 tld_dialog_size(buttons_width_ * 2.f, buttons_height_ * 4);
    ImGui::SetNextWindowSize(tld_dialog_size, ImGuiCond_Always);
    const ImVec2 tld_dialog_pos = top + rendersize * 0.5f  - tld_dialog_size * 0.5f;
    ImGui::SetNextWindowPos(tld_dialog_pos, ImGuiCond_Always);
    if (ImGui::BeginPopupModal(DIALOG_TIMELINE_DURATION, NULL, ImGuiWindowFlags_NoResize))
    {
        const ImVec2 pos = ImGui::GetCursorPos();
        const ImVec2 area = ImGui::GetContentRegionAvail();

        ImGui::Spacing();
        ImGui::Text("Set the duration of the timeline");
        ImGui::Spacing();

        // get current timeline
        Timeline tl = *mediaplayer_active_->timeline();
        ImGui::InputDouble("second", &timeline_duration_, 1.0f, 10.0f, "%.2f");
        timeline_duration_ = ABS(timeline_duration_);

        bool close = false;
        ImGui::SetCursorPos(pos + ImVec2(0.f, area.y - buttons_height_));
        if (ImGui::Button(ICON_FA_TIMES "  Cancel", ImVec2(area.x * 0.3f, 0)))
            close = true;
        ImGui::SetCursorPos(pos + ImVec2(area.x * 0.7f, area.y - buttons_height_));
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Tab));
        if (ImGui::Button(ICON_FA_CHECK "  Apply", ImVec2(area.x * 0.3f, 0))
                || ImGui::IsKeyPressedMap(ImGuiKey_Enter)  || ImGui::IsKeyPressedMap(ImGuiKey_KeyPadEnter) ) {
            // change timeline end
            mediaplayer_active_->timeline()->setEnd( GST_MSECOND * (GstClockTime) ( timeline_duration_ * 1000.f ) );
            // close dialog
            close = true;
        }
        ImGui::PopStyleColor(1);
        if (close)
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void SourceControlWindow::DrawButtonBar(ImVec2 bottom, float width)
{
    // draw box
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(bottom, bottom + ImVec2(width, buttons_height_), ImGui::GetColorU32(ImGuiCol_FrameBg), h_space_);

    // prepare position to draw text
    ImGui::SetCursorScreenPos(bottom + ImVec2(h_space_, v_space_) );

    // play bar is enabled if only one source selected is enabled
    bool enabled = false;
    size_t n_play = 0;
    for (auto source = selection_.begin(); source != selection_.end(); ++source){
        if ( (*source)->active() && (*source)->playable())
            enabled = true;
        if ( (*source)->playing() )
            n_play++;
    }

    // buttons style for disabled / enabled bar
    if (enabled) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.14f, 0.14f, 0.14f, 0.5f));
    }
    else {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6, 0.6, 0.6, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));
    }

    // Always the rewind button
    if (ImGui::Button(ICON_FA_FAST_BACKWARD) && enabled) {
        for (auto source = selection_.begin(); source != selection_.end(); ++source)
            (*source)->replay();
    }
    ImGui::SameLine(0, h_space_);

    // unique play / pause button
    if (n_play < 1 || selection_.size() == n_play) {
        if (n_play) {
            if (ImGui::Button(ICON_FA_PAUSE) && enabled) {
                for (auto source = selection_.begin(); source != selection_.end(); ++source)
                    (*source)->play(false);
                Action::manager().store("Sources Pause");
            }
        }
        else {
            if (ImGui::Button(ICON_FA_PLAY) && enabled){
                for (auto source = selection_.begin(); source != selection_.end(); ++source)
                    (*source)->play(true);
                Action::manager().store("Sources Play");
            }
        }
    }
    // separate play & pause buttons for disagreeing sources
    else {
        if (ImGui::Button(ICON_FA_PLAY) && enabled) {
            for (auto source = selection_.begin(); source != selection_.end(); ++source)
                (*source)->play(true);
            Action::manager().store("Sources Play");
        }
        ImGui::SameLine(0, h_space_);
        if (ImGui::Button(ICON_FA_PAUSE) && enabled) {
            for (auto source = selection_.begin(); source != selection_.end(); ++source)
                (*source)->play(false);
            Action::manager().store("Sources Pause");
        }
    }
    ImGui::SameLine(0, h_space_);

    // restore style
    ImGui::PopStyleColor(3);
}
