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

#include "IconsFontAwesome5.h"
#include "Source/Source.h"
#include "Window/OutputPreviewWindow.h"
#include "View/RenderView.h"
#include "RenderingManager.h"
#include <string>
#define PLOT_ARRAY_SIZE 180
#define WINDOW_TOOLBOX_ALPHA 0.35f
#define WINDOW_TOOLBOX_DIST_TO_BORDER 10.f
#define RECORDER_INDICATION_SIZE 30.f

#include <iostream>
#include <sstream>
#include <iomanip>
#include <sstream>
#include <fstream>

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
#include "Toolkit/SystemToolkit.h"
#include "Toolkit/DialogToolkit.h"
#include "Toolkit/BaseToolkit.h"
#include "Toolkit/GstToolkit.h"
#include "Toolkit/ImGuiToolkit.h"
#include "ControlManager.h"
#include "ActionManager.h"
#include "Resource.h"
#include "SessionCreator.h"
#include "Mixer.h"
#include "Recorder.h"
#include "Source/SourceCallback.h"
#include "Source/ShaderSource.h"
#include "Source/SessionSource.h"
#include "MousePointer.h"
#include "Playlist.h"
#include "FrameGrabbing.h"

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
    ImGuiToolkit::SetFont(ImGuiToolkit::FONT_ITALIC, "Roboto-Italic", int(base_font_size) + 4 );
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
    if ( !Settings::application.pannel_always_visible )
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
            // Create bundle
            if (Mixer::manager().selectionCanBeGroupped()) {
                Mixer::manager().groupSelection();
                // switch pannel to show first source (created)
                navigator.showPannelSource(Mixer::manager().indexCurrentSource());
            } 
            else {
                bool can_bundle = false;
                if (Mixer::manager().currentSource() != nullptr){
                    can_bundle = !(Mixer::manager().currentSource()->icon() == glm::ivec2(ICON_SOURCE_GROUP)) &&
                    !(Mixer::manager().currentSource()->cloned() || Mixer::manager().currentSource()->icon() == glm::ivec2(ICON_SOURCE_CLONE));
                }
                if (can_bundle) {
                    Mixer::manager().groupCurrent();
                    // switch pannel to show first source (created)
                    navigator.showPannelSource(Mixer::manager().indexCurrentSource());
                }
            }
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
            Outputs::manager().start(new PNGRecorder(SystemToolkit::base_filename( Mixer::manager().session()->filename())));
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
    if (DialogToolkit::FileDialog::busy() || DialogToolkit::ColorPickerDialog::instance().busy())
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

    // overlay to ensure color dialog is closed after use
    if (DialogToolkit::ColorPickerDialog::instance().busy()){
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

    // render Recording indication overlay
    if (Outputs::manager().enabled( FrameGrabber::GRABBER_VIDEO ) || 
        Outputs::manager().enabled( FrameGrabber::GRABBER_GPU ) ) {
        ImGuiIO& io = ImGui::GetIO();
        ImVec2 center = ImVec2(io.DisplaySize.x - RECORDER_INDICATION_SIZE * 1.5f, io.DisplaySize.y - RECORDER_INDICATION_SIZE * 1.5f);
        int a = 150 + (int) ( 50.f * sin( (float) Runtime() / 100000000.f ));
        ImGui::GetForegroundDrawList()->AddCircleFilled(center, RECORDER_INDICATION_SIZE, IM_COL32(255, 10, 10, a), 0);
    }

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
}

