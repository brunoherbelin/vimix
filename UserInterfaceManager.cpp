/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2020-2021 Bruno Herbelin <bruno.herbelin@gmail.com>
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
#include <cstring>
#include <sstream>
#include <thread>
#include <algorithm>
#include <map>
#include <iomanip>
#include <fstream>
#include <sstream>

using namespace std;

// ImGui
#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// Desktop OpenGL function loader
#include <glad/glad.h>

// Include glfw3.h after our OpenGL definitions
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

// generic image loader
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "defines.h"
#include "Log.h"
#include "SystemToolkit.h"
#include "DialogToolkit.h"
#include "BaseToolkit.h"
#include "GlmToolkit.h"
#include "GstToolkit.h"
#include "ImGuiToolkit.h"
#include "ImGuiVisitor.h"
#include "RenderingManager.h"
#include "Connection.h"
#include "ActionManager.h"
#include "Resource.h"
#include "Settings.h"
#include "SessionCreator.h"
#include "Mixer.h"
#include "Recorder.h"
#include "Streamer.h"
#include "Loopback.h"
#include "Selection.h"
#include "FrameBuffer.h"
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "SessionSource.h"
#include "PatternSource.h"
#include "DeviceSource.h"
#include "NetworkSource.h"
#include "StreamSource.h"
#include "PickingVisitor.h"
#include "ImageShader.h"
#include "ImageProcessingShader.h"
#include "Metronome.h"

#include "TextEditor.h"
TextEditor editor;

#include "UserInterfaceManager.h"
#define PLOT_ARRAY_SIZE 180
#define LABEL_AUTO_MEDIA_PLAYER ICON_FA_CARET_SQUARE_RIGHT "  Dynamic selection"
#define LABEL_STORE_SELECTION "  Store selection"
#define LABEL_EDIT_FADING ICON_FA_RANDOM "  Fade in & out"

// utility functions
void ShowAboutGStreamer(bool* p_open);
void ShowAboutOpengl(bool* p_open);
void ShowSandbox(bool* p_open);
void SetMouseCursor(ImVec2 mousepos, View::Cursor c = View::Cursor());
void SetNextWindowVisible(ImVec2 pos, ImVec2 size, float margin = 180.f);

std::string readable_date_time_string(std::string date){
    if (date.length()<12)
        return "";
    std::string s = date.substr(6, 2) + "/" + date.substr(4, 2) + "/" + date.substr(0, 4);
    s += " @ " + date.substr(8, 2) + ":" + date.substr(10, 2);
    return s;
}

// globals
const std::chrono::milliseconds timeout = std::chrono::milliseconds(4);
std::vector< std::future<FrameGrabber *> > _video_recorders;
FrameGrabber *delayTrigger(FrameGrabber *g, std::chrono::milliseconds delay) {
    std::this_thread::sleep_for (delay);
    return g;
}

// Helper functions for imgui window aspect-ratio constraints
struct CustomConstraints
{
    static void AspectRatio(ImGuiSizeCallbackData* data) {
        float *ar = (float*) data->UserData;
        data->DesiredSize.y = (data->CurrentSize.x / (*ar)) + 35.f;
    }
    static void Square(ImGuiSizeCallbackData* data) {
        data->DesiredSize.x = data->DesiredSize.y = (data->DesiredSize.x > data->DesiredSize.y ? data->DesiredSize.x : data->DesiredSize.y);
    }

};

UserInterface::UserInterface()
{
    start_time = gst_util_get_timestamp ();
    ctrl_modifier_active = false;
    alt_modifier_active = false;
    shift_modifier_active = false;
    show_vimix_about = false;
    show_imgui_about = false;
    show_gst_about = false;
    show_opengl_about = false;
    show_view_navigator  = 0;
    target_view_navigator = 1;
    currentTextEdit.clear();
    screenshot_step = 0;

    // keep hold on frame grabbers
    video_recorder_ = nullptr;

#if defined(LINUX)
    webcam_emulator_ = nullptr;
#endif

    sessionopendialog = nullptr;
    sessionimportdialog = nullptr;
    sessionsavedialog = nullptr;
}

bool UserInterface::Init()
{
    if (Rendering::manager().mainWindow().window()== nullptr)
        return false;

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = Settings::application.scale;

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(Rendering::manager().mainWindow().window(), true);
    ImGui_ImplOpenGL3_Init(Rendering::manager().glsl_version.c_str());

    // Setup Dear ImGui style
    ImGuiToolkit::SetAccentColor(static_cast<ImGuiToolkit::accent_color>(Settings::application.accent_color));

    //  Estalish the base size from the resolution of the monitor
    float base_font_size =  float(Rendering::manager().mainWindow().pixelsforRealHeight(4.f))  ;
    base_font_size = CLAMP( base_font_size, 8.f, 50.f);
    // Load Fonts (using resource manager, NB: a temporary copy of the raw data is necessary)
    ImGuiToolkit::SetFont(ImGuiToolkit::FONT_DEFAULT, "Roboto-Regular", int(base_font_size) );
    ImGuiToolkit::SetFont(ImGuiToolkit::FONT_BOLD, "Roboto-Bold", int(base_font_size) );
    ImGuiToolkit::SetFont(ImGuiToolkit::FONT_ITALIC, "Roboto-Italic", int(base_font_size) );
    ImGuiToolkit::SetFont(ImGuiToolkit::FONT_MONO, "Hack-Regular", int(base_font_size) - 2);
    ImGuiToolkit::SetFont(ImGuiToolkit::FONT_LARGE, "Hack-Regular", MIN(int(base_font_size * 1.5f), 50), 1 );

    // info
//    Log::Info("Monitor (%.1f,%.1f)", Rendering::manager().monitorWidth(), Rendering::manager().monitorHeight());
    Log::Info("Font size %d", int(base_font_size) );

    // Style
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding.x = base_font_size / 2.5f;
    style.WindowPadding.y = style.WindowPadding.x / 2.f;
    style.FramePadding.x = base_font_size / 2.5f;
    style.FramePadding.y = style.FramePadding.x / 2.f;
    style.IndentSpacing = base_font_size;
    style.ItemSpacing.x = base_font_size / 2.f;
    style.ItemSpacing.y = style.ItemSpacing.x / 3.f;
    style.ItemInnerSpacing.x = base_font_size / 2.5f;
    style.ItemInnerSpacing.y = style.ItemInnerSpacing.x / 2.f;
    style.WindowRounding = base_font_size / 2.5f;
    style.ChildRounding = style.WindowRounding / 2.f;
    style.FrameRounding = style.WindowRounding / 2.f;
    style.PopupRounding = style.WindowRounding / 2.f;
    style.GrabRounding = style.FrameRounding / 2.f;
    style.GrabMinSize = base_font_size / 1.5f;
    style.Alpha = 0.92f;

    // prevent bug with imgui clipboard (null at start)
    ImGui::SetClipboardText("");

    // setup settings filename
    std::string inifile = SystemToolkit::full_filename(SystemToolkit::settings_path(), "imgui.ini");
    char *inifilepath = (char *) malloc( (inifile.size() + 1) * sizeof(char) );
    std::sprintf(inifilepath, "%s", inifile.c_str() );
    io.IniFilename = inifilepath;

    // init dialogs
    sessionopendialog   = new DialogToolkit::OpenSessionDialog("Open Session");
    sessionsavedialog   = new DialogToolkit::SaveSessionDialog("Save Session");
    sessionimportdialog = new DialogToolkit::OpenSessionDialog("Import Session") ;

    return true;
}

uint64_t UserInterface::Runtime() const
{
    return gst_util_get_timestamp () - start_time;
}

void UserInterface::handleKeyboard()
{
    const ImGuiIO& io = ImGui::GetIO();
    alt_modifier_active = io.KeyAlt;
    shift_modifier_active = io.KeyShift;
    bool ctrl = io.ConfigMacOSXBehaviors ? io.KeySuper : io.KeyCtrl;
    bool confirm_quit_popup = false;

    // Application "CTRL +"" Shortcuts
    if ( ctrl ) {

        ctrl_modifier_active = true;

        if (ImGui::IsKeyPressed( GLFW_KEY_Q ))  {
            // offer to Quit
            ImGui::OpenPopup("confirm_quit_popup");
            confirm_quit_popup = true;
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_O )) {
            // SHIFT + CTRL + O : reopen current session
            if (shift_modifier_active && !Mixer::manager().session()->filename().empty())
                Mixer::manager().load( Mixer::manager().session()->filename() );
            // CTRL + O : Open session
            else
                selectOpenFilename();
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_S )) {
            // SHIFT + CTRL + S : save as
            if (shift_modifier_active)
                selectSaveFilename();
            // CTRL + S : save (save as if necessary)
            else
                saveOrSaveAs();
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_W )) {
            // New Session
            Mixer::manager().close();
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_SPACE )) {
            // Suspend / unsuspend session
            Mixer::manager().session()->setActive( !Mixer::manager().session()->active() );
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_L )) {
            // Logs
            Settings::application.widget.logs = !Settings::application.widget.logs;
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_T )) {
            // Timers
            Settings::application.widget.timer = !Settings::application.widget.timer;
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_G )) {
            // Developer toolbox
            Settings::application.widget.toolbox = !Settings::application.widget.toolbox;
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_H )) {
            // Helper
            Settings::application.widget.help = !Settings::application.widget.help;
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_E )) {
            // Shader Editor
            Settings::application.widget.shader_editor = !Settings::application.widget.shader_editor;
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_D )) {
            // Display output
            Settings::application.widget.preview = !Settings::application.widget.preview;
            if (Settings::application.widget.preview_view != Settings::application.current_view) {
                Settings::application.widget.preview_view = -1;
                Settings::application.widget.preview = true;
            }
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_P )) {
            // Media player
            Settings::application.widget.media_player = !Settings::application.widget.media_player;
            if (Settings::application.widget.media_player_view != Settings::application.current_view) {
                Settings::application.widget.media_player_view = -1;
                Settings::application.widget.media_player = true;
            }
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_A )) {
            if (shift_modifier_active)
            {
                // clear selection
                Mixer::manager().unsetCurrentSource();
                Mixer::selection().clear();
            }
            else
                // select all
                Mixer::manager().view()->selectAll();
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_R )) {
            if (shift_modifier_active) {
                FrameGrabbing::manager().add(new PNGRecorder);
            }
            else {
                // toggle recording
                if (video_recorder_) {
                    // allow 'save & continue' for Ctrl+Alt+R if no timeout for recording
                    if (alt_modifier_active && Settings::application.record.timeout == RECORD_MAX_TIMEOUT) {
                        VideoRecorder *rec = new VideoRecorder;
                        FrameGrabbing::manager().chain(video_recorder_, rec);
                        video_recorder_ = rec;
                    }
                    // normal case: Ctrl+R stop recording
                    else
                        video_recorder_->stop();
                }
                else {
                    _video_recorders.emplace_back( std::async(std::launch::async, delayTrigger, new VideoRecorder,
                                                              std::chrono::seconds(Settings::application.record.delay)) );
                }
            }
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_Z )) {
            if (shift_modifier_active)
                Action::manager().redo();
            else
                Action::manager().undo();
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_C )) {
            std::string clipboard = Mixer::selection().clipboard();
            if (!clipboard.empty())
                ImGui::SetClipboardText(clipboard.c_str());
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_X )) {
            std::string clipboard = Mixer::selection().clipboard();
            if (!clipboard.empty()) {
                ImGui::SetClipboardText(clipboard.c_str());
                Mixer::manager().deleteSelection();
            }
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_V )) {
            auto clipboard = ImGui::GetClipboardText();
            if (clipboard != nullptr && strlen(clipboard) > 0)
                Mixer::manager().paste(clipboard);
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_F )) {
            if (shift_modifier_active)
                Rendering::manager().mainWindow().toggleFullscreen();
            else
                Rendering::manager().outputWindow().toggleFullscreen();
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_N ) && shift_modifier_active) {
            Mixer::manager().session()->addNote();
        }

    }
    // No CTRL modifier
    else {
        ctrl_modifier_active = false;
//        Source *_cs = Mixer::manager().currentSource();

        // Application F-Keys
        if (ImGui::IsKeyPressed( GLFW_KEY_F1 ))
            Mixer::manager().setView(View::MIXING);
        else if (ImGui::IsKeyPressed( GLFW_KEY_F2 ))
            Mixer::manager().setView(View::GEOMETRY);
        else if (ImGui::IsKeyPressed( GLFW_KEY_F3 ))
            Mixer::manager().setView(View::LAYER);
        else if (ImGui::IsKeyPressed( GLFW_KEY_F4 ))
            Mixer::manager().setView(View::TEXTURE);
        else if (ImGui::IsKeyPressed( GLFW_KEY_F12 ))
            StartScreenshot();
        // button home to toggle menu
        else if (ImGui::IsKeyPressed( GLFW_KEY_HOME ))
            navigator.togglePannelMenu();
        // button home to toggle menu
        else if (ImGui::IsKeyPressed( GLFW_KEY_INSERT ))
            navigator.togglePannelNew();
        // button esc
        else if (ImGui::IsKeyPressed( GLFW_KEY_ESCAPE )) {
            // 1. escape fullscreen
            if (Rendering::manager().mainWindow().isFullscreen())
                Rendering::manager().mainWindow().exitFullscreen();
            // 2. hide panel
            else if (navigator.pannelVisible())
                navigator.hidePannel();
            // 3. hide windows
            else if (Settings::application.widget.preview ||
                     Settings::application.widget.media_player ||
                     Settings::application.widget.timer ||
                     Settings::application.widget.logs)  {
                Settings::application.widget.preview = false;
                Settings::application.widget.media_player = false;
                Settings::application.widget.timer = false;
                Settings::application.widget.logs = false;
            }
            // 4. cancel selection
            else if (!Mixer::selection().empty()) {
                Mixer::manager().unsetCurrentSource();
                Mixer::selection().clear();
            }
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_END )) {
            Settings::application.render.disabled = !Settings::application.render.disabled;
        }

        // Space bar to toggle play / pause
        if (ImGui::IsKeyPressed( GLFW_KEY_SPACE ))
            sourcecontrol.Play();
        // B to rewind to Beginning
        else if (ImGui::IsKeyPressed( GLFW_KEY_B ))
            sourcecontrol.Replay();

        // normal keys in workspace // make sure no entry / window box is active        
        if ( !ImGui::IsAnyWindowFocused() )
        {
            // Backspace to delete source
            if (ImGui::IsKeyPressed( GLFW_KEY_BACKSPACE ) || ImGui::IsKeyPressed( GLFW_KEY_DELETE ))
                Mixer::manager().deleteSelection();
            // button tab to select next
            else if ( !alt_modifier_active && ImGui::IsKeyPressed( GLFW_KEY_TAB )) {
                if (shift_modifier_active)
                    Mixer::manager().setCurrentPrevious();
                else
                    Mixer::manager().setCurrentNext();
            }
            // arrow keys to act on current view
            else if (ImGui::IsKeyDown( GLFW_KEY_LEFT ))
                Mixer::manager().view()->arrow( glm::vec2(-1.f, 0.f) );
            else if (ImGui::IsKeyDown( GLFW_KEY_RIGHT ))
                Mixer::manager().view()->arrow( glm::vec2(+1.f, 0.f) );
            if (ImGui::IsKeyDown( GLFW_KEY_UP ))
                Mixer::manager().view()->arrow( glm::vec2(0.f, -1.f) );
            else if (ImGui::IsKeyDown( GLFW_KEY_DOWN ))
                Mixer::manager().view()->arrow( glm::vec2(0.f, +1.f) );

            if (ImGui::IsKeyReleased( GLFW_KEY_LEFT ) ||
                ImGui::IsKeyReleased( GLFW_KEY_RIGHT ) ||
                ImGui::IsKeyReleased( GLFW_KEY_UP ) ||
                ImGui::IsKeyReleased( GLFW_KEY_DOWN ) ){
                Mixer::manager().view()->terminate();
            }
        }
    }

    // special case: CTRL + TAB is ALT + TAB in OSX
    if (io.ConfigMacOSXBehaviors ? io.KeyAlt : io.KeyCtrl) {
        if (ImGui::IsKeyPressed( GLFW_KEY_TAB ))
            show_view_navigator += shift_modifier_active ? 3 : 1;
    }
    else if (show_view_navigator > 0) {
        show_view_navigator  = 0;
        Mixer::manager().setView((View::Mode) target_view_navigator);
    }

    // confirmation for leaving vimix: prevent un-wanted Ctrl+Q, but make it easy to confirm
    if (ImGui::BeginPopup("confirm_quit_popup"))
    {
        ImGui::Text(" Leave vimix? [Q to confirm]");
        // Clic Quit or press Q to confirm exit
        if (ImGui::Button( ICON_FA_POWER_OFF "  Quit  ", ImVec2(250,0)) ||
            ( !confirm_quit_popup && ImGui::IsKeyPressed( GLFW_KEY_Q )) )
            Rendering::manager().close();
        ImGui::EndPopup();
    }

}

void UserInterface::handleMouse()
{

    ImGuiIO& io = ImGui::GetIO();
    glm::vec2 mousepos(io.MousePos.x * io.DisplayFramebufferScale.x, io.MousePos.y * io.DisplayFramebufferScale.y);
    mousepos = glm::clamp(mousepos, glm::vec2(0.f), glm::vec2(io.DisplaySize.x * io.DisplayFramebufferScale.x, io.DisplaySize.y * io.DisplayFramebufferScale.y));

    static glm::vec2 mouse_smooth = mousepos;

    static glm::vec2 mouseclic[2];
    mouseclic[ImGuiMouseButton_Left] = glm::vec2(io.MouseClickedPos[ImGuiMouseButton_Left].x * io.DisplayFramebufferScale.y, io.MouseClickedPos[ImGuiMouseButton_Left].y* io.DisplayFramebufferScale.x);
    mouseclic[ImGuiMouseButton_Right] = glm::vec2(io.MouseClickedPos[ImGuiMouseButton_Right].x * io.DisplayFramebufferScale.y, io.MouseClickedPos[ImGuiMouseButton_Right].y* io.DisplayFramebufferScale.x);

    static bool mousedown = false;
    static View *view_drag = nullptr;
    static std::pair<Node *, glm::vec2> picked = { nullptr, glm::vec2(0.f) };

    // steal focus on right button clic
    if (!io.WantCaptureMouse)
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) /*|| ImGui::IsMouseClicked(ImGuiMouseButton_Middle)*/)
            ImGui::FocusWindow(NULL);

    //
    // Mouse over
    //
    {
        View::Cursor c = Mixer::manager().view()->over(mousepos);
        if (c.type > 0)
            SetMouseCursor(io.MousePos, c);
    }

    // if not on any window
    if ( !ImGui::IsAnyWindowHovered() && !ImGui::IsAnyWindowFocused() )
    {
        //
        // Mouse wheel over background
        //
        if ( io.MouseWheel != 0) {
            // scroll => zoom current view
            Mixer::manager().view()->zoom( io.MouseWheel );
        }
        // TODO : zoom with center on source if over current

        //
        // RIGHT Mouse button
        //
        if ( ImGui::IsMouseDragging(ImGuiMouseButton_Right, 10.0f) )
        {
            // right mouse drag => drag current view
            View::Cursor c = Mixer::manager().view()->drag( mouseclic[ImGuiMouseButton_Right], mousepos);
            SetMouseCursor(io.MousePos, c);
        }
        else if ( ImGui::IsMouseDown(ImGuiMouseButton_Right))
        {
            Mixer::manager().unsetCurrentSource();
            navigator.hidePannel();
//                glm::vec3 point = Rendering::manager().unProject(mousepos, Mixer::manager().currentView()->scene.root()->transform_ );
        }

        if ( ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Right) )
        {
            Mixer::manager().view()->recenter();
        }

        //
        // LEFT Mouse button
        //
        if ( ImGui::IsMouseDown(ImGuiMouseButton_Left) ) {

            if ( !mousedown )
            {
                mousedown = true;
                mouse_smooth = mousepos;

                // ask the view what was picked
                picked = Mixer::manager().view()->pick(mousepos);

                bool clear_selection = false;
                // if nothing picked,
                if ( picked.first == nullptr ) {
                    clear_selection = true;
                }
                // something was picked
                else {
                    // get if a source was picked
                    Source *s = Mixer::manager().findSource(picked.first);
                    if (s != nullptr)
                    {
                        // CTRL + clic = multiple selection
                        if (ctrl_modifier_active) {
                            if ( !Mixer::selection().contains(s) )
                                Mixer::selection().add( s );
                            else {
                                Mixer::selection().remove( s );
                                if ( Mixer::selection().size() > 1 )
                                    s = Mixer::selection().front();
                                else {
                                    s = nullptr;
                                }
                            }
                        }
                        // making the picked source the current one
                        if (s)
                            Mixer::manager().setCurrentSource( s );
                        else
                            Mixer::manager().unsetCurrentSource();
                        if (navigator.pannelVisible())
                            navigator.showPannelSource( Mixer::manager().indexCurrentSource() );

                        // indicate to view that an action can be initiated (e.g. grab)
                        Mixer::manager().view()->initiate();
                    }
                    // no source is selected
                    else
                        Mixer::manager().unsetCurrentSource();
                }
                if (clear_selection) {
                    // unset current
                    Mixer::manager().unsetCurrentSource();
                    navigator.hidePannel();
                    // clear selection
                    Mixer::selection().clear();
                }
            }
        }

        if ( ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) )
        {
            // double clic in Transition view means quit
            if (Mixer::manager().view() == Mixer::manager().view(View::TRANSITION)) {
                Mixer::manager().setView(View::MIXING);
            }
            // double clic in other views means toggle pannel
            else {
                if (navigator.pannelVisible())
                    // discard current to select front most source
                    // (because single clic maintains same source active)
                    Mixer::manager().unsetCurrentSource();
                // display source in left pannel
                navigator.showPannelSource( Mixer::manager().indexCurrentSource() );
            }
        }

