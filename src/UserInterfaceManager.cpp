/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2022 Bruno Herbelin <bruno.herbelin@gmail.com>
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
#include <fstream>
#include <sstream>
#include <sstream>
#include <cstring>
#include <thread>
#include <algorithm>
#include <array>
#include <map>

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
#include <glm/gtc/type_ptr.hpp>
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
#include "ControlManager.h"
#include "Connection.h"
#include "ActionManager.h"
#include "Resource.h"
#include "Settings.h"
#include "SessionCreator.h"
#include "Mixer.h"
#include "MixingGroup.h"
#include "Recorder.h"
#include "Streamer.h"
#include "Loopback.h"
#include "Selection.h"
#include "FrameBuffer.h"
#include "MediaPlayer.h"
#include "SourceCallback.h"
#include "CloneSource.h"
#include "RenderSource.h"
#include "MediaSource.h"
#include "SessionSource.h"
#include "PatternSource.h"
#include "DeviceSource.h"
#include "NetworkSource.h"
#include "SrtReceiverSource.h"
#include "StreamSource.h"
#include "MultiFileSource.h"
#include "PickingVisitor.h"
#include "ImageFilter.h"
#include "ImageShader.h"
#include "ImageProcessingShader.h"
#include "Metronome.h"
#include "VideoBroadcast.h"
#include "ShmdataBroadcast.h"
#include "MultiFileRecorder.h"

#include "TextEditor.h"
TextEditor _editor;

#include "UserInterfaceManager.h"
#define PLOT_CIRCLE_SEGMENTS 64
#define PLOT_ARRAY_SIZE 180
#define LABEL_AUTO_MEDIA_PLAYER ICON_FA_USER_CIRCLE "  User selection"
#define LABEL_STORE_SELECTION "  Create batch"
#define LABEL_EDIT_FADING ICON_FA_RANDOM "  Fade in & out"
#define LABEL_VIDEO_SEQUENCE "  Encode an image sequence"

// utility functions
void ShowAboutGStreamer(bool* p_open);
void ShowAboutOpengl(bool* p_open);
void ShowSandbox(bool* p_open);
void SetMouseCursor(ImVec2 mousepos, View::Cursor c = View::Cursor());

std::string readable_date_time_string(std::string date){
    if (date.length()<12)
        return "";
    std::string s = date.substr(6, 2) + "/" + date.substr(4, 2) + "/" + date.substr(0, 4);
    s += " @ " + date.substr(8, 2) + ":" + date.substr(10, 2);
    return s;
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
    screenshot_step = 0;
    pending_save_on_exit = false;

    sessionopendialog = nullptr;
    sessionimportdialog = nullptr;
    sessionsavedialog = nullptr;
}

bool UserInterface::Init()
{
    if (Rendering::manager().mainWindow().window()== nullptr)
        return false;

    pending_save_on_exit = false;

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = Settings::application.scale;

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(Rendering::manager().mainWindow().window(), true);
    ImGui_ImplOpenGL3_Init(Rendering::manager().glsl_version.c_str());

    // hack to change keys according to keyboard layout
    io.KeyMap[ImGuiKey_A] = Control::layoutKey(GLFW_KEY_A);
    io.KeyMap[ImGuiKey_C] = Control::layoutKey(GLFW_KEY_C);
    io.KeyMap[ImGuiKey_V] = Control::layoutKey(GLFW_KEY_V);
    io.KeyMap[ImGuiKey_X] = Control::layoutKey(GLFW_KEY_X);
    io.KeyMap[ImGuiKey_Y] = Control::layoutKey(GLFW_KEY_Y);
    io.KeyMap[ImGuiKey_Z] = Control::layoutKey(GLFW_KEY_Z);

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
    ImGuiToolkit::SetFont(ImGuiToolkit::FONT_LARGE, "Hack-Regular", MIN(int(base_font_size * 1.5f), 50) );

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
    sessionimportdialog = new DialogToolkit::OpenSessionDialog("Import Sources");

    // init tooltips
    ImGuiToolkit::setToolTipsEnabled(Settings::application.show_tooptips);

    return true;
}

uint64_t UserInterface::Runtime() const
{
    return gst_util_get_timestamp () - start_time;
}


void UserInterface::handleKeyboard()
{
    static bool esc_repeat_ = false;

    const ImGuiIO& io = ImGui::GetIO();
    alt_modifier_active = io.KeyAlt;
    shift_modifier_active = io.KeyShift;
    ctrl_modifier_active = io.ConfigMacOSXBehaviors ? io.KeySuper : io.KeyCtrl;
    keyboard_available = !io.WantCaptureKeyboard;

    if (io.WantCaptureKeyboard || io.WantTextInput) {
        // only react to ESC key when keyboard is captured
        if (ImGui::IsKeyPressed( GLFW_KEY_ESCAPE, false )) {

        }
        return;
    }

    // Application "CTRL +"" Shortcuts
    if ( ctrl_modifier_active ) {

        if (ImGui::IsKeyPressed( Control::layoutKey(GLFW_KEY_Q), false ))  {
            // try quit
            if ( TryClose() )
                Rendering::manager().close();
        }
        else if (ImGui::IsKeyPressed( Control::layoutKey(GLFW_KEY_O), false )) {
            // SHIFT + CTRL + O : reopen current session
            if (shift_modifier_active && !Mixer::manager().session()->filename().empty())
                Mixer::manager().load( Mixer::manager().session()->filename() );
            // CTRL + O : Open session
            else
                selectOpenFilename();
        }
        else if (ImGui::IsKeyPressed( Control::layoutKey(GLFW_KEY_S), false )) {
            // SHIFT + CTRL + S : save as
            if (shift_modifier_active)
                selectSaveFilename();
            // CTRL + S : save (save as if necessary)
            else
                saveOrSaveAs();
        }
        else if (ImGui::IsKeyPressed( Control::layoutKey(GLFW_KEY_W), false )) {
            // New Session
            Mixer::manager().close();
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_SPACE, false )) {
            // restart media player
            sourcecontrol.Replay();
        }
        else if (ImGui::IsKeyPressed( Control::layoutKey(GLFW_KEY_L), false )) {
            // Logs
            Settings::application.widget.logs = !Settings::application.widget.logs;
        }
        else if (ImGui::IsKeyPressed( Control::layoutKey(GLFW_KEY_T), false )) {
            // Timers
            timercontrol.setVisible(!Settings::application.widget.timer);
        }
        else if (ImGui::IsKeyPressed( Control::layoutKey(GLFW_KEY_G), false )) {
            // Developer toolbox
            Settings::application.widget.toolbox = !Settings::application.widget.toolbox;
        }
        else if (ImGui::IsKeyPressed( Control::layoutKey(GLFW_KEY_H), false )) {
            // Helper
            Settings::application.widget.help = !Settings::application.widget.help;
        }
        else if (ImGui::IsKeyPressed( Control::layoutKey(GLFW_KEY_E), false )) {
            // Shader Editor
            shadercontrol.setVisible(!Settings::application.widget.shader_editor);
        }
        else if (ImGui::IsKeyPressed( Control::layoutKey(GLFW_KEY_D), false )) {
            // Display output
            outputcontrol.setVisible(!Settings::application.widget.preview);
        }
        else if (ImGui::IsKeyPressed( Control::layoutKey(GLFW_KEY_P), false )) {
            // Media player
            sourcecontrol.setVisible(!Settings::application.widget.media_player);
        }
        else if (ImGui::IsKeyPressed( Control::layoutKey(GLFW_KEY_A), false )) {
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
        else if (ImGui::IsKeyPressed( Control::layoutKey(GLFW_KEY_R), false )) {
            // toggle recording stop / start (or save and continue if + ALT modifier)
            outputcontrol.ToggleRecord(alt_modifier_active);
        }
        else if (ImGui::IsKeyPressed( Control::layoutKey(GLFW_KEY_Z), false )) {
            if (shift_modifier_active)
                Action::manager().redo();
            else
                Action::manager().undo();
        }
        else if (ImGui::IsKeyPressed( Control::layoutKey(GLFW_KEY_C), false )) {
            std::string clipboard = Mixer::selection().clipboard();
            if (!clipboard.empty())
                ImGui::SetClipboardText(clipboard.c_str());
        }
        else if (ImGui::IsKeyPressed( Control::layoutKey(GLFW_KEY_X), false )) {
            std::string clipboard = Mixer::selection().clipboard();
            if (!clipboard.empty()) {
                ImGui::SetClipboardText(clipboard.c_str());
                Mixer::manager().deleteSelection();
            }
        }
        else if (ImGui::IsKeyPressed( Control::layoutKey(GLFW_KEY_V), false )) {
            auto clipboard = ImGui::GetClipboardText();
            if (clipboard != nullptr && strlen(clipboard) > 0)
                Mixer::manager().paste(clipboard);
        }
        else if (ImGui::IsKeyPressed( Control::layoutKey(GLFW_KEY_F), false )) {
            Rendering::manager().mainWindow().toggleFullscreen();
        }
        else if (ImGui::IsKeyPressed( Control::layoutKey(GLFW_KEY_M), false )) {
            Settings::application.widget.stats = !Settings::application.widget.stats;
        }
        else if (ImGui::IsKeyPressed( Control::layoutKey(GLFW_KEY_I), false )) {
            Settings::application.widget.inputs = !Settings::application.widget.inputs;
        }
        else if (ImGui::IsKeyPressed( Control::layoutKey(GLFW_KEY_N), false ) && shift_modifier_active) {
            Mixer::manager().session()->addNote();
        }

    }
    // No CTRL modifier
    else {

        // Application F-Keys
        if (ImGui::IsKeyPressed( GLFW_KEY_F1, false ))
            Mixer::manager().setView(View::MIXING);
        else if (ImGui::IsKeyPressed( GLFW_KEY_F2, false ))
            Mixer::manager().setView(View::GEOMETRY);
        else if (ImGui::IsKeyPressed( GLFW_KEY_F3, false ))
            Mixer::manager().setView(View::LAYER);
        else if (ImGui::IsKeyPressed( GLFW_KEY_F4, false ))
            Mixer::manager().setView(View::TEXTURE);
        else if (ImGui::IsKeyPressed( GLFW_KEY_F5, false ))
            Mixer::manager().setView(View::DISPLAYS);
        else if (ImGui::IsKeyPressed( GLFW_KEY_F9, false ))
            StartScreenshot();
        else if (ImGui::IsKeyPressed( GLFW_KEY_F10, false ))
            sourcecontrol.Capture();
        else if (ImGui::IsKeyPressed( GLFW_KEY_F11, false ))
            FrameGrabbing::manager().add(new PNGRecorder(SystemToolkit::base_filename( Mixer::manager().session()->filename())));
        else if (ImGui::IsKeyPressed( GLFW_KEY_F12, false )) {
            Settings::application.render.disabled = !Settings::application.render.disabled;
        }
        // button home to toggle menu
        else if (ImGui::IsKeyPressed( GLFW_KEY_HOME, false ))
            navigator.togglePannelMenu();
        // button home to toggle menu
        else if (ImGui::IsKeyPressed( GLFW_KEY_INSERT, false ))
            navigator.togglePannelNew();
        // button esc : react to press and to release
        else if (ImGui::IsKeyPressed( GLFW_KEY_ESCAPE, false )) {
            // hide pannel
            navigator.hidePannel();
            // toggle clear workspace
            WorkspaceWindow::toggleClearRestoreWorkspace();
            // ESC key is not yet maintained pressed
            esc_repeat_ = false;
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_ESCAPE, true )) {
            // ESC key is maintained pressed
            esc_repeat_ = true;
        }
        else if ( esc_repeat_ && WorkspaceWindow::clear() && ImGui::IsKeyReleased( GLFW_KEY_ESCAPE )) {
            // restore cleared workspace when releasing ESC after maintain
            WorkspaceWindow::restoreWorkspace();
            esc_repeat_ = false;
        }
        // Space bar
        else if (ImGui::IsKeyPressed( GLFW_KEY_SPACE, false ))
            // Space bar to toggle play / pause
            sourcecontrol.Play();
        // Backspace to delete source
        else if (ImGui::IsKeyPressed( GLFW_KEY_BACKSPACE ) || ImGui::IsKeyPressed( GLFW_KEY_DELETE ))
            Mixer::manager().deleteSelection();
        // button tab to select next
        else if ( !alt_modifier_active && ImGui::IsKeyPressed( GLFW_KEY_TAB )) {
            // cancel selection
            if (Mixer::selection().size() > 1)
                Mixer::selection().clear();
            if (shift_modifier_active)
                Mixer::manager().setCurrentPrevious();
            else
                Mixer::manager().setCurrentNext();
            if (navigator.pannelVisible())
                navigator.showPannelSource( Mixer::manager().indexCurrentSource() );
        }
        // arrow keys to act on current view
        else if ( ImGui::IsKeyDown( GLFW_KEY_LEFT  ) ||
                  ImGui::IsKeyDown( GLFW_KEY_RIGHT ) ||
                  ImGui::IsKeyDown( GLFW_KEY_UP    ) ||
                  ImGui::IsKeyDown( GLFW_KEY_DOWN  ) ){
            glm::vec2 delta(0.f, 0.f);
            delta.x += ImGui::IsKeyDown( GLFW_KEY_RIGHT ) ? 1.f : ImGui::IsKeyDown( GLFW_KEY_LEFT ) ? -1.f : 0.f;
            delta.y += ImGui::IsKeyDown( GLFW_KEY_DOWN ) ? 1.f : ImGui::IsKeyDown( GLFW_KEY_UP ) ? -1.f : 0.f;
            Mixer::manager().view()->arrow( delta );
        }
        else if ( ImGui::IsKeyReleased( GLFW_KEY_LEFT  ) ||
                  ImGui::IsKeyReleased( GLFW_KEY_RIGHT ) ||
                  ImGui::IsKeyReleased( GLFW_KEY_UP    ) ||
                  ImGui::IsKeyReleased( GLFW_KEY_DOWN  ) ){
            Mixer::manager().view()->terminate(true);
        }

    }

    // special case: CTRL + TAB is ALT + TAB in OSX
    if (io.ConfigMacOSXBehaviors ? io.KeyAlt : io.KeyCtrl) {
        if (ImGui::IsKeyPressed( GLFW_KEY_TAB, false ))
            show_view_navigator += shift_modifier_active ? 3 : 1;
    }
    else if (show_view_navigator > 0) {
        show_view_navigator  = 0;
        Mixer::manager().setView((View::Mode) target_view_navigator);
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
    if ( !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) && !ImGui::IsWindowFocused(ImGuiHoveredFlags_AnyWindow) )
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
            // if double clic action of view didn't succeed
            if ( !Mixer::manager().view()->doubleclic(mousepos) ) {
                // default behavior :
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
        if (mousedown || view_drag)
            Mixer::manager().view()->terminate();

        view_drag = nullptr;
        mousedown = false;
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


bool UserInterface::TryClose()
{
    // cannot close if a file dialog is pending
    if (DialogToolkit::FileDialog::busy() || DialogToolkit::ColorPickerDialog::busy())
        return false;

    // always stop all recordings
    FrameGrabbing::manager().stopAll();

    // force close if trying to close again although it is already pending for save
    if (pending_save_on_exit)
        return true;

    // check if there is something to save
    pending_save_on_exit = false;
    if (!Mixer::manager().session()->empty())
    {
        // determine if a pending save of session is required
        if (Mixer::manager().session()->filename().empty())
            // need to wait for user to give a filename
            pending_save_on_exit = true;
        // save on exit
        else if (Settings::application.recentSessions.save_on_exit)
            // ok to save the session
            Mixer::manager().save(false);
    }

    // say we can close if no pending save of session is needed
    return !pending_save_on_exit;
}

void UserInterface::selectSaveFilename()
{
    if (sessionsavedialog) {
        if (!Mixer::manager().session()->filename().empty())
            sessionsavedialog->setFolder( Mixer::manager().session()->filename() );

        sessionsavedialog->open();
    }

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
        if (!ImGui::IsPopupOpen("Busy"))
            ImGui::OpenPopup("Busy");
        if (ImGui::BeginPopupModal("Busy", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Close file dialog box to resume.");
            ImGui::EndPopup();
        }
    }

    // popup to inform to save before close
    if (pending_save_on_exit) {
        if (!ImGui::IsPopupOpen(MENU_SAVE_ON_EXIT))
            ImGui::OpenPopup(MENU_SAVE_ON_EXIT);
        if (ImGui::BeginPopupModal(MENU_SAVE_ON_EXIT, NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Spacing();
            ImGuiToolkit::PushFont(ImGuiToolkit::FONT_ITALIC);
            ImGui::Text("Looks like you started some work");
            ImGui::Text("but didn't save the session.");
            ImGui::PopFont();
            ImGui::Spacing();
            if (ImGui::Button(MENU_SAVEAS_FILE, ImVec2(ImGui::GetWindowContentRegionWidth(), 0))) {
                pending_save_on_exit = false;
                saveOrSaveAs();
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::Button(MENU_QUIT, ImVec2(ImGui::GetWindowContentRegionWidth(), 0))) {
                Rendering::manager().close();
                ImGui::CloseCurrentPopup();
            }
            ImGui::Spacing();
            ImGui::EndPopup();
        }
    }

}

void UserInterface::Render()
{
    // navigator bar first
    navigator.Render();

    // update windows before render
    outputcontrol.Update();
    sourcecontrol.Update();
    timercontrol.Update();
    inputscontrol.Update();
    shadercontrol.Update();

    // warnings and notifications
    Log::Render(&Settings::application.widget.logs);

    if ( WorkspaceWindow::clear())
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.4);

    // Output controller
    if (outputcontrol.Visible())
        outputcontrol.Render();

    // Source controller
    if (sourcecontrol.Visible())
        sourcecontrol.Render();

    // Timer controller
    if (timercontrol.Visible())
        timercontrol.Render();

    // Keyboards controller
    if (inputscontrol.Visible())
        inputscontrol.Render();

    // Shader controller
    if (shadercontrol.Visible())
        shadercontrol.Render();

    // Logs
    if (Settings::application.widget.logs)
        Log::ShowLogWindow(&Settings::application.widget.logs);

    // stats in the corner
    if (Settings::application.widget.stats)
        RenderMetrics(&Settings::application.widget.stats,
                  &Settings::application.widget.stats_corner,
                  &Settings::application.widget.stats_mode);

    if ( WorkspaceWindow::clear())
        ImGui::PopStyleVar();
    // All other windows are simply not rendered if workspace is clear
    else {
        // windows
        if (Settings::application.widget.help)
            helpwindow.Render();
        if (Settings::application.widget.toolbox)
            toolbox.Render();

        // About
        if (show_vimix_about)
            RenderAbout(&show_vimix_about);
        if (show_imgui_about)
            ImGui::ShowAboutWindow(&show_imgui_about);
        if (show_gst_about)
            ShowAboutGStreamer(&show_gst_about);
        if (show_opengl_about)
            ShowAboutOpengl(&show_opengl_about);
    }

    // Notes
    RenderNotes();

    // Navigator
    if (show_view_navigator > 0)
        target_view_navigator = RenderViewNavigator( &show_view_navigator );

    // handle keyboard input after all IMGUI widgets have potentially captured keyboard
    handleKeyboard();

    // all IMGUI Rendering
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

}

void UserInterface::Terminate()
{
    // restore windows position for saving
    WorkspaceWindow::restoreWorkspace(true);

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

    // UNDO
    if ( ImGui::MenuItem( MENU_UNDO, SHORTCUT_UNDO) )
        Action::manager().undo();
    if ( ImGui::MenuItem( MENU_REDO, SHORTCUT_REDO) )
        Action::manager().redo();

    // EDIT
    ImGui::Separator();
    if (ImGui::MenuItem( MENU_CUT, SHORTCUT_CUT, false, has_selection)) {
        std::string copied_text = Mixer::selection().clipboard();
        if (!copied_text.empty()) {
            ImGui::SetClipboardText(copied_text.c_str());
            Mixer::manager().deleteSelection();
        }
        navigator.hidePannel();
    }
    if (ImGui::MenuItem( MENU_COPY, SHORTCUT_COPY, false, has_selection)) {
        std::string copied_text = Mixer::selection().clipboard();
        if (!copied_text.empty())
            ImGui::SetClipboardText(copied_text.c_str());
        navigator.hidePannel();
    }
    if (ImGui::MenuItem( MENU_PASTE, SHORTCUT_PASTE, false, has_clipboard)) {
        if (clipboard)
            Mixer::manager().paste(clipboard);
        navigator.hidePannel();
    }
    if (ImGui::MenuItem( MENU_SELECTALL, SHORTCUT_SELECTALL)) {
        Mixer::manager().view()->selectAll();
        navigator.hidePannel();
    }

    // GROUP
    ImGui::Separator();
    if (ImGuiToolkit::MenuItemIcon(11, 2, " Bundle all active sources", false, Mixer::manager().numSource() > 0)) {
        // create a new group session with only active sources
        Mixer::manager().groupAll( true );
        // switch pannel to show first source (created)
        navigator.showPannelSource(0);
    }
    if (ImGuiToolkit::MenuItemIcon(7, 2, " Expand all bundles", false, Mixer::manager().numSource() > 0)) {
        // create a new group session with all sources
        Mixer::manager().ungroupAll();
    }
}

void UserInterface::showMenuFile()
{
    // NEW
    if (ImGui::MenuItem( MENU_NEW_FILE, SHORTCUT_NEW_FILE)) {
        Mixer::manager().close();
        navigator.hidePannel();
    }
    ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x * 0.54f);
    ImGui::Combo("Ratio", &Settings::application.render.ratio, RenderView::ratio_preset_name, IM_ARRAYSIZE(RenderView::ratio_preset_name) );
    if (Settings::application.render.ratio < RenderView::AspectRatio_Custom) {
        // Presets height
        ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x * 0.54f);
        ImGui::Combo("Height", &Settings::application.render.res, RenderView::height_preset_name, IM_ARRAYSIZE(RenderView::height_preset_name) );
    }
    else {
        // Custom width and height
        ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x * 0.54f);
        ImGui::InputInt("Width", &Settings::application.render.custom_width, 100, 500);
        ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x * 0.54f);
        ImGui::InputInt("Height", &Settings::application.render.custom_height, 100, 500);
    }

    // FILE OPEN AND SAVE
    ImGui::Separator();
    const std::string currentfilename = Mixer::manager().session()->filename();
    const bool currentfileopen = !currentfilename.empty();

    ImGui::MenuItem( MENU_OPEN_ON_START, nullptr, &Settings::application.recentSessions.load_at_start);

    if (ImGui::MenuItem( MENU_OPEN_FILE, SHORTCUT_OPEN_FILE))
        selectOpenFilename();
    if (ImGui::MenuItem( MENU_REOPEN_FILE, SHORTCUT_REOPEN_FILE, false, currentfileopen))
        Mixer::manager().load( currentfilename );

    if (sessionimportdialog && ImGui::MenuItem( ICON_FA_FILE_EXPORT " Import" )) {
        // launch file dialog to open a session file
        sessionimportdialog->open();
        // close pannel to select file
        navigator.hidePannel();
    }

    if (ImGui::MenuItem( MENU_SAVE_FILE, SHORTCUT_SAVE_FILE, false, currentfileopen)) {
        if (saveOrSaveAs())
            navigator.hidePannel();
    }
    if (ImGui::MenuItem( MENU_SAVEAS_FILE, SHORTCUT_SAVEAS_FILE))
        selectSaveFilename();

    ImGui::MenuItem( MENU_SAVE_ON_EXIT, nullptr, &Settings::application.recentSessions.save_on_exit);

    // HELP AND QUIT
    ImGui::Separator();
    if (ImGui::MenuItem( IMGUI_TITLE_HELP, SHORTCUT_HELP))
        Settings::application.widget.help = true;
    if (ImGui::MenuItem( MENU_QUIT, SHORTCUT_QUIT) && TryClose())
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
        for (int v = View::MIXING; v < View::TRANSITION; ++v) {
            ImGui::NextColumn();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetColumnWidth() - ImGui::CalcTextSize(Settings::application.views[v].name.c_str()).x) * 0.5f - ImGui::GetStyle().ItemSpacing.x);
            ImGuiToolkit::PushFont(Settings::application.current_view == v ? ImGuiToolkit::FONT_BOLD : ImGuiToolkit::FONT_DEFAULT);
            ImGui::Text("%s", Settings::application.views[v].name.c_str());
            ImGui::PopFont();
        }

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
        if (!s->failed()) {
            sourcecontrol.setVisible(true);
            sourcecontrol.resetActiveSelection();
            return;
        }
        navigator.showPannelSource( Mixer::manager().indexCurrentSource() );
    }
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

    // combo selection of mode, with fixed width to display enough text
    ImGui::SetNextItemWidth( 8.f * ImGui::GetTextLineHeight());
    ImGui::Combo("##mode", p_mode,
                 ICON_FA_TACHOMETER_ALT "  Performance\0"
                 ICON_FA_HOURGLASS_HALF "   Runtime\0"
                 ICON_FA_VECTOR_SQUARE  "  Source\0");

    ImGui::SameLine(0, IMGUI_SAME_LINE);
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
                s->call(new SetAlpha(v), true);
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
    ImGui::SetNextWindowPos(ImVec2(1100, 20), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("About " APP_TITLE, p_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::End();
        return;
    }

    ImVec2 top = ImGui::GetCursorScreenPos();
#ifdef VIMIX_VERSION_MAJOR
    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
    ImGui::Text("%s %d.%d.%d", APP_NAME, VIMIX_VERSION_MAJOR, VIMIX_VERSION_MINOR, VIMIX_VERSION_PATCH);
    ImGui::PopFont();
#else
    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_BOLD);
    ImGui::Text("%s", APP_NAME);
    ImGui::PopFont();
