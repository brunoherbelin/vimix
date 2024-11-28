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

#define PLOT_ARRAY_SIZE 180
#define WINDOW_TOOLBOX_ALPHA 0.35f
#define WINDOW_TOOLBOX_DIST_TO_BORDER 10.f

#include <iostream>
#include <sstream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <regex>

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

// generic image loader
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "defines.h"
#include "Settings.h"
#include "Log.h"
#include "SystemToolkit.h"
#include "DialogToolkit.h"
#include "BaseToolkit.h"
#include "NetworkToolkit.h"
#include "GlmToolkit.h"
#include "GstToolkit.h"
#include "ImGuiToolkit.h"
#include "ImGuiVisitor.h"
#include "ControlManager.h"
#include "ActionManager.h"
#include "Resource.h"
#include "Connection.h"
#include "SessionCreator.h"
#include "Mixer.h"
#include "Recorder.h"
#include "SourceCallback.h"
#include "MediaSource.h"
#include "PatternSource.h"
#include "DeviceSource.h"
#include "ScreenCaptureSource.h"
#include "MultiFileSource.h"
#include "ShmdataBroadcast.h"
#include "VideoBroadcast.h"
#include "MultiFileRecorder.h"
#include "MousePointer.h"
#include "Playlist.h"
#include "Audio.h"

#include "UserInterfaceManager.h"


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

class Thumbnail
{
    float aspect_ratio_;
    uint texture_;

public:
    Thumbnail();
    ~Thumbnail();

    void reset();
    void fill (const FrameBufferImage *image);
    bool filled();
    void Render(float width);
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
    show_preview = UserInterface::PREVIEW_NONE;

    sessionopendialog = nullptr;
    sessionimportdialog = nullptr;
    sessionsavedialog = nullptr;
}

bool UserInterface::Init(int font_size)
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
    ImGui_ImplOpenGL3_Init(VIMIX_GLSL_VERSION);

    // hack to change keys according to keyboard layout
    io.KeyMap[ImGuiKey_A] = Control::layoutKey(GLFW_KEY_A);
    io.KeyMap[ImGuiKey_C] = Control::layoutKey(GLFW_KEY_C);
    io.KeyMap[ImGuiKey_V] = Control::layoutKey(GLFW_KEY_V);
    io.KeyMap[ImGuiKey_X] = Control::layoutKey(GLFW_KEY_X);
    io.KeyMap[ImGuiKey_Y] = Control::layoutKey(GLFW_KEY_Y);
    io.KeyMap[ImGuiKey_Z] = Control::layoutKey(GLFW_KEY_Z);

    // Setup Dear ImGui style
    ImGuiToolkit::SetAccentColor(static_cast<ImGuiToolkit::accent_color>(Settings::application.accent_color));

    // Read font size from argument
    float base_font_size = float(font_size);
    // Estalish the base size from the resolution of the monitor for a 4mm height text
    if (base_font_size < 1)
        base_font_size = float(Rendering::manager().mainWindow().pixelsforRealHeight(4.f));
    // at least 8 pixels font size
    base_font_size = MAX(base_font_size, 8.f);
    // Load Fonts (using resource manager, NB: a temporary copy of the raw data is necessary)
    ImGuiToolkit::SetFont(ImGuiToolkit::FONT_DEFAULT, "Roboto-Regular", int(base_font_size) );
    ImGuiToolkit::SetFont(ImGuiToolkit::FONT_BOLD, "Roboto-Bold", int(base_font_size) + 1 );
    ImGuiToolkit::SetFont(ImGuiToolkit::FONT_ITALIC, "Roboto-Italic", int(base_font_size) + 1 );
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
    std::snprintf(inifilepath, 2048, "%s", inifile.c_str() );
    io.IniFilename = inifilepath;

    // load favorites
    favorites.load( SystemToolkit::full_filename(SystemToolkit::settings_path(), "favorites.lix") );
    playlists_path = SystemToolkit::full_filename(SystemToolkit::settings_path(), "playlists");
    if ( !SystemToolkit::file_exists(playlists_path)) {
        if ( !SystemToolkit::create_directory(playlists_path) )
            playlists_path = SystemToolkit::home_path();
    }

    // init dialogs
    sessionopendialog   = new DialogToolkit::OpenFileDialog("Open Session",
                                                          VIMIX_FILE_TYPE, VIMIX_FILE_PATTERN);
    sessionsavedialog   = new DialogToolkit::SaveFileDialog("Save Session",
                                                          VIMIX_FILE_TYPE, VIMIX_FILE_PATTERN);
    sessionimportdialog = new DialogToolkit::OpenFileDialog("Import Sources",
                                                            VIMIX_FILE_TYPE, VIMIX_FILE_PATTERN);
    settingsexportdialog = new DialogToolkit::SaveFileDialog("Export settings",
                                                             SETTINGS_FILE_TYPE, SETTINGS_FILE_PATTERN);

    // init tooltips
    ImGuiToolkit::setToolTipsEnabled(Settings::application.show_tooptips);

    // show about dialog on first run
    show_vimix_about = (Settings::application.total_runtime < 1);

    return true;
}

uint64_t UserInterface::Runtime() const
{
    return gst_util_get_timestamp () - start_time;
}


void UserInterface::setView(View::Mode mode)
{
    Mixer::manager().setView(mode);
    navigator.discardPannel();
}

void UserInterface::handleKeyboard()
{
    static bool esc_repeat_ = false;

    const ImGuiIO& io = ImGui::GetIO();
    alt_modifier_active = io.KeyAlt;
    shift_modifier_active = io.KeyShift;
    ctrl_modifier_active = io.ConfigMacOSXBehaviors ? io.KeySuper : io.KeyCtrl;
    keyboard_available = !io.WantCaptureKeyboard;

    // do not capture keys if keyboard is used (e.g. for entering text field)
    if (io.WantCaptureKeyboard || io.WantTextInput)
        return;

    // Application "CTRL +"" Shortcuts
    if ( ctrl_modifier_active ) {

        if (ImGui::IsKeyPressed( Control::layoutKey(GLFW_KEY_Q), false ))  {
            // try quit
            if ( TryClose() )
                Rendering::manager().close();
        }
        else if (ImGui::IsKeyPressed( Control::layoutKey(GLFW_KEY_F), false )) {
            Rendering::manager().mainWindow().toggleFullscreen();
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
        else if (ImGui::IsKeyPressed( Control::layoutKey(GLFW_KEY_B), false )) {
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
            Settings::application.widget.help = true;
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
            // toggle recording stop / start (or save and continue if + SHIFT modifier)
            outputcontrol.ToggleRecord(shift_modifier_active);
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_SPACE, false )) {
            // toggle pause recorder
            outputcontrol.ToggleRecordPause();
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
        else if (ImGui::IsKeyPressed( Control::layoutKey(GLFW_KEY_I), false )) {
            Settings::application.widget.inputs = !Settings::application.widget.inputs;
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_0 ))
            Mixer::selection().toggle( Mixer::manager().sourceAtIndex( 0 ) );
        else if (ImGui::IsKeyPressed( GLFW_KEY_1 ))
            Mixer::selection().toggle( Mixer::manager().sourceAtIndex( 1 ) );
        else if (ImGui::IsKeyPressed( GLFW_KEY_2 ))
            Mixer::selection().toggle( Mixer::manager().sourceAtIndex( 2 ) );
        else if (ImGui::IsKeyPressed( GLFW_KEY_3 ))
            Mixer::selection().toggle( Mixer::manager().sourceAtIndex( 3 ) );
        else if (ImGui::IsKeyPressed( GLFW_KEY_4 ))
            Mixer::selection().toggle( Mixer::manager().sourceAtIndex( 4 ) );
        else if (ImGui::IsKeyPressed( GLFW_KEY_5 ))
            Mixer::selection().toggle( Mixer::manager().sourceAtIndex( 5 ) );
        else if (ImGui::IsKeyPressed( GLFW_KEY_6 ))
            Mixer::selection().toggle( Mixer::manager().sourceAtIndex( 6 ) );
        else if (ImGui::IsKeyPressed( GLFW_KEY_7 ))
            Mixer::selection().toggle( Mixer::manager().sourceAtIndex( 7 ) );
        else if (ImGui::IsKeyPressed( GLFW_KEY_8 ))
            Mixer::selection().toggle( Mixer::manager().sourceAtIndex( 8 ) );
        else if (ImGui::IsKeyPressed( GLFW_KEY_9 ))
            Mixer::selection().toggle( Mixer::manager().sourceAtIndex( 9 ) );

    }
    // No CTRL modifier
    else {

        // Application F-Keys
        if (ImGui::IsKeyPressed( GLFW_KEY_F1, false ))
            setView(View::MIXING);
        else if (ImGui::IsKeyPressed( GLFW_KEY_F2, false ))
            setView(View::GEOMETRY);
        else if (ImGui::IsKeyPressed( GLFW_KEY_F3, false ))
            setView(View::LAYER);
        else if (ImGui::IsKeyPressed( GLFW_KEY_F4, false ))
            setView(View::TEXTURE);
        else if (ImGui::IsKeyPressed( GLFW_KEY_F5, false ))
            setView(View::DISPLAYS);
        else if (ImGui::IsKeyPressed( GLFW_KEY_F6,  false ))
            show_preview = PREVIEW_OUTPUT;
        else if (ImGui::IsKeyPressed( GLFW_KEY_F7,  false ))
            show_preview = PREVIEW_SOURCE;
        else if (ImGui::IsKeyPressed( GLFW_KEY_F9, false ))
            StartScreenshot();
        else if (ImGui::IsKeyPressed( GLFW_KEY_F10, false ))
            sourcecontrol.Capture();
        else if (ImGui::IsKeyPressed( GLFW_KEY_F11, false ))
            FrameGrabbing::manager().add(new PNGRecorder(SystemToolkit::base_filename( Mixer::manager().session()->filename())));
        else if (ImGui::IsKeyPressed( GLFW_KEY_F12, false )) {
            Settings::application.render.disabled = !Settings::application.render.disabled;
        }
        // button home to toggle panel
        else if (ImGui::IsKeyPressed( GLFW_KEY_HOME, false ))
            navigator.togglePannelAutoHide();
        // button home to toggle menu
        else if (ImGui::IsKeyPressed( GLFW_KEY_INSERT, false ))
            navigator.togglePannelNew();
        // button esc : react to press and to release
        else if (ImGui::IsKeyPressed( GLFW_KEY_ESCAPE, false )) {
            // hide pannel
            navigator.discardPannel();
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
        else if (ImGui::IsKeyPressed( GLFW_KEY_0 ))
            setSourceInPanel( 0 );
        else if (ImGui::IsKeyPressed( GLFW_KEY_1 ))
            setSourceInPanel( 1 );
        else if (ImGui::IsKeyPressed( GLFW_KEY_2 ))
            setSourceInPanel( 2 );
        else if (ImGui::IsKeyPressed( GLFW_KEY_3 ))
            setSourceInPanel( 3 );
        else if (ImGui::IsKeyPressed( GLFW_KEY_4 ))
            setSourceInPanel( 4 );
        else if (ImGui::IsKeyPressed( GLFW_KEY_5 ))
            setSourceInPanel( 5 );
        else if (ImGui::IsKeyPressed( GLFW_KEY_6 ))
            setSourceInPanel( 6 );
        else if (ImGui::IsKeyPressed( GLFW_KEY_7 ))
            setSourceInPanel( 7 );
        else if (ImGui::IsKeyPressed( GLFW_KEY_8 ))
            setSourceInPanel( 8 );
        else if (ImGui::IsKeyPressed( GLFW_KEY_9 ))
            setSourceInPanel( 9 );
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
            delta.x += (int) ImGui::IsKeyDown( GLFW_KEY_RIGHT ) - (int) ImGui::IsKeyDown( GLFW_KEY_LEFT );
            delta.y += (int) ImGui::IsKeyDown( GLFW_KEY_DOWN )  - (int) ImGui::IsKeyDown( GLFW_KEY_UP );
            Mixer::manager().view()->arrow( delta );
        }
        else if ( ImGui::IsKeyReleased( GLFW_KEY_LEFT  ) ||
                  ImGui::IsKeyReleased( GLFW_KEY_RIGHT ) ||
                  ImGui::IsKeyReleased( GLFW_KEY_UP    ) ||
                  ImGui::IsKeyReleased( GLFW_KEY_DOWN  ) ){
            Mixer::manager().view()->terminate(true);
            MousePointer::manager().active()->terminate();
        }
    }

    // special case: CTRL + TAB is ALT + TAB in OSX
    if (io.ConfigMacOSXBehaviors ? io.KeyAlt : io.KeyCtrl) {
        if (ImGui::IsKeyPressed( GLFW_KEY_TAB, false ))
            show_view_navigator += shift_modifier_active ? 5 : 1;
    }
    else if (show_view_navigator > 0) {
        show_view_navigator  = 0;
        Mixer::manager().setView((View::Mode) target_view_navigator);
    }

}

