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
#include <vector>
#include <regex>
#include <cmath>

#include "NavigatorInternal.h"

#include "IconsFontAwesome5.h"
#include "defines.h"
#include "IconsVimixImage.h"
#include "Settings.h"
#include "Log.h"
#include "Mixer.h"
#include "MediaPlayer.h"
#include "Connection.h"
#include "ControlManager.h"
#include "MultiFileRifeEncoder.h"
#include "Source/MediaSource.h"
#include "Source/MultiFileSource.h"
#include "Source/PatternSource.h"
#include "Source/DeviceSource.h"
#include "Source/ScreenCaptureSource.h"
#include "Source/RenderSource.h"
#include "Source/SourceCallback.h"
#include "Toolkit/BaseToolkit.h"
#include "Toolkit/SystemToolkit.h"
#include "Toolkit/GstToolkit.h"
#include "Toolkit/DialogToolkit.h"
#include "Toolkit/ImGuiToolkit.h"
#include "View/RenderView.h"
#include "UserInterfaceManager.h"

#include "Navigator.h"
#include "NavigatorCodec.h"
#include "NewSourcePanel.h"

NewSourcePanel::NewSourcePanel() : new_media_mode(MEDIA_RECENT), new_media_mode_changed(true),
    pattern_type(-1), generated_type(-1), custom_type(-1)
{
}

void NewSourcePanel::clearNewPannel()
{
    new_source_preview_.setSource();
    pattern_type = -1;
    generated_type = -1;
    custom_type = -1;
    sourceSequenceFiles.clear();
    sourceMediaFileCurrent.clear();
    new_media_mode_changed = true;
}

void NewSourcePanel::setNewMedia(MediaCreateMode mode, std::string path)
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