#endif

    static unsigned int img_crow = 0;
    if (img_crow == 0)
        img_crow = Resource::getTextureImage("images/vimix_crow_white.png");
    ImGui::SetCursorScreenPos(top);
    ImGui::Image((void*)(intptr_t)img_crow, ImVec2(512, 340));

    ImGui::Text("vimix performs graphical mixing and blending of\nseveral movie clips and computer generated graphics,\nwith image processing effects in real-time.");
    ImGui::Text("\nvimix is licensed under GNU GPL version 3 or later.\n" UNICODE_COPYRIGHT " 2019-2022 Bruno Herbelin.");

    ImGui::Spacing();
    ImGuiToolkit::ButtonOpenUrl("Visit vimix website", "https://brunoherbelin.github.io/vimix/", ImVec2(ImGui::GetContentRegionAvail().x, 0));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("vimix is built using the following libraries:");

    if ( ImGui::Button("About Dear ImGui (build information)", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
        show_imgui_about = true;

    if ( ImGui::Button("About GStreamer (plugins)", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
        show_gst_about = true;

    if ( ImGui::Button("About OpenGL (runtime extensions)", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
        show_opengl_about = true;

    ImGui::Columns(3, "abouts");
    ImGui::Separator();
    ImGuiToolkit::ButtonOpenUrl("glad", "https://glad.dav1d.de", ImVec2(ImGui::GetContentRegionAvail().x, 0));

    ImGui::NextColumn();
    ImGuiToolkit::ButtonOpenUrl("glfw3", "http://www.glfw.org", ImVec2(ImGui::GetContentRegionAvail().x, 0));

    ImGui::NextColumn();
    ImGuiToolkit::ButtonOpenUrl("glm", "https://glm.g-truc.net", ImVec2(ImGui::GetContentRegionAvail().x, 0));

    ImGui::NextColumn();
    ImGuiToolkit::ButtonOpenUrl("OSCPack", "http://www.rossbencina.com/code/oscpack", ImVec2(ImGui::GetContentRegionAvail().x, 0));

    ImGui::NextColumn();
    ImGuiToolkit::ButtonOpenUrl("TinyXML2", "https://github.com/leethomason/tinyxml2.git", ImVec2(ImGui::GetContentRegionAvail().x, 0));

    ImGui::NextColumn();
    ImGuiToolkit::ButtonOpenUrl("STB", "https://github.com/nothings/stb", ImVec2(ImGui::GetContentRegionAvail().x, 0));

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
    if (se!=nullptr && se->beginNotes() != se->endNotes()) {

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
                    close = ImGuiToolkit::IconButton(4,16,"Delete");
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
            if ( ImGui::MenuItem( ICON_FA_CAMERA_RETRO "  View screenshot", "F9") )
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
    static float recorded_bounds[3][2] = {  {40.f, 65.f}, {1.f, 50.f}, {0.f, 50.f} };
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
            recorded_values[1][i] = 16.f;
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
    ImGui::SetNextWindowPos(ImVec2(520, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(460, 800), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(350, 300), ImVec2(FLT_MAX, FLT_MAX));

    if ( !ImGui::Begin(IMGUI_TITLE_HELP, &Settings::application.widget.help, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar |  ImGuiWindowFlags_NoCollapse ) )
    {
        ImGui::End();
        return;
    }

    // menu (no title bar)
    if (ImGui::BeginMenuBar())
    {
        // Close and widget menu
        if (ImGuiToolkit::IconButton(4,16))
            Settings::application.widget.help = false;
        if (ImGui::BeginMenu(IMGUI_TITLE_HELP))
        {
            // Enable/Disable Ableton Link
            if ( ImGui::MenuItem( ICON_FA_BOOK_OPEN "  Online wiki") ) {
                SystemToolkit::open("https://github.com/brunoherbelin/vimix/wiki");
            }

            // Enable/Disable tooltips
            if ( ImGui::MenuItem( ICON_FA_QUESTION_CIRCLE "  Show tooltips", nullptr, &Settings::application.show_tooptips) ) {
                ImGuiToolkit::setToolTipsEnabled( Settings::application.show_tooptips );
            }

            // output manager menu
            ImGui::Separator();
            if ( ImGui::MenuItem( MENU_CLOSE, SHORTCUT_HELP) )
                Settings::application.widget.help = false;

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    const float width_window = ImGui::GetWindowSize().x - ImGui::GetFontSize();
    const float width_column0 = ImGui::GetFontSize() * 6;


    if (ImGui::CollapsingHeader("Documentation", ImGuiTreeNodeFlags_DefaultOpen))
    {

        ImGui::Columns(2, "doccolumn", false); // 4-ways, with border
        ImGui::SetColumnWidth(0, width_column0);

        ImGui::Text("General"); ImGui::NextColumn();
        ImGuiToolkit::ButtonOpenUrl("User manual", "https://github.com/brunoherbelin/vimix/wiki/User-manual", ImVec2(ImGui::GetContentRegionAvail().x, 0));
        ImGui::NextColumn();
        ImGui::Text("Filters"); ImGui::NextColumn();
        ImGuiToolkit::ButtonOpenUrl("Filters and ShaderToy reference", "https://github.com/brunoherbelin/vimix/wiki/Filters-and-ShaderToy", ImVec2(ImGui::GetContentRegionAvail().x, 0));
        ImGui::NextColumn();
        ImGui::Text("OSC"); ImGui::NextColumn();
        ImGuiToolkit::ButtonOpenUrl("Open Sound Control API", "https://github.com/brunoherbelin/vimix/wiki/Open-Sound-Control-API", ImVec2(ImGui::GetContentRegionAvail().x, 0));
        ImGui::NextColumn();
        ImGui::Text("SRT"); ImGui::NextColumn();
        ImGuiToolkit::ButtonOpenUrl("Secure Reliable Transport Broadcast", "https://github.com/brunoherbelin/vimix/wiki/SRT-stream-I-O", ImVec2(ImGui::GetContentRegionAvail().x, 0));

        ImGui::Columns(1);
    }

    if (ImGui::CollapsingHeader("Views"))
    {

        ImGui::Columns(2, "viewscolumn", false); // 4-ways, with border
        ImGui::SetColumnWidth(0, width_column0);
        ImGui::PushTextWrapPos(width_window );

        ImGui::Text(ICON_FA_BULLSEYE "  Mixing"); ImGui::NextColumn();
        ImGui::Text ("Adjust opacity of sources, visible in the center and transparent on the side. Sources are de-activated outside of darker circle.");
        ImGui::NextColumn();
        ImGui::Text(ICON_FA_OBJECT_UNGROUP "  Geometry"); ImGui::NextColumn();
        ImGui::Text ("Move, scale, rotate or crop sources to place them in the output frame.");
        ImGui::NextColumn();
        ImGui::Text(ICON_FA_LAYER_GROUP "  Layers"); ImGui::NextColumn();
        ImGui::Text ("Organize the rendering order of sources, from background to foreground.");
        ImGui::NextColumn();
        ImGui::Text(ICON_FA_CHESS_BOARD "  Texturing"); ImGui::NextColumn();
        ImGui::Text ("Apply masks or freely paint the texture on the source surface. Repeat or crop the graphics.");
        ImGui::NextColumn();

        ImGui::Columns(1);
        ImGui::PopTextWrapPos();
    }

    if (ImGui::CollapsingHeader("Windows"))
    {
        ImGui::Columns(2, "windowcolumn", false); // 4-ways, with border
        ImGui::SetColumnWidth(0, width_column0);
        ImGui::PushTextWrapPos(width_window );

        ImGui::Text(IMGUI_TITLE_PREVIEW); ImGui::NextColumn();
        ImGui::Text ("Preview the output displayed in the rendering window. Control video recording and sharing to other vimix in local network.");
        ImGui::NextColumn();
        ImGui::Text(IMGUI_TITLE_MEDIAPLAYER); ImGui::NextColumn();
        ImGui::Text ("Play, pause, rewind videos or dynamic sources. Control play duration, speed and synchronize multiple videos.");
        ImGui::NextColumn();
        ImGui::Text(IMGUI_TITLE_TIMER); ImGui::NextColumn();
        ImGui::Text ("Keep track of time with a stopwatch or a metronome (Ableton Link).");
        ImGui::NextColumn();
        ImGui::Text(ICON_FA_HAND_PAPER "  Inputs"); ImGui::NextColumn();
        ImGui::Text ("Define how user inputs (e.g. keyboard, joystick) are mapped to custom actions in the session.");
        ImGui::NextColumn();
        ImGui::Text(ICON_FA_STICKY_NOTE "  Notes"); ImGui::NextColumn();
        ImGui::Text ("Place sticky notes into your session.");
        ImGui::NextColumn();
        ImGui::Text(ICON_FA_TACHOMETER_ALT "  Metrics"); ImGui::NextColumn();
        ImGui::Text ("Utility monitoring of metrics on the system (FPS, RAM), the runtime (session duration), or the current source.");
        ImGui::NextColumn();
        ImGui::Text(ICON_FA_COG "  Settings"); ImGui::NextColumn();
        ImGui::Text ("Set user preferences and system settings.");

        ImGui::Columns(1);
        ImGui::PopTextWrapPos();
    }

    if (ImGui::CollapsingHeader("Sources"))
    {
        ImGui::Columns(2, "sourcecolumn", false); // 4-ways, with border
        ImGui::SetColumnWidth(0, width_column0);
        ImGui::PushTextWrapPos(width_window );

        ImGui::Text(ICON_FA_PHOTO_VIDEO); ImGui::NextColumn();
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_BOLD); ImGui::Text("File");ImGui::PopFont();
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_VIDEO); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("Video"); ImGui::NextColumn();
        ImGui::Text ("Video file (*.mpg, *mov, *.avi, etc.).");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_IMAGE); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("Image"); ImGui::NextColumn();
        ImGui::Text ("Image file (*.jpg, *.png, etc.) or vector graphics (*.svg).");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_SESSION); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("Session"); ImGui::NextColumn();
        ImGui::Text ("Render a session (*.mix) as a source.");
        ImGui::NextColumn();
        ImGui::Separator();
        ImGui::Text(ICON_FA_IMAGES); ImGui::NextColumn();
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_BOLD); ImGui::Text("Sequence");ImGui::PopFont();
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_SEQUENCE); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("Sequence"); ImGui::NextColumn();
        ImGui::Text ("Set of images numbered sequentially (*.jpg, *.png, etc.).");
        ImGui::NextColumn();
        ImGui::Separator();
        ImGui::Text(ICON_FA_PLUG); ImGui::NextColumn();
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_BOLD); ImGui::Text("Connected");ImGui::PopFont();
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_DEVICE); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("Device"); ImGui::NextColumn();
        ImGui::Text ("Connected webcam or frame grabber.");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_DEVICE_SCREEN); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("Screen"); ImGui::NextColumn();
        ImGui::Text ("Screen capture.");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_NETWORK); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("Shared"); ImGui::NextColumn();
        ImGui::Text ("Connected stream from another vimix in the local network (shared output stream).");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_SRT); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("SRT"); ImGui::NextColumn();
        ImGui::Text ("Connected Secure Reliable Transport (SRT) stream emitted on the network (e.g. broadcasted by vimix).");
        ImGui::NextColumn();
        ImGui::Separator();
        ImGui::Text(ICON_FA_COG); ImGui::NextColumn();
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_BOLD); ImGui::Text("Generated");ImGui::PopFont();
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_PATTERN); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("Pattern"); ImGui::NextColumn();
        ImGui::Text ("Algorithmically generated source; colors, grids, test patterns, timers...");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_GSTREAMER); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("GStreamer"); ImGui::NextColumn();
        ImGui::Text ("Custom gstreamer pipeline, as described in command line for gst-launch-1.0 (without the target sink).");
        ImGui::NextColumn();
        ImGui::Separator();
        ImGui::Text(ICON_FA_SYNC); ImGui::NextColumn();
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_BOLD); ImGui::Text("Internal");ImGui::PopFont();
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_RENDER); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("Rendering"); ImGui::NextColumn();
        ImGui::Text ("Displays the rendering output as a source, with or without loopback.");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_CLONE); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("Clone"); ImGui::NextColumn();
        ImGui::Text ("Clone a source into another source with a filter.");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_GROUP); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("Group"); ImGui::NextColumn();
        ImGui::Text ("Group of sources rendered together after flattenning them in Layers view.");

        ImGui::Columns(1);
        ImGui::PopTextWrapPos();
    }

    if (ImGui::CollapsingHeader("Filters"))
    {
        ImGui::Text("Select 'Clone & Filter' on a source to access filters;");

        ImGui::Columns(2, "filterscolumn", false); // 4-ways, with border
        ImGui::SetColumnWidth(0, width_column0);
        ImGui::PushTextWrapPos(width_window );

        ImGuiToolkit::Icon(ICON_FILTER_DELAY); ImGui::SameLine(0, IMGUI_SAME_LINE);
        ImGui::Text("Delay"); ImGui::NextColumn();
        ImGui::Text("Postpones the display of the input source by a given delay (between 0.0 and 2.0 seconds).");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_FILTER_RESAMPLE); ImGui::SameLine(0, IMGUI_SAME_LINE);
        ImGui::Text("Resample"); ImGui::NextColumn();
        ImGui::Text ("Displays the input source with a different resolution. Downsampling is producing a smaller resolution (half or quarter). Upsampling is producing a higher resolution (double). GPU filtering is applied to improve scaling quality.");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_FILTER_BLUR); ImGui::SameLine(0, IMGUI_SAME_LINE);
        ImGui::Text("Blur"); ImGui::NextColumn();
        ImGui::Text ("Applies a real-time GPU bluring filter. Radius of the filter (when available) is a fraction of the image height. ");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_FILTER_SHARPEN); ImGui::SameLine(0, IMGUI_SAME_LINE);
        ImGui::Text("Sharpen"); ImGui::NextColumn();
        ImGui::Text ("Applies a real-time GPU sharpening filter.");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_FILTER_SMOOTH); ImGui::SameLine(0, IMGUI_SAME_LINE);
        ImGui::Text("Smooth"); ImGui::NextColumn();
        ImGui::Text ("Applies a real-time GPU smoothing filters to reduce noise. Inverse filters to add noise or grain are also available.");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_FILTER_EDGE); ImGui::SameLine(0, IMGUI_SAME_LINE);
        ImGui::Text("Edge"); ImGui::NextColumn();
        ImGui::Text ("Applies a real-time GPU filter to outline edges.");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_FILTER_ALPHA); ImGui::SameLine(0, IMGUI_SAME_LINE);
        ImGui::Text("Alpha"); ImGui::NextColumn();
        ImGui::Text ("Applies a real-time GPU chroma-key (green screen) or luma-key (black screen). Inverse filter fills transparent alpha with an opaque color.");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_FILTER_IMAGE); ImGui::SameLine(0, IMGUI_SAME_LINE);
        ImGui::Text("Custom"); ImGui::NextColumn();
        ImGui::Text ("Applies a real-time GPU fragment shader defined by custom code in OpenGL Shading Language (GLSL). ");
        ImGuiToolkit::ButtonOpenUrl("About GLSL", "https://www.khronos.org/opengl/wiki/OpenGL_Shading_Language", ImVec2(ImGui::GetContentRegionAvail().x, 0));
        ImGuiToolkit::ButtonOpenUrl("Browse shadertoy.com", "https://www.shadertoy.com", ImVec2(ImGui::GetContentRegionAvail().x, 0));

        ImGui::Columns(1);
        ImGui::PopTextWrapPos();
    }

    if (ImGui::CollapsingHeader("Input Mapping"))
    {
        ImGui::Columns(2, "inputcolumn", false); // 4-ways, with border
        ImGui::SetColumnWidth(0, width_column0);
        ImGui::PushTextWrapPos(width_window );

        ImGui::Text(ICON_FA_KEYBOARD "  Keyboard"); ImGui::NextColumn();
        ImGui::Text ("React to key press on standard keyboard, covering 25 keys from [A] to [Y], without modifier.");
        ImGui::NextColumn();
        ImGui::Text(ICON_FA_CALCULATOR "   Numpad"); ImGui::NextColumn();
        ImGui::Text ("React to key press on numerical keypad, covering 15 keys from [0] to [9] and including [ . ], [ + ], [ - ], [ * ], [ / ], without modifier.");
        ImGui::NextColumn();
        ImGui::Text(ICON_FA_TABLET_ALT "   TouchOSC"); ImGui::NextColumn();
        ImGui::Text ("React to OSC events sent in a local betwork by TouchOSC.");
        ImGuiToolkit::ButtonOpenUrl("Install TouchOSC", "https://github.com/brunoherbelin/vimix/wiki/TouchOSC-companion", ImVec2(ImGui::GetContentRegionAvail().x, 0));
        ImGui::NextColumn();
        ImGui::Text(ICON_FA_GAMEPAD " Gamepad"); ImGui::NextColumn();
        ImGui::Text ("React to button press and axis movement on a gamepad or a joystick. Only the first plugged device is considered.");

        ImGui::Columns(1);
        ImGui::PopTextWrapPos();
    }

    if (ImGui::CollapsingHeader("Keyboard shortcuts"))
    {
        ImGui::Columns(2, "keyscolumns", false); // 4-ways, with border
        ImGui::SetColumnWidth(0, width_column0);

        ImGui::Text("HOME"); ImGui::NextColumn();
        ImGui::Text(ICON_FA_BARS " Main menu"); ImGui::NextColumn();
        ImGui::Text("INS"); ImGui::NextColumn();
        ImGui::Text(ICON_FA_PLUS " New source"); ImGui::NextColumn();
        ImGui::Text("DEL"); ImGui::NextColumn();
        ImGui::Text(ICON_FA_BACKSPACE " Delete source"); ImGui::NextColumn();
        ImGui::Text("TAB"); ImGui::NextColumn();
        ImGui::Text(ICON_FA_EXCHANGE_ALT " Switch Current source"); ImGui::NextColumn();
        ImGui::Text("F1"); ImGui::NextColumn();
        ImGui::Text(ICON_FA_BULLSEYE " Mixing view"); ImGui::NextColumn();
        ImGui::Text("F2"); ImGui::NextColumn();
        ImGui::Text(ICON_FA_OBJECT_UNGROUP " Geometry view"); ImGui::NextColumn();
        ImGui::Text("F3"); ImGui::NextColumn();
        ImGui::Text(ICON_FA_LAYER_GROUP " Layers view"); ImGui::NextColumn();
        ImGui::Text("F4"); ImGui::NextColumn();
        ImGui::Text(ICON_FA_CHESS_BOARD " Texturing view"); ImGui::NextColumn();
        ImGui::Text(CTRL_MOD "TAB"); ImGui::NextColumn();
        ImGui::Text("Switch view"); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_FULLSCREEN); ImGui::NextColumn();
        ImGui::Text(ICON_FA_EXPAND_ALT " " TOOLTIP_FULLSCREEN "main window"); ImGui::NextColumn();
        ImGui::Separator();
        ImGui::Text(SHORTCUT_OUTPUT); ImGui::NextColumn();
        ImGui::Text(ICON_FA_WINDOW_MAXIMIZE " " TOOLTIP_OUTPUT "window"); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_PLAYER); ImGui::NextColumn();
        ImGui::Text(ICON_FA_PLAY_CIRCLE " " TOOLTIP_PLAYER "window" ); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_TIMER); ImGui::NextColumn();
        ImGui::Text(ICON_FA_CLOCK " " TOOLTIP_TIMER "window"); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_INPUTS); ImGui::NextColumn();
        ImGui::Text(ICON_FA_HAND_PAPER " " TOOLTIP_INPUTS "window"); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_SHADEREDITOR); ImGui::NextColumn();
        ImGui::Text(ICON_FA_CODE " " TOOLTIP_SHADEREDITOR "window"); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_NOTE); ImGui::NextColumn();
        ImGui::Text(ICON_FA_STICKY_NOTE " " TOOLTIP_NOTE); ImGui::NextColumn();
        ImGui::Text("ESC"); ImGui::NextColumn();
        ImGui::Text(ICON_FA_TOGGLE_OFF " Hide / " ICON_FA_TOGGLE_ON " Show windows"); ImGui::NextColumn();
        ImGui::Separator();
        ImGui::Text(SHORTCUT_NEW_FILE); ImGui::NextColumn();
        ImGui::Text(MENU_NEW_FILE " session"); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_OPEN_FILE); ImGui::NextColumn();
        ImGui::Text(MENU_OPEN_FILE " session"); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_REOPEN_FILE); ImGui::NextColumn();
        ImGui::Text(MENU_REOPEN_FILE " session"); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_SAVE_FILE); ImGui::NextColumn();
        ImGui::Text(MENU_SAVE_FILE " session"); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_SAVEAS_FILE); ImGui::NextColumn();
        ImGui::Text(MENU_SAVEAS_FILE " session"); ImGui::NextColumn();
        ImGui::Separator();
        ImGui::Text(SHORTCUT_CUT); ImGui::NextColumn();
        ImGui::Text(MENU_CUT " source"); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_COPY); ImGui::NextColumn();
        ImGui::Text(MENU_COPY " source"); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_PASTE); ImGui::NextColumn();
        ImGui::Text(MENU_PASTE); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_SELECTALL); ImGui::NextColumn();
        ImGui::Text(MENU_SELECTALL " source"); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_UNDO); ImGui::NextColumn();
        ImGui::Text(MENU_UNDO); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_REDO); ImGui::NextColumn();
        ImGui::Text(MENU_REDO); ImGui::NextColumn();
        ImGui::Separator();
        ImGui::Text(SHORTCUT_CAPTUREFRAME); ImGui::NextColumn();
        ImGui::Text(MENU_CAPTUREFRAME " Output"); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_RECORD); ImGui::NextColumn();
        ImGui::Text(MENU_RECORD " Output"); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_RECORDCONT); ImGui::NextColumn();
        ImGui::Text(MENU_RECORDCONT " recording"); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_OUTPUTDISABLE); ImGui::NextColumn();
        ImGui::Text(MENU_OUTPUTDISABLE " output window"); ImGui::NextColumn();
        ImGui::Separator();
        ImGui::Text("Space"); ImGui::NextColumn();
        ImGui::Text("Toggle Play/Pause selected videos"); ImGui::NextColumn();
        ImGui::Text(CTRL_MOD "Space"); ImGui::NextColumn();
        ImGui::Text("Restart selected videos"); ImGui::NextColumn();
        ImGui::Text(ICON_FA_ARROW_DOWN " " ICON_FA_ARROW_UP " " ICON_FA_ARROW_DOWN " " ICON_FA_ARROW_RIGHT ); ImGui::NextColumn();
        ImGui::Text("Move the selection in the canvas"); ImGui::NextColumn();
        ImGui::Separator();
        ImGui::Text(SHORTCUT_LOGS); ImGui::NextColumn();
        ImGui::Text(IMGUI_TITLE_LOGS); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_HELP); ImGui::NextColumn();
        ImGui::Text(IMGUI_TITLE_HELP " window"); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_QUIT); ImGui::NextColumn();
        ImGui::Text(MENU_QUIT); ImGui::NextColumn();

        ImGui::Columns(1);
    }

    ImGui::End();

}

///
/// SMART WINDOW
///

bool WorkspaceWindow::clear_workspace_enabled = false;
std::list<WorkspaceWindow *> WorkspaceWindow::windows_;

struct ImGuiProperties
{
    ImGuiWindow *ptr;
    ImVec2 user_pos;
    ImVec2 outside_pos;
    float progress;  // [0 1]
    float target;    //  1 go to outside, 0 go to user pos
    bool  animation;     // need animation
    bool  resizing_workspace;
    ImVec2 resized_pos;

    ImGuiProperties ()
    {
        ptr = nullptr;
        progress = 0.f;
        target = 0.f;
        animation = false;
        resizing_workspace = false;
    }
};

WorkspaceWindow::WorkspaceWindow(const char* name): name_(name), impl_(nullptr)
{
    WorkspaceWindow::windows_.push_back(this);
}

