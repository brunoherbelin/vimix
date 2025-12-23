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
#include <sstream>
#include <regex>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

#include "defines.h"
#include "Settings.h"
#include "Log.h"
#include "Toolkit/SystemToolkit.h"
#include "Toolkit/BaseToolkit.h"
#include "Toolkit/GlmToolkit.h"
#include "Toolkit/ImGuiToolkit.h"
#include "Toolkit/NetworkToolkit.h"
#include "Toolkit/DialogToolkit.h"
#include "View/View.h"
#include "View/RenderView.h"
#include "Scene/Primitives.h"
#include "Visitor/ImGuiVisitor.h"
#include "Visitor/InfoVisitor.h"
#include "Resource.h"
#include "ActionManager.h"
#include "Mixer.h"
#include "Source/MediaSource.h"
#include "Source/PatternSource.h"
#include "Source/DeviceSource.h"
#include "Source/ScreenCaptureSource.h"
#include "Source/MultiFileSource.h"
#include "Source/SourceCallback.h"
#include "RenderingManager.h"
#include "Connection.h"
#include "ControlManager.h"
#include "Recorder.h"
#include "MultiFileRecorder.h"
#include "Audio.h"
#include "MousePointer.h"
#include "Playlist.h"
#include "VideoBroadcast.h"
#include "ShmdataBroadcast.h"
#include "SessionCreator.h"
#include "Window/WorkspaceWindow.h"
#include "UserInterfaceManager.h"
#include "FrameGrabbing.h"

#include "Navigator.h"

// Forward declaration of utility function from UserInterfaceManager.cpp
std::string readable_date_time_string(std::string date);

std::vector< std::pair<int, int> > Navigator::icons_ordering_files = { {2,12}, {3,12}, {4,12}, {5,12} };
std::vector< std::string > Navigator::tooltips_ordering_files = { "Alphabetical", "Invert alphabetical", "Older files first", "Recent files first" };

Navigator::Navigator()
{
    // default geometry
    width_ = 100.f;
    pannel_width_ = 5.f * width_;
    height_ = 100.f;
    padding_width_ = 100.f;

    // clean start
    pannel_main_mode_ = Settings::application.pannel_main_mode;
    pannel_visible_ = false;
    pannel_alpha_ = 0.85f;
    view_pannel_visible = false;
    clearButtonSelection();

    // restore media mode as saved
    if (Settings::application.recentImportFolders.path.empty() ||
        Settings::application.recentImportFolders.path.compare(IMGUI_LABEL_RECENT_FILES) == 0)
        setNewMedia(MEDIA_RECENT);
    else if (Settings::application.recentImportFolders.path.compare(IMGUI_LABEL_RECENT_RECORDS) == 0)
        setNewMedia(MEDIA_RECORDING);
    else
        setNewMedia(MEDIA_FOLDER, Settings::application.recentImportFolders.path);

    source_to_replace = nullptr;
}

void Navigator::applyButtonSelection(int index)
{
    // ensure only one button is active at a time
    bool status = selected_button[index];
    clearButtonSelection();
    selected_button[index] = status;
    selected_index = index;

    // set visible if button is active
    pannel_visible_ = status;

    pannel_main_mode_ = Settings::application.pannel_main_mode;
}

void Navigator::clearNewPannel()
{
    new_source_preview_.setSource();
    pattern_type = -1;
    generated_type = -1;
    custom_connected = false;
    custom_screencapture = false;
    sourceSequenceFiles.clear();
    sourceMediaFileCurrent.clear();
    new_media_mode_changed = true;
}

void Navigator::clearButtonSelection()
{
    // clear all buttons
    for(int i=0; i<NAV_COUNT; ++i)
        selected_button[i] = false;

    // clear new source pannel
    clearNewPannel();
    source_to_replace = nullptr;
    selected_index = -1;
}

void Navigator::showPannelSource(int index)
{
    selected_index = index;
    // invalid index given
    if ( index < 0 )
        discardPannel();
    else {
        selected_button[index] = true;
        applyButtonSelection(index);
    }
}

int Navigator::selectedPannelSource()
{
    return selected_index;
}

void Navigator::showConfig()
{
    selected_button[NAV_MENU] = true;
    applyButtonSelection(NAV_MENU);
    pannel_main_mode_ = 2;
}

void Navigator::togglePannelMenu()
{
    selected_button[NAV_MENU] = !selected_button[NAV_MENU];
    applyButtonSelection(NAV_MENU);

    if (Settings::application.pannel_always_visible)
        showPannelSource(NAV_MENU);
}

void Navigator::togglePannelNew()
{
    selected_button[NAV_NEW] = !selected_button[NAV_NEW];
    applyButtonSelection(NAV_NEW);
    new_media_mode_changed = true;

    if (Settings::application.pannel_always_visible)
        showPannelSource( NAV_NEW );

}

void Navigator::togglePannelAutoHide()
{
    // toggle variable
    Settings::application.pannel_always_visible = !Settings::application.pannel_always_visible;

    // initiate change
    if (Settings::application.pannel_always_visible) {
        int current = Mixer::manager().indexCurrentSource();
        if ( current < 0 ) {
            if (!selected_button[NAV_MENU] && !selected_button[NAV_TRANS] && !selected_button[NAV_NEW] )
                showPannelSource(NAV_MENU);
        }
        else
            showPannelSource( current );
    }
    else {
        pannel_visible_ = true;
        discardPannel();
    }
}

bool Navigator::pannelVisible()
{
    return pannel_visible_ || Settings::application.pannel_always_visible;
}

void Navigator::discardPannel()
{
    // in the 'always visible mode',
    // discard the panel means cancel current action
    if ( Settings::application.pannel_always_visible ) {

        // if panel is the 'Insert' new source
        if ( selected_button[NAV_NEW] ) {
            // cancel the current source creation
            clearNewPannel();
        }
        // if panel is the 'Transition' session
        else if ( selected_button[NAV_TRANS] ) {
            // allows to hide pannel
            clearButtonSelection();
        }
        // if panel shows a source (i.e. not NEW, TRANS nor MENU selected)
        else if ( !selected_button[NAV_MENU] )
        {
            // revert to menu panel
            togglePannelMenu();

/// ALTERNATIVE : stay on source panel
//            // get index of current source
//            int idx = Mixer::manager().indexCurrentSource();
//            if (idx < 0) {
//                // no current source, try to get source previously in panel
//                Source *cs = Mixer::manager().sourceAtIndex( selected_index );
//                if ( cs )
//                    idx = selected_index;
//                else {
//                    // really no source is current, try to get one from user selection
//                    cs = Mixer::selection().front();
//                    if ( cs )
//                        idx = Mixer::manager().session()->index( Mixer::manager().session()->find(cs) );
//                }
//            }
//            // if current source or a selected source, show it's pannel
//            if (idx >= 0)
//                showPannelSource( idx );

        }
    }
    // in the general mode,
    // discard means hide pannel
    else if ( pannel_visible_)
        clearButtonSelection();

    pannel_visible_ = false;
    view_pannel_visible = false;
    pannel_main_mode_ = Settings::application.pannel_main_mode;
}

void Navigator::Render()
{
    std::tuple<std::string, std::string, Source *> tooltip = {"", "", nullptr};
    static uint _timeout_tooltip = 0;

    const ImGuiStyle& style = ImGui::GetStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(COLOR_NAVIGATOR, 1.f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(COLOR_NAVIGATOR, 1.f));

    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.50f, 0.50f));

    // calculate size of items based on text size and display dimensions
    width_ = 2.f *  ImGui::GetTextLineHeightWithSpacing();           // dimension of left bar depends on FONT_LARGE
    pannel_width_ = 5.f * width_;                                    // pannel is 5x the bar
    padding_width_ = 2.f * style.WindowPadding.x;                    // panning for alighment
    height_ = ImGui::GetIO().DisplaySize.y;                          // cover vertically
    const float icon_width = width_ - 2.f * style.WindowPadding.x;         // icons keep padding
    const ImVec2 iconsize(icon_width, icon_width);
    const float sourcelist_height = height_ - 6.5f * icon_width - 6.f * style.WindowPadding.y; // space for 4 icons of view

    // hack to show more sources if not enough space; make source icons smaller...
    ImVec2 sourceiconsize(icon_width, icon_width);
    if (sourcelist_height - 2.f * icon_width < Mixer::manager().session()->size() * icon_width )
        sourceiconsize.y *= 0.75f;

    // Left bar top
    ImGui::SetNextWindowPos( ImVec2(0, 0), ImGuiCond_Always );
    ImGui::SetNextWindowSize( ImVec2(width_, sourcelist_height), ImGuiCond_Always );
    ImGui::SetNextWindowBgAlpha(0.95f); // Transparent background
    if (ImGui::Begin( ICON_FA_BARS " Navigator", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |  ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing))
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        if (Settings::application.current_view != View::TRANSITION) {

            // the vimix icon for menu
            if (ImGuiToolkit::SelectableIcon(2, 16, "", selected_button[NAV_MENU], iconsize)) {
                selected_button[NAV_MENU] = true;
                applyButtonSelection(NAV_MENU);
            }
            if (ImGui::IsItemHovered())
                tooltip = {TOOLTIP_MAIN, SHORTCUT_MAIN, nullptr};

            // the "+" icon for action of creating new source
            if (ImGui::Selectable( source_to_replace != nullptr ? ICON_FA_PLUS_SQUARE : ICON_FA_PLUS,
                                   &selected_button[NAV_NEW], 0, iconsize)) {
                applyButtonSelection(NAV_NEW);
            }
            if (ImGui::IsItemHovered())
                tooltip = {TOOLTIP_NEW_SOURCE, SHORTCUT_NEW_SOURCE, nullptr};
            //
            // the list of INITIALS for sources
            //
            int index = 0;
            SourceList::iterator iter;
            for (iter = Mixer::manager().session()->begin(); iter != Mixer::manager().session()->end(); ++iter, ++index)
            {
                Source *s = (*iter);

                // Show failed sources in RED
                bool pushed = false;
                if (s->failed()){
                    pushed = true;
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_FAILED, 1.));
                    ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetColorU32(ImGuiCol_Button));
                    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGui::GetColorU32(ImGuiCol_ButtonActive));
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetColorU32(ImGuiCol_ButtonHovered));
                }

                // draw an indicator for selected sources (a dot) and for the current source (a line)
                if (s->mode() > Source::VISIBLE) {
                    // source is SELECTED or CURRENT
                    const ImVec2 p1 = ImGui::GetCursorScreenPos() +
                            ImVec2(icon_width, (s->mode() > Source::SELECTED ? 0.f : 0.5f * sourceiconsize.y - 2.5f) );
                    const ImVec2 p2 = ImVec2(p1.x, p1.y + (s->mode() > Source::SELECTED ? sourceiconsize.y : 5.f) );
                    const ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);
                    draw_list->AddLine(p1, p2, color, 5.f);
                }
                // draw select box
                ImGui::PushID(std::to_string(s->group(View::RENDERING)->id()).c_str());
                if (ImGui::Selectable(s->initials(), &selected_button[index], 0, sourceiconsize))
                {
                    applyButtonSelection(index);
                    if (selected_button[index])
                        Mixer::manager().setCurrentIndex(index);
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup)) {
                    std::string label = s->name().size() < 16 ? s->name()
                                                              : s->name().substr(0, 15) + "..";
                    // tooltip with text only if currently selected
                    if (selected_button[index])
                        tooltip = { label, "#" + std::to_string(index), nullptr };
                    // tooltip with preview if not currently selected
                    else
                        tooltip = { label, "#" + std::to_string(index), s };
                }
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
                {
                    ImGui::SetDragDropPayload("DND_SOURCE", &index, sizeof(int));
                    ImGui::Text( ICON_FA_SORT " %s ", s->initials());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_SOURCE"))
                    {
                        if ( payload->DataSize == sizeof(int) ) {
                            bool status_current_index = selected_button[Mixer::manager().indexCurrentSource()];
                            // drop means move index and reorder
                            int payload_index = *(const int*)payload->Data;
                            Mixer::manager().moveIndex(payload_index, index);
                            // index of current source changed
                            selected_button[Mixer::manager().indexCurrentSource()] = status_current_index;
                            applyButtonSelection(Mixer::manager().indexCurrentSource());
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                if (pushed)
                    ImGui::PopStyleColor(4);

                ImGui::PopID();
            }

        }
        else {
            // the ">" icon for transition menu
            if (ImGui::Selectable( ICON_FA_ARROW_CIRCLE_RIGHT, &selected_button[NAV_TRANS], 0, iconsize))
            {
                Mixer::manager().unsetCurrentSource();
                applyButtonSelection(NAV_TRANS);
            }
        }
        ImGui::End();
    }

    // Left bar bottom
    ImGui::SetNextWindowPos( ImVec2(0, sourcelist_height), ImGuiCond_Always );
    ImGui::SetNextWindowSize( ImVec2(width_, height_ - sourcelist_height + 1.f), ImGuiCond_Always );
    ImGui::SetNextWindowBgAlpha(0.95f); // Transparent background
    if (ImGui::Begin("##navigatorViews", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollWithMouse))
    {
        // Mouse pointer selector
        if ( RenderMousePointerSelector(iconsize) )
            tooltip = {TOOLTIP_SNAP_CURSOR, ALT_MOD, nullptr};

        // List of icons for View selection
        static uint view_options_timeout = 0;
        static ImVec2 view_options_pos = ImGui::GetCursorScreenPos();

        bool selected_view[View::INVALID] = {0};
        selected_view[ Settings::application.current_view ] = true;
        int previous_view = Settings::application.current_view;

        if (ImGui::Selectable( ICON_FA_BULLSEYE, &selected_view[View::MIXING], 0, iconsize))
        {
            UserInterface::manager().setView(View::MIXING);
            if (previous_view == Settings::application.current_view) {
                ImGui::OpenPopup( "PopupViewOptions" );
                view_options_pos = ImGui::GetCursorScreenPos();
            }
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup)) {
            tooltip = {Settings::application.views[View::MIXING].name, "F1", nullptr};
            view_options_timeout = 0;
        }

        if (ImGui::Selectable( ICON_FA_OBJECT_UNGROUP , &selected_view[View::GEOMETRY], 0, iconsize))
        {
            UserInterface::manager().setView(View::GEOMETRY);
            if (previous_view == Settings::application.current_view) {
                ImGui::OpenPopup( "PopupViewOptions" );
                view_options_pos = ImGui::GetCursorScreenPos();
            }
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup)) {
            tooltip = {Settings::application.views[View::GEOMETRY].name, "F2", nullptr};
            view_options_timeout = 0;
        }

        if (ImGuiToolkit::SelectableIcon(ICON_WORKSPACE, "", selected_view[View::LAYER], iconsize))
        {
            Settings::application.current_view = View::LAYER;
            UserInterface::manager().setView(View::LAYER);
            if (previous_view == Settings::application.current_view) {
                ImGui::OpenPopup( "PopupViewOptions" );
                view_options_pos = ImGui::GetCursorScreenPos();
            }
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup)) {
            tooltip = {Settings::application.views[View::LAYER].name, "F3", nullptr};
            view_options_timeout = 0;
        }

        if (ImGui::Selectable( ICON_FA_CHESS_BOARD, &selected_view[View::TEXTURE], 0, iconsize))
        {
            UserInterface::manager().setView(View::TEXTURE);
            if (previous_view == Settings::application.current_view) {
                ImGui::OpenPopup( "PopupViewOptions" );
                view_options_pos = ImGui::GetCursorScreenPos();
            }
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup)) {
            tooltip = {Settings::application.views[View::TEXTURE].name, "F4", nullptr};
            view_options_timeout = 0;
        }

        int j = Settings::application.render.disabled ? 8 : 7;
        if (ImGuiToolkit::SelectableIcon(10, j, "", selected_view[View::DISPLAYS], iconsize))
        {
            UserInterface::manager().setView(View::DISPLAYS);
            Settings::application.current_view = View::DISPLAYS;
            if (previous_view == Settings::application.current_view) {
                ImGui::OpenPopup( "PopupViewOptions" );
                view_options_pos = ImGui::GetCursorScreenPos();
            }
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup)) {
            tooltip = {Settings::application.views[View::DISPLAYS].name, "F5", nullptr};
            view_options_timeout = 0;
        }

        ImVec2 pos = ImGui::GetCursorPos();
        ImGui::SetCursorPos(pos + ImVec2(0.f, style.WindowPadding.y));
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_MONO);
        // icon for Fullscreen
        if ( ImGuiToolkit::IconButton( Rendering::manager().mainWindow().isFullscreen() ? ICON_FA_COMPRESS_ALT : ICON_FA_EXPAND_ALT ) )
            Rendering::manager().mainWindow().toggleFullscreen();
        if (ImGui::IsItemHovered())
            tooltip = {TOOLTIP_FULLSCREEN, SHORTCUT_FULLSCREEN, nullptr};

        // icon for toggle always visible / auto hide pannel
        ImGui::SetCursorPos(pos + ImVec2(width_ * 0.5f, style.WindowPadding.y));
        if ( ImGuiToolkit::IconButton( Settings::application.pannel_always_visible ? ICON_FA_TOGGLE_ON : ICON_FA_TOGGLE_OFF ) )
            togglePannelAutoHide();
        if (ImGui::IsItemHovered())
            tooltip = { Settings::application.pannel_always_visible ? TOOLTIP_PANEL_VISIBLE : TOOLTIP_PANEL_AUTO, SHORTCUT_PANEL_MODE, nullptr };

        ImGui::PopFont();

        // render the "PopupViewOptions"
        RenderViewOptions(&view_options_timeout, view_options_pos, iconsize);

        ImGui::End();
    }

    // show tooltip
    if (!std::get<0>(tooltip).empty()) {
        // pseudo timeout for showing tooltip
        if (_timeout_tooltip > IMGUI_TOOLTIP_TIMEOUT) {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 8.f));
            // if a pointer to a Source is provided in tupple
            Source *_s = std::get<2>(tooltip);
            if (_s != nullptr) {
                ImGui::BeginTooltip();
                const ImVec2 image_top = ImGui::GetCursorPos();
                const ImVec2 thumbnail_size = ImVec2(width_, width_ / _s->frame()->aspectRatio()) * 3.f;
                // Render source frame in tooltip
                ImGui::Image((void *) (uintptr_t) _s->frame()->texture(), thumbnail_size);
                // Draw label and shortcut from tupple
                ImGuiToolkit::PushFont(ImGuiToolkit::FONT_DEFAULT);
                ImGui::TextUnformatted(std::get<0>(tooltip).c_str());
                ImGui::SameLine();
                ImGui::SetCursorPosX(thumbnail_size.x + style.WindowPadding.x
                                     - ImGui::GetTextLineHeight() );
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6, 0.6, 0.6, 0.9f));
                ImGui::TextUnformatted(std::get<1>(tooltip).c_str());
                ImGui::PopStyleColor();
                // Draw source icon at the top right corner
                ImGui::SetCursorPos(image_top + ImVec2( thumbnail_size.x - ImGui::GetTextLineHeight()
                                                           - style.ItemSpacing.x, style.ItemSpacing.y ));
                ImGuiToolkit::Icon( _s->icon().x, _s->icon().y);
                ImGui::PopFont();
                ImGui::EndTooltip();
            }
            // otherwise just show a standard tooltip [action - shortcut key]
            else
                ImGuiToolkit::ToolTip(std::get<0>(tooltip).c_str(), std::get<1>(tooltip).c_str());
            ImGui::PopStyleVar();
        }
        else
            ++_timeout_tooltip;
    }
    else
        _timeout_tooltip = 0;

    ImGui::PopStyleVar();
    ImGui::PopFont();

    // Rendering of the side pannel
    if ( Settings::application.pannel_always_visible || pannel_visible_ ){

        // slight differences if temporari vixible or always visible panel
        if (Settings::application.pannel_always_visible)
            pannel_alpha_ = 0.95f;
        else {
            pannel_alpha_ = 0.85f;
            view_pannel_visible = false;
        }

        static bool reset_visitor = true;

        // pannel menu
        if (selected_button[NAV_MENU])
        {
            RenderMainPannel(iconsize);
            reset_visitor = true;
        }
        // pannel to manage transition
        else if (selected_button[NAV_TRANS])
        {
            RenderTransitionPannel(iconsize);
            reset_visitor = true;
        }
        // pannel to create a source
        else if (selected_button[NAV_NEW])
        {
            RenderNewPannel(iconsize);
            reset_visitor = true;
        }
        // pannel to configure a selected source
        else
        {
            if ( selected_index < 0 ) {
                showPannelSource(NAV_MENU);
            }
            // most often, render current sources
            else if ( selected_index == Mixer::manager().indexCurrentSource())
                RenderSourcePannel(Mixer::manager().currentSource(), iconsize, reset_visitor);
            // rarely its not the current source that is selected
            else {
                SourceList::iterator cs = Mixer::manager().session()->at( selected_index );
                if (cs != Mixer::manager().session()->end() )
                    RenderSourcePannel(*cs, iconsize, reset_visitor);
            }
            reset_visitor = false;
        }
    }

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();

}