void NewSourcePanel::Render(Navigator *navigator, const ImVec2 &iconsize)
{
    // the source to replace belongs to the Navigator (shared with the source pannel)
    Source *&source_to_replace = navigator->sourceToReplace();
    if (Settings::application.current_view == View::TRANSITION)
        return;

    const ImGuiStyle& style = ImGui::GetStyle();

    // Next window is a side pannel
    if (beginPannelWindow("##navigatorNewSource"))
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
        if (ImGuiToolkit::SelectableIcon( ICON_VI_OPEN_FILE, "##SOURCE_FILE", selected_type[SOURCE_FILE], iconsize)) {
            Settings::application.source.new_type = SOURCE_FILE;
            clearNewPannel();
        }
        ImGui::NextColumn();
        if (ImGuiToolkit::SelectableIcon( ICON_VI_SOURCE_SEQUENCE, "##SOURCE_SEQUENCE", selected_type[SOURCE_SEQUENCE], iconsize)) {
            Settings::application.source.new_type = SOURCE_SEQUENCE;
            clearNewPannel();
        }
        ImGui::NextColumn();
        if (ImGuiToolkit::SelectableIcon( ICON_VI_NEW_SOURCE_CONNECTED, "##SOURCE_CONNECTED", selected_type[SOURCE_CONNECTED], iconsize)) {
            Settings::application.source.new_type = SOURCE_CONNECTED;
            clearNewPannel();
        }
        ImGui::NextColumn();
        if (ImGuiToolkit::SelectableIcon( ICON_VI_SOURCE_PATTERN, "##SOURCE_GENERATED", selected_type[SOURCE_GENERATED], iconsize)) {
            Settings::application.source.new_type = SOURCE_GENERATED;
            clearNewPannel();
        }
        ImGui::NextColumn();
        if (ImGuiToolkit::SelectableIcon( ICON_VI_SOURCE_GROUP, "##SOURCE_BUNDLE", selected_type[SOURCE_BUNDLE], iconsize)) {
            Settings::application.source.new_type = SOURCE_BUNDLE;
            clearNewPannel();
        }

        ImGui::Columns(1);
        ImGui::PopStyleVar();
        ImGui::PopFont();


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
                    navigator->togglePannelNew();
                }
            }

            // combo to offer lists
            ImGui::Spacing();
            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            if (ImGui::BeginCombo("##SelectionNewMedia", BaseToolkit::truncated(Settings::application.recentImportFolders.path, 23).c_str() ))
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
            // compute maximum number of items to display in listbox according to available height
            int max_items = (int) ((height_ - pos_top.y - 12.f * ImGui::GetFrameHeightWithSpacing() ) / ImGui::GetTextLineHeight());
            max_items = CLAMP(max_items, 6, 25);
            // display the import-list and detect if one was selected
            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            if (ImGui::ListBoxHeader(listboxname[new_media_mode], sourceMediaFiles.size(), CLAMP(sourceMediaFiles.size(), 4, max_items)) ) {
                static int tooltip = 0;
                static std::string filenametooltip;
                float width = ImGui::GetContentRegionAvail().x;
                // loop over list of files
                for(auto it = sourceMediaFiles.begin(); it != sourceMediaFiles.end(); ++it) {
                    // build displayed file name
                    std::string filename = BaseToolkit::transliterate(*it);
                    // std::string label = BaseToolkit::truncated(SystemToolkit::filename(filename), 23);
                    std::string label = ImGuiToolkit::truncatedText(SystemToolkit::filename(filename), width);
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
                if (ImGuiToolkit::IconButton( ICON_VI_CLEAR_LIST, "Clear list")) {
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
                if (ImGuiToolkit::IconButton( ICON_VI_FOLDER_CLOSE, "Close directory")) {
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
                if (ImGuiToolkit::IconButton( ICON_VI_CLEAR_LIST, "Clear list")) {
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
            static MultiFileRifeEncoder _rife_encoder;
            // static int codec_id = -1;

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
                if (_numbered_sequence.valid() && Settings::application.image_sequence.profile < 0) {
                    // propose image sequence if requested and possible
                    // show source preview available if possible
                    std::string label = BaseToolkit::transliterate( BaseToolkit::common_pattern(sourceSequenceFiles) );
                    new_source_preview_
                        .setSource(Mixer::manager().createSourceMultifile(sourceSequenceFiles,
                                                                          Settings::application.image_sequence.framerate_mode),
                                   label);
                } 
                else if (Settings::application.image_sequence.profile < 0)
                    Settings::application.image_sequence.profile = 0; // default to H264 video encoding
            }

            // multiple files selected
            if (sourceSequenceFiles.size() > 1) {

                ImGui::Spacing();

                // encoding is done at the resolution of the images, rounded even
                const int sequence_width = (int) (_numbered_sequence.width & ~1);
                const int sequence_height = (int) (_numbered_sequence.height & ~1);

                // show info sequence
                ImGuiTextBuffer info;
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.14f, 0.9f));
                info.appendf("%d %s (%d x %d)", 
                    (int) sourceSequenceFiles.size(), 
                    _numbered_sequence.codec.c_str(),
                    sequence_width,
                    sequence_height);
                ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                ImGui::InputText("##SequenceSelection", (char *)info.c_str(), info.size(), ImGuiInputTextFlags_ReadOnly);
                ImGui::PopStyleColor(1);
                ImGui::SameLine(0, IMGUI_SAME_LINE);
                if (ImGuiToolkit::TextButton("Selection")) {
                    sourceSequenceFiles.clear();
                    new_source_preview_.setSource();
                    _numbered_sequence = MultiFileSequence();
                }

                // encoding profile validation; ensure it supports the resolution of the images
                if (Settings::application.image_sequence.profile >= 0)
                    ValidateCodecResolution(&Settings::application.image_sequence.profile,
                                            sequence_width, sequence_height);

                // select CODEC: decide for gst sequence (codec_id = -1) or encoding a video
                ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                std::string codec_current = Settings::application.image_sequence.profile < 0 ? ICON_FA_SORT_NUMERIC_DOWN "  Image sequence"
                                                         : std::string(ICON_FA_FILM " ") + GstToolkit::profile_name[Settings::application.image_sequence.profile];
                if (ImGui::BeginCombo("##CodecSequence", codec_current.c_str(), ImGuiComboFlags_HeightLarge)) {
                    // special case; if possible, offer to create an image sequence gst source
                    if (ImGui::Selectable( ICON_FA_SORT_NUMERIC_DOWN "  Image sequence",
                                          Settings::application.image_sequence.profile < 0,
                                          _numbered_sequence.valid()
                                              ? ImGuiSelectableFlags_None
                                              : ImGuiSelectableFlags_Disabled)) {
                        // select id of image sequence
                        Settings::application.image_sequence.profile = -1;
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
                    // offer to encode as an image sequence
                    {
                        std::string label = std::string(ICON_FA_IMAGES " ") + GstToolkit::profile_name[GstToolkit::JPEG_MULTI];
                        const bool supported = GstToolkit::supportsResolution(GstToolkit::JPEG_MULTI,
                                                                             sequence_width, sequence_height,
                                                                             Settings::application.render.gpu_decoding);
                        if (ImGui::Selectable(label.c_str(), Settings::application.image_sequence.profile == GstToolkit::JPEG_MULTI,
                                              supported ? ImGuiSelectableFlags_None : ImGuiSelectableFlags_Disabled)) {
                            // select multi-image encoding (jpeg) for image sequence
                            Settings::application.image_sequence.profile = GstToolkit::JPEG_MULTI;
                            // close source preview (no image sequence)
                            new_source_preview_.setSource();
                        }
                        if (!supported && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                            ImGuiToolkit::ToolTip( GstToolkit::unsupportedResolution(GstToolkit::JPEG_MULTI,
                                                   sequence_width, sequence_height,
                                                   Settings::application.render.gpu_decoding).c_str() );
                    }
                    // offer to encode as a video
                    for (int i = GstToolkit::H264_RT; i < GstToolkit::JPEG_MULTI; ++i) {
                        std::string label = std::string(ICON_FA_FILM " ") + GstToolkit::profile_name[i];
                        const bool supported = GstToolkit::supportsResolution((GstToolkit::Profile) i,
                                                                             sequence_width, sequence_height,
                                                                             Settings::application.render.gpu_decoding);
                        if (ImGui::Selectable(label.c_str(), Settings::application.image_sequence.profile == i,
                                              supported ? ImGuiSelectableFlags_None : ImGuiSelectableFlags_Disabled)) {
                            // select id of video encoding codec
                            Settings::application.image_sequence.profile = i;
                            // close source preview (no image sequence)
                            new_source_preview_.setSource();
                        }
                        if (!supported && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                            ImGuiToolkit::ToolTip( GstToolkit::unsupportedResolution((GstToolkit::Profile) i,
                                                   sequence_width, sequence_height,
                                                   Settings::application.render.gpu_decoding).c_str() );
                    }
                    ImGui::EndCombo();
                }
                // Indication
                ImGui::SameLine();
                ImGuiToolkit::HelpToolTip(ICON_FA_SORT_NUMERIC_DOWN " Create an image sequence from the selected images; "
                                              "possible only if the selected images are numbered consecutively.\n\n"
                                              ICON_FA_IMAGES " Produce a sequence of consecutively numbered JPEG images in a subfolder.\n\n"
                                              ICON_FA_FILM " Encode a video with the selected images and create a video source.");

                // set framerate
                ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                ImGui::SliderInt("##SequenceFramerate", &Settings::application.image_sequence.framerate_mode, 1, 30, "%d fps");
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
                ImGui::SameLine(0, IMGUI_SAME_LINE);
                if (ImGuiToolkit::TextButton("Framerate")) {
                    Settings::application.image_sequence.framerate_mode = 25;
                }

                // if video encoding codec selected
                if ( Settings::application.image_sequence.profile >= 0 )
                {

#if defined(HAVE_NCNN) || defined(HAVE_ONNX)
                    // set number of intermediate frames to generate between each image (for video encoding)
                    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                    // slider position num maps to 2^num -1 intermediate frames;
                    // restore it from the saved value with the inverse (log2)
                    static int num = CLAMP( (int) log2f(
                        (float) Settings::application.image_sequence.buffering_mode + 1.f), 0, 5);
                    Settings::application.image_sequence.buffering_mode = pow(2, num)-1;
                    char buf[64];
                    ImFormatString(buf, IM_ARRAYSIZE(buf), "%d  intermediate frames", 
                                    Settings::application.image_sequence.buffering_mode);
                    ImGui::SliderInt("##Interpolate", &num, 0, 5, buf);
                    ImGui::SameLine();
#if defined(HAVE_NCNN)
                    ImGuiToolkit::Indication("Use Real-time Intermediate Flow Estimation (RIFE), an AI-based "
                                            "algorithm to generates smooth intermediate frames, "
                                            "powered by NCNN backend on GPU (Vulkan).\n\n"
                                            ICON_FA_MINUS_CIRCLE "  Set to 0 to disable interpolation.\n",
                                            ICON_FA_MAGIC);
#else
                    ImGuiToolkit::Indication("Use Real-time Intermediate Flow Estimation (RIFE), an AI-based "
                                            "algorithm to generates smooth intermediate frames, "
                                            "powered by ONNX backend on CPU.\n\n"
                                            ICON_FA_MINUS_CIRCLE "  Set to 0 to disable interpolation.\n",
                                            ICON_FA_MAGIC);
#endif
#endif
                    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                    ImGui::Combo("##SequenceLoop", &Settings::application.image_sequence.priority_mode, 
                        "None\0Rewind\0Mirror\0");
                    ImGui::SameLine(0, IMGUI_SAME_LINE);
                    if (ImGuiToolkit::TextButton("Loop")) {
                        Settings::application.image_sequence.priority_mode = 0;
                    }
                    // Offer to create video from sequence
                    ImGui::NewLine();
                    if ( ImGui::Button( ICON_FA_COGS "  Encode", ImVec2(ImGui::GetContentRegionAvail().x, 0)) ) {
                        RifeOptions options;
                        options.loop = Settings::application.image_sequence.priority_mode;
                        options.fps = Settings::application.image_sequence.framerate_mode;
                        options.mid = Settings::application.image_sequence.buffering_mode;
                        options.profile = (GstToolkit::Profile) Settings::application.image_sequence.profile;
                        _rife_encoder.setFiles( sourceSequenceFiles );
                        _rife_encoder.start(options);
                        // open dialog
                        ImGui::OpenPopup(LABEL_VIDEO_SEQUENCE);
                    }
                }

                // video recorder finished: inform and open pannel to import video source from recent recordings
                if ( _rife_encoder.finished() ) {

                    // reset encoder to be ready for next encoding
                    _rife_encoder.reset();

                    // video recorder failed if it does not return a valid filename
                    if ( !_rife_encoder.success() || _rife_encoder.filename().empty() )
                        Log::Warning("Failed to generate an image sequence (%s).", _rife_encoder.message().c_str() );
                    // JPEG_MULTI produced a folder of numbered images, not a video file
                    else if (Settings::application.image_sequence.profile == GstToolkit::JPEG_MULTI) {

                        sourceSequenceFiles = SystemToolkit::list_directory(_rife_encoder.filename(), {"*.jpg", "*.jpeg", "*.png"});
                        _numbered_sequence = MultiFileSequence(sourceSequenceFiles);

                        if (_numbered_sequence.valid()) {
                            // propose image sequence if possible
                            // show source preview available if possible
                            std::string label = BaseToolkit::transliterate( BaseToolkit::common_pattern(sourceSequenceFiles) );
                            new_source_preview_
                                .setSource(Mixer::manager().createSourceMultifile(sourceSequenceFiles,
                                                                                Settings::application.image_sequence.framerate_mode),
                                        label);
                            // select id of image sequence
                            Settings::application.image_sequence.profile = -1;
                        } 

                    }
                    else {

                        // save path location if valid
                        std::string uri = GstToolkit::filename_to_uri(_rife_encoder.filename());
                        MediaInfo media = MediaPlayer::UriDiscoverer(uri);
                        if (media.valid && !media.isimage)
                            Settings::application.recentRecordings.push(_rife_encoder.filename());
                        else
                            Settings::application.recentRecordings.remove(_rife_encoder.filename());

                        Log::Notify("Image sequence saved to %s.", _rife_encoder.filename().c_str());
                        // open the file as new recording
                        setNewMedia(Navigator::MEDIA_RECORDING, _rife_encoder.filename());
                    }
                }
                else if (ImGui::BeginPopupModal(LABEL_VIDEO_SEQUENCE, NULL, ImGuiWindowFlags_NoResize))
                {
                    ImGui::Spacing();
                    ImGui::Text("Please wait while the video is being encoded :             \n");
                    ImGui::Text("%s\n", _rife_encoder.message().c_str());

                    ImGui::Text("Framerate :");ImGui::SameLine(150);
                    ImGui::Text("%d fps", Settings::application.image_sequence.framerate_mode );
                    ImGui::Text("Codec :");ImGui::SameLine(150);
                    ImGui::Text("%s", GstToolkit::profile_name[ Settings::application.image_sequence.profile ] );
                    ImGui::Text("Frames :");ImGui::SameLine(150);
                    ImGui::Text("%lu (%lu key frames)", (unsigned long)_rife_encoder.numFrames(), (unsigned long)_rife_encoder.files().size() ) ;

                    ImGui::Spacing();
                    ImGui::ProgressBar(_rife_encoder.progress());

                    ImGui::Spacing();
                    ImGui::Spacing();
                    if (ImGui::Button(ICON_FA_TIMES " Cancel",ImVec2(ImGui::GetContentRegionAvail().x, 0)))
                        _rife_encoder.stop();

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
            
            std::string current_pattern;
            if (generated_type == 2 && pattern_type < (int) Pattern::count())
                current_pattern = Pattern::get(pattern_type).label;
            else if (generated_type == 1)
                current_pattern = "Text";
            else if (generated_type == 3)
                current_pattern = "Custom shader";
            else if (generated_type == 0)
                current_pattern = "Custom gstreamer";
            else
                current_pattern = "Select";

            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            if (ImGui::BeginCombo("##Pattern", current_pattern.c_str(), ImGuiComboFlags_HeightLarge))
            {
                if ( ImGuiToolkit::BeginMenuIcon(ICON_VI_SOURCE_PATTERN, "Static patterns"))
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
                if ( ImGuiToolkit::BeginMenuIcon(ICON_VI_SOURCE_PATTERN, "Animated patterns"))
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
                if ( ImGuiToolkit::SelectableIcon(ICON_VI_SOURCE_TEXT, "Text", false) )
                {
                    update_new_source = true;
                    generated_type = 1;
                    pattern_type = -1;
                }
                if ( ImGuiToolkit::SelectableIcon(ICON_VI_SOURCE_SHADER, "Custom shader", false) )
                {
                    update_new_source = true;
                    generated_type = 3;
                    pattern_type = -1;
                }
                if ( ImGuiToolkit::SelectableIcon(ICON_VI_SOURCE_GSTREAMER, "Custom gstreamer", false) )
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
                                      "Displaying text, a custom gstreamer pipeline or a custom shader is also possible.");

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

                ImVec2 pos_bot = ImGui::GetCursorPos();
                // Paste & go
                ImGui::SetCursorPos( pos_bot + ImVec2(fieldsize.x + IMGUI_SAME_LINE, -2.f * ImGui::GetFrameHeightWithSpacing()));
                if ( ImGuiToolkit::IconButton(ICON_FA_PASTE, "Paste & go") ) {
                    _description = ImGui::GetClipboardText();
                    update_new_source = true;
                }
                // Local menu for list of examples
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
            
            std::string current_connection;
            if (custom_type == 0)
                current_connection = "Display Loopback";
            else if (custom_type == 1)
                current_connection = "Screen capture";
            else if (custom_type == 2)
                current_connection = "SRT Broadcast";
            else if (custom_type == 3) {
                current_connection = "Device";
                if (new_source_preview_.filled())
                    current_connection = new_source_preview_.label();
            }
            else
                current_connection = "Select";

            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            if (ImGui::BeginCombo("##ExternalConnected", current_connection.c_str(), ImGuiComboFlags_HeightLarge))
            {
                // 1. Loopback source
                if ( ImGuiToolkit::SelectableIcon(ICON_VI_SOURCE_RENDER, "Display Loopback", false) ) {
                    custom_type = 0;
                    new_source_preview_.setSource();
                }

                // 2. Screen capture (open selector)
                if ( ImGuiToolkit::SelectableIcon(ICON_VI_SOURCE_DEVICE_SCREEN, "Screen capture", false) ) {
                    custom_type = 1;
                    new_source_preview_.setSource();
                }

                // 3. Network connected SRT
                if ( ImGuiToolkit::SelectableIcon(ICON_VI_SOURCE_SRT, "SRT Broadcast", false) ) {
                    custom_type = 2;
                    new_source_preview_.setSource();
                }

                // 4. Devices
                ImGui::Separator();
                for (int d = 0; d < Device::manager().numDevices(); ++d){
                    std::string namedev = Device::manager().name(d);
                    if (ImGui::Selectable( namedev.c_str() )) {
                        custom_type = 3;
                        new_source_preview_.setSource( Mixer::manager().createSourceDevice(namedev), namedev);
                    }
                }

                // 5. Network connected vimix
                for (int d = 1; d < Connection::manager().numHosts(); ++d){
                    std::string namehost = Connection::manager().info(d).name;
                    if (ImGui::Selectable( namehost.c_str() )) {
                        custom_type = 3;
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
                                      ICON_FA_CARET_RIGHT " network SRT broadcast.\n"
                                      "For connected devices;\n"
                                      ICON_FA_CARET_RIGHT " webcams or frame grabbers\n"
                                      ICON_FA_CARET_RIGHT " vimix Peer-to-peer in local network.");
            ImGui::SameLine();
            if (ImGuiToolkit::IconButton(ICON_VI_RELOAD, "Reload list")) {
                Device::manager().reload();
                clearNewPannel();
            }
            ImGui::Spacing();

            if (custom_type==2) {

                bool valid_ = false;
                static std::string url_;
                static std::string ip_ = Settings::application.recentSRT.hosts.empty() ? Settings::application.recentSRT.default_host.first : Settings::application.recentSRT.hosts.front().first;
                static std::string port_ = Settings::application.recentSRT.hosts.empty() ? Settings::application.recentSRT.default_host.second : Settings::application.recentSRT.hosts.front().second;
                static std::regex ipv4("(([0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])\\.){3}([0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])");
                static std::regex numport("([0-9]){4,6}");

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

            if (custom_type==1) {

                std::string current_screen;
                if (new_source_preview_.filled())
                    current_screen = new_source_preview_.label();
                else
                    current_screen = "Select";

                ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                if (ImGui::BeginCombo("##ScreenCaptureSelect", current_screen.c_str(), ImGuiComboFlags_HeightLarge))
                {
                    for (int d = 0; d < ScreenCapture::manager().numWindow(); ++d){
                        std::string namewin = ScreenCapture::manager().name(d);
                        if (ImGui::Selectable( namewin.c_str() )) {
                            new_source_preview_.setSource( Mixer::manager().createSourceScreen(namewin), namewin);
                        }
                    }
                    ImGui::EndCombo();
                }
                // indication
                ImGui::SameLine();
                ImGui::SetCursorPosX(pos.x);
                ImGui::Text("Window");
                if (ImGui::IsItemHovered())
                    ImGuiToolkit::ToolTip("Create a source capturing the screen or other windows.\n"
                                          "The choice is limited by constraints of the operating system.");
            }

            if (custom_type==0) {

                std::string current_loopback;
                if (new_source_preview_.filled())
                    current_loopback = new_source_preview_.label();
                else
                    current_loopback = "Select";

                ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                if (ImGui::BeginCombo("##LoopbackSelect", current_loopback.c_str(), ImGuiComboFlags_HeightLarge))
                {
                    for (auto item = RenderSource::ProvenanceMethod.cbegin(); item != RenderSource::ProvenanceMethod.cend(); ++item) {
                        if (ImGuiToolkit::SelectableIcon(std::get<0>(*item),
                                                        std::get<1>(*item ),
                                                        std::get<2>(*item).c_str(), false)) {
                            new_source_preview_.setSource( Mixer::manager().createSourceRender(
                                std::distance(RenderSource::ProvenanceMethod.cbegin(), item)), std::get<2>(*item));
                        }
                    }
                    ImGui::EndCombo();
                }
                // indication
                ImGui::SameLine();
                ImGui::SetCursorPosX(pos.x);
                ImGui::Text("Mode");
                if (ImGui::IsItemHovered())
                    ImGuiToolkit::ToolTip("Create a source capturing the vimix display output (loopback).\n"
                                          ICON_FA_CARET_RIGHT " Recursive: capture everything shown on the screen, including the loopback source itself.\n"
                                          ICON_FA_CARET_RIGHT " Entire scene: capture everything shown on the screen, excluding the loopback source.\n"
                                          ICON_FA_CARET_RIGHT " Local scene: capture the section of the scene behind the loopback source.\n"
                                          ICON_FA_CARET_RIGHT " Canvas: capture the section of the scene in the selected canvas.");

            }
        }
        else if (Settings::application.source.new_type == SOURCE_BUNDLE) {
            
            ImGui::Text("Bundle of sources");
            
            Source *source_hovered_ = nullptr;

            Session *session = Mixer::manager().session();
            SourceList sources = session->getDepthSortedList();

            const ImGuiStyle& style = ImGui::GetStyle();
            const ImVec2 list_size = ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN -2.f * style.WindowPadding.x,
                               7.f * (ImGui::GetTextLineHeightWithSpacing() + style.FramePadding.y ) + style.FramePadding.y);
            ImVec2 item_size = ImVec2( list_size.x -2.f * style.FramePadding.x, ImGui::GetTextLineHeightWithSpacing());
            item_size.x -= ImGui::GetTextLineHeight() + style.ItemSpacing.x ;
            item_size.x -= session->size() > 6 ? style.ScrollbarSize : 0.f;

            // display list
            ImVec2 pos_top = ImGui::GetCursorPos();
            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            if (ImGui::ListBoxHeader("##SourcesBundle", list_size) ) {

                // list sources of the session
                for (auto it = sources.begin(); it != sources.end(); ++it) {

                    Source *s = *it;

                    // unique ID for item (names can change)
                    ImGui::PushID( std::to_string(s->id()).c_str() );
                    float width = ImGui::GetContentRegionAvail().x;
                    std::string label = ImGuiToolkit::truncatedText(std::string(s->initials()) + " - " + s->name(), width);

                    // item to select
                    ImGui::BeginGroup();
                    if (ImGui::Selectable( label.c_str(), false,
                                        ImGuiSelectableFlags_None, item_size )) {

                    }
                    ImGui::SameLine();

                    if (Mixer::selection().contains(s)) {
                        if ( ImGuiToolkit::IconButton( ICON_VI_SELECT_REMOVE, "Select") )
                            Mixer::selection().remove(s);
                    }
                    else {
                        if ( ImGuiToolkit::IconButton( ICON_VI_SELECT_ADD, "Select") )
                            Mixer::selection().add(s);
                    }
                    ImGui::EndGroup();
                    ImGui::PopID();

                    // what item is hovered for tooltip
                    if (ImGui::IsItemHovered())
                        source_hovered_ = s;

                }

                ImGui::ListBoxFooter();
            }

            // test possibility to create bundle
            bool can_create_bundle = false;
            // Selection of multiple sources
            if (Mixer::manager().selection().size() > 1) {
                can_create_bundle = Mixer::manager().selectionCanBeGroupped();
                // Mixer::manager().unsetCurrentSource();
            }
            // Single source selected
            else if (Mixer::manager().selection().size() > 0){
                Mixer::manager().setCurrentSource(*Mixer::manager().selection().begin());
                // Check if the current source can be grouped
                bool is_bundle = Mixer::manager().currentSource()->icon() == glm::ivec2(ICON_VI_SOURCE_GROUP);
                bool is_clone = Mixer::manager().currentSource()->cloned() || Mixer::manager().currentSource()->icon() == glm::ivec2(ICON_VI_SOURCE_CLONE);
                can_create_bundle = !is_bundle && !is_clone;   
            }
            else 
                Mixer::manager().unsetCurrentSource();
            
            if (can_create_bundle) {
                // Indicator of the selection
                ImGui::NewLine();
                ImGuiToolkit::Icon(ICON_VI_SOURCE_GROUP);
                ImGui::SameLine();
                ImGui::Text("Bundle of %d source%c", 
                    (int) Mixer::manager().selection().size(), 
                    Mixer::manager().selection().size() > 1 ? 's' : ' ');

                // Validate button
                ImGui::NewLine();
                if (ImGui::Button( ICON_FA_CHECK "  Ok", ImVec2(pannel_width_ - padding_width_, 0)) ) {
                    Mixer::manager().groupSelection();
                    // close NEW pannel
                    UserInterface::manager().showPannel(  Mixer::manager().numSource() );
                }
            }

            // Right side of the bundle list icon : select all / none
            ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_top.y + style.ItemSpacing.y));
            if ( ImGuiToolkit::IconButton( ICON_VI_SELECT_ALL, "Select all ")) {
                Mixer::selection().set(sources);
            }
            ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_top.y + style.ItemSpacing.y + ImGui::GetFrameHeightWithSpacing()));
            if ( ImGuiToolkit::IconButton( ICON_VI_SELECT_VISIBLE, "Select visible")) {
                SourceList visible = joinClones ( visible_only(sources) );
                Mixer::selection().set(visible);          
            }            
            ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN + ImGui::GetFrameHeightWithSpacing(), pos_top.y + style.ItemSpacing.y));
            if ( ImGuiToolkit::IconButton( ICON_VI_CLEAR_LIST, "Clear selection")) {
                Mixer::manager().unsetCurrentSource();
                Mixer::selection().clear();                 
            }

            // help indicator bundle list
            pos_top.y += list_size.y;
            ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_top.y -  ImGui::GetFrameHeightWithSpacing()));
            ImGuiToolkit::HelpToolTip("Select one or multiple sources to be bundled.\n\n"
                                    ICON_FA_LAYER_GROUP "  The list is sorted by layer, from back to front. "
                                                        "A bundle can only regroup sources that are visible and consecutive in layer.\n"
                                    ICON_FA_CARET_RIGHT ICON_FA_CARET_RIGHT ICON_FA_CARET_RIGHT " Sources and their clones cannot be separated.\n"
                                    ICON_FA_CUBE  "  A bundle cannot be bundled on its own.\n"
                                );

        }

        // if a new source was added
        if (new_source_preview_.filled()) {
            ImGui::NewLine();
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
                UserInterface::manager().showPannel(  Mixer::manager().numSource() );
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