void WorkspaceWindow::toggleClearRestoreWorkspace()
{
    // stop animations that are ongoing
    for(auto it = windows_.cbegin(); it != windows_.cend(); ++it) {
        if ( (*it)->impl_ && (*it)->impl_->animation )
            (*it)->impl_->animation = false;
    }

    // toggle
    if (clear_workspace_enabled)
        restoreWorkspace();
    else
        clearWorkspace();
}

void WorkspaceWindow::restoreWorkspace(bool instantaneous)
{
    if (clear_workspace_enabled) {
        const ImVec2 display_size = ImGui::GetIO().DisplaySize;
        for(auto it = windows_.cbegin(); it != windows_.cend(); ++it) {
            ImGuiProperties *impl = (*it)->impl_;
            if (impl && impl->ptr)
            {
                float margin = (impl->ptr->MenuBarHeight() + impl->ptr->TitleBarHeight()) * 3.f;
                impl->user_pos.x = CLAMP(impl->user_pos.x, -impl->ptr->SizeFull.x +margin, display_size.x -margin);
                impl->user_pos.y = CLAMP(impl->user_pos.y, -impl->ptr->SizeFull.y +margin, display_size.y -margin);

                if (instantaneous) {
                    impl->animation = false;
                    ImGui::SetWindowPos(impl->ptr, impl->user_pos);
                }
                else {
                    // remember outside position before move
                    impl->outside_pos = impl->ptr->Pos;

                    // initialize animation
                    impl->progress = 1.f;
                    impl->target = 0.f;
                    impl->animation = true;
                }
            }
        }
    }
    clear_workspace_enabled = false;
}

void WorkspaceWindow::clearWorkspace()
{
    if (!clear_workspace_enabled) {
        const ImVec2 display_size = ImGui::GetIO().DisplaySize;
        for(auto it = windows_.cbegin(); it != windows_.cend(); ++it) {
            ImGuiProperties *impl = (*it)->impl_;
            if (impl && impl->ptr)
            {
                // remember user position before move
                impl->user_pos = impl->ptr->Pos;

                // init before decision
                impl->outside_pos = impl->ptr->Pos;

                // distance to right side, top & bottom
                float right = display_size.x - (impl->ptr->Pos.x + impl->ptr->SizeFull.x * 0.7);
                float top = impl->ptr->Pos.y;
                float bottom = display_size.y - (impl->ptr->Pos.y + impl->ptr->SizeFull.y);

                // move to closest border, with a margin to keep visible
                float margin = (impl->ptr->MenuBarHeight() + impl->ptr->TitleBarHeight()) * 1.5f;
                if (top < bottom && top < right)
                    impl->outside_pos.y = margin - impl->ptr->SizeFull.y;
                else if (right < top && right < bottom)
                    impl->outside_pos.x = display_size.x - margin;
                else
                    impl->outside_pos.y = display_size.y - margin;

                // initialize animation
                impl->progress = 0.f;
                impl->target = 1.f;
                impl->animation = true;
            }
        }
    }
    clear_workspace_enabled = true;
}

void WorkspaceWindow::notifyWorkspaceSizeChanged(int prev_width, int prev_height, int curr_width, int curr_height)
{
    // restore windows pos before rescale
    restoreWorkspace(true);

    for(auto it = windows_.cbegin(); it != windows_.cend(); ++it) {
        ImGuiProperties *impl = (*it)->impl_;
        if ( impl && impl->ptr)
        {
            ImVec2 distance_to_corner = ImVec2(prev_width, prev_height) - impl->ptr->Pos - impl->ptr->SizeFull;

            impl->resized_pos = impl->ptr->Pos;

            if ( ABS(distance_to_corner.x) < 100.f ) {
                impl->resized_pos.x += curr_width - prev_width;
                impl->resizing_workspace = true;
            }

            if ( ABS(distance_to_corner.y) < 100.f ) {
                impl->resized_pos.y += curr_height -prev_height;
                impl->resizing_workspace = true;
            }
        }
    }
}

void WorkspaceWindow::Update()
{
    // get ImGui pointer to window (available only after first run)
    if (!impl_) {
        ImGuiWindow *w = ImGui::FindWindowByName(name_);
        if (w != NULL) {
            impl_ = new ImGuiProperties;
            impl_->ptr = w;
            impl_->user_pos = w->Pos;
        }
    }
    else
    {
        if ( Visible() ) {
            // perform animation for clear/restore workspace
            if (impl_->animation) {
                // increment animation progress by small steps
                impl_->progress += SIGN(impl_->target -impl_->progress) * 0.1f;
                // finished animation : apply full target position
                if (ABS_DIFF(impl_->target, impl_->progress) < 0.05f) {
                    impl_->animation = false;
                    ImVec2 pos = impl_->user_pos * (1.f -impl_->target) + impl_->outside_pos * impl_->target;
                    ImGui::SetWindowPos(impl_->ptr, pos);
                }
                // move window by interpolation between user position and outside target position
                else {
                    ImVec2 pos = impl_->user_pos * (1.f -impl_->progress) + impl_->outside_pos * impl_->progress;
                    ImGui::SetWindowPos(impl_->ptr, pos);
                }
            }
            // Restore if clic on overlay
            if (clear_workspace_enabled)
            {
                // draw another window on top of the WorkspaceWindow, at exact same position and size
                ImGuiWindow* window = impl_->ptr;
                ImGui::SetNextWindowPos(window->Pos, ImGuiCond_Always);
                ImGui::SetNextWindowSize(window->Size, ImGuiCond_Always);
                char nameoverlay[64];
                sprintf(nameoverlay, "%sOverlay", name_);
                if (ImGui::Begin(nameoverlay, NULL, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings ))
                {
                    // exit workspace clear mode if user clics on the window
                    ImGui::InvisibleButton("##dummy", window->Size);
                    if (ImGui::IsItemClicked())
                        WorkspaceWindow::restoreWorkspace();
                    ImGui::End();
                }
            }
        }
        // move windows because workspace was resized
        if (impl_->resizing_workspace) {
            // how far from the target ?
            ImVec2 delta = impl_->resized_pos - impl_->ptr->Pos;
            // got close enough to stop workspace resize
            if (ABS(delta.x) < 2.f && ABS(delta.y) < 2.f)
                impl_->resizing_workspace = false;
            // dichotomic reach of target position
            ImVec2 pos = impl_->ptr->Pos + delta * 0.5f;
            ImGui::SetWindowPos(impl_->ptr, pos);
        }
    }
}


///
/// SOURCE CONTROLLER
///
SourceController::SourceController() : WorkspaceWindow("SourceController"),
    min_width_(0.f), h_space_(0.f), v_space_(0.f), scrollbar_(0.f),
    timeline_height_(0.f),  mediaplayer_height_(0.f), buttons_width_(0.f), buttons_height_(0.f),
    play_toggle_request_(false), replay_request_(false), pending_(false),
    active_label_(LABEL_AUTO_MEDIA_PLAYER), active_selection_(-1),
    selection_context_menu_(false), selection_mediaplayer_(nullptr), selection_target_slower_(0), selection_target_faster_(0),
    mediaplayer_active_(nullptr), mediaplayer_edit_fading_(false), mediaplayer_mode_(false), mediaplayer_slider_pressed_(false), mediaplayer_timeline_zoom_(1.f),
    magnifying_glass(false)
{
    info_.setExtendedStringMode();

    captureFolderDialog = new DialogToolkit::OpenFolderDialog("Capture frame Location");
}


void SourceController::resetActiveSelection()
{
    info_.reset();
    active_selection_ = -1;
    active_label_ = LABEL_AUTO_MEDIA_PLAYER;
    play_toggle_request_ = false;
    replay_request_ = false;
    capture_request_ = false;
}

void SourceController::setVisible(bool on)
{
    magnifying_glass = false;

    // restore workspace to show the window
    if (WorkspaceWindow::clear_workspace_enabled) {
        WorkspaceWindow::restoreWorkspace(on);
        // do not change status if ask to hide (consider user asked to toggle because the window was not visible)
        if (!on)  return;
    }

    if (on && Settings::application.widget.media_player_view != Settings::application.current_view)
        Settings::application.widget.media_player_view = -1;

    // if no selection in the player and in the source selection, show all sources
    if (on && selection_.empty() && Mixer::selection().empty() )
        selection_ = valid_only( Mixer::manager().session()->getDepthSortedList() );

    Settings::application.widget.media_player = on;
}

bool SourceController::Visible() const
{
    return ( Settings::application.widget.media_player
             && (Settings::application.widget.media_player_view < 0 || Settings::application.widget.media_player_view == Settings::application.current_view )
             );
}

void SourceController::Update()
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

void SourceController::Render()
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
                sprintf( name_path[1], "%s", ICON_FA_HOME " Home");
                sprintf( name_path[2], "%s", ICON_FA_FOLDER " File location");
                sprintf( name_path[3], "%s", ICON_FA_FOLDER_PLUS " Select");
            }
            if (Settings::application.source.capture_path.empty())
                Settings::application.source.capture_path = SystemToolkit::home_path();
            sprintf( name_path[0], "%s", Settings::application.source.capture_path.c_str());
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
            if (ImGuiToolkit::IconButton( ICON_FA_EXTERNAL_LINK_ALT,  Settings::application.source.capture_path.c_str()))
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
        if (ImGui::BeginMenu(ICON_FA_FILM " Video", mediaplayer_active_) )
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

void SourceController::RenderSelection(size_t i)
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
            if (ms != nullptr && !ms->mediaplayer()->isImage())
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
                if (ms == nullptr || ms->mediaplayer()->isImage()) {
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
                if (action > 1)
                    (*source)->play( ! (*source)->playing() );
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
            ImGui::Columns( CLAMP( int(ceil(w / 250.f)), 1, (int)selection_sources.size()), "##selectioncolumns", false);
            for (auto source = selection_sources.begin(); source != selection_sources.end(); ++source) {
                ///
                /// Source Image Button
                ///
                const ImVec2 framesize(1.5f * timeline_height_ * (*source)->frame()->aspectRatio(), 1.5f * timeline_height_);
                int action = SourceButton(*source, framesize);
                if (action > 1)
                    (*source)->play( ! (*source)->playing() );
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
    if (ImGui::Button(ICON_FA_MINUS_CIRCLE)) {
        resetActiveSelection();
        Mixer::manager().session()->deleteBatch(i);
    }
    if (ImGui::IsItemHovered())
        ImGuiToolkit::ToolTip("Delete batch");

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

void DrawSource(Source *s, ImVec2 framesize, ImVec2 top_image, bool withslider = false, bool withinspector = false)
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
    // 100% opacity for the image (ensure true colors)
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.f);
    DrawSource(s, framesize, top_image, true, with_inspector);
    ImGui::PopStyleVar();

    return ImRect( top_image, top_image + framesize);
}


int SourceController::SourceButton(Source *s, ImVec2 framesize)
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

void SourceController::RenderSelectedSources()
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
                if (action > 1)
                    (*source)->play( ! (*source)->playing() );
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
            width = buttons_width_;
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

void SourceController::RenderSingleSource(Source *s)
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
    }
}

void SourceController::RenderMediaPlayer(MediaSource *ms)
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
        static std::vector< std::pair<int, int> > icons_loop = { {0,15}, {1,15}, {19,14} };
        static std::vector< std::string > tooltips_loop = { "Stop at end", "Loop to start", "Bounce (reverse speed)" };
        current_loop = (int) mediaplayer_active_->loop();
        if ( ImGuiToolkit::ButtonIconMultistate(icons_loop, &current_loop, tooltips_loop) )
            mediaplayer_active_->setLoop( (MediaPlayer::LoopMode) current_loop );

        // speed slider (if enough space)
        if ( rendersize.x > min_width_ * 1.2f ) {
            ImGui::SameLine(0, MAX(h_space_ * 2.f, rendersize.x - min_width_ * 1.4f) );
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - buttons_height_ );
            // speed slider
            float speed = static_cast<float>(mediaplayer_active_->playSpeed());
            if (ImGui::DragFloat( "##Speed", &speed, 0.01f, -10.f, 10.f, UNICODE_MULTIPLY " %.1f", 2.f))
                mediaplayer_active_->setPlaySpeed( static_cast<double>(speed) );
            // store action on mouse release
            if (ImGui::IsItemDeactivatedAfterEdit()){
                oss << ": Speed x" << std::setprecision(3) << speed;
                Action::manager().store(oss.str());
            }
            if (ImGui::IsItemHovered())
                ImGuiToolkit::ToolTip("Play speed");
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
/// OUTPUT PREVIEW
///

OutputPreview::OutputPreview() : WorkspaceWindow("OutputPreview"),
    video_recorder_(nullptr), video_broadcaster_(nullptr), loopback_broadcaster_(nullptr),
    magnifying_glass(false)
{

    recordFolderDialog = new DialogToolkit::OpenFolderDialog("Recording Location");
}

void OutputPreview::setVisible(bool on)
{
    magnifying_glass = false;

    // restore workspace to show the window
    if (WorkspaceWindow::clear_workspace_enabled) {
        WorkspaceWindow::restoreWorkspace(on);
        // do not change status if ask to hide (consider user asked to toggle because the window was not visible)
        if (!on)  return;
    }

    if (on && Settings::application.widget.preview_view != Settings::application.current_view)
        Settings::application.widget.preview_view = -1;

    Settings::application.widget.preview = on;
}

bool OutputPreview::Visible() const
{
    return ( Settings::application.widget.preview
             && (Settings::application.widget.preview_view < 0 || Settings::application.widget.preview_view == Settings::application.current_view )
             );
}

void OutputPreview::Update()
{
    WorkspaceWindow::Update();

    // management of video_recorders
    if ( !_video_recorders.empty() ) {
        // check that file dialog thread finished
        if (_video_recorders.back().wait_for(std::chrono::milliseconds(4)) == std::future_status::ready ) {
            video_recorder_ = _video_recorders.back().get();
            FrameGrabbing::manager().add(video_recorder_);
            _video_recorders.pop_back();
        }
    }
    // verify the video recorder is valid (change to nullptr if invalid)
    FrameGrabbing::manager().verify( (FrameGrabber**) &video_recorder_);
    if (video_recorder_ // if there is an ongoing recorder
        && Settings::application.record.timeout < RECORD_MAX_TIMEOUT  // and if the timeout is valid
        && video_recorder_->duration() > Settings::application.record.timeout ) // and the timeout is reached
    {
        video_recorder_->stop();
    }

    // verify the frame grabbers are valid (change to nullptr if invalid)
    FrameGrabbing::manager().verify( (FrameGrabber**) &video_broadcaster_);
    FrameGrabbing::manager().verify( (FrameGrabber**) &shm_broadcaster_);
    FrameGrabbing::manager().verify( (FrameGrabber**) &loopback_broadcaster_);

}

VideoRecorder *delayTrigger(VideoRecorder *g, std::chrono::milliseconds delay) {
    std::this_thread::sleep_for (delay);
    return g;
}

void OutputPreview::ToggleRecord(bool save_and_continue)
{
    if (video_recorder_) {
        // prepare for next time user open new source panel to show the recording
        if (Settings::application.recentRecordings.load_at_start)
            UserInterface::manager().navigator.setNewMedia(Navigator::MEDIA_RECORDING);
        // 'save & continue'
        if ( save_and_continue) {
            VideoRecorder *rec = new VideoRecorder(SystemToolkit::base_filename( Mixer::manager().session()->filename()));
            FrameGrabbing::manager().chain(video_recorder_, rec);
            video_recorder_ = rec;
        }
        // normal case: Ctrl+R stop recording
        else
            // stop recording
            video_recorder_->stop();
    }
    else {
        _video_recorders.emplace_back( std::async(std::launch::async, delayTrigger, new VideoRecorder(SystemToolkit::base_filename( Mixer::manager().session()->filename())),
                                                  std::chrono::seconds(Settings::application.record.delay)) );
    }
}

void OutputPreview::ToggleVideoBroadcast()
{
    if (video_broadcaster_) {
        video_broadcaster_->stop();
    }
    else {
        if (Settings::application.broadcast_port<1000)
            Settings::application.broadcast_port = BROADCAST_DEFAULT_PORT;
        video_broadcaster_ = new VideoBroadcast(Settings::application.broadcast_port);
        FrameGrabbing::manager().add(video_broadcaster_);
    }
}

void OutputPreview::ToggleSharedMemory()
{
    if (shm_broadcaster_) {
        shm_broadcaster_->stop();
    }
    else {
        if (Settings::application.shm_socket_path.empty())
            Settings::application.shm_socket_path = SHMDATA_DEFAULT_PATH + std::to_string(Settings::application.instance_id);
        shm_broadcaster_ = new ShmdataBroadcast(Settings::application.shm_socket_path);
        FrameGrabbing::manager().add(shm_broadcaster_);
    }
}

bool OutputPreview::ToggleLoopbackCamera()
{
    bool need_initialization = false;
    if (loopback_broadcaster_) {
        loopback_broadcaster_->stop();
    }
    else {
        if (Settings::application.loopback_camera < 1)
            Settings::application.loopback_camera = LOOPBACK_DEFAULT_DEVICE;
        Settings::application.loopback_camera += Settings::application.instance_id;

        try {
            loopback_broadcaster_ = new Loopback(Settings::application.loopback_camera);
            FrameGrabbing::manager().add(loopback_broadcaster_);
        }
        catch (const std::runtime_error &e) {
            need_initialization = true;
            Log::Info("%s", e.what());
        }
    }
    return need_initialization;
}


void OutputPreview::Render()
{
    const ImGuiContext& g = *GImGui;
    bool openInitializeSystemLoopback = false;

    FrameBuffer *output = Mixer::manager().session()->frame();
    if (output)
    {
        // constraint aspect ratio resizing
        float ar = output->aspectRatio();
        ImGui::SetNextWindowSizeConstraints(ImVec2(300, 200), ImVec2(FLT_MAX, FLT_MAX), CustomConstraints::AspectRatio, &ar);
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
                // Output window menu
                if ( ImGui::MenuItem( ICON_FA_WINDOW_RESTORE "  Show window") )
                    Rendering::manager().outputWindow().show();

                ImGui::MenuItem( MENU_OUTPUTDISABLE, SHORTCUT_OUTPUTDISABLE, &Settings::application.render.disabled);

                // output manager menu
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
                if ( ImGui::MenuItem( MENU_CAPTUREFRAME, SHORTCUT_CAPTUREFRAME) ) {
                    FrameGrabbing::manager().add(new PNGRecorder(SystemToolkit::base_filename( Mixer::manager().session()->filename())));
                }
                ImGui::PopStyleColor(1);

                // temporary disabled
                if (!_video_recorders.empty()) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_RECORD, 0.8f));
                    ImGui::MenuItem( MENU_RECORD, SHORTCUT_RECORD, false, false);
                    ImGui::MenuItem( MENU_RECORDCONT, SHORTCUT_RECORDCONT, false, false);
                    ImGui::PopStyleColor(1);
                }
                // Stop recording menu (recorder already exists)
                else if (video_recorder_) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_RECORD, 0.8f));
                    if ( ImGui::MenuItem( ICON_FA_SQUARE "  Stop Record", SHORTCUT_RECORD) ) {
                        // prepare for next time user open new source panel to show the recording
                        if (Settings::application.recentRecordings.load_at_start)
                            UserInterface::manager().navigator.setNewMedia(Navigator::MEDIA_RECORDING);
                        // stop recorder
                        video_recorder_->stop();
                    }
                    // offer the 'save & continue' recording
                    if ( ImGui::MenuItem( MENU_RECORDCONT, SHORTCUT_RECORDCONT) ) {
                        // prepare for next time user open new source panel to show the recording
                        if (Settings::application.recentRecordings.load_at_start)
                            UserInterface::manager().navigator.setNewMedia(Navigator::MEDIA_RECORDING);
                        // create a new recorder chainned to the current one
                        VideoRecorder *rec = new VideoRecorder(SystemToolkit::base_filename( Mixer::manager().session()->filename()));
                        FrameGrabbing::manager().chain(video_recorder_, rec);
                        // swap recorder
                        video_recorder_ = rec;
                    }
                    ImGui::PopStyleColor(1);
                }
                // start recording
                else {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_RECORD, 0.9f));
                    if ( ImGui::MenuItem( MENU_RECORD, SHORTCUT_RECORD) ) {
                        _video_recorders.emplace_back( std::async(std::launch::async, delayTrigger,
                                                                  new VideoRecorder(SystemToolkit::base_filename( Mixer::manager().session()->filename())),
                                                                  std::chrono::seconds(Settings::application.record.delay)) );
                    }
                    ImGui::MenuItem( MENU_RECORDCONT, SHORTCUT_RECORDCONT, false, false);
                    ImGui::PopStyleColor(1);
                }
                // Options menu
                ImGui::Separator();
                ImGui::MenuItem("Settings", nullptr, false, false);
                // offer to open config panel from here for more options
                ImGui::SameLine(ImGui::GetContentRegionAvailWidth() + 1.2f * IMGUI_RIGHT_ALIGN);
                if (ImGuiToolkit::IconButton(13, 5, "Advanced settings"))
                    UserInterface::manager().navigator.showConfig();

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
                    recordFolderDialog->open();
                else if (selected_path > 1)
                    Settings::application.record.path = SystemToolkit::path_filename( Mixer::manager().session()->filename() );
                else if (selected_path > 0)
                    Settings::application.record.path = SystemToolkit::home_path();

                // offer to open folder location
                ImVec2 draw_pos = ImGui::GetCursorPos();
                ImGui::SetCursorPos(draw_pos + ImVec2(ImGui::GetContentRegionAvailWidth() - 1.2 * ImGui::GetTextLineHeightWithSpacing(), -ImGui::GetFrameHeight()) );
                if (ImGuiToolkit::IconButton( ICON_FA_EXTERNAL_LINK_ALT,  Settings::application.record.path.c_str()))
                    SystemToolkit::open(Settings::application.record.path);
                ImGui::SetCursorPos(draw_pos);

                // Naming menu selection
                static const char* naming_style[2] = { ICON_FA_SORT_NUMERIC_DOWN "  Sequential", ICON_FA_CALENDAR "  Date prefix" };
                ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                ImGui::Combo("Filename", &Settings::application.record.naming_mode, naming_style, IM_ARRAYSIZE(naming_style));


                ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                ImGuiToolkit::SliderTiming ("Duration", &Settings::application.record.timeout, 1000, RECORD_MAX_TIMEOUT, 1000, "Until stopped");

                ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                ImGui::SliderInt("Trigger", &Settings::application.record.delay, 0, 5,
                                 Settings::application.record.delay < 1 ? "Immediate" : "After %d s");

                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu(ICON_FA_WIFI  " Stream"))
            {
                // Stream sharing menu
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_STREAM, 0.9f));
                if ( ImGui::MenuItem( ICON_FA_SHARE_ALT_SQUARE "   Peer-to-peer sharing", NULL, &Settings::application.accept_connections) ) {
                    Streaming::manager().enable(Settings::application.accept_connections);
                }
                ImGui::PopStyleColor(1);

                // list active streams:
                std::vector<std::string> ls = Streaming::manager().listStreams();

                // Broadcasting menu
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_BROADCAST, 0.9f));
                if (VideoBroadcast::available()) {
                    if ( ImGui::MenuItem( ICON_FA_GLOBE "   SRT Broadcast", NULL, videoBroadcastEnabled()) )
                        ToggleVideoBroadcast();
                }

                // Shared Memory menu
                if (ShmdataBroadcast::available()) {
                    if ( ImGui::MenuItem( ICON_FA_SHARE_ALT "   Shared Memory", NULL, sharedMemoryEnabled()) )
                        ToggleSharedMemory();
                }

                // Loopback camera menu
                if (Loopback::available()) {
                    if ( ImGui::MenuItem( ICON_FA_VIDEO "  Loopback Camera", NULL, loopbackCameraEnabled()) )
                        openInitializeSystemLoopback = ToggleLoopbackCamera();
                }

                ImGui::PopStyleColor(1);

                // Display list of active stream
                if (ls.size()>0 || videoBroadcastEnabled() || sharedMemoryEnabled() || loopbackCameraEnabled()) {
                    ImGui::Separator();
                    ImGui::MenuItem("Active streams", nullptr, false, false);

                    // First the list of peer 2 peer
                    for (auto it = ls.begin(); it != ls.end(); ++it)
                        ImGui::Text(" %s ", (*it).c_str() );

                    // SRT broadcast description
                    if (videoBroadcastEnabled()) {
                        ImGui::Text(" %s        ", video_broadcaster_->info().c_str());
                        // copy text icon to give user the srt link to connect to
                        ImVec2 draw_pos = ImGui::GetCursorPos();
                        ImGui::SetCursorPos(draw_pos + ImVec2(ImGui::GetContentRegionAvailWidth() - 1.2 * ImGui::GetTextLineHeightWithSpacing(), -0.8 * ImGui::GetFrameHeight()) );
                        char msg[256];
                        ImFormatString(msg, IM_ARRAYSIZE(msg), "srt//%s:%d", NetworkToolkit::host_ips()[1].c_str(), Settings::application.broadcast_port );
                        if (ImGuiToolkit::IconButton( ICON_FA_COPY, msg))
                            ImGui::SetClipboardText(msg);
                        ImGui::SetCursorPos(draw_pos);
                    }

                    // Shared memory broadcast description
                    if (sharedMemoryEnabled()) {
                        ImGui::Text(" %s        ", shm_broadcaster_->info().c_str());
                        // copy text icon to give user the socket path to connect to
                        ImVec2 draw_pos = ImGui::GetCursorPos();
                        ImGui::SetCursorPos(draw_pos + ImVec2(ImGui::GetContentRegionAvailWidth() - 1.2 * ImGui::GetTextLineHeightWithSpacing(), -0.8 * ImGui::GetFrameHeight()) );
                        if (ImGuiToolkit::IconButton( ICON_FA_COPY, shm_broadcaster_->socket_path().c_str()))
                            ImGui::SetClipboardText(shm_broadcaster_->socket_path().c_str());
                        ImGui::SetCursorPos(draw_pos);
                    }

                    // Loopback camera description
                    if (loopbackCameraEnabled()) {
                        ImGui::Text(" %s        ", loopback_broadcaster_->info().c_str());
                        // copy text icon to give user the device path to connect to
                        ImVec2 draw_pos = ImGui::GetCursorPos();
                        ImGui::SetCursorPos(draw_pos + ImVec2(ImGui::GetContentRegionAvailWidth() - 1.2 * ImGui::GetTextLineHeightWithSpacing(), -0.8 * ImGui::GetFrameHeight()) );
                        if (ImGuiToolkit::IconButton( ICON_FA_COPY, loopback_broadcaster_->device_name().c_str()))
                            ImGui::SetClipboardText(loopback_broadcaster_->device_name().c_str());
                        ImGui::SetCursorPos(draw_pos);
                    }
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
        // mouse over the image
        if ( ImGui::IsItemHovered()  ) {
            // raise window on double clic
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) )
                Rendering::manager().outputWindow().show();
            // show magnifying glass if active
            if (magnifying_glass)
                DrawInspector(output->texture(), imagesize, imagesize, draw_pos);
        }

        // disable magnifying glass if window is deactivated
        if ( g.NavWindow != g.CurrentWindow )
            magnifying_glass = false;

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
        if (video_recorder_)
        {
            ImGui::SetCursorScreenPos(ImVec2(draw_pos.x + r, draw_pos.y + r));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_RECORD, 0.8f));
            ImGui::Text(ICON_FA_CIRCLE " %s", video_recorder_->info().c_str() );
            ImGui::PopStyleColor(1);
        }
        else if (!_video_recorders.empty())
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
        if (video_broadcaster_)
        {
            ImGui::SetCursorScreenPos(ImVec2(draw_pos.x + imagesize.x - 2.5f * r, draw_pos.y + vertical));
            if (video_broadcaster_->busy())
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_BROADCAST, 0.8f));
            else
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_BROADCAST, 0.4f));
            ImGui::Text(ICON_FA_GLOBE);
            ImGui::PopStyleColor(1);
            vertical += 2.f * r;
        }
        // shmdata indicator
        if (shm_broadcaster_)
        {
            ImGui::SetCursorScreenPos(ImVec2(draw_pos.x + imagesize.x - 2.5f * r, draw_pos.y + vertical));
            if (shm_broadcaster_->busy())
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_BROADCAST, 0.8f));
            else
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_BROADCAST, 0.4f));
            ImGui::Text(ICON_FA_SHARE_ALT);
            ImGui::PopStyleColor(1);
            vertical += 2.f * r;
        }
        // loopback camera indicator
        if (loopback_broadcaster_)
        {
            ImGui::SetCursorScreenPos(ImVec2(draw_pos.x + imagesize.x - 2.5f * r, draw_pos.y + vertical));
            if (loopback_broadcaster_->busy())
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
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(COLOR_FRAME, 0.8f));
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
            ImGui::Text(" " ICON_FA_LAPTOP "  %d x %d px, %.d fps", output->width(), output->height(), int(Mixer::manager().fps()) );
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
        sprintf(dummy_str, "sudo apt install v4l2loopback-dkms");
        ImGui::NewLine();
        ImGui::Text("Install v4l2loopack (only once, and reboot):");
        ImGui::SetNextItemWidth(600-40);
        ImGui::InputText("##cmd1", dummy_str, IM_ARRAYSIZE(dummy_str), ImGuiInputTextFlags_ReadOnly);
        ImGui::SameLine();
        ImGui::PushID(358794);
        if ( ImGuiToolkit::IconButton(ICON_FA_COPY, "Copy to clipboard") )
            ImGui::SetClipboardText(dummy_str);
        ImGui::PopID();

        sprintf(dummy_str, "sudo modprobe v4l2loopback exclusive_caps=1 video_nr=%d"
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
        if (ImGui::Button("Ok, I'll do this in a terminal and try again later.", ImVec2(w, 0)) ) {
            ImGui::CloseCurrentPopup();
        }

#endif
        ImGui::EndPopup();
    }
}