//        if ( mousedown &&  glm::distance(mouseclic[ImGuiMouseButton_Left], mousepos) > 3.f )
        if ( ImGui::IsMouseDragging(ImGuiMouseButton_Left, 5.0f) )
        {
            if(view_drag == nullptr) {
                view_drag = Mixer::manager().view();

                // indicate to view that an action can be initiated (e.g. grab)
                Mixer::manager().view()->initiate();
            }

            // only operate if the view didn't change
            if (view_drag == Mixer::manager().view()) {

                if ( picked.first != nullptr ) {
                    // Smooth cursor
                    if (Settings::application.smooth_cursor) {
                        // TODO : physics implementation
                        float smoothing = 10.f / ( MAX(io.Framerate, 1.f) );
                        glm::vec2 d = mousepos - mouse_smooth;
                        mouse_smooth += smoothing * d;
                        ImVec2 start = ImVec2(mouse_smooth.x / io.DisplayFramebufferScale.x, mouse_smooth.y / io.DisplayFramebufferScale.y);
                        ImGui::GetBackgroundDrawList()->AddLine(io.MousePos, start, ImGui::GetColorU32(ImGuiCol_HeaderActive), 5.f);
                    }
                    else
                        mouse_smooth = mousepos;

                    // action on current source
                    Source *current = Mixer::manager().currentSource();
                    if (current)
                    {
                        if (!shift_modifier_active) {
                            // grab others from selection
                            for (auto it = Mixer::selection().begin(); it != Mixer::selection().end(); ++it) {
                                if ( *it != current )
                                    Mixer::manager().view()->grab(*it, mouseclic[ImGuiMouseButton_Left], mouse_smooth, picked);
                            }
                        }
                        // grab current sources
                        View::Cursor c = Mixer::manager().view()->grab(current, mouseclic[ImGuiMouseButton_Left], mouse_smooth, picked);
                        SetMouseCursor(io.MousePos, c);
                    }
                    // action on other (non-source) elements in the view
                    else
                    {
                        View::Cursor c = Mixer::manager().view()->grab(nullptr, mouseclic[ImGuiMouseButton_Left], mouse_smooth, picked);
                        SetMouseCursor(io.MousePos, c);
                    }
                }
                // Selection area
                else {
                    // highlight-colored selection rectangle
                    ImVec4 color = ImGuiToolkit::HighlightColor();
                    ImGui::GetBackgroundDrawList()->AddRect(io.MouseClickedPos[ImGuiMouseButton_Left], io.MousePos, ImGui::GetColorU32(color));
                    color.w = 0.12; // transparent
                    ImGui::GetBackgroundDrawList()->AddRectFilled(io.MouseClickedPos[ImGuiMouseButton_Left], io.MousePos, ImGui::GetColorU32(color));

                    // Bounding box multiple sources selection
                    Mixer::manager().view()->select(mouseclic[ImGuiMouseButton_Left], mousepos);
                }

            }
        }
    }
    else {
        // cancel all operations on view when interacting on GUI
        view_drag = nullptr;
        mousedown = false;
        Mixer::manager().view()->terminate();
    }


    if ( ImGui::IsMouseReleased(ImGuiMouseButton_Left) || ImGui::IsMouseReleased(ImGuiMouseButton_Right) )
    {
        view_drag = nullptr;
        mousedown = false;
        picked = { nullptr, glm::vec2(0.f) };
        Mixer::manager().view()->terminate();
        SetMouseCursor(io.MousePos);

        // special case of one single source in selection : make current after release
        if (Mixer::selection().size() == 1)
            Mixer::manager().setCurrentSource( Mixer::selection().front() );
    }
}


bool UserInterface::saveOrSaveAs(bool force_versioning)
{
    bool finished = false;

    if (Mixer::manager().session()->filename().empty())
        selectSaveFilename();
    else {
        Mixer::manager().save(force_versioning || Settings::application.save_version_snapshot);
        finished = true;
    }
    return finished;
}

void UserInterface::selectSaveFilename()
{
    if (sessionsavedialog)
        sessionsavedialog->open();

    navigator.hidePannel();
}

void UserInterface::selectOpenFilename()
{
    // launch file dialog to select a session filename to open
    if (sessionopendialog)
        sessionopendialog->open();

    navigator.hidePannel();
}

void UserInterface::NewFrame()
{
    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // deal with keyboard and mouse events
    handleKeyboard();
    handleMouse();
    handleScreenshot();

    // handle FileDialogs
    if (sessionopendialog && sessionopendialog->closed() && !sessionopendialog->path().empty())
        Mixer::manager().open(sessionopendialog->path());

    if (sessionimportdialog && sessionimportdialog->closed() && !sessionimportdialog->path().empty())
        Mixer::manager().import(sessionimportdialog->path());

    if (sessionsavedialog && sessionsavedialog->closed() && !sessionsavedialog->path().empty())
        Mixer::manager().saveas(sessionsavedialog->path(), Settings::application.save_version_snapshot);

    // overlay to ensure file dialog is modal
    if (DialogToolkit::FileDialog::busy()){
        ImGui::OpenPopup("Busy");
        if (ImGui::BeginPopupModal("Busy", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Close file dialog box to resume.");
            ImGui::EndPopup();
        }
    }

    // navigator bar first
    navigator.Render();
}

void UserInterface::Render()
{
    // management of video_recorders
    if ( !_video_recorders.empty() ) {
        // check that file dialog thread finished
        if (_video_recorders.back().wait_for(timeout) == std::future_status::ready ) {
            video_recorder_ = _video_recorders.back().get();
            FrameGrabbing::manager().add(video_recorder_);
            _video_recorders.pop_back();
        }
    }
    // verify the video recorder is valid (change to nullptr if invalid)
    FrameGrabbing::manager().verify(&video_recorder_);
    if (video_recorder_ // if there is an ongoing recorder
        && Settings::application.record.timeout < RECORD_MAX_TIMEOUT  // and if the timeout is valid
        && video_recorder_->duration() > Settings::application.record.timeout ) // and the timeout is reached
    {
        video_recorder_->stop();
    }

#if defined(LINUX)
    // verify the frame grabber for webcam emulator is valid
    FrameGrabbing::manager().verify(&webcam_emulator_);
#endif

    // warning modal dialog
    Log::Render(&Settings::application.widget.logs);

    // clear view mode in Transition view
    if ( !Settings::application.transition.hide_windows || Settings::application.current_view < View::TRANSITION) {

        // windows
        if (Settings::application.widget.toolbox)
            toolbox.Render();
        if (Settings::application.widget.preview && ( Settings::application.widget.preview_view < 0 ||
            Settings::application.widget.preview_view == Settings::application.current_view ))
            RenderPreview();
        if (Settings::application.widget.timer && ( Settings::application.widget.timer_view < 0 ||
            Settings::application.widget.timer_view == Settings::application.current_view ))
            RenderTimer();
        if (Settings::application.widget.shader_editor)
            RenderShaderEditor();
        if (Settings::application.widget.logs)
            Log::ShowLogWindow(&Settings::application.widget.logs);
        if (Settings::application.widget.help)
            sessiontoolbox.Render();

        // Source controller
        if (sourcecontrol.Visible())
            sourcecontrol.Render();
        sourcecontrol.Update();

        // Notes
        RenderNotes();

        // dialogs
        if (show_view_navigator > 0)
            target_view_navigator = RenderViewNavigator( &show_view_navigator );
        if (show_vimix_about)
            RenderAbout(&show_vimix_about);
        if (show_imgui_about)
            ImGui::ShowAboutWindow(&show_imgui_about);
        if (show_gst_about)
            ShowAboutGStreamer(&show_gst_about);
        if (show_opengl_about)
            ShowAboutOpengl(&show_opengl_about);
    }

    // stats in the corner
    if (Settings::application.widget.stats)
        RenderMetrics(&Settings::application.widget.stats,
                  &Settings::application.widget.stats_corner,
                  &Settings::application.widget.stats_mode);

    // all IMGUI Rendering
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

}

void UserInterface::Terminate()
{
    if (Settings::application.recentSessions.save_on_exit)
        Mixer::manager().save(false);

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void UserInterface::showMenuEdit()
{
    bool has_selection = !Mixer::selection().empty();
    const char *clipboard = ImGui::GetClipboardText();
    bool has_clipboard = (clipboard != nullptr && strlen(clipboard) > 0 && SessionLoader::isClipboard(clipboard));

    if (ImGui::MenuItem( ICON_FA_CUT "  Cut", CTRL_MOD "X", false, has_selection)) {
        std::string copied_text = Mixer::selection().clipboard();
        if (!copied_text.empty()) {
            ImGui::SetClipboardText(copied_text.c_str());
            Mixer::manager().deleteSelection();
        }
        navigator.hidePannel();
    }
    if (ImGui::MenuItem( ICON_FA_COPY "  Copy", CTRL_MOD "C", false, has_selection)) {
        std::string copied_text = Mixer::selection().clipboard();
        if (!copied_text.empty())
            ImGui::SetClipboardText(copied_text.c_str());
        navigator.hidePannel();
    }
    if (ImGui::MenuItem( ICON_FA_PASTE "  Paste", CTRL_MOD "V", false, has_clipboard)) {
        Mixer::manager().paste(clipboard);
        navigator.hidePannel();
    }
    if (ImGui::MenuItem( ICON_FA_LIST "  Select all", CTRL_MOD "A")) {
        Mixer::manager().view()->selectAll();
        navigator.hidePannel();
    }
    ImGui::Separator();
    if ( ImGui::MenuItem( ICON_FA_UNDO "  Undo", CTRL_MOD "Z") )
        Action::manager().undo();
    if ( ImGui::MenuItem( ICON_FA_REDO "  Redo", CTRL_MOD "Shift+Z") )
        Action::manager().redo();
}

void UserInterface::showMenuFile()
{
    if (ImGui::MenuItem( ICON_FA_FILE "  New", CTRL_MOD "W")) {
        Mixer::manager().close();
        navigator.hidePannel();
    }
    ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x * 0.6f);
    ImGui::Combo("Ratio", &Settings::application.render.ratio, FrameBuffer::aspect_ratio_name, IM_ARRAYSIZE(FrameBuffer::aspect_ratio_name) );
    ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x * 0.6f);
    ImGui::Combo("Height", &Settings::application.render.res, FrameBuffer::resolution_name, IM_ARRAYSIZE(FrameBuffer::resolution_name) );

    ImGui::Separator();

    ImGui::MenuItem( ICON_FA_LEVEL_UP_ALT "  Open last on start", nullptr, &Settings::application.recentSessions.load_at_start);

    if (ImGui::MenuItem( ICON_FA_FILE_UPLOAD "  Open", CTRL_MOD "O"))
        selectOpenFilename();
    if (ImGui::MenuItem( ICON_FA_FILE_UPLOAD "  Re-open", CTRL_MOD "Shift+O"))
        Mixer::manager().load( Mixer::manager().session()->filename() );

    if (ImGui::MenuItem( ICON_FA_FILE_EXPORT " Import") && sessionimportdialog) {
        // launch file dialog to open a session file
        sessionimportdialog->open();
        navigator.hidePannel();
    }
    if (ImGui::MenuItem( ICON_FA_FILE_DOWNLOAD "  Save", CTRL_MOD "S")) {
        if (saveOrSaveAs())
            navigator.hidePannel();
    }
    if (ImGui::MenuItem( ICON_FA_FILE_DOWNLOAD "  Save as", CTRL_MOD "Shift+S"))
        selectSaveFilename();

    ImGui::MenuItem( ICON_FA_LEVEL_DOWN_ALT "  Save on exit", nullptr, &Settings::application.recentSessions.save_on_exit);

    ImGui::Separator();
    if (ImGui::MenuItem( ICON_FA_POWER_OFF " Quit", CTRL_MOD "Q"))
        Rendering::manager().close();

}

void UserInterface::StartScreenshot()
{
    screenshot_step = 1;
}

void UserInterface::handleScreenshot()
{
    // taking screenshot is in 3 steps
    // 1) wait 1 frame that the menu / action showing button to take screenshot disapears
    // 2) wait 1 frame that rendering manager takes the actual screenshot
    // 3) if rendering manager current screenshot is ok, save it
    if (screenshot_step > 0) {

        switch(screenshot_step) {
            case 1:
                screenshot_step = 2;
            break;
            case 2:
                Rendering::manager().requestScreenshot();
                screenshot_step = 3;
            break;
            case 3:
            {
                if ( Rendering::manager().currentScreenshot()->isFull() ){
                    std::string filename =  SystemToolkit::full_filename( SystemToolkit::home_path(), SystemToolkit::date_time_string() + "_vmixcapture.png" );
                    Rendering::manager().currentScreenshot()->save( filename );
                    Log::Notify("Screenshot saved %s", filename.c_str() );
                }
                screenshot_step = 4;
            }
            break;
            default:
                screenshot_step = 0;
            break;
        }

    }
}


#define MAX_SEGMENTS 64

void UserInterface::RenderTimer()
{
    // timer modes : 0 Metronome, 1 Stopwatch
    static std::array< std::string, 2 > timer_menu = { "Metronome", "Stopwatch" };

    // constraint position
    static ImVec2 timer_window_pos = ImVec2(1080, 20);
    static ImVec2 timer_window_size_min = ImVec2(11.f * ImGui::GetTextLineHeight(), 11.f * ImGui::GetTextLineHeight());
    static ImVec2 timer_window_size = timer_window_size_min * 1.1f;
    SetNextWindowVisible(timer_window_pos, timer_window_size);

    // constraint square resizing
    ImGui::SetNextWindowSizeConstraints(timer_window_size_min, timer_window_size_min * 1.5f, CustomConstraints::Square);

    if ( !ImGui::Begin(IMGUI_TITLE_TIMER, &Settings::application.widget.timer, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar |  ImGuiWindowFlags_NoCollapse ))
    {
        ImGui::End();
        return;
    }

    // current window
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    timer_window_pos = window->Pos;
    timer_window_size = window->Size;

    // menu (no title bar)
    if (ImGui::BeginMenuBar())
    {
        // Close and widget menu
        if (ImGuiToolkit::IconButton(4,16))
            Settings::application.widget.timer = false;
        if (ImGui::BeginMenu(IMGUI_TITLE_TIMER))
        {
            // Enable/Disable Ableton Link
            if ( ImGui::MenuItem( ICON_FA_USER_CLOCK "  Ableton Link", nullptr, &Settings::application.timer.link_enabled) ) {
                Metronome::manager().setEnabled(Settings::application.timer.link_enabled);
            }

            // output manager menu
            ImGui::Separator();
            bool pinned = Settings::application.widget.timer_view == Settings::application.current_view;
            if ( ImGui::MenuItem( ICON_FA_MAP_PIN "    Pin window to view", nullptr, &pinned) ){
                if (pinned)
                    Settings::application.widget.timer_view = Settings::application.current_view;
                else
                    Settings::application.widget.timer_view = -1;
            }
            if ( ImGui::MenuItem( ICON_FA_TIMES "  Close") )
                Settings::application.widget.timer = false;

            ImGui::EndMenu();
        }
        // Selection of the timer mode
        if (ImGui::BeginMenu( timer_menu[Settings::application.timer.mode].c_str() ))
        {
            for (size_t i = 0; i < timer_menu.size(); ++i) {
                if (ImGui::MenuItem(timer_menu[i].c_str(), NULL, Settings::application.timer.mode==i))
                    Settings::application.timer.mode = i;
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    // Window draw parameters
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    // positions and size of GUI elements
    const float margin = window->MenuBarHeight();
    const float h = 0.4f * ImGui::GetFrameHeight();
    const ImVec2 circle_top_left = window->Pos + ImVec2(margin + h, margin + h);
    const ImVec2 circle_top_right = window->Pos + ImVec2(window->Size.y - margin - h, margin + h);
//    const ImVec2 circle_botom_left = window->Pos + ImVec2(margin + h, window->Size.x - margin - h);
    const ImVec2 circle_botom_right = window->Pos + ImVec2(window->Size.y - margin - h, window->Size.x - margin - h);
    const ImVec2 circle_center = window->Pos + (window->Size + ImVec2(margin, margin) )/ 2.f;
    const float circle_radius = (window->Size.y - 2.f * margin) / 2.f;

    // color palette
    const ImU32 colorbg = ImGui::GetColorU32(ImGuiCol_FrameBgActive, 0.6f);
    const ImU32 colorfg = ImGui::GetColorU32(ImGuiCol_FrameBg, 2.5f);
    const ImU32 colorline = ImGui::GetColorU32(ImGuiCol_PlotHistogram);

    //
    // METRONOME
    //
    if (Settings::application.timer.mode < 1) {

        // Metronome info
        double t = Metronome::manager().tempo();
        double p = Metronome::manager().phase();
        double q = Metronome::manager().quantum();
        uint np  = (int) Metronome::manager().peers();

        // draw background ring
        draw_list->AddCircleFilled(circle_center, circle_radius, colorbg, MAX_SEGMENTS);

        // draw quarter
        static const float resolution = MAX_SEGMENTS / (2.f * M_PI);
        static ImVec2 buffer[MAX_SEGMENTS];
        float a0 = -M_PI_2 + (floor(p)/floor(q)) * (2.f * M_PI);
        float a1 = a0 + (1.f / floor(q)) * (2.f * M_PI);
        int n = ImMax(3, (int)((a1 - a0) * resolution));
        double da = (a1 - a0) / (n - 1);
        int index = 0;
        buffer[index++] = circle_center;
        for (int i = 0; i < n; ++i) {
            double a = a0 + i * da;
            buffer[index++] = ImVec2(circle_center.x + circle_radius * cos(a), circle_center.y + circle_radius * sin(a));
        }
        draw_list->AddConvexPolyFilled(buffer, index, colorfg);

        // draw clock hand
        a0 = -M_PI_2 + (p/q) * (2.f * M_PI);
        draw_list->AddLine(ImVec2(circle_center.x + margin * cos(a0), circle_center.y + margin * sin(a0)),
                           ImVec2(circle_center.x + circle_radius * cos(a0), circle_center.y + circle_radius * sin(a0)), colorline, 2.f);

        // centered indicator 'x / N'
        draw_list->AddCircleFilled(circle_center, margin, colorfg, MAX_SEGMENTS);
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_MONO);
        char text_buf[24];
        sprintf(text_buf, "%d/%d", (int)(p)+1, (int)(q) );
        ImVec2 label_size = ImGui::CalcTextSize(text_buf, NULL);
        ImGui::SetCursorScreenPos(circle_center - label_size/2);
        ImGui::Text("%s", text_buf);
        ImGui::PopFont();

        // left slider : quantum
        float float_value = ceil(Metronome::manager().quantum());
        ImGui::SetCursorScreenPos(timer_window_pos + ImVec2(0.5f * margin, 1.5f * margin));
        if ( ImGui::VSliderFloat("##quantum", ImVec2(0.5f * margin, 2.f * circle_radius ), &float_value, 2, 200, "", 2.f) ){
            Metronome::manager().setQuantum( ceil(float_value) );
        }
        if (ImGui::IsItemHovered() || ImGui::IsItemActive() )  {
            ImGui::BeginTooltip();
            guint64 time_phase = GST_SECOND * (60.0 * q / t) ;
            ImGui::Text("%d beats per phase\n= %s at %d BPM", (int) ceil(float_value),
                        GstToolkit::time_to_string(time_phase, GstToolkit::TIME_STRING_READABLE).c_str(), (int) t);
            ImGui::EndTooltip();
        }

        // Controls NOT operational if peer connected
        if (np >0 ) {
            // Tempo
            ImGui::SetCursorScreenPos(circle_top_right);
            ImGuiToolkit::PushFont(ImGuiToolkit::FONT_BOLD);
            ImGui::PushStyleColor(ImGuiCol_Text, colorfg);
            sprintf(text_buf, "%d", (int) ceil(t) );
            ImGui::Text("%s", text_buf);
            ImGui::PopStyleColor();
            ImGui::PopFont();
            if (ImGui::IsItemHovered()){
                sprintf(text_buf, "%d BPM\n(set by peer)", (int) ceil(t));
                ImGuiToolkit::ToolTip(text_buf);
            }
        }
        // Controls operational only if no peer
        else {
            // Tempo
            ImGui::SetCursorScreenPos(circle_top_right);
            ImGuiToolkit::PushFont(ImGuiToolkit::FONT_BOLD);
            sprintf(text_buf, "%d", (int) ceil(t) );
            ImGui::Text("%s", text_buf);
            ImGui::PopFont();
            if (ImGui::IsItemClicked())
                ImGui::OpenPopup("bpm_popup");
            else if (ImGui::IsItemHovered()){
                sprintf(text_buf, "%d BPM\n(clic to edit)", (int) ceil(t));
                ImGuiToolkit::ToolTip(text_buf);
            }
            if (ImGui::BeginPopup("bpm_popup", ImGuiWindowFlags_NoMove))
            {
                ImGui::SetNextItemWidth(80);
                ImGui::InputText("BPM", text_buf, 8, ImGuiInputTextFlags_CharsDecimal);
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    int t = 0;
                    sscanf(text_buf, "%d", &t);
                    t = CLAMP(t, 20, 2000);
                    Metronome::manager().setTempo((double) t);
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
            // restart icon
            ImGui::SetCursorScreenPos(circle_top_left);
            if (ImGuiToolkit::IconButton(9, 13)) {
                Metronome::manager().restart();
            }
        }

        // Network indicator, if link enabled
        if (Settings::application.timer.link_enabled) {
            ImGui::SetCursorScreenPos(circle_botom_right);
            ImGuiToolkit::Icon(16, 5, np > 0);
            if (ImGui::IsItemHovered()){
                sprintf(text_buf, np < 1 ? "Ableton Link\nNo peer" : "Ableton Link\n%d peer%c", np, np < 2 ? ' ' : 's' );
                ImGuiToolkit::ToolTip(text_buf);
            }
        }

    }
    //
    // STOPWATCH
    //
    else {
        // clock times
        static guint64 start_time_    = gst_util_get_timestamp ();
        static guint64 start_time_hand_ = gst_util_get_timestamp ();
        static guint64 duration_hand_   = Settings::application.timer.stopwatch_duration *  GST_SECOND;
        guint64 time_ = gst_util_get_timestamp ();

        // draw ring
        draw_list->AddCircle(circle_center, circle_radius, colorbg, MAX_SEGMENTS, 12 );
        draw_list->AddCircleFilled(ImVec2(circle_center.x, circle_center.y - circle_radius), 7, colorfg, MAX_SEGMENTS);
        // draw indicator time hand
        double da = -M_PI_2 + ( (double) (time_-start_time_hand_) / (double) duration_hand_) * (2.f * M_PI);
        draw_list->AddCircleFilled(ImVec2(circle_center.x + circle_radius * cos(da), circle_center.y + circle_radius * sin(da)), 7, colorline, MAX_SEGMENTS);

        // left slider : countdown
        float float_value = (float) Settings::application.timer.stopwatch_duration;
        ImGui::SetCursorScreenPos(timer_window_pos + ImVec2(0.5f * margin, 1.5f * margin));
        if ( ImGui::VSliderFloat("##duration", ImVec2(0.5f * margin, 2.f * circle_radius ), &float_value, 1, 3600, "", 3.f) ){
            Settings::application.timer.stopwatch_duration = (uint64_t) float_value;
            duration_hand_ = Settings::application.timer.stopwatch_duration * GST_SECOND;
        }
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())  {
            ImGui::BeginTooltip();
            ImGui::Text("%s\ncountdown", GstToolkit::time_to_string(duration_hand_, GstToolkit::TIME_STRING_READABLE).c_str() );
            ImGui::EndTooltip();
        }

        // main text: elapsed time
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        char text_buf[24];
        sprintf(text_buf, "%s", GstToolkit::time_to_string(time_-start_time_, GstToolkit::TIME_STRING_FIXED).c_str() );
        ImVec2 label_size = ImGui::CalcTextSize(text_buf, NULL);
        ImGui::SetCursorScreenPos(circle_center - label_size/2);
        ImGui::Text("%s", text_buf);
        ImGui::PopFont();

        // small text: remaining time
        ImGui::PushStyleColor(ImGuiCol_Text, colorfg);
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_BOLD);
        sprintf(text_buf, "%s", GstToolkit::time_to_string(duration_hand_-(time_-start_time_hand_)%duration_hand_, GstToolkit::TIME_STRING_READABLE).c_str() );
        label_size = ImGui::CalcTextSize(text_buf, NULL);
        ImGui::SetCursorScreenPos(circle_center + ImVec2(0.f, circle_radius * -0.7f) - label_size/2);
        ImGui::Text("%s", text_buf);
        ImGui::PopFont();
        ImGui::PopStyleColor();

        // reset icon
        ImGui::SetCursorScreenPos(circle_top_left);
        if (ImGuiToolkit::IconButton(8, 13))
            start_time_ = start_time_hand_ = time_; // reset timers

        // TODO : pause ?
    }

    ImGui::End();
}

void UserInterface::RenderPreview()
{
#if defined(LINUX)
    bool openInitializeSystemLoopback = false;
#endif

    // recording location
    static DialogToolkit::OpenFolderDialog recordFolderDialog("Recording Location");

    FrameBuffer *output = Mixer::manager().session()->frame();
    if (output)
    {
        ImGui::SetNextWindowPos(ImVec2(1180, 20), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(400, 260), ImGuiCond_FirstUseEver);

        // constraint position
        static ImVec2 preview_window_pos = ImVec2(1180, 20);
        static ImVec2 preview_window_size = ImVec2(400, 260);
        SetNextWindowVisible(preview_window_pos, preview_window_size);

        // constraint aspect ratio resizing
        float ar = output->aspectRatio();
        ImGui::SetNextWindowSizeConstraints(ImVec2(300, 200), ImVec2(FLT_MAX, FLT_MAX), CustomConstraints::AspectRatio, &ar);

        if ( !ImGui::Begin("Preview", &Settings::application.widget.preview,
                           ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar |
                           ImGuiWindowFlags_NoTitleBar |  ImGuiWindowFlags_NoCollapse ) )
        {
            ImGui::End();
            return;
        }
        preview_window_pos = ImGui::GetWindowPos();
        preview_window_size = ImGui::GetWindowSize();

        // return from thread for folder openning
        if (recordFolderDialog.closed() && !recordFolderDialog.path().empty())
            // get the folder from this file dialog
            Settings::application.record.path = recordFolderDialog.path();

        // menu (no title bar)
        if (ImGui::BeginMenuBar())
        {
            if (ImGuiToolkit::IconButton(4,16))
                Settings::application.widget.preview = false;
            if (ImGui::BeginMenu(IMGUI_TITLE_PREVIEW))
            {
                // Output window menu
                if ( ImGui::MenuItem( ICON_FA_WINDOW_RESTORE "  Show window") )
                    Rendering::manager().outputWindow().show();

                bool isfullscreen = Rendering::manager().outputWindow().isFullscreen();
                if ( ImGui::MenuItem( ICON_FA_EXPAND_ALT "  Fullscreen window", CTRL_MOD "F", &isfullscreen) ) {
                    Rendering::manager().outputWindow().show();
                    Rendering::manager().outputWindow().toggleFullscreen();
                }

                ImGui::MenuItem( ICON_FA_EYE_SLASH " Disable", "END", &Settings::application.render.disabled);

                // output manager menu
                ImGui::Separator();
                bool pinned = Settings::application.widget.preview_view == Settings::application.current_view;
                if ( ImGui::MenuItem( ICON_FA_MAP_PIN "    Pin window to view", nullptr, &pinned) ){
                    if (pinned)
                        Settings::application.widget.preview_view = Settings::application.current_view;
                    else
                        Settings::application.widget.preview_view = -1;
                }
                if ( ImGui::MenuItem( ICON_FA_TIMES "   Close", CTRL_MOD "D") )
                    Settings::application.widget.preview = false;

                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Record"))
            {
                if ( ImGui::MenuItem( ICON_FA_CAMERA_RETRO "  Capture frame", CTRL_MOD "Shitf+R") )
                    FrameGrabbing::manager().add(new PNGRecorder);

                // Stop recording menu if main recorder already exists

                if (!_video_recorders.empty()) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_RECORD, 0.8f));
                    ImGui::MenuItem( ICON_FA_SQUARE "  Record starting", CTRL_MOD "R", false, false);
                    ImGui::PopStyleColor(1);
                }
                else if (video_recorder_) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_RECORD, 0.8f));
                    if ( ImGui::MenuItem( ICON_FA_SQUARE "  Stop Record", CTRL_MOD "R") ) {
                        video_recorder_->stop();
                    }
                    // offer the 'save & continue' recording for undefined duration
                    if (Settings::application.record.timeout == RECORD_MAX_TIMEOUT) {
                        if ( ImGui::MenuItem( ICON_FA_ARROW_ALT_CIRCLE_DOWN "  Save & continue", CTRL_MOD "Alt+R") ) {
                            VideoRecorder *rec = new VideoRecorder;
                            FrameGrabbing::manager().chain(video_recorder_, rec);
                            video_recorder_ = rec;
                        }
                    }
                    ImGui::PopStyleColor(1);
                }
                // start recording
                else {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_RECORD, 0.9f));
                    if ( ImGui::MenuItem( ICON_FA_CIRCLE "  Record", CTRL_MOD "R") ) {
                        _video_recorders.emplace_back( std::async(std::launch::async, delayTrigger, new VideoRecorder,
                                                                  std::chrono::seconds(Settings::application.record.delay)) );
                    }
                    ImGui::PopStyleColor(1);
                }
                // Options menu
                ImGui::Separator();
                ImGui::MenuItem("Options", nullptr, false, false);
                // offer to open config panel from here for more options
                ImGui::SameLine(ImGui::GetContentRegionAvailWidth() + 1.2f * IMGUI_RIGHT_ALIGN);
                if (ImGuiToolkit::IconButton(13, 5))
                    navigator.showConfig();
                ImGui::SameLine(0);
                ImGui::Text("Settings");
                // BASIC OPTIONS
                static char* name_path[4] = { nullptr };
                if ( name_path[0] == nullptr ) {
                    for (int i = 0; i < 4; ++i)
                        name_path[i] = (char *) malloc( 1024 * sizeof(char));
                    sprintf( name_path[1], "%s", ICON_FA_HOME " Home");
                    sprintf( name_path[2], "%s", ICON_FA_FOLDER " Session location");
                    sprintf( name_path[3], "%s", ICON_FA_FOLDER_PLUS " Select");
                }
                if (Settings::application.record.path.empty())
                    Settings::application.record.path = SystemToolkit::home_path();
                sprintf( name_path[0], "%s", Settings::application.record.path.c_str());

                int selected_path = 0;
                ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                ImGui::Combo("Path", &selected_path, name_path, 4);
                if (selected_path > 2)
                    recordFolderDialog.open();
                else if (selected_path > 1)
                    Settings::application.record.path = SystemToolkit::path_filename( Mixer::manager().session()->filename() );
                else if (selected_path > 0)
                    Settings::application.record.path = SystemToolkit::home_path();

                ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                ImGuiToolkit::SliderTiming ("Duration", &Settings::application.record.timeout, 1000, RECORD_MAX_TIMEOUT, 1000, "Until stopped");
                ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                ImGui::SliderInt("Trigger", &Settings::application.record.delay, 0, 5,
                                 Settings::application.record.delay < 1 ? "Immediate" : "After %d s");

