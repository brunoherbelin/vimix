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

#include <GLFW/glfw3.h>

#include "NavigatorInternal.h"

#include "IconsFontAwesome5.h"
#include "defines.h"
#include "Settings.h"
#include "Log.h"
#include "Audio.h"
#include "Mixer.h"
#include "Recorder.h"
#include "FrameGrabbing.h"
#include "ControlManager.h"
#include "RenderingManager.h"
#include "VideoBroadcast.h"
#include "ShmdataBroadcast.h"
#include "Toolkit/BaseToolkit.h"
#include "Toolkit/SystemToolkit.h"
#include "Toolkit/NetworkToolkit.h"
#include "Toolkit/DialogToolkit.h"
#include "Toolkit/GstToolkit.h"
#include "Toolkit/ImGuiToolkit.h"
#include "UserInterfaceManager.h"

#include "NavigatorCodec.h"
#include "SettingsPanel.h"

void SettingsPanel::Render()
{
    ImGuiContext& g = *GImGui;
    float align_x = g.FontSize + g.Style.FramePadding.x * 3;

    //
    // save settings
    //
    ImVec2 pos = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(pannel_width_ IMGUI_RIGHT_ALIGN, pos.y - 1.6 * ImGui::GetTextLineHeight()));
    if (ImGui::BeginMenu("Config")) {
        UserInterface::manager().showMenuConfig();
        ImGui::EndMenu();
    }
    ImGui::SetCursorPos(pos);

    //
    // Appearance 
    //
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.f,0.f,0.f,0.f));
    Settings::application.pannel_settings[5] = ImGui::CollapsingHeader("Appearance",
                                                                       Settings::application.pannel_settings[5] ? ImGuiTreeNodeFlags_DefaultOpen : 0);
    ImGui::PopStyleColor(1);
    if (Settings::application.pannel_settings[5]){
        //
        // Appearance
        //
        bool changed = false;
        int color = Settings::application.accent_color;

        // colored button to select accent color
        ImGui::SetCursorPosX(align_x);
        ImGui::PushStyleColor(ImGuiCol_Button, g.Style.Colors[ImGuiCol_Header]);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, g.Style.Colors[ImGuiCol_HeaderHovered]);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, g.Style.Colors[ImGuiCol_HeaderActive]);
        if (ImGui::Button("    ", ImVec2(pannel_width_ -align_x - g.Style.ItemSpacing.x + IMGUI_RIGHT_ALIGN, 0)) ) {
            color = (color+1)%3;
            changed = true;
        }
        ImGui::PopStyleColor(3);
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        if (ImGuiToolkit::TextButton("Color")) {
            color = 0;
            changed = true;
        }
        // change accent color; outside of if(Button) to escape Push/Pop mismatch
        if (changed) {
            Settings::application.accent_color = color;
            ImGuiToolkit::SetAccentColor(static_cast<ImGuiToolkit::accent_color>(Settings::application.accent_color));
            // ask Views to update
            View::need_deep_update_++;
        }
        
        // Scale of interface
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
    }

    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.f,0.f,0.f,0.f));
    //
    // Recording preferences
    //
    Settings::application.pannel_settings[0] = ImGui::CollapsingHeader("Recording",
                                                                       Settings::application.pannel_settings[0] ? ImGuiTreeNodeFlags_DefaultOpen : 0);

    if (Settings::application.pannel_settings[0]){

        // recording is done at the resolution of the session output
        glm::ivec2 output_resolution(0, 0);
        const FrameBuffer *output_frame = Mixer::manager().session()->frame();
        if (output_frame)
            output_resolution = glm::ivec2(output_frame->resolution());

        // select Encoder codec
        ImGui::SetCursorPosX(align_x);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ComboCodec("##Codec", &Settings::application.record.profile,
                   output_resolution.x, output_resolution.y);
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        if (ImGuiToolkit::TextButton("Codec")) {
            Settings::application.record.profile = GstToolkit::H264_RT;
            ValidateCodecResolution(&Settings::application.record.profile,
                                    output_resolution.x, output_resolution.y);
        }

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
            std::string _shm_socket_path = Settings::application.shm_socket_path;
            std::string _shm_socket_file = Settings::application.shm_socket_path;
            if (_shm_socket_path.empty() || !SystemToolkit::file_exists(_shm_socket_path))
                _shm_socket_path = SystemToolkit::temp_path();
            _shm_socket_file = SystemToolkit::full_filename(_shm_socket_path, "shm");

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
            char bufsocket[256] = "";
            snprintf(bufsocket, 256, "%s", _shm_socket_path.c_str());
            // disable edition if SHM output is enabled
            if (Outputs::manager().enabled( FrameGrabber::GRABBER_SHM )) {
                ImGui::InputText("##SHM path read", bufsocket, 256, ImGuiInputTextFlags_ReadOnly);
                if (ImGui::IsItemHovered())
                    ImGuiToolkit::ToolTip("Disable Shared Memory streaming to edit the path.");
                ImGui::SameLine(0, IMGUI_SAME_LINE);
                ImGui::Text("SHM path");
                if (ShmdataBroadcast::available(ShmdataBroadcast::SHM_SHMDATASINK)) {
                    ImGui::SetCursorPosX(align_x);
                    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                    snprintf(bufsocket, 256, "%s", Settings::application.shm_method == 0 ? "shmsink" : "shmdatasink");
                    ImGui::InputText("##SHM sink read", bufsocket, 256, ImGuiInputTextFlags_ReadOnly);
                    if (ImGui::IsItemHovered())
                        ImGuiToolkit::ToolTip("Disable Shared Memory streaming to edit the sink method.");
                    ImGui::SameLine(0, IMGUI_SAME_LINE);
                    ImGui::Text("SHM sink");
                }
            }
            // enable edition if SHM output is disabled
            else {
                ImGui::InputTextWithHint("##SHM path", SystemToolkit::temp_path().c_str(), bufsocket, 256);
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
        if ( ImGuiToolkit::IconButton(5, 15, "Reload") )
            Control::manager().init();
        ImGui::SameLine(0, 3);
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
        if ( ImGuiToolkit::IconButton(5, 15, "Reload") )
            Control::manager().loadGamepadMappings();
        ImGui::SameLine(0, 3);
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
        static bool glmemory = Settings::application.render.gst_glmemory_context;
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
        change |= ImGuiToolkit::ButtonSwitch( "Gst GLMemory context", &glmemory);
#endif
        change |= ImGuiToolkit::ButtonSwitch( "Vertical synchronization", &vsync);
        change |= ImGuiToolkit::ButtonSwitch( "Multisample antialiasing", &multi);
#endif
        if (change) {
            need_restart = ( vsync != (Settings::application.render.vsync > 0) ||
                    multi != (Settings::application.render.multisampling > 0) ||
                    gpu != Settings::application.render.gpu_decoding ||
                    glmemory != Settings::application.render.gst_glmemory_context ||
                    audio != Settings::application.accept_audio );
        }

        if (need_restart) {
            ImGuiToolkit::Spacing();
            if (ImGui::Button( ICON_FA_POWER_OFF "  Quit & restart to apply", ImVec2(ImGui::GetContentRegionAvail().x - 50, 0))) {
                Settings::application.render.vsync = vsync ? 1 : 0;
                Settings::application.render.multisampling = multi ? 3 : 0;
                Settings::application.render.gst_glmemory_context = glmemory;
                Settings::application.render.gpu_decoding = gpu;
                Settings::application.accept_audio = audio;
                if (UserInterface::manager().TryClose())
                    Rendering::manager().close();
            }
        }
    }

    ImGui::PopStyleColor(1);
}