///
/// TIMER & METRONOME
///

TimerMetronome::TimerMetronome() : WorkspaceWindow("Timer")
{
    // timer modes : 0 Metronome, 1 Stopwatch
    timer_menu = { "Metronome", "Stopwatch" };
    // clock times
    start_time_      = gst_util_get_timestamp ();
    start_time_hand_ = gst_util_get_timestamp ();
    duration_hand_   = Settings::application.timer.stopwatch_duration *  GST_SECOND;
}


void TimerMetronome::setVisible(bool on)
{
    // restore workspace to show the window
    if (WorkspaceWindow::clear_workspace_enabled) {
        WorkspaceWindow::restoreWorkspace(on);
        // do not change status if ask to hide (consider user asked to toggle because the window was not visible)
        if (!on)  return;
    }

    if (on && Settings::application.widget.timer_view != Settings::application.current_view)
        Settings::application.widget.timer_view = -1;

    Settings::application.widget.timer = on;
}

bool TimerMetronome::Visible() const
{
    return ( Settings::application.widget.timer
             && (Settings::application.widget.timer_view < 0 || Settings::application.widget.timer_view == Settings::application.current_view )
             );
}

void TimerMetronome::Render()
{
    // constraint square resizing
    static ImVec2 timer_window_size_min = ImVec2(11.f * ImGui::GetTextLineHeight(), 11.f * ImGui::GetTextLineHeight());
    ImGui::SetNextWindowSizeConstraints(timer_window_size_min, timer_window_size_min * 1.5f, CustomConstraints::Square);
    ImGui::SetNextWindowPos(ImVec2(600, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(timer_window_size_min, ImGuiCond_FirstUseEver);

    if ( !ImGui::Begin(name_, &Settings::application.widget.timer,
                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar |  ImGuiWindowFlags_NoCollapse ))
    {
        ImGui::End();
        return;
    }

    // menu (no title bar)
    if (ImGui::BeginMenuBar())
    {
        // Close and widget menu
        if (ImGuiToolkit::IconButton(4,16))
            Settings::application.widget.timer = false;
        if (ImGui::BeginMenu(IMGUI_TITLE_TIMER))
        {
            // Enable/Disable Ableton Link
            if ( ImGui::MenuItem( ICON_FA_USER_CLOCK " Ableton Link", nullptr, &Settings::application.timer.link_enabled) ) {
                Metronome::manager().setEnabled(Settings::application.timer.link_enabled);
            }

            // output manager menu
            ImGui::Separator();
            bool pinned = Settings::application.widget.timer_view == Settings::application.current_view;
            std::string menutext = std::string( ICON_FA_MAP_PIN "    Stick to ") + Settings::application.views[Settings::application.current_view].name + " view";
            if ( ImGui::MenuItem( menutext.c_str(), nullptr, &pinned) ){
                if (pinned)
                    Settings::application.widget.timer_view = Settings::application.current_view;
                else
                    Settings::application.widget.timer_view = -1;
            }
            if ( ImGui::MenuItem( MENU_CLOSE, SHORTCUT_TIMER) )
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

    // current windowdraw parameters
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* draw_list = window->DrawList;
    // positions and size of GUI elements
    const float margin = window->MenuBarHeight();
    const float h = 0.4f * ImGui::GetFrameHeight();
    const ImVec2 circle_top_left = window->Pos + ImVec2(margin + h, margin + h);
    const ImVec2 circle_top_right = window->Pos + ImVec2(window->Size.y - margin - h, margin + h);
    const ImVec2 circle_botom_right = window->Pos + ImVec2(window->Size.y - margin - h, window->Size.x - margin - h);
    const ImVec2 circle_center = window->Pos + (window->Size + ImVec2(margin, margin) )/ 2.f;
    const float circle_radius = (window->Size.y - 2.f * margin) / 2.f;

    // color palette
    static ImU32 colorbg = ImGui::GetColorU32(ImGuiCol_FrameBgActive, 0.6f);
    static ImU32 colorfg = ImGui::GetColorU32(ImGuiCol_FrameBg, 2.5f);
    static ImU32 colorline = ImGui::GetColorU32(ImGuiCol_PlotHistogram);

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
        draw_list->AddCircleFilled(circle_center, circle_radius, colorbg, PLOT_CIRCLE_SEGMENTS);

        // draw quarter
        static const float resolution = PLOT_CIRCLE_SEGMENTS / (2.f * M_PI);
        static ImVec2 buffer[PLOT_CIRCLE_SEGMENTS];
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
        draw_list->AddCircleFilled(circle_center, margin, colorfg, PLOT_CIRCLE_SEGMENTS);
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_MONO);
        char text_buf[24];
        sprintf(text_buf, "%d/%d", (int)(p)+1, (int)(q) );
        ImVec2 label_size = ImGui::CalcTextSize(text_buf, NULL);
        ImGui::SetCursorScreenPos(circle_center - label_size/2);
        ImGui::Text("%s", text_buf);
        ImGui::PopFont();

        // left slider : quantum
        float float_value = ceil(Metronome::manager().quantum());
        ImGui::SetCursorPos(ImVec2(0.5f * margin, 1.5f * margin));
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
        guint64 time_ = gst_util_get_timestamp ();

        // draw ring
        draw_list->AddCircle(circle_center, circle_radius, colorbg, PLOT_CIRCLE_SEGMENTS, 12 );
        draw_list->AddCircleFilled(ImVec2(circle_center.x, circle_center.y - circle_radius), 7, colorfg, PLOT_CIRCLE_SEGMENTS);
        // draw indicator time hand
        double da = -M_PI_2 + ( (double) (time_-start_time_hand_) / (double) duration_hand_) * (2.f * M_PI);
        draw_list->AddCircleFilled(ImVec2(circle_center.x + circle_radius * cos(da), circle_center.y + circle_radius * sin(da)), 7, colorline, PLOT_CIRCLE_SEGMENTS);

        // left slider : countdown
        float float_value = (float) Settings::application.timer.stopwatch_duration;
        ImGui::SetCursorPos(ImVec2(0.5f * margin, 1.5f * margin));
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


///
/// KEYBOARDS
///

InputMappingInterface::InputMappingInterface() : WorkspaceWindow("InputMappingInterface")
{
    input_mode = { ICON_FA_KEYBOARD "  Keyboard",
                   ICON_FA_CALCULATOR "   Numpad" ,
                   ICON_FA_TABLET_ALT "   TouchOSC" ,
                   ICON_FA_GAMEPAD " Gamepad" };
    current_input_for_mode = { INPUT_KEYBOARD_FIRST, INPUT_NUMPAD_FIRST, INPUT_MULTITOUCH_FIRST, INPUT_JOYSTICK_FIRST };
    current_input_ = current_input_for_mode[Settings::application.mapping.mode];
}

void InputMappingInterface::setVisible(bool on)
{
    // restore workspace to show the window
    if (WorkspaceWindow::clear_workspace_enabled) {
        WorkspaceWindow::restoreWorkspace(on);
        // do not change status if ask to hide (consider user asked to toggle because the window was not visible)
        if (!on)  return;
    }

    if (on && Settings::application.widget.inputs_view != Settings::application.current_view)
        Settings::application.widget.inputs_view = -1;

    Settings::application.widget.inputs = on;
}

bool InputMappingInterface::Visible() const
{
    return ( Settings::application.widget.inputs
             && (Settings::application.widget.inputs_view < 0 || Settings::application.widget.inputs_view == Settings::application.current_view )
             );
}

///
/// Draw a combo box listing all sources and all batch of the current session
/// Returns a Target variant : non-assigned by default (std::monostate), or a Source, or a Batch
/// If current element is indicated, it is displayed first
///
Target InputMappingInterface::ComboSelectTarget(const Target &current)
{
    Target selected;
    std::string label = "Select";

    // depending on variant, fill the label of combo
    if (current.index() > 0) {
        if (Source * const* v = std::get_if<Source *>(&current)) {
            label = std::string((*v)->initials()) + " - " + (*v)->name();
        }
        else if ( const size_t* v = std::get_if<size_t>(&current)) {
            label = std::string("Batch #") + std::to_string(*v);
        }
    }

    if (ImGui::BeginCombo("##ComboSelectSource", label.c_str()) )
    {
        Session *ses = Mixer::manager().session();
        for (auto sit = ses->begin(); sit != ses->end(); ++sit) {
            label = std::string((*sit)->initials()) + " - " + (*sit)->name();
            if (ImGui::Selectable( label.c_str() )) {
                selected = *sit;
            }
        }
        for (size_t b = 0; b < Mixer::manager().session()->numBatch(); ++b){
            label = std::string("Batch #") + std::to_string(b);
            if (ImGui::Selectable( label.c_str() )) {
                selected = b;
            }
        }

        ImGui::EndCombo();
    }

    return selected;
}

uint InputMappingInterface::ComboSelectCallback(uint current, bool imageprocessing)
{
    const char* callback_names[23] = { "Select",
                                       ICON_FA_BULLSEYE "  Alpha",
                                       ICON_FA_BULLSEYE "  Loom",
                                       ICON_FA_OBJECT_UNGROUP "  Geometry",
                                       ICON_FA_OBJECT_UNGROUP "  Grab",
                                       ICON_FA_OBJECT_UNGROUP "  Resize",
                                       ICON_FA_OBJECT_UNGROUP "  Turn",
                                       ICON_FA_LAYER_GROUP "  Depth",
                                       ICON_FA_PLAY_CIRCLE "  Play",
                                       ICON_FA_PLAY_CIRCLE "  Speed",
                                       ICON_FA_PLAY_CIRCLE "  Fast forward",
                                       ICON_FA_PLAY_CIRCLE "  Seek",
                                       "  None",
                                       "  None",
                                       "  None",
                                       ICON_FA_PALETTE "  Gamma",
                                       ICON_FA_PALETTE "  Brightness",
                                       ICON_FA_PALETTE "  Contrast",
                                       ICON_FA_PALETTE "  Saturation",
                                       ICON_FA_PALETTE "  Hue",
                                       ICON_FA_PALETTE "  Threshold",
                                       ICON_FA_PALETTE "  Invert",
                                       "  None"
    };

    uint selected = 0;
    if (ImGui::BeginCombo("##ComboSelectCallback", callback_names[current]) ) {
        for (uint i = SourceCallback::CALLBACK_ALPHA; i <= SourceCallback::CALLBACK_SEEK; ++i){
            if ( ImGui::Selectable( callback_names[i]) ) {
                selected = i;
            }
        }
        if (imageprocessing) {
            for (uint i = SourceCallback::CALLBACK_GAMMA; i <= SourceCallback::CALLBACK_INVERT; ++i){
                if ( ImGui::Selectable( callback_names[i]) ) {
                    selected = i;
                }
            }
        }
        ImGui::EndCombo();
    }

    return selected;
}

struct ClosestIndex
{
    int index;
    float val;
    ClosestIndex (float v) { val = v; index = 0; }
    void operator()(float v) { if (v < val) ++index; }
};

void InputMappingInterface::SliderParametersCallback(SourceCallback *callback, const Target &target)
{
    const float right_align = -1.05f * ImGui::GetTextLineHeightWithSpacing();
    static const char *press_tooltip[3] = {"Key Press\nApply value on key press",
                              "Key Down\nApply value on key down,\nrevert on key up",
                              "Repeat\nMaintain key down to repeat and iterate" };
    static std::vector< std::pair<int,int> > speed_icon = { {18,15}, {17,15}, {16,15}, {15,15}, {14,15} };
    static std::vector< std::string > speed_tooltip = { "Fastest\n0 ms", "Fast\n60 ms", "Smooth\n120 ms", "Slow\n240 ms", "Slowest\n500 ms" };
    static std::vector< float > speed_values = { 0.f, 60.f, 120.f, 240.f, 500.f };

    switch ( callback->type() ) {
    case SourceCallback::CALLBACK_ALPHA:
    {
        SetAlpha *edited = static_cast<SetAlpha*>(callback);

        bool bd = edited->bidirectional();
        if ( ImGuiToolkit::IconToggle(2, 13, 3, 13, &bd, press_tooltip ) )
            edited->setBidirectional(bd);

        ClosestIndex d = std::for_each(speed_values.begin(), speed_values.end(), ClosestIndex(edited->duration()));
        int speed_index = d.index;
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGuiToolkit::IconMultistate(speed_icon, &speed_index, speed_tooltip ))
            edited->setDuration(speed_values[speed_index]);

        float val = edited->value();
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        ImGui::SetNextItemWidth(right_align);
        if (ImGui::SliderFloat("##CALLBACK_ALPHA", &val, -1.f, 1.f, "%.2f"))
            edited->setValue(val);

        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        ImGuiToolkit::Indication("Alpha value to set if the source is\nvisible (1.0), transparent (0.0),\nor innactive (-1.0)", 18, 12);

    }
        break;
    case SourceCallback::CALLBACK_LOOM:
    {
        ImGuiToolkit::Indication(press_tooltip[2], 18, 5);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        Loom *edited = static_cast<Loom*>(callback);
        float val = edited->value();
        ImGui::SetNextItemWidth(right_align);
        if (ImGui::SliderFloat("##CALLBACK_LOOM", &val, -1.f, 1.f, "%.2f", 2.f))
            edited->setValue(val);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        ImGuiToolkit::Indication("Increment alpha to make the source more visible (>0) or more transparent (<0)", 19, 12);
    }
        break;
    case SourceCallback::CALLBACK_GEOMETRY:
    {
        SetGeometry *edited = static_cast<SetGeometry*>(callback);

        bool bd = edited->bidirectional();
        if ( ImGuiToolkit::IconToggle(2, 13, 3, 13, &bd, press_tooltip ) )
            edited->setBidirectional(bd);

        ClosestIndex d = std::for_each(speed_values.begin(), speed_values.end(), ClosestIndex(edited->duration()));
        int speed_index = d.index;
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGuiToolkit::IconMultistate(speed_icon, &speed_index, speed_tooltip ))
            edited->setDuration(speed_values[speed_index]);

        if (target.index() > 0) {
            // 1. Case of variant as Source pointer
            if (Source * const* v = std::get_if<Source *>(&target)) {
                // Button to capture the source current geometry
                ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
                if (ImGui::Button("Capture", ImVec2(right_align, 0))) {
                    edited->setTarget( (*v)->group(View::GEOMETRY) );
                }
            }
            // 2. Case of variant as index of batch
            else if ( const size_t* v = std::get_if<size_t>(&target)) {

                std::vector<SourceIdList> _batch = Mixer::manager().session()->getAllBatch();

                std::string label = "Capture";
                // Combo box to set which source to capture
                if (ImGui::BeginCombo("##ComboSelectGeometryCapture", label.c_str()) )
                {
                    if ( *v < _batch.size() )
                    {
                        for (auto sid = _batch[*v].begin(); sid != _batch[*v].end(); ++sid){
                            SourceList::iterator sit = Mixer::manager().session()->find(*sid);
                            if ( sit != Mixer::manager().session()->end() ) {

                                label = std::string((*sit)->initials()) + " - " + (*sit)->name();
                                // C to capture the source current geometry
                                if (ImGui::Selectable( label.c_str() )) {
                                    edited->setTarget( (*sit)->group(View::GEOMETRY) );
                                }
                            }
                        }
                    }

                    ImGui::EndCombo();
                }
            }

            ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
            ImGuiToolkit::Indication("Capture source geometry to restore it later (position, scale and rotation).", 1, 16);
        }
        else {

            ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
            ImGui::TextDisabled("Invalid");
        }
    }
        break;

    case SourceCallback::CALLBACK_GRAB:
    {
        ImGuiToolkit::Indication(press_tooltip[2], 18, 5);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        Grab *edited = static_cast<Grab*>(callback);
        float val[2] = {edited->value().x, edited->value().y};
        ImGui::SetNextItemWidth(right_align);
        if (ImGui::SliderFloat2("##CALLBACK_GRAB", val, -2.f, 2.f, "%.2f"))
            edited->setValue( glm::vec2(val[0], val[1]));
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        ImGuiToolkit::Indication("Increment vector (x,y) to move the source horizontally and vertically.", 6, 15);
    }
        break;

    case SourceCallback::CALLBACK_RESIZE:
    {
        ImGuiToolkit::Indication(press_tooltip[2], 18, 5);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        Resize *edited = static_cast<Resize*>(callback);
        float val[2] = {edited->value().x, edited->value().y};
        ImGui::SetNextItemWidth(right_align);
        if (ImGui::SliderFloat2("##CALLBACK_RESIZE", val, -2.f, 2.f, "%.2f"))
            edited->setValue( glm::vec2(val[0], val[1]));
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        ImGuiToolkit::Indication("Increment vector (x,y) to scale the source horizontally and vertically.", 2, 15);

    }
        break;

    case SourceCallback::CALLBACK_TURN:
    {
        ImGuiToolkit::Indication(press_tooltip[2], 18, 5);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        Turn *edited = static_cast<Turn*>(callback);
        float val = edited->value();
        ImGui::SetNextItemWidth(right_align);
        if ( ImGui::SliderAngle("##CALLBACK_TURN", &val, -180.f, 180.f) )
            edited->setValue(val );

        ImGui::SameLine(0, IMGUI_SAME_LINE / 3);
        ImGuiToolkit::Indication("Rotation speed (\u00B0/s) to turn the source clockwise (>0) or counterclockwise (<0)", 18, 9);
    }
        break;

    case SourceCallback::CALLBACK_DEPTH:
    {
        SetDepth *edited = static_cast<SetDepth*>(callback);

        bool bd = edited->bidirectional();
        if ( ImGuiToolkit::IconToggle(2, 13, 3, 13, &bd, press_tooltip ) )
            edited->setBidirectional(bd);

        ClosestIndex d = std::for_each(speed_values.begin(), speed_values.end(), ClosestIndex(edited->duration()));
        int speed_index = d.index;
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGuiToolkit::IconMultistate(speed_icon, &speed_index, speed_tooltip ))
            edited->setDuration(speed_values[speed_index]);

        float val = edited->value();
        ImGui::SetNextItemWidth(right_align);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGui::SliderFloat("##CALLBACK_DEPTH", &val, 11.9f, 0.1f, "%.1f"))
            edited->setValue(val);

        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        ImGuiToolkit::Indication("Depth value to place the source front (12) or back (0) in the scene.", 6, 6);
    }
        break;

    case SourceCallback::CALLBACK_PLAY:
    {
        Play *edited = static_cast<Play*>(callback);

        bool bd = edited->bidirectional();
        if ( ImGuiToolkit::IconToggle(2, 13, 3, 13, &bd, press_tooltip ) )
            edited->setBidirectional(bd);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);

        int val = edited->value() ? 1 : 0;
        ImGui::SetNextItemWidth(right_align);
        if (ImGui::SliderInt("##CALLBACK_PLAY", &val, 0, 1, "Pause  |   Play "))
            edited->setValue(val>0);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        ImGuiToolkit::Indication("Play or pause the source.", 12, 7);
    }
        break;

    case SourceCallback::CALLBACK_PLAYSPEED:
    {
        PlaySpeed *edited = static_cast<PlaySpeed*>(callback);

        bool bd = edited->bidirectional();
        if ( ImGuiToolkit::IconToggle(2, 13, 3, 13, &bd, press_tooltip ) )
            edited->setBidirectional(bd);

        ClosestIndex d = std::for_each(speed_values.begin(), speed_values.end(), ClosestIndex(edited->duration()));
        int speed_index = d.index;
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGuiToolkit::IconMultistate(speed_icon, &speed_index, speed_tooltip ))
            edited->setDuration(speed_values[speed_index]);

        float val = edited->value();
        ImGui::SetNextItemWidth(right_align);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGui::SliderFloat("##CALLBACK_PLAYSPEED", &val, 0.1f, 20.f, "x %.1f"))
            edited->setValue(val);

        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        ImGuiToolkit::Indication("Factor to multiply the playback speed of a video source.", 16, 7);
    }
        break;

    case SourceCallback::CALLBACK_PLAYFFWD:
    {
        PlayFastForward *edited = static_cast<PlayFastForward*>(callback);

        ImGuiToolkit::Indication(press_tooltip[2], 18, 5);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);

        int val = (int) edited->value();
        ImGui::SetNextItemWidth(right_align);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGui::SliderInt("##CALLBACK_PLAYFFWD", &val, 30, 1000, "%d ms"))
            edited->setValue( MAX(1, val) );

        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        ImGuiToolkit::Indication("Step increment (in miliseconds) to jump fast-forward in a video source.", 13, 7);
    }
        break;

    case SourceCallback::CALLBACK_SEEK:
    {
        Seek *edited = static_cast<Seek*>(callback);

        bool bd = edited->bidirectional();
        if ( ImGuiToolkit::IconToggle(2, 13, 3, 13, &bd, press_tooltip ) )
            edited->setBidirectional(bd);

        // get value (seconds) and convert to minutes and seconds
        float val = edited->value();
        int min = (int) floor(val / 60.f);
        float sec = val - 60.f * (float) min;

        // filtering for reading MM:SS.MS text entry
        static bool valid = true;
        static std::regex RegExTime("([0-9]+\\:)*([0-5][0-9]|[0-9])(\\.[0-9]+)*");
        struct TextFilters { static int FilterTime(ImGuiInputTextCallbackData* data) { if (data->EventChar < 256 && strchr("0123456789.:", (char)data->EventChar)) return 0; return 1; } };
        char buf6[64] = "";
        sprintf(buf6, "%d:%.2f", min, sec );

        // Text input field for MM:SS:MS seek target time
        ImGui::SetNextItemWidth(right_align);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, valid ? 1.0f : 0.2f, valid ? 1.0f : 0.2f, 1.f));
        ImGui::InputText("##CALLBACK_SEEK", buf6, 64, ImGuiInputTextFlags_CallbackCharFilter, TextFilters::FilterTime);
        valid = std::regex_match(buf6, RegExTime);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            if (valid){
                // user confirmed the entry and the input is valid
                min = 0;
                std::string minutes = buf6;
                std::string seconds = buf6;
                const size_t min_idx = minutes.rfind(':');
                if (string::npos != min_idx) {
                    seconds = minutes.substr(min_idx+1);
                    minutes.erase(min_idx);
                    BaseToolkit::is_a_number(minutes, &min);
                }
                if (BaseToolkit::is_a_value(seconds, &sec))
                    edited->setValue( sec + 60.f * (float) min );
            }
            // force to test validity next frame
            valid = false;
        }
        ImGui::PopStyleColor();

        ImGui::SameLine(0, IMGUI_SAME_LINE / 3);
        ImGuiToolkit::Indication("Target time (minutes and seconds, MM:SS.MS) to set where to jump to in a video source.", 15, 7);
    }
        break;

    case SourceCallback::CALLBACK_BRIGHTNESS:
    {
        SetBrightness *edited = static_cast<SetBrightness*>(callback);

        bool bd = edited->bidirectional();
        if ( ImGuiToolkit::IconToggle(2, 13, 3, 13, &bd, press_tooltip ) )
            edited->setBidirectional(bd);

        ClosestIndex d = std::for_each(speed_values.begin(), speed_values.end(), ClosestIndex(edited->duration()));
        int speed_index = d.index;
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGuiToolkit::IconMultistate(speed_icon, &speed_index, speed_tooltip ))
            edited->setDuration(speed_values[speed_index]);

        float val = edited->value();
        ImGui::SetNextItemWidth(right_align);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGui::SliderFloat("##CALLBACK_BRIGHTNESS", &val, -1.f, 1.f))
            edited->setValue(val);

        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        ImGuiToolkit::Indication("Brightness for color correction.", 5, 16);
    }
        break;

    case SourceCallback::CALLBACK_CONTRAST:
    {
        SetContrast *edited = static_cast<SetContrast*>(callback);

        bool bd = edited->bidirectional();
        if ( ImGuiToolkit::IconToggle(2, 13, 3, 13, &bd, press_tooltip ) )
            edited->setBidirectional(bd);

        ClosestIndex d = std::for_each(speed_values.begin(), speed_values.end(), ClosestIndex(edited->duration()));
        int speed_index = d.index;
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGuiToolkit::IconMultistate(speed_icon, &speed_index, speed_tooltip ))
            edited->setDuration(speed_values[speed_index]);

        float val = edited->value();
        ImGui::SetNextItemWidth(right_align);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGui::SliderFloat("##CALLBACK_CONTRAST", &val, -1.f, 1.f))
            edited->setValue(val);

        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        ImGuiToolkit::Indication("Contrast for color correction.", 5, 16);
    }
        break;

    case SourceCallback::CALLBACK_SATURATION:
    {
        SetSaturation *edited = static_cast<SetSaturation*>(callback);

        bool bd = edited->bidirectional();
        if ( ImGuiToolkit::IconToggle(2, 13, 3, 13, &bd, press_tooltip ) )
            edited->setBidirectional(bd);

        ClosestIndex d = std::for_each(speed_values.begin(), speed_values.end(), ClosestIndex(edited->duration()));
        int speed_index = d.index;
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGuiToolkit::IconMultistate(speed_icon, &speed_index, speed_tooltip ))
            edited->setDuration(speed_values[speed_index]);

        float val = edited->value();
        ImGui::SetNextItemWidth(right_align);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGui::SliderFloat("##CALLBACK_SATURATION", &val, -1.f, 1.f))
            edited->setValue(val);

        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        ImGuiToolkit::Indication("Saturation for color correction.", 9, 16);
    }
        break;

    case SourceCallback::CALLBACK_HUE:
    {
        SetHue *edited = static_cast<SetHue*>(callback);

        bool bd = edited->bidirectional();
        if ( ImGuiToolkit::IconToggle(2, 13, 3, 13, &bd, press_tooltip ) )
            edited->setBidirectional(bd);

        ClosestIndex d = std::for_each(speed_values.begin(), speed_values.end(), ClosestIndex(edited->duration()));
        int speed_index = d.index;
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGuiToolkit::IconMultistate(speed_icon, &speed_index, speed_tooltip ))
            edited->setDuration(speed_values[speed_index]);

        float val = edited->value();
        ImGui::SetNextItemWidth(right_align);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGui::SliderFloat("##CALLBACK_HUE", &val, 0.f, 1.f))
            edited->setValue(val);

        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        ImGuiToolkit::Indication("Hue shift for color correction.", 12, 4);
    }
        break;

    case SourceCallback::CALLBACK_THRESHOLD:
    {
        SetThreshold *edited = static_cast<SetThreshold*>(callback);

        bool bd = edited->bidirectional();
        if ( ImGuiToolkit::IconToggle(2, 13, 3, 13, &bd, press_tooltip ) )
            edited->setBidirectional(bd);

        ClosestIndex d = std::for_each(speed_values.begin(), speed_values.end(), ClosestIndex(edited->duration()));
        int speed_index = d.index;
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGuiToolkit::IconMultistate(speed_icon, &speed_index, speed_tooltip ))
            edited->setDuration(speed_values[speed_index]);

        float val = edited->value();
        ImGui::SetNextItemWidth(right_align);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGui::SliderFloat("##CALLBACK_THRESHOLD", &val, 0.0, 1.0, val < 0.001 ? "None" : "%.2f"))
            edited->setValue(val);

        ImGui::SameLine(0, IMGUI_SAME_LINE / 3);
        ImGuiToolkit::Indication("Threshold for color correction.", 8, 1);
    }
        break;

    case SourceCallback::CALLBACK_GAMMA:
    {
        SetGamma *edited = static_cast<SetGamma*>(callback);

        bool bd = edited->bidirectional();
        if ( ImGuiToolkit::IconToggle(2, 13, 3, 13, &bd, press_tooltip ) )
            edited->setBidirectional(bd);

        ClosestIndex d = std::for_each(speed_values.begin(), speed_values.end(), ClosestIndex(edited->duration()));
        int speed_index = d.index;
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGuiToolkit::IconMultistate(speed_icon, &speed_index, speed_tooltip ))
            edited->setDuration(speed_values[speed_index]);

        glm::vec4 val = edited->value();
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGui::ColorEdit3("##CALLBACK_GAMMA Color", glm::value_ptr(val), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel) )
            edited->setValue(val);
        ImGui::SetNextItemWidth(right_align);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        if (ImGui::SliderFloat("##CALLBACK_GAMMA Gamma", &val.w, 0.5f, 10.f, "%.2f", 2.f) )
            edited->setValue(val);

        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        ImGuiToolkit::Indication("Set Gamma color correction.", 6, 4);
    }
        break;

    case SourceCallback::CALLBACK_INVERT:
    {
        SetInvert *edited = static_cast<SetInvert*>(callback);

        bool bd = edited->bidirectional();
        if ( ImGuiToolkit::IconToggle(2, 13, 3, 13, &bd, press_tooltip ) )
            edited->setBidirectional(bd);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);

        int val = (int) edited->value();
        ImGui::SetNextItemWidth(right_align);
        if (ImGui::Combo("##CALLBACK_INVERT", &val, "None\0Color RGB\0Luminance\0"))
            edited->setValue( (float) val);
        ImGui::SameLine(0, IMGUI_SAME_LINE / 2);
        ImGuiToolkit::Indication("Invert mode for color correction.", 6, 16);
    }
        break;

    default:
        break;
    }
}