void Navigator::RenderViewOptions(uint *timeout, const ImVec2 &pos, const ImVec2 &size)
{
    ImGuiContext& g = *GImGui;

    ImGui::SetNextWindowPos( pos + ImVec2(size.x + g.Style.WindowPadding.x, -size.y), ImGuiCond_Always );
    ImGui::SetNextWindowSize( ImVec2(size.x * 7.f, size.y), ImGuiCond_Always );
    if (ImGui::BeginPopup( "PopupViewOptions" ))
    {
        // vertical padding
        ImGui::SetCursorPosY( ImGui::GetCursorPosY() + g.Style.WindowPadding.y * 0.5f );

        // reset zoom
        if (ImGuiToolkit::IconButton(8,7)) {
            Mixer::manager().view((View::Mode)Settings::application.current_view)->recenter();
        }

        // percent zoom slider
        int percent_zoom = Mixer::manager().view((View::Mode)Settings::application.current_view)->size();
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        ImGui::SetNextItemWidth(-1.f);
        if (ImGui::SliderInt("##zoom", &percent_zoom, 0, 100, "%d %%" )) {
            Mixer::manager().view((View::Mode)Settings::application.current_view)->resize(percent_zoom);
        }

        // timer to close popup like a tooltip
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
            *timeout=0;
        else if ( (*timeout)++ > 10)
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
}

// Source pannel : *s was checked before
void Navigator::RenderSourcePannel(Source *s, const ImVec2 &iconsize, bool reset)
{
    if (s == nullptr || Settings::application.current_view == View::TRANSITION)
        return;

    // Next window is a side pannel
    const ImGuiStyle& style = ImGui::GetStyle();
    ImGui::SetNextWindowPos( ImVec2(width_, 0), ImGuiCond_Always );
    ImGui::SetNextWindowSize( ImVec2(pannel_width_, height_), ImGuiCond_Always );
    ImGui::SetNextWindowBgAlpha( pannel_alpha_ ); // Transparent background
    if (ImGui::Begin("##navigatorSource", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |  ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
    {
        // TITLE
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ImGui::SetCursorPosY(0.5f * (iconsize.y - ImGui::GetTextLineHeight()));
        ImGui::Text("Source");

        // index indicator
        ImGui::SetCursorPos(ImVec2(pannel_width_ - 2.8f * ImGui::GetTextLineHeightWithSpacing(), IMGUI_TOP_ALIGN));

        if (Mixer::manager().indexCurrentSource() < 0)
            Mixer::manager().setCurrentIndex(selected_index);
        ImGui::TextDisabled("#%d", Mixer::manager().indexCurrentSource());

        ImGui::PopFont();

        // name
        std::string sname = s->name();
        ImGui::SetCursorPosY(width_ - style.WindowPadding.x);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if (ImGuiToolkit::InputText("Name", &sname) ){
            Mixer::manager().renameSource(s, sname);
        }

        // Source pannel
        static ImGuiVisitor v;
        if (reset)
            v.reset();
        s->accept(v);

        ///
        /// AUDIO PANEL if audio available on source
        ///
        if (Settings::application.accept_audio && s->audioFlags() & Source::Audio_available) {
            ImGuiIO &io = ImGui::GetIO();

            // test audio and read volume
            bool audio_is_on = s->audioFlags() & Source::Audio_enabled;
            int vol = audio_is_on ? (int) (s->audioVolumeFactor(Source::VOLUME_BASE) * 100.f) : -1;
            std::string label = audio_is_on ? (vol > 50 ? ICON_FA_VOLUME_UP " %d%%"
                                                        : ICON_FA_VOLUME_DOWN " %d%%")
                                            : ICON_FA_VOLUME_MUTE " Disabled";
            // VOLUME & on/off slider
            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            bool volume_change = ImGui::SliderInt("##VolumeAudio", &vol, -1, 100, label.c_str());
            if (ImGui::IsItemHovered()) {
                if (io.MouseWheel != 0.f) {
                    vol = CLAMP(vol + int(10.f * io.MouseWheel), 0, 100);
                    volume_change = true;
                } else if (!audio_is_on)
                    ImGuiToolkit::ToolTip("Enabling audio will reload source.");
            }
            if (volume_change) {
                if (vol < 0)
                    s->setAudioEnabled(false);
                else {
                    s->setAudioEnabled(true);
                    s->setAudioVolumeFactor(Source::VOLUME_BASE,
                                            CLAMP((float) (vol) *0.01f, 0.f, 1.f));
                }
            }
            ImGui::SameLine(0, IMGUI_SAME_LINE);
            if (ImGuiToolkit::TextButton("Audio")) {
                s->setAudioEnabled(false);
            }

            // AUDIO MIXING menu
            if (audio_is_on) {
                ImGui::SameLine(0, 2 * IMGUI_SAME_LINE);
                static uint counter_menu_timeout_2 = 0;
                if (ImGuiToolkit::IconButton(6, 2)
                    || ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup)) {
                    counter_menu_timeout_2 = 0;
                    ImGui::OpenPopup("MenuMixAudio");
                }
                if (ImGui::BeginPopup("MenuMixAudio")) {
                    ImGui::TextDisabled("Multiply volume with:");
                    Source::AudioVolumeMixing flags = s->audioVolumeMix();
                    bool mix = flags & Source::Volume_mult_alpha;
                    if (ImGui::MenuItem("Source alpha", NULL, &mix)) {
                        if (mix)
                            s->setAudioVolumeMix(flags | Source::Volume_mult_alpha);
                        else
                            s->setAudioVolumeMix(flags & ~Source::Volume_mult_alpha);
                    }
                    mix = flags & Source::Volume_mult_opacity;
                    if (ImGui::MenuItem("Source fading", NULL, &mix)) {
                        if (mix)
                            s->setAudioVolumeMix(flags | Source::Volume_mult_opacity);
                        else
                            s->setAudioVolumeMix(flags & ~Source::Volume_mult_opacity);
                    }
                    mix = flags & Source::Volume_mult_session;
                    if (ImGui::MenuItem("Output fading", NULL, &mix)) {
                        if (mix)
                            s->setAudioVolumeMix(flags | Source::Volume_mult_session);
                        else
                            s->setAudioVolumeMix(flags & ~Source::Volume_mult_session);
                    }
                    if (ImGui::IsWindowHovered())
                        counter_menu_timeout_2 = 0;
                    else if (++counter_menu_timeout_2 > 10)
                        ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                }
            }
        }

        ///
        /// ACTION BUTTONS PANEL if not loading
        ///
        ImGui::Text(" ");
        if (s->ready() || s->failed()) {

            ImVec2 size = ImVec2(ImGui::GetContentRegionAvail().x, 0);

            // clone button
            if ( s->failed() )
                ImGuiToolkit::ButtonDisabled( ICON_FA_SHARE_SQUARE " Clone & Filter", size);
            else if ( ImGui::Button( ICON_FA_SHARE_SQUARE " Clone & Filter", size) ) {
                Mixer::manager().addSource ( (Source *) Mixer::manager().createSourceClone() );
                UserInterface::manager().showPannel(  Mixer::manager().numSource() );
            }

            // replace button
            if ( ImGui::Button( ICON_FA_PLUS_SQUARE " Replace", ImVec2((size.x - IMGUI_SAME_LINE)/2.f, 0)) ) {

                // prepare panel for new source of same type
                glm::ivec2 i = s->icon();
                if ( i == glm::ivec2(ICON_SOURCE_VIDEO) || i == glm::ivec2(ICON_SOURCE_IMAGE)
                     || i == glm::ivec2(ICON_SOURCE_CLONE) )
                    Settings::application.source.new_type = SOURCE_FILE;
                else if ( i == glm::ivec2(ICON_SOURCE_SEQUENCE) )
                    Settings::application.source.new_type = SOURCE_SEQUENCE;
                else if ( i == glm::ivec2(ICON_SOURCE_PATTERN) || i == glm::ivec2(ICON_SOURCE_TEXT)
                          || i == glm::ivec2(ICON_SOURCE_GSTREAMER) || i == glm::ivec2(ICON_SOURCE_SHADER)  )
                    Settings::application.source.new_type = SOURCE_GENERATED;
                else
                    Settings::application.source.new_type = SOURCE_CONNECTED;

                // switch to panel new source
                showPannelSource(NAV_NEW);
                // set source to be replaced
                source_to_replace = s;
            }

            // delete button
            ImGui::SameLine(0, IMGUI_SAME_LINE);
            if ( ImGui::Button( ACTION_DELETE, ImVec2((size.x - IMGUI_SAME_LINE)/2.f, 0)) ) {
                Mixer::manager().deleteSource(s);
                Action::manager().store(sname + std::string(": Deleted"));
            }
            // delete all button
            if ( Mixer::manager().session()->failedSources().size() > 1 ) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_FAILED, 1.));
                if ( ImGui::Button( ICON_FA_BACKSPACE " Delete all failed", size) ) {
                    auto failedsources = Mixer::manager().session()->failedSources();
                    for (auto sit = failedsources.cbegin(); sit != failedsources.cend(); ++sit) {
                        Mixer::manager().deleteSource( Mixer::manager().findSource( (*sit)->id() ) );
                    }
                }
                ImGui::PopStyleColor(1);
            }
        }
        ImGui::End();
    }
}


void Navigator::setNewMedia(MediaCreateMode mode, std::string path)
{
    Settings::application.source.new_type = Navigator::SOURCE_FILE;

    // change mode
    new_media_mode = mode;
    new_media_mode_changed = true;

    // mode dependent actions
    switch (new_media_mode) {
    case MEDIA_RECENT:
        // set filename
        sourceMediaFileCurrent = path;
        // set combo to 'recent files'
        Settings::application.recentImportFolders.path = IMGUI_LABEL_RECENT_FILES;
        break;
    case MEDIA_RECORDING:
        // set filename
        sourceMediaFileCurrent = path;
        // set combo to 'recent recordings'
        Settings::application.recentImportFolders.path = IMGUI_LABEL_RECENT_RECORDS;
        break;
    default:
    case MEDIA_FOLDER:
        // reset filename
        sourceMediaFileCurrent.clear();
        // set combo: a path was selected
        if (!path.empty())
            Settings::application.recentImportFolders.path.assign(path);
        break;
    }

    // clear preview
    new_source_preview_.setSource();
}