//                ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
//                ImGui::SliderInt("Buffer", &Settings::application.record.buffering_mode, 0, VIDEO_RECORDER_BUFFERING_NUM_PRESET-1,
//                                 VideoRecorder::buffering_preset_name[Settings::application.record.buffering_mode]);

                //
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Share"))
            {

#if defined(LINUX_NOT_YET_WORKING)
                bool on = webcam_emulator_ != nullptr;
                if ( ImGui::MenuItem( ICON_FA_CAMERA "  Emulate video camera", NULL, &on) ) {
                    if (on) {
                        if (Loopback::systemLoopbackInitialized()) {
                            webcam_emulator_ = new Loopback;
                            FrameGrabbing::manager().add(webcam_emulator_);
                        }
                        else
                            openInitializeSystemLoopback = true;
                    }
                    else if (webcam_emulator_ != nullptr) {
                        webcam_emulator_->stop();
                        webcam_emulator_ = nullptr;
                    }
                }
#endif
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_STREAM, 0.9f));
                if ( ImGui::MenuItem( ICON_FA_SHARE_ALT "  Accept connections            ", NULL, &Settings::application.accept_connections) ) {
                    Streaming::manager().enable(Settings::application.accept_connections);
                }
                ImGui::PopStyleColor(1);
                if (Settings::application.accept_connections)
                {
                    static char dummy_str[512];
                    sprintf(dummy_str, "%s", Connection::manager().info().name.c_str());
                    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                    ImGui::InputText("My ID", dummy_str, IM_ARRAYSIZE(dummy_str), ImGuiInputTextFlags_ReadOnly);

                    std::vector<std::string> ls = Streaming::manager().listStreams();
                    if (ls.size()>0) {
                        ImGui::Separator();
                        ImGui::MenuItem("Active streams", nullptr, false, false);
                        for (auto it = ls.begin(); it != ls.end(); ++it)
                            ImGui::Text(" %s", (*it).c_str() );
                    }

                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        float width = ImGui::GetContentRegionAvail().x;

        ImVec2 imagesize ( width, width / ar);
        // virtual button to show the output window when clic on the preview
        ImVec2 draw_pos = ImGui::GetCursorScreenPos();
        // preview image
        ImGui::Image((void*)(intptr_t)output->texture(), imagesize);
        // tooltip overlay
        if (ImGui::IsItemHovered())
        {
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            draw_list->AddRectFilled(draw_pos,  ImVec2(draw_pos.x + width, draw_pos.y + ImGui::GetTextLineHeightWithSpacing()), IMGUI_COLOR_OVERLAY);
            ImGui::SetCursorScreenPos(draw_pos);
            ImGui::Text(" %d x %d px, %.d fps", output->width(), output->height(), int(Mixer::manager().fps()) );

            // raise window on double clic
            if (ImGui::IsMouseDoubleClicked(0) )
                Rendering::manager().outputWindow().show();
        }
        const float r = ImGui::GetTextLineHeightWithSpacing();
        // recording indicator overlay
        if (video_recorder_)
        {
            ImGui::SetCursorScreenPos(ImVec2(draw_pos.x + r, draw_pos.y + r));
            ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_RECORD, 0.8f));
            ImGui::Text(ICON_FA_CIRCLE " %s", video_recorder_->info().c_str() );
            ImGui::PopStyleColor(1);
            ImGui::PopFont();
        }
        else if (!_video_recorders.empty())
        {
            ImGui::SetCursorScreenPos(ImVec2(draw_pos.x + r, draw_pos.y + r));
            ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
            static double anim = 0.f;
            double a = sin(anim+=0.104); // 2 * pi / 60fps
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_RECORD, a));
            ImGui::Text(ICON_FA_CIRCLE);
            ImGui::PopStyleColor(1);
            ImGui::PopFont();
        }
        // streaming indicator overlay
        if (Settings::application.accept_connections)
        {
            ImGui::SetCursorScreenPos(ImVec2(draw_pos.x + width - 2.f * r, draw_pos.y + r));
            ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
            if ( Streaming::manager().busy())
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_STREAM, 0.8f));
            else
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_STREAM, 0.2f));
            ImGui::Text(ICON_FA_SHARE_ALT_SQUARE);
            ImGui::PopStyleColor(1);
            ImGui::PopFont();
        }

        // OUTPUT DISABLED indicator overlay
        if (Settings::application.render.disabled)
        {
            ImGui::SetCursorScreenPos(ImVec2(draw_pos.x + r, draw_pos.y + (width / ar) - 2.f*r));
            ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_RECORD, 0.8f));
            ImGui::Text(ICON_FA_EYE_SLASH);
            ImGui::PopStyleColor(1);
            ImGui::PopFont();
        }


#if defined(LINUX)
        // streaming indicator overlay
        if (webcam_emulator_)
        {
            float r = ImGui::GetTextLineHeightWithSpacing();
            ImGui::SetCursorScreenPos(ImVec2(draw_pos.x + width - 2.f * r, draw_pos.y + imagesize.y - 2.f * r));
            ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 0.8f));
            ImGui::Text(ICON_FA_CAMERA);
            ImGui::PopStyleColor(1);
            ImGui::PopFont();
        }
#endif

        ImGui::End();
    }

#if defined(LINUX)
    if (openInitializeSystemLoopback && !ImGui::IsPopupOpen("Initialize System Loopback"))
        ImGui::OpenPopup("Initialize System Loopback");
    if (ImGui::BeginPopupModal("Initialize System Loopback", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        int w = 600;
        ImGui::Text("In order to enable the video4linux camera loopback,\n"
                    "'v4l2loopack' has to be installed and initialized on your machine\n\n"
                    "To do so, the following commands should be executed (admin rights):\n");

        static char dummy_str[512];
        sprintf(dummy_str, "sudo apt install v4l2loopback-dkms");
        ImGui::Text("Install v4l2loopack (once):");
        ImGui::SetNextItemWidth(600-40);
        ImGui::InputText("##cmd1", dummy_str, IM_ARRAYSIZE(dummy_str), ImGuiInputTextFlags_ReadOnly);
        ImGui::SameLine();
        ImGui::PushID(358794);
        if ( ImGuiToolkit::ButtonIcon(11,2, "Copy to clipboard") )
            ImGui::SetClipboardText(dummy_str);
        ImGui::PopID();

        sprintf(dummy_str, "sudo modprobe v4l2loopback exclusive_caps=1 video_nr=10 card_label=\"vimix loopback\"");
        ImGui::Text("Initialize v4l2loopack (after reboot):");
        ImGui::SetNextItemWidth(600-40);
        ImGui::InputText("##cmd2", dummy_str, IM_ARRAYSIZE(dummy_str), ImGuiInputTextFlags_ReadOnly);
        ImGui::SameLine();
        ImGui::PushID(899872);
        if ( ImGuiToolkit::ButtonIcon(11,2, "Copy to clipboard") )
            ImGui::SetClipboardText(dummy_str);
        ImGui::PopID();

        ImGui::Separator();
        ImGui::SetItemDefaultFocus();
        if (ImGui::Button("Ok, I'll do this in a terminal and try again later.", ImVec2(w, 0)) ) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
#endif
}

int UserInterface::RenderViewNavigator(int *shift)
{
    // calculate potential target view index :
    // - shift increment : minus 1 to not react to first trigger
    // - current_view : indices are between 1 (Mixing) and 5 (Appearance)
    // - Modulo 4 to allow multiple repetition of shift increment
    int target_index = ( (Settings::application.current_view -1)+ (*shift -1) )%4 + 1;

    // prepare rendering of centered, fixed-size, semi-transparent window;
    const ImGuiIO& io = ImGui::GetIO();
    ImVec2 window_pos = ImVec2(io.DisplaySize.x / 2.f, io.DisplaySize.y / 2.f);
    ImVec2 window_pos_pivot = ImVec2(0.5f, 0.5f);
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
    ImGui::SetNextWindowSize(ImVec2(500.f, 120.f + 2.f * ImGui::GetTextLineHeight()), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.85f);

    // show window
    if (ImGui::Begin("Views", NULL,  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
    {
        // prepare rendering of the array of selectable icons
        bool selected_view[View::INVALID] = { };
        selected_view[ target_index ] = true;
        ImVec2 iconsize(120.f, 120.f);

        // draw icons centered horizontally and vertically
        ImVec2 alignment = ImVec2(0.4f, 0.5f);
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, alignment);

        // draw in 4 columns
        ImGui::Columns(4, NULL, false);

        // 4 selectable large icons
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        if (ImGui::Selectable( ICON_FA_BULLSEYE, &selected_view[1], 0, iconsize))
        {
            Mixer::manager().setView(View::MIXING);
            *shift = 0;
        }
        ImGui::NextColumn();
        if (ImGui::Selectable( ICON_FA_OBJECT_UNGROUP , &selected_view[2], 0, iconsize))
        {
            Mixer::manager().setView(View::GEOMETRY);
            *shift = 0;
        }
        ImGui::NextColumn();
        if (ImGui::Selectable( ICON_FA_LAYER_GROUP, &selected_view[3], 0, iconsize))
        {
            Mixer::manager().setView(View::LAYER);
            *shift = 0;
        }
        ImGui::NextColumn();
        if (ImGui::Selectable( ICON_FA_CHESS_BOARD, &selected_view[4], 0, iconsize))
        {
            Mixer::manager().setView(View::TEXTURE);
            *shift = 0;
        }
        ImGui::PopFont();

        // 4 subtitles (text centered in column)
        ImGui::NextColumn();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetColumnWidth() - ImGui::CalcTextSize("Mixing").x) * 0.5f - ImGui::GetStyle().ItemSpacing.x);

        ImGuiToolkit::PushFont(Settings::application.current_view == 1 ? ImGuiToolkit::FONT_BOLD : ImGuiToolkit::FONT_DEFAULT);
        ImGui::Text("Mixing");
        ImGui::PopFont();
        ImGui::NextColumn();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetColumnWidth() - ImGui::CalcTextSize("Geometry").x) * 0.5f - ImGui::GetStyle().ItemSpacing.x);
        ImGuiToolkit::PushFont(Settings::application.current_view == 2 ? ImGuiToolkit::FONT_BOLD : ImGuiToolkit::FONT_DEFAULT);
        ImGui::Text("Geometry");
        ImGui::PopFont();
        ImGui::NextColumn();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetColumnWidth() - ImGui::CalcTextSize("Layers").x) * 0.5f - ImGui::GetStyle().ItemSpacing.x);
        ImGuiToolkit::PushFont(Settings::application.current_view == 3 ? ImGuiToolkit::FONT_BOLD : ImGuiToolkit::FONT_DEFAULT);
        ImGui::Text("Layers");
        ImGui::PopFont();
        ImGui::NextColumn();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetColumnWidth() - ImGui::CalcTextSize("Texturing").x) * 0.5f - ImGui::GetStyle().ItemSpacing.x);
        ImGuiToolkit::PushFont(Settings::application.current_view == 4 ? ImGuiToolkit::FONT_BOLD : ImGuiToolkit::FONT_DEFAULT);
        ImGui::Text("Texturing");
        ImGui::PopFont();

        ImGui::Columns(1);
        ImGui::PopStyleVar();

        ImGui::End();
    }

    return target_index;
}

void UserInterface::showSourceEditor(Source *s)
{
    Mixer::manager().unsetCurrentSource();
    Mixer::selection().clear();

    if (s) {
        Mixer::manager().setCurrentSource( s );
        if (s->playable()) {
            Settings::application.widget.media_player = true;
            sourcecontrol.resetActiveSelection();
            return;
        }
        CloneSource *cs = dynamic_cast<CloneSource *>(s);
        if (cs != nullptr) {
            Mixer::manager().setCurrentSource( cs->origin() );
            return;
        }
        RenderSource *rs = dynamic_cast<RenderSource *>(s);
        if (rs != nullptr) {
            Settings::application.widget.preview = true;
            return;
        }
        navigator.showPannelSource( Mixer::manager().indexCurrentSource() );
    }
}

void UserInterface::fillShaderEditor(const std::string &text)
{
    static bool initialized = false;
    if (!initialized) {
        auto lang = TextEditor::LanguageDefinition::GLSL();

        static const char* const keywords[] = {
            "discard", "attribute", "varying", "uniform", "in", "out", "inout", "bvec2", "bvec3", "bvec4", "dvec2",
            "dvec3", "dvec4", "ivec2", "ivec3", "ivec4", "uvec2", "uvec3", "uvec4", "vec2", "vec3", "vec4", "mat2",
            "mat3", "mat4", "dmat2", "dmat3", "dmat4", "sampler1D", "sampler2D", "sampler3D", "samplerCUBE", "samplerbuffer",
            "sampler1DArray", "sampler2DArray", "sampler1DShadow", "sampler2DShadow", "vec4", "vec4", "smooth", "flat",
            "precise", "coherent", "uint", "struct", "switch", "unsigned", "void", "volatile", "while", "readonly"
        };
        for (auto& k : keywords)
            lang.mKeywords.insert(k);

        static const char* const identifiers[] = {
            "radians",  "degrees",   "sin",  "cos", "tan", "asin", "acos", "atan", "pow", "exp2", "log2", "sqrt", "inversesqrt",
            "abs", "sign", "floor", "ceil", "fract", "mod", "min", "max", "clamp", "mix", "step", "smoothstep", "length", "distance",
            "dot", "cross", "normalize", "ftransform", "faceforward", "reflect", "matrixcompmult", "lessThan", "lessThanEqual",
            "greaterThan", "greaterThanEqual", "equal", "notEqual", "any", "all", "not", "texture1D", "texture1DProj", "texture1DLod",
            "texture1DProjLod", "texture", "texture2D", "texture2DProj", "texture2DLod", "texture2DProjLod", "texture3D",
            "texture3DProj", "texture3DLod", "texture3DProjLod", "textureCube", "textureCubeLod", "shadow1D", "shadow1DProj",
            "shadow1DLod", "shadow1DProjLod", "shadow2D", "shadow2DProj", "shadow2DLod", "shadow2DProjLod",
            "dFdx", "dFdy", "fwidth", "noise1", "noise2", "noise3", "noise4", "refract", "exp", "log", "mainImage",
        };
        for (auto& k : identifiers)
        {
            TextEditor::Identifier id;
            id.mDeclaration = "Added function";
            lang.mIdentifiers.insert(std::make_pair(std::string(k), id));
        }
        // init editor
        editor.SetLanguageDefinition(lang);
        initialized = true;
    }

    // remember text
    currentTextEdit = text;
    // fill editor
    editor.SetText(currentTextEdit);
}

void UserInterface::RenderShaderEditor()
{
    static bool show_statusbar = true;

    if ( !ImGui::Begin(IMGUI_TITLE_SHADEREDITOR, &Settings::application.widget.shader_editor, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_MenuBar) )
    {
        ImGui::End();
        return;
    }

    ImGui::SetWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Edit"))
        {
            bool ro = editor.IsReadOnly();
            if (ImGui::MenuItem("Read-only mode", nullptr, &ro))
                editor.SetReadOnly(ro);
            ImGui::Separator();

            if (ImGui::MenuItem( ICON_FA_UNDO " Undo", CTRL_MOD "Z", nullptr, !ro && editor.CanUndo()))
                editor.Undo();
            if (ImGui::MenuItem( ICON_FA_REDO " Redo", CTRL_MOD "Y", nullptr, !ro && editor.CanRedo()))
                editor.Redo();

            ImGui::Separator();

            if (ImGui::MenuItem( ICON_FA_COPY " Copy", CTRL_MOD "C", nullptr, editor.HasSelection()))
                editor.Copy();
            if (ImGui::MenuItem( ICON_FA_CUT " Cut", CTRL_MOD "X", nullptr, !ro && editor.HasSelection()))
                editor.Cut();
            if (ImGui::MenuItem( ICON_FA_ERASER " Delete", "Del", nullptr, !ro && editor.HasSelection()))
                editor.Delete();
            if (ImGui::MenuItem( ICON_FA_PASTE " Paste", CTRL_MOD "V", nullptr, !ro && ImGui::GetClipboardText() != nullptr))
                editor.Paste();

            ImGui::Separator();

            if (ImGui::MenuItem( "Select all", nullptr, nullptr))
                editor.SetSelection(TextEditor::Coordinates(), TextEditor::Coordinates(editor.GetTotalLines(), 0));

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            bool ws = editor.IsShowingWhitespaces();
            if (ImGui::MenuItem( ICON_FA_LONG_ARROW_ALT_RIGHT " Whitespace", nullptr, &ws))
                editor.SetShowWhitespaces(ws);
            ImGui::MenuItem( ICON_FA_WINDOW_MAXIMIZE  " Statusbar", nullptr, &show_statusbar);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    if (show_statusbar) {
        auto cpos = editor.GetCursorPosition();
        ImGui::Text("%6d/%-6d %6d lines  | %s | %s | %s ", cpos.mLine + 1, cpos.mColumn + 1, editor.GetTotalLines(),
                    editor.IsOverwrite() ? "Ovr" : "Ins",
                    editor.CanUndo() ? "*" : " ",
                    editor.GetLanguageDefinition().mName.c_str());
    }

    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_MONO);
    editor.Render("ShaderEditor");
    ImGui::PopFont();

    ImGui::End();
}