void InputMappingInterface::Render()
{
    const ImGuiContext& g = *GImGui;
    const ImVec2 keyItemSpacing = ImVec2(g.FontSize * 0.2f, g.FontSize * 0.2f);
    const ImVec2 keyLetterIconSize = ImVec2(g.FontSize * 1.9f, g.FontSize * 1.9f);
    const ImVec2 keyLetterItemSize = keyLetterIconSize + keyItemSpacing;
    const ImVec2 keyNumpadIconSize = ImVec2(g.FontSize * 2.4f, g.FontSize * 2.4f);
    const ImVec2 keyNumpadItemSize = keyNumpadIconSize + keyItemSpacing;
    const float  fixed_height = keyLetterItemSize.y * 5.f + g.Style.WindowBorderSize + g.FontSize + g.Style.FramePadding.y * 2.0f + keyItemSpacing.y;
    const float  inputarea_width = keyLetterItemSize.x * 5.f;

    ImGui::SetNextWindowPos(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(1000, fixed_height), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(900, fixed_height), ImVec2(FLT_MAX, fixed_height));

    if ( !ImGui::Begin(name_, &Settings::application.widget.inputs,
                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar |  ImGuiWindowFlags_NoCollapse ))
    {
        ImGui::End();
        return;
    }

    // short variable to refer to session
    Session *S = Mixer::manager().session();

    // menu (no title bar)
    if (ImGui::BeginMenuBar())
    {
        // Close and widget menu
        if (ImGuiToolkit::IconButton(4,16))
            Settings::application.widget.inputs = false;
        if (ImGui::BeginMenu(IMGUI_TITLE_INPUT_MAPPING))
        {
            // Disable
            ImGui::MenuItem( ICON_FA_BAN "  Disable", nullptr, &Settings::application.mapping.disabled);

            // Clear all
            if ( ImGui::MenuItem( ICON_FA_BACKSPACE " Clear all" ) )
                S->clearInputCallbacks();

            // output manager menu
            ImGui::Separator();
            bool pinned = Settings::application.widget.inputs_view == Settings::application.current_view;
            std::string menutext = std::string( ICON_FA_MAP_PIN "    Stick to ") + Settings::application.views[Settings::application.current_view].name + " view";
            if ( ImGui::MenuItem( menutext.c_str(), nullptr, &pinned) ){
                if (pinned)
                    Settings::application.widget.inputs_view = Settings::application.current_view;
                else
                    Settings::application.widget.inputs_view = -1;
            }
            if ( ImGui::MenuItem( MENU_CLOSE, SHORTCUT_INPUTS) )
                Settings::application.widget.inputs = false;

            ImGui::EndMenu();
        }

        // Selection of the keyboard mode
        if (ImGui::BeginMenu( input_mode[Settings::application.mapping.mode].c_str() ))
        {
            for (size_t i = 0; i < input_mode.size(); ++i) {
                if (ImGui::MenuItem(input_mode[i].c_str(), NULL, Settings::application.mapping.mode==i)) {
                    current_input_for_mode[Settings::application.mapping.mode] = current_input_;
                    Settings::application.mapping.mode = i;
                    current_input_ = current_input_for_mode[i];
                }
            }
            ImGui::EndMenu();
        }

        // Options for current key
        const std::string key = (current_input_ < INPUT_NUMPAD_LAST) ? "  Key " : "  ";
        const std::string keymenu = ICON_FA_HAND_POINT_RIGHT + key + Control::manager().inputLabel(current_input_);
        if (ImGui::BeginMenu(keymenu.c_str()) )
        {
            if ( ImGui::MenuItem( ICON_FA_WINDOW_CLOSE "  Reset mapping", NULL, false, S->inputAssigned(current_input_) ) )
                // remove all source callback of this input
                S->deleteInputCallbacks(current_input_);

            if (ImGui::BeginMenu(ICON_FA_CLOCK "  Metronome", S->inputAssigned(current_input_)))
            {
                Metronome::Synchronicity sync = S->inputSynchrony(current_input_);
                bool active = sync == Metronome::SYNC_NONE;
                if (ImGuiToolkit::MenuItemIcon(5, 13, " Not synchronized", active )){
                    S->setInputSynchrony(current_input_, Metronome::SYNC_NONE);
                }
                active = sync == Metronome::SYNC_BEAT;
                if (ImGuiToolkit::MenuItemIcon(6, 13, " Sync to beat", active )){
                    S->setInputSynchrony(current_input_, Metronome::SYNC_BEAT);
                }
                active = sync == Metronome::SYNC_PHASE;
                if (ImGuiToolkit::MenuItemIcon(7, 13, " Sync to phase", active )){
                    S->setInputSynchrony(current_input_, Metronome::SYNC_PHASE);
                }
                ImGui::EndMenu();
            }

            std::list<uint> models = S->assignedInputs();
            if (ImGui::BeginMenu(ICON_FA_COPY "  Duplicate", models.size() > 0) )
            {
                for (auto m = models.cbegin(); m != models.cend(); ++m) {
                    if ( *m != current_input_ )   {
                        if ( ImGui::MenuItem( Control::inputLabel( *m ).c_str() ) ){
                            S->copyInputCallback( *m, current_input_);
                        }
                    }
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // current window draw parameters
    const ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* draw_list = window->DrawList;
    ImVec2 frame_top = ImGui::GetCursorScreenPos();

    //
    // KEYBOARD
    //
    if ( Settings::application.mapping.mode == 0 ) {

        // Draw table of letter keys [A] to [Y]
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.50f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, keyItemSpacing);
        ImVec4 color = ImGui::GetStyle().Colors[ImGuiCol_Header];
        color.w /= Settings::application.mapping.disabled ? 2.f : 0.9f;
        ImGui::PushStyleColor(ImGuiCol_Header, color);
        color = ImGui::GetStyle().Colors[ImGuiCol_Text];
        color.w /= Settings::application.mapping.disabled ? 2.f : 1.0f;
        ImGui::PushStyleColor(ImGuiCol_Text, color);

        for (uint ik = INPUT_KEYBOARD_FIRST; ik < INPUT_KEYBOARD_LAST; ++ik){
            int i = ik - INPUT_KEYBOARD_FIRST;
            // draw overlay on active keys
            if ( Control::manager().inputActive(ik) ) {
                ImVec2 pos = frame_top + keyLetterItemSize * ImVec2( i % 5, i / 5);
                draw_list->AddRectFilled(pos, pos + keyLetterIconSize, ImGui::GetColorU32(ImGuiCol_Border), 6.f);
                // set current
                current_input_ = ik;
            }
            // draw key button
            ImGui::PushID(ik);
            if (ImGui::Selectable(Control::manager().inputLabel(ik).c_str(), S->inputAssigned(ik), 0, keyLetterIconSize)) {
                current_input_ = ik;
                // TODO SET VAR current input assigned??
            }
            ImGui::PopID();

            // if user clics and drags an assigned key icon...
            if (S->inputAssigned(ik) && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("DND_KEYBOARD", &ik, sizeof(uint));
                ImGui::Text( ICON_FA_HAND_POINT_RIGHT " %s ", Control::manager().inputLabel(ik).c_str());
                ImGui::EndDragDropSource();
            }
            // ...and drops it onto another key icon
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_KEYBOARD")) {
                    if ( payload->DataSize == sizeof(uint) ) {
                        // drop means change key of input callbacks
                        uint previous_input_key = *(const int*)payload->Data;
                        // swap
                        S->swapInputCallback(previous_input_key, ik);
                        // switch to this key
                        current_input_ = ik;
                    }
                }
                ImGui::EndDragDropTarget();
            }

            // 5 elements in a row
            if ((i % 5) < 4) ImGui::SameLine();

            // Draw frame
            ImVec2 pos = frame_top + keyLetterItemSize * ImVec2( i % 5, i / 5);
            if (ik == current_input_)
                draw_list->AddRect(pos, pos + keyLetterIconSize, ImGui::GetColorU32(ImGuiCol_Text), 6.f, ImDrawCornerFlags_All, 3.f);
            else
                draw_list->AddRect(pos, pos + keyLetterIconSize, ImGui::GetColorU32(ImGuiCol_Button), 6.f, ImDrawCornerFlags_All, 0.1f);

        }
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        ImGui::PopFont();

    }
    //
    // NUMPAD
    //
    else if ( Settings::application.mapping.mode == 1 ) {

        // custom layout of numerical keypad
        std::vector<uint> numpad_inputs = { INPUT_NUMPAD_FIRST+7, INPUT_NUMPAD_FIRST+8, INPUT_NUMPAD_FIRST+9, INPUT_NUMPAD_FIRST+11,
                                            INPUT_NUMPAD_FIRST+4, INPUT_NUMPAD_FIRST+5, INPUT_NUMPAD_FIRST+6, INPUT_NUMPAD_FIRST+12,
                                            INPUT_NUMPAD_FIRST+1, INPUT_NUMPAD_FIRST+2, INPUT_NUMPAD_FIRST+3, INPUT_NUMPAD_FIRST+13,
                                            INPUT_NUMPAD_FIRST+0, INPUT_NUMPAD_FIRST+10, INPUT_NUMPAD_FIRST+14 };

        // Draw table of keypad keys
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.50f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, keyItemSpacing);
        ImVec4 color = ImGui::GetStyle().Colors[ImGuiCol_Header];
        color.w /= Settings::application.mapping.disabled ? 2.f : 0.9f;
        ImGui::PushStyleColor(ImGuiCol_Header, color);
        color = ImGui::GetStyle().Colors[ImGuiCol_Text];
        color.w /= Settings::application.mapping.disabled ? 2.f : 1.0f;
        ImGui::PushStyleColor(ImGuiCol_Text, color);

        for (size_t p = 0; p < numpad_inputs.size(); ++p){
            uint ik = numpad_inputs[p];
            ImVec2 iconsize = p == 12 ? keyNumpadIconSize + ImVec2(keyNumpadIconSize.x+ g.Style.ItemSpacing.x, 0.f) : keyNumpadIconSize;
            ImVec2 itemsize = p == 12 ? keyNumpadItemSize + ImVec2(keyNumpadItemSize.x+ g.Style.ItemSpacing.x, 0.f) : keyNumpadItemSize;
            // draw overlay on active keys
            if ( Control::manager().inputActive(ik) ) {
                ImVec2 pos = frame_top + itemsize * ImVec2( p % 4, p / 4);
                pos += p > 12 ? ImVec2(keyNumpadIconSize.x+ g.Style.ItemSpacing.x, 0.f) : ImVec2(0,0);
                draw_list->AddRectFilled(pos, pos + iconsize, ImGui::GetColorU32(ImGuiCol_Border), 6.f);
                // set current
                current_input_ = ik;
            }
            // draw key button
            ImGui::PushID(ik);
            if (ImGui::Selectable(Control::manager().inputLabel(ik).c_str(), S->inputAssigned(ik), 0, iconsize)) {
                current_input_ = ik;
            }
            ImGui::PopID();
            // if user clics and drags an assigned key icon...
            if (S->inputAssigned(ik) && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("DND_NUMPAD", &ik, sizeof(uint));
                ImGui::Text( ICON_FA_HAND_POINT_RIGHT " %s ", Control::manager().inputLabel(ik).c_str());
                ImGui::EndDragDropSource();
            }
            // ...and drops it onto another key icon
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_NUMPAD")) {
                    if ( payload->DataSize == sizeof(uint) ) {
                        // drop means change key of input callbacks
                        uint previous_input_key = *(const int*)payload->Data;
                        // swap
                        S->swapInputCallback(previous_input_key, ik);
                        // switch to this key
                        current_input_ = ik;
                    }
                }
                ImGui::EndDragDropTarget();
            }

            // 4 elements in a row
            if ((p % 4) < 3) ImGui::SameLine();

            // Draw frame
            ImVec2 pos = frame_top + itemsize * ImVec2( p % 4, p / 4);
            pos += p > 12 ? ImVec2(keyNumpadIconSize.x + g.Style.ItemSpacing.x, 0.f) : ImVec2(0,0);
            if (ik == current_input_)
                draw_list->AddRect(pos, pos + iconsize, ImGui::GetColorU32(ImGuiCol_Text), 6.f, ImDrawCornerFlags_All, 3.f);
            else
                draw_list->AddRect(pos, pos + iconsize, ImGui::GetColorU32(ImGuiCol_Button), 6.f, ImDrawCornerFlags_All, 0.1f);

        }
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        ImGui::PopFont();

    }
    //
    // MULTITOUCH OSC
    //
    else if ( Settings::application.mapping.mode == 2 ) {

        // Draw table of TouchOSC buttons
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, keyItemSpacing);
        ImVec4 color = ImGui::GetStyle().Colors[ImGuiCol_Header];
        color.w /= Settings::application.mapping.disabled ? 2.f : 0.9f;
        ImGui::PushStyleColor(ImGuiCol_Header, color);
        color = ImGui::GetStyle().Colors[ImGuiCol_Text];
        color.w /= Settings::application.mapping.disabled ? 2.f : 1.0f;
        ImGui::PushStyleColor(ImGuiCol_Text, color);

        const ImVec2 touch_bar_size = keyNumpadItemSize * ImVec2(0.65f, 0.2f);
        const ImVec2 touch_bar_pos  = keyNumpadItemSize * ImVec2(0.125f, 0.6f);

        for (size_t t = 0; t < INPUT_MULTITOUCH_COUNT; ++t){
            uint it = INPUT_MULTITOUCH_FIRST + t;
            ImVec2 pos = frame_top + keyNumpadItemSize * ImVec2( t % 4, t / 4);

            // draw overlay on active keys
            if ( Control::manager().inputActive(it) ) {
                draw_list->AddRectFilled(pos, pos + keyNumpadIconSize, ImGui::GetColorU32(ImGuiCol_Border), 6.f);
                // set current
                current_input_ = it;
            }

            // draw key button
            ImGui::PushID(it);
            if (ImGui::Selectable(" ", S->inputAssigned(it), 0, keyNumpadIconSize))
                current_input_ = it;
            ImGui::PopID();

            // if user clics and drags an assigned key icon...
            if (S->inputAssigned(it) && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("DND_MULTITOUCH", &it, sizeof(uint));
                ImGui::Text( ICON_FA_HAND_POINT_RIGHT " %s ", Control::manager().inputLabel(it).c_str());
                ImGui::EndDragDropSource();
            }
            // ...and drops it onto another key icon
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_MULTITOUCH")) {
                    if ( payload->DataSize == sizeof(uint) ) {
                        // drop means change key of input callbacks
                        uint previous_input_key = *(const int*)payload->Data;
                        // swap
                        S->swapInputCallback(previous_input_key, it);
                        // switch to this key
                        current_input_ = it;
                    }
                }
                ImGui::EndDragDropTarget();
            }

            // 4 elements in a row
            if ((t % 4) < 3) ImGui::SameLine();

            // Draw frame
            if (it == current_input_)
                draw_list->AddRect(pos, pos + keyNumpadIconSize, ImGui::GetColorU32(ImGuiCol_Text), 6.f, ImDrawCornerFlags_All, 3.f);
            else
                draw_list->AddRect(pos, pos + keyNumpadIconSize, ImGui::GetColorU32(ImGuiCol_Button), 6.f, ImDrawCornerFlags_All, 0.1f);

            // Draw value bar
            ImVec2 prev = ImGui::GetCursorScreenPos();
            ImGui::SetCursorScreenPos( pos + touch_bar_pos);
            ImGui::ProgressBar(Control::manager().inputValue(it), touch_bar_size, "");
            ImGui::SetCursorScreenPos( prev );

        }

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        ImGui::PopFont();

    }
    //
    // JOYSTICK
    //
    else if ( Settings::application.mapping.mode == 3 ) {

        // custom layout of gamepad buttons
        std::vector<uint> gamepad_inputs = { INPUT_JOYSTICK_FIRST_BUTTON+11, INPUT_JOYSTICK_FIRST_BUTTON+13,
                                             INPUT_JOYSTICK_FIRST_BUTTON+6,
                                             INPUT_JOYSTICK_FIRST_BUTTON+2, INPUT_JOYSTICK_FIRST_BUTTON+3,

                                             INPUT_JOYSTICK_FIRST_BUTTON+14, INPUT_JOYSTICK_FIRST_BUTTON+12,
                                             INPUT_JOYSTICK_FIRST_BUTTON+7,
                                             INPUT_JOYSTICK_FIRST_BUTTON+0, INPUT_JOYSTICK_FIRST_BUTTON+1,

                                             INPUT_JOYSTICK_FIRST_BUTTON+4, INPUT_JOYSTICK_FIRST_BUTTON+9,
                                             INPUT_JOYSTICK_FIRST_BUTTON+8,
                                             INPUT_JOYSTICK_FIRST_BUTTON+10, INPUT_JOYSTICK_FIRST_BUTTON+5  };

        std::vector< std::string > gamepad_labels = {  ICON_FA_ARROW_UP,  ICON_FA_ARROW_DOWN,
                                                       ICON_FA_CHEVRON_CIRCLE_LEFT, "X", "Y",
                                                       ICON_FA_ARROW_LEFT, ICON_FA_ARROW_RIGHT,
                                                       ICON_FA_CHEVRON_CIRCLE_RIGHT, "A", "B",
                                                       "L1", "LT", ICON_FA_DOT_CIRCLE, "RT", "R1" };

        // Draw table of Gamepad Buttons
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ImVec4 color = ImGui::GetStyle().Colors[ImGuiCol_Header];
        color.w /= Settings::application.mapping.disabled ? 2.f : 0.9f;
        ImGui::PushStyleColor(ImGuiCol_Header, color);
        color = ImGui::GetStyle().Colors[ImGuiCol_Text];
        color.w /= Settings::application.mapping.disabled ? 2.f : 1.0f;
        ImGui::PushStyleColor(ImGuiCol_Text, color);

        // CENTER text for button
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, keyItemSpacing);
        for (size_t b = 0; b < gamepad_inputs.size(); ++b ){
            uint ig = gamepad_inputs[b];
            // draw overlay on active keys
            if ( Control::manager().inputActive(ig) ) {
                ImVec2 pos = frame_top + keyLetterItemSize * ImVec2( b % 5, b / 5);
                draw_list->AddRectFilled(pos, pos + keyLetterIconSize, ImGui::GetColorU32(ImGuiCol_Border), 6.f);
                // set current
                current_input_ = ig;
            }
            // draw key button
            ImGui::PushID(ig);
            if (ImGui::Selectable(gamepad_labels[b].c_str(), S->inputAssigned(ig), 0, keyLetterIconSize))
                current_input_ = ig;
            ImGui::PopID();

            // if user clics and drags an assigned key icon...
            if (S->inputAssigned(ig) && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("DND_GAMEPAD", &ig, sizeof(uint));
                ImGui::Text( ICON_FA_HAND_POINT_RIGHT " %s ", Control::manager().inputLabel(ig).c_str());
                ImGui::EndDragDropSource();
            }
            // ...and drops it onto another key icon
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_GAMEPAD")) {
                    if ( payload->DataSize == sizeof(uint) ) {
                        // drop means change key of input callbacks
                        uint previous_input_key = *(const int*)payload->Data;
                        // swap
                        S->swapInputCallback(previous_input_key, ig);
                        // switch to this key
                        current_input_ = ig;
                    }
                }
                ImGui::EndDragDropTarget();
            }

            if ((b % 5) < 4) ImGui::SameLine();

            // Draw frame
            ImVec2 pos = frame_top + keyLetterItemSize * ImVec2( b % 5, b / 5);
            if (ig == current_input_)
                draw_list->AddRect(pos, pos + keyLetterIconSize, ImGui::GetColorU32(ImGuiCol_Text), 6.f, ImDrawCornerFlags_All, 3.f);
            else if ( b!= 2 && b!= 7 && b!=12 )
                draw_list->AddRect(pos, pos + keyLetterIconSize, ImGui::GetColorU32(ImGuiCol_Button), 6.f, ImDrawCornerFlags_All, 0.1f);

        }
        ImGui::PopStyleVar();
        ImGui::PopFont();

        // Table of Gamepad Axis
        const ImVec2 axis_top = frame_top + ImVec2(0.f, 3.f * keyLetterItemSize.y);
        const ImVec2 axis_item_size(inputarea_width / 2.f, (2.f * keyLetterItemSize.y) / 3.f);
        const ImVec2 axis_icon_size = axis_item_size - g.Style.ItemSpacing;
        const ImVec2 axis_bar_size = axis_icon_size * ImVec2(0.4f, 0.4f);
        ImVec2 axis_bar_pos(axis_icon_size.x * 0.5f, axis_icon_size.y *0.3f );

        // LEFT align for 3 axis on the left
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.09f, 0.5f));

        // define top left screen cursor position
        ImVec2 pos = axis_top;
        // Draw a little bar showing activity on the gamepad axis
        ImGui::SetCursorScreenPos( pos + axis_bar_pos);
        ImGuiToolkit::ValueBar(Control::manager().inputValue(INPUT_JOYSTICK_FIRST_AXIS), axis_bar_size);
        // Draw button to assign the axis to an action
        ImGui::SetCursorScreenPos( pos );
        if (ImGui::Selectable("LX", S->inputAssigned(INPUT_JOYSTICK_FIRST_AXIS), 0, axis_icon_size))
            current_input_ = INPUT_JOYSTICK_FIRST_AXIS;
        // Draw frame around current gamepad axis
        if (current_input_ == INPUT_JOYSTICK_FIRST_AXIS)
            draw_list->AddRect(pos, pos + axis_icon_size, ImGui::GetColorU32(ImGuiCol_Text), 6.f, ImDrawCornerFlags_All, 3.f);

        pos = axis_top + ImVec2( 0, axis_item_size.y);
        ImGui::SetCursorScreenPos( pos + axis_bar_pos);
        ImGuiToolkit::ValueBar(Control::manager().inputValue(INPUT_JOYSTICK_FIRST_AXIS+1), axis_bar_size);
        ImGui::SetCursorScreenPos( pos );
        if (ImGui::Selectable("LY", S->inputAssigned(INPUT_JOYSTICK_FIRST_AXIS+1), 0, axis_icon_size))
            current_input_ = INPUT_JOYSTICK_FIRST_AXIS+1;
        if (current_input_ == INPUT_JOYSTICK_FIRST_AXIS+1)
            draw_list->AddRect(pos, pos + axis_icon_size, ImGui::GetColorU32(ImGuiCol_Text), 6.f, ImDrawCornerFlags_All, 3.f);

        pos = axis_top + ImVec2( 0, 2.f * axis_item_size.y);
        ImGui::SetCursorScreenPos( pos + axis_bar_pos);
        ImGuiToolkit::ValueBar(Control::manager().inputValue(INPUT_JOYSTICK_FIRST_AXIS+2), axis_bar_size);
        ImGui::SetCursorScreenPos( pos );
        if (ImGui::Selectable("L2", S->inputAssigned(INPUT_JOYSTICK_FIRST_AXIS+2), 0, axis_icon_size))
            current_input_ = INPUT_JOYSTICK_FIRST_AXIS+2;
        if (current_input_ == INPUT_JOYSTICK_FIRST_AXIS+2)
            draw_list->AddRect(pos, pos + axis_icon_size, ImGui::GetColorU32(ImGuiCol_Text), 6.f, ImDrawCornerFlags_All, 3.f);

        ImGui::PopStyleVar();

        // RIGHT align for 3 axis on the right
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.91f, 0.5f));
        axis_bar_pos.x = g.Style.ItemSpacing.x;

        pos = axis_top + ImVec2( axis_item_size.x, 0.f);
        ImGui::SetCursorScreenPos( pos + axis_bar_pos);
        ImGuiToolkit::ValueBar(Control::manager().inputValue(INPUT_JOYSTICK_FIRST_AXIS+3), axis_bar_size);
        ImGui::SetCursorScreenPos( pos );
        if (ImGui::Selectable("RX", S->inputAssigned(INPUT_JOYSTICK_FIRST_AXIS+3), 0, axis_icon_size))
            current_input_ = INPUT_JOYSTICK_FIRST_AXIS+3;
        if (current_input_ == INPUT_JOYSTICK_FIRST_AXIS+3)
            draw_list->AddRect(pos, pos + axis_icon_size, ImGui::GetColorU32(ImGuiCol_Text), 6.f, ImDrawCornerFlags_All, 3.f);

        pos = axis_top + ImVec2( axis_item_size.x, axis_item_size.y);
        ImGui::SetCursorScreenPos( pos + axis_bar_pos);
        ImGuiToolkit::ValueBar(Control::manager().inputValue(INPUT_JOYSTICK_FIRST_AXIS+4), axis_bar_size);
        ImGui::SetCursorScreenPos( pos );
        if (ImGui::Selectable("RY", S->inputAssigned(INPUT_JOYSTICK_FIRST_AXIS+4), 0, axis_icon_size))
            current_input_ = INPUT_JOYSTICK_FIRST_AXIS+4;
        if (current_input_ == INPUT_JOYSTICK_FIRST_AXIS+4)
            draw_list->AddRect(pos, pos + axis_icon_size, ImGui::GetColorU32(ImGuiCol_Text), 6.f, ImDrawCornerFlags_All, 3.f);

        pos = axis_top + ImVec2( axis_item_size.x, 2.f * axis_item_size.y);
        ImGui::SetCursorScreenPos( pos + axis_bar_pos);
        ImGuiToolkit::ValueBar(Control::manager().inputValue(INPUT_JOYSTICK_FIRST_AXIS+5), axis_bar_size);
        ImGui::SetCursorScreenPos( pos );
        if (ImGui::Selectable("R2", S->inputAssigned(INPUT_JOYSTICK_FIRST_AXIS+5), 0, axis_icon_size))
            current_input_ = INPUT_JOYSTICK_FIRST_AXIS+5;
        if (current_input_ == INPUT_JOYSTICK_FIRST_AXIS+5)
            draw_list->AddRect(pos, pos + axis_icon_size, ImGui::GetColorU32(ImGuiCol_Text), 6.f, ImDrawCornerFlags_All, 3.f);

        ImGui::PopStyleVar(2);

        // Done with color and font change
        ImGui::PopStyleColor(2);

    }

    // Draw child Window (right) to list reactions to input
    ImGui::SetCursorScreenPos(frame_top + g.Style.FramePadding + ImVec2(inputarea_width,0.f));
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.f, g.Style.ItemSpacing.y * 2.f) );
        ImGui::BeginChild("InputsMappingInterfacePanel", ImVec2(0, 0), false);

        float w = ImGui::GetWindowWidth() *0.25f;

        ///
        /// list of input callbacks for the current input
        ///
        if (S->inputAssigned(current_input_)) {

            auto result = S->getSourceCallbacks(current_input_);
            for (auto kit = result.cbegin(); kit != result.cend(); ++kit) {

                Target target = kit->first;
                SourceCallback *callback = kit->second;

                // push ID because we repeat the same UI
                ImGui::PushID( (void*) callback);

                // Delete interface
                if (ImGuiToolkit::IconButton(ICON_FA_MINUS, "Remove") ){
                    S->deleteInputCallback(callback);
                    // reload
                    ImGui::PopID();
                    break;
                }

                // Select Target
                ImGui::SameLine(0, IMGUI_SAME_LINE);
                ImGui::SetNextItemWidth(w);
                Target selected_target = ComboSelectTarget(target);
                // if the selected target variant was filled with a value
                if (selected_target.index() > 0) {
                    // reassign the callback to newly selected source
                    S->assignInputCallback(current_input_, selected_target, callback);
                    // reload
                    ImGui::PopID();
                    break;
                }

                // check if target is a Source with image processing enabled
                bool withimageprocessing = false;
                if ( target.index() == 1 ) {
                    if (Source * const* v = std::get_if<Source *>(&target)) {
                        withimageprocessing = (*v)->imageProcessingEnabled();
                    }
                }

                // Select Reaction
                ImGui::SameLine(0, IMGUI_SAME_LINE);
                ImGui::SetNextItemWidth(w);
                uint type = ComboSelectCallback( callback->type(), withimageprocessing );
                if (type > 0) {
                    // remove previous callback
                    S->deleteInputCallback(callback);
                    // assign callback
                    S->assignInputCallback(current_input_, target, SourceCallback::create((SourceCallback::CallbackType)type) );
                    // reload
                    ImGui::PopID();
                    break;
                }

                // Adjust parameters
                ImGui::SameLine(0, IMGUI_SAME_LINE);
                if (callback)
                    SliderParametersCallback( callback, target );

                ImGui::PopID();

            }

        }
        else {
            ImGui::Text("No action mapped to this input. Add one with +.");
        }

        ///
        /// Add a new interface
        ///
        static bool temp_new_input = false;
        static Target temp_new_target;
        static uint temp_new_callback = 0;

        // step 1 : press '+'
        if (temp_new_input) {
            if (ImGuiToolkit::IconButton(ICON_FA_TIMES, "Cancel") ){
                temp_new_target = std::monostate();
                temp_new_callback = 0;
                temp_new_input = false;
            }
        }
        else if (ImGuiToolkit::IconButton(ICON_FA_PLUS, "Add mapping") )
            temp_new_input = true;

        if (temp_new_input) {
            // step 2 : Get input for source
            ImGui::SameLine(0, IMGUI_SAME_LINE);
            ImGui::SetNextItemWidth(w);

            Target selected_target = ComboSelectTarget(temp_new_target);
            // if the selected target variant was filled with a value
            if (selected_target.index() > 0) {
                temp_new_target = selected_target;
                temp_new_callback = 0;
            }
            // possible new target
            if (temp_new_target.index() > 0) {
                // check if target is a Source with image processing enabled
                bool withimageprocessing = false;
                if ( temp_new_target.index() == 1 ) {
                    if (Source * const* v = std::get_if<Source *>(&temp_new_target)) {
                        withimageprocessing = (*v)->imageProcessingEnabled();
                    }
                }
                // step 3: Get input for callback type
                ImGui::SameLine(0, IMGUI_SAME_LINE);
                ImGui::SetNextItemWidth(w);
                temp_new_callback = ComboSelectCallback( temp_new_callback, withimageprocessing );
                // user selected a callback type
                if (temp_new_callback > 0) {
                    // step 4 : create new callback and add it to source
                    S->assignInputCallback(current_input_, temp_new_target, SourceCallback::create((SourceCallback::CallbackType)temp_new_callback) );
                    // done
                    temp_new_target = std::monostate();
                    temp_new_callback = 0;
                    temp_new_input = false;
                }
            }
        }

        ///
        /// Sync info lower right corner
        ///
        Metronome::Synchronicity sync = S->inputSynchrony(current_input_);
        if ( sync > Metronome::SYNC_NONE) {
            ImGui::SetCursorPos(ImGui::GetWindowSize() - ImVec2(50, 50));
            ImGuiToolkit::Icon( sync > Metronome::SYNC_BEAT ? 7 : 6, 13);
        }

        // close child window
        ImGui::EndChild();
        ImGui::PopStyleVar();
    }


    ImGui::End();
}