void UserInterface::handleMouse()
{
    ImGuiIO& io = ImGui::GetIO();

    // get mouse coordinates and prevent invalid values
    static glm::vec2 _prev_mousepos = glm::vec2(0.f);
    glm::vec2 mousepos = _prev_mousepos;
    if (io.MousePos.x > -1 && io.MousePos.y > -1) {
        mousepos =  glm::vec2 (io.MousePos.x * io.DisplayFramebufferScale.x, io.MousePos.y * io.DisplayFramebufferScale.y);
        mousepos = glm::clamp(mousepos, glm::vec2(0.f), glm::vec2(io.DisplaySize.x * io.DisplayFramebufferScale.x, io.DisplaySize.y * io.DisplayFramebufferScale.y));
        _prev_mousepos = mousepos;
    }

    static glm::vec2 mouseclic[2];
    mouseclic[ImGuiMouseButton_Left] = glm::vec2(io.MouseClickedPos[ImGuiMouseButton_Left].x * io.DisplayFramebufferScale.y, io.MouseClickedPos[ImGuiMouseButton_Left].y* io.DisplayFramebufferScale.x);
    mouseclic[ImGuiMouseButton_Right] = glm::vec2(io.MouseClickedPos[ImGuiMouseButton_Right].x * io.DisplayFramebufferScale.y, io.MouseClickedPos[ImGuiMouseButton_Right].y* io.DisplayFramebufferScale.x);

    static bool mousedown = false;
    static View *view_drag = nullptr;
    static std::pair<Node *, glm::vec2> picked = {nullptr, glm::vec2(0.f)};

    // allow toggle ALT to enable / disable mouse pointer
    static bool _was_alt = false;
    if (_was_alt != alt_modifier_active) {
        // remember to toggle
        _was_alt = alt_modifier_active;
        // simulate mouse release (mouse down will be re-activated)
        mousedown = false;
        // cancel active mouse pointer
        MousePointer::manager().active()->terminate();
        MousePointer::manager().setActiveMode( Pointer::POINTER_DEFAULT );
    }

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
            navigator.discardPannel();
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

                // initiate Mouse pointer from position at mouse down event
                if (alt_modifier_active || Settings::application.mouse_pointer_lock) {
                    MousePointer::manager().setActiveMode( (Pointer::Mode) Settings::application.mouse_pointer );
                    MousePointer::manager().active()->setStrength( Settings::application.mouse_pointer_strength[Settings::application.mouse_pointer] );
                }
                else
                    MousePointer::manager().setActiveMode( Pointer::POINTER_DEFAULT );

                // ask the view what was picked
                picked = Mixer::manager().view()->pick(mousepos);

                bool clear_selection = false;
                // if nothing picked,
                if ( picked.first == nullptr ) {
                    clear_selection = true;
                }
                // something was picked
                else {
                    // initiate the pointer effect
                    MousePointer::manager().active()->initiate(mousepos);

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
                        if (navigator.pannelVisible() &&
                                navigator.selectedPannelSource() < NAV_MAX)
                            navigator.showPannelSource( Mixer::manager().indexCurrentSource() );

                        // indicate to view that an action can be initiated (e.g. grab)
                        Mixer::manager().view()->initiate();
                    }
                    // no source is selected
                    else {
                        // unset current
                        Mixer::manager().unsetCurrentSource();
                        navigator.discardPannel();
                    }
                }
                if (clear_selection) {
                    // unset current
                    Mixer::manager().unsetCurrentSource();
                    navigator.discardPannel();
                    // clear selection
                    Mixer::selection().clear();
                }
            }
        }

        if ( ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) )
        {
            // if double clic event was not used in view
            if ( !Mixer::manager().view()->doubleclic(mousepos) ) {
                int i = Mixer::manager().indexCurrentSource();
                // if no current source
                if (i<0){
                    // hide left pannel & toggle clear workspace
                    navigator.discardPannel();
                    WorkspaceWindow::toggleClearRestoreWorkspace();
                }
                else
                    // display current source in left panel /or/ hide left panel if no current source
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

                    // Apply Mouse pointer filter
                    // + scrollwheel changes strength
                    if ( io.MouseWheel != 0) {
                        MousePointer::manager().active()->incrementStrength(0.1 * io.MouseWheel);
                        Settings::application.mouse_pointer_strength[Settings::application.mouse_pointer] = MousePointer::manager().active()->strength();
                    }
                    MousePointer::manager().active()->update(mousepos, 1.f / ( MAX(io.Framerate, 1.f) ));

                    // action on current source
                    View::Cursor c;
                    Source *current = Mixer::manager().currentSource();
                    if (current)
                    {
                        // grab current sources
                        c = Mixer::manager().view()->grab(current, mouseclic[ImGuiMouseButton_Left],
                                                          MousePointer::manager().active()->target(), picked);
                    }
                    // action on other (non-source) elements in the view
                    else
                    {
                        // grab picked object
                        c = Mixer::manager().view()->grab(nullptr, mouseclic[ImGuiMouseButton_Left],
                                                          MousePointer::manager().active()->target(), picked);
                    }

                    // Set cursor appearance
                    SetMouseCursor(io.MousePos, c);

                    // Draw Mouse pointer effect
                    MousePointer::manager().active()->draw();

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
        //
        // Mouse wheel over background without source action
        //
        else if ( !mousedown && io.MouseWheel != 0) {
            // scroll => zoom current view
            Mixer::manager().view()->zoom( io.MouseWheel );
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
        // special case of one single source in area selection : make current after release
        if (view_drag && picked.first == nullptr && Mixer::selection().size() == 1) {
            Mixer::manager().setCurrentSource( Mixer::selection().front() );
            navigator.discardPannel();
        }

        view_drag = nullptr;
        mousedown = false;
        picked = { nullptr, glm::vec2(0.f) };
        Mixer::manager().view()->terminate();
        MousePointer::manager().active()->terminate();
        SetMouseCursor(io.MousePos);
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

    // always stop all recordings and pending actions
    FrameGrabbing::manager().stopAll();    
    navigator.discardPannel();

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

    navigator.discardPannel();
}

void UserInterface::selectOpenFilename()
{
    // launch file dialog to select a session filename to open
    if (sessionopendialog)
        sessionopendialog->open();

    navigator.discardPannel();
}

void Spinner(const ImU32 &color)
{
    ImGuiContext &g = *GImGui;

    const ImVec2 pos = ImGui::GetIO().MousePos;
    const float radius = g.FontSize;
    const ImVec2 size(radius*2.f, radius*2.f);

    // Render
    g.ForegroundDrawList.PathClear();

    const int num_segments = 30;
    const int start = abs(ImSin(g.Time * 1.8f) * (num_segments - 5));
    const float a_min = IM_PI * 2.0f * ((float) start) / (float) num_segments;
    const float a_max = IM_PI * 2.0f * ((float) num_segments - 3) / (float) num_segments;
    const ImVec2 centre = ImVec2(pos.x + radius, pos.y + radius);

    for (int i = 0; i < num_segments; i++) {
        const float a = a_min + ((float) i / (float) num_segments) * (a_max - a_min);
        g.ForegroundDrawList.PathLineTo(ImVec2(centre.x + ImCos(a + g.Time * 8) * radius,
                                            centre.y + ImSin(a + g.Time * 8) * radius));
    }

    g.ForegroundDrawList.PathStroke(color, false, radius * 0.3f);
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

    if (settingsexportdialog && settingsexportdialog->closed()
        && !settingsexportdialog->path().empty())
        Settings::Save(0, settingsexportdialog->path());

    // overlay to ensure file dialog is modal
    if (DialogToolkit::FileDialog::busy()){
        if (!ImGui::IsPopupOpen("Busy"))
            ImGui::OpenPopup("Busy");
        if (ImGui::BeginPopupModal("Busy", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Close file dialog box to resume.");
            ImGui::EndPopup();
        }
    }

    // overlay to ensure file color dialog is closed after use
    if (DialogToolkit::ColorPickerDialog::busy()){
        if (!ImGui::IsPopupOpen("##ColorBusy"))
            ImGui::OpenPopup("##ColorBusy");
        if (ImGui::BeginPopup("##ColorBusy")) {
            ImGui::Text("Validate color dialog to return to vimix.");
            ImGui::EndPopup();
        }
    }

    // overlay foreground busy animation
    if (Mixer::manager().busy() || !Mixer::manager().session()->ready()) {
        Spinner(ImGui::GetColorU32(ImGuiCol_TabActive));
    }

    // popup to inform to save before close
    if (pending_save_on_exit) {
        if (!ImGui::IsPopupOpen(MENU_SAVE_ON_EXIT))
            ImGui::OpenPopup(MENU_SAVE_ON_EXIT);
        if (ImGui::BeginPopupModal(MENU_SAVE_ON_EXIT, NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Spacing();
            ImGuiToolkit::PushFont(ImGuiToolkit::FONT_ITALIC);
            ImGui::Text(" Looks like you started some work ");
            ImGui::Text(" but didn't save the session. ");
            ImGui::PopFont();
            ImGui::Spacing();
            if (ImGui::Button(ICON_FA_TIMES "  Cancel", ImVec2(ImGui::GetWindowContentRegionWidth(), 0))) {
                pending_save_on_exit = false;
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::Button(MENU_SAVEAS_FILE, ImVec2(ImGui::GetWindowContentRegionWidth(), 0))) {
                pending_save_on_exit = false;
                saveOrSaveAs();
                ImGui::CloseCurrentPopup();
            }
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Tab));
            if (ImGui::Button(MENU_QUIT, ImVec2(ImGui::GetWindowContentRegionWidth(), 0))
                     || ImGui::IsKeyPressed(GLFW_KEY_ENTER)  || ImGui::IsKeyPressed(GLFW_KEY_KP_ENTER) ) {
                Rendering::manager().close();
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(1);
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

    // stats in the corner
    if (Settings::application.widget.stats)
        RenderMetrics(&Settings::application.widget.stats,
                  &Settings::application.widget.stats_corner,
                  &Settings::application.widget.stats_mode);

    // source editor
    if (Settings::application.widget.source_toolbar)
        RenderSourceToolbar(&Settings::application.widget.source_toolbar,
                           &Settings::application.widget.source_toolbar_border,
                           &Settings::application.widget.source_toolbar_mode);

    if ( WorkspaceWindow::clear())
        ImGui::PopStyleVar();
    // All other windows are simply not rendered if workspace is clear
    else {
        // windows
        if (Settings::application.widget.logs)
            Log::ShowLogWindow(&Settings::application.widget.logs);
        if (Settings::application.widget.help)
            RenderHelp();
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

    //
    RenderPreview();

    // handle keyboard input after all IMGUI widgets have potentially captured keyboard
    handleKeyboard();

    // all IMGUI Rendering
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

}

void UserInterface::Terminate()
{
    // save favorites
    favorites.save();

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
    if ( ImGui::MenuItem( MENU_UNDO, SHORTCUT_UNDO, false, Action::manager().current() > Action::manager().min()) )
        Action::manager().undo();
    if ( ImGui::MenuItem( MENU_REDO, SHORTCUT_REDO, false, Action::manager().current() < Action::manager().max()) )
        Action::manager().redo();

    // EDIT
    ImGui::Separator();
    if (ImGui::MenuItem( MENU_CUT, SHORTCUT_CUT, false, has_selection)) {
        std::string copied_text = Mixer::selection().clipboard();
        if (!copied_text.empty()) {
            ImGui::SetClipboardText(copied_text.c_str());
            Mixer::manager().deleteSelection();
        }
        navigator.discardPannel();
    }
    if (ImGui::MenuItem( MENU_COPY, SHORTCUT_COPY, false, has_selection)) {
        std::string copied_text = Mixer::selection().clipboard();
        if (!copied_text.empty())
            ImGui::SetClipboardText(copied_text.c_str());
        navigator.discardPannel();
    }
    if (ImGui::MenuItem( MENU_PASTE, SHORTCUT_PASTE, false, has_clipboard)) {
        if (clipboard)
            Mixer::manager().paste(clipboard);
        navigator.discardPannel();
    }
    if (ImGui::MenuItem( MENU_SELECTALL, SHORTCUT_SELECTALL, false, Mixer::manager().numSource() > 0)) {
        Mixer::manager().view()->selectAll();
        navigator.discardPannel();
    }

    // GROUP
    ImGui::Separator();
    if (ImGuiToolkit::MenuItemIcon(11, 2, " Bundle all active sources", NULL, false, Mixer::manager().numSource() > 0)) {
        // create a new group session with only active sources
        Mixer::manager().groupAll( true );
        // switch pannel to show first source (created)
        navigator.showPannelSource(0);
    }
    if (ImGuiToolkit::MenuItemIcon(7, 2, " Expand all bundles", NULL, false, Mixer::manager().numSource() > 0)) {
        // create a new group session with all sources
        Mixer::manager().ungroupAll();
    }
}

void UserInterface::showMenuWindows()
{
    if ( ImGui::MenuItem( MENU_OUTPUT, SHORTCUT_OUTPUT, &Settings::application.widget.preview) )
        UserInterface::manager().outputcontrol.setVisible(Settings::application.widget.preview);

    if ( ImGui::MenuItem( MENU_PLAYER, SHORTCUT_PLAYER, &Settings::application.widget.media_player) )
        UserInterface::manager().sourcecontrol.setVisible(Settings::application.widget.media_player);

    if ( ImGui::MenuItem( MENU_TIMER, SHORTCUT_TIMER, &Settings::application.widget.timer) )
        UserInterface::manager().timercontrol.setVisible(Settings::application.widget.timer);

    if ( ImGui::MenuItem( MENU_INPUTS, SHORTCUT_INPUTS, &Settings::application.widget.inputs) )
        UserInterface::manager().inputscontrol.setVisible(Settings::application.widget.inputs);

    if ( ImGui::MenuItem( MENU_SHADEREDITOR, SHORTCUT_SHADEREDITOR, &Settings::application.widget.shader_editor) )
        UserInterface::manager().shadercontrol.setVisible(Settings::application.widget.shader_editor);

    // Show Help
    ImGui::MenuItem( MENU_HELP, SHORTCUT_HELP, &Settings::application.widget.help );
    // Show Logs
    ImGui::MenuItem( MENU_LOGS, SHORTCUT_LOGS, &Settings::application.widget.logs );

    ImGui::Separator();

    // Enable / disable source toolbar
    ImGui::MenuItem( MENU_SOURCE_TOOL, NULL, &Settings::application.widget.source_toolbar );
    // Enable / disable metrics toolbar
    ImGui::MenuItem( MENU_METRICS, NULL, &Settings::application.widget.stats );

    ImGui::Separator();

    // Enter / exit Fullscreen
    if (Settings::application.windows[0].fullscreen){
        if (ImGui::MenuItem( ICON_FA_COMPRESS_ALT "   Exit Fullscreen", SHORTCUT_FULLSCREEN ))
            Rendering::manager().mainWindow().toggleFullscreen();
    }
    else {
        if (ImGui::MenuItem( ICON_FA_EXPAND_ALT "   Fullscreen", SHORTCUT_FULLSCREEN ))
            Rendering::manager().mainWindow().toggleFullscreen();
    }
}

void UserInterface::showMenuFile()
{
    // NEW
    if (ImGui::MenuItem( MENU_NEW_FILE, SHORTCUT_NEW_FILE)) {
        Mixer::manager().close();
        navigator.discardPannel();
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
        navigator.discardPannel();
    }

    if (ImGui::MenuItem( MENU_SAVE_FILE, SHORTCUT_SAVE_FILE, false, currentfileopen)) {
        if (saveOrSaveAs())
            navigator.discardPannel();
    }
    if (ImGui::MenuItem( MENU_SAVEAS_FILE, SHORTCUT_SAVEAS_FILE))
        selectSaveFilename();

    ImGui::MenuItem( MENU_SAVE_ON_EXIT, nullptr, &Settings::application.recentSessions.save_on_exit);

    // QUIT
    ImGui::Separator();
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
    // - current_view : indices are >= 1 (Mixing) and < 7 (INVALID)
    // - Modulo 6 to allow multiple repetition of shift increment
    // - skipping TRANSITION view 5 that is called only during transition
    int target_index = ( (Settings::application.current_view -1) + (*shift -1) )%6 + 1;

    // skip TRANSITION view
    if (target_index == View::TRANSITION)
        ++target_index;

    // prepare rendering of centered, fixed-size, semi-transparent window;
    const ImGuiIO& io = ImGui::GetIO();
    ImVec2 window_pos = ImVec2(io.DisplaySize.x / 2.f, io.DisplaySize.y / 2.f);
    ImVec2 window_pos_pivot = ImVec2(0.5f, 0.5f);
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
    ImGui::SetNextWindowSize(ImVec2(5.f * 120.f, 120.f + 2.f * ImGui::GetTextLineHeight()), ImGuiCond_Always);
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

        // draw in 5 columns
        ImGui::Columns(5, NULL, false);

        // 4 selectable large icons
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        if (ImGui::Selectable( ICON_FA_BULLSEYE, &selected_view[1], 0, iconsize))
        {
            setView(View::MIXING);
            *shift = 0;
        }
        ImGui::NextColumn();
        if (ImGui::Selectable( ICON_FA_OBJECT_UNGROUP , &selected_view[2], 0, iconsize))
        {
            setView(View::GEOMETRY);
            *shift = 0;
        }
        ImGui::NextColumn();
        if (ImGuiToolkit::SelectableIcon(ICON_WORKSPACE, "", selected_view[3], iconsize))
        {
            setView(View::LAYER);
            *shift = 0;
        }
        ImGui::NextColumn();
        if (ImGui::Selectable( ICON_FA_CHESS_BOARD, &selected_view[4], 0, iconsize))
        {
            setView(View::TEXTURE);
            *shift = 0;
        }
        // skip TRANSITION view
        ImGui::NextColumn();
        if (ImGuiToolkit::SelectableIcon(10, 7, "", selected_view[6], iconsize))
        {
            setView(View::DISPLAYS);
            *shift = 0;
        }
        ImGui::PopFont();

        // 5 subtitles (text centered in column)
        for (int v = View::MIXING; v < View::INVALID; ++v) {
            // skip TRANSITION view
            if (v == View::TRANSITION)
                continue;
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

void UserInterface::setSourceInPanel(int index)
{
    Mixer::manager().setCurrentIndex(index);
    if (navigator.pannelVisible())
        navigator.showPannelSource( Mixer::manager().indexCurrentSource() );
}

void UserInterface::setSourceInPanel(Source *s)
{
    if (s) {
        Mixer::manager().setCurrentSource( s );
        if (navigator.pannelVisible())
            navigator.showPannelSource( Mixer::manager().indexCurrentSource() );
    }
}

Source *UserInterface::sourceInPanel()
{
    Source *ret = nullptr;

    int idxpanel = navigator.selectedPannelSource();
    if (idxpanel > -1 && idxpanel < NAV_MAX) {
        ret = Mixer::manager().sourceAtIndex(idxpanel);
    }

    return ret;
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
        }
        else
            setSourceInPanel(s);
    }
}

void UserInterface::RenderPreview()
{
    static bool _inspector = false;
    static bool _sustain = false;
    static FrameBuffer *_framebuffer = nullptr;

    if (show_preview != PREVIEW_NONE && !ImGui::IsPopupOpen("##RENDERPREVIEW")) {
        // select which framebuffer to display depending on input
        if (show_preview == PREVIEW_OUTPUT)
            _framebuffer = Mixer::manager().session()->frame();
        else if (show_preview == PREVIEW_SOURCE) {
            _framebuffer = sourcecontrol.renderedFramebuffer();
            if (_framebuffer == nullptr && Mixer::manager().currentSource() != nullptr) {
                _framebuffer = Mixer::manager().currentSource()->frame();
            }
        }

        // if a famebuffer is valid, open preview
        if (_framebuffer != nullptr) {
            ImGui::OpenPopup("##RENDERPREVIEW");
            _inspector = false;
            _sustain = false;
        } else
            show_preview = PREVIEW_NONE;
    }

    if (ImGui::BeginPopupModal("##RENDERPREVIEW", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav)) {
        // making sure the pointer is still valid
        if (_framebuffer != nullptr)
        {
            ImGuiIO& io = ImGui::GetIO();
            float ar = _framebuffer->aspectRatio();
            // image takes the available window area
            ImVec2 imagesize = io.DisplaySize;
            // image height respects original aspect ratio but is at most the available window height
            imagesize.y = MIN( imagesize.x / ar, imagesize.y) * 0.95f;
            // image respects original aspect ratio
            imagesize.x = imagesize.y * ar;

            // 100% opacity for the image (ensures true colors)
            ImVec2 draw_pos = ImGui::GetCursorScreenPos();
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.f);
            ImGui::Image((void*)(intptr_t)_framebuffer->texture(), imagesize);
            ImGui::PopStyleVar();

            // closing icon in top left corner
            ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
            ImGui::SetCursorScreenPos(draw_pos + ImVec2(IMGUI_SAME_LINE, IMGUI_SAME_LINE));
            if (ImGuiToolkit::IconButton(ICON_FA_TIMES, "Close preview"))
                show_preview = PREVIEW_NONE;
            ImGui::PopFont();

            // handle mouse clic and hovering on image
            const ImRect bb(draw_pos, draw_pos + imagesize);
            const ImGuiID id = ImGui::GetCurrentWindow()->GetID("##preview-texture");
            bool hovered, held;
            bool pressed = ImGui::ButtonBehavior(bb,
                                                 id,
                                                 &hovered,
                                                 &held,
                                                 ImGuiButtonFlags_PressedOnClick);
            // toggle inspector on mouse clic
            if (pressed)
                _inspector = !_inspector;
            // draw inspector (magnifying glass) on mouse hovering
            if (hovered & _inspector)
                DrawInspector(_framebuffer->texture(), imagesize, imagesize, draw_pos);

            // close view on mouse clic outside
            // and ignore show_preview on single clic
            if (!hovered
                && !_sustain
                && !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)
                && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                show_preview = PREVIEW_NONE;
            }
        }

        // local keyboard handler (because focus is captured by modal dialog)
        if (ImGui::IsKeyPressed( GLFW_KEY_ESCAPE, false ) ||
            (show_preview == PREVIEW_OUTPUT && ImGui::IsKeyPressed( GLFW_KEY_F6, false )) ||
            (show_preview == PREVIEW_SOURCE && ImGui::IsKeyPressed( GLFW_KEY_F7, false )) )
            show_preview = PREVIEW_NONE;
        else if ((show_preview == PREVIEW_OUTPUT && ImGui::IsKeyPressed( GLFW_KEY_F6, true )) ||
                 (show_preview == PREVIEW_SOURCE && ImGui::IsKeyPressed( GLFW_KEY_F7, true )) )
            _sustain = true;
        else if ((show_preview == PREVIEW_OUTPUT && _sustain &&  ImGui::IsKeyReleased( GLFW_KEY_F6 )) ||
                 (show_preview == PREVIEW_SOURCE && _sustain &&  ImGui::IsKeyReleased( GLFW_KEY_F7 )) )
            show_preview = PREVIEW_NONE;

        if ( !alt_modifier_active && ImGui::IsKeyPressed( GLFW_KEY_TAB )) {
            if (shift_modifier_active)
                Mixer::manager().setCurrentPrevious();
            else
                Mixer::manager().setCurrentNext();
            if (navigator.pannelVisible())
                navigator.showPannelSource( Mixer::manager().indexCurrentSource() );
            // re-open after change source
            ImGui::CloseCurrentPopup();
        }

        // close
        if (show_preview == PREVIEW_NONE) {
            _framebuffer = nullptr;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}


enum MetricsFlags_
{
    Metrics_none       = 0,
    Metrics_framerate  = 1,
    Metrics_ram        = 2,
    Metrics_gpu        = 4,
    Metrics_session    = 8,
    Metrics_runtime    = 16,
    Metrics_lifetime   = 32
};

void UserInterface::RenderMetrics(bool *p_open, int* p_corner, int *p_mode)
{
    if (!p_open || !p_corner || !p_mode)
        return;

    if (*p_mode == Metrics_none)
        *p_mode = Metrics_framerate;

    ImGuiIO& io = ImGui::GetIO();
    if (*p_corner != -1) {
        ImVec2 window_pos = ImVec2((*p_corner & 1) ? io.DisplaySize.x - WINDOW_TOOLBOX_DIST_TO_BORDER : WINDOW_TOOLBOX_DIST_TO_BORDER,
                                   (*p_corner & 2) ? io.DisplaySize.y - WINDOW_TOOLBOX_DIST_TO_BORDER : WINDOW_TOOLBOX_DIST_TO_BORDER);
        ImVec2 window_pos_pivot = ImVec2((*p_corner & 1) ? 1.0f : 0.0f, (*p_corner & 2) ? 1.0f : 0.0f);
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
    }

    ImGui::SetNextWindowBgAlpha(WINDOW_TOOLBOX_ALPHA); // Transparent background

    if (!ImGui::Begin("Metrics", NULL, (*p_corner != -1 ? ImGuiWindowFlags_NoMove : 0) |
                      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                      ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
    {
        ImGui::End();
        return;
    }

    // title
    ImGui::Text( MENU_METRICS );
    ImGui::SameLine(0, 2.2f * ImGui::GetTextLineHeightWithSpacing());
    if (ImGuiToolkit::IconButton(5,8))
        ImGui::OpenPopup("metrics_menu");

    // read Memory info every 1/2 second
    static long ram = 0;
    static glm::ivec2 gpu(INT_MAX, INT_MAX);
    {
        static GTimer *timer = g_timer_new ();
        double elapsed = g_timer_elapsed (timer, NULL);
        if ( elapsed > 0.5 ){
            ram = SystemToolkit::memory_usage();
            gpu = Rendering::manager().getGPUMemoryInformation();
            g_timer_start(timer);
        }
    }
    static char dummy_str[256];
    uint64_t time = Runtime();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.f, 2.5f));
    const float _width = 4.f * ImGui::GetTextLineHeightWithSpacing();

    if (*p_mode & Metrics_framerate) {
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_BOLD);
        snprintf(dummy_str, 256, "%.1f", io.Framerate);
        ImGui::SetNextItemWidth(_width);
        ImGui::InputText("##dummy", dummy_str, IM_ARRAYSIZE(dummy_str), ImGuiInputTextFlags_ReadOnly);
        ImGui::PopFont();
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        ImGui::Text("FPS");
        if (ImGui::IsItemHovered())
            ImGuiToolkit::ToolTip("Frames per second");
    }

    if (*p_mode & Metrics_ram) {
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_BOLD);
        snprintf(dummy_str, 256, "%s", BaseToolkit::byte_to_string( ram ).c_str());
        ImGui::SetNextItemWidth(_width);
        ImGui::InputText("##dummy2", dummy_str, IM_ARRAYSIZE(dummy_str), ImGuiInputTextFlags_ReadOnly);
        ImGui::PopFont();
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        ImGui::Text("RAM");
        if (ImGui::IsItemHovered())
            ImGuiToolkit::ToolTip("Amount of physical memory\nused by vimix");
    }

    // GPU RAM if available
    if (gpu.x < INT_MAX && gpu.x > 0 && *p_mode & Metrics_gpu) {
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_BOLD);
        // got free and max GPU RAM (nvidia)
        if (gpu.y < INT_MAX && gpu.y > 0)
            snprintf(dummy_str, 256, "%s", BaseToolkit::byte_to_string( long(gpu.y-gpu.x) * 1024 ).c_str());
        // got used GPU RAM (ati)
        else
            snprintf(dummy_str, 256, "%s", BaseToolkit::byte_to_string( long(gpu.x) * 1024 ).c_str());
        ImGui::SetNextItemWidth(_width);
        ImGui::InputText("##dummy3", dummy_str, IM_ARRAYSIZE(dummy_str), ImGuiInputTextFlags_ReadOnly);
        ImGui::PopFont();
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        ImGui::Text("GPU");
        if (ImGui::IsItemHovered())
            ImGuiToolkit::ToolTip("Total memory used in GPU");
    }

    if (*p_mode & Metrics_session) {
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_BOLD);
        snprintf(dummy_str, 256, "%s", GstToolkit::time_to_string(Mixer::manager().session()->runtime(), GstToolkit::TIME_STRING_READABLE).c_str());
        ImGui::SetNextItemWidth(_width);
        ImGui::InputText("##dummy", dummy_str, IM_ARRAYSIZE(dummy_str), ImGuiInputTextFlags_ReadOnly);
        ImGui::PopFont();
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        ImGui::Text("Session");
        if (ImGui::IsItemHovered())
            ImGuiToolkit::ToolTip("Runtime since session load");
    }

    if (*p_mode & Metrics_runtime) {
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_BOLD);
        snprintf(dummy_str, 256, "%s", GstToolkit::time_to_string(time, GstToolkit::TIME_STRING_READABLE).c_str());
        ImGui::SetNextItemWidth(_width);
        ImGui::InputText("##dummy2", dummy_str, IM_ARRAYSIZE(dummy_str), ImGuiInputTextFlags_ReadOnly);
        ImGui::PopFont();
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        ImGui::Text("Runtime");
        if (ImGui::IsItemHovered())
            ImGuiToolkit::ToolTip("Runtime since vimix started");
    }

    if (*p_mode & Metrics_lifetime) {
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_BOLD);
        time += Settings::application.total_runtime;
        snprintf(dummy_str, 256, "%s", GstToolkit::time_to_string(time, GstToolkit::TIME_STRING_READABLE).c_str());
        ImGui::SetNextItemWidth(_width);
        ImGui::InputText("##dummy3", dummy_str, IM_ARRAYSIZE(dummy_str), ImGuiInputTextFlags_ReadOnly);
        ImGui::PopFont();
        ImGui::SameLine(0, IMGUI_SAME_LINE);
        ImGui::Text("Lifetime");
        if (ImGui::IsItemHovered())
            ImGuiToolkit::ToolTip("Accumulated runtime of vimix\nsince its installation");
    }

    ImGui::PopStyleVar();

    if (ImGui::BeginPopup("metrics_menu"))
    {
        if (ImGui::MenuItem( "Framerate", NULL, *p_mode & Metrics_framerate))
            *p_mode ^= Metrics_framerate;
        if (ImGui::MenuItem( "RAM", NULL, *p_mode & Metrics_ram))
            *p_mode ^= Metrics_ram;
        // GPU RAM if available
        if (gpu.x < INT_MAX && gpu.x > 0)
            if (ImGui::MenuItem( "GPU", NULL, *p_mode & Metrics_gpu))
                *p_mode ^= Metrics_gpu;
        if (ImGui::MenuItem( "Session time", NULL, *p_mode & Metrics_session))
            *p_mode ^= Metrics_session;
        if (ImGui::MenuItem( "Runtime", NULL, *p_mode & Metrics_runtime))
            *p_mode ^= Metrics_runtime;
        if (ImGui::MenuItem( "Lifetime", NULL, *p_mode & Metrics_lifetime))
            *p_mode ^= Metrics_lifetime;

        ImGui::Separator();

        if (ImGui::MenuItem( ICON_FA_ANGLE_UP "  Top right",    NULL, *p_corner == 1))
            *p_corner = 1;
        if (ImGui::MenuItem( ICON_FA_ANGLE_DOWN "  Bottom right", NULL, *p_corner == 3))
            *p_corner = 3;
        if (ImGui::MenuItem( ICON_FA_ARROWS_ALT " Free position", NULL, *p_corner == -1))
            *p_corner = -1;
        if (p_open && ImGui::MenuItem( ICON_FA_TIMES "  Close"))
            *p_open = false;

        ImGui::EndPopup();
    }

    ImGui::End();
}