void Navigator::RenderNewPannel(const ImVec2 &iconsize)
{
    if (Settings::application.current_view == View::TRANSITION)
        return;

    const ImGuiStyle& style = ImGui::GetStyle();

    // Next window is a side pannel
    ImGui::SetNextWindowPos( ImVec2(width_, 0), ImGuiCond_Always );
    ImGui::SetNextWindowSize( ImVec2(pannel_width_, height_), ImGuiCond_Always );
    ImGui::SetNextWindowBgAlpha( pannel_alpha_ ); // Transparent background
    if (ImGui::Begin("##navigatorNewSource", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |  ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
    {
        // TITLE
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ImGui::SetCursorPosY(0.5f * (iconsize.y - ImGui::GetTextLineHeight()));
        if (source_to_replace != nullptr)
            ImGui::Text("Replace");
        else
            ImGui::Text("Insert");

        //
        // News Source selection pannel
        //
        ImGui::SetCursorPosY(width_ - style.WindowPadding.x);
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));

        ImGui::Columns(5, NULL, false);
        bool selected_type[5] = {0};
        selected_type[Settings::application.source.new_type] = true;
        if (ImGuiToolkit::SelectableIcon( 2, 5, "##SOURCE_FILE", selected_type[SOURCE_FILE], iconsize)) {
            Settings::application.source.new_type = SOURCE_FILE;
            clearNewPannel();
        }
        ImGui::NextColumn();
        if (ImGuiToolkit::SelectableIcon( ICON_SOURCE_SEQUENCE, "##SOURCE_SEQUENCE", selected_type[SOURCE_SEQUENCE], iconsize)) {
            Settings::application.source.new_type = SOURCE_SEQUENCE;
            clearNewPannel();
        }
        ImGui::NextColumn();
        if (ImGuiToolkit::SelectableIcon( 10, 9, "##SOURCE_CONNECTED", selected_type[SOURCE_CONNECTED], iconsize)) {
            Settings::application.source.new_type = SOURCE_CONNECTED;
            clearNewPannel();
        }
        ImGui::NextColumn();
        if (ImGuiToolkit::SelectableIcon( ICON_SOURCE_PATTERN, "##SOURCE_GENERATED", selected_type[SOURCE_GENERATED], iconsize)) {
            Settings::application.source.new_type = SOURCE_GENERATED;
            clearNewPannel();
        }
        ImGui::NextColumn();
        static int _previous_new_type = Settings::application.source.new_type;
        if (source_to_replace == nullptr) {
            if (ImGuiToolkit::SelectableIcon( ICON_SOURCE_GROUP, "##SOURCE_BUNDLE", selected_type[SOURCE_BUNDLE],
                iconsize, ImGuiDir_Right)) {
                _previous_new_type = Settings::application.source.new_type;
                Settings::application.source.new_type = SOURCE_BUNDLE;
                ImGui::OpenPopup("SOURCE_BUNDLE_MENU");            
                clearNewPannel();
            }
        }

        ImGui::Columns(1);
        ImGui::PopStyleVar();
        ImGui::PopFont();

        // Menu popup for SOURCE_BUNDLE
        if (ImGui::BeginPopup("SOURCE_BUNDLE_MENU")) {
            UserInterface::manager().showMenuBundle();
            ImGui::EndPopup();
        }
        // restore previous type after closing popup
        if (Settings::application.source.new_type == SOURCE_BUNDLE && !ImGui::IsPopupOpen("SOURCE_BUNDLE_MENU")) 
            Settings::application.source.new_type = _previous_new_type;

        // Edit menu
        ImGui::SetCursorPosY(2.f * width_ - style.WindowPadding.x);
        static bool request_open_shader_editor = false;

        // File Source creation
        if (Settings::application.source.new_type == SOURCE_FILE) {

            static DialogToolkit::OpenFileDialog fileimportdialog("Open Media",
                                                                   MEDIA_FILES_TYPE,
                                                                   MEDIA_FILES_PATTERN );
            static DialogToolkit::OpenFolderDialog folderimportdialog("Select Folder");

            ImGui::Text("Video, image & session files");

            // clic button to load file
            if ( ImGui::Button( ICON_FA_FOLDER_OPEN " Open", ImVec2(ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN, 0)) )
                fileimportdialog.open();
            // Indication
            ImGui::SameLine();
            ImGuiToolkit::HelpToolTip("Create a source from a file:\n"
                                                 ICON_FA_CARET_RIGHT " Video (*.mpg, *mov, *.avi, etc.)\n"
                                                 ICON_FA_CARET_RIGHT " Image (*.jpg, *.png, etc.)\n"
                                                 ICON_FA_CARET_RIGHT " Vector graphics (*.svg)\n"
                                                 ICON_FA_CARET_RIGHT " Vimix session (*.mix)\n"
                                                 "\nNB: Equivalent to dropping the file in the workspace");

            // get media file if dialog finished
            if (fileimportdialog.closed()){
                // get the filename from this file dialog
                std::string importpath = fileimportdialog.path();
                // switch to recent files
                setNewMedia(MEDIA_RECENT, importpath);
                // open file
                if (!importpath.empty()) {
                    // replace or open source
                    if (source_to_replace != nullptr)
                        Mixer::manager().replaceSource(source_to_replace, Mixer::manager().createSourceFile(sourceMediaFileCurrent));
                    else
                        Mixer::manager().addSource( Mixer::manager().createSourceFile(sourceMediaFileCurrent) );
                    // close NEW pannel
                    togglePannelNew();
                }
            }

            // combo to offer lists
            ImGui::Spacing();
            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            if (ImGui::BeginCombo("##SelectionNewMedia", BaseToolkit::truncated(Settings::application.recentImportFolders.path, 25).c_str() ))
            {
                // Mode MEDIA_RECENT : recent files
                if (ImGui::Selectable( ICON_FA_LIST_OL IMGUI_LABEL_RECENT_FILES) ) {
                     setNewMedia(MEDIA_RECENT);
                }
                // Mode MEDIA_RECORDING : recent recordings
                if (ImGui::Selectable( ICON_FA_LIST IMGUI_LABEL_RECENT_RECORDS) ) {
                    setNewMedia(MEDIA_RECORDING);
                }
                // Mode MEDIA_FOLDER : known folders
                for(auto foldername = Settings::application.recentImportFolders.filenames.begin();
                    foldername != Settings::application.recentImportFolders.filenames.end(); foldername++) {
                    std::string f = std::string(ICON_FA_FOLDER) + " " + BaseToolkit::truncated( *foldername, 40);
                    if (ImGui::Selectable( f.c_str() )) {
                        setNewMedia(MEDIA_FOLDER, *foldername);
                    }
                }
                // Add a folder for MEDIA_FOLDER
                if (ImGui::Selectable( ICON_FA_FOLDER_PLUS " List directory") ) {
                    folderimportdialog.open();
                }
                ImGui::EndCombo();
            }

            // return from thread for folder openning
            if (folderimportdialog.closed() && !folderimportdialog.path().empty()) {
                Settings::application.recentImportFolders.push(folderimportdialog.path());
                setNewMedia(MEDIA_FOLDER, folderimportdialog.path());
            }

            // position on top of list
            ImVec2 pos_top = ImGui::GetCursorPos();

            // change session list if changed
            if (new_media_mode_changed || Settings::application.recentImport.changed || Settings::application.recentRecordings.changed) {

                // MODE RECENT
                if ( new_media_mode == MEDIA_RECENT) {
                    // show list of recent imports
                    Settings::application.recentImport.validate();
                    sourceMediaFiles = Settings::application.recentImport.filenames;
                    // done changed
                    Settings::application.recentImport.changed = false;
                }
                // MODE RECORDINGS
                else if ( new_media_mode == MEDIA_RECORDING) {
                    // show list of recent records
                    Settings::application.recentRecordings.validate();
                    sourceMediaFiles = Settings::application.recentRecordings.filenames;
                    // in auto
                    if (Settings::application.recentRecordings.load_at_start
                            && Settings::application.recentRecordings.changed
                            && Settings::application.recentRecordings.filenames.size() > 0){
                        sourceMediaFileCurrent = sourceMediaFiles.front();
                        std::string label = BaseToolkit::transliterate( sourceMediaFileCurrent );
                        new_source_preview_.setSource( Mixer::manager().createSourceFile(sourceMediaFileCurrent), label);
                    }
                    // done changed
                    Settings::application.recentRecordings.changed = false;
                }
                // MODE LIST FOLDER
                else if ( new_media_mode == MEDIA_FOLDER) {
                    // show list of media files in folder
                    sourceMediaFiles = SystemToolkit::list_directory( Settings::application.recentImportFolders.path, { MEDIA_FILES_PATTERN },
                                                                      (SystemToolkit::Ordering) Settings::application.recentImportFolders.ordering);
                }
                // indicate the list changed (do not change at every frame)
                new_media_mode_changed = false;
            }

            // different labels for each mode
            static const char *listboxname[3] = { "##NewSourceMediaRecent", "##NewSourceMediaRecording", "##NewSourceMediafolder"};
            // display the import-list and detect if one was selected
            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            if (ImGui::ListBoxHeader(listboxname[new_media_mode], sourceMediaFiles.size(), CLAMP(sourceMediaFiles.size(), 4, 6)) ) {
                static int tooltip = 0;
                static std::string filenametooltip;
                // loop over list of files
                for(auto it = sourceMediaFiles.begin(); it != sourceMediaFiles.end(); ++it) {
                    // build displayed file name
                    std::string filename = BaseToolkit::transliterate(*it);
                    std::string label = BaseToolkit::truncated(SystemToolkit::filename(filename), 25);
                    // add selectable item to ListBox; open if clickec
                    if (ImGui::Selectable( label.c_str(), sourceMediaFileCurrent.compare(*it) == 0 )) {
                        // set new source preview
                        new_source_preview_.setSource( Mixer::manager().createSourceFile(*it), filename);
                        // remember current list item
                        sourceMediaFileCurrent = *it;
                    }
                    // smart tooltip : displays only after timout when item changed
                    if (ImGui::IsItemHovered()){
                        if (filenametooltip.compare(filename)==0){
                            ++tooltip;
                            if (tooltip>30) {
                                ImGui::BeginTooltip();
                                ImGui::Text("%s", filenametooltip.c_str());
                                ImGui::EndTooltip();
                            }
                        }
                        else {
                            filenametooltip.assign(filename);
                            tooltip = 0;
                        }
                    }
                }
                ImGui::ListBoxFooter();
            }

            // Supplementary icons to manage the list
            ImVec2 pos_bot = ImGui::GetCursorPos();
            if (new_media_mode == MEDIA_RECORDING) {
                // Clear list
                ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_top.y) );
                if (ImGuiToolkit::IconButton( 12, 14, "Clear list")) {
                    Settings::application.recentRecordings.filenames.clear();
                    Settings::application.recentRecordings.front_is_valid = false;
                    setNewMedia(MEDIA_RECORDING);
                }
                // Bottom Right side of the list: helper and options of Recent Recordings
                ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_bot.y - 2.f * ImGui::GetFrameHeightWithSpacing()));
                ImGuiToolkit::HelpToolTip("Recently recorded videos (lastest on top). Clic on a filename to open.\n\n"
                                         ICON_FA_CHEVRON_CIRCLE_RIGHT "  Auto-preload prepares this panel with the "
                                         "most recent recording after 'Stop Record' or 'Save & continue'.");
                ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_bot.y - ImGui::GetFrameHeightWithSpacing()) );
                if (ImGuiToolkit::ButtonToggle( ICON_FA_CHEVRON_CIRCLE_RIGHT, &Settings::application.recentRecordings.load_at_start, "Auto-preload" ) ){
                    // demonstrate action
                    if (Settings::application.recentRecordings.load_at_start
                            && Settings::application.recentRecordings.filenames.size() > 0) {
                        sourceMediaFileCurrent = sourceMediaFiles.front();
                        std::string label = BaseToolkit::transliterate( sourceMediaFileCurrent );
                        new_source_preview_.setSource( Mixer::manager().createSourceFile(sourceMediaFileCurrent), label);
                    }
                }
            }
            else if (new_media_mode == MEDIA_FOLDER) {
                ImGui::PushID("##new_media_directory_actions");
                // close list
                ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_top.y) );
                if (ImGuiToolkit::IconButton( 4, 5, "Close directory")) {
                    Settings::application.recentImportFolders.filenames.remove(Settings::application.recentImportFolders.path);
                    if (Settings::application.recentImportFolders.filenames.empty())
                        // revert mode RECENT
                        setNewMedia(MEDIA_RECENT);
                    else
                        setNewMedia(MEDIA_FOLDER, Settings::application.recentImportFolders.filenames.front());
                }
                // ordering list
                ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_top.y + ImGui::GetFrameHeightWithSpacing()) );
                if ( ImGuiToolkit::IconMultistate(icons_ordering_files, &Settings::application.recentImportFolders.ordering, tooltips_ordering_files) )
                    new_media_mode_changed = true;
                ImGui::PopID();
            }
            else if ( new_media_mode == MEDIA_RECENT ) {
                // Clear list
                ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_top.y) );
                if (ImGuiToolkit::IconButton( 12, 14, "Clear list")) {
                    Settings::application.recentImport.filenames.clear();
                    Settings::application.recentImport.front_is_valid = false;
                    setNewMedia(MEDIA_RECENT);
                }
            }
            // come back...
            ImGui::SetCursorPos(pos_bot);

        }
        // Sequence Source creator
        else if (Settings::application.source.new_type == SOURCE_SEQUENCE){

            static DialogToolkit::OpenManyFilesDialog _selectImagesDialog("Select multiple images",
                                                                          IMAGES_FILES_TYPE,
                                                                          IMAGES_FILES_PATTERN);
            static MultiFileSequence _numbered_sequence;
            static MultiFileRecorder _video_recorder;
            static int codec_id = -1;

            ImGui::Text("Image sequence");

            // clic button to load file
            if ( ImGui::Button( ICON_FA_FOLDER_OPEN " Open multiple", ImVec2(ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN, 0)) ) {
                sourceSequenceFiles.clear();
                new_source_preview_.setSource();
                _selectImagesDialog.open();
            }

            // Indication
            ImGui::SameLine();
            ImGuiToolkit::HelpToolTip("Create a source displaying a sequence of images;\n"
                                     ICON_FA_CARET_RIGHT " files numbered consecutively\n"
                                     ICON_FA_CARET_RIGHT " create a video from many images");

            // return from thread for folder openning
            if (_selectImagesDialog.closed()) {
                // clear
                new_source_preview_.setSource();
                // store list of files from dialog
                sourceSequenceFiles = _selectImagesDialog.files();
                if (sourceSequenceFiles.empty())
                    Log::Notify("No file selected.");

                // set sequence
                _numbered_sequence = MultiFileSequence(sourceSequenceFiles);

                // automatically create a MultiFile Source if possible
                if (_numbered_sequence.valid()) {
                    // always come back to propose image sequence when possible
                    codec_id = -1;
                    // show source preview available if possible
                    std::string label = BaseToolkit::transliterate( BaseToolkit::common_pattern(sourceSequenceFiles) );
                    new_source_preview_
                        .setSource(Mixer::manager().createSourceMultifile(sourceSequenceFiles,
                                                                          Settings::application.image_sequence.framerate_mode),
                                   label);
                } else
                    codec_id = Settings::application.image_sequence.profile;
            }

            // multiple files selected
            if (sourceSequenceFiles.size() > 1) {

                ImGui::Spacing();

                // show info sequence
                ImGuiTextBuffer info;
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.14f, 0.9f));
                info.appendf("%d %s", (int) sourceSequenceFiles.size(), _numbered_sequence.codec.c_str());
                ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                ImGui::InputText("Images", (char *)info.c_str(), info.size(), ImGuiInputTextFlags_ReadOnly);
                ImGui::PopStyleColor(1);

                // set framerate
                ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                ImGui::SliderInt("Framerate", &Settings::application.image_sequence.framerate_mode, 1, 30, "%d fps");
                if (ImGui::IsItemDeactivatedAfterEdit()){
                    if (new_source_preview_.filled()) {
                        std::string label = BaseToolkit::transliterate( BaseToolkit::common_pattern(sourceSequenceFiles) );
                        new_source_preview_
                            .setSource(Mixer::manager().createSourceMultifile(
                                           sourceSequenceFiles,
                                           Settings::application.image_sequence.framerate_mode),
                                       label);
                    }
                }

                // select CODEC: decide for gst sequence (codec_id = -1) or encoding a video
                ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                std::string codec_current = codec_id < 0 ? ICON_FA_SORT_NUMERIC_DOWN " Numbered images"
                                                         : std::string(ICON_FA_FILM " ") + VideoRecorder::profile_name[codec_id];
                if (ImGui::BeginCombo("##CodecSequence", codec_current.c_str())) {
                    // special case; if possible, offer to create an image sequence gst source
                    if (ImGui::Selectable( ICON_FA_SORT_NUMERIC_DOWN " Numbered images",
                                          codec_id < 0,
                                          _numbered_sequence.valid()
                                              ? ImGuiSelectableFlags_None
                                              : ImGuiSelectableFlags_Disabled)) {
                        // select id of image sequence
                        codec_id = -1;
                        // Open source preview for image sequence
                        if (_numbered_sequence.valid()) {
                            std::string label = BaseToolkit::transliterate(
                                BaseToolkit::common_pattern(sourceSequenceFiles));
                            new_source_preview_
                                .setSource(Mixer::manager().createSourceMultifile(
                                               sourceSequenceFiles,
                                               Settings::application.image_sequence.framerate_mode),
                                           label);
                        }
                    }
                    // always offer to encode a video
                    for (int i = VideoRecorder::H264_STANDARD; i < VideoRecorder::VP8; ++i) {
                        std::string label = std::string(ICON_FA_FILM " ") + VideoRecorder::profile_name[i];
                        if (ImGui::Selectable(label.c_str(), codec_id == i)) {
                            // select id of video encoding codec
                            codec_id = i;
                            Settings::application.image_sequence.profile = i;
                            // close source preview (no image sequence)
                            new_source_preview_.setSource();
                        }
                    }
                    ImGui::EndCombo();
                }
                // Indication
                ImGui::SameLine();
                if (_numbered_sequence.valid())
                    ImGuiToolkit::HelpToolTip(ICON_FA_SORT_NUMERIC_DOWN " Selected images are numbered consecutively; "
                                              "an image sequence source can be created.\n\n"
                                              ICON_FA_FILM " Alternatively, choose a codec to encode a video with the selected images and create a video source.");
                else
                    ImGuiToolkit::HelpToolTip(ICON_FA_SORT_NUMERIC_DOWN " Selected images are NOT numbered consecutively; "
                                              "it is not possible to create a sequence source.\n\n"
                                              ICON_FA_FILM " Instead, choose a codec to encode a video with the selected images and create a video source.");

                // if video encoding codec selected
                if ( codec_id >= 0 )
                {
                    // Offer to create video from sequence
                    ImGui::NewLine();
                    if ( ImGui::Button( ICON_FA_FILM " Encode video", ImVec2(ImGui::GetContentRegionAvail().x, 0)) ) {
                        // start video recorder
                        _video_recorder.setFiles( sourceSequenceFiles );
                        _video_recorder.setFramerate( Settings::application.image_sequence.framerate_mode );
                        _video_recorder.setProfile( (VideoRecorder::Profile) Settings::application.image_sequence.profile );
                        _video_recorder.start();
                        // open dialog
                        ImGui::OpenPopup(LABEL_VIDEO_SEQUENCE);
                    }
                }

                // video recorder finished: inform and open pannel to import video source from recent recordings
                if ( _video_recorder.finished() ) {
                    // video recorder failed if it does not return a valid filename
                    if ( _video_recorder.filename().empty() )
                        Log::Warning("Failed to generate an image sequence.");
                    else {
                        Log::Notify("Image sequence saved to %s.", _video_recorder.filename().c_str());
                        // open the file as new recording
                        //   if (Settings::application.recentRecordings.load_at_start)
                        UserInterface::manager().navigator.setNewMedia(Navigator::MEDIA_RECORDING, _video_recorder.filename());
                    }
                }
                else if (ImGui::BeginPopupModal(LABEL_VIDEO_SEQUENCE, NULL, ImGuiWindowFlags_NoResize))
                {
                    ImGui::Spacing();
                    ImGui::Text("Please wait while the video is being encoded :\n");

                    ImGui::Text("Resolution :");ImGui::SameLine(150);
                    ImGui::Text("%d x %d", _video_recorder.width(), _video_recorder.height() );
                    ImGui::Text("Framerate :");ImGui::SameLine(150);
                    ImGui::Text("%d fps", _video_recorder.framerate() );
                    ImGui::Text("Codec :");ImGui::SameLine(150);
                    ImGui::Text("%s", VideoRecorder::profile_name[ _video_recorder.profile() ] );
                    ImGui::Text("Frames :");ImGui::SameLine(150);
                    ImGui::Text("%lu / %lu", (unsigned long)_video_recorder.numFrames(),
                                (unsigned long)_video_recorder.files().size() );

                    ImGui::Spacing();
                    ImGui::ProgressBar(_video_recorder.progress());

                    ImGui::Spacing();
                    ImGui::Spacing();
                    if (ImGui::Button(ICON_FA_TIMES " Cancel",ImVec2(ImGui::GetContentRegionAvail().x, 0)))
                        _video_recorder.cancel();

                    ImGui::EndPopup();
                }

            }
            // single file selected
            else if (sourceSequenceFiles.size() > 0) {
                // open image file as source
                std::string label = BaseToolkit::transliterate( sourceSequenceFiles.front() );
                new_source_preview_.setSource( Mixer::manager().createSourceFile(sourceSequenceFiles.front()), label);
                // done with sequence
                sourceSequenceFiles.clear();
            }


        }
        // Generated patterns Source creator
        else if (Settings::application.source.new_type == SOURCE_GENERATED){

            static DialogToolkit::OpenFileDialog subtitleopenialog("Open Subtitle",
                                                                   SUBTITLE_FILES_TYPE,
                                                                   SUBTITLE_FILES_PATTERN );
            bool update_new_source = false;

            ImGui::Text("Patterns & generated graphics");

            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            if (ImGui::BeginCombo("##Pattern", "Select", ImGuiComboFlags_HeightLarge))
            {
                if ( ImGuiToolkit::BeginMenuIcon(ICON_SOURCE_PATTERN, "Static patterns"))
                {
                    for (int p = 0; p < (int) Pattern::count(); ++p) {
                        pattern_descriptor pattern = Pattern::get(p);
                        if (pattern.available && !pattern.animated) {
                            if (ImGui::Selectable(pattern.label.c_str())) {
                                update_new_source = true;
                                generated_type = 2;
                                pattern_type = p;
                            }
                        }
                    }
                    ImGui::EndMenu();
                }
                if ( ImGuiToolkit::BeginMenuIcon(ICON_SOURCE_PATTERN, "Animated patterns"))
                {
                    for (int p = 0; p < (int) Pattern::count(); ++p) {
                        pattern_descriptor pattern = Pattern::get(p);
                        if (pattern.available && pattern.animated) {
                            if (ImGui::Selectable(pattern.label.c_str())) {
                                update_new_source = true;
                                generated_type = 2;
                                pattern_type = p;
                            }
                        }
                    }
                    ImGui::EndMenu();
                }
                if ( ImGuiToolkit::SelectableIcon(ICON_SOURCE_TEXT, "Text", false) )
                {
                    update_new_source = true;
                    generated_type = 1;
                    pattern_type = -1;
                }
                if ( ImGuiToolkit::SelectableIcon(ICON_SOURCE_SHADER, "Custom shader", false) )
                {
                    update_new_source = true;
                    generated_type = 3;
                    pattern_type = -1;
                }
                if ( ImGuiToolkit::SelectableIcon(ICON_SOURCE_GSTREAMER, "Custom gstreamer", false) )
                {
                    update_new_source = true;
                    generated_type = 0;
                    pattern_type = -1;
                }
                ImGui::EndCombo();
            }

            static ImVec2 fieldsize(ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN, 100);
            static int numlines = 0;
            const ImGuiContext& g = *GImGui;
            fieldsize.y = MAX(3, numlines) * g.FontSize + g.Style.ItemSpacing.y + g.Style.FramePadding.y;

            // Indication
            ImGui::SameLine();
            ImGuiToolkit::HelpToolTip("Create a source with patterns or graphics generated algorithmically. "
                                      "Entering text or a custom gstreamer pipeline is also possible.");

            ImGui::Spacing();
            if (generated_type == 0) {
                static std::vector< std::pair< std::string, std::string> > _examples = { {"Videotest", "videotestsrc horizontal-speed=1 ! video/x-raw, width=640, height=480 " },
                                                                                         {"Checker", "videotestsrc pattern=checkers-8 ! video/x-raw, width=64, height=64 "},
                                                                                         {"Color", "videotestsrc pattern=gradient foreground-color= 0xff55f54f background-color= 0x000000 "},
                                                                                         {"Text", "videotestsrc pattern=black ! textoverlay text=\"vimix\" halignment=center valignment=center font-desc=\"Sans,72\" "},
                                                                                         {"GStreamer Webcam", "udpsrc port=5000 buffer-size=200000 ! h264parse ! avdec_h264 "},
                                                                                         {"SRT listener", "srtsrc uri=\"srt://:5000?mode=listener\" ! decodebin "}
                                                                                       };
                static std::string _description = _examples[0].second;

                // Editor
                if ( ImGuiToolkit::InputCodeMultiline("Pipeline", &_description, fieldsize, &numlines) )
                    update_new_source = true;

                // Local menu for list of examples
                ImVec2 pos_bot = ImGui::GetCursorPos();
                ImGui::SetCursorPos( pos_bot + ImVec2(fieldsize.x + IMGUI_SAME_LINE, -ImGui::GetFrameHeightWithSpacing()));
                if (ImGui::BeginCombo("##Examples", "Examples", ImGuiComboFlags_NoPreview | ImGuiComboFlags_HeightLarge))  {
                    ImGui::TextDisabled("Examples");
                    for (auto it = _examples.begin(); it != _examples.end(); ++it) {
                        if (ImGui::Selectable( it->first.c_str() ) ) {
                            _description = it->second;
                            update_new_source = true;
                        }
                    }
                    ImGui::Separator();
                    ImGui::TextDisabled("Explore online");
                    if (ImGui::Selectable( ICON_FA_EXTERNAL_LINK_ALT " Documentation" ) )
                        SystemToolkit::open("https://gstreamer.freedesktop.org/documentation/tools/gst-launch.html?gi-language=c#pipeline-description");
                    if (ImGui::Selectable( ICON_FA_EXTERNAL_LINK_ALT " Examples" ) )
                         SystemToolkit::open("https://github.com/thebruce87m/gstreamer-cheat-sheet");
                    ImGui::EndCombo();
                }
                ImGui::SetCursorPos(pos_bot);
                // take action
                if (update_new_source)
                    new_source_preview_.setSource( Mixer::manager().createSourceStream(_description), "Gstreamer source");

            }
            // if text source selected
            else if (generated_type == 1) {
                static std::vector<std::pair<std::string, std::string> > _examples
                    = {{"Hello", "Hello world!"},
                       {"Rich text", "Text in <i>italics</i> or <b>bold</b>"},
                       {"Multiline", "One\nTwo\nThree\nFour\nFive"} };
                static std::string _contents = _examples[0].second;

                // Editor
                if ( (SystemToolkit::has_extension(_contents, "srt") || SystemToolkit::has_extension(_contents, "sub") )
                    && SystemToolkit::file_exists(_contents)) {
                    static char dummy_str[1024];
                    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                    snprintf(dummy_str, 1024, "%s", _contents.c_str());
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.14f, 0.9f));
                    ImGui::InputText("##Filesubtitle",
                                     dummy_str,
                                     IM_ARRAYSIZE(dummy_str),
                                     ImGuiInputTextFlags_ReadOnly);
                    ImGui::PopStyleColor(1);
                }
                else if (ImGuiToolkit::InputTextMultiline("Text", &_contents, fieldsize, &numlines))
                    update_new_source = true;

                // Local menu for list of examples
                ImVec2 pos_bot = ImGui::GetCursorPos();
                ImGui::SetCursorPos(
                    pos_bot
                    + ImVec2(fieldsize.x + IMGUI_SAME_LINE, -ImGui::GetFrameHeightWithSpacing()));
                if (ImGui::BeginCombo("##Examples",
                                      "Examples",
                                      ImGuiComboFlags_NoPreview | ImGuiComboFlags_HeightLarge)) {
                    if (ImGui::Selectable(ICON_FA_FOLDER_OPEN " Open subtitle"))
                         subtitleopenialog.open();
                    ImGui::Separator();
                    ImGui::TextDisabled("Examples");
                    for (auto it = _examples.begin(); it != _examples.end(); ++it) {
                         if (ImGui::Selectable(it->first.c_str())) {
                            _contents = it->second;
                            update_new_source = true;
                         }
                    }
                    ImGui::Separator();
                    ImGui::TextDisabled("Explore online");
                    if (ImGui::Selectable(ICON_FA_EXTERNAL_LINK_ALT " Pango markup syntax"))
                         SystemToolkit::open("https://docs.gtk.org/Pango/pango_markup.html");
                    if (ImGui::Selectable(ICON_FA_EXTERNAL_LINK_ALT " SubRip file format"))
                         SystemToolkit::open("https://en.wikipedia.org/wiki/SubRip");
                    ImGui::EndCombo();
                }
                ImGui::SameLine(0, IMGUI_SAME_LINE);
                ImGuiToolkit::Indication("Format and layout options will be available after source creation.", ICON_FA_INFO_CIRCLE);
                ImGui::SetCursorPos(pos_bot);

                // resolution
                ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                if (ImGui::Combo("Ratio",
                                 &Settings::application.source.ratio,
                                 GlmToolkit::aspect_ratio_names,
                                 IM_ARRAYSIZE(GlmToolkit::aspect_ratio_names)))
                    update_new_source = true;

                ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                if (ImGui::Combo("Height",
                                 &Settings::application.source.res,
                                 GlmToolkit::height_names,
                                 IM_ARRAYSIZE(GlmToolkit::height_names)))
                    update_new_source = true;

                // get subtitle file if dialog finished
                if (subtitleopenialog.closed()) {
                    // get the filename from this file dialog
                    std::string importpath = subtitleopenialog.path();
                    // open file
                    if (!importpath.empty()) {
                         _contents = importpath;
                         update_new_source = true;
                    }
                }

                // take action
                if (update_new_source) {
                    glm::ivec2 res = GlmToolkit::resolutionFromDescription(Settings::application.source.ratio, Settings::application.source.res);
                    new_source_preview_.setSource(Mixer::manager().createSourceText(_contents, res), "Text source");
                }
            }
            // if shader source selected
            else if (generated_type == 3 ) {
                static bool auto_open_shader_editor = true;
                if (ImGuiToolkit::ButtonSwitch( ICON_FA_CODE " Open editor at creation", &auto_open_shader_editor) ) 
                    request_open_shader_editor = auto_open_shader_editor;
                ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                if (ImGui::Combo("Ratio", &Settings::application.source.ratio,
                                 GlmToolkit::aspect_ratio_names, IM_ARRAYSIZE(GlmToolkit::aspect_ratio_names) ) )
                    update_new_source = true;

                ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                if (ImGui::Combo("Height", &Settings::application.source.res,
                                 GlmToolkit::height_names, IM_ARRAYSIZE(GlmToolkit::height_names) ) )
                    update_new_source = true;

                // create preview
                if (update_new_source) {
                    glm::ivec2 res = GlmToolkit::resolutionFromDescription(Settings::application.source.ratio, Settings::application.source.res);
                    new_source_preview_.setSource( Mixer::manager().createSourceShader(res), "Shader source");
                    request_open_shader_editor = auto_open_shader_editor;
                }
            }
            else {
                // resolution
                if (pattern_type >= 0) {

                    static char dummy_str[1024];
                    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                    pattern_descriptor pattern = Pattern::get(pattern_type);
                    snprintf(dummy_str, 1024, "%s", pattern.label.c_str());
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.14f, 0.9f));
                    ImGui::InputText("Pattern", dummy_str, IM_ARRAYSIZE(dummy_str), ImGuiInputTextFlags_ReadOnly);
                    ImGui::PopStyleColor(1);

                    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                    if (ImGui::Combo("Ratio", &Settings::application.source.ratio,
                                     GlmToolkit::aspect_ratio_names, IM_ARRAYSIZE(GlmToolkit::aspect_ratio_names) ) )
                        update_new_source = true;

                    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                    if (ImGui::Combo("Height", &Settings::application.source.res,
                                     GlmToolkit::height_names, IM_ARRAYSIZE(GlmToolkit::height_names) ) )
                        update_new_source = true;
                }
                // create preview
                if (update_new_source) {
                    glm::ivec2 res = GlmToolkit::resolutionFromDescription(Settings::application.source.ratio, Settings::application.source.res);
                    new_source_preview_.setSource( Mixer::manager().createSourcePattern(pattern_type, res),
                                                   Pattern::get(pattern_type).label);
                }
            }
        }
        // Input and connected source creator
        else if (Settings::application.source.new_type == SOURCE_CONNECTED){

            ImGui::Text("Input devices & streams");

            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            if (ImGui::BeginCombo("##ExternalConnected", "Select "))
            {
                // 1. Loopback source
                if ( ImGuiToolkit::SelectableIcon(ICON_SOURCE_RENDER, "Display Loopback", false) ) {
                    custom_connected = false;
                    custom_screencapture = false;
                    new_source_preview_.setSource( Mixer::manager().createSourceRender(), "Display Loopback");
                }

                // 2. Screen capture (open selector)
                if ( ImGuiToolkit::SelectableIcon(ICON_SOURCE_DEVICE_SCREEN, "Screen capture", false) ) {
                    custom_connected = false;
                    new_source_preview_.setSource();
                    custom_screencapture = true;
                }

                // 3. Network connected SRT
                if ( ImGuiToolkit::SelectableIcon(ICON_SOURCE_SRT, "SRT Broadcast", false) ) {
                    new_source_preview_.setSource();
                    custom_connected = true;
                    custom_screencapture = false;
                }

                // 4. Devices
                ImGui::Separator();
                for (int d = 0; d < Device::manager().numDevices(); ++d){
                    std::string namedev = Device::manager().name(d);
                    if (ImGui::Selectable( namedev.c_str() )) {
                        custom_connected = false;
                        custom_screencapture = false;
                        new_source_preview_.setSource( Mixer::manager().createSourceDevice(namedev), namedev);
                    }
                }

                // 5. Network connected vimix
                for (int d = 1; d < Connection::manager().numHosts(); ++d){
                    std::string namehost = Connection::manager().info(d).name;
                    if (ImGui::Selectable( namehost.c_str() )) {
                        custom_connected = false;
                        custom_screencapture = false;
                        new_source_preview_.setSource( Mixer::manager().createSourceNetwork(namehost), namehost);
                    }
                }

                ImGui::EndCombo();
            }

            // Indication
            ImGui::SameLine();
            ImVec2 pos = ImGui::GetCursorPos();
            ImGuiToolkit::HelpToolTip("Create a source capturing video streams from connected devices or machines;\n"
                                      ICON_FA_CARET_RIGHT " vimix display loopback\n"
                                      ICON_FA_CARET_RIGHT " screen capture\n"
                                      ICON_FA_CARET_RIGHT " broadcasted with SRT over network.\n"
                                      ICON_FA_CARET_RIGHT " webcams or frame grabbers\n"
                                      ICON_FA_CARET_RIGHT " vimix Peer-to-peer in local network.");
            ImGui::SameLine();
            if (ImGuiToolkit::IconButton(5, 15, "Reload list")) {
                Device::manager().reload();
                clearNewPannel();
            }
            ImGui::Spacing();

            if (custom_connected) {

                bool valid_ = false;
                static std::string url_;
                static std::string ip_ = Settings::application.recentSRT.hosts.empty() ? Settings::application.recentSRT.default_host.first : Settings::application.recentSRT.hosts.front().first;
                static std::string port_ = Settings::application.recentSRT.hosts.empty() ? Settings::application.recentSRT.default_host.second : Settings::application.recentSRT.hosts.front().second;
                static std::regex ipv4("(([0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])\\.){3}([0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])");
                static std::regex numport("([0-9]){4,6}");

                ImGui::NewLine();
                ImGuiToolkit::Icon(ICON_SOURCE_SRT);
                ImGui::SameLine();
                ImGui::Text("SRT broadcast");
                ImGui::SameLine();
                ImGui::SetCursorPosX(pos.x);
                ImGuiToolkit::HelpToolTip("Set the IP and Port for connecting with Secure Reliable Transport (SRT) protocol to a video broadcaster that is waiting for connections (listener mode).");

                // Entry field for IP
                ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                ImGuiToolkit::InputText("IP", &ip_, ImGuiInputTextFlags_CharsDecimal);
                valid_ = std::regex_match(ip_, ipv4);

                // Entry field for port
                ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                ImGuiToolkit::InputText("Port", &port_, ImGuiInputTextFlags_CharsDecimal);
                valid_ &= std::regex_match(port_, numport);

                // URL generated from protorol, IP and port
                url_ = Settings::application.recentSRT.protocol + ip_ + ":" + port_;

                // push style for disabled text entry
                ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.14f, 0.8f));

                // display default IP & port
                if (Settings::application.recentSRT.hosts.empty()) {
                    ImGuiToolkit::InputText("##url", &url_, ImGuiInputTextFlags_ReadOnly);
                }
                // display most recent host & offer list of known hosts
                else {
                    if (ImGui::BeginCombo("##SRThosts", url_.c_str()))  {
                        for (auto it = Settings::application.recentSRT.hosts.begin(); it != Settings::application.recentSRT.hosts.end(); ++it) {

                            if (ImGui::Selectable( std::string(Settings::application.recentSRT.protocol + it->first + ":" + it->second).c_str() ) ) {
                                ip_ = it->first;
                                port_ = it->second;
                            }
                        }
                        ImGui::EndCombo();
                    }
                    // icons to clear lists
                    ImVec2 pos_top = ImGui::GetCursorPos();
                    ImGui::SameLine();
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.7);
                    if (ImGuiToolkit::IconButton( ICON_FA_BACKSPACE, "Clear list of recent uri")) {
                        Settings::application.recentSRT.hosts.clear();
                        ip_ = Settings::application.recentSRT.default_host.first;
                        port_ = Settings::application.recentSRT.default_host.second;
                    }
                    ImGui::PopStyleVar();
                    ImGui::SetCursorPos(pos_top);

                }

                // pop disabled style
                ImGui::PopStyleColor(1);

                // push a RED color style if host is not valid
                ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, valid_ ? 0.0f : 0.6f, 0.4f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.0f, valid_ ? 0.0f : 0.7f, 0.3f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.0f, valid_ ? 0.0f : 0.8f, 0.2f));

                // create a new SRT source if host is valid
                if ( ImGui::Button("Call", ImVec2(IMGUI_RIGHT_ALIGN, 0)) && valid_ ) {
                    // set preview source
                    new_source_preview_.setSource( Mixer::manager().createSourceSrt(ip_, port_), url_);
                    // remember known host
                    Settings::application.recentSRT.push(ip_, port_);
                }

                ImGui::PopStyleColor(3);
            }

            if (custom_screencapture) {

                ImGui::NewLine();
                ImGuiToolkit::Icon(ICON_SOURCE_DEVICE_SCREEN);
                ImGui::SameLine();
                ImGui::Text("Screen Capture");

                ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                if (ImGui::BeginCombo("##ScreenCaptureSelect", "Select window", ImGuiComboFlags_HeightLarge))
                {
                    for (int d = 0; d < ScreenCapture::manager().numWindow(); ++d){
                        std::string namewin = ScreenCapture::manager().name(d);
                        if (ImGui::Selectable( namewin.c_str() )) {
                            new_source_preview_.setSource( Mixer::manager().createSourceScreen(namewin), namewin);
                        }
                    }
                    ImGui::EndCombo();
                }
            }
        }

        ImGui::NewLine();

        // if a new source was added
        if (new_source_preview_.filled()) {
            // show preview
            new_source_preview_.Render(ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN);
            // ask to import the source in the mixer
            ImGui::NewLine();
            if (new_source_preview_.ready() && ImGui::Button( ICON_FA_CHECK "  Ok", ImVec2(pannel_width_ - padding_width_, 0)) ) {
                // take out the source from the preview
                Source *s = new_source_preview_.getSource();
                // restart and add the source.
                if (source_to_replace != nullptr)
                    Mixer::manager().replaceSource(source_to_replace, s);
                else
                    Mixer::manager().addSource(s);
                s->replay();
                // close NEW pannel
                togglePannelNew();
                // open shader editor if requested
                if (request_open_shader_editor) {
                    Settings::application.widget.shader_editor = true;
                    request_open_shader_editor = false;
                }
            }
        }

        ImGui::End();
    }
}