///
/// SHADER EDITOR
///
///
ShaderEditor::ShaderEditor() : WorkspaceWindow("Shader"), current_(nullptr), show_shader_inputs_(false)
{
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
        "radians",  "degrees", "sin",  "cos", "tan", "pow", "exp2", "log2", "sqrt", "inversesqrt",
        "sign", "floor", "ceil", "fract", "mod", "min", "max", "clamp", "mix", "step", "smoothstep", "length", "distance",
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
        id.mDeclaration = "GLSL function";
        lang.mIdentifiers.insert(std::make_pair(std::string(k), id));
    }

    static const char* const filter_keyword[] = {
        "iResolution", "iTime", "iTimeDelta", "iFrame", "iChannelResolution", "iDate", "iMouse",
        "iChannel0", "iChannel1", "iTransform", "FragColor", "vertexColor", "vertexUV"
    };
    for (auto& k : filter_keyword)
    {
        TextEditor::Identifier id;
        id.mDeclaration = "Shader keyword";
        lang.mPreprocIdentifiers.insert(std::make_pair(std::string(k), id));
    }

    // init editor
    _editor.SetLanguageDefinition(lang);
    _editor.SetHandleKeyboardInputs(true);
    _editor.SetShowWhitespaces(false);
    _editor.SetText("");
    _editor.SetReadOnly(true);

    // status
    status_ = "-";
}

void ShaderEditor::setVisible(bool on)
{
    // restore workspace to show the window
    if (WorkspaceWindow::clear_workspace_enabled) {
        WorkspaceWindow::restoreWorkspace(on);
        // do not change status if ask to hide (consider user asked to toggle because the window was not visible)
        if (!on)  return;
    }

    if (on && Settings::application.widget.shader_editor_view != Settings::application.current_view)
        Settings::application.widget.shader_editor_view = -1;

    Settings::application.widget.shader_editor = on;
}

bool ShaderEditor::Visible() const
{
    return ( Settings::application.widget.shader_editor
             && (Settings::application.widget.shader_editor_view < 0 || Settings::application.widget.shader_editor_view == Settings::application.current_view )
             );
}

void ShaderEditor::BuildShader()
{
    // if the UI has a current clone, and ref to code for current clone is valid
    if (current_ != nullptr &&  filters_.find(current_) != filters_.end()) {

        // set the code of the current filter
        filters_[current_].setCode( { _editor.GetText(), "" } );

        // filter changed, cannot be named as before
        filters_[current_].setName("Custom");

        // change the filter of the current image filter
        // => this triggers compilation of shader
        compilation_ = new std::promise<std::string>();
        current_->setProgram( filters_[current_], compilation_ );
        compilation_return_ = compilation_->get_future();

        // inform status
        status_ = "Building...";
    }
}

void ShaderEditor::Render()
{
    ImGui::SetWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

    if ( !ImGui::Begin(name_, &Settings::application.widget.shader_editor,
                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar |  ImGuiWindowFlags_NoCollapse ))
    {
        ImGui::End();
        return;
    }

    // menu (no title bar)
    if (ImGui::BeginMenuBar())
    {
        // Close and widget menu
        if (ImGuiToolkit::IconButton(4,16))
            Settings::application.widget.shader_editor = false;
        if (ImGui::BeginMenu(IMGUI_TITLE_SHADEREDITOR))
        {
            // reload code from GPU
            if (ImGui::MenuItem( ICON_FA_SYNC "  Reload", nullptr, nullptr, current_ != nullptr)) {
                // force reload
                if ( current_ != nullptr )
                    filters_.erase(current_);
                current_ = nullptr;
            }

            // Menu section for presets
            if (ImGui::BeginMenu( ICON_FA_SCROLL " Example code", current_ != nullptr))
            {
                for (auto p = FilteringProgram::presets.begin(); p != FilteringProgram::presets.end(); ++p){

                    if (current_ != nullptr && ImGui::MenuItem( p->name().c_str() )) {
                        // change the filter of the current image filter
                        // => this triggers compilation of shader
                        compilation_ = new std::promise<std::string>();
                        current_->setProgram( *p, compilation_ );
                        compilation_return_ = compilation_->get_future();
                        // inform status
                        status_ = "Building...";
                        // force reload
                        if ( current_ != nullptr )
                            filters_.erase(current_);
                        current_ = nullptr;
                    }
                }
                ImGui::EndMenu();
            }

            // Open browser to shadertoy website
            if (ImGui::MenuItem( ICON_FA_EXTERNAL_LINK_ALT "  Browse shadertoy.com"))
                SystemToolkit::open("https://www.shadertoy.com/");

            // Enable/Disable editor options
            ImGui::Separator();
            ImGui::MenuItem( ICON_FA_UNDERLINE "  Show Shader Inputs", nullptr, &show_shader_inputs_);
            bool ws = _editor.IsShowingWhitespaces();
            if (ImGui::MenuItem( ICON_FA_LONG_ARROW_ALT_RIGHT "  Show whitespace", nullptr, &ws))
                _editor.SetShowWhitespaces(ws);

            // output manager menu
            ImGui::Separator();
            bool pinned = Settings::application.widget.shader_editor_view == Settings::application.current_view;
            std::string menutext = std::string( ICON_FA_MAP_PIN "    Stick to ") + Settings::application.views[Settings::application.current_view].name + " view";
            if ( ImGui::MenuItem( menutext.c_str(), nullptr, &pinned) ){
                if (pinned)
                    Settings::application.widget.shader_editor_view = Settings::application.current_view;
                else
                    Settings::application.widget.shader_editor_view = -1;
            }
            if ( ImGui::MenuItem( MENU_CLOSE, SHORTCUT_SHADEREDITOR) )
                Settings::application.widget.shader_editor = false;

            ImGui::EndMenu();
        }

        // Edit menu
        bool ro = _editor.IsReadOnly();
        if (ImGui::BeginMenu( "Edit", current_ != nullptr ) ) {

            if (ImGui::MenuItem( MENU_UNDO, SHORTCUT_UNDO, nullptr, !ro && _editor.CanUndo()))
                _editor.Undo();
            if (ImGui::MenuItem( MENU_REDO, CTRL_MOD "Y", nullptr, !ro && _editor.CanRedo()))
                _editor.Redo();
            if (ImGui::MenuItem( MENU_COPY, SHORTCUT_COPY, nullptr, _editor.HasSelection()))
                _editor.Copy();
            if (ImGui::MenuItem( MENU_CUT, SHORTCUT_CUT, nullptr, !ro && _editor.HasSelection()))
                _editor.Cut();
            if (ImGui::MenuItem( MENU_DELETE, SHORTCUT_DELETE, nullptr, !ro && _editor.HasSelection()))
                _editor.Delete();
            if (ImGui::MenuItem( MENU_PASTE, SHORTCUT_PASTE, nullptr, !ro && ImGui::GetClipboardText() != nullptr))
                _editor.Paste();
            if (ImGui::MenuItem( MENU_SELECTALL, SHORTCUT_SELECTALL, nullptr, _editor.GetText().size() > 1 ))
                _editor.SetSelection(TextEditor::Coordinates(), TextEditor::Coordinates(_editor.GetTotalLines(), 0));

            ImGui::EndMenu();
        }

        // Build action menu
        if (ImGui::MenuItem( ICON_FA_HAMMER " Build", CTRL_MOD "B", nullptr, current_ != nullptr ))
            BuildShader();

        ImGui::EndMenuBar();
    }

    // garbage collection
    if ( Mixer::manager().session()->numSources() < 1 )
    {
        filters_.clear();
        current_ = nullptr;
        _editor.SetText("");
    }

    // if compiling, cannot change source nor do anything else
    static std::chrono::milliseconds timeout = std::chrono::milliseconds(4);
    if (compilation_ != nullptr && compilation_return_.wait_for(timeout) == std::future_status::ready )
    {
        // get message returned from compilation
        status_ = compilation_return_.get();

        // end compilation promise
        delete compilation_;
        compilation_ = nullptr;
    }
    // not compiling
    else {

        ImageFilter *i = nullptr;
        // get current clone source
        Source *s = Mixer::manager().currentSource();
        // if there is a current source
        if (s != nullptr) {
            CloneSource *c = dynamic_cast<CloneSource *>(s);
            // if the current source is a clone
            if ( c != nullptr ) {
                FrameBufferFilter *f = c->filter();
                // if the filter seems to be an Image Filter
                if (f != nullptr && f->type() == FrameBufferFilter::FILTER_IMAGE ) {
                    i = dynamic_cast<ImageFilter *>(f);
                    // if we can access the code of the filter
                    if (i != nullptr) {
                        // if the current clone was not already registered
                        if ( filters_.find(i) == filters_.end() )
                            // remember code for this clone
                            filters_[i] = i->program();
                    }
                    else {
                        filters_.erase(i);
                        i = nullptr;
                    }
                }
                else
                    status_ = "-";
            }
            else
                status_ = "-";
        }
        else
            status_ = "-";

        // change editor text only if current changed
        if ( current_ != i)
        {
            // get the editor text and remove trailing '\n'
            std::string code = _editor.GetText();
            code = code.substr(0, code.size() -1);

            // remember this code as buffered for the filter of this source
            filters_[current_].setCode( { code, "" } );

            // if switch to another shader code
            if ( i != nullptr ) {
                // change editor
                _editor.SetText( filters_[i].code().first );
                _editor.SetReadOnly(false);
                status_ = "Ready.";
            }
            // cancel edit clone
            else {
                // cancel editor
                _editor.SetText("");
                _editor.SetReadOnly(true);
                status_ = "-";
            }
            // current changed
            current_ = i;
        }

    }

    // render the window content in mono font
    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_MONO);

    // render status message
    ImGui::Text("Status: %s", status_.c_str());

    // render shader input
    if (show_shader_inputs_) {
        ImGuiTextBuffer info;
        info.append(FilteringProgram::getFilterCodeInputs().c_str());

        // Show info text bloc (multi line, dark background)
        ImGuiToolkit::PushFont( ImGuiToolkit::FONT_MONO );
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6, 0.6, 0.6, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.14f, 0.9f));
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ImGui::InputTextMultiline("##Info", (char *)info.c_str(), info.size(), ImVec2(-1, 8*ImGui::GetTextLineHeightWithSpacing()), ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleColor(2);
        ImGui::PopFont();

        // sliders iMouse
        ImGui::SliderFloat4("##iMouse", glm::value_ptr( FilteringProgram::iMouse ), 0.0, 1.0 );
    }
    else
        ImGui::Spacing();

    // special case for 'CTRL + B' keyboard shortcut
    // the TextEditor captures keyboard focus from the main imgui context
    // so UserInterface::handleKeyboard cannot capture this event:
    // reading key press before render bypasses this problem
    const ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigMacOSXBehaviors ? io.KeySuper : io.KeyCtrl && ImGui::IsKeyPressed(GLFW_KEY_B))
        BuildShader();

    // render main editor
    _editor.Render("Shader Editor");

    ImGui::PopFont();

    ImGui::End();
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

    // restore media mode as saved
    if (Settings::application.recentImportFolders.path.compare(IMGUI_LABEL_RECENT_FILES) == 0)
        new_media_mode = MEDIA_RECENT;
    else if (Settings::application.recentImportFolders.path.compare(IMGUI_LABEL_RECENT_RECORDS) == 0)
        new_media_mode = MEDIA_RECORDING;
    else
        new_media_mode = MEDIA_FOLDER;
    new_media_mode_changed = true;

    source_to_replace = nullptr;
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