void UserInterface::RenderMetrics(bool *p_open, int* p_corner, int *p_mode)
{
    if (!p_corner || !p_open)
        return;

    const float DISTANCE = 10.0f;
    int corner = *p_corner;
    ImGuiIO& io = ImGui::GetIO();
    if (corner != -1)
    {
        ImVec2 window_pos = ImVec2((corner & 1) ? io.DisplaySize.x - DISTANCE : DISTANCE, (corner & 2) ? io.DisplaySize.y - DISTANCE : DISTANCE);
        ImVec2 window_pos_pivot = ImVec2((corner & 1) ? 1.0f : 0.0f, (corner & 2) ? 1.0f : 0.0f);
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
    }

    ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background

    if (!ImGui::Begin("Metrics", NULL, (corner != -1 ? ImGuiWindowFlags_NoMove : 0) | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
    {
        ImGui::End();
        return;
    }

    ImGui::SetNextItemWidth(200);
    ImGui::Combo("##mode", p_mode,
                 ICON_FA_TACHOMETER_ALT "  Performance\0"
                 ICON_FA_HOURGLASS_HALF "  Runtime\0"
                 ICON_FA_VECTOR_SQUARE  "  Source\0");

    ImGui::SameLine();
    if (ImGuiToolkit::IconButton(5,8))
        ImGui::OpenPopup("metrics_menu");
    ImGui::Spacing();

    if (*p_mode > 1) {
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_MONO);
        Source *s = Mixer::manager().currentSource();
        if (s) {
            float rightalign = -2.5f * ImGui::GetTextLineHeightWithSpacing();
            std::ostringstream info;
            info << s->name() << ": ";

            float v = s->alpha();
            ImGui::SetNextItemWidth(rightalign);
            if ( ImGui::DragFloat("Alpha", &v, 0.01f, 0.f, 1.f) )
                s->setAlpha(v);
            if ( ImGui::IsItemDeactivatedAfterEdit() ) {
                info << "Alpha " << std::fixed << std::setprecision(3) << v;
                Action::manager().store(info.str());
            }

            Group *n = s->group(View::GEOMETRY);
            float translation[2] = { n->translation_.x, n->translation_.y};
            ImGui::SetNextItemWidth(rightalign);
            if ( ImGui::DragFloat2("Pos", translation, 0.01f, -MAX_SCALE, MAX_SCALE, "%.2f") )  {
                n->translation_.x = translation[0];
                n->translation_.y = translation[1];
                s->touch();
            }
            if ( ImGui::IsItemDeactivatedAfterEdit() ){
                info << "Position " << std::setprecision(3) << n->translation_.x << ", " << n->translation_.y;
                Action::manager().store(info.str());
            }
            float scale[2] = { n->scale_.x, n->scale_.y} ;
            ImGui::SetNextItemWidth(rightalign);
            if ( ImGui::DragFloat2("Scale", scale, 0.01f, -MAX_SCALE, MAX_SCALE, "%.2f") )
            {
                n->scale_.x = CLAMP_SCALE(scale[0]);
                n->scale_.y = CLAMP_SCALE(scale[1]);
                s->touch();
            }
            if ( ImGui::IsItemDeactivatedAfterEdit() ){
                info << "Scale " << std::setprecision(3) << n->scale_.x << " x " << n->scale_.y;
                Action::manager().store(info.str());
            }

            ImGui::SetNextItemWidth(rightalign);
            if ( ImGui::SliderAngle("Angle", &(n->rotation_.z), -180.f, 180.f) )
                s->touch();
            if ( ImGui::IsItemDeactivatedAfterEdit() ) {
                info << "Angle " << std::setprecision(3) << n->rotation_.z * 180.f / M_PI;
                Action::manager().store(info.str());
            }
        }
        else
            ImGui::Text("No source selected");
        ImGui::PopFont();
    }
    else if (*p_mode > 0) {
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_MONO);
        ImGui::Text("Session  %s", GstToolkit::time_to_string(Mixer::manager().session()->runtime(), GstToolkit::TIME_STRING_READABLE).c_str());
        uint64_t time = Runtime();
        ImGui::Text("Program  %s", GstToolkit::time_to_string(time, GstToolkit::TIME_STRING_READABLE).c_str());
        time += Settings::application.total_runtime;
        ImGui::Text("Total    %s", GstToolkit::time_to_string(time, GstToolkit::TIME_STRING_READABLE).c_str());
        ImGui::PopFont();
    }
    else {
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_MONO);
        ImGui::Text("Window  %.0f x %.0f", io.DisplaySize.x, io.DisplaySize.y);
        //        ImGui::Text("HiDPI (retina) %s", io.DisplayFramebufferScale.x > 1.f ? "on" : "off");
        ImGui::Text("Refresh %.1f FPS", io.Framerate);
        ImGui::Text("Memory  %s", BaseToolkit::byte_to_string( SystemToolkit::memory_usage()).c_str() );
        ImGui::PopFont();

    }

    if (ImGui::BeginPopup("metrics_menu"))
    {
        ImGui::TextDisabled("Metrics");
        if (ImGui::MenuItem( ICON_FA_ANGLE_UP "  Top",    NULL, corner == 1)) *p_corner = 1;
        if (ImGui::MenuItem( ICON_FA_ANGLE_DOWN "  Bottom", NULL, corner == 3)) *p_corner = 3;
        if (ImGui::MenuItem( ICON_FA_EXPAND_ARROWS_ALT " Free position", NULL, corner == -1)) *p_corner = -1;
        if (p_open && ImGui::MenuItem( ICON_FA_TIMES "  Close")) *p_open = false;
        ImGui::EndPopup();
    }

    ImGui::End();
}

void UserInterface::RenderAbout(bool* p_open)
{
    ImGui::SetNextWindowPos(ImVec2(1000, 20), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("About " APP_TITLE, p_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::End();
        return;
    }

#ifdef VIMIX_VERSION_MAJOR
    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_BOLD);
    ImGui::Text("%s %d.%d.%d", APP_NAME, VIMIX_VERSION_MAJOR, VIMIX_VERSION_MINOR, VIMIX_VERSION_PATCH);
    ImGui::PopFont();
#else
    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_BOLD);
    ImGui::Text("%s", APP_NAME);
    ImGui::PopFont();
#endif

    ImGui::Separator();
    ImGui::Text("vimix performs graphical mixing and blending of\nseveral movie clips and computer generated graphics,\nwith image processing effects in real-time.");
    ImGui::Text("\nvimix is licensed under GNU GPL version 3 or later.\n" UNICODE_COPYRIGHT " 2019-2021 Bruno Herbelin.");

    ImGui::Spacing();
    ImGuiToolkit::ButtonOpenUrl("Visit vimix website", "https://brunoherbelin.github.io/vimix/", ImVec2(ImGui::GetContentRegionAvail().x, 0));
    ImGuiToolkit::ButtonOpenUrl("User Manual", "https://github.com/brunoherbelin/vimix/wiki/User-manual", ImVec2(ImGui::GetContentRegionAvail().x, 0));


    ImGui::Spacing();
    ImGui::Text("\nvimix is built using the following libraries:");