void UserInterface::showMenuBundle()
{       
    ImVec4 disabled_color = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    ImGui::MenuItem("Create bundle with:", NULL, false, false);

    // 
    // Menu Bundle current source
    // 
    bool is_bundle = false;
    bool is_clone = false;
    bool has_current = Mixer::manager().currentSource() != nullptr;
    if (has_current){
        is_bundle = Mixer::manager().currentSource()->icon() == glm::ivec2(ICON_SOURCE_GROUP);
        is_clone = Mixer::manager().currentSource()->cloned() || Mixer::manager().currentSource()->icon() == glm::ivec2(ICON_SOURCE_CLONE);
    }
    // cannot be bundled if no current source, or if current source is a bundle or a clone
    if (has_current && (is_bundle || is_clone) )
        // disabled menu is tinted in red to show there is a problem
        disabled_color.x = 0.7f;
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, disabled_color);
    if (ImGuiToolkit::MenuItemIcon(11, 2, " Current source", SHORTCUT_BUNDLE, false, 
                        has_current && !is_bundle && !is_clone)) {
        Mixer::manager().groupCurrent();
        // switch pannel to show first source (created)
        navigator.showPannelSource(Mixer::manager().indexCurrentSource());
    }
    // disabled menu explains the problem with tooltip
    if (has_current && (is_bundle || is_clone) && ImGui::IsItemHovered()) {
        ImGuiToolkit::ToolTip("Bundles, clones or cloned source cannot be bundled.");
    }
    ImGui::PopStyleColor();

    // 
    // Menu Bundle selected sources
    // 
    bool has_selection = Mixer::manager().selection().size() > 1;
    bool cangroup_selection = Mixer::manager().selectionCanBeGroupped();
    disabled_color = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    // cannot be bundled if selection is too small or if selection cannot be grouped
    if (has_selection && !cangroup_selection)
        // disabled menu is tinted in red to show there is a problem
        disabled_color.x = 0.7f;
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, disabled_color);
    if (ImGuiToolkit::MenuItemIcon(11, 2, " Selected sources", SHORTCUT_BUNDLE, false, 
                        cangroup_selection)) {
        Mixer::manager().groupSelection();
        // switch pannel to show first source (created)
        navigator.showPannelSource(Mixer::manager().indexCurrentSource());
    }
    // disabled menu explains the problem with tooltip
    if (has_selection && !cangroup_selection && ImGui::IsItemHovered()) {
        ImGuiToolkit::ToolTip("Selection must be contiguous in layer & clones cannot be separated from their origin source.");
    }
    ImGui::PopStyleColor();

    // 
    // Menu Bundle all active sources
    // 
    if (ImGuiToolkit::MenuItemIcon(11, 2, " All active sources", NULL, false, 
                        Mixer::manager().numSource() > 0)) {
        // create a new group session with only active sources
        Mixer::manager().groupAll( true );
        // switch pannel to show first source (created)
        navigator.showPannelSource(Mixer::manager().indexCurrentSource());
    }
    // 
    // Menu Bundle all sources
    // 
    if (ImGuiToolkit::MenuItemIcon(11, 2, " Everything", NULL, false, 
                        Mixer::manager().numSource() > 0)) {
        // create a new group session the whole session
        Mixer::manager().groupAll( false );
        // switch pannel to show first source (created)
        navigator.showPannelSource(0);
    }
    // 
    // Menu to unbundle selected bundle sources
    // 
    ImGui::Separator();
    if (ImGuiToolkit::MenuItemIcon(7, 2, " Uncover selected bundle", NULL, false, 
                        is_bundle)) {
        // import sourses of bundle 
        Mixer::manager().import( dynamic_cast<SessionSource*>(Mixer::manager().currentSource()) );
    }
    if (ImGuiToolkit::MenuItemIcon(7, 2, " Uncover every bundles", NULL, false, 
                        Mixer::manager().numSource() > 0)) {
        // ungroup all bundle sources
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
            // show player
            sourcecontrol.setVisible(true);
            // show code editor for shader sources
            ShaderSource *sc = dynamic_cast<ShaderSource *>(s);
            if (sc != nullptr)
                shadercontrol.setVisible(true);
            // select
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
            if (ImGuiToolkit::IconButton(ICON_FA_TIMES))
                show_preview = PREVIEW_NONE;
            if (ImGui::IsItemHovered()) {

                ImGui::SameLine();
                if (show_preview == PREVIEW_SOURCE)
                    ImGui::Text("Preview Player source");
                else
                    ImGui::Text("Preview Display output");  
            }
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
    ImGui::Text("\nvimix is licensed under GNU GPL version 3 or later.\n" UNICODE_COPYRIGHT " 2019-2025 Bruno Herbelin.");

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
            if ( ImGui::MenuItem( ICON_FA_COMMENT "  Online Discussion") ) {
                SystemToolkit::open("https://github.com/brunoherbelin/vimix/discussions");
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
        ImGui::Text("OSC"); ImGui::NextColumn();
        ImGuiToolkit::ButtonOpenUrl("Open Sound Control API", "https://github.com/brunoherbelin/vimix/wiki/Open-Sound-Control-API", ImVec2(ImGui::GetContentRegionAvail().x, 0));
        ImGui::NextColumn();
        ImGui::Text("Command"); ImGui::NextColumn();
        ImGuiToolkit::ButtonOpenUrl("Command line usage", "https://github.com/brunoherbelin/vimix/wiki/Command-line-usage", ImVec2(ImGui::GetContentRegionAvail().x, 0));

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
        ImGui::Text ("Setup the metronome (Ableton Link) that is used to trigger actions, and keep track of time with a stopwatch.");
        ImGui::NextColumn();
        ImGui::Text(ICON_FA_HAND_PAPER "  Inputs"); ImGui::NextColumn();
        ImGui::Text ("Map several user inputs (e.g. keyboard, joystick) to custom actions in the session.");
        ImGui::NextColumn();
        ImGui::Text(ICON_FA_CODE "  Shader"); ImGui::NextColumn();
        ImGui::Text ("Edit and compiles the GLSL code of custom shader sources and filters. Load .glsl text files.");
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
        ImGui::Text ("Image file (*.jpg, *.png, etc.) or vector graphics (*.svg). Transparency is supported.");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_SESSION); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("Session"); ImGui::NextColumn();
        ImGui::Text ("Render a vimix session (*.mix) as a source. Recursion is limited.");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_SEQUENCE); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("Sequence"); ImGui::NextColumn();
        ImGui::Text ("Displays a set of images numbered sequentially (*.jpg, *.png, etc.) or assemble a video from a selection of images.");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_RENDER); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("Loopback"); ImGui::NextColumn();
        ImGui::Text ("Loopback the rendering output as a source, with or without recursion.");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_DEVICE_SCREEN); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("Screen"); ImGui::NextColumn();
        ImGui::Text ("Screen capture of the entire screen or of a selected window (Linux X11 only).");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_DEVICE); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("Device"); ImGui::NextColumn();
        ImGui::Text ("Connected webcam or frame grabber. Highest resolution and framerate automatically selected.");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_NETWORK); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("Shared"); ImGui::NextColumn();
        ImGui::Text ("Peer-to-peer stream from another vimix on the same machine (SHM shared memory) or in the local network (RTP Real-time Transport Protocol).");
        ImGuiToolkit::ButtonOpenUrl("Example script", "https://github.com/brunoherbelin/vimix/blob/master/rsc/osc/vimix_connect.sh", ImVec2(ImGui::GetContentRegionAvail().x, 0));
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_SRT); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("SRT"); ImGui::NextColumn();
        ImGui::Text ("Connected Secure Reliable Transport (SRT) stream emitted on the network (e.g. broadcasted by vimix).");
        ImGuiToolkit::ButtonOpenUrl("Documentation", "https://github.com/brunoherbelin/vimix/wiki/SRT-stream-I-O", ImVec2(ImGui::GetContentRegionAvail().x, 0));
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_PATTERN); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("Pattern"); ImGui::NextColumn();
        ImGui::Text ("Algorithmically generated source, static or animated (colors, grids, test patterns, timers, etc.).");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_TEXT); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("Text"); ImGui::NextColumn();
        ImGui::Text ("Renders rich text or subtitle files (SRT), with layout control (alignment and margins) and Pango markup and font description.");
        ImGuiToolkit::ButtonOpenUrl("Pango font", "https://docs.gtk.org/Pango/pango_fonts.html", ImVec2(ImGui::GetContentRegionAvail().x, 0));
        ImGuiToolkit::ButtonOpenUrl("Pango markup", "https://docs.gtk.org/Pango/pango_markup.html", ImVec2(ImGui::GetContentRegionAvail().x, 0));
        ImGuiToolkit::ButtonOpenUrl("SubRip file format", "https://en.wikipedia.org/wiki/SubRip", ImVec2(ImGui::GetContentRegionAvail().x, 0));
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_SHADER); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("Shader"); ImGui::NextColumn();
        ImGui::Text ("Custom GLSL shader, compatible with ShaderToy syntax.");
        ImGuiToolkit::ButtonOpenUrl("About GLSL", "https://www.khronos.org/opengl/wiki/OpenGL_Shading_Language", ImVec2(ImGui::GetContentRegionAvail().x, 0));
        ImGuiToolkit::ButtonOpenUrl("Browse shadertoy.com", "https://www.shadertoy.com", ImVec2(ImGui::GetContentRegionAvail().x, 0));
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_GSTREAMER); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("GStreamer"); ImGui::NextColumn();
        ImGui::Text ("Custom gstreamer pipeline, as described in command line for gst-launch-1.0 (without the target sink).");
        ImGuiToolkit::ButtonOpenUrl("GST Documentation", "https://gstreamer.freedesktop.org/documentation/tools/gst-launch.html?gi-language=c#pipeline-description", ImVec2(ImGui::GetContentRegionAvail().x, 0));
        ImGuiToolkit::ButtonOpenUrl("Examples", "https://github.com/thebruce87m/gstreamer-cheat-sheet", ImVec2(ImGui::GetContentRegionAvail().x, 0));
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_CLONE); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("Clone"); ImGui::NextColumn();
        ImGui::Text ("Clones the frames of a source into another one and optionnaly applies a filter (see below).");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_SOURCE_GROUP); ImGui::SameLine(0, IMGUI_SAME_LINE);ImGui::Text("Bundle"); ImGui::NextColumn();
        ImGui::Text ("Bundles together several sources and renders them as an internal session, "
            " e.g., select multiple sources in the Layers view and choose 'Bundle selection'.");

        ImGui::Columns(1);
        ImGui::PopTextWrapPos();
    }

    if (ImGui::CollapsingHeader("Filters"))
    {
        ImGui::Text("Select 'Clone & Filter' on a source to access filters;");
        ImGuiToolkit::ButtonOpenUrl("Filters and ShaderToy reference", "https://github.com/brunoherbelin/vimix/wiki/Filters-and-ShaderToy", ImVec2(ImGui::GetContentRegionAvail().x, 0));


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
        ImGui::Text ("React to button press and axis movement on a gamepad or a joystick.");
        ImGui::NextColumn();
        ImGui::Text(ICON_FA_CLOCK " Timer"); ImGui::NextColumn();
        ImGui::Text ("React to beats of the metronome (configured in the Timer window).");

        ImGui::Columns(1);
        ImGui::PopTextWrapPos();
    }
        
    if (ImGui::CollapsingHeader("Snap Cursors"))
    {
        ImGui::Text("Maintain the [" ALT_MOD "] key to enable mouse cursor snapping;");
        ImGui::Columns(2, "cursorscolumn", false); // 4-ways, with border
        ImGui::SetColumnWidth(0, width_column0);
        ImGui::PushTextWrapPos(width_window );
        ImGuiToolkit::Icon(ICON_POINTER_GRID); ImGui::SameLine(0, IMGUI_SAME_LINE);
        ImGui::Text("Grid"); ImGui::NextColumn();
        ImGui::Text ("Snap the mouse cursor to the closest grid intersection point.");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_POINTER_LINEAR); ImGui::SameLine(0, IMGUI_SAME_LINE);
        ImGui::Text("Line"); ImGui::NextColumn();
        ImGui::Text ("Move the mouse cursor at constant speed towards the target.");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_POINTER_SPRING); ImGui::SameLine(0, IMGUI_SAME_LINE);
        ImGui::Text("Spring"); ImGui::NextColumn();
        ImGui::Text ("Simulate spring physics with mass and damping for smooth cursor motion.");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_POINTER_WIGGLY); ImGui::SameLine(0, IMGUI_SAME_LINE);
        ImGui::Text("Wiggly"); ImGui::NextColumn();
        ImGui::Text ("Add a circular wiggling motion to the cursor movement.");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_POINTER_BROWNIAN); ImGui::SameLine(0, IMGUI_SAME_LINE);
        ImGui::Text("Brownian"); ImGui::NextColumn();
        ImGui::Text ("Add random Brownian motion to the cursor movement.");
        ImGui::NextColumn();
        ImGuiToolkit::Icon(ICON_POINTER_METRONOME); ImGui::SameLine(0, IMGUI_SAME_LINE);
        ImGui::Text("Metronome"); ImGui::NextColumn();
        ImGui::Text ("Jump the cursor to the target in synchrony with metronome beats.");
        ImGui::NextColumn();
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
        ImGui::Text(SHORTCUT_BUNDLE); ImGui::NextColumn();
        ImGui::Text("Create bundle with current source or selection"); ImGui::NextColumn();
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