enum SourceToolbarFlags_
{
    SourceToolbar_none       = 0,
    SourceToolbar_linkar     = 1,
    SourceToolbar_autohide   = 2
};

void UserInterface::RenderSourceToolbar(bool *p_open, int* p_border, int *p_mode) {

    if (!p_open || !p_border || !p_mode || !Mixer::manager().session()->ready())
        return;

    Source *s = Mixer::manager().currentSource();
    if (s || !(*p_mode & SourceToolbar_autohide) ) {

        ImGuiIO& io = ImGui::GetIO();
        std::ostringstream info;
        const glm::vec3 out = Mixer::manager().session()->frame()->resolution();
        const char *tooltip_lock[2] = {"Width & height not linked", "Width & height linked"};

        //
        // horizontal layout for top and bottom placements
        //
        if (*p_border > 0) {

            ImVec2 window_pos = ImVec2((*p_border & 1) ? io.DisplaySize.x * 0.5 : WINDOW_TOOLBOX_DIST_TO_BORDER,
                                       (*p_border & 2) ? io.DisplaySize.y - WINDOW_TOOLBOX_DIST_TO_BORDER : WINDOW_TOOLBOX_DIST_TO_BORDER);
            ImVec2 window_pos_pivot = ImVec2((*p_border & 1) ? 0.5f : 0.0f, (*p_border & 2) ? 1.0f : 0.0f);
            ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
            ImGui::SetNextWindowBgAlpha(WINDOW_TOOLBOX_ALPHA); // Transparent background

            if (!ImGui::Begin("SourceToolbarfixed", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |
                              ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav))
            {
                ImGui::End();
                return;
            }

            const float sliderwidth = 3.f * ImGui::GetTextLineHeightWithSpacing();

            if (s) {

                // get info on source
                Group *n = s->group(View::GEOMETRY);
                info << s->name() << ": ";

                //
                // ALPHA
                //
                float v = s->alpha() * 100.f;
                if (ImGuiToolkit::TextButton( ICON_FA_BULLSEYE , "Alpha")) {
                    s->call(new SetAlpha(1.f), true);
                    info << "Alpha " << std::fixed << std::setprecision(3) << 0.f;
                    Action::manager().store(info.str());
                }
                ImGui::SameLine(0, IMGUI_SAME_LINE);
                ImGui::SetNextItemWidth(sliderwidth);
                if ( ImGui::DragFloat("##Alpha", &v, 0.1f, 0.f, 100.f, "%.1f%%") )
                    s->call(new SetAlpha(v*0.01f), true);
                if (ImGui::IsItemHovered() && io.MouseWheel != 0.f ){
                    v = CLAMP(v + 0.1f * io.MouseWheel, 0.f, 100.f);
                    s->call(new SetAlpha(v*0.01f), true);
                    info << "Alpha " << std::fixed << std::setprecision(3) << v*0.01f;
                    Action::manager().store(info.str());
                }
                if ( ImGui::IsItemDeactivatedAfterEdit() ) {
                    info << "Alpha " << std::fixed << std::setprecision(3) << v*0.01f;
                    Action::manager().store(info.str());
                }

                ImGui::SameLine(0, IMGUI_SAME_LINE);
                ImGui::Text("|");
                //
                // POSITION COORDINATES
                //
                ImGui::SameLine(0, IMGUI_SAME_LINE);
                if (ImGuiToolkit::TextButton( ICON_FA_SIGN, "Position")) {
                    n->translation_.x = 0.f;
                    n->translation_.y = 0.f;
                    s->touch();
                    info << "Position " << std::setprecision(3) << n->translation_.x << ", " << n->translation_.y;
                    Action::manager().store(info.str());
                }
                // Position X
                v = n->translation_.x * (0.5f * out.y);
                ImGui::SameLine(0, IMGUI_SAME_LINE);
                ImGui::SetNextItemWidth( sliderwidth);
                if ( ImGui::DragFloat("##PosX", &v, 1.0f, -MAX_SCALE * (0.5f * out.y), MAX_SCALE * (0.5f * out.y), "%.0fpx") )  {
                    n->translation_.x = v / (0.5f * out.y);
                    s->touch();
                }
                if ( ImGui::IsItemHovered() && io.MouseWheel != 0.f ){
                    v += io.MouseWheel;
                    n->translation_.x = v / (0.5f * out.y);
                    s->touch();
                    info << "Position " << std::setprecision(3) << n->translation_.x << ", " << n->translation_.y;
                    Action::manager().store(info.str());
                }
                if ( ImGui::IsItemDeactivatedAfterEdit() ){
                    info << "Position " << std::setprecision(3) << n->translation_.x << ", " << n->translation_.y;
                    Action::manager().store(info.str());
                }
                // Position Y
                v = n->translation_.y * (0.5f * out.y);
                ImGui::SameLine(0, IMGUI_SAME_LINE);
                ImGui::SetNextItemWidth( sliderwidth);
                if ( ImGui::DragFloat("##PosY", &v, 1.0f, -MAX_SCALE * (0.5f * out.y), MAX_SCALE * (0.5f * out.y), "%.0fpx") )  {
                    n->translation_.y = v / (0.5f * out.y);
                    s->touch();
                }
                if (ImGui::IsItemHovered() && io.MouseWheel != 0.f ){
                    v += io.MouseWheel;
                    n->translation_.y = v / (0.5f * out.y);
                    s->touch();
                    info << "Position " << std::setprecision(3) << n->translation_.x << ", " << n->translation_.y;
                    Action::manager().store(info.str());
                }
                if ( ImGui::IsItemDeactivatedAfterEdit() ){
                    info << "Position " << std::setprecision(3) << n->translation_.x << ", " << n->translation_.y;
                    Action::manager().store(info.str());
                }
                ImGui::SameLine(0, IMGUI_SAME_LINE);
                ImGui::Text("|");

                //
                // SCALE
                //
                ImGui::SameLine(0, IMGUI_SAME_LINE);
                if (ImGuiToolkit::TextButton( ICON_FA_RULER_COMBINED, "Size")) {
                    n->scale_.x = 1.f;
                    n->scale_.y = 1.f;
                    s->touch();
                    info << "Scale " << std::setprecision(3) << n->scale_.x << ", " << n->scale_.y;
                    Action::manager().store(info.str());
                }
                float ar_scale = n->scale_.x / n->scale_.y;
                // SCALE X
                v = n->scale_.x * ( out.y * s->frame()->aspectRatio());
                ImGui::SameLine(0, IMGUI_SAME_LINE);
                ImGui::SetNextItemWidth( sliderwidth );
                if ( ImGui::DragFloat("##ScaleX", &v, 1.f, -MAX_SCALE * out.x, MAX_SCALE * out.x, "%.0fpx") ) {
                    if (v > 10.f) {
                        n->scale_.x = v / ( out.y * s->frame()->aspectRatio());
                        if (*p_mode & SourceToolbar_linkar)
                            n->scale_.y = n->scale_.x / ar_scale;
                        s->touch();
                    }
                }
                if (ImGui::IsItemHovered() && io.MouseWheel != 0.f && v > 10.f){
                    v += io.MouseWheel;
                    n->scale_.x = v / ( out.y * s->frame()->aspectRatio());
                    if (*p_mode & SourceToolbar_linkar)
                        n->scale_.y = n->scale_.x / ar_scale;
                    s->touch();
                    info << "Scale " << std::setprecision(3) << n->scale_.x << " x " << n->scale_.y;
                    Action::manager().store(info.str());
                }
                if ( ImGui::IsItemDeactivatedAfterEdit() ){
                    info << "Scale " << std::setprecision(3) << n->scale_.x << " x " << n->scale_.y;
                    Action::manager().store(info.str());
                }
                // SCALE LOCK ASPECT RATIO
                ImGui::SameLine(0, 0);
                bool lock = *p_mode & SourceToolbar_linkar;
                if (ImGuiToolkit::IconToggle(5,1,6,1, &lock, tooltip_lock ))
                    *p_mode ^= SourceToolbar_linkar; //             *p_mode |= lock ? SourceToolbar_linkar : !SourceToolbar_linkar;
                ImGui::SameLine(0, 0);
                // SCALE Y
                v = n->scale_.y * out.y;
                ImGui::SetNextItemWidth( sliderwidth );
                if ( ImGui::DragFloat("##ScaleY", &v, 1.f, -MAX_SCALE * out.y, MAX_SCALE * out.y, "%.0fpx") ) {
                    if (v > 10.f) {
                        n->scale_.y = v / out.y;
                        if (*p_mode & SourceToolbar_linkar)
                            n->scale_.x = n->scale_.y * ar_scale;
                        s->touch();
                    }
                }
                if (ImGui::IsItemHovered() && io.MouseWheel != 0.f && v > 10.f){
                    v += io.MouseWheel;
                    n->scale_.y = v / out.y;
                    if (*p_mode & SourceToolbar_linkar)
                        n->scale_.x = n->scale_.y * ar_scale;
                    s->touch();
                    info << "Scale " << std::setprecision(3) << n->scale_.x << " x " << n->scale_.y;
                    Action::manager().store(info.str());
                }
                if ( ImGui::IsItemDeactivatedAfterEdit() ){
                    info << "Scale " << std::setprecision(3) << n->scale_.x << " x " << n->scale_.y;
                    Action::manager().store(info.str());
                }

                ImGui::SameLine(0, IMGUI_SAME_LINE);
                ImGui::Text("|");

                //
                // ROTATION ANGLE
                //
                ImGui::SameLine(0, IMGUI_SAME_LINE);
                if (ImGuiToolkit::IconButton( 18, 9, "Angle")) {
                    n->rotation_.z = 0.f;
                    s->touch();
                    info << "Angle " << std::fixed << std::setprecision(2) << n->rotation_.z * 180.f / M_PI << UNICODE_DEGREE;
                    Action::manager().store(info.str());
                }
                float v_deg = n->rotation_.z * 360.0f / (2.f*M_PI);
                ImGui::SameLine(0, IMGUI_SAME_LINE);
                ImGui::SetNextItemWidth(sliderwidth);
                if ( ImGui::DragFloat("##Angle", &v_deg, 0.02f, -180.f, 180.f, "%.2f" UNICODE_DEGREE) ) {
                    n->rotation_.z = v_deg * (2.f*M_PI) / 360.0f;
                    s->touch();
                }
                if (ImGui::IsItemHovered() && io.MouseWheel != 0.f){
                    v_deg = CLAMP(v_deg + 0.01f * io.MouseWheel, -180.f, 180.f);
                    n->rotation_.z = v_deg * (2.f*M_PI) / 360.0f;
                    s->touch();
                    info << "Angle " << std::fixed << std::setprecision(2) << n->rotation_.z * 180.f / M_PI << UNICODE_DEGREE;
                    Action::manager().store(info.str());
                }
                if ( ImGui::IsItemDeactivatedAfterEdit() ) {
                    info << "Angle " << std::fixed << std::setprecision(2) << n->rotation_.z * 180.f / M_PI << UNICODE_DEGREE;
                    Action::manager().store(info.str());
                }

                ImGui::SameLine(0, 2 * IMGUI_SAME_LINE);

            }
            // NO SOURCE and not auto hide
            else {
                ImGui::AlignTextToFramePadding();
                ImGui::Text( MENU_SOURCE_TOOL );
                ImGui::SameLine(0, sliderwidth);
                ImGui::TextDisabled("No active source");
                ImGui::SameLine(0, sliderwidth);
            }

            if (ImGuiToolkit::IconButton(5,8))
                ImGui::OpenPopup("sourcetool_menu");
        }
        //
        // compact layout for free placement
        //
        else {
            ImGui::SetNextWindowPos(ImVec2(690,20), ImGuiCond_FirstUseEver); // initial pos
            ImGui::SetNextWindowBgAlpha(WINDOW_TOOLBOX_ALPHA); // Transparent background
            if (!ImGui::Begin("SourceToolbar", NULL, ImGuiWindowFlags_NoDecoration |
                              ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav))
            {
                ImGui::End();
                return;
            }

            // Title window
            ImGui::Text( MENU_SOURCE_TOOL );
            ImGui::SameLine(0, 2.f * ImGui::GetTextLineHeightWithSpacing());
            if (ImGuiToolkit::IconButton(5,8))
                ImGui::OpenPopup("sourcetool_menu");

            // WITH SOURCE
            if (s) {

                // get info on source
                Group *n = s->group(View::GEOMETRY);
                info << s->name() << ": ";

                const float sliderwidth = 6.4f * ImGui::GetTextLineHeightWithSpacing();
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.f, 2.f));

                //
                // ALPHA
                //
                float v = s->alpha() * 100.f;
                ImGui::SetNextItemWidth(sliderwidth);
                if ( ImGui::DragFloat("##Alpha", &v, 0.1f, 0.f, 100.f, "%.1f%%") )
                    s->call(new SetAlpha(v*0.01f), true);
                if (ImGui::IsItemHovered() && io.MouseWheel != 0.f ){
                    v = CLAMP(v + 0.1f * io.MouseWheel, 0.f, 100.f);
                    s->call(new SetAlpha(v*0.01f), true);
                    info << "Alpha " << std::fixed << std::setprecision(3) << v*0.01f;
                    Action::manager().store(info.str());
                }
                if ( ImGui::IsItemDeactivatedAfterEdit() ) {
                    info << "Alpha " << std::fixed << std::setprecision(3) << v*0.01f;
                    Action::manager().store(info.str());
                }
                ImGui::SameLine(0, IMGUI_SAME_LINE);
                if (ImGuiToolkit::TextButton("Alpha")) {
                    s->call(new SetAlpha(1.f), true);
                    info << "Alpha " << std::fixed << std::setprecision(3) << 0.f;
                    Action::manager().store(info.str());
                }

                //
                // POSITION COORDINATES
                //
                // Position X
                v = n->translation_.x * (0.5f * out.y);
                ImGui::SetNextItemWidth( 3.08f * ImGui::GetTextLineHeightWithSpacing() );
                if ( ImGui::DragFloat("##PosX", &v, 1.0f, -MAX_SCALE * (0.5f * out.y), MAX_SCALE * (0.5f * out.y), "%.0fpx") )  {
                    n->translation_.x = v / (0.5f * out.y);
                    s->touch();
                }
                if ( ImGui::IsItemHovered() && io.MouseWheel != 0.f ){
                    v += io.MouseWheel;
                    n->translation_.x = v / (0.5f * out.y);
                    s->touch();
                    info << "Position " << std::setprecision(3) << n->translation_.x << ", " << n->translation_.y;
                    Action::manager().store(info.str());
                }
                if ( ImGui::IsItemDeactivatedAfterEdit() ){
                    info << "Position " << std::setprecision(3) << n->translation_.x << ", " << n->translation_.y;
                    Action::manager().store(info.str());
                }
                // Position Y
                v = n->translation_.y * (0.5f * out.y);
                ImGui::SameLine(0, IMGUI_SAME_LINE);
                ImGui::SetNextItemWidth( 3.08f * ImGui::GetTextLineHeightWithSpacing() );
                if ( ImGui::DragFloat("##PosY", &v, 1.0f, -MAX_SCALE * (0.5f * out.y), MAX_SCALE * (0.5f * out.y), "%.0fpx") )  {
                    n->translation_.y = v / (0.5f * out.y);
                    s->touch();
                }
                if ( ImGui::IsItemHovered() && io.MouseWheel != 0.f ){
                    v += io.MouseWheel;
                    n->translation_.y = v / (0.5f * out.y);
                    s->touch();
                    info << "Position " << std::setprecision(3) << n->translation_.x << ", " << n->translation_.y;
                    Action::manager().store(info.str());
                }
                if ( ImGui::IsItemDeactivatedAfterEdit() ){
                    info << "Position " << std::setprecision(3) << n->translation_.x << ", " << n->translation_.y;
                    Action::manager().store(info.str());
                }
                ImGui::SameLine(0, IMGUI_SAME_LINE);
                if (ImGuiToolkit::TextButton("Pos")) {
                    n->translation_.x = 0.f;
                    n->translation_.y = 0.f;
                    s->touch();
                    info << "Position " << std::setprecision(3) << n->translation_.x << ", " << n->translation_.y;
                    Action::manager().store(info.str());
                }

                //
                // SCALE
                //
                float ar_scale = n->scale_.x / n->scale_.y;
                // SCALE X
                v = n->scale_.x * ( out.y * s->frame()->aspectRatio());
                ImGui::SetNextItemWidth( 2.7f * ImGui::GetTextLineHeightWithSpacing() );
                if ( ImGui::DragFloat("##ScaleX", &v, 1.f, -MAX_SCALE * out.x, MAX_SCALE * out.x, "%.0f") ) {
                    if (v > 10.f) {
                        n->scale_.x = v / ( out.y * s->frame()->aspectRatio());
                        if (*p_mode & SourceToolbar_linkar)
                            n->scale_.y = n->scale_.x / ar_scale;
                        s->touch();
                    }
                }
                if (ImGui::IsItemHovered() && io.MouseWheel != 0.f && v > 10.f){
                    v += io.MouseWheel;
                    n->scale_.x = v / ( out.y * s->frame()->aspectRatio());
                    if (*p_mode & SourceToolbar_linkar)
                        n->scale_.y = n->scale_.x / ar_scale;
                    s->touch();
                    info << "Scale " << std::setprecision(3) << n->scale_.x << " x " << n->scale_.y;
                    Action::manager().store(info.str());
                }
                if ( ImGui::IsItemDeactivatedAfterEdit() ){
                    info << "Scale " << std::setprecision(3) << n->scale_.x << " x " << n->scale_.y;
                    Action::manager().store(info.str());
                }
                // SCALE LOCK ASPECT RATIO
                ImGui::SameLine(0, 0);
                bool lock = *p_mode & SourceToolbar_linkar;
                if (ImGuiToolkit::IconToggle(5,1,6,1, &lock, tooltip_lock ))
                    *p_mode ^= SourceToolbar_linkar;
                ImGui::SameLine(0, 0);
                // SCALE Y
                v = n->scale_.y * out.y ;
                ImGui::SetNextItemWidth( 2.7f * ImGui::GetTextLineHeightWithSpacing() );
                if ( ImGui::DragFloat("##ScaleY", &v, 1.f, -MAX_SCALE * out.y, MAX_SCALE * out.y, "%.0f") ) {
                    if (v > 10.f) {
                        n->scale_.y = v / out.y;
                        if (*p_mode & SourceToolbar_linkar)
                            n->scale_.x = n->scale_.y * ar_scale;
                        s->touch();
                    }
                }
                if (ImGui::IsItemHovered() && io.MouseWheel != 0.f && v > 10.f){
                    v += io.MouseWheel;
                    n->scale_.y = v / out.y;
                    if (*p_mode & SourceToolbar_linkar)
                        n->scale_.x = n->scale_.y * ar_scale;
                    s->touch();
                    info << "Scale " << std::setprecision(3) << n->scale_.x << " x " << n->scale_.y;
                    Action::manager().store(info.str());
                }
                if ( ImGui::IsItemDeactivatedAfterEdit() ){
                    info << "Scale " << std::setprecision(3) << n->scale_.x << " x " << n->scale_.y;
                    Action::manager().store(info.str());
                }
                ImGui::SameLine(0, IMGUI_SAME_LINE);
                if (ImGuiToolkit::TextButton("Size")) {
                    n->scale_.x = 1.f;
                    n->scale_.y = 1.f;
                    s->touch();
                    info << "Scale " << std::setprecision(3) << n->scale_.x << ", " << n->scale_.y;
                    Action::manager().store(info.str());
                }

                //
                // ROTATION ANGLE
                //
                float v_deg = n->rotation_.z * 360.0f / (2.f*M_PI);
                ImGui::SetNextItemWidth(sliderwidth);
                if ( ImGui::DragFloat("##Angle", &v_deg, 0.02f, -180.f, 180.f, "%.2f" UNICODE_DEGREE) ) {
                    n->rotation_.z = v_deg * (2.f*M_PI) / 360.0f;
                    s->touch();
                }
                if (ImGui::IsItemHovered() && io.MouseWheel != 0.f){
                    v_deg = CLAMP(v_deg + 0.01f * io.MouseWheel, -180.f, 180.f);
                    n->rotation_.z = v_deg * (2.f*M_PI) / 360.0f;
                    s->touch();
                    info << "Angle " << std::setprecision(3) << n->rotation_.z * 180.f / M_PI;
                    Action::manager().store(info.str());
                }
                if ( ImGui::IsItemDeactivatedAfterEdit() ) {
                    info << "Angle " << std::setprecision(3) << n->rotation_.z * 180.f / M_PI;
                    Action::manager().store(info.str());
                }
                ImGui::SameLine(0, IMGUI_SAME_LINE);
                if (ImGuiToolkit::TextButton("Angle")) {
                    n->rotation_.z = 0.f;
                    s->touch();
                    info << "Angle " << std::setprecision(3) << n->rotation_.z * 180.f / M_PI;
                    Action::manager().store(info.str());
                }

                ImGui::PopStyleVar();
            }
            // NO SOURCE and not auto hide
            else {

                ImGui::TextDisabled("  ");
                ImGui::TextDisabled("No active source");
                ImGui::TextDisabled("  ");

            }
        }

        if (ImGui::BeginPopup("sourcetool_menu"))
        {
            if (ImGui::MenuItem( "Auto hide", NULL, *p_mode & SourceToolbar_autohide))
                *p_mode ^= SourceToolbar_autohide;

            ImGui::Separator();

            if (ImGui::MenuItem( ICON_FA_ANGLE_UP "  Top",    NULL, *p_border == 1))
                *p_border = 1;
            if (ImGui::MenuItem( ICON_FA_ANGLE_DOWN "  Bottom", NULL, *p_border == 3))
                *p_border = 3;
            if (ImGui::MenuItem( ICON_FA_ARROWS_ALT " Free position", NULL, *p_border == -1))
                *p_border = -1;
            if (p_open && ImGui::MenuItem( ICON_FA_TIMES "  Close"))
                *p_open = false;

            ImGui::EndPopup();
        }

        ImGui::Spacing();
        ImGui::End();
    }

}