//    tinyfd_inputBox("tinyfd_query", NULL, NULL);
//    ImGui::Text("- Tinyfiledialogs v%s mode '%s'", tinyfd_version, tinyfd_response);

    ImGui::Columns(3, "abouts");
    ImGui::Separator();

    ImGui::Text("Dear ImGui");
    ImGui::PushID("dearimguiabout");
    if ( ImGui::Button("More info", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
        show_imgui_about = true;
    ImGui::PopID();

    ImGui::NextColumn();

    ImGui::Text("GStreamer");
    ImGui::PushID("gstreamerabout");
    if ( ImGui::Button("More info", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
        show_gst_about = true;
    ImGui::PopID();

    ImGui::NextColumn();

    ImGui::Text("OpenGL");
    ImGui::PushID("openglabout");
    if ( ImGui::Button("More info", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
        show_opengl_about = true;
    ImGui::PopID();

    ImGui::Columns(1);


    ImGui::End();
}

void UserInterface::showPannel(int id)
{
    if (id == NAV_MENU)
        navigator.togglePannelMenu();
    else if (id == NAV_NEW)
        navigator.togglePannelNew();
    else
        navigator.showPannelSource(id);
}

void UserInterface::RenderNotes()
{
    Session *se = Mixer::manager().session();
    if (se!=nullptr) {

        ImVec4 color = ImGui::GetStyle().Colors[ImGuiCol_ResizeGripHovered];
        color.w = 0.35f;
        ImGui::PushStyleColor(ImGuiCol_WindowBg, color);
        ImGui::PushStyleColor(ImGuiCol_TitleBg, color);
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, color);
        ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, color);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4());

        for (auto note = se->beginNotes(); note != se->endNotes(); ) {
            // detect close clic
            bool close = false;

            if ( (*note).stick < 1 || (*note).stick == Settings::application.current_view)
            {
                // window
                ImGui::SetNextWindowSizeConstraints(ImVec2(150, 150), ImVec2(500, 500));
                ImGui::SetNextWindowPos(ImVec2( (*note).pos.x, (*note).pos.y ), ImGuiCond_Once);
                ImGui::SetNextWindowSize(ImVec2((*note).size.x, (*note).size.y), ImGuiCond_Once);
                ImGui::SetNextWindowBgAlpha(color.w); // Transparent background

                // draw
                if (ImGui::Begin((*note).label.c_str(), NULL, ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing |
                                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings))
                {
                    ImVec2 size = ImGui::GetContentRegionAvail();
                    ImVec2 pos = ImGui::GetCursorPos();
                    // close & delete
                    if (ImGuiToolkit::IconButton(4,16)) close = true;
                    if (ImGui::IsItemHovered())
                        ImGuiToolkit::ToolTip("Delete");

                    if (ImGui::IsWindowFocused()) {
                        // font size
                        pos.x = size.x - 2.f * ImGui::GetTextLineHeightWithSpacing();
                        ImGui::SetCursorPos( pos );
                        if (ImGuiToolkit::IconButton(1, 13) )
                            (*note).large = !(*note).large ;
                        // stick to views icon
                        pos.x = size.x - ImGui::GetTextLineHeightWithSpacing() + 8.f;
                        ImGui::SetCursorPos( pos );
                        bool s = (*note).stick > 0;
                        if (ImGuiToolkit::IconToggle(5, 2, 4, 2, &s) )
                            (*note).stick = s ? Settings::application.current_view : 0;
                    }

                    // Text area
                    size.y -= ImGui::GetTextLineHeightWithSpacing() + 2.f;
                    ImGuiToolkit::PushFont( (*note).large ? ImGuiToolkit::FONT_LARGE : ImGuiToolkit::FONT_MONO );
                    ImGuiToolkit::InputTextMultiline("##notes", &(*note).text, size);
                    ImGui::PopFont();

                    // TODO smart detect when window moves
                    ImVec2 p = ImGui::GetWindowPos();
                    (*note).pos = glm::vec2( p.x, p.y);
                    p = ImGui::GetWindowSize();
                    (*note).size = glm::vec2( p.x, p.y);

                    ImGui::End();
                }
            }
            // loop
            if (close)
                note = se->deleteNote(note);
            else
                ++note;
        }


        ImGui::PopStyleColor(5);
    }

}

///
/// TOOLBOX
///
ToolBox::ToolBox()
{
    show_demo_window = false;
    show_icons_window = false;
    show_sandbox = false;
}

void ToolBox::Render()
{
    static bool record_ = false;
    static std::ofstream csv_file_;

    // first run
    ImGui::SetNextWindowPos(ImVec2(40, 40), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(350, 300), ImVec2(FLT_MAX, FLT_MAX));
    if ( !ImGui::Begin(IMGUI_TITLE_TOOLBOX, &Settings::application.widget.toolbox,  ImGuiWindowFlags_MenuBar) )
    {
        ImGui::End();
        return;
    }

    // Menu Bar
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Render"))
        {
            if ( ImGui::MenuItem( ICON_FA_CAMERA_RETRO "  Screenshot", "F12") )
                UserInterface::manager().StartScreenshot();

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Gui"))
        {
            ImGui::MenuItem("Sandbox", nullptr, &show_sandbox);
            ImGui::MenuItem("Icons", nullptr, &show_icons_window);
            ImGui::MenuItem("Demo ImGui", nullptr, &show_demo_window);

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Stats"))
        {
            if (ImGui::MenuItem("Record", nullptr, &record_) )
            {
                if ( record_ )
                    csv_file_.open( SystemToolkit::home_path() + std::to_string(BaseToolkit::uniqueId()) + ".csv", std::ofstream::out | std::ofstream::app);
                else
                    csv_file_.close();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    //
    // display histogram of update time and plot framerate
    //
    // keep array of 180 values, i.e. approx 3 seconds of recording
    static float recorded_values[3][PLOT_ARRAY_SIZE] = {{}};
    static float recorded_sum[3] = { 0.f, 0.f, 0.f };
    static float recorded_bounds[3][2] = {  {40.f, 65.f}, {0.f, 50.f}, {0.f, 50.f} };
    static float refresh_rate = -1.f;
    static int   values_index = 0;
    float megabyte = static_cast<float>( static_cast<double>(SystemToolkit::memory_usage()) / 1048576.0 );

    // init
    if (refresh_rate < 0.f) {

        const GLFWvidmode* mode = glfwGetVideoMode(Rendering::manager().outputWindow().monitor());
        refresh_rate = float(mode->refreshRate);
        if (Settings::application.render.vsync > 0)
            refresh_rate /= Settings::application.render.vsync;
        else
            refresh_rate = 0.f;
        recorded_bounds[0][0] = refresh_rate - 15.f; // min fps
        recorded_bounds[0][1] = refresh_rate + 10.f;  // max

        for(int i = 0; i<PLOT_ARRAY_SIZE; ++i) {
            recorded_values[0][i] = refresh_rate;
            recorded_sum[0] += recorded_values[0][i];
            recorded_values[1][i] = 1.f / refresh_rate;
            recorded_sum[1] += recorded_values[1][i];
            recorded_values[2][i] = megabyte;
            recorded_sum[2] += recorded_values[2][i];
        }
    }

    // compute average step 1: remove previous value from the sum
    recorded_sum[0] -= recorded_values[0][values_index];
    recorded_sum[1] -= recorded_values[1][values_index];
    recorded_sum[2] -= recorded_values[2][values_index];

    // store values
    recorded_values[0][values_index] = MINI(ImGui::GetIO().Framerate, 1000.f);
    recorded_values[1][values_index] = MINI(Mixer::manager().dt(), 100.f);
    recorded_values[2][values_index] = megabyte;

    // compute average step 2: add current value to the sum
    recorded_sum[0] += recorded_values[0][values_index];
    recorded_sum[1] += recorded_values[1][values_index];
    recorded_sum[2] += recorded_values[2][values_index];

    // move inside array
    values_index = (values_index+1) % PLOT_ARRAY_SIZE;

    // non-vsync fixed FPS : have to calculate plot dimensions based on past values
    if (refresh_rate < 1.f) {
        recorded_bounds[0][0] = recorded_sum[0] / float(PLOT_ARRAY_SIZE) - 15.f;
        recorded_bounds[0][1] = recorded_sum[0] / float(PLOT_ARRAY_SIZE) + 10.f;
    }

    recorded_bounds[2][0] = recorded_sum[2] / float(PLOT_ARRAY_SIZE) - 400.f;
    recorded_bounds[2][1] = recorded_sum[2] / float(PLOT_ARRAY_SIZE) + 300.f;

    // plot values, with title overlay to display the average
    ImVec2 plot_size = ImGui::GetContentRegionAvail();
    plot_size.y *= 0.32;
    char overlay[128];
    sprintf(overlay, "Rendering %.1f FPS", recorded_sum[0] / float(PLOT_ARRAY_SIZE));
    ImGui::PlotLines("LinesRender", recorded_values[0], PLOT_ARRAY_SIZE, values_index, overlay, recorded_bounds[0][0], recorded_bounds[0][1], plot_size);
    sprintf(overlay, "Update time %.1f ms (%.1f FPS)", recorded_sum[1] / float(PLOT_ARRAY_SIZE), (float(PLOT_ARRAY_SIZE) * 1000.f) / recorded_sum[1]);
    ImGui::PlotHistogram("LinesMixer", recorded_values[1], PLOT_ARRAY_SIZE, values_index, overlay, recorded_bounds[1][0], recorded_bounds[1][1], plot_size);
    sprintf(overlay, "Memory %.1f MB / %s", recorded_values[2][(values_index+PLOT_ARRAY_SIZE-1) % PLOT_ARRAY_SIZE], BaseToolkit::byte_to_string(SystemToolkit::memory_max_usage()).c_str() );
    ImGui::PlotLines("LinesMemo", recorded_values[2], PLOT_ARRAY_SIZE, values_index, overlay, recorded_bounds[2][0], recorded_bounds[2][1], plot_size);

    ImGui::End();

    // save to file
    if ( record_ && csv_file_.is_open()) {
            csv_file_ << megabyte << ", " << ImGui::GetIO().Framerate << std::endl;
    }

    // About and other utility windows
    if (show_icons_window)
        ImGuiToolkit::ShowIconsWindow(&show_icons_window);
    if (show_sandbox)
        ShowSandbox(&show_sandbox);
    if (show_demo_window)
        ImGui::ShowDemoWindow(&show_demo_window);

}


///
/// SESSION REPAIR WINDOW
///
HelperToolbox::HelperToolbox()
{

}

void HelperToolbox::Render()
{
    // first run
    ImGui::SetNextWindowPos(ImVec2(40, 40), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(350, 300), ImVec2(FLT_MAX, FLT_MAX));
    if ( !ImGui::Begin(IMGUI_TITLE_HELP, &Settings::application.widget.help) )
    {
        ImGui::End();
        return;
    }

    if (ImGui::CollapsingHeader("File repair"))
    {

    }

    if (ImGui::CollapsingHeader("Keyboard shortcuts"))
    {
        ImGui::Columns(2, "mycolumns"); // 4-ways, with border
        ImGui::Text("HOME"); ImGui::NextColumn();
        ImGui::Text("Main menu"); ImGui::NextColumn();
        ImGui::Text("INS"); ImGui::NextColumn();
        ImGui::Text("New source"); ImGui::NextColumn();
        ImGui::Text("F1"); ImGui::NextColumn();
        ImGui::Text("Mixing view"); ImGui::NextColumn();
        ImGui::Text("F2"); ImGui::NextColumn();
        ImGui::Text("Geometry view"); ImGui::NextColumn();
        ImGui::Text("F3"); ImGui::NextColumn();
        ImGui::Text("Layers view"); ImGui::NextColumn();
        ImGui::Text("F4"); ImGui::NextColumn();
        ImGui::Text("Texturing view"); ImGui::NextColumn();
        ImGui::Text(CTRL_MOD "TAB"); ImGui::NextColumn();
        ImGui::Text("Change view"); ImGui::NextColumn();
        ImGui::Separator();
        ImGui::Text("Ctrl + O"); ImGui::NextColumn();
        ImGui::Text("Open Session file"); ImGui::NextColumn();
        ImGui::Separator();

        ImGui::Columns(1);
    }

    ImGui::End();

}

///
/// SOURCE CONTROLLER
///
SourceController::SourceController() : focused_(false), min_width_(0.f), h_space_(0.f), v_space_(0.f), scrollbar_(0.f),
    timeline_height_(0.f),  mediaplayer_height_(0.f), buttons_width_(0.f), buttons_height_(0.f),
    play_toggle_request_(false), replay_request_(false), pending_(false),
    active_label_(LABEL_AUTO_MEDIA_PLAYER), active_selection_(-1),
    selection_context_menu_(false), selection_mediaplayer_(nullptr), selection_target_slower_(0), selection_target_faster_(0),
    mediaplayer_active_(nullptr), mediaplayer_edit_fading_(false), mediaplayer_mode_(false), mediaplayer_slider_pressed_(false), mediaplayer_timeline_zoom_(1.f)
{
    info_.setExtendedStringMode();
}


void SourceController::resetActiveSelection()
{
    info_.reset();
    active_selection_ = -1;
    active_label_ = LABEL_AUTO_MEDIA_PLAYER;
}


bool SourceController::Visible() const
{
    return ( Settings::application.widget.media_player
             && (Settings::application.widget.media_player_view < 0 || Settings::application.widget.media_player_view == Settings::application.current_view )
             );
}

void SourceController::Update()
{
    SourceList selectedsources;

    // get new selection or validate previous list if selection was not updated
    selectedsources = selection_;
    if (selectedsources.empty() && !Mixer::selection().empty())
        selectedsources = playable_only(Mixer::selection().getCopy());
    size_t n_source = selectedsources.size();
//    selection_.clear();

    size_t n_play = 0;
    for (auto source = selectedsources.begin(); source != selectedsources.end(); ++source){
        if ( (*source)->active() && (*source)->playing() )
            n_play++;
    }

    // Play button or keyboard [space] was pressed
    if ( play_toggle_request_ ) {

        for (auto source = selectedsources.begin(); source != selectedsources.end(); ++source)
            (*source)->play( n_play < n_source );

        play_toggle_request_ = false;
    }

    // Replay / rewind button or keyboard [B] was pressed
    if ( replay_request_ ) {

        for (auto source = selectedsources.begin(); source != selectedsources.end(); ++source)
            (*source)->replay();

        replay_request_ = false;
    }

    // reset on session change
    static Session *__session = nullptr;
    if ( Mixer::manager().session() != __session ) {
        __session = Mixer::manager().session();
        resetActiveSelection();
    }
}

void SourceController::Render()
{
    ImGui::SetNextWindowPos(ImVec2(1180, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);

    // estimate window size
    const ImGuiContext& g = *GImGui;
    h_space_ = g.Style.ItemInnerSpacing.x;
    v_space_ = g.Style.FramePadding.y;
    buttons_height_  = g.FontSize + v_space_ * 4.0f ;
    buttons_width_  = g.FontSize * 7.0f ;
    min_width_ = 6.f * buttons_height_;
    timeline_height_ = (g.FontSize + v_space_) * 2.0f ; // double line for each timeline
    scrollbar_ = g.Style.ScrollbarSize;
    // all together: 1 title bar + spacing + 1 toolbar + spacing + 2 timelines + scrollbar
    mediaplayer_height_ = buttons_height_ + 2.f * timeline_height_ + 2.f * scrollbar_ +  v_space_;

    // constraint position
    static ImVec2 source_window_pos = ImVec2(1180, 20);
    static ImVec2 source_window_size = ImVec2(400, 260);
    SetNextWindowVisible(source_window_pos, source_window_size);

    ImGui::SetNextWindowSizeConstraints(ImVec2(min_width_, 2.f * mediaplayer_height_), ImVec2(FLT_MAX, FLT_MAX));

    if ( !ImGui::Begin(IMGUI_TITLE_MEDIAPLAYER, &Settings::application.widget.media_player, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar |  ImGuiWindowFlags_NoCollapse ))
    {
        ImGui::End();
        return;
    }
    source_window_pos = ImGui::GetWindowPos();
    source_window_size = ImGui::GetWindowSize();
    focused_ = ImGui::IsWindowFocused();

    // menu (no title bar)
    if (ImGui::BeginMenuBar())
    {
        if (ImGuiToolkit::IconButton(4,16)){
            Settings::application.widget.media_player = false;
            selection_.clear();
        }
        if (ImGui::BeginMenu(IMGUI_TITLE_MEDIAPLAYER))
        {
            // Menu section for play control
            if (ImGui::MenuItem( ICON_FA_FAST_BACKWARD "  Back", "B"))
                replay_request_ = true;
            if (ImGui::MenuItem( ICON_FA_PLAY "  Play | Pause", "Space"))
                play_toggle_request_ = true;

            // Menu section for list
            ImGui::Separator();
            if (ImGui::MenuItem( ICON_FA_TH "  List all")) {
                selection_.clear();
                resetActiveSelection();
                Mixer::manager().unsetCurrentSource();
                Mixer::selection().clear();
                selection_ = playable_only( Mixer::manager().session()->getDepthSortedList() );
            }
            if (ImGui::MenuItem( ICON_FA_WIND "  Clear")) {
                selection_.clear();
                resetActiveSelection();
                Mixer::manager().unsetCurrentSource();
                Mixer::selection().clear();
            }
            // Menu section for window management
            ImGui::Separator();
            bool pinned = Settings::application.widget.media_player_view == Settings::application.current_view;
            if ( ImGui::MenuItem( ICON_FA_MAP_PIN "    Pin window to view", nullptr, &pinned) ){
                if (pinned)
                    Settings::application.widget.media_player_view = Settings::application.current_view;
                else
                    Settings::application.widget.media_player_view = -1;
            }

            if ( ImGui::MenuItem( ICON_FA_TIMES "   Close", CTRL_MOD "P") ) {
                Settings::application.widget.media_player = false;
                selection_.clear();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(active_label_.c_str()))
        {
            // info on selection status
            size_t N = Mixer::manager().session()->numPlayGroups();
            bool enabled = !selection_.empty() && active_selection_ < 0;

            // Menu : Dynamic selection
            if (ImGui::MenuItem(LABEL_AUTO_MEDIA_PLAYER))
                resetActiveSelection();
            // Menu : store selection
            if (ImGui::MenuItem(ICON_FA_PLUS_SQUARE LABEL_STORE_SELECTION, NULL, false, enabled))
            {
                active_selection_ = N;
                active_label_ = std::string(ICON_FA_CHECK_SQUARE "  Selection #") + std::to_string(active_selection_);
                Mixer::manager().session()->addPlayGroup( ids(playable_only(selection_)) );
                info_.reset();
            }
            // Menu : list of selections
            if (N>0) {
                ImGui::Separator();
                for (size_t i = 0 ; i < N; ++i)
                {
                    std::string label = std::string(ICON_FA_CHECK_SQUARE "  Selection #") + std::to_string(i);
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

        if (mediaplayer_active_) {
            if (ImGui::BeginMenu(ICON_FA_FILM " Video") )
            {
                if (ImGui::MenuItem(ICON_FA_WINDOW_CLOSE "  Reset timeline")){
                    mediaplayer_timeline_zoom_ = 1.f;
                    mediaplayer_active_->timeline()->clearFading();
                    mediaplayer_active_->timeline()->clearGaps();
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
                    if (ImGuiToolkit::MenuItemIcon(5, 13, " Not synchronized", active ))
                        mediaplayer_active_->setSyncToMetronome(Metronome::SYNC_NONE);
                    active = sync == Metronome::SYNC_BEAT;
                    if (ImGuiToolkit::MenuItemIcon(6, 13, " Sync to beat", active ))
                        mediaplayer_active_->setSyncToMetronome(Metronome::SYNC_BEAT);
                    active = sync == Metronome::SYNC_PHASE;
                    if (ImGuiToolkit::MenuItemIcon(7, 13, " Sync to phase", active ))
                        mediaplayer_active_->setSyncToMetronome(Metronome::SYNC_PHASE);
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu(ICON_FA_SNOWFLAKE "   Deactivation"))
                {
                    bool option = !mediaplayer_active_->rewindOnDisabled();
                    if (ImGui::MenuItem(ICON_FA_STOP "  Stop", NULL, &option ))
                        mediaplayer_active_->setRewindOnDisabled(false);
                    option = mediaplayer_active_->rewindOnDisabled();
                    if (ImGui::MenuItem(ICON_FA_FAST_BACKWARD "  Rewind & Stop", NULL, &option ))
                        mediaplayer_active_->setRewindOnDisabled(true);
                    ImGui::EndMenu();
                }

//                if (ImGui::BeginMenu(ICON_FA_CUT "  Auto cut" ))
//                {
//                    if (ImGuiToolkit::MenuItemIcon(14, 12,  "Cut faded areas" ))
//                        if (mediaplayer_active_->timeline()->autoCut()){
//                            std::ostringstream oss;
//                            oss << SystemToolkit::base_filename( mediaplayer_active_->filename() );
//                            oss << ": Cut faded areas";
//                            Action::manager().store(oss.str());
//                        }
//                    ImGui::EndMenu();
//                }
                if (Settings::application.render.gpu_decoding)
                {
                    ImGui::Separator();
                    if (ImGui::BeginMenu(ICON_FA_MICROCHIP "  Hardware decoding"))
                    {
                        bool hwdec = !mediaplayer_active_->softwareDecodingForced();
                        if (ImGui::MenuItem("Auto", "", &hwdec ))
                            mediaplayer_active_->setSoftwareDecodingForced(false);
                        hwdec = mediaplayer_active_->softwareDecodingForced();
                        if (ImGui::MenuItem("Disabled", "", &hwdec ))
                            mediaplayer_active_->setSoftwareDecodingForced(true);
                        ImGui::EndMenu();
                    }
                }

                ImGui::EndMenu();
            }
        }
        else {
            ImGui::SameLine(0, 2.f * g.Style.ItemSpacing.x );
            ImGui::TextDisabled(ICON_FA_FILM " Video");
        }

        ImGui::EndMenuBar();
    }

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
    const ImRect timescale_bbox( timescale_pos, timescale_pos + timeline_size);

    ImGuiToolkit::RenderTimeline(window, timescale_bbox, 0, duration, 1000, true);

}

std::list< std::pair<float, guint64> > DrawTimeline(const char* label, Timeline *timeline, guint64 time,
                                                    double width_ratio, float height, double tempo = 0, double quantum = 0)
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
        if (tempo > 0 && quantum > 0)
            ImGuiToolkit::RenderTimelineBPM(window, section_bbox, tempo, quantum, section->begin, section->end, timeline->step());
        else
            ImGuiToolkit::RenderTimeline(window, section_bbox, section->begin, section->end, timeline->step());

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

void SourceController::RenderSelection(size_t i)
{
    ImVec2 top = ImGui::GetCursorScreenPos();
    ImVec2 rendersize = ImGui::GetContentRegionAvail() - ImVec2(0, buttons_height_ + scrollbar_ + v_space_);
    ImVec2 bottom = ImVec2(top.x, top.y + rendersize.y + v_space_);

    selection_ = Mixer::manager().session()->playGroup(i);
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
            if (ms != nullptr)
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

            // draw play time scale if a duration is set
            if (maxduration > 0) {
                ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2( maxframewidth + h_space_, 0));
                DrawTimeScale("##timescale", maxduration, width_ratio);
            }

            // loop over all sources
            for (auto source = selection_.begin(); source != selection_.end(); ++source) {

                ImVec2 framesize(1.5f * timeline_height_ * (*source)->frame()->aspectRatio(), 1.5f * timeline_height_);
                ImVec2 image_top = ImGui::GetCursorPos();

                // Thumbnail of source (clic to open)
                if (SourceButton(*source, framesize))
                    UserInterface::manager().showSourceEditor(*source);

                // text below thumbnail to show status
                ImGuiToolkit::PushFont(ImGuiToolkit::FONT_MONO);
                ImGui::Text("%s %s", SourcePlayIcon(*source), GstToolkit::time_to_string((*source)->playtime()).c_str() );
                ImGui::PopFont();

                // get media source
                MediaSource *ms = dynamic_cast<MediaSource *>(*source);
                if (ms != nullptr) {
                    // ok, get the media player of the media source
                    MediaPlayer *mp = ms->mediaplayer();

                    // start to draw timeline aligned at maximum frame width + horizontal space
                    ImVec2 pos = image_top + ImVec2( maxframewidth + h_space_, 0);
                    ImGui::SetCursorPos(pos);

                    // draw the mediaplayer's timeline, with the indication of cursor position
                    // NB: use the same width/time ratio for all to ensure timing vertical correspondance

                    if (mp->syncToMetronome() > Metronome::SYNC_NONE)
                        DrawTimeline("##timeline_mediaplayer_bpm", mp->timeline(), mp->position(),
                                     width_ratio / fabs(mp->playSpeed()), framesize.y,
                                     Metronome::manager().tempo(), Metronome::manager().quantum());
                    else
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

                }

                // next line position
                ImGui::SetCursorPos(image_top + ImVec2(0, 2.0f * timeline_height_ + 2.f * v_space_));
            }

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
        std::string label = std::string(ICON_FA_CHECK_SQUARE "  ") + std::to_string(numsources) + ( numsources > 1 ? " sources" : " source");
        if (ImGui::BeginCombo("##SelectionImport", label.c_str()))
        {
            // select all playable sources
            for (auto s = Mixer::manager().session()->begin(); s != Mixer::manager().session()->end(); ++s) {
                if ( (*s)->playable() ) {
                    if (std::find(selection_.begin(),selection_.end(),*s) == selection_.end()) {
                        if (ImGui::MenuItem( (*s)->name().c_str() , 0, false ))
                            Mixer::manager().session()->addToPlayGroup(i, *s);
                    }
                    else {
                        if (ImGui::MenuItem( (*s)->name().c_str(), 0, true ))
                            Mixer::manager().session()->removeFromPlayGroup(i, *s);
                    }
                }
            }
            ImGui::EndCombo();
        }
    }

    ImGui::SameLine();
    ImGui::SetCursorPosX(rendersize.x - buttons_height_ / 1.3f);
    if (ImGui::Button(ICON_FA_MINUS_SQUARE)) {
        resetActiveSelection();
        Mixer::manager().session()->deletePlayGroup(i);
    }

    ImGui::PopStyleColor(4);
}

void SourceController::RenderSelectionContextMenu()
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

        if ( ImGuiToolkit::MenuItemIcon(14, 16,  ICON_FA_CARET_LEFT " Accelerate" , false, fabs(selection_target_faster_) > 0 )){
            selection_mediaplayer_->setPlaySpeed( selection_target_faster_ );
            info << ": Speed x" << std::setprecision(3) << selection_target_faster_;
            Action::manager().store(info.str());
        }
        if ( ImGuiToolkit::MenuItemIcon(15, 16,  "Slow down " ICON_FA_CARET_RIGHT , false, fabs(selection_target_slower_) > 0 )){
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


bool SourceController::SourceButton(Source *s, ImVec2 framesize)
{
    bool ret = false;

    ImVec2 frame_top = ImGui::GetCursorScreenPos();
    ImGui::Image((void*)(uintptr_t) s->texture(), framesize);
    frame_top.x += 1.f;
    ImGui::SetCursorScreenPos(frame_top);

    ImGui::PushID(s->id());
    ImGui::InvisibleButton("##sourcebutton", framesize);
    if (ImGui::IsItemClicked()) {
        ret = true;
    }
    if (ImGui::IsItemHovered()){
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRect(frame_top, frame_top + framesize - ImVec2(1.f, 0.f), ImGui::GetColorU32(ImGuiCol_Text), 0, 0, 3.f);
        frame_top.x += (framesize.x  - ImGui::GetTextLineHeight()) / 2.f;
        frame_top.y += (framesize.y  - ImGui::GetTextLineHeight()) / 2.f;
        draw_list->AddText(frame_top, ImGui::GetColorU32(ImGuiCol_Text), ICON_FA_CARET_SQUARE_RIGHT);
    }
    ImGui::PopID();

    return ret;
}

void SourceController::RenderSelectedSources()
{
    ImVec2 top = ImGui::GetCursorScreenPos();
    ImVec2 rendersize = ImGui::GetContentRegionAvail() - ImVec2(0, buttons_height_ + scrollbar_ + v_space_);
    ImVec2 bottom = ImVec2(top.x, top.y + rendersize.y + v_space_);

    // get new selection or validate previous list if selection was not updated
    if (Mixer::selection().empty())
        selection_ = Mixer::manager().validate(selection_);
    else
        selection_ = playable_only(Mixer::selection().getCopy());
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
                if (SourceButton(*source, framesize))
                    UserInterface::manager().showSourceEditor(*source);

                // Play icon lower left corner
                ImGuiToolkit::PushFont(framesize.x > 350.f ? ImGuiToolkit::FONT_LARGE : ImGuiToolkit::FONT_MONO);
                float h = ImGui::GetTextLineHeightWithSpacing();
                ImGui::SetCursorPos(image_top + ImVec2( h_space_, framesize.y - h));
                ImGui::Text("%s %s", SourcePlayIcon(*source),  GstToolkit::time_to_string((*source)->playtime()).c_str() );
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
        std::string label(ICON_FA_PLUS_SQUARE);
        if (space > buttons_width_) { // enough space to show full button with label text
            label += LABEL_STORE_SELECTION;
            width = buttons_width_;
        }
        ImGui::SameLine(0, space -width);
        ImGui::SetNextItemWidth(width);
        if (ImGui::Button( label.c_str() )) {
            active_selection_ = Mixer::manager().session()->numPlayGroups();
            active_label_ = std::string("Selection #") + std::to_string(active_selection_);
            Mixer::manager().session()->addPlayGroup( ids(playable_only(selection_)) );
        }
        if (space < buttons_width_ && ImGui::IsItemHovered())
            ImGuiToolkit::ToolTip(LABEL_STORE_SELECTION);

        ImGui::PopStyleColor(2);
    }

}

void SourceController::RenderSingleSource(Source *s)
{
    if ( s == nullptr)
        return;

    // in case of a MediaSource
    MediaSource *ms = dynamic_cast<MediaSource *>(s);
    if ( ms != nullptr ) {
        RenderMediaPlayer( ms->mediaplayer() );
    }
    else
    {
        ImVec2 top = ImGui::GetCursorScreenPos();
        ImVec2 rendersize = ImGui::GetContentRegionAvail() - ImVec2(0, buttons_height_ + scrollbar_ + v_space_);
        ImVec2 bottom = ImVec2(top.x, top.y + rendersize.y + v_space_);

        ///
        /// Centered frame
        ///
        FrameBuffer *frame = s->frame();
        ImVec2 framesize = rendersize;
        ImVec2 corner(0.f, 0.f);
        ImVec2 tmp = ImVec2(framesize.y * frame->aspectRatio(), framesize.x / frame->aspectRatio());
        if (tmp.x > framesize.x) {
            corner.y = (framesize.y - tmp.y) / 2.f;
            framesize.y = tmp.y;
        }
        else {
            corner.x = (framesize.x - tmp.x) / 2.f;
            framesize.x = tmp.x;
        }

        ///
        /// Image
        ///
        top += corner;
        ImGui::SetCursorScreenPos(top);
        ImGui::Image((void*)(uintptr_t) s->texture(), framesize);

        ///
        /// Info overlays
        ///
        ImGui::SetCursorScreenPos(top + ImVec2(framesize.x - ImGui::GetTextLineHeightWithSpacing(), v_space_));
        ImGui::Text(ICON_FA_INFO_CIRCLE);
        if (ImGui::IsItemHovered()){
            // fill info string
            s->accept(info_);

            float tooltip_height = 3.f * ImGui::GetTextLineHeightWithSpacing();
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            draw_list->AddRectFilled(top, top + ImVec2(framesize.x, tooltip_height), IMGUI_COLOR_OVERLAY);
            ImGui::SetCursorScreenPos(top + ImVec2(h_space_, v_space_));
            ImGui::Text("%s", info_.str().c_str());

            StreamSource *sts = dynamic_cast<StreamSource*>(s);
            if (sts && s->playing()) {
                ImGui::SetCursorScreenPos(top + ImVec2( framesize.x - 1.5f * buttons_height_, 0.5f * tooltip_height));
                ImGui::Text("%.1f Hz", sts->stream()->updateFrameRate());
            }
        }
        // Play icon lower left corner
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ImGui::SetCursorScreenPos(bottom + ImVec2(h_space_, -ImGui::GetTextLineHeightWithSpacing()));
        ImGui::Text("%s %s", SourcePlayIcon(s), GstToolkit::time_to_string(s->playtime()).c_str() );
        ImGui::PopFont();

        ///
        /// Play button bar
        ///
        DrawButtonBar(bottom, rendersize.x);
    }
}

void SourceController::RenderMediaPlayer(MediaPlayer *mp)
{
    mediaplayer_active_ = mp;

    // for action manager
    std::ostringstream oss;
    oss << SystemToolkit::base_filename( mediaplayer_active_->filename() );

    // for draw
    const float slider_zoom_width = timeline_height_ / 2.f;
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    const ImVec2 top = ImGui::GetCursorScreenPos();
    const ImVec2 rendersize = ImGui::GetContentRegionAvail() - ImVec2(0, mediaplayer_height_);
    ImVec2 bottom = ImVec2(top.x, top.y + rendersize.y + v_space_);

    ///
    /// Centered frame
    ///
    ImVec2 framesize = rendersize;
    ImVec2 corner(0.f, 0.f);
    ImVec2 tmp = ImVec2(framesize.y * mediaplayer_active_->aspectRatio(), framesize.x / mediaplayer_active_->aspectRatio());
    if (tmp.x > framesize.x) {
        corner.y = (framesize.y - tmp.y) / 2.f;
        framesize.y = tmp.y;
    }
    else {
        corner.x = (framesize.x - tmp.x) / 2.f;
        framesize.x = tmp.x;
    }

    ///
    /// Image
    ///
    const ImVec2 top_image = top + corner;
    ImGui::SetCursorScreenPos(top_image);
    ImGui::Image((void*)(uintptr_t) mediaplayer_active_->texture(), framesize);

    ///
    /// Info overlays
    ///
    ImGui::SetCursorScreenPos(top_image + ImVec2(framesize.x - ImGui::GetTextLineHeightWithSpacing(), v_space_));
    ImGui::Text(ICON_FA_INFO_CIRCLE);
    if (ImGui::IsItemHovered()){
        // information visitor
        mediaplayer_active_->accept(info_);
        float tooltip_height = 3.f * ImGui::GetTextLineHeightWithSpacing();
        draw_list->AddRectFilled(top_image, top_image + ImVec2(framesize.x, tooltip_height), IMGUI_COLOR_OVERLAY);
        ImGui::SetCursorScreenPos(top_image + ImVec2(h_space_, v_space_));
        ImGui::Text("%s", info_.str().c_str());

        // Icon to inform hardware decoding
        if ( mediaplayer_active_->decoderName().compare("software") != 0) {
            ImGui::SetCursorScreenPos(top_image + ImVec2( framesize.x - ImGui::GetTextLineHeightWithSpacing(), 0.35f * tooltip_height));
            ImGui::Text(ICON_FA_MICROCHIP);
        }

        // refresh frequency
        if ( mediaplayer_active_->isPlaying()) {
            ImGui::SetCursorScreenPos(top_image + ImVec2( framesize.x - 1.5f * buttons_height_, 0.667f * tooltip_height));
            ImGui::Text("%.1f Hz", mediaplayer_active_->updateFrameRate());
        }
    }

    // Play icon lower left corner
    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
    ImGui::SetCursorScreenPos(bottom + ImVec2(h_space_, -ImGui::GetTextLineHeightWithSpacing()));
    if (mediaplayer_active_->isEnabled())
        ImGui::Text("%s %s", mediaplayer_active_->isPlaying() ? ICON_FA_PLAY : ICON_FA_PAUSE, GstToolkit::time_to_string(mediaplayer_active_->position()).c_str() );
    else
        ImGui::Text( ICON_FA_SNOWFLAKE " %s", GstToolkit::time_to_string(mediaplayer_active_->position()).c_str() );
    ImGui::PopFont();


    const ImVec2 scrollwindow = ImVec2(ImGui::GetContentRegionAvail().x - slider_zoom_width - 3.0,
                                 2.f * timeline_height_ + scrollbar_ );

    ///
    /// media player timelines
    ///
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
                                                      Settings::application.widget.timeline_editmode, &released, size) ) {
                    tl->update();
                }
                else if (released)
                {
                    tl->refresh();
                    if (Settings::application.widget.timeline_editmode)
                        oss << ": Timeline cut";
                    else
                        oss << ": Timeline opacity";
                    Action::manager().store(oss.str());
                }

                // custom timeline slider
                if (mediaplayer_active_->syncToMetronome() > Metronome::SYNC_NONE)
                    mediaplayer_slider_pressed_ = ImGuiToolkit::TimelineSlider("##timeline", &seek_t, tl->begin(),
                                                                               tl->first(), tl->end(), tl->step(), size.x,
                                                                               Metronome::manager().tempo(), Metronome::manager().quantum());
                else
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
        ImGuiToolkit::IconToggle(7,4,8,3, &Settings::application.widget.timeline_editmode, tooltip);

        ImGui::SetCursorScreenPos(bottom + ImVec2(1.f, 0.5f * timeline_height_));
        if (Settings::application.widget.timeline_editmode) {
            // action cut
            if (mediaplayer_active_->isPlaying()) {
                ImGuiToolkit::HelpIcon("Pause video to enable cut options", 9, 3);
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
            if (ImGui::Button(ICON_FA_PAUSE))
                mediaplayer_mode_ = false;
            ImGui::SameLine(0, h_space_);

            ImGui::PushButtonRepeat(true);
            if (ImGui::Button( mediaplayer_active_->playSpeed() < 0 ? ICON_FA_BACKWARD :ICON_FA_FORWARD))
                mediaplayer_active_->jump ();
            ImGui::PopButtonRepeat();
        }
        else {
            if (ImGui::Button(ICON_FA_PLAY))
                mediaplayer_mode_ = true;
            ImGui::SameLine(0, h_space_);

            ImGui::PushButtonRepeat(true);
            if (ImGui::Button( mediaplayer_active_->playSpeed() < 0 ? ICON_FA_STEP_BACKWARD : ICON_FA_STEP_FORWARD))
                mediaplayer_active_->step();
            ImGui::PopButtonRepeat();
        }

        // loop modes button
        ImGui::SameLine(0, h_space_);
        static int current_loop = 0;
        static std::vector< std::pair<int, int> > iconsloop = { {0,15}, {1,15}, {19,14} };
        current_loop = (int) mediaplayer_active_->loop();
        if ( ImGuiToolkit::ButtonIconMultistate(iconsloop, &current_loop, "Loop mode") )
            mediaplayer_active_->setLoop( (MediaPlayer::LoopMode) current_loop );

        // speed slider (if enough space)
        if ( rendersize.x > min_width_ * 1.4f ) {
            ImGui::SameLine(0, MAX(h_space_ * 2.f, rendersize.x - min_width_ * 1.6f) );
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - buttons_height_ );
            // speed slider
            float speed = static_cast<float>(mediaplayer_active_->playSpeed());
            if (ImGui::DragFloat( "##Speed", &speed, 0.01f, -10.f, 10.f, "Speed " UNICODE_MULTIPLY " %.1f", 2.f))
                mediaplayer_active_->setPlaySpeed( static_cast<double>(speed) );
            // store action on mouse release
            if (ImGui::IsItemDeactivatedAfterEdit()){
                oss << ": Speed x" << std::setprecision(3) << speed;
                Action::manager().store(oss.str());
            }
        }

        ImGui::SameLine();
        ImGui::SetCursorPosX(rendersize.x - buttons_height_ / 1.4f);
        if (ImGuiToolkit::ButtonIcon(12,14,"Reset speed" )) {
            mediaplayer_active_->setPlaySpeed( 1.0 );
            oss << ": Speed x1";
            Action::manager().store(oss.str());
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

        const ImGuiContext& g = *GImGui;
        const double width_ratio = static_cast<double>(scrollwindow.x - slider_zoom_width + g.Style.FramePadding.x ) / static_cast<double>(mediaplayer_active_->timeline()->sectionsDuration());
        DrawTimeline("##timeline_mediaplayers", mediaplayer_active_->timeline(), mediaplayer_active_->position(), width_ratio, 2.f * timeline_height_);

        ///
        /// Play button bar
        ///
        bottom.y += 2.f * timeline_height_ + scrollbar_;
        DrawButtonBar(bottom, rendersize.x);
    }


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
        static std::vector< std::pair<int, int> > icons_loc = { {19,7}, {18,7}, {0,8} };
        static std::vector< std::string > labels_loc = { "Fade in", "Fade out", "Auto fade in & out" };
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ImGuiToolkit::ComboIcon("Fading", icons_loc, labels_loc, &l);

        static int c = 0;
        static std::vector< std::pair<int, int> > icons_curve = { {18,3}, {19,3}, {17,3} };
        static std::vector< std::string > labels_curve = { "Linear", "Progressive", "Abrupt" };
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ImGuiToolkit::ComboIcon("Curve", icons_curve, labels_curve, &c);

        static uint d = 1000;
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ImGuiToolkit::SliderTiming ("Duration", &d, 200, 5050, 50, "Maximum");
        if (d > 5000)
            d = UINT_MAX;

        bool close = false;
        ImGui::SetCursorPos(pos + ImVec2(0.f, area.y - buttons_height_));
        if (ImGui::Button("Cancel", ImVec2(area.x * 0.3f, 0)))
            close = true;
        ImGui::SetCursorPos(pos + ImVec2(area.x * 0.7f, area.y - buttons_height_));
        if (ImGui::Button("Apply", ImVec2(area.x * 0.3f, 0))) {
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

        if (close)
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

const char *SourceController::SourcePlayIcon(Source *s)
{
    if (s->active()) {
        if ( s->playing() )
            return ICON_FA_PLAY;
        else
            return ICON_FA_PAUSE;
    }
    else
        return ICON_FA_SNOWFLAKE;
}

void SourceController::DrawButtonBar(ImVec2 bottom, float width)
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
        if ( (*source)->active() )
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
            }
        }
        else {
            if (ImGui::Button(ICON_FA_PLAY) && enabled){
                for (auto source = selection_.begin(); source != selection_.end(); ++source)
                    (*source)->play(true);
            }
        }
    }
    // separate play & pause buttons for disagreeing sources
    else {
        if (ImGui::Button(ICON_FA_PLAY) && enabled) {
            for (auto source = selection_.begin(); source != selection_.end(); ++source)
                (*source)->play(true);
        }
        ImGui::SameLine(0, h_space_);
        if (ImGui::Button(ICON_FA_PAUSE) && enabled) {
            for (auto source = selection_.begin(); source != selection_.end(); ++source)
                (*source)->play(false);
        }
    }
    ImGui::SameLine(0, h_space_);

    // restore style
    ImGui::PopStyleColor(3);
}

///
/// NAVIGATOR
///
Navigator::Navigator()
{
    // default geometry
    width_ = 100;
    pannel_width_ = 5.f * width_;
    height_ = 100;
    padding_width_ = 100;

    // clean start
    show_config_ = false;
    pannel_visible_ = false;
    view_pannel_visible = false;
    clearButtonSelection();
}

void Navigator::applyButtonSelection(int index)
{
    // ensure only one button is active at a time
    bool status = selected_button[index];
    clearButtonSelection();
    selected_button[index] = status;

    // set visible if button is active
    pannel_visible_ = status;

    show_config_ = false;
}

void Navigator::clearButtonSelection()
{
    // clear all buttons
    for(int i=0; i<NAV_COUNT; ++i)
        selected_button[i] = false;

    // clear new source pannel
    new_source_preview_.setSource();
    pattern_type = -1;
    _selectedFiles.clear();
}

void Navigator::showPannelSource(int index)
{
    // invalid index given
    if ( index < 0)
        hidePannel();
    else {
        selected_button[index] = true;
        applyButtonSelection(index);
    }
}

void Navigator::showConfig()
{
    selected_button[NAV_MENU] = true;
    applyButtonSelection(NAV_MENU);
    show_config_ = true;
}

void Navigator::togglePannelMenu()
{
    selected_button[NAV_MENU] = !selected_button[NAV_MENU];
    applyButtonSelection(NAV_MENU);
}

void Navigator::togglePannelNew()
{
    selected_button[NAV_NEW] = !selected_button[NAV_NEW];
    applyButtonSelection(NAV_NEW);
}

void Navigator::hidePannel()
{
    clearButtonSelection();
    pannel_visible_ = false;
    view_pannel_visible = false;    
    show_config_ = false;
}

void Navigator::Render()
{
    std::string tooltip_ = "";
    static uint _timeout_tooltip = 0;

    const ImGuiIO& io = ImGui::GetIO();
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
    height_ = io.DisplaySize.y;                                      // cover vertically
    float sourcelist_height_ = height_ - 8.f * ImGui::GetTextLineHeight(); // space for 4 icons of view
    float icon_width = width_ - 2.f * style.WindowPadding.x;         // icons keep padding
    ImVec2 iconsize(icon_width, icon_width);

    // Left bar top
    ImGui::SetNextWindowPos( ImVec2(0, 0), ImGuiCond_Always );
    ImGui::SetNextWindowSize( ImVec2(width_, sourcelist_height_), ImGuiCond_Always );
    ImGui::SetNextWindowBgAlpha(0.95f); // Transparent background
    if (ImGui::Begin( ICON_FA_BARS " Navigator", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |  ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing))
    {
        if (Settings::application.current_view < View::TRANSITION) {

            // the "=" icon for menu
            if (ImGui::Selectable( ICON_FA_BARS, &selected_button[NAV_MENU], 0, iconsize))
                applyButtonSelection(NAV_MENU);
            if (ImGui::IsItemHovered())
                tooltip_ = "Main menu  HOME";

            // the list of INITIALS for sources
            int index = 0;
            SourceList::iterator iter;
            for (iter = Mixer::manager().session()->begin(); iter != Mixer::manager().session()->end(); ++iter, ++index)
            {
                Source *s = (*iter);
                // draw an indicator for current source
                if ( s->mode() >= Source::SELECTED ){
                    ImDrawList* draw_list = ImGui::GetWindowDrawList();
                    ImVec2 p1 = ImGui::GetCursorScreenPos() + ImVec2(icon_width, 0.5f * icon_width);
                    ImVec2 p2 = ImVec2(p1.x + 2.f, p1.y + 2.f);
                    const ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);
                    if ((*iter)->mode() == Source::CURRENT)  {
                        p1 = ImGui::GetCursorScreenPos() + ImVec2(icon_width, 0);
                        p2 = ImVec2(p1.x + 2.f, p1.y + icon_width);
                    }
                    draw_list->AddRect(p1, p2, color, 0.0f,  0, 3.f);
                }
                // draw select box
                ImGui::PushID(std::to_string(s->group(View::RENDERING)->id()).c_str());
                if (ImGui::Selectable(s->initials(), &selected_button[index], 0, iconsize))
                {
                    applyButtonSelection(index);
                    if (selected_button[index])
                        Mixer::manager().setCurrentIndex(index);
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

                ImGui::PopID();
            }

            // the "+" icon for action of creating new source
            if (ImGui::Selectable( ICON_FA_PLUS, &selected_button[NAV_NEW], 0, iconsize))
                applyButtonSelection(NAV_NEW);
            if (ImGui::IsItemHovered())
                tooltip_ = "New Source   INS";
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
    ImGui::SetNextWindowPos( ImVec2(0, sourcelist_height_), ImGuiCond_Always );
    ImGui::SetNextWindowSize( ImVec2(width_, height_ - sourcelist_height_), ImGuiCond_Always );
    ImGui::SetNextWindowBgAlpha(0.95f); // Transparent background
    if (ImGui::Begin("##navigatorViews", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |  ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
    {
        bool selected_view[View::INVALID] = { };
        selected_view[ Settings::application.current_view ] = true;
        int previous_view = Settings::application.current_view;
        if (ImGui::Selectable( ICON_FA_BULLSEYE, &selected_view[1], 0, iconsize))
        {
            Mixer::manager().setView(View::MIXING);
            view_pannel_visible = previous_view == Settings::application.current_view;
        }
        if (ImGui::IsItemHovered())
            tooltip_ = "Mixing    F1";
        if (ImGui::Selectable( ICON_FA_OBJECT_UNGROUP , &selected_view[2], 0, iconsize))
        {
            Mixer::manager().setView(View::GEOMETRY);
            view_pannel_visible = previous_view == Settings::application.current_view;
        }
        if (ImGui::IsItemHovered())
            tooltip_ = "Geometry    F2";
        if (ImGui::Selectable( ICON_FA_LAYER_GROUP, &selected_view[3], 0, iconsize))
        {
            Mixer::manager().setView(View::LAYER);
            view_pannel_visible = previous_view == Settings::application.current_view;
        }
        if (ImGui::IsItemHovered())
            tooltip_ = "Layers    F3";
        if (ImGui::Selectable( ICON_FA_CHESS_BOARD, &selected_view[4], 0, iconsize))
        {
            Mixer::manager().setView(View::TEXTURE);
            view_pannel_visible = previous_view == Settings::application.current_view;
        }
        if (ImGui::IsItemHovered())
            tooltip_ = "Texturing    F4";

        ImGui::End();
    }

    // show tooltip
    if (!tooltip_.empty()) {
        // pseudo timeout for showing tooltip
        if (_timeout_tooltip > IMGUI_TOOLTIP_TIMEOUT)
            ImGuiToolkit::ToolTip(tooltip_.substr(0, tooltip_.size()-6).c_str(), tooltip_.substr(tooltip_.size()-6, 6).c_str());
        else
            ++_timeout_tooltip;
    }
    else
        _timeout_tooltip = 0;

    if ( view_pannel_visible && !pannel_visible_ )
        RenderViewPannel( ImVec2(width_, sourcelist_height_), ImVec2(width_*0.8f, height_ - sourcelist_height_) );

    ImGui::PopStyleVar();
    ImGui::PopFont();

    if ( pannel_visible_){
        // pannel menu
        if (selected_button[NAV_MENU])
        {
            RenderMainPannel();
        }
        // pannel to manage transition
        else if (selected_button[NAV_TRANS])
        {
            RenderTransitionPannel();
        }
        // pannel to create a source
        else if (selected_button[NAV_NEW])
        {
            RenderNewPannel();
        }
        // pannel to configure a selected source
        else
        {
            RenderSourcePannel(Mixer::manager().currentSource());
        }
        view_pannel_visible = false;
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();

}

void Navigator::RenderViewPannel(ImVec2 draw_pos , ImVec2 draw_size)
{
    ImGui::SetNextWindowPos( draw_pos, ImGuiCond_Always );
    ImGui::SetNextWindowSize( draw_size, ImGuiCond_Always );
    ImGui::SetNextWindowBgAlpha(0.95f); // Transparent background
    if (ImGui::Begin("##ViewPannel", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |  ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
    {
        ImGui::SetCursorPosX(10.f);
        ImGui::SetCursorPosY(10.f);
        if (ImGuiToolkit::IconButton(5,7)) {
            // reset zoom
            Mixer::manager().view((View::Mode)Settings::application.current_view)->recenter();
        }

        draw_size.x *= 0.5;
        ImGui::SetCursorPosX( 10.f);
        draw_size.y -= ImGui::GetCursorPosY() + 10.f;
        int percent_zoom = Mixer::manager().view((View::Mode)Settings::application.current_view)->size();
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1, 0.1, 0.1, 0.95));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.14, 0.14, 0.14, 0.95));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.14, 0.14, 0.14, 0.95));
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.9, 0.9, 0.9, 0.95));
        if (ImGui::VSliderInt("##z", draw_size, &percent_zoom, 0, 100, "") )
        {
            Mixer::manager().view((View::Mode)Settings::application.current_view)->resize(percent_zoom);
        }
        ImGui::PopStyleColor(4);
        if (ImGui::IsItemActive() || ImGui::IsItemHovered())
            ImGui::SetTooltip("Zoom %d %%", percent_zoom);
        ImGui::End();
    }

}

// Source pannel : *s was checked before
void Navigator::RenderSourcePannel(Source *s)
{
    if (s == nullptr || Settings::application.current_view >= View::TRANSITION)
        return;

    // Next window is a side pannel
    ImGui::SetNextWindowPos( ImVec2(width_, 0), ImGuiCond_Always );
    ImGui::SetNextWindowSize( ImVec2(pannel_width_, height_), ImGuiCond_Always );
    ImGui::SetNextWindowBgAlpha(0.85f); // Transparent background
    if (ImGui::Begin("##navigatorSource", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |  ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
    {
        // TITLE
        ImGui::SetCursorPosY(IMGUI_TOP_ALIGN);
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ImGui::Text("Source");
        ImGui::PopFont();

//        ImGui::SetCursorPos(ImVec2(pannel_width_  - 35.f, 15.f));
//        const char *tooltip[2] = {"Pin pannel\nCurrent: double-clic on source", "Un-pin Pannel\nCurrent: single-clic on source"};
//        ImGuiToolkit::IconToggle(5,2,4,2, &Settings::application.pannel_stick, tooltip );

        std::string sname = s->name();
        ImGui::SetCursorPosY(width_);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if (ImGuiToolkit::InputText("Name", &sname) ){
            Mixer::manager().renameSource(s, sname);
        }
        // Source pannel
        static ImGuiVisitor v;
        s->accept(v);
        // clone & delete buttons
        ImGui::Text(" ");
        // Action on source
        if ( ImGui::Button( ICON_FA_SHARE_SQUARE " Clone", ImVec2(ImGui::GetContentRegionAvail().x, 0)) )
            Mixer::manager().addSource ( Mixer::manager().createSourceClone() );
        if ( ImGui::Button( ICON_FA_BACKSPACE " Delete", ImVec2(ImGui::GetContentRegionAvail().x, 0)) ) {
            Mixer::manager().deleteSource(s);
            Action::manager().store(sname + std::string(": deleted"));
        }
        ImGui::End();
    }
}

void Navigator::RenderNewPannel()
{
    // Next window is a side pannel
    ImGui::SetNextWindowPos( ImVec2(width_, 0), ImGuiCond_Always );
    ImGui::SetNextWindowSize( ImVec2(pannel_width_, height_), ImGuiCond_Always );
    ImGui::SetNextWindowBgAlpha(0.85f); // Transparent background
    if (ImGui::Begin("##navigatorNewSource", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |  ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
    {
        // TITLE
        ImGui::SetCursorPosY(10);
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ImGui::Text("Insert");
        ImGui::PopFont();

        // Edit menu
        ImGui::SetCursorPosY(width_);
        ImGui::Text("Source");

        //
        // News Source selection pannel
        //
        static const char* origin_names[5] = { ICON_FA_PHOTO_VIDEO "  File",
                                               ICON_FA_SORT_NUMERIC_DOWN "   Sequence",
                                               ICON_FA_PLUG "    Connected",
                                               ICON_FA_COG "   Generated",
                                               ICON_FA_SYNC "   Internal"
                                             };
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if (ImGui::Combo("##Origin", &Settings::application.source.new_type, origin_names, IM_ARRAYSIZE(origin_names)) )
            new_source_preview_.setSource();

        ImGui::SetCursorPosY(2.f * width_);

        // File Source creation
        if (Settings::application.source.new_type == 0) {

            static DialogToolkit::OpenMediaDialog fileimportdialog("Open Media");

            // clic button to load file
            if ( ImGui::Button( ICON_FA_FILE_EXPORT " Open media", ImVec2(ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN, 0)) )
                fileimportdialog.open();

            // Indication
            ImGui::SameLine();
            ImGuiToolkit::HelpMarker("Create a source from a file:\n"
                                     ICON_FA_CARET_RIGHT " Video (*.mpg, *mov, *.avi, etc.)\n"
                                     ICON_FA_CARET_RIGHT " Image (*.jpg, *.png, etc.)\n"
                                     ICON_FA_CARET_RIGHT " Vector graphics (*.svg)\n"
                                     ICON_FA_CARET_RIGHT " vimix session (*.mix)\n\n"
                                     "(Equivalent to dropping the file in the workspace)");

            // get media file if dialog finished
            if (fileimportdialog.closed()){
                // get the filename from this file dialog
                std::string open_filename = fileimportdialog.path();
                // create a source with this file
                if (open_filename.empty()) {
                    new_source_preview_.setSource();
                    Log::Notify("No file selected.");
                } else {
                    std::string label = BaseToolkit::transliterate( open_filename );
                    label = BaseToolkit::trunc_string(label, 35);
                    new_source_preview_.setSource( Mixer::manager().createSourceFile(open_filename), label);
                }
            }

            // combo of recent media filenames
            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            if (ImGui::BeginCombo("##RecentImport", IMGUI_LABEL_RECENT_FILES))
            {
                std::list<std::string> recent = Settings::application.recentImport.filenames;
                for (std::list<std::string>::iterator path = recent.begin(); path != recent.end(); ++path )
                {
                    std::string recentpath(*path);
                    if ( SystemToolkit::file_exists(recentpath)) {
                        std::string label = BaseToolkit::transliterate( recentpath );
                        label = BaseToolkit::trunc_string(label, 35);
                        if (ImGui::Selectable( label.c_str() )) {
                            new_source_preview_.setSource( Mixer::manager().createSourceFile(recentpath.c_str()), label);
                        }
                    }
                }
                ImGui::EndCombo();
            }
        }
        // Folder Source creator
        else if (Settings::application.source.new_type == 1){

            bool update_new_source = false;
            static DialogToolkit::MultipleImagesDialog _selectImagesDialog("Select Images");

            // clic button to load file
            if ( ImGui::Button( ICON_FA_IMAGES " Open images", ImVec2(ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN, 0)) ) {
                _selectedFiles.clear();
                _selectImagesDialog.open();
            }

            // Indication
            ImGui::SameLine();
            ImGuiToolkit::HelpMarker("Create a source from a sequence of numbered images.");

            // return from thread for folder openning
            if (_selectImagesDialog.closed()) {
                _selectedFiles = _selectImagesDialog.images();
                if (_selectedFiles.empty())
                    Log::Notify("No file selected.");
                // ask to reload the preview
                update_new_source = true;
            }

            // multiple files selected
            if (_selectedFiles.size() > 1) {

                // set framerate
                static int _fps = 30;
                static bool _fps_changed = false;
                ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                if ( ImGui::SliderInt("Framerate", &_fps, 1, 30, "%d fps") ) {
                    _fps_changed = true;
                }
                // only call for new source after mouse release to avoid repeating call to re-open the stream
                else if (_fps_changed && ImGui::IsMouseReleased(ImGuiMouseButton_Left)){
                    update_new_source = true;
                    _fps_changed = false;
                }

                if (update_new_source) {
                    std::string label = BaseToolkit::transliterate( BaseToolkit::common_pattern(_selectedFiles) );
                    label = BaseToolkit::trunc_string(label, 35);
                    new_source_preview_.setSource( Mixer::manager().createSourceMultifile(_selectedFiles, _fps), label);
                }
            }
            // single file selected
            else if (_selectedFiles.size() > 0) {

                ImGui::Text("Single file selected");

                if (update_new_source) {
                    std::string label = BaseToolkit::transliterate( _selectedFiles.front() );
                    label = BaseToolkit::trunc_string(label, 35);
                    new_source_preview_.setSource( Mixer::manager().createSourceFile(_selectedFiles.front()), label);
                }

            }

        }
        // Internal Source creator
        else if (Settings::application.source.new_type == 4){

            // fill new_source_preview with a new source
            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            if (ImGui::BeginCombo("##Source", "Select object"))
            {
                std::string label = "Rendering output";
                if (ImGui::Selectable( label.c_str() )) {
                    new_source_preview_.setSource( Mixer::manager().createSourceRender(), label);
                }
                SourceList::iterator iter;
                for (iter = Mixer::manager().session()->begin(); iter != Mixer::manager().session()->end(); ++iter)
                {
                    label = std::string("Source ") + (*iter)->name();
                    if (ImGui::Selectable( label.c_str() )) {
                        label = std::string("Clone of ") + label;
                        new_source_preview_.setSource( Mixer::manager().createSourceClone((*iter)->name()),label);
                    }
                }
                ImGui::EndCombo();
            }

            // Indication
            ImGui::SameLine();
            ImGuiToolkit::HelpMarker("Create a source replicating internal vimix objects.\n"
                                     ICON_FA_CARET_RIGHT " Loopback from output\n"
                                     ICON_FA_CARET_RIGHT " Clone other sources");
        }
        // Generated Source creator
        else if (Settings::application.source.new_type == 3){

            bool update_new_source = false;

            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            if (ImGui::BeginCombo("##Pattern", "Select generator"))
            {
                for (int p = 0; p < (int) Pattern::count(); ++p){
                    if (Pattern::get(p).available && ImGui::Selectable( Pattern::get(p).label.c_str() )) {
                        pattern_type = p;
                        update_new_source = true;
                    }
                }
                ImGui::EndCombo();
            }

            // Indication
            ImGui::SameLine();
            ImGuiToolkit::HelpMarker("Create a source with graphics generated algorithmically.");

            // resolution (if pattern selected)
            if (pattern_type > 0) {
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
            if (update_new_source)
            {
                glm::ivec2 res = GlmToolkit::resolutionFromDescription(Settings::application.source.ratio, Settings::application.source.res);
                new_source_preview_.setSource( Mixer::manager().createSourcePattern(pattern_type, res),
                                               Pattern::get(pattern_type).label);
            }
        }
        // External source creator
        else if (Settings::application.source.new_type == 2){

            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            if (ImGui::BeginCombo("##External", "Select device"))
            {
                for (int d = 0; d < Device::manager().numDevices(); ++d){
                    std::string namedev = Device::manager().name(d);
                    if (ImGui::Selectable( namedev.c_str() )) {

                        new_source_preview_.setSource( Mixer::manager().createSourceDevice(namedev), namedev);
                    }
                }
                for (int d = 1; d < Connection::manager().numHosts(); ++d){
                    std::string namehost = Connection::manager().info(d).name;
                    if (ImGui::Selectable( namehost.c_str() )) {
                        new_source_preview_.setSource( Mixer::manager().createSourceNetwork(namehost), namehost);
                    }
                }
                ImGui::EndCombo();
            }

            // Indication
            ImGui::SameLine();
            ImGuiToolkit::HelpMarker("Create a source getting images from connected devices or machines;\n"
                                     ICON_FA_CARET_RIGHT " webcams or frame grabbers\n"
                                     ICON_FA_CARET_RIGHT " screen capture\n"
                                     ICON_FA_CARET_RIGHT " stream from connected vimix");

        }

        ImGui::NewLine();

        // if a new source was added
        if (new_source_preview_.filled()) {
            // show preview
            new_source_preview_.Render(ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN, Settings::application.source.new_type != 2);
            // ask to import the source in the mixer
            ImGui::NewLine();
            if (new_source_preview_.ready() && ImGui::Button( ICON_FA_CHECK "  Create", ImVec2(pannel_width_ - padding_width_, 0)) ) {
                Mixer::manager().addSource(new_source_preview_.getSource());
                selected_button[NAV_NEW] = false;
            }
        }

        ImGui::End();
    }
}

void Navigator::RenderMainPannelVimix()
{
    // TITLE
    ImGui::SetCursorPosY(IMGUI_TOP_ALIGN);
    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
    ImGui::Text(APP_NAME);
    ImGui::PopFont();

    // MENU
    ImGui::SameLine();
    ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, IMGUI_TOP_ALIGN) );
    if (ImGui::BeginMenu("File"))
    {
        UserInterface::manager().showMenuFile();
        ImGui::EndMenu();
    }
    ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, IMGUI_TOP_ALIGN + ImGui::GetTextLineHeightWithSpacing()) );
    if (ImGui::BeginMenu("Edit"))
    {
        UserInterface::manager().showMenuEdit();
        ImGui::EndMenu();
    }

    ImGui::SetCursorPosY(width_);

    //
    // SESSION panel
    //
    ImGui::Text("Sessions");
    static bool selection_session_mode_changed = true;
    static int selection_session_mode = 0;
    static DialogToolkit::OpenFolderDialog customFolder("Open Folder");

    // Show combo box of quick selection modes
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    if (ImGui::BeginCombo("##SelectionSession", BaseToolkit::trunc_string(Settings::application.recentFolders.path, 25).c_str() )) {

        // Option 0 : recent files
        if (ImGui::Selectable( ICON_FA_CLOCK IMGUI_LABEL_RECENT_FILES) ) {
             Settings::application.recentFolders.path = IMGUI_LABEL_RECENT_FILES;
             selection_session_mode = 0;
             selection_session_mode_changed = true;
        }
        // Options 1 : known folders
        for(auto foldername = Settings::application.recentFolders.filenames.begin();
            foldername != Settings::application.recentFolders.filenames.end(); foldername++) {
            std::string f = std::string(ICON_FA_FOLDER) + " " + BaseToolkit::trunc_string( *foldername, 40);
            if (ImGui::Selectable( f.c_str() )) {
                // remember which path was selected
                Settings::application.recentFolders.path.assign(*foldername);
                // set mode
                selection_session_mode = 1;
                selection_session_mode_changed = true;
            }
        }
        // Option 2 : add a folder
        if (ImGui::Selectable( ICON_FA_FOLDER_PLUS " Add Folder") )
            customFolder.open();
        ImGui::EndCombo();
    }

    // return from thread for folder openning
    if (customFolder.closed() && !customFolder.path().empty()) {
        Settings::application.recentFolders.push(customFolder.path());
        Settings::application.recentFolders.path.assign(customFolder.path());
        selection_session_mode = 1;
        selection_session_mode_changed = true;
    }

    // icon to clear list
    ImVec2 pos_top = ImGui::GetCursorPos();
    ImGui::SameLine();
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.7);
    if ( selection_session_mode == 1) {
        if (ImGuiToolkit::IconButton( ICON_FA_FOLDER_MINUS, "Discard folder")) {
            Settings::application.recentFolders.filenames.remove(Settings::application.recentFolders.path);
            if (Settings::application.recentFolders.filenames.empty()) {
                Settings::application.recentFolders.path.assign(IMGUI_LABEL_RECENT_FILES);
                selection_session_mode = 0;
            }
            else
                Settings::application.recentFolders.path = Settings::application.recentFolders.filenames.front();
            // reload the list next time
            selection_session_mode_changed = true;
        }
    }
    else {
        if (ImGuiToolkit::IconButton( ICON_FA_BACKSPACE, "Clear history")) {
            Settings::application.recentSessions.filenames.clear();
            Settings::application.recentSessions.front_is_valid = false;
            // reload the list next time
            selection_session_mode_changed = true;
        }
    }
    ImGui::PopStyleVar();
    ImGui::SetCursorPos(pos_top);

    // fill the session list depending on the mode
    static std::list<std::string> sessions_list;
    // change session list if changed
    if (selection_session_mode_changed || Settings::application.recentSessions.changed) {

        // selection MODE 0 ; RECENT sessions
        if ( selection_session_mode == 0) {
            // show list of recent sessions
            Settings::application.recentSessions.validate();
            sessions_list = Settings::application.recentSessions.filenames;
            Settings::application.recentSessions.changed = false;
        }
        // selection MODE 1 : LIST FOLDER
        else if ( selection_session_mode == 1) {
            // show list of vimix files in folder
            sessions_list = SystemToolkit::list_directory( Settings::application.recentFolders.path, {"mix", "MIX"});
        }
        // indicate the list changed (do not change at every frame)
        selection_session_mode_changed = false;
    }

    {
        static std::list<std::string>::iterator _file_over = sessions_list.end();
        static std::list<std::string>::iterator _displayed_over = sessions_list.end();
        static bool _tooltip = 0;

        // display the sessions list and detect if one was selected (double clic)
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if (ImGui::ListBoxHeader("##Sessions", sessions_list.size(), CLAMP(sessions_list.size(), 4, 8)) ) {

            bool done = false;
            int count_over = 0;
            ImVec2 size = ImVec2( ImGui::GetContentRegionAvailWidth(), ImGui::GetTextLineHeight() );

            for(auto it = sessions_list.begin(); it != sessions_list.end(); ++it) {

                if (it->empty())
                    continue;

                std::string shortname = SystemToolkit::filename(*it);
                if (ImGui::Selectable( shortname.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick, size )) {
                    // open on double clic
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) /*|| file_selected == it*/) {
                        Mixer::manager().open( *it, Settings::application.smooth_transition );
                        done = true;
                    }
                    else
                        // show tooltip on clic
                        _tooltip = true;

                }
                if (ImGui::IsItemHovered())
                    _file_over = it;

                if (_tooltip && _file_over != sessions_list.end() && count_over < 1) {

                    static std::string _file_info = "";
                    static Thumbnail _file_thumbnail;
                    static bool with_tag_ = false;

                    // load info only if changed from the one already displayed
                    if (_displayed_over != _file_over) {
                        _displayed_over = _file_over;
                        SessionInformation info = SessionCreator::info(*_displayed_over);
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

                        ImGui::BeginTooltip();
                        ImVec2 p_ = ImGui::GetCursorScreenPos();
                        _file_thumbnail.Render(size.x);
                        ImGui::Text("%s", _file_info.c_str());
                        if (with_tag_) {
                            ImGui::SetCursorScreenPos(p_ + ImVec2(6, 6));
                            ImGui::Text(ICON_FA_TAG);
                        }
                        ImGui::EndTooltip();
                    }
                    else
                        selection_session_mode_changed = true;

                    ++count_over; // prevents display twice on item overlap
                }
            }
            ImGui::ListBoxFooter();

            // done the selection !
            if (done) {
                hidePannel();
                _tooltip = false;
                _displayed_over = _file_over = sessions_list.end();
                // reload the list next time
                selection_session_mode_changed = true;
            }
        }
        // cancel tooltip and mouse over on mouse exit
        if ( !ImGui::IsItemHovered()) {
            _tooltip = false;
            _displayed_over = _file_over = sessions_list.end();
        }
    }

    ImVec2 pos_bot = ImGui::GetCursorPos();

    // Right side of the list: helper and options
    ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_top.y));
    if ( ImGuiToolkit::IconButton( ICON_FA_FILE " +" )) {
        Mixer::manager().close(Settings::application.smooth_transition );
        hidePannel();
    }
    if (ImGui::IsItemHovered())
        ImGuiToolkit::ToolTip("New session", CTRL_MOD "W");

    ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_bot.y - 2.f * ImGui::GetFrameHeightWithSpacing()));
    ImGuiToolkit::HelpMarker("Select the history of recently opened files or a folder. "
                             "Double-clic on a filename to open it.\n\n"
                             ICON_FA_ARROW_CIRCLE_RIGHT "  Smooth transition "
                             "performs cross fading to the openned session.");
    // toggle button for smooth transition
    ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_bot.y - ImGui::GetFrameHeightWithSpacing()) );
    ImGuiToolkit::ButtonToggle(ICON_FA_ARROW_CIRCLE_RIGHT, &Settings::application.smooth_transition);
    if (ImGui::IsItemHovered())
        ImGuiToolkit::ToolTip("Smooth transition");
    // come back...
    ImGui::SetCursorPos(pos_bot);

    //
    // Status
    //
    ImGuiToolkit::Spacing();
    ImGui::Text("Current session");
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::Combo("##Selectpanelsession", &Settings::application.pannel_current_session_mode,
                 ICON_FA_CODE_BRANCH "  Versions\0" ICON_FA_HISTORY " Undo history\0" ICON_FA_FILE_ALT "  Properties\0");
    pos_bot = ImGui::GetCursorPos();

    //
    // Current 2. PROPERTIES
    //
    if (Settings::application.pannel_current_session_mode > 1) {

        std::string sessionfilename = Mixer::manager().session()->filename();

        // Information and resolution
        const FrameBuffer *output = Mixer::manager().session()->frame();
        if (output)
        {
            // Show info text bloc (dark background)
            ImGuiTextBuffer info;
            if (sessionfilename.empty())
                info.appendf("<unsaved>");
            else
                info.appendf("%s", SystemToolkit::filename(sessionfilename).c_str());
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.14f, 0.9f));
            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            ImGui::InputText("##Info", (char *)info.c_str(), info.size(), ImGuiInputTextFlags_ReadOnly);
            ImGui::PopStyleColor(1);

            // Kept for later? Larger info box with more details on the session file...
            //            ImGuiTextBuffer info;
            //            if (!sessionfilename.empty())
            //                info.appendf("%s\n", SystemToolkit::filename(sessionfilename).c_str());
            //            else
            //                info.append("<unsaved>\n");
            //            info.appendf("%dx%dpx", output->width(), output->height());
            //            if (p.x > -1)
            //                info.appendf(", %s", FrameBuffer::aspect_ratio_name[p.x]);
            //            // content info
            //            const uint N = Mixer::manager().session()->numSource();
            //            if (N > 1)
            //                info.appendf("\n%d sources", N);
            //            else if (N > 0)
            //                info.append("\n1 source");
            //            const size_t M = MediaPlayer::registered().size();
            //            if (M > 0) {
            //                info.appendf("\n%d media files:", M);
            //                for (auto mit = MediaPlayer::begin(); mit != MediaPlayer::end(); ++mit)
            //                    info.appendf("\n- %s", SystemToolkit::filename((*mit)->filename()).c_str());
            //            }
            //            // Show info text bloc (multi line, dark background)
            //            ImGuiToolkit::PushFont( ImGuiToolkit::FONT_MONO );
            //            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.14f, 0.9f));
            //            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            //            ImGui::InputTextMultiline("##Info", (char *)info.c_str(), info.size(), ImVec2(IMGUI_RIGHT_ALIGN, 2*ImGui::GetTextLineHeightWithSpacing()), ImGuiInputTextFlags_ReadOnly);
            //            ImGui::PopStyleColor(1);
            //            ImGui::PopFont();

            // change resolution (height only)
            // get parameters to edit resolution
            glm::ivec2 p = FrameBuffer::getParametersFromResolution(output->resolution());
            if (p.y > -1) {
                // cannot change resolution when recording
                if ( UserInterface::manager().isRecording() ) {
                    // show static info (same size than combo)
                    static char dummy_str[512];
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.14f, 0.9f));
                    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                    sprintf(dummy_str, "%s", FrameBuffer::aspect_ratio_name[p.x]);
                    ImGui::InputText("Ratio", dummy_str, IM_ARRAYSIZE(dummy_str), ImGuiInputTextFlags_ReadOnly);
                    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                    sprintf(dummy_str, "%s", FrameBuffer::resolution_name[p.y]);
                    ImGui::InputText("Height", dummy_str, IM_ARRAYSIZE(dummy_str), ImGuiInputTextFlags_ReadOnly);
                    ImGui::PopStyleColor(1);
                }
                // offer to change ratio and resolution
                else {
                    // combo boxes to select Rario and Height
                    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                    if (ImGui::Combo("Ratio", &p.x, FrameBuffer::aspect_ratio_name, IM_ARRAYSIZE(FrameBuffer::aspect_ratio_name) ) )
                    {
                        glm::vec3 res = FrameBuffer::getResolutionFromParameters(p.x, p.y);
                        Mixer::manager().setResolution(res);
                    }
                    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                    if (ImGui::Combo("Height", &p.y, FrameBuffer::resolution_name, IM_ARRAYSIZE(FrameBuffer::resolution_name) ) )
                    {
                        glm::vec3 res = FrameBuffer::getResolutionFromParameters(p.x, p.y);
                        Mixer::manager().setResolution(res);
                    }
                }
            }
        }

        // the session file exists
        if (!sessionfilename.empty())
        {
            // Folder
            std::string path = SystemToolkit::path_filename(sessionfilename);
            std::string label = BaseToolkit::trunc_string(path, 23);
            label = BaseToolkit::transliterate(label);
            ImGuiToolkit::ButtonOpenUrl( label.c_str(), path.c_str(), ImVec2(IMGUI_RIGHT_ALIGN, 0) );
            ImGui::SameLine();
            ImGui::Text("Folder");

            // Thumbnail
            static Thumbnail _file_thumbnail;
            static FrameBufferImage *thumbnail = nullptr;
            if ( ImGui::Button( ICON_FA_TAG "  New thumbnail", ImVec2(IMGUI_RIGHT_ALIGN, 0)) ) {
                Mixer::manager().session()->setThumbnail();
                thumbnail = nullptr;
            }
            pos_bot = ImGui::GetCursorPos();
            if (ImGui::IsItemHovered()){
                // thumbnail changed
                if (thumbnail != Mixer::manager().session()->thumbnail()) {
                    _file_thumbnail.reset();
                    thumbnail = Mixer::manager().session()->thumbnail();
                    if (thumbnail != nullptr)
                        _file_thumbnail.fill( thumbnail );
                }
                if (_file_thumbnail.filled()) {
                    ImGui::BeginTooltip();
                    _file_thumbnail.Render(230);
                    ImGui::Text("Thumbnail used in the\nlist of Sessions above.");
                    ImGui::EndTooltip();
                }
            }
            if (Mixer::manager().session()->thumbnail()) {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.7);
                ImGui::SameLine();
                if (ImGuiToolkit::IconButton(ICON_FA_BACKSPACE, "Remove thumbnail")) {
                    Mixer::manager().session()->resetThumbnail();
                    _file_thumbnail.reset();
                    thumbnail = nullptr;
                }
                ImGui::PopStyleVar();
            }
            ImGui::SetCursorPos( pos_bot );
        }

    }
    //
    // Current 1. UNDO History
    //
    else if (Settings::application.pannel_current_session_mode > 0) {

        static uint _over = 0;
        static uint64_t _displayed_over = 0;
        static bool _tooltip = 0;

        ImGui::SameLine();
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.7);
        if (ImGuiToolkit::IconButton( ICON_FA_BACKSPACE, "Clear undo")) {
            Action::manager().init();
        }
        ImGui::PopStyleVar();
        // come back...
        ImGui::SetCursorPos(pos_bot);

        pos_top = ImGui::GetCursorPos();
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if ( ImGui::ListBoxHeader("##UndoHistory", Action::manager().max(), CLAMP(Action::manager().max(), 4, 8)) ) {

            int count_over = 0;
            ImVec2 size = ImVec2( ImGui::GetContentRegionAvailWidth(), ImGui::GetTextLineHeight() );

            for (uint i = Action::manager().max(); i > 0; --i) {

                if (ImGui::Selectable( Action::manager().label(i).c_str(), i == Action::manager().current(), ImGuiSelectableFlags_AllowDoubleClick, size )) {
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
                            text = text.insert( text.find_first_of(':') + 1, 1, '\n');
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
                    ImGui::BeginTooltip();
                    _undo_thumbnail.Render(size.x);
                    ImGui::Text("%s", text.c_str());
                    ImGui::EndTooltip();
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

        pos_bot = ImGui::GetCursorPos();

        // right buttons
        ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_top.y ));
        if ( Action::manager().current() > 1 ) {
            if ( ImGuiToolkit::IconButton( ICON_FA_UNDO ) )
                Action::manager().undo();
        } else
            ImGui::TextDisabled( ICON_FA_UNDO );

        ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_top.y + ImGui::GetTextLineHeightWithSpacing() + 4));
        if ( Action::manager().current() < Action::manager().max() ) {
            if ( ImGuiToolkit::IconButton( ICON_FA_REDO ))
                Action::manager().redo();
        } else
            ImGui::TextDisabled( ICON_FA_REDO );

        ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_bot.y - ImGui::GetFrameHeightWithSpacing()) );
        ImGuiToolkit::ButtonToggle(ICON_FA_MAP_MARKED_ALT, &Settings::application.action_history_follow_view);
        if (ImGui::IsItemHovered())
            ImGuiToolkit::ToolTip("Show in view");
    }
    //
    // Current 0. VERSIONS
    //
    else {
        static uint64_t _over = 0;
        static bool _tooltip = 0;

        ImGui::SameLine();
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.7);
        if (ImGuiToolkit::IconButton( ICON_FA_BACKSPACE, "Clear versions")) {
            Action::manager().clearSnapshots();
        }
        ImGui::PopStyleVar();
        // come back...
        ImGui::SetCursorPos(pos_bot);

        // list snapshots
        std::list<uint64_t> snapshots = Action::manager().snapshots();
        pos_top = ImGui::GetCursorPos();
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
                    ImGui::BeginTooltip();
                    _snap_thumbnail.Render(size.x);
                    ImGui::Text(_snap_date.c_str());
                    ImGui::EndTooltip();
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

        // right buttons
        ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_top.y ));
        if ( ImGuiToolkit::IconButton( ICON_FA_FILE_DOWNLOAD " +")) {
            UserInterface::manager().saveOrSaveAs(true);
        }
        if (ImGui::IsItemHovered())
            ImGuiToolkit::ToolTip("Save & Keep version");

        ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_bot.y - 2.f * ImGui::GetFrameHeightWithSpacing()));
        ImGuiToolkit::HelpMarker("Previous versions of the session (latest on top). "
                                 "Double-clic on a version to restore it.\n\n"
                                 ICON_FA_CODE_BRANCH "  Iterative saving automatically "
                                 "keeps a version each time a session is saved.");
        // toggle button for versioning
        ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_bot.y - ImGui::GetFrameHeightWithSpacing()) );
        ImGuiToolkit::ButtonToggle(" " ICON_FA_CODE_BRANCH " ", &Settings::application.save_version_snapshot);
        if (ImGui::IsItemHovered())
            ImGuiToolkit::ToolTip("Iterative saving");

        ImGui::SetCursorPos( pos_bot );
    }

    //
    // Buttons to show WINDOWS
    //
    ImGuiToolkit::Spacing();
    ImGui::Text("Windows");
    ImGui::Spacing();

    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
    std::string tooltip_ = "";

    ImGui::SameLine(0, 0.5f * ImGui::GetTextLineHeight());
    if ( ImGuiToolkit::IconButton( Rendering::manager().mainWindow().isFullscreen() ? ICON_FA_COMPRESS_ALT : ICON_FA_EXPAND_ALT ) )
        Rendering::manager().mainWindow().toggleFullscreen();
    if (ImGui::IsItemHovered())
        tooltip_ = "Fullscreen " CTRL_MOD "Shift+F";

    ImGui::SameLine(0, ImGui::GetTextLineHeight());
    if ( ImGuiToolkit::IconButton( ICON_FA_STICKY_NOTE ) )
        Mixer::manager().session()->addNote();
    if (ImGui::IsItemHovered())
        tooltip_ = "New note " CTRL_MOD "Shift+N";

    ImGui::SameLine(0, ImGui::GetTextLineHeight());
    if ( ImGuiToolkit::IconButton( ICON_FA_PLAY_CIRCLE ) ) {
        if (Settings::application.widget.media_player && Settings::application.widget.media_player_view > -1 && Settings::application.widget.media_player_view != Settings::application.current_view)
            Settings::application.widget.media_player_view = Settings::application.current_view;
        else
            Settings::application.widget.media_player = !Settings::application.widget.media_player;
    }
    if (ImGui::IsItemHovered())
        tooltip_ = "Player       " CTRL_MOD "P";

    ImGui::SameLine(0, ImGui::GetTextLineHeight());
    if ( ImGuiToolkit::IconButton( ICON_FA_DESKTOP ) ) {
        if (Settings::application.widget.preview && Settings::application.widget.preview_view > -1 && Settings::application.widget.preview_view != Settings::application.current_view)
            Settings::application.widget.preview_view = Settings::application.current_view;
        else
            Settings::application.widget.preview = !Settings::application.widget.preview;
    }
    if (ImGui::IsItemHovered())
        tooltip_ = "Output       " CTRL_MOD "D";

    ImGui::SameLine(0, ImGui::GetTextLineHeight());
    if ( ImGuiToolkit::IconButton( ICON_FA_CLOCK ) ) {
        if (Settings::application.widget.timer && Settings::application.widget.timer_view > -1 && Settings::application.widget.timer_view != Settings::application.current_view)
            Settings::application.widget.timer_view = Settings::application.current_view;
        else
            Settings::application.widget.timer = !Settings::application.widget.timer;
    }
    if (ImGui::IsItemHovered())
        tooltip_ = "Timer        " CTRL_MOD "T";

    ImGui::PopFont();
    if (!tooltip_.empty()) {
        ImGuiToolkit::ToolTip(tooltip_.substr(0, tooltip_.size()-12).c_str(), tooltip_.substr(tooltip_.size()-12, 12).c_str());
    }

}

