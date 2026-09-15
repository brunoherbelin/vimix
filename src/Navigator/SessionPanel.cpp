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

#include "NavigatorInternal.h"

#include "IconsFontAwesome5.h"
#include "defines.h"
#include "Settings.h"
#include "Log.h"
#include "Mixer.h"
#include "Exporter.h"
#include "Playlist.h"
#include "FrameBuffer.h"
#include "FrameGrabber.h"
#include "FrameGrabbing.h"
#include "ActionManager.h"
#include "SessionCreator.h"
#include "View/RenderView.h"
#include "Toolkit/BaseToolkit.h"
#include "Toolkit/SystemToolkit.h"
#include "Toolkit/DialogToolkit.h"
#include "Toolkit/ImGuiToolkit.h"
#include "UserInterfaceManager.h"

#include "NavigatorWidgets.h"
#include "SessionPanel.h"

// utility function defined in UserInterfaceManager.cpp
std::string readable_date_time_string(std::string date);

void SessionPanel::Render()
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
            ImGui::PushID(it->c_str());
            if (ImGui::Selectable( SystemToolkit::filename(*it).c_str() ) ) {
                Mixer::manager().open( *it );
            }
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text( "%s", (*it).c_str() );
                ImGui::EndTooltip();
            }
            ImGui::PopID();
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
                    if (ImGui::Selectable( ICON_FA_FILE_DOWNLOAD "     Export as session", false, 0, size )) {
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
        if (ImGuiToolkit::IconButton( ICON_FA_CODE_BRANCH "+", "Save & Keep version"))
            UserInterface::manager().saveOrSaveAs(true);
        if (!snapshots.empty()) {
            ImGui::SameLine();
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
    else {
        if ( Action::manager().max() > 1 )  {
            ImVec2 pos_tmp = ImGui::GetCursorPos();
            ImVec2 space_size = ImGui::CalcTextSize(" Undo history ", NULL);
            space_size.x += ImGui::GetTextLineHeightWithSpacing() * 2.f;
            space_size.y = -ImGui::GetTextLineHeightWithSpacing() - space;
            ImGui::SetCursorPos( pos_tmp + space_size );
            ImGui::Text("( %u )", Action::manager().max() - 1);
            ImGui::SetCursorPos( pos_tmp );
        }
    }

    //
    // EXPORT
    //    
    static DialogToolkit::OpenFolderDialog exportFolder("Export to Folder");
    // return from thread for export folder selection
    if (exportFolder.closed() && !exportFolder.path().empty()){
        Settings::application.recentExportFolder.path = exportFolder.path();
    }

    static Exporter *exporter = nullptr;
    Settings::application.pannel_session[3] = ImGui::CollapsingHeader("Export",
                                                                      Settings::application.pannel_session[3] ? ImGuiTreeNodeFlags_DefaultOpen : 0);
    if (Settings::application.pannel_session[3]) {

        // check for completion
        if (exporter != nullptr && exporter->finished()) {
            if (exporter->success()) {
                // notify success
                Log::Notify("Export completed: %d file(s) copied to '%s'.",
                            exporter->count(), Settings::application.recentExportFolder.path.c_str());
            }
            else
                Log::Info("Export cancelled.");
            delete exporter;
            exporter = nullptr;
        }

        // path display + folder choose button
        std::string label = BaseToolkit::truncated(Settings::application.recentExportFolder.path, 23);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetColorU32(ImGuiCol_FrameBgHovered));
        ImGuiToolkit::InputText("##session_export_path", &label, ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", Settings::application.recentExportFolder.path.c_str());
        ImVec2 pos_bot = ImGui::GetCursorPos();
        ImGui::SameLine();
        if (ImGuiToolkit::IconButton(ICON_FA_FOLDER_OPEN, "Choose destination folder") && exporter == nullptr)
            exportFolder.open();

        // copy media toggle
        ImGui::SetCursorPos(pos_bot);
        ImGuiToolkit::ButtonSwitch("Copy media", &Settings::application.export_options[0],
            "Copy all media files referenced in the session file", exporter == nullptr);
        ImGuiToolkit::ButtonSwitch("Discard versions", &Settings::application.export_options[2],
            "Do NOT copy versions in the exported session file", exporter == nullptr);
#ifdef VIMIX_USE_MINIZ
        ImGuiToolkit::ButtonSwitch("Archive", &Settings::application.export_options[1],
            "Pack all files into a ZIP archive", exporter == nullptr);
#endif

        // export button or progress bar
        if (exporter == nullptr) {
            // cannot export if no target folder or session file,
            // or if target folder is same than current session folder (avoid overwriting)
            if (Settings::application.recentExportFolder.path.empty() ||
                Mixer::manager().session()->filename().empty() ||
                SystemToolkit::path_filename(Mixer::manager().session()->filename()) == Settings::application.recentExportFolder.path + PATH_SEP) {
                ImGuiToolkit::ButtonDisabled(ICON_FA_SAVE "  Export", ImVec2(IMGUI_RIGHT_ALIGN, 0));
            }
            else if (ImGui::Button(ICON_FA_SAVE "  Export", ImVec2(IMGUI_RIGHT_ALIGN, 0))) {
                const std::string archive_name =
                    SystemToolkit::base_filename(Mixer::manager().session()->filename());
                Playlist tmp;
                tmp.add(Mixer::manager().session()->filename());
                exporter = new Exporter(tmp, Settings::application.recentExportFolder.path,
                                        Settings::application.export_options[0],
                                        Settings::application.export_options[2],
                                        Settings::application.export_options[1],
                                        archive_name);
                if (!exporter->start()) {
                    Log::Warning("Export failed to start: %s", exporter->error().c_str());
                    delete exporter;
                    exporter = nullptr;
                }
            }
            ImGui::SameLine();
            ImGuiToolkit::HelpToolTip("Export the session to a target folder or archive.\n\n"
                    ICON_FA_FOLDER_OPEN "  Choose a destination folder where to save "
                    "a complete copy of the session. ");
        }
        else {
            float progress = exporter->progress();
#ifdef VIMIX_USE_MINIZ
            const char *overlay = exporter->compressing() ? "compressing..."
                                : progress < (float)EPSILON ? "preparing..." : nullptr;
#else
            const char *overlay = progress < (float)EPSILON ? "preparing..." : nullptr;
#endif
            ImGui::ProgressBar(progress, ImVec2(IMGUI_RIGHT_ALIGN, 0), overlay);
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_TIMES " Cancel"))
                exporter->stop();
        }
    }
    else {
        if (exporter != nullptr && !exporter->finished()) {
            ImVec2 pos_tmp = ImGui::GetCursorPos();
            ImVec2 space_size = ImGui::CalcTextSize(" Export ", NULL);
            space_size.x += ImGui::GetTextLineHeightWithSpacing() * 2.f;
            space_size.y = -ImGui::GetTextLineHeightWithSpacing() - ImGui::GetStyle().ItemSpacing.y;
            ImGui::SetCursorPos( pos_tmp + space_size );
            ImGui::Text("( %d %% )", (int)(100.0 * exporter->progress()));
            ImGui::SetCursorPos( pos_tmp );
        }
    }
    ImGui::PopStyleColor(1);
}