void UserInterface::RenderAbout(bool* p_open)
{
    ImGui::SetNextWindowPos(ImVec2(600, 40), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("About " APP_TITLE, p_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::End();
        return;
    }

    ImVec2 top = ImGui::GetCursorScreenPos();
    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
#ifdef VIMIX_VERSION_MAJOR
    ImGui::Text("%s %d.%d.%d", APP_NAME, VIMIX_VERSION_MAJOR, VIMIX_VERSION_MINOR, VIMIX_VERSION_PATCH);
#else
    ImGui::Text("%s", APP_NAME);
#endif
    ImGui::PopFont();

#ifdef VIMIX_GIT
    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_ITALIC);
    ImGui::Text(VIMIX_GIT);
    ImGui::PopFont();
#endif

    static unsigned int img_crow = 0;
    if (img_crow == 0)
        img_crow = Resource::getTextureImage("images/vimix_crow_white.png");
    ImGui::SetCursorScreenPos(top);
    ImGui::Image((void*)(intptr_t)img_crow, ImVec2(512, 340));

    ImGui::Text("vimix performs graphical mixing and blending of\nseveral movie clips and computer generated graphics,\nwith image processing effects in real-time.");
    ImGui::Text("\nvimix is licensed under GNU GPL version 3 or later.\n" UNICODE_COPYRIGHT " 2019-2023 Bruno Herbelin.");

    ImGui::Spacing();
    ImGuiToolkit::ButtonOpenUrl("Visit vimix website", "https://brunoherbelin.github.io/vimix/", ImVec2(ImGui::GetContentRegionAvail().x, 0));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Text("Learn more about the libraries behind vimix:");
    ImGui::Spacing();

    if ( ImGui::Button("About GStreamer (available plugins)", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
        show_gst_about = true;

    if ( ImGui::Button("About OpenGL (runtime extensions)", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
        show_opengl_about = true;

    if ( ImGui::Button("About Dear ImGui (build information)", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
        show_imgui_about = true;

    ImGui::Columns(3, "abouts");
    ImGui::Separator();
    ImGuiToolkit::ButtonOpenUrl("Glad", "https://glad.dav1d.de", ImVec2(ImGui::GetContentRegionAvail().x, 0));

    ImGui::NextColumn();
    ImGuiToolkit::ButtonOpenUrl("GLFW", "http://www.glfw.org", ImVec2(ImGui::GetContentRegionAvail().x, 0));

    ImGui::NextColumn();
    ImGuiToolkit::ButtonOpenUrl("glm", "https://glm.g-truc.net", ImVec2(ImGui::GetContentRegionAvail().x, 0));

    ImGui::NextColumn();
    ImGuiToolkit::ButtonOpenUrl("OSCPack", "https://github.com/RossBencina/oscpack", ImVec2(ImGui::GetContentRegionAvail().x, 0));

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
            if ( ImGui::MenuItem( MENU_CAPTUREGUI, SHORTCUT_CAPTURE_GUI) )
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
    float megabyte = static_cast<float>( static_cast<double>(FrameBuffer::memory_usage()) / 1000000.0 );

    // init
    if (refresh_rate < 0.f) {

        const GLFWvidmode* mode = glfwGetVideoMode(Rendering::manager().mainWindow().monitor());
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
    snprintf(overlay, 128, "Rendering %.1f FPS", recorded_sum[0] / float(PLOT_ARRAY_SIZE));
    ImGui::PlotLines("LinesRender", recorded_values[0], PLOT_ARRAY_SIZE, values_index, overlay, recorded_bounds[0][0], recorded_bounds[0][1], plot_size);
    snprintf(overlay, 128, "Update time %.1f ms (%.1f FPS)", recorded_sum[1] / float(PLOT_ARRAY_SIZE), (float(PLOT_ARRAY_SIZE) * 1000.f) / recorded_sum[1]);
    ImGui::PlotHistogram("LinesMixer", recorded_values[1], PLOT_ARRAY_SIZE, values_index, overlay, recorded_bounds[1][0], recorded_bounds[1][1], plot_size);
    snprintf(overlay, 128, "Framebuffers %.1f MB", recorded_values[2][(values_index+PLOT_ARRAY_SIZE-1) % PLOT_ARRAY_SIZE] );
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



void UserInterface::RenderHelp()
{
    // first run
    ImGui::SetNextWindowPos(ImVec2(520, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(460, 800), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(350, 300), ImVec2(FLT_MAX, FLT_MAX));

    if ( !ImGui::Begin(IMGUI_TITLE_HELP, &Settings::application.widget.help,
                       ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse ) )
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

        ImGui::Text(ICON_FA_MOUSE_POINTER "  Snap cursor"); ImGui::NextColumn();
        ImGui::Text ("Snapping mouse cursors modify the mouse effective position to enhace the movement: e.g. snap to grid, move on a line, or trigger on metronome. "
                     "They are activated with the [" ALT_MOD "] key" );
        ImGui::NextColumn();
        ImGui::Text(ICON_FA_BULLSEYE "  Mixing"); ImGui::NextColumn();
        ImGui::Text ("Adjust opacity of sources, visible in the center and transparent on the side. Sources are de-activated outside of darker circle.");
        ImGui::NextColumn();
        ImGui::Text(ICON_FA_OBJECT_UNGROUP "  Geometry"); ImGui::NextColumn();
        ImGui::Text ("Move, scale, rotate or crop sources to place them in the output frame.");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_WORKSPACE); ImGui::SameLine(0, IMGUI_SAME_LINE); ImGui::Text("Layers"); ImGui::NextColumn();
        ImGui::Text ("Organize the rendering order of sources in depth, from background to foreground.");
        ImGui::NextColumn();
        ImGui::Text(ICON_FA_CHESS_BOARD "  Texturing"); ImGui::NextColumn();
        ImGui::Text ("Apply masks or freely paint the texture on the source surface. Repeat or crop the graphics.");
        ImGui::NextColumn();
        ImGui::Text(ICON_FA_TV "  Displays"); ImGui::NextColumn();
        ImGui::Text ("Manage and place output windows in computer's displays (e.g. fullscreen mode, white balance adjustment).");
        ImGui::NextColumn();

        ImGui::Columns(1);
        ImGui::PopTextWrapPos();
    }

    if (ImGui::CollapsingHeader("Tools"))
    {
        ImGui::Columns(2, "windowcolumn", false); // 4-ways, with border
        ImGui::SetColumnWidth(0, width_column0);
        ImGui::PushTextWrapPos(width_window );

        ImGui::Text(IMGUI_TITLE_PREVIEW); ImGui::NextColumn();
        ImGui::Text ("Preview the output displayed in the rendering window(s). Control video recording and streaming.");
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
        ImGui::Text(IMGUI_TITLE_LOGS); ImGui::NextColumn();
        ImGui::Text ("History of program logs, with information on success and failure of commands.");
        ImGui::NextColumn();
        ImGui::Text(IMGUI_TITLE_HELP); ImGui::NextColumn();
        ImGui::Text ("Link to online documentation and list of concepts (this window).");
        ImGui::NextColumn();
        ImGui::Text(ICON_FA_WRENCH " Source"); ImGui::NextColumn();
        ImGui::Text ("Toolbar to show and edit alpha and geometry of the current source.");
        ImGui::NextColumn();
        ImGui::Text(ICON_FA_TACHOMETER_ALT "  Metrics"); ImGui::NextColumn();
        ImGui::Text ("Monitoring of metrics on the system (e.g. FPS, RAM) and runtime (e.g. session duration).");

        ImGui::Columns(1);
        ImGui::PopTextWrapPos();
    }

    if (ImGui::CollapsingHeader("Files"))
    {
        {
            float H = ImGui::GetFrameHeightWithSpacing();
            ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_MenuBar;
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);

            ImGui::BeginChild("PlaylistHelp", ImVec2(0, 10.f * H), true, window_flags);
            if (ImGui::BeginMenuBar()) {
                ImGuiToolkit::Icon(4, 8);
                ImGui::Text (" Playlist");
                ImGui::EndMenuBar();
            }

            {
                ImGui::BeginChild("SessionHelp", ImVec2(0, 7.f * H), true, window_flags);
                if (ImGui::BeginMenuBar()) {
                    ImGuiToolkit::Icon(7, 1);
                    ImGui::Text (" Session");
                    ImGui::EndMenuBar();
                }

                {
                    ImGui::BeginChild("SourceHelp", ImVec2(0, 4.f * H), true, window_flags);
                    if (ImGui::BeginMenuBar()) {
                        ImGuiToolkit::Icon(14, 11);
                        ImGui::Text ("Source");
                        ImGui::EndMenuBar();
                    }

                    ImGui::BulletText("Video, image & session files");
                    ImGui::BulletText("Image sequence (image files)");
                    ImGui::BulletText("Input devices & streams (e.g. webcams)");
                    ImGui::BulletText("Patterns & generated graphics (e.g. text)");
                    ImGui::EndChild();
                }

                ImGui::PushTextWrapPos(width_window - 10.f);
                ImGui::Spacing();
                ImGui::Text ("A session contains several sources mixed together and keeps previous versions. "
                             "It is saved in a .mix file.");
                ImGui::PopTextWrapPos();
                ImGui::EndChild();
            }

            ImGui::PushTextWrapPos(width_window - 10.f);
            ImGui::Spacing();
            ImGui::Text ("A playlist keeps a list of sessions (or lists the .mix files in a folder) "
                         "for smooth transitions between files.");
            ImGui::PopTextWrapPos();
            ImGui::EndChild();

            ImGui::PopStyleVar();
        }
    }

    if (ImGui::CollapsingHeader("Sources"))
    {
        ImGui::Columns(2, "sourcecolumn", false); // 4-ways, with border
        ImGui::SetColumnWidth(0, width_column0);
        ImGui::PushTextWrapPos(width_window );

        ImGuiToolkit::Icon(ICON_SOURCE_VIDEO); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("Video"); ImGui::NextColumn();
        ImGui::Text ("Video file (*.mpg, *mov, *.avi, etc.). Decoding can be optimized with hardware acceleration.");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_IMAGE); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("Image"); ImGui::NextColumn();
        ImGui::Text ("Image file (*.jpg, *.png, etc.) or vector graphics (*.svg).");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_SESSION); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("Session"); ImGui::NextColumn();
        ImGui::Text ("Render a session (*.mix) as a source. Recursion is limited.");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_SEQUENCE); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("Sequence"); ImGui::NextColumn();
        ImGui::Text ("Set of images numbered sequentially (*.jpg, *.png, etc.).");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_RENDER); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("Loopback"); ImGui::NextColumn();
        ImGui::Text ("Loopback the rendering output as a source, with or without recursion.");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_DEVICE_SCREEN); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("Screen"); ImGui::NextColumn();
        ImGui::Text ("Screen capture of the entire screen or a selected window.");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_DEVICE); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("Device"); ImGui::NextColumn();
        ImGui::Text ("Connected webcam or frame grabber. Highest resolution and framerate automatically selected.");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_NETWORK); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("Shared"); ImGui::NextColumn();
        ImGui::Text ("Connected stream from another vimix in the local network (peer-to-peer).");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_SRT); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("SRT"); ImGui::NextColumn();
        ImGui::Text ("Connected Secure Reliable Transport (SRT) stream emitted on the network (e.g. broadcasted by vimix).");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_PATTERN); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("Pattern"); ImGui::NextColumn();
        ImGui::Text ("Algorithmically generated source; colors, grids, test patterns, timers...");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_GSTREAMER); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("GStreamer"); ImGui::NextColumn();
        ImGui::Text ("Custom gstreamer pipeline, as described in command line for gst-launch-1.0 (without the target sink).");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_CLONE); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("Clone"); ImGui::NextColumn();
        ImGui::Text ("Clones the frames of a source into another one and applies a GPU filter.");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_GROUP); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("Bundle"); ImGui::NextColumn();
        ImGui::Text ("Bundles together several sources and renders them as an internal session.");

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
        ImGui::Text(ICON_FA_BARS " Toggle left panel"); ImGui::NextColumn();
        ImGui::Text("INS"); ImGui::NextColumn();
        ImGui::Text(ICON_FA_PLUS " New source"); ImGui::NextColumn();
        ImGui::Text("DEL"); ImGui::NextColumn();
        ImGui::Text(ICON_FA_BACKSPACE " Delete source"); ImGui::NextColumn();
        ImGui::Text("TAB"); ImGui::NextColumn();
        ImGui::Text(ICON_FA_EXCHANGE_ALT " Switch Current source"); ImGui::NextColumn();
        ImGui::Text("[ 0 ][ i ]..[ 9 ]"); ImGui::NextColumn();
        ImGui::Text(ICON_FA_HASHTAG " Switch to source at index i"); ImGui::NextColumn();
        ImGui::Text(ALT_MOD); ImGui::NextColumn();
        ImGui::Text(ICON_FA_MOUSE_POINTER "  Activate Snap mouse cursor"); ImGui::NextColumn();
        ImGui::Text("F1"); ImGui::NextColumn();
        ImGui::Text(ICON_FA_BULLSEYE " Mixing view"); ImGui::NextColumn();
        ImGui::Text("F2"); ImGui::NextColumn();
        ImGui::Text(ICON_FA_OBJECT_UNGROUP " Geometry view"); ImGui::NextColumn();
        ImGui::Text("F3"); ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_WORKSPACE); ImGui::SameLine(0, IMGUI_SAME_LINE); ImGui::Text("Layers view"); ImGui::NextColumn();
        ImGui::Text("F4"); ImGui::NextColumn();
        ImGui::Text(ICON_FA_CHESS_BOARD "  Texturing view"); ImGui::NextColumn();
        ImGui::Text("F5"); ImGui::NextColumn();
        ImGui::Text(ICON_FA_TV " Displays view"); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_PREVIEW_OUT); ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_PREVIEW); ImGui::SameLine(0, IMGUI_SAME_LINE); ImGui::Text("Preview Output"); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_PREVIEW_SRC); ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_PREVIEW); ImGui::SameLine(0, IMGUI_SAME_LINE); ImGui::Text("Preview Source"); ImGui::NextColumn();
        ImGui::NextColumn();
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_ITALIC); ImGui::Text("Press & hold for momentary on/off"); ImGui::PopFont();
        ImGui::NextColumn();
        ImGui::Text(CTRL_MOD "TAB"); ImGui::NextColumn();
        ImGui::Text("Switch view"); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_FULLSCREEN); ImGui::NextColumn();
        ImGui::Text(ICON_FA_EXPAND_ALT " " TOOLTIP_FULLSCREEN " window"); ImGui::NextColumn();
        ImGui::Separator();
        ImGui::Text(SHORTCUT_OUTPUT); ImGui::NextColumn();
        ImGui::Text(ICON_FA_DESKTOP " " TOOLTIP_OUTPUT "window"); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_PLAYER); ImGui::NextColumn();
        ImGui::Text(ICON_FA_PLAY_CIRCLE " " TOOLTIP_PLAYER "window" ); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_TIMER); ImGui::NextColumn();
        ImGui::Text(ICON_FA_CLOCK " " TOOLTIP_TIMER "window"); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_INPUTS); ImGui::NextColumn();
        ImGui::Text(ICON_FA_HAND_PAPER " " TOOLTIP_INPUTS "window"); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_SHADEREDITOR); ImGui::NextColumn();
        ImGui::Text(ICON_FA_CODE " " TOOLTIP_SHADEREDITOR "window"); ImGui::NextColumn();
        ImGui::Text("ESC"); ImGui::NextColumn();
        ImGui::Text(" Hide | Show all windows"); ImGui::NextColumn();
        ImGui::NextColumn();
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_ITALIC); ImGui::Text("Press & hold for momentary on/off"); ImGui::PopFont();
        ImGui::NextColumn();
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
        ImGui::Text(SHORTCUT_UNDO); ImGui::NextColumn();
        ImGui::Text(MENU_UNDO); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_REDO); ImGui::NextColumn();
        ImGui::Text(MENU_REDO); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_CUT); ImGui::NextColumn();
        ImGui::Text(MENU_CUT " source"); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_COPY); ImGui::NextColumn();
        ImGui::Text(MENU_COPY " source"); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_PASTE); ImGui::NextColumn();
        ImGui::Text(MENU_PASTE); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_SELECTALL); ImGui::NextColumn();
        ImGui::Text(MENU_SELECTALL " sources"); ImGui::NextColumn();
        ImGui::Separator();
        ImGui::Text(SHORTCUT_CAPTURE_DISPLAY); ImGui::NextColumn();
        ImGui::Text(MENU_CAPTUREFRAME " display"); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_OUTPUTDISABLE); ImGui::NextColumn();
        ImGui::Text(MENU_OUTPUTDISABLE " display output"); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_RECORD); ImGui::NextColumn();
        ImGui::Text(MENU_RECORD " Output"); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_RECORDCONT); ImGui::NextColumn();
        ImGui::Text(MENU_RECORDCONT " recording"); ImGui::NextColumn();
        ImGui::Separator();
        ImGui::Text(SHORTCUT_CAPTURE_PLAYER); ImGui::NextColumn();
        ImGui::Text(MENU_CAPTUREFRAME " Player"); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_PLAY_PAUSE); ImGui::NextColumn();
        ImGui::Text(MENU_PLAY_PAUSE " selected videos"); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_PLAY_BEGIN); ImGui::NextColumn();
        ImGui::Text(MENU_PLAY_BEGIN " selected videos"); ImGui::NextColumn();
        ImGui::Text(ICON_FA_ARROW_DOWN " " ICON_FA_ARROW_UP " " ICON_FA_ARROW_DOWN " " ICON_FA_ARROW_RIGHT ); ImGui::NextColumn();
        ImGui::Text("Move the selection in the canvas"); ImGui::NextColumn();
        ImGui::Separator();
        ImGui::Text(SHORTCUT_CAPTURE_GUI); ImGui::NextColumn();
        ImGui::Text(MENU_CAPTUREGUI); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_LOGS); ImGui::NextColumn();
        ImGui::Text(IMGUI_TITLE_LOGS); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_HELP); ImGui::NextColumn();
        ImGui::Text(IMGUI_TITLE_HELP ); ImGui::NextColumn();
        ImGui::Text(SHORTCUT_QUIT); ImGui::NextColumn();
        ImGui::Text(MENU_QUIT); ImGui::NextColumn();

        ImGui::Columns(1);
    }

    ImGui::End();

}


