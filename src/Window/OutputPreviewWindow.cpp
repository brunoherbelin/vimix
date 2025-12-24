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

#include <cstdint>
#include <thread>

// ImGui
#include "FrameGrabber.h"
#include "Toolkit/ImGuiToolkit.h"
#include "imgui_internal.h"

#include "defines.h"
#include "Log.h"
#include "Toolkit/SystemToolkit.h"
#include "Settings.h"
#include "Mixer.h"
#include "Recorder.h"
#include "Connection.h"
#include "Streamer.h"
#include "Loopback.h"
#include "VideoBroadcast.h"
#include "ShmdataBroadcast.h"
#include "FrameGrabbing.h"
#include "GPUVideoRecorder.h"

#include "UserInterfaceManager.h"
#include "OutputPreviewWindow.h"


OutputPreviewWindow::OutputPreviewWindow() : WorkspaceWindow("OutputPreview"),
    magnifying_glass(false)
{
    recordFolderDialog = new DialogToolkit::OpenFolderDialog("Recording Location");
}

void OutputPreviewWindow::setVisible(bool on)
{
    magnifying_glass = false;

    // restore workspace to show the window
    if (WorkspaceWindow::clear_workspace_enabled) {
        WorkspaceWindow::restoreWorkspace(on);
        // do not change status if ask to hide (consider user asked to toggle because the window was not visible)
        if (!on)  return;
    }

    if (Settings::application.widget.preview_view > 0 && Settings::application.widget.preview_view != Settings::application.current_view) {
        Settings::application.widget.preview_view = -1;
        on = true;
    }

    Settings::application.widget.preview = on;
}

bool OutputPreviewWindow::Visible() const
{
    return ( Settings::application.widget.preview
             && (Settings::application.widget.preview_view < 0 || Settings::application.widget.preview_view == Settings::application.current_view )
             );
}

void OutputPreviewWindow::Update()
{
    WorkspaceWindow::Update();

}

VideoRecorder *delayTrigger(VideoRecorder *g, std::chrono::milliseconds delay) {
    std::this_thread::sleep_for (delay);
    return g;
}

void OutputPreviewWindow::ToggleRecord(bool save_and_continue)
{
    if (Outputs::manager().enabled( FrameGrabber::GRABBER_VIDEO, FrameGrabber::GRABBER_GPU  ) ) {
        if (save_and_continue) {
#ifdef USE_GST_OPENGL_SYNC_HANDLER
            if ( Settings::application.render.gst_glmemory_context && 
                 GPUVideoRecorder::hasProfile(Settings::application.record.profile) ) {
                Outputs::manager().chain(
                    new GPUVideoRecorder(SystemToolkit::base_filename( Mixer::manager().session()->filename()))
                );
            } else
#endif
            {
                Outputs::manager().chain(
                    new VideoRecorder(SystemToolkit::base_filename( Mixer::manager().session()->filename()))
                );
            }
        } 
        else  
            Outputs::manager().stop( FrameGrabber::GRABBER_VIDEO, FrameGrabber::GRABBER_GPU  );
    }
    else {
        uint64_t timeout = Settings::application.record.timeout;
        if (timeout >= RECORD_MAX_TIMEOUT)
            timeout = 0;

#ifdef USE_GST_OPENGL_SYNC_HANDLER
        if ( Settings::application.render.gst_glmemory_context && 
             GPUVideoRecorder::hasProfile(Settings::application.record.profile) ) {
            Outputs::manager().start(
                new GPUVideoRecorder(SystemToolkit::base_filename( Mixer::manager().session()->filename())),
                std::chrono::seconds(Settings::application.record.delay), timeout
            );
        } else 
#endif
        {
            Outputs::manager().start(
                new VideoRecorder(SystemToolkit::base_filename( Mixer::manager().session()->filename())),
                std::chrono::seconds(Settings::application.record.delay), timeout
            );
        }
    }
}