void Navigator::RenderMainPannelSettings()
{
        // TITLE
        ImGui::SetCursorPosY(IMGUI_TOP_ALIGN);
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ImGui::Text("Settings");
        ImGui::PopFont();
        ImGui::SetCursorPosY(width_);

        //
        // Appearance
        //
        ImGui::Text("Appearance");
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if ( ImGui::DragFloat("Scale", &Settings::application.scale, 0.01, 0.5f, 2.0f, "%.1f"))
            ImGui::GetIO().FontGlobalScale = Settings::application.scale;
        bool b = ImGui::RadioButton("Blue", &Settings::application.accent_color, 0); ImGui::SameLine();
        bool o = ImGui::RadioButton("Orange", &Settings::application.accent_color, 1); ImGui::SameLine();
        bool g = ImGui::RadioButton("Grey", &Settings::application.accent_color, 2);
        if (b || o || g)
            ImGuiToolkit::SetAccentColor(static_cast<ImGuiToolkit::accent_color>(Settings::application.accent_color));

        //
        // Options
        //
        ImGuiToolkit::Spacing();
        ImGui::Text("Options");
        ImGuiToolkit::ButtonSwitch( ICON_FA_MOUSE_POINTER "  Smooth cursor", &Settings::application.smooth_cursor);
        ImGuiToolkit::ButtonSwitch( ICON_FA_TACHOMETER_ALT " Metrics", &Settings::application.widget.stats);

#ifndef NDEBUG
        ImGui::Text("Expert");
        ImGuiToolkit::ButtonSwitch( IMGUI_TITLE_SHADEREDITOR, &Settings::application.widget.shader_editor, CTRL_MOD  "E");
        ImGuiToolkit::ButtonSwitch( IMGUI_TITLE_TOOLBOX, &Settings::application.widget.toolbox, CTRL_MOD  "G");
        ImGuiToolkit::ButtonSwitch( IMGUI_TITLE_LOGS, &Settings::application.widget.logs, CTRL_MOD "L");
#endif
        //
        // Recording preferences
        //
        ImGuiToolkit::Spacing();
        ImGui::Text("Recording");

        // select CODEC and FPS
        ImGui::SetCursorPosX(-1.f * IMGUI_RIGHT_ALIGN);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ImGui::Combo("Codec", &Settings::application.record.profile, VideoRecorder::profile_name, IM_ARRAYSIZE(VideoRecorder::profile_name) );

        ImGui::SetCursorPosX(-1.f * IMGUI_RIGHT_ALIGN);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ImGui::Combo("Framerate", &Settings::application.record.framerate_mode, VideoRecorder::framerate_preset_name, IM_ARRAYSIZE(VideoRecorder::framerate_preset_name) );

        // compute number of frames in buffer and show warning sign if too low
        const FrameBuffer *output = Mixer::manager().session()->frame();
        if (output) {
            guint64 nb = 0;
            nb = VideoRecorder::buffering_preset_value[Settings::application.record.buffering_mode] / (output->width() * output->height() * 4);
            char buf[256]; sprintf(buf, "Buffer can contain %ld frames (%dx%d), %.1f sec", nb, output->width(), output->height(),
                                   (float)nb / (float) VideoRecorder::framerate_preset_value[Settings::application.record.framerate_mode] );
            ImGuiToolkit::HelpMarker(buf, ICON_FA_INFO_CIRCLE);
            ImGui::SameLine(0);
        }

        ImGui::SetCursorPosX(-1.f * IMGUI_RIGHT_ALIGN);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ImGui::SliderInt("Buffer", &Settings::application.record.buffering_mode, 0, IM_ARRAYSIZE(VideoRecorder::buffering_preset_name)-1,
                         VideoRecorder::buffering_preset_name[Settings::application.record.buffering_mode]);

        ImGuiToolkit::HelpMarker("Priority when buffer is full and recorder skips frames;\n"
                                 ICON_FA_CARET_RIGHT " Clock: variable framerate, correct duration.\n"
                                 ICON_FA_CARET_RIGHT " Framerate: correct framerate,  shorter duration.");
        ImGui::SameLine(0);
        ImGui::SetCursorPosX(-1.f * IMGUI_RIGHT_ALIGN);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ImGui::Combo("Priority", &Settings::application.record.priority_mode, "Clock\0Framerate\0");

        //
        // System preferences
        //
        ImGuiToolkit::Spacing();
        ImGui::Text("System");
        ImGui::SameLine( ImGui::GetContentRegionAvailWidth() IMGUI_RIGHT_ALIGN * 0.8);
        ImGuiToolkit::HelpMarker("If you encounter some rendering issues on your machine, "
                                 "you can try to disable some of the OpenGL optimizations below.");

        static bool need_restart = false;
        static bool vsync = (Settings::application.render.vsync > 0);
        static bool blit = Settings::application.render.blit;
        static bool multi = (Settings::application.render.multisampling > 0);
        static bool gpu = Settings::application.render.gpu_decoding;
        bool change = false;
        change |= ImGuiToolkit::ButtonSwitch( "Vertical synchronization", &vsync);
        change |= ImGuiToolkit::ButtonSwitch( "Blit framebuffer", &blit);
        change |= ImGuiToolkit::ButtonSwitch( "Antialiasing framebuffer", &multi);
        // hardware support deserves more explanation
        ImGuiToolkit::HelpMarker("If enabled, tries to find a platform adapted hardware accelerated "
                                 "driver to decode (read) or encode (record) videos.", ICON_FA_MICROCHIP);
        ImGui::SameLine(0);
        change |= ImGuiToolkit::ButtonSwitch( "Hardware video de/encoding", &gpu);

        if (change) {
            need_restart = ( vsync != (Settings::application.render.vsync > 0) ||
                 blit != Settings::application.render.blit ||
                 multi != (Settings::application.render.multisampling > 0) ||
                 gpu != Settings::application.render.gpu_decoding );
        }
        if (need_restart) {
            ImGuiToolkit::Spacing();
            if (ImGui::Button( ICON_FA_POWER_OFF "  Restart to apply", ImVec2(ImGui::GetContentRegionAvail().x - 50, 0))) {
                Settings::application.render.vsync = vsync ? 1 : 0;
                Settings::application.render.blit = blit;
                Settings::application.render.multisampling = multi ? 3 : 0;
                Settings::application.render.gpu_decoding = gpu;
                Rendering::manager().close();
            }
        }

}