///
/// NAVIGATOR
///
///

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
        showPannelSource( Mixer::manager().numSource() );

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
                Mixer::manager().unsetCurrentSource();
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

        // pannel menu
        if (selected_button[NAV_MENU])
        {
            RenderMainPannel(iconsize);
        }
        // pannel to manage transition
        else if (selected_button[NAV_TRANS])
        {
            RenderTransitionPannel(iconsize);
        }
        // pannel to create a source
        else if (selected_button[NAV_NEW])
        {
            RenderNewPannel(iconsize);
        }
        // pannel to configure a selected source
        else
        {
            if ( selected_index < 0 )
                showPannelSource(NAV_MENU);
            // most often, render current sources
            else if ( selected_index == Mixer::manager().indexCurrentSource())
                RenderSourcePannel(Mixer::manager().currentSource(), iconsize);
            // rarely its not the current source that is selected
            else {
                SourceList::iterator cs = Mixer::manager().session()->at( selected_index );
                if (cs != Mixer::manager().session()->end() )
                    RenderSourcePannel(*cs, iconsize);
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
void Navigator::RenderSourcePannel(Source *s, const ImVec2 &iconsize)
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

            // clone button
            if ( s->failed() ) {
                ImGuiToolkit::ButtonDisabled( ICON_FA_SHARE_SQUARE " Clone & Filter", ImVec2(ImGui::GetContentRegionAvail().x, 0));
            }
            else if ( ImGui::Button( ICON_FA_SHARE_SQUARE " Clone & Filter", ImVec2(ImGui::GetContentRegionAvail().x, 0)) ) {
                Mixer::manager().addSource ( (Source *) Mixer::manager().createSourceClone() );
                UserInterface::manager().showPannel(  Mixer::manager().numSource() );
            }

            // replace button
            if ( ImGui::Button( ICON_FA_PLUS_SQUARE " Replace", ImVec2(ImGui::GetContentRegionAvail().x, 0)) ) {
                // prepare panel for new source of same type
                MediaSource *file = dynamic_cast<MediaSource *>(s);
                MultiFileSource *sequence = dynamic_cast<MultiFileSource *>(s);
                PatternSource *generated = dynamic_cast<PatternSource *>(s);
                if (file != nullptr)
                    Settings::application.source.new_type = SOURCE_FILE;
                else if (sequence != nullptr)
                    Settings::application.source.new_type = SOURCE_SEQUENCE;
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
            if ( ImGui::Button( ACTION_DELETE, ImVec2(ImGui::GetContentRegionAvail().x, 0)) ) {
                Mixer::manager().deleteSource(s);
                Action::manager().store(sname + std::string(": Deleted"));
            }
            if ( Mixer::manager().session()->failedSources().size() > 1 ) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_FAILED, 1.));
                if ( ImGui::Button( ICON_FA_BACKSPACE " Delete all failed", ImVec2(ImGui::GetContentRegionAvail().x, 0)) ) {
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

        ImGui::Columns(1);
        ImGui::PopStyleVar();
        ImGui::PopFont();

        // Edit menu
        ImGui::SetCursorPosY(2.f * width_ - style.WindowPadding.x);

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
                if ( ImGuiToolkit::SelectableIcon(16, 16, "Custom gstreamer", false) )
                {
                    update_new_source = true;
                    generated_type = 0;
                    pattern_type = -1;
                }
                if ( ImGuiToolkit::SelectableIcon(0, 13, "Text", false) )
                {
                    update_new_source = true;
                    generated_type = 1;
                    pattern_type = -1;
                }
                if (ImGui::BeginMenu(ICON_FA_CUBES "  Static patterns"))
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
                if (ImGui::BeginMenu(ICON_FA_PLAY_CIRCLE "  Animated patterns"))
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
                    if (ImGui::Selectable( ICON_FA_EXTERNAL_LINK_ALT " Video test source" ) )
                         SystemToolkit::open("https://gstreamer.freedesktop.org/documentation/videotestsrc/index.html?gi-language=c#videotestsrc-page");
                    ImGui::EndCombo();
                }
                ImGui::SetCursorPos(pos_bot);
                // take action
                if (update_new_source)
                    new_source_preview_.setSource( Mixer::manager().createSourceStream(_description), "Custom");

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
                    if (ImGui::Selectable(ICON_FA_EXTERNAL_LINK_ALT " Pango syntax"))
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
                    new_source_preview_.setSource(Mixer::manager().createSourceText(_contents, res), "Text");
                }
            }
            // if pattern selected
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

                // 2. Screen capture (open selector if more than one window)
                if (ScreenCapture::manager().numWindow() > 0) {
                    std::string namewin = ScreenCapture::manager().name(0);
                    if ( ImGuiToolkit::SelectableIcon(ICON_SOURCE_DEVICE_SCREEN, namewin.c_str(), false) ) {
                        custom_connected = false;
                        if (ScreenCapture::manager().numWindow() > 1) {
                            new_source_preview_.setSource();
                            custom_screencapture = true;
                        }
                        else {
                            new_source_preview_.setSource( Mixer::manager().createSourceScreen(namewin), namewin);
                            custom_screencapture = false;
                        }
                    }
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
            if (ImGuiToolkit::IconButton(5, 15, "Reload list"))
                Device::manager().reload();
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
    if (enabled)
        ImGui::PushStyleColor( ImGuiCol_Text, ImGui::IsPopupOpen("MenuMousePointer") ? colors[ImGuiCol_DragDropTarget] : colors[ImGuiCol_Text] );
    else
        ImGui::PushStyleColor( ImGuiCol_Text, colors[ImGuiCol_TextDisabled] );

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
    float width = preview_width;
    float height = se->frame()->projectionSize().y * width / ( se->frame()->projectionSize().x * se->frame()->aspectRatio());
    if (height > preview_height - space) {
        height = preview_height - space;
        width = height * se->frame()->aspectRatio() * ( se->frame()->projectionSize().x / se->frame()->projectionSize().y);
    }
    // centered image
    ImGui::SetCursorPos( ImVec2(pos.x + 0.5f * (preview_width-width), pos.y) );
    ImGui::Image((void*)(uintptr_t) se->frame()->texture(), ImVec2(width, height));

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

    //
    // Menu for actions on current session
    ImGui::SetCursorPos( ImVec2( pos.x, pos.y + preview_height));
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::Combo("##Selectpanelsession", &Settings::application.pannel_current_session_mode,
                 ICON_FA_CODE_BRANCH "  Versions\0" ICON_FA_HISTORY " Undo history\0" ICON_FA_BORDER_STYLE "  Resolution\0");
    ImVec2 pos_bot = ImGui::GetCursorPos();   

    //
    // Current 2. RESOLUTION
    //
    if (Settings::application.pannel_current_session_mode > 1) {

        // Information and resolution
        const FrameBuffer *output = Mixer::manager().session()->frame();
        if (output)  {
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
    //
    // Current 1. UNDO History
    //
    else if (Settings::application.pannel_current_session_mode > 0) {

        static uint _over = 0;
        static uint64_t _displayed_over = 0;
        static bool _tooltip = 0;

        ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_bot.y -ImGui::GetFrameHeight() ));
        if ( Action::manager().current() > Action::manager().min() ) {
            if ( ImGuiToolkit::IconButton( ICON_FA_UNDO, "Undo" ) )
                Action::manager().undo();
        } else
            ImGui::TextDisabled( ICON_FA_UNDO );
        ImGui::SameLine();
        if ( Action::manager().current() < Action::manager().max() ) {
            if ( ImGuiToolkit::IconButton( ICON_FA_REDO, "Redo" ))
                Action::manager().redo();
        } else
            ImGui::TextDisabled( ICON_FA_REDO );

        // come back...
        ImGui::SetCursorPos(pos_bot);

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

        pos_bot = ImGui::GetCursorPos();

        // right buttons
        if ( Action::manager().max() > 1 ) {
            ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_top.y ));
            if (ImGuiToolkit::IconButton( 12, 14, "Clear history"))
                Action::manager().init("Reset");
        }

        ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_bot.y - 2.f * ImGui::GetFrameHeightWithSpacing()));
        ImGuiToolkit::HelpToolTip("History of actions (latest on top). "
                                 "Double-clic on an action to restore its status.\n\n"
                                 ICON_FA_MAP_MARKED_ALT "  Enable Show in view to automatically "
                                 "navigate to the view when the action is undone/redone.");
        // toggle button for shhow in view
        ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_bot.y - ImGui::GetFrameHeightWithSpacing()) );
        ImGuiToolkit::ButtonToggle(ICON_FA_MAP_MARKED_ALT, &Settings::application.action_history_follow_view, "Show in view");
    }
    //
    // Current 0. VERSIONS
    //
    else {
        static uint64_t _over = 0;
        static bool _tooltip = 0;

        ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_bot.y -ImGui::GetFrameHeight() ));
        if ( ImGuiToolkit::IconButton( ICON_FA_FILE_DOWNLOAD " +")) {
            UserInterface::manager().saveOrSaveAs(true);
        }
        if (ImGui::IsItemHovered())
            ImGuiToolkit::ToolTip("Save & Keep version");
        // come back...
        ImGui::SetCursorPos(pos_bot);

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

        // right buttons
        if (!snapshots.empty()) {
            ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_top.y ));
            if (ImGuiToolkit::IconButton( 12, 14, "Clear list"))
                Action::manager().clearSnapshots();
        }

        ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_bot.y - 2.f * ImGui::GetFrameHeightWithSpacing()));
        ImGuiToolkit::HelpToolTip("Previous versions of the session (latest on top). "
                                 "Double-clic on a version to restore it.\n\n"
                                 ICON_FA_CODE_BRANCH "  With iterative saving enabled, a new version "
                                 "is kept each time the session is saved.");
        // toggle button for versioning
        ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_bot.y - ImGui::GetFrameHeightWithSpacing()) );
        ImGuiToolkit::ButtonToggle(" " ICON_FA_CODE_BRANCH " ", &Settings::application.save_version_snapshot,"Iterative saving");

        ImGui::SetCursorPos( pos_bot );
    }

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
    //
    // Appearance
    //
    int v = Settings::application.accent_color;
    ImGui::SetCursorPosX(0.5f * width_);
    if (ImGui::RadioButton("##Color", &v, v)){
        Settings::application.accent_color = (v+1)%3;
        ImGuiToolkit::SetAccentColor(static_cast<ImGuiToolkit::accent_color>(Settings::application.accent_color));
        // ask Views to update
        View::need_deep_update_++;
    }
    if (ImGui::IsItemHovered())
        ImGuiToolkit::ToolTip("Change accent color");
    ImGui::SameLine();
    ImGui::SetCursorPosX(width_);
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

    //
    // Recording preferences
    //
    ImGuiToolkit::Spacing();
    ImGui::TextDisabled("Recording");

    // select CODEC and FPS
    ImGui::SetCursorPosX(width_);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::Combo("##Codec",
                 &Settings::application.record.profile,
                 VideoRecorder::profile_name,
                 IM_ARRAYSIZE(VideoRecorder::profile_name));
    ImGui::SameLine(0, IMGUI_SAME_LINE);
    if (ImGuiToolkit::TextButton("Codec"))
        Settings::application.record.profile = 0;

    ImGui::SetCursorPosX(width_);
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

    ImGui::SetCursorPosX(width_);
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
    ImGui::SetCursorPosX(width_);
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
        ImGui::SetCursorPosX(width_);
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

    //
    // Steaming preferences
    //
    ImGuiToolkit::Spacing();
    ImGui::TextDisabled("Stream");

    ImGuiToolkit::Indication("Peer-to-peer sharing local network\n\n"
                             "vimix can stream JPEG (default) or H264 (less bandwidth, higher encoding cost)", ICON_FA_SHARE_ALT_SQUARE);
    ImGui::SameLine(0);
    ImGui::SetCursorPosX(width_);
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
        ImGui::SetCursorPosX(width_);
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
        ImGui::SetCursorPosX(width_);
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
            ImGui::SetCursorPosX(width_);
            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            ImGui::Combo("SHM sink", &Settings::application.shm_method, "shmsink\0shmdatasink\0");
        }
    }

    //
    // OSC preferences
    //
    ImGuiToolkit::Spacing();
    ImGui::TextDisabled("OSC");

    std::ostringstream msg;
    msg << "Open Sound Control" << std::endl << std::endl;
    msg << "vimix accepts OSC messages sent by UDP on Port " << Settings::application.control.osc_port_receive;
    msg << " and replies on Port " << Settings::application.control.osc_port_send << std::endl << std::endl;
    msg << "Valid network addresses:" << std::endl;
    for (const auto& ips : NetworkToolkit::host_ips()){
        msg << "udp://" << ips << ":" << Settings::application.control.osc_port_receive << std::endl;
    }
    ImGuiToolkit::Indication(msg.str().c_str(), ICON_FA_NETWORK_WIRED);
    ImGui::SameLine(0);

    ImGui::SetCursorPosX(width_);
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

    ImGui::SetCursorPosX(width_);
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

    ImGui::SetCursorPosX(width_);
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
    ImGui::TextDisabled("System");
    ImGui::SameLine();

    ImGui::SetCursorPosX( pannel_width_ IMGUI_RIGHT_ALIGN);
    if ( ImGuiToolkit::IconButton(ICON_FA_SAVE,"Export settings\nYou can then "
                                               "launch vimix with the option "
                                               "'--settings filename.xml' "
                                               "to restore output windows and configuration.") ){
        // launch file dialog to select file to save settings
        if (UserInterface::manager().settingsexportdialog)
            UserInterface::manager().settingsexportdialog->open();
    }
    ImGui::Spacing();

    static bool need_restart = false;
    static bool vsync = (Settings::application.render.vsync > 0);
    static bool multi = (Settings::application.render.multisampling > 0);
    static bool gpu = Settings::application.render.gpu_decoding;
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
    change |= ImGuiToolkit::ButtonSwitch( "Vertical synchronization", &vsync);
    change |= ImGuiToolkit::ButtonSwitch( "Multisample antialiasing", &multi);