void Navigator::clearNewPannel()
{
    new_source_preview_.setSource();
    pattern_type = -1;
    custom_pipeline = false;
    custom_connected = false;
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
    new_media_mode_changed = true;
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
    std::pair<std::string, std::string> tooltip = {"", ""};
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
    const float sourcelist_height = height_ - 5.5f * icon_width - 5.f * style.WindowPadding.y; // space for 4 icons of view

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

            // the "=" icon for menu
            if (ImGui::Selectable( ICON_FA_BARS, &selected_button[NAV_MENU], 0, iconsize))
                applyButtonSelection(NAV_MENU);
            if (ImGui::IsItemHovered())
                tooltip = {TOOLTIP_MAIN, SHORTCUT_MAIN};

            //
            // the list of INITIALS for sources
            //
            int index = 0;
            SourceList::iterator iter;
            for (iter = Mixer::manager().session()->begin(); iter != Mixer::manager().session()->end(); ++iter, ++index)
            {
                Source *s = (*iter);

                // ignore source if failed (managed in stash below)
                if (s->failed()){
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_FAILED, 1.));
                    ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetColorU32(ImGuiCol_Button));
                    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGui::GetColorU32(ImGuiCol_ButtonActive));
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetColorU32(ImGuiCol_ButtonHovered));
                }

                // draw an indicator for current source
                if ( s->mode() >= Source::SELECTED ){
                    ImVec2 p1 = ImGui::GetCursorScreenPos() + ImVec2(icon_width, 0.5f * sourceiconsize.y);
                    ImVec2 p2 = ImVec2(p1.x + 2.f, p1.y + 2.f);
                    const ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);
                    if ((*iter)->mode() == Source::CURRENT)  {
                        p1 = ImGui::GetCursorScreenPos() + ImVec2(icon_width, 0);
                        p2 = ImVec2(p1.x + 2.f, p1.y + sourceiconsize.y);
                    }
                    draw_list->AddRect(p1, p2, color, 0.0f,  0, 3.f);
                }
                // draw select box
                ImGui::PushID(std::to_string(s->group(View::RENDERING)->id()).c_str());
                if (ImGui::Selectable(s->initials(), &selected_button[index], 0, sourceiconsize))
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

                if (s->failed()){
                    ImGui::PopStyleColor(4);
                }

                ImGui::PopID();
            }

            // the "+" icon for action of creating new source
            if (ImGui::Selectable( source_to_replace != nullptr ? ICON_FA_PLUS_SQUARE : ICON_FA_PLUS,
                                   &selected_button[NAV_NEW], 0, iconsize)) {
                Mixer::manager().unsetCurrentSource();
                applyButtonSelection(NAV_NEW);
            }
            if (ImGui::IsItemHovered())
                tooltip = {TOOLTIP_NEW_SOURCE, SHORTCUT_NEW_SOURCE};
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
    ImGui::SetNextWindowSize( ImVec2(width_, height_ - sourcelist_height), ImGuiCond_Always );
    ImGui::SetNextWindowBgAlpha(0.95f); // Transparent background
    if (ImGui::Begin("##navigatorViews", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollWithMouse))
    {
        bool selected_view[View::INVALID] = { };
        selected_view[ Settings::application.current_view ] = true;
        int previous_view = Settings::application.current_view;
        if (ImGui::Selectable( ICON_FA_BULLSEYE, &selected_view[View::MIXING], 0, iconsize))
        {
            Mixer::manager().setView(View::MIXING);
            view_pannel_visible = previous_view == Settings::application.current_view;
        }
        if (ImGui::IsItemHovered())
            tooltip = {"Mixing ", "F1"};
        if (ImGui::Selectable( ICON_FA_OBJECT_UNGROUP , &selected_view[View::GEOMETRY], 0, iconsize))
        {
            Mixer::manager().setView(View::GEOMETRY);
            view_pannel_visible = previous_view == Settings::application.current_view;
        }
        if (ImGui::IsItemHovered())
            tooltip = {"Geometry ", "F2"};
        if (ImGui::Selectable( ICON_FA_LAYER_GROUP, &selected_view[View::LAYER], 0, iconsize))
        {
            Mixer::manager().setView(View::LAYER);
            view_pannel_visible = previous_view == Settings::application.current_view;
        }
        if (ImGui::IsItemHovered())
            tooltip = {"Layers ", "F3"};
        if (ImGui::Selectable( ICON_FA_CHESS_BOARD, &selected_view[View::TEXTURE], 0, iconsize))
        {
            Mixer::manager().setView(View::TEXTURE);
            view_pannel_visible = previous_view == Settings::application.current_view;
        }
        if (ImGui::IsItemHovered())
            tooltip = {"Texturing ", "F4"};
        if (ImGui::Selectable( ICON_FA_TV, &selected_view[View::DISPLAYS], 0, iconsize))
        {
            Mixer::manager().setView(View::DISPLAYS);
            view_pannel_visible = previous_view == Settings::application.current_view;
        }
        if (ImGui::IsItemHovered())
            tooltip = {"Displays  ", "F5"};

        ImVec2 pos = ImGui::GetCursorPos();
        ImGui::SetCursorPos(pos + ImVec2(0.f, style.WindowPadding.y));
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_MONO);
        if ( ImGuiToolkit::IconButton( Rendering::manager().mainWindow().isFullscreen() ? ICON_FA_COMPRESS_ALT : ICON_FA_EXPAND_ALT ) )
            Rendering::manager().mainWindow().toggleFullscreen();
        if (ImGui::IsItemHovered())
            tooltip = {TOOLTIP_FULLSCREEN, SHORTCUT_FULLSCREEN};

        ImGui::SetCursorPos(pos + ImVec2(width_ * 0.5f, style.WindowPadding.y));
        if ( ImGuiToolkit::IconButton( WorkspaceWindow::clear() ? ICON_FA_TOGGLE_OFF : ICON_FA_TOGGLE_ON ) )
            WorkspaceWindow::toggleClearRestoreWorkspace();
        if (ImGui::IsItemHovered())
            tooltip = { WorkspaceWindow::clear() ? TOOLTIP_SHOW : TOOLTIP_HIDE, SHORTCUT_HIDE};

        ImGui::PopFont();

        ImGui::End();
    }

    // show tooltip
    if (!tooltip.first.empty()) {
        // pseudo timeout for showing tooltip
        if (_timeout_tooltip > IMGUI_TOOLTIP_TIMEOUT)
            ImGuiToolkit::ToolTip(tooltip.first.c_str(), tooltip.second.c_str());
        else
            ++_timeout_tooltip;
    }
    else
        _timeout_tooltip = 0;

    if ( view_pannel_visible && !pannel_visible_ )
        RenderViewPannel( ImVec2(width_, sourcelist_height), ImVec2(width_*0.8f, height_ - sourcelist_height) );

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
        if (ImGuiToolkit::IconButton(8,7)) {
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
    if (s == nullptr || Settings::application.current_view == View::TRANSITION)
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

        // index indicator
        ImGui::SetCursorPos(ImVec2(pannel_width_ - 2 * ImGui::GetTextLineHeight(), 15.f));
        ImGui::TextDisabled("#%d", Mixer::manager().indexCurrentSource());

        // name
        std::string sname = s->name();
        ImGui::SetCursorPosY(width_);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if (ImGuiToolkit::InputText("Name", &sname) ){
            Mixer::manager().renameSource(s, sname);
        }

        // Source pannel
        static ImGuiVisitor v;
        s->accept(v);
        ImGui::Text(" ");

        // clone button
        if ( s->failed() ) {
            ImGuiToolkit::ButtonDisabled( ICON_FA_SHARE_SQUARE " Clone & Filter", ImVec2(ImGui::GetContentRegionAvail().x, 0));
        }
        else if ( ImGui::Button( ICON_FA_SHARE_SQUARE " Clone & Filter", ImVec2(ImGui::GetContentRegionAvail().x, 0)) )
            Mixer::manager().addSource ( Mixer::manager().createSourceClone() );

        // replace button
        if ( s->cloned() ) {
            ImGuiToolkit::ButtonDisabled( ICON_FA_PLUS_SQUARE " Replace", ImVec2(ImGui::GetContentRegionAvail().x, 0));
            if (ImGui::IsItemHovered())
                ImGuiToolkit::ToolTip("Cannot replace if source is cloned");
        }
        else if ( ImGui::Button( ICON_FA_PLUS_SQUARE " Replace", ImVec2(ImGui::GetContentRegionAvail().x, 0)) ) {
            // prepare panel for new source of same type
            MediaSource *file = dynamic_cast<MediaSource *>(s);
            MultiFileSource *sequence = dynamic_cast<MultiFileSource *>(s);
            CloneSource *internal = dynamic_cast<CloneSource *>(s);
            PatternSource *generated = dynamic_cast<PatternSource *>(s);
            if (file != nullptr)
                Settings::application.source.new_type = SOURCE_FILE;
            else if (sequence != nullptr)
                Settings::application.source.new_type = SOURCE_SEQUENCE;
            else if (internal != nullptr)
                Settings::application.source.new_type = SOURCE_INTERNAL;
            else if (generated != nullptr)
                Settings::application.source.new_type = SOURCE_GENERATED;
            else
                Settings::application.source.new_type = SOURCE_CONNECTED;

            // switch to panel new source
            showPannelSource(NAV_NEW);
            // set source to be replaced
            source_to_replace = s;
        }
        // delete button
        if ( ImGui::Button( ICON_FA_BACKSPACE " Delete", ImVec2(ImGui::GetContentRegionAvail().x, 0)) ) {
            Mixer::manager().deleteSource(s);
            Action::manager().store(sname + std::string(": deleted"));
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
        if (source_to_replace != nullptr)
            ImGui::Text("Replace");
        else
            ImGui::Text("Insert");
        ImGui::PopFont();

        // Edit menu
        ImGui::SetCursorPosY(width_);
        ImGui::Text("Source");

        //
        // News Source selection pannel
        //
        static const char* origin_names[SOURCE_TYPES] = { ICON_FA_PHOTO_VIDEO "  File",
                                               ICON_FA_IMAGES "   Sequence",
                                               ICON_FA_PLUG "    Connected",
                                               ICON_FA_COG "   Generated",
                                               ICON_FA_SYNC "   Internal"
                                             };
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if (ImGui::Combo("##Origin", &Settings::application.source.new_type, origin_names, IM_ARRAYSIZE(origin_names)) )
            clearNewPannel();

        ImGui::SetCursorPosY(2.f * width_);

        // File Source creation
        if (Settings::application.source.new_type == SOURCE_FILE) {

            static DialogToolkit::OpenMediaDialog fileimportdialog("Open Media");
            static DialogToolkit::OpenFolderDialog folderimportdialog("Select Folder");

            // clic button to load file
            if ( ImGui::Button( ICON_FA_FOLDER_OPEN " Open File", ImVec2(ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN, 0)) )
                fileimportdialog.open();
            // Indication
            ImGui::SameLine();
            ImGuiToolkit::HelpToolTip("Create a source from a file:\n"
                                                 ICON_FA_CARET_RIGHT " Video (*.mpg, *mov, *.avi, etc.)\n"
                                                 ICON_FA_CARET_RIGHT " Image (*.jpg, *.png, etc.)\n"
                                                 ICON_FA_CARET_RIGHT " Vector graphics (*.svg)\n"
                                                 ICON_FA_CARET_RIGHT " vimix session (*.mix)\n"
                                                 "\nNB: Equivalent to dropping the file in the workspace");

            // get media file if dialog finished
            if (fileimportdialog.closed()){
                // get the filename from this file dialog
                std::string importpath = fileimportdialog.path();
                // switch to recent files
                setNewMedia(MEDIA_RECENT, importpath);
                // open file
                if (!importpath.empty()) {
                    std::string label = BaseToolkit::transliterate( sourceMediaFileCurrent );
                    new_source_preview_.setSource( Mixer::manager().createSourceFile(sourceMediaFileCurrent), label);
                }
            }

            // combo to offer lists
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
                if (ImGui::Selectable( ICON_FA_FOLDER_PLUS " Add Folder") ) {
                    folderimportdialog.open();
                }
                ImGui::EndCombo();
            }

            // return from thread for folder openning
            if (folderimportdialog.closed() && !folderimportdialog.path().empty()) {
                Settings::application.recentImportFolders.push(folderimportdialog.path());
                setNewMedia(MEDIA_FOLDER, folderimportdialog.path());
            }

            // icons to clear lists or discarc folder
            ImVec2 pos_top = ImGui::GetCursorPos();
            ImGui::SameLine();
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.7);
            if ( new_media_mode == MEDIA_FOLDER ) {
                if (ImGuiToolkit::IconButton( ICON_FA_FOLDER_MINUS, "Discard folder")) {
                    Settings::application.recentImportFolders.filenames.remove(Settings::application.recentImportFolders.path);
                    if (Settings::application.recentImportFolders.filenames.empty())
                        // revert mode RECENT
                        setNewMedia(MEDIA_RECENT);
                    else
                        setNewMedia(MEDIA_FOLDER, Settings::application.recentImportFolders.filenames.front());
                }
            }
            else if ( new_media_mode == MEDIA_RECORDING ) {
                if (ImGuiToolkit::IconButton( ICON_FA_BACKSPACE, "Clear list")) {
                    Settings::application.recentRecordings.filenames.clear();
                    Settings::application.recentRecordings.front_is_valid = false;
                    setNewMedia(MEDIA_RECORDING);
                }
            }
            else if ( new_media_mode == MEDIA_RECENT ) {
                if (ImGuiToolkit::IconButton( ICON_FA_BACKSPACE, "Clear list")) {
                    Settings::application.recentImport.filenames.clear();
                    Settings::application.recentImport.front_is_valid = false;
                    setNewMedia(MEDIA_RECENT);
                }
            }
            ImGui::PopStyleVar();
            ImGui::SetCursorPos(pos_top);

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
                    sourceMediaFiles = SystemToolkit::list_directory( Settings::application.recentImportFolders.path, { MEDIA_FILES_PATTERN });
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

            if (new_media_mode == MEDIA_RECORDING) {
                // Bottom Right side of the list: helper and options
                ImVec2 pos_bot = ImGui::GetCursorPos();
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
                // come back...
                ImGui::SetCursorPos(pos_bot);
            }

        }
        // Folder Source creator
        else if (Settings::application.source.new_type == SOURCE_SEQUENCE){

            static DialogToolkit::MultipleImagesDialog _selectImagesDialog("Select multiple images");
            static MultiFileSequence _numbered_sequence;
            static MultiFileRecorder _video_recorder;
            static int _fps = 25;

            // clic button to load file
            if ( ImGui::Button( ICON_FA_FOLDER_OPEN " Open images", ImVec2(ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN, 0)) ) {
                sourceSequenceFiles.clear();
                new_source_preview_.setSource();
                _selectImagesDialog.open();
            }

            // Indication
            ImGui::SameLine();
            ImGuiToolkit::HelpToolTip("Create a source displaying a sequence of images (PNG, JPG, TIF);\n"
                                     ICON_FA_CARET_RIGHT " files numbered consecutively\n"
                                     ICON_FA_CARET_RIGHT " create a video from many images\n");

            // return from thread for folder openning
            if (_selectImagesDialog.closed()) {
                // clear
                new_source_preview_.setSource();
                // store list of files from dialog
                sourceSequenceFiles = _selectImagesDialog.images();
                if (sourceSequenceFiles.empty())
                    Log::Notify("No file selected.");

                // set sequence
                _numbered_sequence = MultiFileSequence(sourceSequenceFiles);

                // automatically create a MultiFile Source if possible
                if (_numbered_sequence.valid()) {
                    std::string label = BaseToolkit::transliterate( BaseToolkit::common_pattern(sourceSequenceFiles) );
                    new_source_preview_.setSource( Mixer::manager().createSourceMultifile(sourceSequenceFiles, _fps), label);
                }
            }

            // multiple files selected
            if (sourceSequenceFiles.size() > 1) {

                ImGui::Text("\nCreate image sequence:");

                // show info sequence
                ImGuiTextBuffer info;
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.14f, 0.9f));
                info.appendf("%d %s", (int) sourceSequenceFiles.size(), _numbered_sequence.codec.c_str());
                ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                ImGui::InputText("Images", (char *)info.c_str(), info.size(), ImGuiInputTextFlags_ReadOnly);
                info.clear();
                if (_numbered_sequence.location.empty())
                    info.append("Not consecutively numbered");
                else
                    info.appendf("%s", SystemToolkit::base_filename(_numbered_sequence.location).c_str());
                ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                ImGui::InputText("Filenames", (char *)info.c_str(), info.size(), ImGuiInputTextFlags_ReadOnly);
                ImGui::PopStyleColor(1);

                // offer to open file browser at location
                std::string path = SystemToolkit::path_filename(sourceSequenceFiles.front());
                std::string label = BaseToolkit::truncated(path, 25);
                label = BaseToolkit::transliterate(label);
                ImGuiToolkit::ButtonOpenUrl( label.c_str(), path.c_str(), ImVec2(IMGUI_RIGHT_ALIGN, 0) );
                ImGui::SameLine(0, IMGUI_SAME_LINE);
                ImGui::Text("Folder");

                // set framerate
                ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                ImGui::SliderInt("Framerate", &_fps, 1, 30, "%d fps");
                if (ImGui::IsItemDeactivatedAfterEdit()){
                    if (new_source_preview_.filled()) {
                        std::string label = BaseToolkit::transliterate( BaseToolkit::common_pattern(sourceSequenceFiles) );
                        new_source_preview_.setSource( Mixer::manager().createSourceMultifile(sourceSequenceFiles, _fps), label);
                    }
                }

                ImGui::Spacing();

                // Offer to create video from sequence
                if ( ImGui::Button( ICON_FA_FILM " Make a video", ImVec2(ImGui::GetContentRegionAvail().x, 0)) ) {
                    // start video recorder
                    _video_recorder.setFiles( sourceSequenceFiles );
                    _video_recorder.setFramerate( _fps );
                    _video_recorder.setProfile( (VideoRecorder::Profile) Settings::application.record.profile );
                    _video_recorder.start();
                    // dialog
                    ImGui::OpenPopup(LABEL_VIDEO_SEQUENCE);
                }

                // video recorder finished: inform and open pannel to import video source from recent recordings
                if ( _video_recorder.finished() ) {
                    // video recorder failed if it does not return a valid filename
                    if ( _video_recorder.filename().empty() )
                        Log::Warning("Failed to generate an image sequence.");
                    else {
                        Log::Notify("Image sequence saved to %s.", _video_recorder.filename().c_str());
                        // open the file as new recording
//                        if (Settings::application.recentRecordings.load_at_start)
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
                    ImGui::Text("%ld / %ld", _video_recorder.numFrames(), _video_recorder.files().size() );

                    ImGui::Spacing();
                    ImGui::ProgressBar(_video_recorder.progress());

                    ImGui::Spacing();
                    if (ImGui::Button("Cancel"))
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
        // Internal Source creator
        else if (Settings::application.source.new_type == SOURCE_INTERNAL){

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
                    label = std::string("Source ") + (*iter)->initials() + " - " + (*iter)->name();
                    if (ImGui::Selectable( label.c_str() )) {
                        label = std::string("Clone of ") + label;
                        new_source_preview_.setSource( Mixer::manager().createSourceClone((*iter)->name(), false),label);
                    }
                }
                ImGui::EndCombo();
            }

            // Indication
            ImGui::SameLine();
            ImGuiToolkit::HelpToolTip("Create a source replicating internal vimix objects.\n"
                                     ICON_FA_CARET_RIGHT " Loopback from output\n"
                                     ICON_FA_CARET_RIGHT " Clone other sources");
        }
        // Generated Source creator
        else if (Settings::application.source.new_type == SOURCE_GENERATED){

            bool update_new_source = false;

            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            if (ImGui::BeginCombo("##Pattern", "Select generator", ImGuiComboFlags_HeightLarge))
            {
                if ( ImGui::Selectable("Custom " ICON_FA_CARET_RIGHT) ) {
                    update_new_source = true;
                    custom_pipeline = true;
                    pattern_type = -1;
                }
                for (int p = 0; p < (int) Pattern::count(); ++p){
                    pattern_descriptor pattern = Pattern::get(p);
                    std::string label = pattern.label + (pattern.animated ? " " ICON_FA_CARET_RIGHT : " ");
                    if (pattern.available && ImGui::Selectable( label.c_str(), p == pattern_type )) {
                        update_new_source = true;
                        custom_pipeline = false;
                        pattern_type = p;
                    }
                }
                ImGui::EndCombo();
            }

            // Indication
            ImGui::SameLine();
            ImGuiToolkit::HelpToolTip("Create a source with graphics generated algorithmically.");

            if (custom_pipeline) {
                static std::vector< std::pair< std::string, std::string> > _examples = { {"Videotest", "videotestsrc horizontal-speed=1 " },
                                                                                         {"Checker", "videotestsrc pattern=checkers-8 ! video/x-raw, width=64, height=64 "},
                                                                                         {"Color", "videotestsrc pattern=gradient foreground-color= 0xff55f54f background-color= 0x000000 "},
                                                                                         {"Text", "videotestsrc pattern=black ! textoverlay text=\"vimix\" halignment=center valignment=center font-desc=\"Sans,72\" "},
                                                                                         {"GStreamer Webcam", "udpsrc port=5000 buffer-size=200000 ! h264parse ! avdec_h264 "},
                                                                                         {"SRT listener", "srtsrc uri=\"srt://:5000?mode=listener\" ! decodebin "}
                                                                                       };
                static std::string _description = _examples[0].second;
                static ImVec2 fieldsize(ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN, 100);
                static int numlines = 0;
                const ImGuiContext& g = *GImGui;
                fieldsize.y = MAX(3, numlines) * g.FontSize + g.Style.ItemSpacing.y + g.Style.FramePadding.y;

                // Editor
                if ( ImGuiToolkit::InputCodeMultiline("Pipeline", &_description, fieldsize, &numlines) )
                    update_new_source = true;

                // Local menu for list of examples
                ImVec2 pos_bot = ImGui::GetCursorPos();
                ImGui::SetCursorPos( pos_bot + ImVec2(fieldsize.x + IMGUI_SAME_LINE, -ImGui::GetFrameHeightWithSpacing()));
                if (ImGui::BeginCombo("##Examples", "Examples", ImGuiComboFlags_NoPreview))  {
                    ImGui::TextDisabled("Gstreamer examples");
                    ImGui::Separator();
                    for (auto it = _examples.begin(); it != _examples.end(); ++it) {
                        if (ImGui::Selectable( it->first.c_str() ) ) {
                            _description = it->second;
                            update_new_source = true;
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::SetCursorPos(pos_bot);
                // take action
                if (update_new_source)
                    new_source_preview_.setSource( Mixer::manager().createSourceStream(_description), "Custom");

            }
            // if pattern selected
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
        // External source creator
        else if (Settings::application.source.new_type == SOURCE_CONNECTED){

            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            if (ImGui::BeginCombo("##External", "Select stream"))
            {
                for (int d = 0; d < Device::manager().numDevices(); ++d){
                    std::string namedev = Device::manager().name(d);
                    if (ImGui::Selectable( namedev.c_str() )) {
                        custom_connected = false;
                        new_source_preview_.setSource( Mixer::manager().createSourceDevice(namedev), namedev);
                    }
                }
                for (int d = 1; d < Connection::manager().numHosts(); ++d){
                    std::string namehost = Connection::manager().info(d).name;
                    if (ImGui::Selectable( namehost.c_str() )) {
                        custom_connected = false;
                        new_source_preview_.setSource( Mixer::manager().createSourceNetwork(namehost), namehost);
                    }
                }

                if ( ImGui::Selectable("SRT Broadcaster") ) {
                    new_source_preview_.setSource();
                    custom_connected = true;
                }

                ImGui::EndCombo();
            }

            // Indication
            ImGui::SameLine();
            ImVec2 pos = ImGui::GetCursorPos();
            ImGuiToolkit::HelpToolTip("Create a source capturing video streams from connected devices or machines;\n"
                                     ICON_FA_CARET_RIGHT " webcams or frame grabbers\n"
                                     ICON_FA_CARET_RIGHT " screen capture\n"
                                     ICON_FA_CARET_RIGHT " shared by vimix on local network\n"
                                     ICON_FA_CARET_RIGHT " broadcasted with SRT over network.");

            if (custom_connected) {

                bool valid_ = false;
                static std::string url_;
                static std::string ip_ = Settings::application.recentSRT.hosts.empty() ? Settings::application.recentSRT.default_host.first : Settings::application.recentSRT.hosts.front().first;
                static std::string port_ = Settings::application.recentSRT.hosts.empty() ? Settings::application.recentSRT.default_host.second : Settings::application.recentSRT.hosts.front().second;
                static std::regex ipv4("(([0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])\\.){3}([0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])");
                static std::regex numport("([0-9]){4,6}");

                ImGui::Text("\nCall an SRT broadcaster:");
                ImGui::SetCursorPos(pos + ImVec2(0.f, 1.8f * ImGui::GetFrameHeight()) );
                ImGuiToolkit::Indication("Set the IP and Port for connecting with Secure Reliable Transport (SRT) protocol to a video broadcaster that is waiting for connections (listener mode).", ICON_FA_GLOBE);

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
                    if (ImGuiToolkit::IconButton( ICON_FA_BACKSPACE, "Clear list")) {
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
    static int selection_session_mode = (Settings::application.recentFolders.path == IMGUI_LABEL_RECENT_FILES) ? 0 : 1;
    static DialogToolkit::OpenFolderDialog customFolder("Open Folder");

    // Show combo box of quick selection modes
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    if (ImGui::BeginCombo("##SelectionSession", BaseToolkit::truncated(Settings::application.recentFolders.path, 25).c_str() )) {

        // Mode 0 : recent files
        if (ImGui::Selectable( ICON_FA_LIST_OL IMGUI_LABEL_RECENT_FILES) ) {
             Settings::application.recentFolders.path = IMGUI_LABEL_RECENT_FILES;
             selection_session_mode = 0;
             selection_session_mode_changed = true;
        }
        // Mode 1 : known folders
        for(auto foldername = Settings::application.recentFolders.filenames.begin();
            foldername != Settings::application.recentFolders.filenames.end(); foldername++) {
            std::string f = std::string(ICON_FA_FOLDER) + " " + BaseToolkit::truncated( *foldername, 40);
            if (ImGui::Selectable( f.c_str() )) {
                // remember which path was selected
                Settings::application.recentFolders.path.assign(*foldername);
                // set mode
                selection_session_mode = 1;
                selection_session_mode_changed = true;
            }
        }
        // Add a folder
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
        if (ImGuiToolkit::IconButton( ICON_FA_BACKSPACE, "Clear list")) {
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
    static std::list<std::string>::iterator _file_over = sessions_list.end();
    static std::list<std::string>::iterator _displayed_over = sessions_list.end();

    // change session list if changed
    if (selection_session_mode_changed || Settings::application.recentSessions.changed || Settings::application.recentFolders.changed) {

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
            sessions_list = SystemToolkit::list_directory( Settings::application.recentFolders.path, { VIMIX_FILE_PATTERN });
        }
        // indicate the list changed (do not change at every frame)
        selection_session_mode_changed = false;
        _file_over = sessions_list.end();
        _displayed_over = sessions_list.end();
    }

    {
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
                        if (Settings::application.smooth_transition)
                            WorkspaceWindow::clearWorkspace();
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
        if (Settings::application.smooth_transition)
            WorkspaceWindow::clearWorkspace();
        hidePannel();
    }
    if (ImGui::IsItemHovered())
        ImGuiToolkit::ToolTip("New session", SHORTCUT_NEW_FILE);

    ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_bot.y - 2.f * ImGui::GetFrameHeightWithSpacing()));
    ImGuiToolkit::HelpToolTip("Select the history of recently opened files or a folder. "
                             "Double-clic on a filename to open the session.\n\n"
                             ICON_FA_ARROW_CIRCLE_RIGHT "  Smooth transition "
                             "performs cross fading to the opened session.");
    // toggle button for smooth transition
    ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_bot.y - ImGui::GetFrameHeightWithSpacing()) );
    ImGuiToolkit::ButtonToggle(ICON_FA_ARROW_CIRCLE_RIGHT, &Settings::application.smooth_transition, "Smooth transition");
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
            glm::ivec2 preset = RenderView::presetFromResolution(output->resolution());
            glm::ivec2 custom = glm::ivec2(output->resolution());
            if (preset.x > -1) {
                // cannot change resolution when recording
                if ( UserInterface::manager().outputcontrol.isRecording() ) {
                    // show static info (same size than combo)
                    static char dummy_str[512];
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.14f, 0.9f));
                    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                    sprintf(dummy_str, "%s", RenderView::ratio_preset_name[preset.x]);
                    ImGui::InputText("Ratio", dummy_str, IM_ARRAYSIZE(dummy_str), ImGuiInputTextFlags_ReadOnly);
                    if (preset.x < RenderView::AspectRatio_Custom) {
                        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                        sprintf(dummy_str, "%s", RenderView::height_preset_name[preset.y]);
                        ImGui::InputText("Height", dummy_str, IM_ARRAYSIZE(dummy_str), ImGuiInputTextFlags_ReadOnly);
                    } else {
                        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                        sprintf(dummy_str, "%d", custom.x);
                        ImGui::InputText("Width", dummy_str, IM_ARRAYSIZE(dummy_str), ImGuiInputTextFlags_ReadOnly);
                        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                        sprintf(dummy_str, "%d", custom.y);
                        ImGui::InputText("Height", dummy_str, IM_ARRAYSIZE(dummy_str), ImGuiInputTextFlags_ReadOnly);
                    }
                    ImGui::PopStyleColor(1);
                }
                // offer to change filename, ratio and resolution
                else {
                    ImVec2 draw_pos = ImGui::GetCursorScreenPos();
                    ImGui::SameLine();
                    if ( ImGuiToolkit::IconButton(ICON_FA_FILE_DOWNLOAD, "Save as" )) {
                        UserInterface::manager().selectSaveFilename();
                    }
                    ImGui::SetCursorScreenPos(draw_pos);
                    // combo boxes to select aspect rario
                    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                    if (ImGui::Combo("Ratio", &preset.x, RenderView::ratio_preset_name, IM_ARRAYSIZE(RenderView::ratio_preset_name) ) )
                    {
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
                        if (ImGui::Combo("Height", &preset.y, RenderView::height_preset_name, IM_ARRAYSIZE(RenderView::height_preset_name) ) )
                        {
                            glm::vec3 res = RenderView::resolutionFromPreset(preset.x, preset.y);
                            Mixer::manager().setResolution(res);
                        }
                    }
                    //  - custom aspect ratio : input width and height
                    else {
                        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);                        
                        ImGui::InputInt("Width", &custom.x, 100, 500);
                        if (ImGui::IsItemDeactivatedAfterEdit())
                            Mixer::manager().setResolution( glm::vec3(custom, 0.f));
                        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                        ImGui::InputInt("Height", &custom.y, 100, 500);
                        if (ImGui::IsItemDeactivatedAfterEdit())
                            Mixer::manager().setResolution( glm::vec3(custom, 0.f));

                    }
                }
            }
        }

        // the session file exists
        if (!sessionfilename.empty())
        {
            // Folder
            std::string path = SystemToolkit::path_filename(sessionfilename);
            std::string label = BaseToolkit::truncated(path, 23);
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
        if (ImGuiToolkit::IconButton( ICON_FA_BACKSPACE, "Clear history")) {
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
        ImGuiToolkit::ButtonToggle(ICON_FA_MAP_MARKED_ALT, &Settings::application.action_history_follow_view, "Show in view");
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
                    ImGui::Text("%s", _snap_date.c_str());
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
        ImGuiToolkit::HelpToolTip("Previous versions of the session (latest on top). "
                                 "Double-clic on a version to restore it.\n\n"
                                 ICON_FA_CODE_BRANCH "  Iterative saving automatically "
                                 "keeps a version each time a session is saved.");
        // toggle button for versioning
        ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_bot.y - ImGui::GetFrameHeightWithSpacing()) );
        ImGuiToolkit::ButtonToggle(" " ICON_FA_CODE_BRANCH " ", &Settings::application.save_version_snapshot,"Iterative saving");

        ImGui::SetCursorPos( pos_bot );
    }

    //
    // Buttons to show WINDOWS
    //
    ImGuiToolkit::Spacing();
    ImGui::Text("Windows");
    ImGui::Spacing();

    std::pair<std::string, std::string> tooltip_;

    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);

    ImGui::SameLine(0, 0.5f * ImGui::GetTextLineHeight());
    if ( ImGuiToolkit::IconButton( ICON_FA_LAPTOP ) )
        UserInterface::manager().outputcontrol.setVisible(!Settings::application.widget.preview);
    if (ImGui::IsItemHovered())
        tooltip_ = { TOOLTIP_OUTPUT, SHORTCUT_OUTPUT};

    ImGui::SameLine(0, ImGui::GetTextLineHeight());
    if ( ImGuiToolkit::IconButton( ICON_FA_PLAY_CIRCLE ) )
        UserInterface::manager().sourcecontrol.setVisible(!Settings::application.widget.media_player);
    if (ImGui::IsItemHovered())
        tooltip_ = { TOOLTIP_PLAYER, SHORTCUT_PLAYER};

    ImGui::SameLine(0, ImGui::GetTextLineHeight());
    if ( ImGuiToolkit::IconButton( ICON_FA_CLOCK ) )
        UserInterface::manager().timercontrol.setVisible(!Settings::application.widget.timer);
    if (ImGui::IsItemHovered())
        tooltip_ = { TOOLTIP_TIMER, SHORTCUT_TIMER};

    ImGui::SameLine(0, ImGui::GetTextLineHeight());
    if ( ImGuiToolkit::IconButton( ICON_FA_HAND_PAPER ) )
        UserInterface::manager().inputscontrol.setVisible(!Settings::application.widget.inputs);
    if (ImGui::IsItemHovered())
        tooltip_ = { TOOLTIP_INPUTS, SHORTCUT_INPUTS};

    ImGui::SameLine(0, ImGui::GetTextLineHeight());
    if ( ImGuiToolkit::IconButton( ICON_FA_STICKY_NOTE ) )
        Mixer::manager().session()->addNote();
    if (ImGui::IsItemHovered())
        tooltip_ = { TOOLTIP_NOTE, SHORTCUT_NOTE};

    ImGui::PopFont();

    if (!tooltip_.first.empty()) {
        ImGuiToolkit::ToolTip(tooltip_.first.c_str(), tooltip_.second.c_str());
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
        ImGui::Text("Interface");
        int v = Settings::application.accent_color;
        if (ImGui::RadioButton("##Color", &v, v)){
            Settings::application.accent_color = (v+1)%3;
            ImGuiToolkit::SetAccentColor(static_cast<ImGuiToolkit::accent_color>(Settings::application.accent_color));
        }
        if (ImGui::IsItemHovered())
            ImGuiToolkit::ToolTip("Change accent color");
        ImGui::SameLine();
        ImGui::SetCursorPosX(-1.f * IMGUI_RIGHT_ALIGN);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if ( ImGui::DragFloat("Scale", &Settings::application.scale, 0.01f, 0.5f, 2.0f, "%.1f"))
            ImGui::GetIO().FontGlobalScale = Settings::application.scale;

        ImGuiToolkit::ButtonSwitch( ICON_FA_MOUSE_POINTER "  Smooth cursor", &Settings::application.smooth_cursor);

        //
        // Recording preferences
        //
        ImGui::Text("Record");

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
            char buf[256]; sprintf(buf, "Buffer can contain %ld frames (%dx%d), %.1f sec", (unsigned long)nb, output->width(), output->height(),
                                   (float)nb / (float) VideoRecorder::framerate_preset_value[Settings::application.record.framerate_mode] );
            ImGuiToolkit::Indication(buf, ICON_FA_MEMORY);
            ImGui::SameLine(0);
        }

        ImGui::SetCursorPosX(-1.f * IMGUI_RIGHT_ALIGN);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ImGui::SliderInt("Buffer", &Settings::application.record.buffering_mode, 0, IM_ARRAYSIZE(VideoRecorder::buffering_preset_name)-1,
                         VideoRecorder::buffering_preset_name[Settings::application.record.buffering_mode]);

        ImGuiToolkit::HelpToolTip("Priority when buffer is full and recorder has to skip frames;\n"
                                 ICON_FA_CARET_RIGHT " Duration:\n  Variable framerate, correct duration.\n"
                                 ICON_FA_CARET_RIGHT " Framerate:\n  Correct framerate,  shorter duration.");
        ImGui::SameLine(0);
        ImGui::SetCursorPosX(-1.f * IMGUI_RIGHT_ALIGN);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ImGui::Combo("Priority", &Settings::application.record.priority_mode, "Duration\0Framerate\0");

        //
        // Networking preferences
        //
        ImGuiToolkit::Spacing();
        ImGui::Text("Stream");

        ImGuiToolkit::Indication("Peer-to-peer sharing on local network\n\n"
                                 "vimix can stream JPEG (default) or H264 (requires less bandwidth but more resources for encoding)", ICON_FA_SHARE_ALT_SQUARE);
        ImGui::SameLine(0);
        ImGui::SetCursorPosX(-1.f * IMGUI_RIGHT_ALIGN);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ImGui::Combo("Share P2P", &Settings::application.stream_protocol, "JPEG\0H264\0");

        if (VideoBroadcast::available()) {
            char msg[256];
            ImFormatString(msg, IM_ARRAYSIZE(msg), "Broadcast SRT\n\n"
                                                   "vimix is listening to SRT requests on Port %d. "
                                                   "Example network addresses to call:\n"
                                                   " srt//%s:%d (localhost)\n"
                                                   " srt//%s:%d (local IP)",
                           Settings::application.broadcast_port,
                           NetworkToolkit::host_ips()[0].c_str(), Settings::application.broadcast_port,
                    NetworkToolkit::host_ips()[1].c_str(), Settings::application.broadcast_port );

            ImGuiToolkit::Indication(msg, ICON_FA_GLOBE);
            ImGui::SameLine(0);
            ImGui::SetCursorPosX(-1.f * IMGUI_RIGHT_ALIGN);
            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            char bufport[7] = "";
            sprintf(bufport, "%d", Settings::application.broadcast_port);
            ImGui::InputTextWithHint("Port SRT", "7070", bufport, 6, ImGuiInputTextFlags_CharsDecimal);
            if (ImGui::IsItemDeactivatedAfterEdit()){
                if ( BaseToolkit::is_a_number(bufport, &Settings::application.broadcast_port))
                    Settings::application.broadcast_port = CLAMP(Settings::application.broadcast_port, 1029, 49150);
            }
        }

        //
        // OSC preferences
        //
        ImGuiToolkit::Spacing();
        ImGui::Text("OSC");

        char msg[256];
        ImFormatString(msg, IM_ARRAYSIZE(msg), "Open Sound Control\n\n"
                                               "vimix accepts OSC messages sent by UDP on Port %d and replies on Port %d."
                                               "Example network addresses:\n"
                                               " udp//%s:%d (localhost)\n"
                                               " udp//%s:%d (local IP)",
                Settings::application.control.osc_port_receive,
                Settings::application.control.osc_port_send,
                NetworkToolkit::host_ips()[0].c_str(), Settings::application.control.osc_port_receive,
                NetworkToolkit::host_ips()[1].c_str(), Settings::application.control.osc_port_receive );
        ImGuiToolkit::Indication(msg, ICON_FA_NETWORK_WIRED);
        ImGui::SameLine(0);

        ImGui::SetCursorPosX(-1.f * IMGUI_RIGHT_ALIGN);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        char bufreceive[7] = "";
        sprintf(bufreceive, "%d", Settings::application.control.osc_port_receive);
        ImGui::InputTextWithHint("Port in", "7000", bufreceive, 7, ImGuiInputTextFlags_CharsDecimal);
        if (ImGui::IsItemDeactivatedAfterEdit()){
            if ( BaseToolkit::is_a_number(bufreceive, &Settings::application.control.osc_port_receive)){
                Settings::application.control.osc_port_receive = CLAMP(Settings::application.control.osc_port_receive, 1029, 49150);
                Control::manager().init();
            }
        }

        ImGui::SetCursorPosX(-1.f * IMGUI_RIGHT_ALIGN);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        char bufsend[7] = "";
        sprintf(bufsend, "%d", Settings::application.control.osc_port_send);
        ImGui::InputTextWithHint("Port out", "7001", bufsend, 7, ImGuiInputTextFlags_CharsDecimal);
        if (ImGui::IsItemDeactivatedAfterEdit()){
            if ( BaseToolkit::is_a_number(bufsend, &Settings::application.control.osc_port_send)){
                Settings::application.control.osc_port_send = CLAMP(Settings::application.control.osc_port_send, 1029, 49150);
                Control::manager().init();
            }
        }

        ImGui::SetCursorPosX(-1.f * IMGUI_RIGHT_ALIGN);
        const float w = IMGUI_RIGHT_ALIGN - ImGui::GetFrameHeightWithSpacing();
        ImGuiToolkit::ButtonOpenUrl( "Edit", Settings::application.control.osc_filename.c_str(), ImVec2(w, 0) );
        ImGui::SameLine(0, 6);
        if ( ImGuiToolkit::IconButton(15, 12, "Reload") )
            Control::manager().init();
        ImGui::SameLine();
        ImGui::Text("Translator");

        //
        // System preferences
        //
        ImGuiToolkit::Spacing();