void Navigator::RenderTransitionPannel()
{
    if (Settings::application.current_view < View::TRANSITION) {
        hidePannel();
        return;
    }

    // Next window is a side pannel
    ImGui::SetNextWindowPos( ImVec2(width_, 0), ImGuiCond_Always );
    ImGui::SetNextWindowSize( ImVec2(pannel_width_, height_), ImGuiCond_Always );
    ImGui::SetNextWindowBgAlpha(0.85f); // Transparent background
    if (ImGui::Begin("##navigatorTrans", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |  ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
    {
        // TITLE
        ImGui::SetCursorPosY(IMGUI_TOP_ALIGN);
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ImGui::Text("Transition");
        ImGui::PopFont();

        // Session menu
        ImGui::SetCursorPosY(width_);
        ImGui::Text("Behavior");
        ImGuiToolkit::ButtonSwitch( ICON_FA_RANDOM " Cross fading", &Settings::application.transition.cross_fade);
        ImGuiToolkit::ButtonSwitch( ICON_FA_CLOUD_SUN " Clear view", &Settings::application.transition.hide_windows);

        // Transition options
        ImGuiToolkit::Spacing();
        ImGui::Text("Animation");
        if (ImGuiToolkit::ButtonIcon(4, 13)) Settings::application.transition.duration = 1.f;
        ImGui::SameLine(0, 10);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ImGui::SliderFloat("Duration", &Settings::application.transition.duration, TRANSITION_MIN_DURATION, TRANSITION_MAX_DURATION, "%.1f s");
        if (ImGuiToolkit::ButtonIcon(9, 1)) Settings::application.transition.profile = 0;
        ImGui::SameLine(0, 10);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ImGui::Combo("Curve", &Settings::application.transition.profile, "Linear\0Quadratic\0");

        // specific transition actions
        ImGui::Text(" ");
        if ( ImGui::Button( ICON_FA_PLAY "  Play ", ImVec2(IMGUI_RIGHT_ALIGN, 0)) ){
            TransitionView *tv = static_cast<TransitionView *>(Mixer::manager().view(View::TRANSITION));
            if (tv) tv->play(false);
        }
        ImGui::SameLine();
        ImGui::Text("Animation");
        if ( ImGui::Button( ICON_FA_FILE_UPLOAD "  Open ", ImVec2(IMGUI_RIGHT_ALIGN, 0)) ){
            TransitionView *tv = static_cast<TransitionView *>(Mixer::manager().view(View::TRANSITION));
            if (tv) tv->open();
        }
        ImGui::SameLine();
        ImGui::Text("Session");

        // General transition actions
        ImGui::Text(" ");
        if ( ImGui::Button( ICON_FA_PLAY "  Play &  " ICON_FA_FILE_UPLOAD " Open ", ImVec2(ImGui::GetContentRegionAvail().x, 0)) ){
            TransitionView *tv = static_cast<TransitionView *>(Mixer::manager().view(View::TRANSITION));
            if (tv) tv->play(true);
        }
        if ( ImGui::Button( ICON_FA_DOOR_OPEN " Exit", ImVec2(ImGui::GetContentRegionAvail().x, 0)) )
            Mixer::manager().setView(View::MIXING);

        ImGui::End();
    }
}

void Navigator::RenderMainPannel()
{
    // Next window is a side pannel
    ImGui::SetNextWindowPos( ImVec2(width_, 0), ImGuiCond_Always );
    ImGui::SetNextWindowSize( ImVec2(pannel_width_, height_), ImGuiCond_Always );
    ImGui::SetNextWindowBgAlpha(0.85f); // Transparent background
    if (ImGui::Begin("##navigatorMain", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |  ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
    {
        //
        // Panel content depends on show_config_
        //
        if (show_config_)
            RenderMainPannelSettings();
        else
            RenderMainPannelVimix();

        // Bottom aligned Logo (if enougth room)
        static unsigned int vimixicon = Resource::getTextureImage("images/vimix_256x256.png");
        static float height_about = 1.6f * ImGui::GetTextLineHeightWithSpacing();
        bool show_icon = ImGui::GetCursorPosY() + height_about + 128.f < height_ ;
        if ( show_icon ) {
            ImGui::SetCursorPos(ImVec2((pannel_width_ -1.5f * ImGui::GetTextLineHeightWithSpacing()) / 2.f - 64.f, height_ -height_about - 128.f));
            ImGui::Image((void*)(intptr_t)vimixicon, ImVec2(128, 128));
        }
        else {
            ImGui::SetCursorPosY(height_ -height_about);
        }

        // About & System config toggle
        if ( ImGui::Button( ICON_FA_CROW " About vimix", ImVec2(pannel_width_ IMGUI_RIGHT_ALIGN, 0)) )
            UserInterface::manager().show_vimix_about = true;
        ImGui::SameLine(0,  ImGui::GetTextLineHeightWithSpacing());
        ImGuiToolkit::IconToggle(13,5,12,5,&show_config_);


        ImGui::End();
    }
}

///
/// SOURCE PREVIEW
///

SourcePreview::SourcePreview() : source_(nullptr), label_(""), reset_(0)
{

}

void SourcePreview::setSource(Source *s, const string &label)
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

void SourcePreview::Render(float width, bool controlbutton)
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

            if (controlbutton && source_->ready()) {
                ImVec2 pos = ImGui::GetCursorPos();
                ImGui::SameLine();
                bool active = source_->active();
                if (ImGuiToolkit::IconToggle(12,7,1,8, &active))
                    source_->setActive(active);
                ImGui::SetCursorPos(pos);
            }
            ImGuiToolkit::Icon(source_->icon().x, source_->icon().y);
            ImGui::SameLine(0, 10);
            ImGui::Text("%s", label_.c_str());
            if (source_->ready())
                ImGui::Text("%d x %d %s", frame->width(), frame->height(), frame->use_alpha() ? "RGBA" : "RGB");
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
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB8, SESSION_THUMBNAIL_HEIGHT * 2, SESSION_THUMBNAIL_HEIGHT);
    }

    aspect_ratio_ = static_cast<float>(image->width) / static_cast<float>(image->height);
    glBindTexture( GL_TEXTURE_2D, texture_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, image->width, image->height, GL_RGB, GL_UNSIGNED_BYTE, image->rgb);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void Thumbnail::Render(float width)
{
    if (filled())
        ImGui::Image((void*)(intptr_t)texture_, ImVec2(width, width/aspect_ratio_), ImVec2(0,0), ImVec2(0.5f*aspect_ratio_, 1.f));
}

///
/// UTILITY
///

#define SEGMENT_ARRAY_MAX 1000
#define MAXSIZE 65535


void ShowSandbox(bool* p_open)
{
    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 260), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin( ICON_FA_BABY_CARRIAGE "  Sandbox", p_open))
    {
        ImGui::End();
        return;
    }

    ImGui::Text("Testing sandox");
    ImGui::Separator();

    ImGui::Text("IMAGE of Font");

    ImGuiToolkit::ImageGlyph(ImGuiToolkit::FONT_DEFAULT, 'v');
    ImGui::SameLine();
    ImGuiToolkit::ImageGlyph(ImGuiToolkit::FONT_BOLD, 'i');
    ImGui::SameLine();
    ImGuiToolkit::ImageGlyph(ImGuiToolkit::FONT_ITALIC, 'm');
    ImGui::SameLine();
    ImGuiToolkit::ImageGlyph(ImGuiToolkit::FONT_MONO, 'i');
    ImGui::SameLine();
    ImGuiToolkit::ImageGlyph(ImGuiToolkit::FONT_LARGE, 'x');

    ImGui::Separator();
    ImGui::Text("Source list");
    Session *se = Mixer::manager().session();
    for (auto sit = se->begin(); sit != se->end(); ++sit) {

        ImGui::Text("[%s] %s ", std::to_string((*sit)->id()).c_str(), (*sit)->name().c_str());
    }

    ImGui::Separator();

    static char buf1[1280] = "videotestsrc pattern=smpte";
    ImGui::InputText("gstreamer pipeline", buf1, 1280);
    if (ImGui::Button("Create Generic Stream Source") )
    {
        Mixer::manager().addSource(Mixer::manager().createSourceStream(buf1));
    }

    static char str[128] = "";
    ImGui::InputText("Command", str, IM_ARRAYSIZE(str));
    if ( ImGui::Button("Execute") )
        SystemToolkit::execute(str);

    static char str0[128] = "àöäüèáû вторая строчка";
    ImGui::InputText("##inputtext", str0, IM_ARRAYSIZE(str0));
    std::string tra = BaseToolkit::transliterate(std::string(str0));
    ImGui::Text("Transliteration: '%s'", tra.c_str());

    ImGui::End();
}