#endif
    if (change) {
        need_restart = ( vsync != (Settings::application.render.vsync > 0) ||
                        multi != (Settings::application.render.multisampling > 0) ||
                        gpu != Settings::application.render.gpu_decoding ||
                        audio != Settings::application.accept_audio );
    }
    if (need_restart) {
        ImGuiToolkit::Spacing();
        if (ImGui::Button( ICON_FA_POWER_OFF "  Quit & restart to apply", ImVec2(ImGui::GetContentRegionAvail().x - 50, 0))) {
            Settings::application.render.vsync = vsync ? 1 : 0;
            Settings::application.render.multisampling = multi ? 3 : 0;
            Settings::application.render.gpu_decoding = gpu;
            Settings::application.accept_audio = audio;
            if (UserInterface::manager().TryClose())
                Rendering::manager().close();
        }
    }

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

///
/// UTILITY
///

#define SEGMENT_ARRAY_MAX 1000
#define MAXSIZE 65535

#include <glm/gtc/type_ptr.hpp>

void ShowSandbox(bool* p_open)
{
    ImGui::SetNextWindowSize(ImVec2(400, 260), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin( ICON_FA_BABY_CARRIAGE "  Sandbox", p_open))
    {
        ImGui::End();
        return;
    }

    ImGui::Text("Testing sandox");
    ImGui::Separator();

    ImGui::Separator();
    ImGui::Text("Reset GST");

    if (ImGui::Button("RESET")){




    }


    ImGui::Text("Source list");
    Session *se = Mixer::manager().session();
    for (auto sit = se->begin(); sit != se->end(); ++sit) {
        ImGui::Text("[%s] %s ", std::to_string((*sit)->id()).c_str(), (*sit)->name().c_str());
    }

    ImGui::Separator();
    ImGui::Text("Current source");

    Source *so = Mixer::manager().currentSource();
    if (so) {
        glm::vec2 v = so->attractor(0);
        if (ImGui::SliderFloat2("LL corner", glm::value_ptr(v), 0.0, 2.0))
            so->setAttractor(0,v);
        v = so->attractor(1);
        if (ImGui::SliderFloat2("UL corner", glm::value_ptr(v), 0.0, 2.0))
            so->setAttractor(1,v);
        v = so->attractor(2);
        if (ImGui::SliderFloat2("LR corner", glm::value_ptr(v), 0.0, 2.0))
            so->setAttractor(2,v);
        v = so->attractor(3);
        if (ImGui::SliderFloat2("UR corner", glm::value_ptr(v), 0.0, 2.0))
            so->setAttractor(3,v);
    }


//    ImGui::Separator();
//    static char str[128] = "";
//    ImGui::InputText("Command", str, IM_ARRAYSIZE(str));
//    if ( ImGui::Button("Execute") )
//        SystemToolkit::execute(str);


    //    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_BOLD);
    //    ImGui::Text("This text is in BOLD");
    //    ImGui::PopFont();
    //    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_DEFAULT);
    //    ImGui::Text("This text is in REGULAR");
    //    ImGui::PopFont();
    //    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_ITALIC);
    //    ImGui::Text("This text is in ITALIC");
    //    ImGui::PopFont();

    //    ImGui::Text("IMAGE of Font");

    //    ImGuiToolkit::ImageGlyph(ImGuiToolkit::FONT_DEFAULT, 'v');
    //    ImGui::SameLine();
    //    ImGuiToolkit::ImageGlyph(ImGuiToolkit::FONT_BOLD, 'i');
    //    ImGui::SameLine();
    //    ImGuiToolkit::ImageGlyph(ImGuiToolkit::FONT_ITALIC, 'm');
    //    ImGui::SameLine();
    //    ImGuiToolkit::ImageGlyph(ImGuiToolkit::FONT_MONO, 'i');
    //    ImGui::SameLine();
    //    ImGuiToolkit::ImageGlyph(ImGuiToolkit::FONT_LARGE, 'x');

//    static char str0[128] = "àöäüèáû вторая строчка";
//    ImGui::InputText("##inputtext", str0, IM_ARRAYSIZE(str0));
//    std::string tra = BaseToolkit::transliterate(std::string(str0));
//    ImGui::Text("Transliteration: '%s'", tra.c_str());

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
