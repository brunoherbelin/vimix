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

#include <cstddef>
#include <string>
#include <utility>

#include "NavigatorInternal.h"

#include "IconsFontAwesome5.h"
#include "defines.h"
#include "IconsVimixImage.h"
#include "Settings.h"
#include "Toolkit/ImGuiToolkit.h"
#include "View/View.h"
#include "View/RenderView.h"
#include "Resource.h"
#include "Mixer.h"
#include "MediaPlayer.h"
#include "Source/PatternSource.h"
#include "Source/SourceCallback.h"
#include "RenderingManager.h"
#include "ControlManager.h"
#include "MousePointer.h"
#include "SessionCreator.h"
#include "Window/WorkspaceWindow.h"
#include "UserInterfaceManager.h"
#include "FrameGrabbing.h"

#include "NewSourcePanel.h"
#include "PlaylistPanel.h"
#include "SourcePanel.h"
#include "SessionPanel.h"
#include "SettingsPanel.h"
#include "Navigator.h"



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
        new_source_panel_.setNewMedia(MEDIA_RECENT);
    else if (Settings::application.recentImportFolders.path.compare(IMGUI_LABEL_RECENT_RECORDS) == 0)
        new_source_panel_.setNewMedia(MEDIA_RECORDING);
    else
        new_source_panel_.setNewMedia(MEDIA_FOLDER, Settings::application.recentImportFolders.path);

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

void Navigator::clearButtonSelection()
{
    // clear all buttons
    for(int i=0; i<NAV_COUNT; ++i)
        selected_button[i] = false;

    // clear new source pannel
    new_source_panel_.clearNewPannel();
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
    new_source_panel_.mediaModeChanged();

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
            new_source_panel_.clearNewPannel();
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
            if (ImGuiToolkit::SelectableIcon(ICON_VI_VIMIX_LOGO, "", selected_button[NAV_MENU], iconsize)) {
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

        if (ImGuiToolkit::SelectableIcon(ICON_VI_LAYERS, "", selected_view[View::LAYER], iconsize))
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

        std::pair<int, int> icon_index = Settings::application.render.disabled ? std::make_pair(ICON_VI_DISPLAYS_DISABLED) : std::make_pair(ICON_VI_DISPLAYS);
        if (ImGuiToolkit::SelectableIcon(icon_index.first, icon_index.second, "", selected_view[View::DISPLAYS], iconsize))
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
            if (_s != nullptr && _s->failed() == Source::FAIL_NONE) {
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

        // give the geometry of this frame to the pannels
        session_panel_.setGeometry(width_, pannel_width_, height_, padding_width_, pannel_alpha_);
        playlist_panel_.setGeometry(width_, pannel_width_, height_, padding_width_, pannel_alpha_);
        source_panel_.setGeometry(width_, pannel_width_, height_, padding_width_, pannel_alpha_);
        new_source_panel_.setGeometry(width_, pannel_width_, height_, padding_width_, pannel_alpha_);
        settings_panel_.setGeometry(width_, pannel_width_, height_, padding_width_, pannel_alpha_);

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
            new_source_panel_.Render(this, iconsize);
            reset_visitor = true;
        }
        // pannel to configure a selected source
        else
        {
            if ( selected_index < 0 ) {
                showPannelSource(NAV_MENU);
            }
            else {
                // rarely its not the current source that is selected
                if ( selected_index != Mixer::manager().indexCurrentSource())
                    showPannelSource( Mixer::manager().indexCurrentSource() );
                // render current sources
                source_panel_.Render(this, Mixer::manager().currentSource(), iconsize, reset_visitor);
                reset_visitor = false;
            }
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
        if (ImGuiToolkit::IconButton(ICON_VI_ZOOM_RESET)) {
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
            if ( ImGuiToolkit::IconToggle(ICON_VI_GRID_SQUARE, ICON_VI_GRID_ASPECT_RATIO, &Settings::application.proportional_grid, tooltip_lock) )
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
            {ICON_VI_TRANSITION_CROSS_FADE, "Cross fading"},
            {ICON_VI_TRANSITION_FADE_BLACK, "Fade to black"}
        };
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        int tmp = Settings::application.transition.cross_fade ? 0 : 1;
        if (ImGuiToolkit::ComboIcon("##Fading", &tmp, profile_fading))
            Settings::application.transition.cross_fade = tmp < 1;
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        if (ImGuiToolkit::TextButton("Fading "))
            Settings::application.transition.cross_fade = true;

        static std::vector< std::tuple<int, int, std::string> > profile_options = {
            {ICON_VI_TRANSITION_LINEAR, "Linear"},
            {ICON_VI_TRANSITION_QUADRATIC, "Quadratic"}
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
        if (ImGuiToolkit::SelectableIcon( ICON_VI_PANNEL_SESSION, "##SESSION_FILE", selected_panel_mode[0], iconsize))
            Settings::application.pannel_main_mode = pannel_main_mode_ = 0;
        ImGui::NextColumn();
        if (ImGuiToolkit::SelectableIcon( ICON_VI_PANNEL_PLAYLIST, "##SESSION_PLAYLIST", selected_panel_mode[1], iconsize))
            Settings::application.pannel_main_mode = pannel_main_mode_ = 1;
        ImGui::NextColumn();
        if (ImGuiToolkit::SelectableIcon( ICON_VI_PANNEL_SETTINGS, "##SETTINGS", selected_panel_mode[2], iconsize))
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
            session_panel_.Render();
        }
        else if (pannel_main_mode_ == 1) {
            ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
            ImGui::Text("Playlist");
            ImGui::SetCursorPosY(__p + ImGui::GetFrameHeightWithSpacing());
            ImGui::PopFont();
            playlist_panel_.Render();
        }
        else {
            ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
            ImGui::Text("Settings");
            ImGui::SetCursorPosY(__p + ImGui::GetFrameHeightWithSpacing());
            ImGui::PopFont();
            settings_panel_.Render();
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


void Navigator::setNewMedia(MediaCreateMode mode, std::string path)
{
    new_source_panel_.setNewMedia(mode, path);
}