bool Navigator::RenderMousePointerSelector(const ImVec2 &size)
{
    ImGuiContext& g = *GImGui;
    ImVec2 top = ImGui::GetCursorPos();
    bool enabled = Settings::application.current_view != View::TRANSITION;
    bool ret = false;
    ///
    /// interactive button of the given size: show menu if clic or mouse over
    ///
    static uint counter_menu_timeout = 0;
    if ( ImGui::InvisibleButton("##MenuMousePointerButton", size) /*|| ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup)*/ ) {

        if (enabled)
            ImGui::OpenPopup( "MenuMousePointer" );
    }
    ImVec2 bottom = ImGui::GetCursorScreenPos();

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup)) {
        ret = true;
        counter_menu_timeout=0;
    }

    // Change color of icons depending on context menu status
    const ImVec4* colors = ImGui::GetStyle().Colors;
    if (!enabled) 
        ImGui::PushStyleColor( ImGuiCol_Text, colors[ImGuiCol_TextDisabled] );
    else if (ret || ImGui::IsPopupOpen("MenuMousePointer") )
        ImGui::PushStyleColor( ImGuiCol_Text, colors[ImGuiCol_DragDropTarget]);
    else        
        ImGui::PushStyleColor( ImGuiCol_Text, colors[ImGuiCol_Text] );

    // Draw centered icon of Mouse pointer
    ImVec2 margin = (size - ImVec2(g.FontSize, g.FontSize)) * 0.5f;
    ImGui::SetCursorPos( top + margin );

    if ( UserInterface::manager().altModifier() || Settings::application.mouse_pointer_lock) {
        // icon with corner erased
        ImGuiToolkit::Icon(ICON_POINTER_OPTION);

        // Draw sub-icon of Mouse pointer type
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_DEFAULT);
        ImVec2 t = top + size - ImVec2(g.FontSize, g.FontSize) - ImVec2(g.Style.FramePadding.y, g.Style.FramePadding.y);
        ImGui::SetCursorPos( t );
        std::tuple<int, int, std::string, std::string> mode = Pointer::Modes.at( (size_t) Settings::application.mouse_pointer);
        ImGuiToolkit::Icon(std::get<0>(mode), std::get<1>(mode));
        ImGui::PopFont();
    }
    else
        // standard icon
        ImGuiToolkit::Icon(ICON_POINTER_DEFAULT);

    // Revert
    ImGui::PopStyleColor(1);
    ImGui::SetCursorScreenPos(bottom);

    ///
    /// Render the Popup menu selector
    ///
    ImGui::SetNextWindowPos( bottom + ImVec2(size.x + g.Style.WindowPadding.x, -size.y), ImGuiCond_Always );
    if (ImGui::BeginPopup( "MenuMousePointer" ))
    {
        // loop over all mouse pointer modes
        for ( size_t m = Pointer::POINTER_GRID; m < Pointer::POINTER_INVALID; ++m) {
            bool on = m == (size_t) Settings::application.mouse_pointer;
            const std::tuple<int, int, std::string, std::string> mode = Pointer::Modes.at(m);
            // show icon of mouse mode and set mouse pointer if selected
            if (ImGuiToolkit::IconToggle( std::get<0>(mode), std::get<1>(mode), &on, std::get<2>(mode).c_str()) )
                Settings::application.mouse_pointer = (int) m;
            // space between icons
            ImGui::SameLine(0, IMGUI_SAME_LINE);
        }

        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_DEFAULT);

        // button to lock the ALT activation
        ImGui::SetCursorPosY(margin.y);
        ImGui::SameLine(0, IMGUI_SAME_LINE * 3);
        ImGuiToolkit::ButtonToggle(Settings::application.mouse_pointer_lock ? ICON_FA_LOCK ALT_LOCK : ICON_FA_UNLOCK ALT_LOCK,
                                   &Settings::application.mouse_pointer_lock,
                                   "Activate the selected Snap mouse cursor by pressing the [" ALT_MOD "] key.\n\n"
                                   ICON_FA_LOCK ALT_LOCK " keeps the Snap mouse cursor active.");

        // slider to adjust strength of the mouse pointer
        float *val = &Settings::application.mouse_pointer_strength[ Settings::application.mouse_pointer ];
        // General case
        if (Settings::application.mouse_pointer != Pointer::POINTER_GRID) {
            int percent = *val * 100.f;
            ImGui::SetNextItemWidth( IMGUI_RIGHT_ALIGN );
            if (ImGui::SliderInt( "##sliderstrenght", &percent, 0, 100, percent < 1 ? "Min" : "%d%%") )
                *val = 0.01f * (float) percent;
            if (ImGui::IsItemHovered() && g.IO.MouseWheel != 0.f ){
                *val += 0.1f * g.IO.MouseWheel;
                *val = CLAMP( *val, 0.f, 1.f);
            }
        }
        // special case of GRID
        else {
            // toggle proportional grid
            const char *tooltip_lock[2] = {"Square grid", "Aspect-ratio grid"};
            if ( ImGuiToolkit::IconToggle(19, 2, 18, 2, &Settings::application.proportional_grid, tooltip_lock) )
                View::need_deep_update_++;
            ImGui::SameLine(0, IMGUI_SAME_LINE);
            // slider of 5 text values
            static const char* grid_names[Grid::UNIT_ONE+1] = { "Precise", "Small", "Default", "Large", "Huge"};
            int grid_current = (Grid::Units) round( *val * 4.f) ;
            const char* grid_current_name = (grid_current >= 0 && grid_current <= Grid::UNIT_ONE) ?
                        grid_names[grid_current] : "Unknown";
            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            if (ImGui::SliderInt("##slidergrid", &grid_current, 0, Grid::UNIT_ONE, grid_current_name) )
                *val = (float) grid_current * 0.25f;
            if (ImGui::IsItemHovered() && g.IO.MouseWheel != 0.f ){
                *val += 0.25f * g.IO.MouseWheel;
                *val = CLAMP( *val, 0.f, 1.f);
            }
        }
        // Label & reset button
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        if (ImGuiToolkit::TextButton( std::get<3>(Pointer::Modes.at(Settings::application.mouse_pointer)).c_str() ))
            *val = 0.5f;
        ImGui::PopFont();

        // timer to close menu like a tooltip
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
            counter_menu_timeout=0;
        else if (++counter_menu_timeout > 10)
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    return ret;
}