void ShowAboutOpengl(bool* p_open)
{
    ImGui::SetNextWindowPos(ImVec2(430, 640), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("About OpenGL", p_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::End();
        return;
    }

    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_BOLD);
    ImGui::Text("OpenGL %s", glGetString(GL_VERSION) );
    ImGui::PopFont();
    ImGui::Separator();
    ImGui::Text("OpenGL is the premier environment for developing portable, \ninteractive 2D and 3D graphics applications.");
    ImGuiToolkit::ButtonOpenUrl("Visit website", "https://www.opengl.org");
    ImGui::SameLine();

    static bool show_opengl_info = false;
    ImGui::SetNextItemWidth(-100.f);
    ImGui::Text("          Details");
    ImGui::SameLine();

    ImGuiToolkit::IconToggle(10,0,11,0,&show_opengl_info);
    if (show_opengl_info)
    {
        ImGui::Separator();
        bool copy_to_clipboard = ImGui::Button( ICON_FA_COPY " Copy");
        ImGui::SameLine(0.f, 60.f);
        static char _openglfilter[64] = "";
        ImGui::InputText("Filter", _openglfilter, 64);
        ImGui::SameLine();
        if ( ImGuiToolkit::ButtonIcon( 12, 14 ) )
            _openglfilter[0] = '\0';
        std::string filter(_openglfilter);

        ImGui::BeginChildFrame(ImGui::GetID("gstinfos"), ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 18), ImGuiWindowFlags_NoMove);
        if (copy_to_clipboard)
        {
            ImGui::LogToClipboard();
            ImGui::LogText("```\n");
        }

        ImGui::Text("OpenGL %s", glGetString(GL_VERSION) );
        ImGui::Text("%s %s", glGetString(GL_RENDERER), glGetString(GL_VENDOR));
        ImGui::Text("Extensions (runtime) :");

        GLint numExtensions = 0;
        glGetIntegerv( GL_NUM_EXTENSIONS, &numExtensions );
        for (int i = 0; i < numExtensions; ++i){
            std::string ext( (char*) glGetStringi(GL_EXTENSIONS, i) );
            if ( filter.empty() || ext.find(filter) != std::string::npos )
                ImGui::Text("%s", ext.c_str());
        }


        if (copy_to_clipboard)
        {
            ImGui::LogText("\n```\n");
            ImGui::LogFinish();
        }

        ImGui::EndChildFrame();
    }
    ImGui::End();
}

void ShowAboutGStreamer(bool* p_open)
{
    ImGui::SetNextWindowPos(ImVec2(430, 20), ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(600, 200), ImGuiCond_Appearing);
    if (ImGui::Begin("About Gstreamer", p_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
    {
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_BOLD);
        ImGui::Text("GStreamer %s", GstToolkit::gst_version().c_str());
        ImGui::PopFont();
        ImGui::Separator();
        ImGui::Text("A flexible, fast and multiplatform multimedia framework.");
        ImGui::Text("GStreamer is licensed under the LGPL License.");
        ImGuiToolkit::ButtonOpenUrl("Visit website", "https://gstreamer.freedesktop.org/");
        ImGui::SameLine();

        static bool show_config_info = false;
        ImGui::SetNextItemWidth(-100.f);
        ImGui::Text("          Details");
        ImGui::SameLine();
        ImGuiToolkit::IconToggle(10,0,11,0,&show_config_info);
        if (show_config_info)
        {
            ImGui::Separator();
            bool copy_to_clipboard = ImGui::Button( ICON_FA_COPY " Copy");
            ImGui::SameLine(0.f, 60.f);
            static char _filter[64] = ""; ImGui::InputText("Filter", _filter, 64);
            ImGui::SameLine();
            if ( ImGuiToolkit::ButtonIcon( 12, 14 ) )
                _filter[0] = '\0';
            std::string filter(_filter);

            ImGui::BeginChildFrame(ImGui::GetID("gstinfos"), ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 18), ImGuiWindowFlags_NoMove);
            if (copy_to_clipboard)
            {
                ImGui::LogToClipboard();
                ImGui::LogText("```\n");
            }

            ImGui::Text("GStreamer %s", GstToolkit::gst_version().c_str());
            ImGui::Text("Plugins & features (runtime) :");

            std::list<std::string> filteredlist;
            static std::list<std::string> pluginslist;
            static std::map<std::string, std::list<std::string> > featureslist;
            if (pluginslist.empty()) {
                pluginslist = GstToolkit::all_plugins();
                for (auto const& i: pluginslist) {
                    // list features
                    featureslist[i] = GstToolkit::all_plugin_features(i);
                }
            }

            // filter list
            if ( filter.empty() )
                filteredlist = pluginslist;
            else {
                for (auto const& i: pluginslist) {
                    // add plugin if plugin name matches
                    if ( i.find(filter) != std::string::npos )
                        filteredlist.push_back( i );
                    // check in features
                    for (auto const& j: featureslist[i]) {
                        // add plugin if feature name matches
                        if ( j.find(filter) != std::string::npos )
                            filteredlist.push_back( i );
                    }
                }
                filteredlist.unique();
            }

            // display list
            for (auto const& t: filteredlist) {
                ImGui::Text("> %s", t.c_str());
                for (auto const& j: featureslist[t]) {
                    if ( j.find(filter) != std::string::npos )
                    {
                        ImGui::Text(" -   %s", j.c_str());
                    }
                }
            }

            if (copy_to_clipboard)
            {
                ImGui::LogText("\n```\n");
                ImGui::LogFinish();
            }

            ImGui::EndChildFrame();
        }
        ImGui::End();
    }
}

void SetMouseCursor(ImVec2 mousepos, View::Cursor c)
{
    // Hack if GLFW does not have all cursors, ask IMGUI to redraw cursor
#if GLFW_HAS_NEW_CURSORS == 0
    ImGui::GetIO().MouseDrawCursor = (c.type > 0); // only redraw non-arrow cursor
#endif
    ImGui::SetMouseCursor(c.type);

    if ( !c.info.empty()) {
        float d = 0.5f * ImGui::GetFrameHeight() ;
        ImVec2 window_pos = ImVec2( mousepos.x - d, mousepos.y - d );
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.75f); // Transparent background
        if (ImGui::Begin("MouseInfoContext", NULL, ImGuiWindowFlags_NoMouseInputs | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
        {
            ImGuiToolkit::PushFont(ImGuiToolkit::FONT_MONO);
            ImGui::Text("   %s", c.info.c_str());
            ImGui::PopFont();
            ImGui::End();
        }
    }
}

void SetNextWindowVisible(ImVec2 pos, ImVec2 size, float margin)
{
    bool need_update = false;
    ImVec2 pos_target = pos;
    const ImGuiIO& io = ImGui::GetIO();

    if ( pos_target.y > io.DisplaySize.y - margin  ){
        pos_target.y = io.DisplaySize.y - margin;
        need_update = true;
    }
    if ( pos_target.y + size.y < margin ){
        pos_target.y = margin - size.y;
        need_update = true;
    }
    if ( pos_target.x > io.DisplaySize.x - margin){
        pos_target.x = io.DisplaySize.x - margin;
        need_update = true;
    }
    if ( pos_target.x + size.x < margin ){
        pos_target.x = margin - size.x;
        need_update = true;
    }
    if (need_update)
        ImGui::SetNextWindowPos(pos_target, ImGuiCond_Always);
}

