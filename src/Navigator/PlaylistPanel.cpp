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
#include <map>
#include <set>
#include <chrono>

#include "NavigatorInternal.h"

#include "IconsFontAwesome5.h"
#include "defines.h"
#include "IconsVimixImage.h"
#include "Settings.h"
#include "Log.h"
#include "Mixer.h"
#include "Exporter.h"
#include "Playlist.h"
#include "SessionCreator.h"
#include "Toolkit/BaseToolkit.h"
#include "Toolkit/SystemToolkit.h"
#include "Toolkit/DialogToolkit.h"
#include "Toolkit/ImGuiToolkit.h"
#include "Window/WorkspaceWindow.h"
#include "UserInterfaceManager.h"

#include "NavigatorWidgets.h"
#include "PlaylistPanel.h"

#define PLAYLIST_FAVORITES ICON_FA_HEART " Favorites"

void PlaylistPanel::Render()
{
    const ImGuiStyle& style = ImGui::GetStyle();
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
    static DialogToolkit::OpenFolderDialog exportFolder("Export to Folder");

    // actions on playlist
    static uint counter_menu_timeout = 0;
    static Playlist playlist_edit;
    static std::string playlist_edit_name;
    static int playlist_edit_action = 0; // 0 = none, 1 = save, 2 = rename, 3 = delete
    static std::string playlist_name_str;

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
    //    if ( ImGuiToolkit::IconButton( ICON_VI_PLAYLIST_CREATE, "Create playlist")) {
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

    // return from thread for export folder selection
    if (exportFolder.closed() && !exportFolder.path().empty()){
        Settings::application.recentExportFolder.path = exportFolder.path();
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

    // Make sure to reset the edit action if the user clicks outside the playlist panel
    if ( !ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) ) {
        playlist_name_str.clear();
        playlist_edit_action = 0;
    }

    //
    // Show combo box of quick selection of recent playlist / directory
    //
    if (playlist_edit_action == 0) {

        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if (ImGui::BeginCombo("##SelectionPlaylist", playlist_header.c_str(), ImGuiComboFlags_HeightLarge )) {

            // Mode 0 : Favorite playlist
            if (ImGuiToolkit::SelectableIcon( ICON_VI_FAVORITES, "Favorites", false) ) {
                Settings::application.pannel_playlist_mode = 0;
            }
            // Mode 1 : Playlists
            for(auto playlistname = Settings::application.recentPlaylists.filenames.begin();
                playlistname != Settings::application.recentPlaylists.filenames.end(); playlistname++) {
                if (ImGuiToolkit::SelectableIcon( ICON_VI_PLAYLIST, SystemToolkit::base_filename( *playlistname ).c_str(), false )) {
                    // remember which path was selected
                    Settings::application.recentPlaylists.assign(*playlistname);
                    // set mode
                    Settings::application.pannel_playlist_mode = 1;
                }
            }
            // Mode 2 : known folders
            for(auto foldername = Settings::application.recentFolders.filenames.begin();
                foldername != Settings::application.recentFolders.filenames.end(); foldername++) {
                if (ImGuiToolkit::SelectableIcon( ICON_VI_FOLDER, BaseToolkit::truncated( *foldername, 40).c_str(), false) ) {
                    // remember which path was selected
                    Settings::application.recentFolders.assign(*foldername);
                    // set mode
                    Settings::application.pannel_playlist_mode = 2;
                }
            }

            // NEW playlist or directory
            ImGui::Separator();
            if (ImGuiToolkit::SelectableIcon( ICON_VI_PLAYLIST_NEW, "New Playlist", false )) {
                // create empty playlist and default name to start rename
                playlist_edit_name = "MyPlaylist";
                playlist_edit.clear();
                playlist_edit_action = 1;
                // set empty current playlist
                active_playlist.clear();
                Settings::application.pannel_playlist_mode = 1;
            }
            if (ImGuiToolkit::SelectableIcon( ICON_VI_FOLDER_ADD, "Add Directory list", false )) {
                customFolder.open();
            }

            ImGui::EndCombo();
        }

        //
        // icon menu playlist
        //
        ImGui::SameLine(0, IMGUI_SAME_LINE );      
        if (ImGuiToolkit::IconButton(ICON_VI_MENU_OPTIONS) || ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup)) {
            counter_menu_timeout=0;
            ImGui::OpenPopup( "menu_playlist_popup" );
        }
    }
    //
    // Show string edit to set the name and save a playlist
    //
    else if (playlist_edit_action < 3) {

        // initial name of playlist to edit
        if (playlist_name_str.empty()) 
            playlist_name_str = playlist_edit_name;

        // If filename exists show name in red (should not duplicate with same name)
        std::string filename = SystemToolkit::full_filename( UserInterface::manager().playlists_path, playlist_name_str + "."  VIMIX_PLAYLIST_FILE_EXT);
        if (SystemToolkit::file_exists(filename)) 
            ImGui::PushStyleColor(ImGuiCol_FrameBg, (ImVec4)ImColor::HSV(0.0f, 0.6f, 0.4f));
        else 
            ImGui::PushStyleColor(ImGuiCol_FrameBg, style.Colors[ImGuiCol_FrameBg]);
        // Edit name of playlist
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        bool changed = ImGuiToolkit::InputText("##RenamePlaylist", &playlist_name_str, 
            ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopStyleColor();

        // Cancel button
        ImGui::SameLine(0, ImGui::GetTextLineHeightWithSpacing() + IMGUI_SAME_LINE / 2.f);      
        if (ImGui::Button(ICON_FA_TIMES)) {
            // discard
            playlist_edit_action = 0;
            playlist_name_str.clear();
        }
        // Validate button
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2.f); 
        changed |= ImGui::Button(ICON_FA_CHECK);
        if (changed) {

            // Rename request: delete file
            if (playlist_edit_action == 2){
                SystemToolkit::remove_file(Settings::application.recentPlaylists.path);
            }

            // Apply save
            if ( !filename.empty() ) {
                playlist_edit.saveAs( filename );

                // set mode to Playlist mode
                Settings::application.recentPlaylists.push(filename);
                Settings::application.recentPlaylists.assign(filename);
                Settings::application.pannel_playlist_mode = 1;

                // reload
                Settings::application.recentPlaylists.changed = true;
            }

            // reset the edit action
            playlist_edit_action = 0;
            playlist_name_str.clear();
        }  
    }
    //
    // Show name and confirm delete of a playlist
    //
    else if (playlist_edit_action == 3) {

        // Disabled name field to show the name of the playlist to delete
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.14f, 0.9f));
        ImGuiToolkit::InputText("##DeletePlaylist", &playlist_header, ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleColor(1);

        // Cancel button 
        ImGui::SameLine(0, ImGui::GetTextLineHeightWithSpacing() + IMGUI_SAME_LINE / 2.f);      
        if (ImGui::Button(ICON_FA_TIMES)) 
            playlist_edit_action = 0;
        // Validate button
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2.f);      
        if (ImGui::Button(ICON_FA_TRASH_ALT)) {
            // delete the file
            SystemToolkit::remove_file(Settings::application.recentPlaylists.path);
            // remove from the list
            Settings::application.recentPlaylists.filenames.remove(Settings::application.recentPlaylists.path);
            if (Settings::application.recentPlaylists.filenames.empty())
                Settings::application.pannel_playlist_mode = 0;
            else
                Settings::application.recentPlaylists.assign( Settings::application.recentPlaylists.filenames.front() );
            // reload list
            Settings::application.recentPlaylists.changed = true;
            playlist_edit_action = 0;
        }  
    }

    ImVec2 pos_top = ImGui::GetCursorPos();
    ImVec2 pos_bottom = ImGui::GetCursorPos();

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
        size_t index_to_remove = index_max;
        item_size.x -= ImGui::GetTextLineHeight() + style.ItemSpacing.x ;
        item_size.x -= index_max > 6 ? style.ScrollbarSize : 0.f;

        // display the sessions list and detect if one was selected (double clic)
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if (ImGui::ListBoxHeader("##Favorites", list_size) ) {

            // list session files in favorite playlist
            for (size_t index = 0; index < index_max; ++index) {
                // get name of session file at index
                std::string session_file = UserInterface::manager().favorites.at(index);

                // unique ID for item (filename can be at different index)
                ImGui::PushID( session_file.c_str() );
                float width = ImGui::GetContentRegionAvail().x;
                std::string label = ImGuiToolkit::truncatedText(SystemToolkit::filename(session_file), width);

                // item to select
                ImGui::BeginGroup();
                if (ImGui::Selectable( label.c_str(), false,
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
                    ImGuiToolkit::IconButton( ICON_VI_DRAG_HANDLE);
                }
                else {
                    if ( ImGuiToolkit::IconButton( ICON_VI_REMOVE, "Remove") )
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
        pos_bottom = ImGui::GetCursorPos();
        // cancel tooltip and mouse over on mouse exit
        if ( !ImGui::IsItemHovered())
            session_tooltip_ = 0;

        // Remove
        if ( index_to_remove < index_max ) {
            UserInterface::manager().favorites.remove( index_to_remove );
            UserInterface::manager().favorites.save();
        }
    }
    //
    // selection MODE 1 : PLAYLISTS
    //
    else if ( Settings::application.pannel_playlist_mode == 1) {

        // set header
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
                float width = ImGui::GetContentRegionAvail().x;
                std::string label = ImGuiToolkit::truncatedText(SystemToolkit::filename(session_file), width);

                // item to select
                ImGui::BeginGroup();
                if (ImGui::Selectable( label.c_str(), false,
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
                    ImGuiToolkit::IconButton( ICON_VI_DRAG_HANDLE);
                }
                else {
                    if ( ImGuiToolkit::IconButton( ICON_VI_REMOVE, "Remove") )
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
        pos_bottom = ImGui::GetCursorPos();
        // cancel tooltip and mouse over on mouse exit
        if ( !ImGui::IsItemHovered())
            session_tooltip_ = 0;

        // Remove
        if ( index_to_remove < index_max ) {
            active_playlist.remove( index_to_remove );
            active_playlist.save();
        }

        if ( playlist_edit_action == 0 ) {
            // Right side of the list icon to add sessions to the playlist
            ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_top.y + style.ItemSpacing.y));
            if (ImGuiToolkit::IconButton( ICON_VI_ADD, "Add sessions")) {
                selectSessions.open();
            }
            if (active_playlist.size() > 0) {
                static std::map< std::string, std::set<std::string> > resolutions;
                static std::map< std::string, std::chrono::system_clock::time_point > dates;
                static std::list< std::string > missing;
                ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_top.y + style.ItemSpacing.y + ImGui::GetFrameHeightWithSpacing()));
                if ( ImGuiToolkit::IconButton( ICON_VI_MULTIPLE_REMOVE, "Remove sessions")) {
                    // list resolutions and dates of all sessions of active_playlist
                    resolutions.clear();
                    dates.clear();
                    missing.clear();
                    for (size_t index = 0; index < active_playlist.size(); ++index) {
                        std::string filename = active_playlist.at(index);
                        // list missing files separately
                        if (!SystemToolkit::file_exists(filename)) {
                            missing.push_back(filename);
                            continue;
                        }
                        SessionInformation info = SessionCreator::info(filename, false);
                        resolutions[info.resolution].insert(filename);
                        dates[filename] = info.date;
                    }
                    // show popup menu to select sessions to remove
                    ImGui::OpenPopup( "menu_playlist_selection" );
                }
                if (ImGui::BeginPopup("menu_playlist_selection")) {

                    ImGui::TextDisabled("Remove by resolution");
                    for (auto it = resolutions.begin(); it != resolutions.end(); ++it) {
                        std::string label = it->first + " (" + std::to_string(it->second.size()) + ")";
                        if (ImGui::Selectable(label.c_str())) {
                            for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
                                active_playlist.remove(*it2);
                            }
                            active_playlist.save();
                        }
                    }
                    ImGui::Separator();
                    ImGui::TextDisabled("Remove by date");
                    static const std::vector< std::pair<std::string, std::chrono::hours> > ages = {
                        { "Older than 1 year",  std::chrono::hours(8766) },  // 365.25 days
                        { "Older than 1 month", std::chrono::hours(730) },   // 365.25 / 12 days
                        { "Older than 1 week",  std::chrono::hours(168) },
                        { "Older than 1 day",   std::chrono::hours(24) }
                    };
                    const auto now = std::chrono::system_clock::now();
                    for (auto it = ages.begin(); it != ages.end(); ++it) {
                        // list sessions older than this age
                        std::list<std::string> older;
                        for (auto it2 = dates.begin(); it2 != dates.end(); ++it2) {
                            if (now - it2->second > it->second)
                                older.push_back(it2->first);
                        }
                        std::string label = it->first + " (" + std::to_string(older.size()) + ")";
                        if (ImGui::Selectable(label.c_str(), false, older.empty() ? ImGuiSelectableFlags_Disabled : ImGuiSelectableFlags_None)) {
                            for (auto it2 = older.begin(); it2 != older.end(); ++it2) {
                                active_playlist.remove(*it2);
                            }
                            active_playlist.save();
                        }
                    }

                    ImGui::Separator();
                    ImGui::TextDisabled("Remove missing files");
                    std::string label = "Missing files (" + std::to_string(missing.size()) + ")";
                    if (ImGui::Selectable(label.c_str(), false, missing.empty() ? ImGuiSelectableFlags_Disabled : ImGuiSelectableFlags_None)) {
                        for (auto it = missing.begin(); it != missing.end(); ++it) {
                            active_playlist.remove(*it);
                        }
                        active_playlist.save();
                    }

                    ImGui::EndPopup();
                }

            }
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
                
                float width = ImGui::GetContentRegionAvail().x;
                std::string label = ImGuiToolkit::truncatedText(SystemToolkit::filename(*it), width);

                // item to select
                if (ImGui::Selectable( label.c_str(), false,
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
        pos_bottom = ImGui::GetCursorPos();
        // cancel tooltip and mouse over on mouse exit
        if ( !ImGui::IsItemHovered())
            session_tooltip_ = 0;

        // Ordering button
        ImGui::PushID("##playlist_directory_actions");
        ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_top.y + style.ItemSpacing.y) );
        if ( ImGuiToolkit::IconMultistate(icons_ordering_files, &Settings::application.recentFolders.ordering, tooltips_ordering_files) )
            Settings::application.recentFolders.changed = true;
        ImGui::PopID();

    }

    ImGui::SetCursorPos(pos_bottom);

    // export all to folder
    static Exporter *playlist_exporter = nullptr;
    static bool _playlist_exporting = false;

    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.f,0.f,0.f,0.f));
    _playlist_exporting = ImGui::CollapsingHeader("Export",
        _playlist_exporting ? ImGuiTreeNodeFlags_DefaultOpen : 0);
    
    if (_playlist_exporting ) {

        // check for completion
        if (playlist_exporter != nullptr && playlist_exporter->finished()) {
            if (playlist_exporter->success()) {
                // notify success
                Log::Notify("Export completed: %d file(s) copied to '%s'.",
                            playlist_exporter->count(), Settings::application.recentExportFolder.path.c_str());
            }
            else
                Log::Info("Export cancelled.");
            delete playlist_exporter;
            playlist_exporter = nullptr;
        }

        // path display + folder choose button
        std::string label = BaseToolkit::truncated(Settings::application.recentExportFolder.path, 23);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetColorU32(ImGuiCol_FrameBgHovered));
        ImGuiToolkit::InputText("##export_path", &label, ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleColor();

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", Settings::application.recentExportFolder.path.c_str());
        pos_bottom = ImGui::GetCursorPos();
        ImGui::SameLine();
        if (ImGuiToolkit::IconButton(ICON_FA_FOLDER_OPEN, "Choose destination folder") && playlist_exporter == nullptr)
            exportFolder.open();

        // copy media toggle
        ImGui::SetCursorPos(pos_bottom);
        ImGuiToolkit::ButtonSwitch("Copy media", &Settings::application.export_options[0],
            "Copy all media files referenced in the session files", playlist_exporter == nullptr);
        ImGuiToolkit::ButtonSwitch("Discard versions", &Settings::application.export_options[2],
            "Do NOT copy versions in the exported session files", playlist_exporter == nullptr);
#ifdef VIMIX_USE_MINIZ
        ImGuiToolkit::ButtonSwitch("Archive", &Settings::application.export_options[1],
            "Pack all files into a ZIP archive", playlist_exporter == nullptr);
#endif

        // export button or progress bar
        if (playlist_exporter == nullptr) {
            if (Settings::application.recentExportFolder.path.empty()){
                ImGuiToolkit::ButtonDisabled(ICON_FA_SAVE "  Export all", ImVec2(IMGUI_RIGHT_ALIGN, 0));
            }
            else if (ImGui::Button(ICON_FA_SAVE "  Export all", ImVec2(IMGUI_RIGHT_ALIGN, 0))) {
                Playlist tmp;
                std::string archive_name;
                if (Settings::application.pannel_playlist_mode == 0) {
                    tmp = UserInterface::manager().favorites;
                    archive_name = SystemToolkit::base_filename(
                        UserInterface::manager().favorites.filename());
                } else if (Settings::application.pannel_playlist_mode == 1) {
                    tmp = active_playlist;
                    archive_name = SystemToolkit::base_filename(active_playlist.filename());
                } else {
                    tmp.add(folder_session_files);
                    archive_name = SystemToolkit::filename(Settings::application.recentFolders.path);
                }
                playlist_exporter = new Exporter(tmp, Settings::application.recentExportFolder.path,
                                                  Settings::application.export_options[0],
                                                  Settings::application.export_options[2],
                                                  Settings::application.export_options[1],
                                                  archive_name);
                if (!playlist_exporter->start()) {
                    Log::Warning("Export failed to start: %s", playlist_exporter->error().c_str());
                    delete playlist_exporter;
                    playlist_exporter = nullptr;
                }
            }
            ImGui::SameLine();
            ImGuiToolkit::HelpToolTip("Export the playlist to a target folder.\n\n"
                    ICON_FA_FOLDER_OPEN "  Choose a destination folder where to copy "
                    "the session files of the playlist. ");
        }
        else {
            float progress = playlist_exporter->progress();
#ifdef VIMIX_USE_MINIZ
            const char *overlay = playlist_exporter->compressing() ? "compressing..."
                                : progress < (float)EPSILON ? "preparing..." : nullptr;
#else
            const char *overlay = progress < (float)EPSILON ? "preparing..." : nullptr;
#endif
            ImGui::ProgressBar(progress, ImVec2(IMGUI_RIGHT_ALIGN, 0), overlay);
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_TIMES " Cancel"))
                playlist_exporter->stop();
        }
    }
    else {
        if (playlist_exporter != nullptr && !playlist_exporter->finished()) {
            ImVec2 pos_tmp = ImGui::GetCursorPos();
            ImVec2 space_size = ImGui::CalcTextSize(" Export ", NULL);
            space_size.x += ImGui::GetTextLineHeightWithSpacing() * 2.f;
            space_size.y = -ImGui::GetTextLineHeightWithSpacing() - ImGui::GetStyle().ItemSpacing.y;
            ImGui::SetCursorPos( pos_tmp + space_size );
            ImGui::Text("( %d %% )", (int)(100.0 * playlist_exporter->progress()));
            ImGui::SetCursorPos( pos_tmp );
        }
    }

    ImGui::PopStyleColor();

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

        // discard any pending edit action
        playlist_edit_action = 0;
    }
    // help indicator
    pos_top.y += list_size.y;
    ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_top.y - 2.f * ImGui::GetFrameHeightWithSpacing()));
    ImGuiToolkit::HelpToolTip("Double-clic on a filename to open the session.\n\n "
                              ICON_FA_ARROWS_ALT_V   "   Drag to reorder the sessions in the playlist.\n"
                              ICON_FA_ARROW_CIRCLE_RIGHT "  With Smooth transition enabled,"
                                                         " the session will open with a cross fade or a fade to black.");

    // toggle button for smooth transition
    ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_top.y - ImGui::GetFrameHeightWithSpacing()) );
    ImGuiToolkit::ButtonToggle(ICON_FA_ARROW_CIRCLE_RIGHT, &Settings::application.smooth_transition, "Smooth transition");

    // transition mode icon if enabled
    if (Settings::application.smooth_transition) {
        const char *tooltip[2] = {"Fade to black", "Cross fading"};
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        if (Mixer::manager().session()->fading() > 0.01)
            ImGuiToolkit::Icon(ICON_VI_TRANSITION_FADE_BLACK, false);
        else
            ImGuiToolkit::IconToggle(ICON_VI_TRANSITION_FADE_BLACK, ICON_VI_TRANSITION_CROSS_FADE, &Settings::application.transition.cross_fade, tooltip );
    }

    //
    // Popup window of menu playlist
    //
    if (ImGui::BeginPopup("menu_playlist_popup"))
    {
        // Menu for Favorites
        if ( Settings::application.pannel_playlist_mode == 0) {
            if (ImGui::MenuItem("Duplicate" )){
                playlist_edit = UserInterface::manager().favorites;
                playlist_edit_name = "Favorites_copy";
                playlist_edit_action = 1;
            }        
            ImGui::TextDisabled("Rename");
            ImGui::TextDisabled("Remove");
        } 
        // Menu for Playlists
        else if ( Settings::application.pannel_playlist_mode == 1) {
            if (ImGui::MenuItem("Duplicate" )){
                playlist_edit = active_playlist;
                playlist_edit_name = SystemToolkit::base_filename(Settings::application.recentPlaylists.path);
                playlist_edit_name += "_copy";
                playlist_edit_action = 1;
            }        
            if (ImGui::MenuItem("Rename" )){
                playlist_edit = active_playlist;
                playlist_edit_name = SystemToolkit::base_filename(Settings::application.recentPlaylists.path);
                playlist_edit_action = 2;
            }        
            if (ImGui::MenuItem("Remove" )){
                playlist_edit_action = 3;
            }
        } 
        // Menu for Directory list
        else if ( Settings::application.pannel_playlist_mode == 2) {
            if (ImGui::MenuItem("Duplicate" )){
                playlist_edit.clear();
                playlist_edit.add(folder_session_files);
                playlist_edit_name = SystemToolkit::base_filename(Settings::application.recentFolders.path);
                playlist_edit_action = 1;
            }                    
            ImGui::TextDisabled("Rename");
            if (ImGui::MenuItem("Remove" )){
                Settings::application.recentFolders.filenames.remove(Settings::application.recentFolders.path);
                if (Settings::application.recentFolders.filenames.empty())
                    Settings::application.pannel_playlist_mode = 0;
                else
                    Settings::application.recentFolders.assign( Settings::application.recentFolders.filenames.front() );
            }
        }

        if (ImGui::IsWindowHovered())
            counter_menu_timeout=0;
        else if (++counter_menu_timeout > 20)
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }



}