void Navigator::RenderMainPannelSession()
{
    const float preview_width = ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN;
    const float preview_height = 4.5f * ImGui::GetFrameHeightWithSpacing();
    const float space = ImGui::GetStyle().ItemSpacing.y;

    //
    // Session
    //
    std::string sessions_current = Mixer::manager().session()->filename();
    if (sessions_current.empty())
        sessions_current = "<unsaved>";
    else
        sessions_current = SystemToolkit::filename(sessions_current);

    //
    // Show combo box of recent files
    //
    static std::list<std::string> sessions_list;
    // get list of recent sessions when it changed, not at every frame
    if (Settings::application.recentSessions.changed) {
        Settings::application.recentSessions.changed = false;
        Settings::application.recentSessions.validate();
        sessions_list = Settings::application.recentSessions.filenames;
    }
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    if (ImGui::BeginCombo("##RecentSessions", sessions_current.c_str() )) {
        // list all sessions in recent list
        for(auto it = sessions_list.begin(); it != sessions_list.end(); ++it) {
            if (ImGui::Selectable( SystemToolkit::filename(*it).c_str() ) ) {
                Mixer::manager().open( *it );
            }
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text( "%s", (*it).c_str() );
                ImGui::EndTooltip();
            }
        }
        ImGui::EndCombo();
    }
    ImVec2 pos = ImGui::GetCursorPos();
    if (!Mixer::manager().session()->filename().empty()) {
        ImGui::SameLine();
        if ( ImGuiToolkit::IconButton(ICON_FA_TIMES, "Close"))
            Mixer::manager().close();
        ImGui::SetCursorPos(pos);
    }
////    if ( Mixer::manager().session()->filename().empty()) {
////        if ( ImGuiToolkit::IconButton(ICON_FA_FILE_DOWNLOAD, "Save as"))
////            UserInterface::manager().saveOrSaveAs();
////    } else {
////        if (ImGuiToolkit::IconButton(3, 5, "Show in finder"))
////            SystemToolkit::open(SystemToolkit::path_filename(Mixer::manager().session()->filename()));
////    }

    //
    // Preview session
    //
    Session *se = Mixer::manager().session();
    if (se->frame()) {
        float width = preview_width;
        float height = se->frame()->projectionSize().y * width / ( se->frame()->projectionSize().x * se->frame()->aspectRatio());
        if (height > preview_height - space) {
            height = preview_height - space;
            width = height * se->frame()->aspectRatio() * ( se->frame()->projectionSize().x / se->frame()->projectionSize().y);
        }
        // centered image
        ImGui::SetCursorPos( ImVec2(pos.x + 0.5f * (preview_width-width), pos.y) );
        ImGui::Image((void*)(uintptr_t) se->frame()->texture(), ImVec2(width, height));
    }

    // right side options for session
    if (!Mixer::manager().session()->filename().empty()) {

        //
        // Right align icon top : heart to add to favorites
        //
        ImGui::SetCursorPos( ImVec2(preview_width + 20, pos.y + space) );
        // if session is in favorites
        if ( UserInterface::manager().favorites.has( Mixer::manager().session()->filename() ) > 0 ) {
            // offer to remove from favorites
            if ( ImGuiToolkit::IconButton( 15, 4 , "Remove from favorites")) {
                UserInterface::manager().favorites.remove( Mixer::manager().session()->filename() );
            }
        }
        // else session is not in favorites, offer to add
        else if ( ImGuiToolkit::IconButton( 16, 4 , "Add to favorites")) {
            UserInterface::manager().favorites.add( Mixer::manager().session()->filename() );
        }

        //
        // Right align icon middle : sticky note
        //
        ImGui::SetCursorPos( ImVec2(preview_width + 20, pos.y + preview_height - 2.f * ImGui::GetFrameHeightWithSpacing()) );
        if ( ImGuiToolkit::IconButton( ICON_FA_STICKY_NOTE " +", "Add a sticky note")) {
            Mixer::manager().session()->addNote();
        }

        //
        // Right align bottom icon : thumbnail of session file, on/off
        //
        static Thumbnail _session_thumbnail;
        static FrameBufferImage *_thumbnail = nullptr;
        bool _user_thumbnail = Mixer::manager().session()->thumbnail() != nullptr;
        ImGui::SetCursorPos( ImVec2(preview_width + 20, pos.y + preview_height - ImGui::GetFrameHeightWithSpacing()) );
        if (ImGuiToolkit::IconToggle(2, 8, 7, 8, &_user_thumbnail)) {
            if (_user_thumbnail)
                Mixer::manager().session()->setThumbnail();
            else {
                Mixer::manager().session()->resetThumbnail();
                _session_thumbnail.reset();
            }
            _thumbnail = nullptr;
        }
        if (ImGui::IsItemHovered()){
            // thumbnail changed
            if (_thumbnail != Mixer::manager().session()->thumbnail()) {
                _session_thumbnail.reset();
                _thumbnail = Mixer::manager().session()->thumbnail();
                if (_thumbnail != nullptr)
                    _session_thumbnail.fill( _thumbnail );
            }
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 8.f));
            ImGui::BeginTooltip();
            if (_session_thumbnail.filled()) {
                _session_thumbnail.Render(230);
                ImGui::Text(" Custom thumbnail");
            }
            else {
                ImGui::Text(" Automatic thumbnail ");
            }
            ImGui::EndTooltip();
            ImGui::PopStyleVar();
        }
    }

    // Menu for actions on current session
    ImGui::SetCursorPos( ImVec2( pos.x, pos.y + preview_height));
    ImVec2 pos_bot = ImGui::GetCursorPos();
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.f,0.f,0.f,0.f));

    //
    // RESOLUTION
    //
    Settings::application.pannel_session[0] = ImGui::CollapsingHeader("Resolution",
                                                                      Settings::application.pannel_session[0] ? ImGuiTreeNodeFlags_DefaultOpen : 0);
    if (Settings::application.pannel_session[0]) {

        // Information and resolution
        const FrameBuffer *output = Mixer::manager().session()->frame();
        if (output)  {
            // change resolution (height only)
            // get parameters to edit resolution
            glm::ivec2 preset = RenderView::presetFromResolution(output->resolution());
            glm::ivec2 custom = glm::ivec2(output->resolution());
            if (preset.x > -1) {
                // cannot change resolution when recording
                if ( Outputs::manager().enabled( FrameGrabber::GRABBER_VIDEO ) ||
                     Outputs::manager().enabled( FrameGrabber::GRABBER_GPU ) ) {
                    // show static info (same size than combo)
                    static char dummy_str[512];
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.14f, 0.9f));
                    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                    snprintf(dummy_str, 512, "%s", RenderView::ratio_preset_name[preset.x]);
                    ImGui::InputText("Ratio", dummy_str, IM_ARRAYSIZE(dummy_str), ImGuiInputTextFlags_ReadOnly);
                    if (preset.x < RenderView::AspectRatio_Custom) {
                        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                        snprintf(dummy_str, 512, "%s", RenderView::height_preset_name[preset.y]);
                        ImGui::InputText("Height", dummy_str, IM_ARRAYSIZE(dummy_str), ImGuiInputTextFlags_ReadOnly);
                    } else {
                        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                        snprintf(dummy_str, 512, "%d", custom.x);
                        ImGui::InputText("Width", dummy_str, IM_ARRAYSIZE(dummy_str), ImGuiInputTextFlags_ReadOnly);
                        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                        snprintf(dummy_str, 512, "%d", custom.y);
                        ImGui::InputText("Height", dummy_str, IM_ARRAYSIZE(dummy_str), ImGuiInputTextFlags_ReadOnly);
                    }
                    ImGui::PopStyleColor(1);
                }
                // offer to change filename, ratio and resolution
                else {
                    // combo boxes to select aspect rario
                    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                    if (ImGui::Combo("Ratio", &preset.x, RenderView::ratio_preset_name, IM_ARRAYSIZE(RenderView::ratio_preset_name) ) )  {
                        // change to custom aspect ratio: propose 1:1
                        glm::vec3 res = glm::vec3(custom.y, custom.y, 0.f);
                        // else, change to preset aspect ratio
                        if (preset.x < RenderView::AspectRatio_Custom)
                            res = RenderView::resolutionFromPreset(preset.x, preset.y);
                        // change resolution
                        Mixer::manager().setResolution(res);
                    }
                    //  - preset aspect ratio : propose preset height
                    if (preset.x < RenderView::AspectRatio_Custom) {
                        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                        if (ImGui::Combo("Height", &preset.y, RenderView::height_preset_name, IM_ARRAYSIZE(RenderView::height_preset_name) ) )   {
                            glm::vec3 res = RenderView::resolutionFromPreset(preset.x, preset.y);
                            Mixer::manager().setResolution(res);
                        }
                        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.14f, 0.9f));
                        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                        static char dummy_str[512];
                        snprintf(dummy_str, 512, "%d", custom.x );
                        ImGui::InputText("Width", dummy_str, IM_ARRAYSIZE(dummy_str), ImGuiInputTextFlags_ReadOnly);
                        ImGui::PopStyleColor(1);
                    }
                    //  - custom aspect ratio : input width and height
                    else {
                        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                        ImGui::InputInt("Height", &custom.y, 100, 500);
                        if (ImGui::IsItemDeactivatedAfterEdit())
                            Mixer::manager().setResolution( glm::vec3(custom, 0.f));                        
                        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                        ImGui::InputInt("Width", &custom.x, 100, 500);
                        if (ImGui::IsItemDeactivatedAfterEdit())
                            Mixer::manager().setResolution( glm::vec3(custom, 0.f));
                    }
                }
            }
        }
    }
    else {
        const FrameBuffer *output = Mixer::manager().session()->frame();
        if (output)  {
            ImVec2 pos_tmp = ImGui::GetCursorPos();
            ImVec2 space_size = ImGui::CalcTextSize(" Resolution ", NULL);
            space_size.x += ImGui::GetTextLineHeightWithSpacing() * 2.f;
            space_size.y = -ImGui::GetTextLineHeightWithSpacing() - space;
            ImGui::SetCursorPos( pos_tmp + space_size );
            ImGui::Text("( %d x %d )", output->width(), output->height());
            ImGui::SetCursorPos( pos_tmp );
        }
    }
    //
    // VERSIONS
    //
    Settings::application.pannel_session[1] = ImGui::CollapsingHeader("Versions",
                                                                      Settings::application.pannel_session[1] ? ImGuiTreeNodeFlags_DefaultOpen : 0);
    if (Settings::application.pannel_session[1]){
        static uint64_t _over = 0;
        static bool _tooltip = 0;

        // list snapshots
        std::list<uint64_t> snapshots = Action::manager().snapshots();
        ImVec2 pos_top = ImGui::GetCursorPos();
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if ( ImGui::ListBoxHeader("##Snapshots", snapshots.size(), CLAMP(snapshots.size(), 4, 8)) ) {

            static uint64_t _selected = 0;
            static Thumbnail _snap_thumbnail;
            static std::string _snap_label = "";
            static std::string _snap_date = "";

            int count_over = 0;
            ImVec2 size = ImVec2( ImGui::GetContentRegionAvailWidth(), ImGui::GetTextLineHeight() );
            for (auto snapit = snapshots.rbegin(); snapit != snapshots.rend(); ++snapit)
            {
                // entry
                ImVec2 pos = ImGui::GetCursorPos();

                // context menu icon on currently hovered item
                if ( _over == *snapit ) {
                    // open context menu
                    ImGui::SetCursorPos(ImVec2(size.x-ImGui::GetTextLineHeight()/2.f, pos.y));
                    if ( ImGuiToolkit::IconButton( ICON_FA_CHEVRON_DOWN ) ) {
                        // current list item
                        Action::manager().open(*snapit);
                        // open menu
                        ImGui::OpenPopup( "MenuSnapshot" );
                    }
                    // show tooltip and select on mouse over menu icon
                    if (ImGui::IsItemHovered()) {
                        _selected = *snapit;
                        _tooltip = true;
                    }
                    ImGui::SetCursorPos(pos);
                }

                // snapshot item
                if (ImGui::Selectable( Action::manager().label(*snapit).c_str(), (*snapit == _selected), ImGuiSelectableFlags_AllowDoubleClick, size )) {
                    // shot tooltip on clic
                    _tooltip = true;
                    // trigger snapshot on double clic
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        Action::manager().restore(*snapit);
                }
                // mouse over
                if (ImGui::IsItemHovered()) {
                    _over = *snapit;
                    _selected = 0;
                }

                // if mouse over (only once)
                if (_tooltip && _over > 0 && count_over < 1) {
                    static uint64_t current_over = 0;
                    // load label and thumbnail only if current changed
                    if (current_over != _over) {
                        _snap_label = Action::manager().label(_over);
                        _snap_date  = "Version of " + readable_date_time_string(Action::manager().date(_over));
                        FrameBufferImage *im = Action::manager().thumbnail(_over);
                        if (im) {
                            // set image content to thumbnail display
                            _snap_thumbnail.fill( im );
                            delete im;
                        }
                        else
                            _snap_thumbnail.reset();
                        current_over = _over;
                    }
                    // draw thumbnail in tooltip
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 8.f));
                    ImGui::BeginTooltip();
                    _snap_thumbnail.Render(size.x);
                    ImGui::Text("%s", _snap_date.c_str());
                    ImGui::EndTooltip();
                    ImGui::PopStyleVar();
                    ++count_over; // prevents display twice on item overlap
                }
            }

            // context menu on currently open snapshot
            uint64_t current = Action::manager().currentSnapshot();
            if (ImGui::BeginPopup( "MenuSnapshot" ) && current > 0 )
            {
                _selected = current;
                // snapshot thumbnail
                _snap_thumbnail.Render(size.x);
                // snapshot editable label
                ImGui::SetNextItemWidth(size.x);
                if ( ImGuiToolkit::InputText("##Rename", &_snap_label ) )
                    Action::manager().setLabel( current, _snap_label);
                // snapshot actions
                if (ImGui::Selectable( ICON_FA_ANGLE_DOUBLE_RIGHT "    Restore", false, 0, size ))
                    Action::manager().restore();
                if (ImGui::Selectable( ICON_FA_CODE_BRANCH "-    Remove", false, 0, size ))
                    Action::manager().remove();
                // export option if possible
                std::string filename = Mixer::manager().session()->filename();
                if (filename.size()>0) {
                    if (ImGui::Selectable( ICON_FA_FILE_DOWNLOAD "     Export", false, 0, size )) {
                        Action::manager().saveas(filename);
                    }
                }
                ImGui::EndPopup();
            }
            else
                _selected = 0;

            // end list snapshots
            ImGui::ListBoxFooter();
        }
        // cancel tooltip and mouse over on mouse exit
        if ( !ImGui::IsItemHovered()) {
            _tooltip = false;
            _over = 0;
        }

        // Right panel buton
        pos_bot = ImGui::GetCursorPos();

        // right button
        ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_top.y ));
        if (ImGuiToolkit::IconButton( ICON_FA_CODE_BRANCH " +", "Save & Keep version"))
            UserInterface::manager().saveOrSaveAs(true);
        if (!snapshots.empty()) {
            ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_top.y + ImGui::GetFrameHeight()));
            if (ImGuiToolkit::IconButton( 12, 14, "Clear list"))
                Action::manager().clearSnapshots();
        }

        ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_bot.y - 2.f * ImGui::GetFrameHeightWithSpacing()));
        ImGuiToolkit::HelpToolTip("Previous versions of the session (latest on top). "
                                 "Double-clic on a version to restore it.\n\n"
                                ICON_FA_CODE_BRANCH " With Iterative saving enabled, a new version "
                                "is kept automatically each time the session is saved.");
        // toggle button for versioning
        ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_bot.y - ImGui::GetFrameHeightWithSpacing()) );
        ImGuiToolkit::ButtonToggle(" " ICON_FA_CODE_BRANCH " ", &Settings::application.save_version_snapshot, "Iterative saving");

        ImGui::SetCursorPos( pos_bot );
    }
    else {
        if (!Action::manager().snapshots().empty())  {
            ImVec2 pos_tmp = ImGui::GetCursorPos();
            ImVec2 space_size = ImGui::CalcTextSize(" Versions ", NULL);
            space_size.x += ImGui::GetTextLineHeightWithSpacing() * 2.f;
            space_size.y = -ImGui::GetTextLineHeightWithSpacing() - space;
            ImGui::SetCursorPos( pos_tmp + space_size );
            ImGui::Text("( %zu )", Action::manager().snapshots().size());
            ImGui::SetCursorPos( pos_tmp );
        }
    }
    //
    // UNDO History
    //
    Settings::application.pannel_session[2] = ImGui::CollapsingHeader("Undo history",
                                                                      Settings::application.pannel_session[2] ? ImGuiTreeNodeFlags_DefaultOpen : 0);
    if (Settings::application.pannel_session[2]){

        static uint _over = 0;
        static uint64_t _displayed_over = 0;
        static bool _tooltip = 0;

        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.39f, 0.39f, 0.39f, 0.55f));
        ImVec2 pos_top = ImGui::GetCursorPos();
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if ( ImGui::ListBoxHeader("##UndoHistory", Action::manager().max(), CLAMP(Action::manager().max(), 4, 8)) ) {

            int count_over = 0;
            ImVec2 size = ImVec2( ImGui::GetContentRegionAvailWidth(), ImGui::GetTextLineHeight() );

            for (uint i = Action::manager().max();
                 i >= Action::manager().min(); --i) {

                if (ImGui::Selectable( Action::manager().shortlabel(i).c_str(), i == Action::manager().current(), ImGuiSelectableFlags_AllowDoubleClick, size )) {
                    // go to on double clic
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        Action::manager().stepTo(i);
                    else
                        // show tooltip on clic
                        _tooltip = true;
                }
                // mouse over
                if (ImGui::IsItemHovered())
                    _over = i;

                // if mouse over (only once)
                if (_tooltip && _over > 0 && count_over < 1) {
                    static std::string text = "";
                    static Thumbnail _undo_thumbnail;
                    // load label and thumbnail only if current changed
                    if (_displayed_over != _over) {
                        _displayed_over = _over;
                        text = Action::manager().label(_over);
                        if (text.find_first_of(':') < text.size())
                            text = text.insert( text.find_first_of(':') + 2, 1, '\n');
                        FrameBufferImage *im = Action::manager().thumbnail(_over);
                        if (im) {
                            // set image content to thumbnail display
                            _undo_thumbnail.fill( im );
                            delete im;
                        }
                        else
                            _undo_thumbnail.reset();
                    }
                    // draw thumbnail in tooltip
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 8.f));
                    ImGui::BeginTooltip();
                    _undo_thumbnail.Render(size.x);
                    ImGui::Text("%s", text.c_str());
                    ImGui::EndTooltip();
                    ImGui::PopStyleVar();
                    ++count_over; // prevents display twice on item overlap
                }

            }
            ImGui::ListBoxFooter();
        }
        // cancel tooltip and mouse over on mouse exit
        if ( !ImGui::IsItemHovered()) {
            _tooltip = false;
            _displayed_over = _over = 0;
        }
        ImGui::PopStyleColor(1);

        pos_bot = ImGui::GetCursorPos();

        // right buttons
        ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_top.y ));
        if ( Action::manager().current() > Action::manager().min() ) {
            if ( ImGuiToolkit::IconButton( ICON_FA_UNDO, MENU_UNDO, SHORTCUT_UNDO) )
                Action::manager().undo();
        } else
            ImGui::TextDisabled( ICON_FA_UNDO );
        ImGui::SameLine();
        if ( Action::manager().current() < Action::manager().max() ) {
            if ( ImGuiToolkit::IconButton( ICON_FA_REDO, MENU_REDO, SHORTCUT_REDO ))
                Action::manager().redo();
        } else
            ImGui::TextDisabled( ICON_FA_REDO );

        ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_bot.y - 2.f * ImGui::GetFrameHeightWithSpacing()));
        ImGuiToolkit::HelpToolTip("History of actions (latest on top). "
                                 "Double-clic on an action to restore its status.\n\n"
                                 ICON_FA_MAP_MARKED_ALT " With Show action View enabled, navigate "
                                 "automatically to the view showing the action on undo/redo.");
        // toggle button for shhow in view
        ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_bot.y - ImGui::GetFrameHeightWithSpacing()) );
        ImGuiToolkit::ButtonToggle(ICON_FA_MAP_MARKED_ALT, &Settings::application.action_history_follow_view, "Show action View");
    }

    ImGui::PopStyleColor(1);
}