void OutputPreviewWindow::ToggleRecordPause()
{
    if (Outputs::manager().enabled( FrameGrabber::GRABBER_VIDEO, FrameGrabber::GRABBER_GPU ) )
    {
        if (Outputs::manager().paused( FrameGrabber::GRABBER_VIDEO, FrameGrabber::GRABBER_GPU ))
            Outputs::manager().unpause( FrameGrabber::GRABBER_VIDEO, FrameGrabber::GRABBER_GPU );
        else
            Outputs::manager().pause( FrameGrabber::GRABBER_VIDEO, FrameGrabber::GRABBER_GPU );
    }
}

void OutputPreviewWindow::ToggleVideoBroadcast()
{
    if (Outputs::manager().enabled( FrameGrabber::GRABBER_BROADCAST ) )
        Outputs::manager().stop( FrameGrabber::GRABBER_BROADCAST );
    else {
        if (Settings::application.broadcast_port<1000)
            Settings::application.broadcast_port = BROADCAST_DEFAULT_PORT;
        Outputs::manager().start( new VideoBroadcast(Settings::application.broadcast_port));
    }
}

void OutputPreviewWindow::ToggleSharedMemory()
{
    if (Outputs::manager().enabled( FrameGrabber::GRABBER_SHM ) )
        Outputs::manager().stop( FrameGrabber::GRABBER_SHM );
    else {
        // find a folder to put the socket for shm
        std::string _shm_socket_file = Settings::application.shm_socket_path;
        if (_shm_socket_file.empty() || !SystemToolkit::file_exists(_shm_socket_file))
            _shm_socket_file = SystemToolkit::home_path();
        _shm_socket_file = SystemToolkit::full_filename(_shm_socket_file, ".shm_vimix" + std::to_string(Settings::application.instance_id));

        // create shhmdata broadcaster with method
        Outputs::manager().start( new ShmdataBroadcast( (ShmdataBroadcast::Method) Settings::application.shm_method, _shm_socket_file));
    }
}

bool OutputPreviewWindow::ToggleLoopbackCamera()
{
    bool need_initialization = false;
    if (Outputs::manager().enabled( FrameGrabber::GRABBER_LOOPBACK )) 
        Outputs::manager().stop( FrameGrabber::GRABBER_LOOPBACK );
    else {
        if (Settings::application.loopback_camera < 1)
            Settings::application.loopback_camera = LOOPBACK_DEFAULT_DEVICE;
        Settings::application.loopback_camera += Settings::application.instance_id;

        try {
            Outputs::manager().start( new Loopback(Settings::application.loopback_camera) );
        }
        catch (const std::runtime_error &e) {
            need_initialization = true;
            Log::Info("%s", e.what());
        }
    }
    return need_initialization;
}