//        ImGuiToolkit::HelpMarker("If you encounter some rendering issues on your machine, "
//                                 "you can try to disable some of the OpenGL optimizations below.");
//        ImGui::SameLine();
        ImGui::Text("System");

        static bool need_restart = false;
        static bool vsync = (Settings::application.render.vsync > 0);
        static bool blit = Settings::application.render.blit;
        static bool multi = (Settings::application.render.multisampling > 0);
        static bool gpu = Settings::application.render.gpu_decoding;
        bool change = false;
        change |= ImGuiToolkit::ButtonSwitch( "Blit framebuffer", &blit);
#ifndef NDEBUG
        change |= ImGuiToolkit::ButtonSwitch( "Vertical synchronization", &vsync);
        change |= ImGuiToolkit::ButtonSwitch( "Antialiasing framebuffer", &multi);
#endif
        // hardware support deserves more explanation
        ImGuiToolkit::Indication("If enabled, tries to find a platform adapted hardware-accelerated "
                                 "driver to decode (read) or encode (record) videos.", ICON_FA_MICROCHIP);
        ImGui::SameLine(0);
        if (Settings::application.render.gpu_decoding_available)
            change |= ImGuiToolkit::ButtonSwitch( "Hardware en/decoding", &gpu);
        else
            ImGui::TextDisabled("Hardware en/decoding unavailable");

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
                if (UserInterface::manager().TryClose())
                    Rendering::manager().close();
            }
        }

}

void Navigator::RenderTransitionPannel()
{
    if (Settings::application.current_view != View::TRANSITION) {
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

        // Transition options
        ImGuiToolkit::Spacing();
        ImGui::Text("Transition");
        if (ImGuiToolkit::IconButton(0, 8)) Settings::application.transition.cross_fade = true;
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        int mode = Settings::application.transition.cross_fade ? 0 : 1;
        if (ImGui::Combo("Fading", &mode, "Cross fading\0Fade to black\0") )
            Settings::application.transition.cross_fade = mode < 1;
        if (ImGuiToolkit::IconButton(4, 13)) Settings::application.transition.duration = 1.f;
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ImGui::SliderFloat("Duration", &Settings::application.transition.duration, TRANSITION_MIN_DURATION, TRANSITION_MAX_DURATION, "%.1f s");
        if (ImGuiToolkit::IconButton(9, 1)) Settings::application.transition.profile = 0;
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ImGui::Combo("Curve", &Settings::application.transition.profile, "Linear\0Quadratic\0");

        // specific transition actions
        ImGuiToolkit::Spacing();
        ImGui::Text("Actions");
        if ( ImGui::Button( ICON_FA_TIMES "  Cancel ", ImVec2(IMGUI_RIGHT_ALIGN, 0)) ){
            TransitionView *tv = static_cast<TransitionView *>(Mixer::manager().view(View::TRANSITION));
            if (tv) tv->cancel();
        }
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
        // Temporary fix for preventing horizontal scrolling (https://github.com/ocornut/imgui/issues/2915)
        ImGui::SetScrollX(0);

        //
        // Panel content depends on show_config_
        //
        if (show_config_)
            RenderMainPannelSettings();
        else
            RenderMainPannelVimix();

        //
        // Icon and About vimix
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
                ImGui::SetCursorScreenPos( rightcorner - ImVec2( (icon_height + pannel_width_) * 0.5f, icon_height + button_height + g.Style.ItemSpacing.y) );
                ImGui::Image((void*)(intptr_t)vimixicon, ImVec2(icon_height, icon_height));
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

        //
        // Settings icon (non scollable) in Bottom-right corner
        //
        ImGui::SetCursorScreenPos( rightcorner - ImVec2(button_height, button_height));
        const char *tooltip[2] = {"Settings", "Settings"};
        ImGuiToolkit::IconToggle(13,5,12,5, &show_config_, tooltip);
        // Metrics icon just above
        ImGui::SetCursorScreenPos( rightcorner - ImVec2(button_height, 2.1 * button_height));
        if ( ImGuiToolkit::IconButton( 11, 3 , TOOLTIP_METRICS) )
            Settings::application.widget.stats = !Settings::application.widget.stats;


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
    static char str[128] = "";
    ImGui::InputText("Command", str, IM_ARRAYSIZE(str));
    if ( ImGui::Button("Execute") )
        SystemToolkit::execute(str);

    static char str0[128] = "àöäüèáû вторая строчка";
    ImGui::InputText("##inputtext", str0, IM_ARRAYSIZE(str0));
    std::string tra = BaseToolkit::transliterate(std::string(str0));
    ImGui::Text("Transliteration: '%s'", tra.c_str());

//    ImGui::Separator();

//    static bool selected[25] = { true, false, false, false, false,
//                                 true, false, false, false, false,
//                                 true, false, false, true, false,
//                                 false, false, false, true, false,
//                                 false, false, false, true, false };

//    ImVec2 keyIconSize = ImVec2(60,60);

//    ImGuiContext& g = *GImGui;
//    ImVec2 itemsize = keyIconSize + g.Style.ItemSpacing;
//    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
//    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.50f));
//    ImVec2 frame_top = ImGui::GetCursorScreenPos();


////    static int key = 0;
//    static ImVec2 current(-1.f, -1.f);

//    for (int i = 0; i < 25; ++i) {
//        ImGui::PushID(i);
//        char letter[2];
//        sprintf(letter, "%c", 'A' + i);
//        if (ImGui::Selectable(letter, selected[i], 0, keyIconSize))
//        {
//            current = ImVec2(i % 5, i / 5);
////            key = GLFW_KEY_A + i;
//        }
//        ImGui::PopID();
//        if ((i % 5) < 4) ImGui::SameLine();
//    }
//    ImGui::PopStyleVar();
//    ImGui::PopFont();

//    if (current.x > -1 && current.y > -1) {
//        ImVec2 pos = frame_top + itemsize * current;
//        ImDrawList* draw_list = ImGui::GetWindowDrawList();
//        draw_list->AddRect(pos, pos + keyIconSize, ImGui::GetColorU32(ImGuiCol_Text), 6.f, ImDrawCornerFlags_All, 3.f);

//    }



    ImGui::End();
}

void ShowAboutOpengl(bool* p_open)
{
    ImGui::SetNextWindowPos(ImVec2(520, 320), ImGuiCond_FirstUseEver);
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
        bool copy_to_clipboard = ImGui::Button(MENU_COPY);
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
            bool copy_to_clipboard = ImGui::Button(MENU_COPY);
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