#define PLAYLIST_FAVORITES ICON_FA_HEART " Favorites"

void Navigator::RenderMainPannelPlaylist()
{
    //
    // SESSION panel
    //
    // currently active playlist and folder
    static std::string playlist_header = PLAYLIST_FAVORITES;
    static Playlist active_playlist;
    static std::list<std::string> folder_session_files;

    // file dialogs to open / save playlist files and folders
    static DialogToolkit::OpenFolderDialog customFolder("Open Folder");
    static DialogToolkit::OpenManyFilesDialog selectSessions("Select vimix sessions",
                                                             VIMIX_FILE_TYPE,
                                                             VIMIX_FILE_PATTERN);

    //    static DialogToolkit::OpenPlaylistDialog openPlaylist("Open Playlist");
    //    static DialogToolkit::SavePlaylistDialog savePlaylist("Save Playlist");

    //    // return from thread for playlist file openning
    //    if (openPlaylist.closed() && !openPlaylist.path().empty()) {
    //        Settings::application.recentPlaylists.push(openPlaylist.path());
    //        Settings::application.recentPlaylists.assign(openPlaylist.path());
    //        Settings::application.pannel_playlist_mode = 1;
    //    }

    //    ImGui::SameLine();
    //    ImGui::SetCursorPosX( pannel_width_ IMGUI_RIGHT_ALIGN);
    //    if ( ImGuiToolkit::IconButton( 16, 3, "Create playlist")) {
    //        savePlaylist.open();
    //    }
    //    if (savePlaylist.closed() && !savePlaylist.path().empty()) {
    //        Settings::application.recentPlaylists.push(savePlaylist.path());
    //        Settings::application.recentPlaylists.assign(savePlaylist.path());
    //        Settings::application.pannel_playlist_mode = 1;
    //    }


    // return from thread for folder openning
    if (customFolder.closed() && !customFolder.path().empty()) {
        Settings::application.recentFolders.push(customFolder.path());
        Settings::application.recentFolders.assign(customFolder.path());
        Settings::application.pannel_playlist_mode = 2;
    }

    // load the list of session in playlist, only once when list changed
    if (Settings::application.recentPlaylists.changed) {
        Settings::application.recentPlaylists.changed = false;
        Settings::application.recentPlaylists.validate();
        // load list
        if ( !Settings::application.recentPlaylists.path.empty())
            active_playlist.load( Settings::application.recentPlaylists.path );
    }

    // get list of vimix files in folder, only once when list changed
    if (Settings::application.recentFolders.changed) {
        Settings::application.recentFolders.changed = false;
        Settings::application.recentFolders.validate();
        // list directory
        if ( !Settings::application.recentFolders.path.empty())
            folder_session_files = SystemToolkit::list_directory( Settings::application.recentFolders.path, { VIMIX_FILE_PATTERN },
                                                       (SystemToolkit::Ordering) Settings::application.recentFolders.ordering);
    }

    //
    // Show combo box of quick selection of recent playlist / directory
    //
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    if (ImGui::BeginCombo("##SelectionPlaylist", playlist_header.c_str(), ImGuiComboFlags_HeightLarge )) {

        // Mode 0 : Favorite playlist
        if (ImGuiToolkit::SelectableIcon( 16, 4, "Favorites", false) ) {
            Settings::application.pannel_playlist_mode = 0;
        }
        // Mode 1 : Playlists
        for(auto playlistname = Settings::application.recentPlaylists.filenames.begin();
            playlistname != Settings::application.recentPlaylists.filenames.end(); playlistname++) {
            if (ImGuiToolkit::SelectableIcon( 12, 3, SystemToolkit::base_filename( *playlistname ).c_str(), false )) {
                // remember which path was selected
                Settings::application.recentPlaylists.assign(*playlistname);
                // set mode
                Settings::application.pannel_playlist_mode = 1;
            }
        }
        // Mode 2 : known folders
        for(auto foldername = Settings::application.recentFolders.filenames.begin();
            foldername != Settings::application.recentFolders.filenames.end(); foldername++) {
            if (ImGuiToolkit::SelectableIcon( 6, 5, BaseToolkit::truncated( *foldername, 40).c_str(), false) ) {
                // remember which path was selected
                Settings::application.recentFolders.assign(*foldername);
                // set mode
                Settings::application.pannel_playlist_mode = 2;
            }
        }
        ImGui::EndCombo();
    }

    //
    // icon to create new playlist
    //
    ImVec2 pos_top = ImGui::GetCursorPos();
    ImVec2 pos_right = ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_top.y - ImGui::GetFrameHeight());
    ImGui::SetCursorPos( pos_right );
    if (ImGuiToolkit::IconButton( 13, 3, "Create playlist")) {
        ImGui::OpenPopup("new_playlist_popup");
    }

    //
    // icon to list directory
    //
    pos_right.x += ImGui::GetTextLineHeightWithSpacing() + IMGUI_SAME_LINE;
    ImGui::SetCursorPos( pos_right );
    if (ImGuiToolkit::IconButton( 5, 5, "List directory")) {
       customFolder.open();
    }

    ImGui::SetCursorPos(pos_top);

    const ImGuiStyle& style = ImGui::GetStyle();
    const ImVec2 list_size = ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN -2.f * style.WindowPadding.x,
                               7.f * (ImGui::GetTextLineHeightWithSpacing() + style.FramePadding.y ) + style.FramePadding.y);
    ImVec2 item_size = ImVec2( list_size.x -2.f * style.FramePadding.x, ImGui::GetTextLineHeightWithSpacing());

    std::string session_hovered_ = "";
    std::string session_triggered_ = "";
    static uint session_tooltip_ = 0;
    ++session_tooltip_;

    //
    // Show session list depending on the mode
    //
    // selection MODE 0 ; FAVORITES
    //
    if ( Settings::application.pannel_playlist_mode == 0) {

        // set header
        playlist_header = PLAYLIST_FAVORITES;

        // how many session files in favorite playlist
        size_t index_max = UserInterface::manager().favorites.size();
        item_size.x -= index_max > 7 ? style.ScrollbarSize : 0.f;

        // display the sessions list and detect if one was selected (double clic)
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if (ImGui::ListBoxHeader("##Favorites", list_size) ) {

            // list session files in favorite playlist
            for (size_t index = 0; index < index_max; ++index) {
                // get name of session file at index
                std::string session_file = UserInterface::manager().favorites.at(index);

                // unique ID for item (filename can be at different index)
                ImGui::PushID( session_file.c_str() );

                // item to select
                ImGui::BeginGroup();
                if (ImGui::Selectable( SystemToolkit::filename(session_file).c_str(), false,
                                       ImGuiSelectableFlags_AllowDoubleClick, item_size )) {
                    // trigger on double clic
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        session_triggered_ = session_file;
                    }
                    // show tooltips on single clic
                    else
                        session_tooltip_ = 100;
                }
                if (ImGui::IsItemActive()) {
                    ImGui::SameLine( item_size.x - 2.f * style.ScrollbarSize );
                    ImGuiToolkit::Icon( 8, 15 );
                }
                ImGui::EndGroup();
                ImGui::PopID();
                // what item is hovered for tooltip
                if (ImGui::IsItemHovered())
                    session_hovered_ = session_file;
                // simple drag to reorder
                else if (ImGui::IsItemActive())
                {
                    size_t index_next = index + (ImGui::GetMouseDragDelta(0).y < -2.f * style.ItemSpacing.y ? -1 : ImGui::GetMouseDragDelta(0).y > 2.f * style.ItemSpacing.y ? 1 : 0);
                    if ( index_next < index_max && index != index_next ) {
                        // reorder in list
                        UserInterface::manager().favorites.move(index, index_next);
                        UserInterface::manager().favorites.save();
                        // cancel tooltip during drag
                        session_tooltip_ = 0;
                        // reset drag
                        ImGui::ResetMouseDragDelta();
                    }
                }
            }

            ImGui::ListBoxFooter();
        }
        // cancel tooltip and mouse over on mouse exit
        if ( !ImGui::IsItemHovered())
            session_tooltip_ = 0;

    }
    //
    // selection MODE 1 : PLAYLISTS
    //
    else if ( Settings::application.pannel_playlist_mode == 1) {

        // set header
        if (Settings::application.recentPlaylists.path.empty())
            Settings::application.pannel_playlist_mode = 0;
        else
            playlist_header = std::string(ICON_FA_STAR) + " " + SystemToolkit::base_filename(Settings::application.recentPlaylists.path);

        // how many session files in favorite playlist
        size_t index_max = active_playlist.size();
        size_t index_to_remove = index_max;
        item_size.x -= ImGui::GetTextLineHeight() + style.ItemSpacing.x ;
        item_size.x -= index_max > 6 ? style.ScrollbarSize : 0.f;

        // display the sessions list and detect if one was selected (double clic)
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if (ImGui::ListBoxHeader("##Playlist", list_size) ) {

            // list session files in favorite playlist
            for (size_t index = 0; index < index_max; ++index) {

                // get name of session file at index
                std::string session_file = active_playlist.at(index);

                // unique ID for item (filename can be at different index)
                ImGui::PushID( session_file.c_str() );

                // item to select
                ImGui::BeginGroup();
                if (ImGui::Selectable( SystemToolkit::filename(session_file).c_str(), false,
                                       ImGuiSelectableFlags_AllowDoubleClick, item_size )) {
                    // trigger on double clic
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        session_triggered_ = session_file;
                    }
                    // show tooltips on single clic
                    else
                        session_tooltip_ = 100;
                }
                ImGui::SameLine();
                if (ImGui::IsItemActive()) {
                    ImGuiToolkit::IconButton( 8, 15 );
                }
                else {
                    if ( ImGuiToolkit::IconButton( 19, 4, "Remove") )
                        index_to_remove = index;
                }
                ImGui::EndGroup();
                ImGui::PopID();

                // what item is hovered for tooltip
                if (ImGui::IsItemHovered())
                    session_hovered_ = session_file;
                // simple drag to reorder
                else if (ImGui::IsItemActive())
                {
                    size_t index_next = index + (ImGui::GetMouseDragDelta(0).y < -2.f * style.ItemSpacing.y ? -1 : ImGui::GetMouseDragDelta(0).y > 2.f * style.ItemSpacing.y ? 1 : 0);
                    if ( index_next < index_max && index != index_next ) {
                        // reorder in list and save new status
                        active_playlist.move(index, index_next);
                        active_playlist.save();
                        // cancel tooltip during drag
                        session_tooltip_ = 0;
                        // reset drag
                        ImGui::ResetMouseDragDelta();
                    }
                }
            }

            ImGui::ListBoxFooter();
        }
        // cancel tooltip and mouse over on mouse exit
        if ( !ImGui::IsItemHovered())
            session_tooltip_ = 0;

        // Remove
        if ( index_to_remove < index_max ) {
            active_playlist.remove( index_to_remove );
            active_playlist.save();
        }

        // Right side of the list : close and save
        ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_top.y));
        if (ImGuiToolkit::IconButton( 14, 3, "Delete playlist")) {
            ImGui::OpenPopup("delete_playlist_popup");
        }
        ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_top.y + 1.5f * ImGui::GetTextLineHeightWithSpacing()));
        if ( ImGuiToolkit::IconButton( 18, 4, "Add sessions")) {
            selectSessions.open();
        }

        // return from thread for sessions multiple selection
        if (selectSessions.closed() && !selectSessions.files().empty()) {
            active_playlist.add(selectSessions.files());
            active_playlist.save();
        }

    }
    //
    // selection MODE 2 : LIST FOLDER
    //
    else if ( Settings::application.pannel_playlist_mode == 2) {

        // set header
        if (Settings::application.recentFolders.path.empty())
            Settings::application.pannel_playlist_mode = 0;
        else
            playlist_header = std::string(ICON_FA_FOLDER) + " " + BaseToolkit::truncated(Settings::application.recentFolders.path, 40);

        // how many listed
        item_size.x -= folder_session_files.size() > 7 ? style.ScrollbarSize : 0.f;

        // display the sessions list and detect if one was selected (double clic)
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if (ImGui::ListBoxHeader("##FolderList", list_size) ) {

            // list session files in folder
            for(auto it = folder_session_files.begin(); it != folder_session_files.end(); ++it) {
                // item to select
                if (ImGui::Selectable( SystemToolkit::filename(*it).c_str(), false,
                                       ImGuiSelectableFlags_AllowDoubleClick, item_size )) {
                    // trigger on double clic
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        session_triggered_ = *it;
                    }
                    // show tooltips on clic
                    else
                        session_tooltip_ = 100;
                }
                if (ImGui::IsItemHovered())
                    session_hovered_ = *it;
            }

            ImGui::ListBoxFooter();
        }
        // cancel tooltip and mouse over on mouse exit
        if ( !ImGui::IsItemHovered())
            session_tooltip_ = 0;

        // Closing and ordering button
        ImGui::PushID("##playlist_directory_actions");
        // close list
        ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_top.y) );
        if (ImGuiToolkit::IconButton( 4, 5, "Close directory")) {
            Settings::application.recentFolders.filenames.remove(Settings::application.recentFolders.path);
            if (Settings::application.recentFolders.filenames.empty())
                Settings::application.pannel_playlist_mode = 0;
            else
                Settings::application.recentFolders.assign( Settings::application.recentFolders.filenames.front() );
        }
        // ordering list
        ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_top.y + ImGui::GetFrameHeightWithSpacing()));
        if ( ImGuiToolkit::IconMultistate(icons_ordering_files, &Settings::application.recentFolders.ordering, tooltips_ordering_files) )
            Settings::application.recentFolders.changed = true;
        ImGui::PopID();

    }

    //
    // Tooltip to show Session thumbnail
    //
    if (session_tooltip_ > 60 && !session_hovered_.empty()) {

        static std::string _current_hovered = "";
        static std::string _file_info = "";
        static Thumbnail _file_thumbnail;
        static bool with_tag_ = false;

        // load info only if changed from the one already displayed
        if (session_hovered_ != _current_hovered) {
            _current_hovered = session_hovered_;
            SessionInformation info = SessionCreator::info(_current_hovered);
            _file_info = info.description;
            if (info.thumbnail) {
                // set image content to thumbnail display
                _file_thumbnail.fill( info.thumbnail );
                with_tag_ = info.user_thumbnail_;
                delete info.thumbnail;
            } else
                _file_thumbnail.reset();
        }

        if ( !_file_info.empty()) {

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 8.f));
            ImGui::BeginTooltip();
            ImVec2 p_ = ImGui::GetCursorScreenPos();
            _file_thumbnail.Render(240);
            ImGui::Text("%s", _file_info.c_str());
            if (with_tag_) {
                ImGui::SetCursorScreenPos(p_ + ImVec2(6, 6));
                ImGui::Text(ICON_FA_TAG);
            }
            ImGui::EndTooltip();
            ImGui::PopStyleVar();
        }
    }

    //
    // Double clic to trigger openning of session
    //
    if (!session_triggered_.empty()) {
        Mixer::manager().open( session_triggered_, Settings::application.smooth_transition );
        if (Settings::application.smooth_transition)
            WorkspaceWindow::clearWorkspace();
    }
    // help indicator
    pos_top.y += list_size.y;
    ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_top.y - 2.f * ImGui::GetFrameHeightWithSpacing()));
    ImGuiToolkit::HelpToolTip("Double-clic on a filename to open the session.\n\n"
                              ICON_FA_ARROW_CIRCLE_RIGHT "  enable Smooth transition "
                                                         "to perform a cross fading with the current session.");

    // toggle button for smooth transition
    ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_top.y - ImGui::GetFrameHeightWithSpacing()) );
    ImGuiToolkit::ButtonToggle(ICON_FA_ARROW_CIRCLE_RIGHT, &Settings::application.smooth_transition, "Smooth transition");

    // transition mode icon if enabled
    if (Settings::application.smooth_transition) {
        const char *tooltip[2] = {"Fade to black", "Cross fading"};
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        if (Mixer::manager().session()->fading() > 0.01)
            ImGuiToolkit::Icon(9, 8, false);
        else
            ImGuiToolkit::IconToggle(9, 8, 0, 8, &Settings::application.transition.cross_fade, tooltip );
    }

    //
    // Popup window to create playlist
    //
    ImGui::SetNextWindowSize(ImVec2(0.8f * pannel_width_, 2.2f*ImGui::GetFrameHeightWithSpacing()), ImGuiCond_Always );
    if (ImGui::BeginPopup("new_playlist_popup", ImGuiWindowFlags_NoMove))
    {
        static bool withcopy = false;
        char text_buf[64] = "";
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if ( ImGui::InputTextWithHint("Name", "[Enter] to validate", text_buf, 64, ImGuiInputTextFlags_EnterReturnsTrue) ) {

            std::string filename = std::string(text_buf);

            if ( !filename.empty() ) {
                filename += "."  VIMIX_PLAYLIST_FILE_EXT;
                filename = SystemToolkit::full_filename( UserInterface::manager().playlists_path, filename);

                // create and fill the playlist
                Playlist tmp;
                if (withcopy) {
                    if (Settings::application.pannel_playlist_mode == 0)
                        tmp = UserInterface::manager().favorites;
                    else if (Settings::application.pannel_playlist_mode == 1)
                        tmp = active_playlist;
                    else if (Settings::application.pannel_playlist_mode == 2)
                        tmp.add(folder_session_files);
                }
                tmp.saveAs( filename );

                // set mode to Playlist mode
                Settings::application.recentPlaylists.push(filename);
                Settings::application.recentPlaylists.assign(filename);
                Settings::application.pannel_playlist_mode = 1;

                ImGui::CloseCurrentPopup();
            }
        }

        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_ITALIC);
        ImGuiToolkit::ButtonSwitch("Duplicate current", &withcopy);
        ImGui::PopFont();

        ImGui::EndPopup();
    }

    //
    // Popup window to delete playlist
    //
    if (ImGui::BeginPopup("delete_playlist_popup", ImGuiWindowFlags_NoMove))
    {
        std::string question = "Yes, delete '";
        question += SystemToolkit::base_filename(Settings::application.recentPlaylists.path) + "' ";
        if ( ImGui::Button( question.c_str() )) {
            // delete the file
            SystemToolkit::remove_file(Settings::application.recentPlaylists.path);

            // remove from the list
            Settings::application.recentPlaylists.filenames.remove(Settings::application.recentPlaylists.path);
            if (Settings::application.recentPlaylists.filenames.empty())
                Settings::application.pannel_playlist_mode = 0;
            else
                Settings::application.recentPlaylists.assign( Settings::application.recentPlaylists.filenames.front() );

            ImGui::CloseCurrentPopup();
        }
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_ITALIC);
        ImGui::Text("This cannot be undone");
        ImGui::PopFont();

        ImGui::EndPopup();
    }

}