void OutputPreviewWindow::Render()
{
    const ImGuiContext& g = *GImGui;
    bool openInitializeSystemLoopback = false;

    FrameBuffer *output = Mixer::manager().session()->frame();
    if (output)
    {
        // constraint aspect ratio resizing
        float ar = output->aspectRatio();
        ImGui::SetNextWindowSizeConstraints(ImVec2(300, 200), ImVec2(FLT_MAX, FLT_MAX), ImGuiToolkit::CustomConstraints::AspectRatio, &ar);
        ImGui::SetNextWindowPos(ImVec2(1180, 20), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(400, 260), ImGuiCond_FirstUseEver);

        // Window named "OutputPreview" at instanciation
        if ( !ImGui::Begin(name_, &Settings::application.widget.preview,
                           ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse ) )
        {
            ImGui::End();
            return;
        }

        // return from thread for folder openning
        if (recordFolderDialog->closed() && !recordFolderDialog->path().empty())
            // get the folder from this file dialog
            Settings::application.record.path = recordFolderDialog->path();

        // menu (no title bar)
        if (ImGui::BeginMenuBar())
        {
            if (ImGuiToolkit::IconButton(4,16))
                Settings::application.widget.preview = false;

            if (ImGui::BeginMenu(IMGUI_TITLE_PREVIEW))
            {
                // Preview and output menu
                if (ImGuiToolkit::MenuItemIcon(ICON_PREVIEW, MENU_PREVIEW, SHORTCUT_PREVIEW_OUT) )
                    UserInterface::manager().show_preview = UserInterface::PREVIEW_OUTPUT;
                ImGui::MenuItem( MENU_OUTPUTDISABLE, SHORTCUT_OUTPUTDISABLE, &Settings::application.render.disabled);

                // Display window manager menu
                ImGui::Separator();
                bool pinned = Settings::application.widget.preview_view == Settings::application.current_view;
                std::string menutext = std::string( ICON_FA_MAP_PIN "    Stick to ") + Settings::application.views[Settings::application.current_view].name + " view";
                if ( ImGui::MenuItem( menutext.c_str(), nullptr, &pinned) ){
                    if (pinned)
                        Settings::application.widget.preview_view = Settings::application.current_view;
                    else
                        Settings::application.widget.preview_view = -1;
                }
                if ( ImGui::MenuItem( MENU_CLOSE, SHORTCUT_OUTPUT) )
                    Settings::application.widget.preview = false;

                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu( ICON_FA_ARROW_ALT_CIRCLE_DOWN " Capture"))
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_CAPTURE, 0.8f));
                if ( ImGui::MenuItem( MENU_CAPTUREFRAME, SHORTCUT_CAPTURE_DISPLAY) ) {
                    Outputs::manager().start(new PNGRecorder(SystemToolkit::base_filename( Mixer::manager().session()->filename())));
                }
                ImGui::PopStyleColor(1);

                // temporary disabled
                if (Outputs::manager().pending(FrameGrabber::GRABBER_VIDEO, FrameGrabber::GRABBER_GPU)) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_RECORD, 0.8f));
                    ImGui::MenuItem( MENU_RECORD, SHORTCUT_RECORD, false, false);
                    ImGui::MenuItem( MENU_RECORDPAUSE, SHORTCUT_RECORDPAUSE, false, false);
                    ImGui::MenuItem( MENU_RECORDCONT, SHORTCUT_RECORDCONT, false, false);
                    ImGui::PopStyleColor(1);
                }
                // Stop recording menu (recorder already exists)
                else if (Outputs::manager().enabled(FrameGrabber::GRABBER_VIDEO, FrameGrabber::GRABBER_GPU)) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_RECORD, 0.8f));                    
                    if ( ImGui::MenuItem( ICON_FA_SQUARE "  Stop Record", SHORTCUT_RECORD) ) {
                        // prepare for next time user open new source panel to show the recording
                        if (Settings::application.recentRecordings.load_at_start)
                            UserInterface::manager().navigator.setNewMedia(Navigator::MEDIA_RECORDING);
                        // stop recorder
                        Outputs::manager().stop(FrameGrabber::GRABBER_VIDEO, FrameGrabber::GRABBER_GPU);
                    }
                    // offer the Pause recording
                    if (Outputs::manager().paused( FrameGrabber::GRABBER_VIDEO, FrameGrabber::GRABBER_GPU )) {
                        if (ImGui::MenuItem(ICON_FA_PAUSE_CIRCLE "  Resume Record", SHORTCUT_RECORDPAUSE))
                            Outputs::manager().unpause(FrameGrabber::GRABBER_VIDEO, FrameGrabber::GRABBER_GPU);
                    } else {
                        if (ImGui::MenuItem(MENU_RECORDPAUSE, SHORTCUT_RECORDPAUSE))
                            Outputs::manager().pause(FrameGrabber::GRABBER_VIDEO, FrameGrabber::GRABBER_GPU);
                    }
                    // offer the 'save & continue' recording
                    if ( ImGui::MenuItem( MENU_RECORDCONT, SHORTCUT_RECORDCONT) ) {
                        // prepare for next time user open new source panel to show the recording
                        if (Settings::application.recentRecordings.load_at_start)
                            UserInterface::manager().navigator.setNewMedia(Navigator::MEDIA_RECORDING);
                        // create a new recorder chainned to the current one
                        ToggleRecord(true);
                    }
                    ImGui::PopStyleColor(1);
                }
                // start recording
                else {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_RECORD, 0.9f));
                    if ( ImGui::MenuItem( MENU_RECORD, SHORTCUT_RECORD) ) {
                        ToggleRecord(false);
                    }
                    ImGui::MenuItem( MENU_RECORDPAUSE, SHORTCUT_RECORDPAUSE, false, false);
                    ImGui::MenuItem( MENU_RECORDCONT, SHORTCUT_RECORDCONT, false, false);
                    ImGui::PopStyleColor(1);
                }

                // Options menu if not recording
                ImGui::Separator();
                if (Outputs::manager().enabled(FrameGrabber::GRABBER_VIDEO, FrameGrabber::GRABBER_GPU)) {
                    ImGui::MenuItem(Outputs::manager().info(true, FrameGrabber::GRABBER_VIDEO, FrameGrabber::GRABBER_GPU).c_str(), nullptr, false, false);
                }
                else {
                    ImGui::MenuItem("Settings", nullptr, false, false);
                    float combo_width = ImGui::GetTextLineHeightWithSpacing() * 7.f;

                    // offer to open config panel from here for more options
                    ImGui::SameLine(combo_width, IMGUI_SAME_LINE);
                    if (ImGuiToolkit::IconButton(13, 5, "Settings")) {
                        Settings::application.pannel_settings[0] = true;
                        UserInterface::manager().navigator.showConfig();
                    }

                    // BASIC OPTIONS
                    static char* name_path[4] = { nullptr };
                    if ( name_path[0] == nullptr ) {
                        for (int i = 0; i < 4; ++i)
                            name_path[i] = (char *) malloc( 1024 * sizeof(char));
                        snprintf( name_path[1], 1024, "%s", ICON_FA_HOME " Home");
                        snprintf( name_path[2], 1024, "%s", ICON_FA_FOLDER " Session location");
                        snprintf( name_path[3], 1024, "%s", ICON_FA_FOLDER_PLUS " Select");
                    }
                    if (Settings::application.record.path.empty())
                        Settings::application.record.path = SystemToolkit::home_path();
                    snprintf( name_path[0], 1024, "%s", Settings::application.record.path.c_str());
                    int selected_path = 0;
                    ImGui::SetNextItemWidth(combo_width);
                    if (ImGui::Combo("##Path", &selected_path, name_path, 4) ) {
                        if (selected_path > 2)
                            recordFolderDialog->open();
                        else if (selected_path > 1)
                            Settings::application.record.path = SystemToolkit::path_filename( Mixer::manager().session()->filename() );
                        else if (selected_path > 0)
                            Settings::application.record.path = SystemToolkit::home_path();
                    }
                    ImGui::SameLine(0, IMGUI_SAME_LINE);
                    if (ImGuiToolkit::TextButton("Path"))
                        Settings::application.record.path = SystemToolkit::home_path();

                    // offer to open folder location
                    ImVec2 draw_pos = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(draw_pos + ImVec2(combo_width + 3.f * ImGui::GetTextLineHeight(), -ImGui::GetFrameHeight()) );
                    if (ImGuiToolkit::IconButton(3, 5, "Show in finder"))
                        SystemToolkit::open(Settings::application.record.path);
                    ImGui::SetCursorPos(draw_pos);

                    // Naming menu selection
                    static const char* naming_style[2] = { ICON_FA_SORT_NUMERIC_DOWN "  Sequential", ICON_FA_CALENDAR "  Date prefix" };
                    ImGui::SetNextItemWidth(combo_width);
                    ImGui::Combo("##Filename", &Settings::application.record.naming_mode, naming_style, IM_ARRAYSIZE(naming_style));
                    ImGui::SameLine(0, IMGUI_SAME_LINE);
                    if (ImGuiToolkit::TextButton("Filename"))
                        Settings::application.record.naming_mode = 1;

                    ImGui::SetNextItemWidth(combo_width);
                    ImGuiToolkit::SliderTiming ("##Duration", &Settings::application.record.timeout, 1000, RECORD_MAX_TIMEOUT, 1000, "Until stopped");
                    ImGui::SameLine(0, IMGUI_SAME_LINE);
                    if (ImGuiToolkit::TextButton("Duration"))
                        Settings::application.record.timeout = RECORD_MAX_TIMEOUT;

                    ImGui::SetNextItemWidth(combo_width);
                    ImGui::SliderInt("##Trigger", &Settings::application.record.delay, 0, 5,
                                     Settings::application.record.delay < 1 ? "Immediate" : "After %d s");
                    ImGui::SameLine(0, IMGUI_SAME_LINE);
                    if (ImGuiToolkit::TextButton("Trigger"))
                        Settings::application.record.delay = 0;
                }

                ImGui::EndMenu();
            }
            if (ImGuiToolkit::BeginMenuIcon(19, 11, "Stream"))
            {
                // Stream sharing menu
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_STREAM, 0.9f));
                if ( ImGui::MenuItem( ICON_FA_SHARE_ALT_SQUARE "   P2P Peer-to-peer sharing", NULL, &Settings::application.accept_connections) ) {
                    Streaming::manager().enable(Settings::application.accept_connections);
                }
                ImGui::PopStyleColor(1);

                // list active streams:
                std::vector<std::string> ls = Streaming::manager().listStreams();

                // Broadcasting menu
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_BROADCAST, 0.9f));
                if (VideoBroadcast::available()) {
                    if ( ImGui::MenuItem( ICON_FA_GLOBE "   SRT Broadcast", NULL, Outputs::manager().enabled( FrameGrabber::GRABBER_BROADCAST ) ) )
                        ToggleVideoBroadcast();
                }

                // Shared Memory menu
                if (ShmdataBroadcast::available()) {
                    if ( ImGui::MenuItem( ICON_FA_MEMORY "  SHM Shared Memory", NULL, Outputs::manager().enabled( FrameGrabber::GRABBER_SHM ) ) )
                        ToggleSharedMemory();
                }

                // Loopback camera menu
                if (Loopback::available()) {
                    if ( ImGui::MenuItem( ICON_FA_VIDEO "  Loopback Camera", NULL, Outputs::manager().enabled( FrameGrabber::GRABBER_LOOPBACK )) )
                        openInitializeSystemLoopback = ToggleLoopbackCamera();
                }

                ImGui::PopStyleColor(1);

                // Display list of active stream
                if (ls.size()>0 || Outputs::manager().enabled(
                        FrameGrabber::GRABBER_BROADCAST,
                        FrameGrabber::GRABBER_SHM,
                        FrameGrabber::GRABBER_LOOPBACK )) {
                    ImGui::Separator();
                    ImGui::MenuItem("Active streams:", nullptr, false, false);

                    // First the list of peer 2 peer
                    for (auto it = ls.begin(); it != ls.end(); ++it)
                        ImGui::Text(" %s ", (*it).c_str() );

                    // SRT broadcast description
                    if (Outputs::manager().enabled( FrameGrabber::GRABBER_BROADCAST )) {
                        ImGui::Text(" %s        ", Outputs::manager().info(false, FrameGrabber::GRABBER_BROADCAST ).c_str());
                        // copy text icon to give user the srt link to connect to
                        ImVec2 draw_pos = ImGui::GetCursorPos();
                        ImGui::SetCursorPos(draw_pos + ImVec2(ImGui::GetContentRegionAvailWidth() - 1.2 * ImGui::GetTextLineHeightWithSpacing(), -0.8 * ImGui::GetFrameHeight()) );
                        std::string moreinfo = Outputs::manager().info(true, FrameGrabber::GRABBER_BROADCAST);
                        if (ImGuiToolkit::IconButton( ICON_FA_COPY, moreinfo.c_str()))
                            ImGui::SetClipboardText(moreinfo.c_str());
                        ImGui::SetCursorPos(draw_pos);
                    }

                    // Shared memory broadcast description
                    if (Outputs::manager().enabled( FrameGrabber::GRABBER_SHM )) {
                        ImGui::Text(" %s        ", Outputs::manager().info(false, FrameGrabber::GRABBER_SHM ).c_str());
                        // copy text icon to give user the socket path to connect to
                        ImVec2 draw_pos = ImGui::GetCursorPos();
                        ImGui::SetCursorPos(draw_pos + ImVec2(ImGui::GetContentRegionAvailWidth() - 1.2 * ImGui::GetTextLineHeightWithSpacing(), -0.8 * ImGui::GetFrameHeight()) );
                        std::string moreinfo = Outputs::manager().info(true, FrameGrabber::GRABBER_SHM);
                        if (ImGuiToolkit::IconButton( ICON_FA_COPY, moreinfo.c_str()))
                            ImGui::SetClipboardText(moreinfo.c_str());
                        ImGui::SetCursorPos(draw_pos);
                    }

                    // Loopback camera description
                    if (Outputs::manager().enabled( FrameGrabber::GRABBER_LOOPBACK )) {
                        ImGui::Text(" %s        ", Outputs::manager().info(false, FrameGrabber::GRABBER_LOOPBACK ).c_str());
                        // copy text icon to give user the device path to connect to
                        ImVec2 draw_pos = ImGui::GetCursorPos();
                        ImGui::SetCursorPos(draw_pos + ImVec2(ImGui::GetContentRegionAvailWidth() - 1.2 * ImGui::GetTextLineHeightWithSpacing(), -0.8 * ImGui::GetFrameHeight()) );
                        std::string moreinfo = Outputs::manager().info(true, FrameGrabber::GRABBER_LOOPBACK);
                        if (ImGuiToolkit::IconButton( ICON_FA_COPY, moreinfo.c_str()))
                            ImGui::SetClipboardText(moreinfo.c_str());
                        ImGui::SetCursorPos(draw_pos);
                    }
                }
                else {
                    ImGui::Separator();
                    ImGui::MenuItem("No active streams", nullptr, false, false);
                }

                ImGui::EndMenu();
            }

            // button to activate the magnifying glass at top right corner
            ImVec2 p = g.CurrentWindow->Pos;
            p.x += g.CurrentWindow->Size.x - 2.1f * g.FontSize;
            if (g.CurrentWindow->DC.CursorPos.x < p.x)
            {
                ImGui::SetCursorScreenPos(p);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
                ImGuiToolkit::ButtonToggle( ICON_FA_SEARCH, &magnifying_glass);
                ImGui::PopStyleColor();
            }

            ImGui::EndMenuBar();
        }

        // image takes the available window area
        ImVec2 imagesize = ImGui::GetContentRegionAvail();
        // image height respects original aspect ratio but is at most the available window height
        imagesize.y = MIN( imagesize.x / ar, imagesize.y);
        // image respects original aspect ratio
        imagesize.x = imagesize.y * ar;

        // virtual button to show the output window when clic on the preview
        ImVec2 draw_pos = ImGui::GetCursorScreenPos();
        // 100% opacity for the image (ensures true colors)
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.f);
        ImGui::Image((void*)(intptr_t)output->texture(), imagesize);
        ImGui::PopStyleVar();

        // disable magnifying glass if window is deactivated
        if (g.NavWindow != g.CurrentWindow)
            magnifying_glass = false;

        // handle mouse clic and hovering on image
        const ImRect bb(draw_pos, draw_pos + imagesize);
        const ImGuiID id = ImGui::GetCurrentWindow()->GetID("##output-texture");
        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held,
                                             ImGuiButtonFlags_PressedOnClick);
        // toggle preview on double clic
        if (pressed) {
            // trigger to show preview (will be ignored if not double clic)
            UserInterface::manager().show_preview = UserInterface::PREVIEW_OUTPUT;
            // cancel the button behavior to let window move on drag
            ImGui::SetActiveID(0, ImGui::GetCurrentWindow());
            ImGui::SetHoveredID(0);
        }
        // show magnifying glass if active and mouse hovering
        else if (hovered && magnifying_glass)
            DrawInspector(output->texture(), imagesize, imagesize, draw_pos);

        ///
        /// Icons overlays
        ///
        const float r = ImGui::GetTextLineHeightWithSpacing();

        // info indicator
        bool drawoverlay = false;
        if (!magnifying_glass) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.f, 0.f, 0.8f));
            ImGui::SetCursorScreenPos(draw_pos + ImVec2(imagesize.x - r, 6));
            ImGui::Text(ICON_FA_CIRCLE);
            ImGui::PopStyleColor(1);
            ImGui::SetCursorScreenPos(draw_pos + ImVec2(imagesize.x - r, 6));
            ImGui::Text(ICON_FA_INFO_CIRCLE);
            drawoverlay = ImGui::IsItemHovered();
        }

        // icon indicators
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        // recording indicator
        if (Outputs::manager().enabled( FrameGrabber::GRABBER_VIDEO, FrameGrabber::GRABBER_GPU ))
        {
            ImGui::SetCursorScreenPos(ImVec2(draw_pos.x + r, draw_pos.y + r));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_RECORD, 0.8f));
            ImGui::Text(ICON_FA_CIRCLE " %s", Outputs::manager().info(false, FrameGrabber::GRABBER_VIDEO, FrameGrabber::GRABBER_GPU ).c_str() );
            ImGui::PopStyleColor(1);
        }
        else if (Outputs::manager().pending( FrameGrabber::GRABBER_VIDEO, FrameGrabber::GRABBER_GPU ))
        {
            ImGui::SetCursorScreenPos(ImVec2(draw_pos.x + r, draw_pos.y + r));
            static double anim = 0.f;
            double a = sin(anim+=0.104); // 2 * pi / 60fps
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_RECORD, a));
            ImGui::Text(ICON_FA_CIRCLE);
            ImGui::PopStyleColor(1);
        }
        // broadcast indicator
        float vertical = r;
        if (Outputs::manager().enabled( FrameGrabber::GRABBER_BROADCAST ))
        {
            ImGui::SetCursorScreenPos(ImVec2(draw_pos.x + imagesize.x - 2.5f * r, draw_pos.y + vertical));
            if (Outputs::manager().busy( FrameGrabber::GRABBER_BROADCAST ))
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_BROADCAST, 0.8f));
            else
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_BROADCAST, 0.4f));
            ImGui::Text(ICON_FA_GLOBE);
            ImGui::PopStyleColor(1);
            vertical += 2.f * r;
        }
        // shmdata indicator
        if (Outputs::manager().enabled( FrameGrabber::GRABBER_SHM ))
        {
            ImGui::SetCursorScreenPos(ImVec2(draw_pos.x + imagesize.x - 2.5f * r, draw_pos.y + vertical));
            if (Outputs::manager().busy( FrameGrabber::GRABBER_SHM ))
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_BROADCAST, 0.8f));
            else
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_BROADCAST, 0.4f));
            ImGui::Text(ICON_FA_MEMORY);
            ImGui::PopStyleColor(1);
            vertical += 2.f * r;
        }
        // loopback camera indicator
        if (Outputs::manager().enabled( FrameGrabber::GRABBER_LOOPBACK ))
        {
            ImGui::SetCursorScreenPos(ImVec2(draw_pos.x + imagesize.x - 2.5f * r, draw_pos.y + vertical));
            if (Outputs::manager().busy( FrameGrabber::GRABBER_LOOPBACK ))
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_BROADCAST, 0.8f));
            else
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_BROADCAST, 0.4f));
            ImGui::Text(ICON_FA_VIDEO);
            ImGui::PopStyleColor(1);
        }
        // streaming indicator
        if (Settings::application.accept_connections)
        {
            ImGui::SetCursorScreenPos(ImVec2(draw_pos.x + imagesize.x - 2.4f * r, draw_pos.y + imagesize.y - 2.f*r));
            if ( Streaming::manager().busy())
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_STREAM, 0.8f));
            else
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_STREAM, 0.4f));
            ImGui::Text(ICON_FA_SHARE_ALT_SQUARE);
            ImGui::PopStyleColor(1);
        }
        // OUTPUT DISABLED indicator
        if (Settings::application.render.disabled)
        {
            ImGui::SetCursorScreenPos(ImVec2(draw_pos.x + r, draw_pos.y + imagesize.y - 2.f*r));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(COLOR_WINDOW, 0.8f));
            ImGui::Text(ICON_FA_EYE_SLASH);
            ImGui::PopStyleColor(1);
        }
        ImGui::PopFont();


        ///
        /// Info overlay over image
        ///
        if (drawoverlay)
        {
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            float h = 1.f;
            h += (Settings::application.accept_connections ? 1.f : 0.f);
            draw_list->AddRectFilled(draw_pos,  ImVec2(draw_pos.x + imagesize.x, draw_pos.y + h * r), IMGUI_COLOR_OVERLAY);
            ImGui::SetCursorScreenPos(draw_pos);
            ImGui::Text(" " ICON_FA_DESKTOP "  %d x %d px, %.d fps", output->width(), output->height(), int(Mixer::manager().fps()) );
            if (Settings::application.accept_connections)
                ImGui::Text( "  " ICON_FA_SHARE_ALT_SQUARE "   Available as %s (%ld peer connected)",
                             Connection::manager().info().name.c_str(),
                             Streaming::manager().listStreams().size() );
        }

        ImGui::End();
    }

    // Dialog to explain to the user how to initialize the loopback on the system
    if (openInitializeSystemLoopback && !ImGui::IsPopupOpen("Initialize System Loopback"))
        ImGui::OpenPopup("Initialize System Loopback");
    if (ImGui::BeginPopupModal("Initialize System Loopback", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
#if defined(LINUX)
        int w = 600;
        ImGui::Text("In order to enable the video4linux camera loopback,\n"
                    "'v4l2loopack' has to be installed and initialized on your machine");
        ImGui::Spacing();
        ImGuiToolkit::ButtonOpenUrl("More information online on v4l2loopback", "https://github.com/umlaeute/v4l2loopback");
        ImGui::Spacing();
        ImGui::Text("To do so, the following commands should be executed\n(with admin rights):");

        static char dummy_str[512];
        snprintf(dummy_str, 512, "sudo apt install v4l2loopback-dkms");
        ImGui::NewLine();
        ImGui::Text("Install v4l2loopack (only once, and reboot):");
        ImGui::SetNextItemWidth(600-40);
        ImGui::InputText("##cmd1", dummy_str, IM_ARRAYSIZE(dummy_str), ImGuiInputTextFlags_ReadOnly);
        ImGui::SameLine();
        ImGui::PushID(358794);
        if ( ImGuiToolkit::IconButton(ICON_FA_COPY, "Copy to clipboard") )
            ImGui::SetClipboardText(dummy_str);
        ImGui::PopID();

        snprintf(dummy_str, 512, "sudo modprobe v4l2loopback exclusive_caps=1 video_nr=%d"
                           " card_label=\"vimix loopback\"" , Settings::application.loopback_camera);
        ImGui::NewLine();
        ImGui::Text("Initialize v4l2loopack:");
        ImGui::SetNextItemWidth(600-40);
        ImGui::InputText("##cmd2", dummy_str, IM_ARRAYSIZE(dummy_str), ImGuiInputTextFlags_ReadOnly);
        ImGui::SameLine();
        ImGui::PushID(899872);
        if ( ImGuiToolkit::IconButton(ICON_FA_COPY, "Copy to clipboard") )
            ImGui::SetClipboardText(dummy_str);
        ImGui::PopID();


        ImGui::NewLine();
        ImGui::SetItemDefaultFocus();
        if (ImGui::Button("Ok, I'll do this in a terminal and try again later.", ImVec2(w, 0))
                || ImGui::IsKeyPressedMap(ImGuiKey_Enter) || ImGui::IsKeyPressedMap(ImGuiKey_KeyPadEnter) ) {
            ImGui::CloseCurrentPopup();
        }

#endif
        ImGui::EndPopup();
    }
}