void Navigator::RenderMainPannelSettings()
{
    ImGuiContext& g = *GImGui;
    float align_x = g.FontSize + g.Style.FramePadding.x * 3;

    //
    // save settings
    //
    ImVec2 pos = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(pannel_width_ IMGUI_RIGHT_ALIGN, pos.y - 1.6 * ImGui::GetTextLineHeight()));
    if ( ImGuiToolkit::IconButton(ICON_FA_SAVE,"Export settings\nYou can then "
                                  "launch vimix with the option "
                                  "'--settings filename.xml' "
                                  "to restore output windows and configuration.") ){
        // launch file dialog to select file to save settings
        if (UserInterface::manager().settingsexportdialog)
            UserInterface::manager().settingsexportdialog->open();
    }
    ImGui::SetCursorPos(pos);

    //
    // Appearance
    //
    int v = Settings::application.accent_color;
    if (ImGui::RadioButton("##Color", &v, v)){
        Settings::application.accent_color = (v+1)%3;
        ImGuiToolkit::SetAccentColor(static_cast<ImGuiToolkit::accent_color>(Settings::application.accent_color));
        // ask Views to update
        View::need_deep_update_++;
    }
    if (ImGui::IsItemHovered())
        ImGuiToolkit::ToolTip("Change accent color");
    ImGui::SameLine();
    ImGui::SetCursorPosX(align_x);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    if ( ImGui::InputFloat("##Scale", &Settings::application.scale, 0.1f, 0.1f, "%.1f")) {
        Settings::application.scale = CLAMP(Settings::application.scale, 0.5f, 5.f);
        ImGui::GetIO().FontGlobalScale = Settings::application.scale;
    }
    ImGui::SameLine(0, IMGUI_SAME_LINE);
    if (ImGuiToolkit::TextButton("Scale")) {
        Settings::application.scale = 1.f;
        ImGui::GetIO().FontGlobalScale = Settings::application.scale;
    }

    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.f,0.f,0.f,0.f));

    //
    // Recording preferences
    //
    Settings::application.pannel_settings[0] = ImGui::CollapsingHeader("Recording",
                                                                       Settings::application.pannel_settings[0] ? ImGuiTreeNodeFlags_DefaultOpen : 0);

    if (Settings::application.pannel_settings[0]){

        // select Encoder codec
        ImGui::SetCursorPosX(align_x);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ImGui::Combo("##Codec",
                     &Settings::application.record.profile,
                     VideoRecorder::profile_name,
                     IM_ARRAYSIZE(VideoRecorder::profile_name));
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        if (ImGuiToolkit::TextButton("Codec"))
            Settings::application.record.profile = 0;

        // select FPS
        ImGui::SetCursorPosX(align_x);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ImGui::Combo("##Framerate",
                     &Settings::application.record.framerate_mode,
                     VideoRecorder::framerate_preset_name,
                     IM_ARRAYSIZE(VideoRecorder::framerate_preset_name));
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        if (ImGuiToolkit::TextButton("Framerate"))
            Settings::application.record.framerate_mode = 1;

        // compute number of frames in buffer and show warning sign if too low
        const FrameBuffer *output = Mixer::manager().session()->frame();
        if (output) {
            guint64 nb = 0;
            nb = VideoRecorder::buffering_preset_value[Settings::application.record.buffering_mode] / (output->width() * output->height() * 4);
            char buf[512]; snprintf(buf, 512, "Buffer of %s can contain %ld frames (%dx%d), i.e. %.1f sec",
                                    VideoRecorder::buffering_preset_name[Settings::application.record.buffering_mode],
                    (unsigned long)nb, output->width(), output->height(),
                    (float)nb / (float) VideoRecorder::framerate_preset_value[Settings::application.record.framerate_mode] );
            ImGuiToolkit::Indication(buf, 4, 6);
            ImGui::SameLine(0);
        }

        ImGui::SetCursorPosX(align_x);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ImGui::SliderInt("##Buffer", &Settings::application.record.buffering_mode, 0,
                         IM_ARRAYSIZE(VideoRecorder::buffering_preset_name)-1,
                         VideoRecorder::buffering_preset_name[Settings::application.record.buffering_mode]);
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        if (ImGuiToolkit::TextButton("Buffer"))
            Settings::application.record.buffering_mode = 2;

        ImGuiToolkit::Indication("Priority when buffer is full and recorder has to skip frames;\n"
                                 ICON_FA_CARET_RIGHT " Duration: Correct duration, variable framerate.\n"
                                 ICON_FA_CARET_RIGHT " Framerate: Correct framerate, shorter duration.",
                                 ICON_FA_CHECK_DOUBLE);
        ImGui::SameLine(0);
        ImGui::SetCursorPosX(align_x);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        const char *prioritylabel[2] = {"Duration", "Framerate"};
        if (ImGui::BeginCombo("##Priority", prioritylabel[Settings::application.record.priority_mode])) {
            if (ImGui::Selectable(prioritylabel[0], Settings::application.record.priority_mode == 0))
                Settings::application.record.priority_mode = 0;
            if (!Settings::application.accept_audio || Settings::application.record.audio_device.empty()) {
                if (ImGui::Selectable(prioritylabel[1], Settings::application.record.priority_mode == 1))
                    Settings::application.record.priority_mode = 1;
            } else {
                ImGui::Selectable(prioritylabel[1], false, ImGuiSelectableFlags_Disabled);
                if (ImGui::IsItemHovered())
                    ImGuiToolkit::ToolTip("Unable to set priority Framerate when recoding with audio.");
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        if (ImGuiToolkit::TextButton("Priority"))
            Settings::application.record.priority_mode = 0;

        //
        // AUDIO
        //
        if (Settings::application.accept_audio) {

            // Displayed name of current audio device
            std::string current_audio = "None";
            if (!Settings::application.record.audio_device.empty()) {
                if (Audio::manager().exists(Settings::application.record.audio_device))
                    current_audio = Settings::application.record.audio_device;
                else
                    Settings::application.record.audio_device = "";
            }

            // help indication
            ImGuiToolkit::Indication("Select the audio to merge into the recording;\n"
                                     ICON_FA_MICROPHONE_ALT_SLASH " no audio\n "
                                     ICON_FA_MICROPHONE_ALT "  a microphone input\n "
                                     ICON_FA_VOLUME_DOWN "  an audio output",
                                     ICON_FA_MUSIC);
            ImGui::SameLine(0);

            // Combo selector of audio device
            ImGui::SetCursorPosX(align_x);
            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            if (ImGui::BeginCombo("##Audio", current_audio.c_str())) {
                // No audio selection
                if (ImGui::Selectable(ICON_FA_MICROPHONE_ALT_SLASH " None"))
                    Settings::application.record.audio_device = "";
                // list of devices from Audio manager
                for (int d = 0; d < Audio::manager().numDevices(); ++d) {
                    std::string namedev = Audio::manager().name(d);
                    std::string labeldev = (Audio::manager().is_monitor(d) ? ICON_FA_VOLUME_DOWN "  "
                                                                           : ICON_FA_MICROPHONE_ALT "  ")
                            + namedev;
                    if (ImGui::Selectable(labeldev.c_str())) {
                        Settings::application.record.audio_device = namedev;
                        // warning on recording mode
                        if (Settings::application.record.priority_mode > 0) {
                            Log::Notify( "When recording with audio, Priority mode must be set to 'Duration'.");
                            Settings::application.record.priority_mode=0;
                        }
                    }
                }
                ImGui::EndCombo();
            }
            if (!Settings::application.record.audio_device.empty() && ImGui::IsItemHovered())
                ImGuiToolkit::ToolTip(current_audio.c_str());
            ImGui::SameLine(0, IMGUI_SAME_LINE);
            if (ImGuiToolkit::TextButton("Audio"))
                Settings::application.record.audio_device = "";

        }
        ImGuiToolkit::Spacing();
    }
    //
    // Steaming preferences
    //
    Settings::application.pannel_settings[1] = ImGui::CollapsingHeader("Streaming",
                                                                       Settings::application.pannel_settings[1] ? ImGuiTreeNodeFlags_DefaultOpen : 0);

    if (Settings::application.pannel_settings[1]){
        ImGuiToolkit::Indication("Peer-to-peer sharing local network\n\n"
                                 "vimix can stream JPEG (default) or H264 (less bandwidth, higher encoding cost)", ICON_FA_SHARE_ALT_SQUARE);
        ImGui::SameLine(0);
        ImGui::SetCursorPosX(align_x);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ImGui::Combo("##P2P codec", &Settings::application.stream_protocol, "JPEG\0H264\0");
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        if (ImGuiToolkit::TextButton("P2P codec"))
            Settings::application.stream_protocol = 0;

        if (VideoBroadcast::available()) {

            std::ostringstream msg;
            msg << "SRT Broadcast" << std::endl << std::endl;
            msg << "vimix listens to SRT requests on Port " << Settings::application.broadcast_port << std::endl << std::endl;
            msg << "Valid network addresses :" << std::endl;
            for (const auto& ips : NetworkToolkit::host_ips()){
                msg << "srt://" << ips << ":" << Settings::application.broadcast_port << std::endl;
            }
            ImGuiToolkit::Indication(msg.str().c_str(), ICON_FA_GLOBE);
            ImGui::SameLine(0);
            ImGui::SetCursorPosX(align_x);
            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            char bufport[7] = "";
            snprintf(bufport, 7, "%d", Settings::application.broadcast_port);
            ImGui::InputTextWithHint("##SRT Port", "7070", bufport, 6, ImGuiInputTextFlags_CharsDecimal);
            if (ImGui::IsItemDeactivatedAfterEdit()){
                if ( BaseToolkit::is_a_number(bufport, &Settings::application.broadcast_port))
                    Settings::application.broadcast_port = CLAMP(Settings::application.broadcast_port, 1029, 49150);
            }
            ImGui::SameLine(0, IMGUI_SAME_LINE);
            if (ImGuiToolkit::TextButton("SRT Port"))
                Settings::application.broadcast_port = 7070;
        }

        if (ShmdataBroadcast::available()) {
            std::string _shm_socket_file = Settings::application.shm_socket_path;
            if (_shm_socket_file.empty() || !SystemToolkit::file_exists(_shm_socket_file))
                _shm_socket_file = SystemToolkit::home_path();
            _shm_socket_file = SystemToolkit::full_filename(_shm_socket_file, ".shm_vimix" + std::to_string(Settings::application.instance_id));

            char msg[256];
            if (ShmdataBroadcast::available(ShmdataBroadcast::SHM_SHMDATASINK)) {
                ImFormatString(msg, IM_ARRAYSIZE(msg), "Shared Memory\n\n"
                                                       "vimix can share to RAM with "
                                                       "gstreamer default 'shmsink' "
                                                       "and with 'shmdatasink'.\n\n"
                                                       "Socket file to connect to:\n%s\n",
                               _shm_socket_file.c_str());
            }
            else {
                ImFormatString(msg, IM_ARRAYSIZE(msg), "Shared Memory\n\n"
                                                       "vimix can share to RAM with "
                                                       "gstreamer 'shmsink'.\n\n"
                                                       "Socket file to connect to:\n%s\n",
                               _shm_socket_file.c_str());
            }
            ImGuiToolkit::Indication(msg, ICON_FA_MEMORY);
            ImGui::SameLine(0);
            ImGui::SetCursorPosX(align_x);
            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            char bufsocket[128] = "";
            snprintf(bufsocket, 128, "%s", Settings::application.shm_socket_path.c_str());
            ImGui::InputTextWithHint("##SHM path", SystemToolkit::home_path().c_str(), bufsocket, 128);
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                Settings::application.shm_socket_path = bufsocket;
            }
            ImGui::SameLine(0, IMGUI_SAME_LINE);
            if (ImGuiToolkit::TextButton("SHM path"))
                Settings::application.shm_socket_path = "";
            if (ShmdataBroadcast::available(ShmdataBroadcast::SHM_SHMDATASINK)) {
                ImGui::SetCursorPosX(align_x);
                ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                ImGui::Combo("SHM sink", &Settings::application.shm_method, "shmsink\0shmdatasink\0");
            }
        }
        ImGuiToolkit::Spacing();
    }
    //
    // OSC preferences
    //
    Settings::application.pannel_settings[2] = ImGui::CollapsingHeader("Open Sound Control",
                                                                       Settings::application.pannel_settings[2] ? ImGuiTreeNodeFlags_DefaultOpen : 0);

    if (Settings::application.pannel_settings[2]){
        // ImGuiToolkit::Spacing();
        // ImGui::TextDisabled("OSC");

        std::ostringstream msg;
        msg << "vimix accepts OSC messages sent by UDP on Port " << Settings::application.control.osc_port_receive;
        msg << " and replies on Port " << Settings::application.control.osc_port_send << std::endl << std::endl;
        msg << "Valid network addresses:" << std::endl;
        for (const auto& ips : NetworkToolkit::host_ips()){
            msg << "udp://" << ips << ":" << Settings::application.control.osc_port_receive << std::endl;
        }
        ImGuiToolkit::Indication(msg.str().c_str(), ICON_FA_NETWORK_WIRED);
        ImGui::SameLine(0);

        ImGui::SetCursorPosX(align_x);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        char bufreceive[7] = "";
        snprintf(bufreceive, 7, "%d", Settings::application.control.osc_port_receive);
        ImGui::InputTextWithHint("##Port in", "7000", bufreceive, 7, ImGuiInputTextFlags_CharsDecimal);
        if (ImGui::IsItemDeactivatedAfterEdit()){
            if ( BaseToolkit::is_a_number(bufreceive, &Settings::application.control.osc_port_receive)){
                Settings::application.control.osc_port_receive = CLAMP(Settings::application.control.osc_port_receive, 1029, 49150);
                Control::manager().init();
            }
        }
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        if (ImGuiToolkit::TextButton("Port in"))
            Settings::application.control.osc_port_receive = OSC_PORT_RECV_DEFAULT;

        ImGui::SetCursorPosX(align_x);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        char bufsend[7] = "";
        snprintf(bufsend, 7, "%d", Settings::application.control.osc_port_send);
        ImGui::InputTextWithHint("##Port out", "7001", bufsend, 7, ImGuiInputTextFlags_CharsDecimal);
        if (ImGui::IsItemDeactivatedAfterEdit()){
            if ( BaseToolkit::is_a_number(bufsend, &Settings::application.control.osc_port_send)){
                Settings::application.control.osc_port_send = CLAMP(Settings::application.control.osc_port_send, 1029, 49150);
                Control::manager().init();
            }
        }
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        if (ImGuiToolkit::TextButton("Port out"))
            Settings::application.control.osc_port_send = OSC_PORT_SEND_DEFAULT;

        ImGui::SetCursorPosX(align_x);
        const float w = IMGUI_RIGHT_ALIGN - ImGui::GetFrameHeightWithSpacing();
        ImGuiToolkit::ButtonOpenUrl( "Edit", Settings::application.control.osc_filename.c_str(), ImVec2(w, 0) );
        ImGui::SameLine(0, 6);
        if ( ImGuiToolkit::IconButton(15, 12, "Reload") )
            Control::manager().init();
        ImGui::SameLine();
        ImGui::Text("Translator");
        ImGuiToolkit::Spacing();
    }

    //
    // Gamepad preferences
    //
    Settings::application.pannel_settings[3] = ImGui::CollapsingHeader("Gamepad Input",
                                                                       Settings::application.pannel_settings[3] ? ImGuiTreeNodeFlags_DefaultOpen : 0);

    if (Settings::application.pannel_settings[3]){

        // Gamepad Device selection
        char text_buf[512];
        if ( glfwJoystickPresent( Settings::application.gamepad_id ) == GLFW_TRUE &&
                glfwJoystickIsGamepad(Settings::application.gamepad_id) == GLFW_TRUE )
            ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), "%s", glfwGetJoystickName(Settings::application.gamepad_id));
        else
            ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), "None recognized");

        ImGui::SetCursorPosX(align_x);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if (ImGui::BeginCombo("Device", text_buf, ImGuiComboFlags_None)) {
            for( int g = GLFW_JOYSTICK_1; g < GLFW_JOYSTICK_LAST; ++g) {
                if ( glfwJoystickPresent( g ) == GLFW_TRUE &&
                        glfwJoystickIsGamepad(g) == GLFW_TRUE ) {
                    ImFormatString(text_buf, IM_ARRAYSIZE(text_buf), "%s", glfwGetJoystickName(g));
                    if (ImGui::Selectable(text_buf, Settings::application.gamepad_id == g) ) {
                        Settings::application.gamepad_id = g;
                    }
                }
            }
            ImGui::EndCombo();
        }
        ImGui::Spacing();

        // Custom mapping file selection
        static DialogToolkit::OpenFileDialog gamepadmappingdialog("Select Gamepad Mapping File",
                                                                   "Gamepad Mapping",
                                                                   {"gamecontrollerdb.txt", "*.txt"});

        ImGuiToolkit::Indication("SDL gamepad mapping database.\n\n"
                                 "Get one from: github.com/gabomdq/SDL_GameControllerDB\n"
                                 "Or use SDL2 Gamepad Tool to create custom mappings:\n"
                                 "generalarcade.com/gamepadtool", ICON_FA_GAMEPAD);
        ImGui::SameLine(0, IMGUI_SAME_LINE);

        // File path input for mapping file
        ImGui::SetCursorPosX(align_x);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        char bufgpfilename[512] = "";
        snprintf(bufgpfilename, 512, "%s", Settings::application.gamepad_mapping_filename.c_str());
        // Change text color if file does not exist
        if (!Settings::application.gamepad_mapping_filename.empty()) {
            std::string expanded_path = Settings::application.gamepad_mapping_filename;
            if (!expanded_path.empty() && expanded_path[0] == '~') 
                expanded_path = SystemToolkit::home_path() + expanded_path.substr(1);
            if (SystemToolkit::file_exists(expanded_path)) 
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_Text));
            else 
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_FAILED, 1.));
        }
        else
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_Text));
        // text entry for filename
        ImGui::InputTextWithHint("##GamepadMappingPath", "~/gamecontrollerdb.txt", bufgpfilename, 512);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            Settings::application.gamepad_mapping_filename = bufgpfilename;
            Control::manager().loadGamepadMappings();
        }
        ImGui::PopStyleColor();
        // label and reset button
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        if (ImGuiToolkit::TextButton("Database")){
            Settings::application.gamepad_mapping_filename = "";
            Control::manager().loadGamepadMappings();
        }
        // File dialog to browse for mapping file
        ImGui::SetCursorPosX(align_x);
        const float w = IMGUI_RIGHT_ALIGN - ImGui::GetFrameHeightWithSpacing();
        if (ImGui::Button(ICON_FA_FOLDER_OPEN " Browse", ImVec2(w, 0))) {
            gamepadmappingdialog.open();
        }
        ImGui::SameLine(0, 6);
        if ( ImGuiToolkit::IconButton(15, 12, "Reload") )
            Control::manager().loadGamepadMappings();
        ImGui::SameLine();
        if ( ImGuiToolkit::IconButton(ICON_FA_EXTERNAL_LINK_ALT, "Search online") )
            SystemToolkit::open("https://github.com/mdqinc/SDL_GameControllerDB");

        // Handle file dialog
        if (gamepadmappingdialog.closed()) {
            std::string selected_path = gamepadmappingdialog.path();
            if (!selected_path.empty()) {
                Settings::application.gamepad_mapping_filename = selected_path;
                Control::manager().loadGamepadMappings();
            }
        }
        ImGuiToolkit::Spacing();
    }
    //
    // System preferences
    //
    Settings::application.pannel_settings[4] = ImGui::CollapsingHeader("System",
                                                                       Settings::application.pannel_settings[4] ? ImGuiTreeNodeFlags_DefaultOpen : 0);

    if (Settings::application.pannel_settings[4]){
        static bool need_restart = false;
        static bool vsync = (Settings::application.render.vsync > 0);
        static bool multi = (Settings::application.render.multisampling > 0);
        static bool gpu = Settings::application.render.gpu_decoding;
        static bool glmemory = Settings::application.render.gst_glmemory_texturing;
        static bool audio = Settings::application.accept_audio;
        bool change = false;
        // hardware support deserves more explanation
        ImGuiToolkit::Indication("If enabled, tries to find a platform adapted hardware-accelerated "
                                 "driver to decode (read) or encode (record) videos.", gpu ? 13 : 14, 2);
        ImGui::SameLine(0);
        if (Settings::application.render.gpu_decoding_available)
            change |= ImGuiToolkit::ButtonSwitch( "Hardware en/decoding", &gpu);
        else
            ImGui::TextDisabled("Hardware en/decoding unavailable");

        // audio support deserves more explanation
        ImGuiToolkit::Indication("If enabled, tries to find audio in openned videos "
                                 "and allows recording audio.", audio ? ICON_FA_VOLUME_UP : ICON_FA_VOLUME_MUTE);
        ImGui::SameLine(0);
        change |= ImGuiToolkit::ButtonSwitch( "Audio (experimental)", &audio);

#ifndef NDEBUG

#ifdef USE_GST_OPENGL_SYNC_HANDLER
        change |= ImGuiToolkit::ButtonSwitch( "Gst-GLMemory texturing", &glmemory);
#endif
        change |= ImGuiToolkit::ButtonSwitch( "Vertical synchronization", &vsync);
        change |= ImGuiToolkit::ButtonSwitch( "Multisample antialiasing", &multi);
#endif
        if (change) {
            need_restart = ( vsync != (Settings::application.render.vsync > 0) ||
                    multi != (Settings::application.render.multisampling > 0) ||
                    gpu != Settings::application.render.gpu_decoding ||
                    glmemory != Settings::application.render.gst_glmemory_texturing ||
                    audio != Settings::application.accept_audio );
        }

        if (need_restart) {
            ImGuiToolkit::Spacing();
            if (ImGui::Button( ICON_FA_POWER_OFF "  Quit & restart to apply", ImVec2(ImGui::GetContentRegionAvail().x - 50, 0))) {
                Settings::application.render.vsync = vsync ? 1 : 0;
                Settings::application.render.multisampling = multi ? 3 : 0;
                Settings::application.render.gst_glmemory_texturing = glmemory;
                Settings::application.render.gpu_decoding = gpu;
                Settings::application.accept_audio = audio;
                if (UserInterface::manager().TryClose())
                    Rendering::manager().close();
            }
        }
    }

    ImGui::PopStyleColor(1);
}

void Navigator::RenderTransitionPannel(const ImVec2 &iconsize)
{
    if (Settings::application.current_view != View::TRANSITION) {
        discardPannel();
        return;
    }

    // Next window is a side pannel
    ImGui::SetNextWindowPos( ImVec2(width_, 0), ImGuiCond_Always );
    ImGui::SetNextWindowSize( ImVec2(pannel_width_, height_), ImGuiCond_Always );
    ImGui::SetNextWindowBgAlpha( pannel_alpha_ ); // Transparent background
    if (ImGui::Begin("##navigatorTrans", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |  ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
    {
        // TITLE
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ImGui::SetCursorPosY(0.5f * (iconsize.y - ImGui::GetTextLineHeight()));
        ImGui::Text("Transition");
        ImGui::PopFont();

        // Transition options
        ImGuiToolkit::Spacing();
        ImGui::Text("Parameters");

        static std::vector< std::tuple<int, int, std::string> > profile_fading = {
            {0, 8, "Cross fading"},
            {9, 8, "Fade to black"}
        };
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        int tmp = Settings::application.transition.cross_fade ? 0 : 1;
        if (ImGuiToolkit::ComboIcon("##Fading", &tmp, profile_fading))
            Settings::application.transition.cross_fade = tmp < 1;
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        if (ImGuiToolkit::TextButton("Fading "))
            Settings::application.transition.cross_fade = true;

        static std::vector< std::tuple<int, int, std::string> > profile_options = {
            {11, 12, "Linear"},
            {10, 12, "Quadratic"}
        };
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        tmp = Settings::application.transition.profile ? 1 : 0;
        if (ImGuiToolkit::ComboIcon("##Curve", &tmp, profile_options))
            Settings::application.transition.profile = tmp > 0;
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        if (ImGuiToolkit::TextButton("Curve "))
            Settings::application.transition.profile = false;

        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ImGui::SliderFloat("##Duration",
                           &Settings::application.transition.duration,
                           TRANSITION_MIN_DURATION,
                           TRANSITION_MAX_DURATION,
                           "%.1f s");
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        if (ImGuiToolkit::TextButton("Duration "))
            Settings::application.transition.duration = 1.f;

        // transition actions
        ImGuiToolkit::Spacing();
        ImGui::Text("Actions");
        if ( ImGui::Button( ICON_FA_PLAY "  Play & Open", ImVec2(IMGUI_RIGHT_ALIGN, 0)) ){
            TransitionView *tv = static_cast<TransitionView *>(Mixer::manager().view(View::TRANSITION));
            if (tv) tv->play(true);
        }
        if ( ImGui::Button( ICON_FA_FAST_FORWARD "  Fast Open", ImVec2(IMGUI_RIGHT_ALIGN, 0)) ){
            TransitionView *tv = static_cast<TransitionView *>(Mixer::manager().view(View::TRANSITION));
            if (tv) tv->open();
        }
        if ( ImGui::Button( ICON_FA_TIMES "  Cancel ", ImVec2(IMGUI_RIGHT_ALIGN, 0)) ){
            TransitionView *tv = static_cast<TransitionView *>(Mixer::manager().view(View::TRANSITION));
            if (tv) tv->cancel();
        }

        // Exit transition
        ImGui::Text(" ");
        if ( ImGui::Button( ICON_FA_DOOR_OPEN " Exit", ImVec2(ImGui::GetContentRegionAvail().x, 0)) )
            UserInterface::manager().setView(View::MIXING);

        ImGui::End();
    }
}

void Navigator::RenderMainPannel(const ImVec2 &iconsize)
{
    const ImGuiStyle& style = ImGui::GetStyle();

    if (Settings::application.current_view == View::TRANSITION)
        return;

    // Next window is a side pannel
    ImGui::SetNextWindowPos( ImVec2(width_, 0), ImGuiCond_Always );
    ImGui::SetNextWindowSize( ImVec2(pannel_width_, height_), ImGuiCond_Always );
    ImGui::SetNextWindowBgAlpha( pannel_alpha_ ); // Transparent background
    if (ImGui::Begin("##navigatorMain", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |  ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
    {
        // Temporary fix for preventing horizontal scrolling (https://github.com/ocornut/imgui/issues/2915)
        ImGui::SetScrollX(0);

        //
        // Panel Mode selector
        //
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
        ImGui::Columns(5, NULL, false);
        bool selected_panel_mode[5] = {0};
        selected_panel_mode[pannel_main_mode_] = true;
        if (ImGuiToolkit::SelectableIcon( 7, 1, "##SESSION_FILE", selected_panel_mode[0], iconsize))
            Settings::application.pannel_main_mode = pannel_main_mode_ = 0;
        ImGui::NextColumn();
        if (ImGuiToolkit::SelectableIcon( 4, 8, "##SESSION_PLAYLIST", selected_panel_mode[1], iconsize))
            Settings::application.pannel_main_mode = pannel_main_mode_ = 1;
        ImGui::NextColumn();
        if (ImGuiToolkit::SelectableIcon( 13, 5, "##SETTINGS", selected_panel_mode[2], iconsize))
            pannel_main_mode_ = 2;
        ImGui::Columns(1);
        ImGui::PopStyleVar();
        ImGui::PopFont();

        //
        // Panel Menu
        //
        ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, IMGUI_TOP_ALIGN) );
        if (ImGui::BeginMenu("File")) {
            UserInterface::manager().showMenuFile();
            ImGui::EndMenu();
        }
        ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, IMGUI_TOP_ALIGN + ImGui::GetTextLineHeightWithSpacing()) );
        if (ImGui::BeginMenu("Edit")) {
            UserInterface::manager().showMenuEdit();
            ImGui::EndMenu();
        }
        ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, IMGUI_TOP_ALIGN + 2.f * ImGui::GetTextLineHeightWithSpacing()) );
        if (ImGui::BeginMenu("Tools")) {
            UserInterface::manager().showMenuWindows();
            ImGui::EndMenu();
        }

        //
        // Panel content
        //
        float __p = width_ + style.ItemSpacing.y + ImGui::GetTextLineHeightWithSpacing();
        ImGui::SetCursorPosY(__p);
        if (pannel_main_mode_ == 0) {
            ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
            ImGui::Text("Session");
            ImGui::SetCursorPosY(__p + ImGui::GetFrameHeightWithSpacing());
            ImGui::PopFont();
            RenderMainPannelSession();
        }
        else if (pannel_main_mode_ == 1) {
            ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
            ImGui::Text("Playlist");
            ImGui::SetCursorPosY(__p + ImGui::GetFrameHeightWithSpacing());
            ImGui::PopFont();
            RenderMainPannelPlaylist();
        }
        else {
            ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
            ImGui::Text("Settings");
            ImGui::SetCursorPosY(__p + ImGui::GetFrameHeightWithSpacing());
            ImGui::PopFont();
            RenderMainPannelSettings();
        }

        //
        // About vimix
        //
        ImGuiContext& g = *GImGui;
        const ImVec2 rightcorner(pannel_width_ + width_, height_);
        const float remaining_height = height_ - ImGui::GetCursorPosY();
        const float button_height = g.FontSize + g.Style.FramePadding.y * 2.0f + g.Style.ItemSpacing.y;
        const float icon_height = 128;
        // About vimix button (if enough room)
        if (remaining_height > button_height + g.Style.ItemSpacing.y)  {
            int index_label = 0;
            const char *button_label[2] = {ICON_FA_CROW " About vimix", "About vimix"};
            // Logo (if enougth room)
            if (remaining_height > icon_height + button_height + g.Style.ItemSpacing.y)  {
                static unsigned int vimixicon = Resource::getTextureImage("images/vimix_256x256.png");
                const ImVec2 draw_pos = rightcorner
                                        - ImVec2((icon_height + pannel_width_) * 0.5f,
                                                 icon_height + button_height + g.Style.ItemSpacing.y);
                ImGui::SetCursorScreenPos(draw_pos);
                ImGui::Image((void *) (intptr_t) vimixicon, ImVec2(icon_height, icon_height));
                // Hidden action: add a source with vimix logo if double clic on vimix logo
                const ImRect bb(draw_pos, draw_pos + ImVec2(icon_height, icon_height));
                const ImGuiID id = ImGui::GetCurrentWindow()->GetID("##easteregg");
                bool hovered, held;
                if (ImGui::ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_PressedOnDoubleClick) )
                    Mixer::manager().paste( Resource::getText("images/logo.vmx") );

                index_label = 1;
            }
            // Button About
            ImGui::SetCursorScreenPos( rightcorner - ImVec2(pannel_width_ * 0.75f, button_height) );
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4());
            if ( ImGui::Button( button_label[index_label], ImVec2(pannel_width_ * 0.5f, 0)) ) {
                UserInterface::manager().show_vimix_about = true;
                WorkspaceWindow::restoreWorkspace(true);
            }
            ImGui::PopStyleColor();
        }

        ImGui::End();
    }
}

///
/// SOURCE PREVIEW
///

SourcePreview::SourcePreview() : source_(nullptr), label_(""), reset_(0)
{

}

void SourcePreview::setSource(Source *s, const std::string &label)
{
    if(source_)
        delete source_;

    source_ = s;
    label_ = label;
    reset_ = true;
}

Source * SourcePreview::getSource()
{
    Source *s = source_;
    source_ = nullptr;
    return s;
}

void SourcePreview::Render(float width)
{
    if(source_) {
        // cancel if failed
        if (source_->failed()) {
            // remove from list of recent import files if relevant
            MediaSource *failedFile = dynamic_cast<MediaSource *>(source_);
            if (failedFile != nullptr) {
                Settings::application.recentImport.remove( failedFile->path() );
            }
            setSource();
        }
        else
        {
            // render framebuffer
            if ( reset_  && source_->ready() ) {
                // trick to ensure a minimum of 2 frames are rendered actively
                source_->setActive(true);
                source_->update( Mixer::manager().dt() );
                source_->render();
                source_->setActive(false);
                reset_ = false;
            }
            else {
                // update source
                source_->update( Mixer::manager().dt() );
                source_->render();
            }

            // draw preview
            FrameBuffer *frame = source_->frame();
            ImVec2 preview_size(width, width / frame->aspectRatio());
            ImGui::Image((void*)(uintptr_t) frame->texture(), preview_size);
            bool mouseover = ImGui::IsItemHovered();
            if (mouseover) {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(label_.c_str());
                ImGui::EndTooltip();
            }
            // if the source is playable and once its ready
            if (source_->playable() && source_->ready()) {
                // activate the source on mouse over
                if (source_->active() != mouseover)
                    source_->setActive(mouseover);
                // show icon '>' to indicate if we can activate it
                if (!mouseover) {
                    ImVec2 pos = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(pos + preview_size * ImVec2(0.5f, -0.6f));
                    ImGuiToolkit::Icon(12,7);
                    ImGui::SetCursorPos(pos);
                }
            }
            // information text
            ImGuiToolkit::Icon(source_->icon().x, source_->icon().y);
            ImGui::SameLine(0, IMGUI_SAME_LINE);
            ImGui::Text("%s", source_->info().c_str());
            if (source_->ready()) {
                static InfoVisitor _info;
                source_->accept(_info);
                ImGui::Text("%s", _info.str().c_str());
            }
            else
                ImGui::Text("loading...");
        }
    }
}

bool SourcePreview::ready() const
{
    return source_ != nullptr && source_->ready();
}

///
/// THUMBNAIL
///

Thumbnail::Thumbnail() : aspect_ratio_(-1.f), texture_(0)
{
}

Thumbnail::~Thumbnail()
{
    if (texture_)
        glDeleteTextures(1, &texture_);
}

bool Thumbnail::filled()
{
    return aspect_ratio_ > 0.f;
}

void Thumbnail::reset()
{
    aspect_ratio_ = -1.f;
}

void Thumbnail::fill(const FrameBufferImage *image)
{
    if (!texture_) {
        glGenTextures(1, &texture_);
        glBindTexture( GL_TEXTURE_2D, texture_);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB8, SESSION_THUMBNAIL_HEIGHT * 3, SESSION_THUMBNAIL_HEIGHT);
    }

    aspect_ratio_ = static_cast<float>(image->width) / static_cast<float>(image->height);
    glBindTexture( GL_TEXTURE_2D, texture_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, image->width, image->height, GL_RGB, GL_UNSIGNED_BYTE, image->rgb);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void Thumbnail::Render(float width)
{
    if (filled())
        ImGui::Image((void*)(intptr_t)texture_, ImVec2(width, width/aspect_ratio_), ImVec2(0,0), ImVec2(0.333f*aspect_ratio_, 1.f));
}

