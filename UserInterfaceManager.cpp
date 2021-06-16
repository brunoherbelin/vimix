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

#include "TextEditor.h"
static TextEditor editor;

#include "UserInterfaceManager.h"
#define PLOT_ARRAY_SIZE 180
#define LABEL_AUTO_MEDIA_PLAYER ICON_FA_CARET_SQUARE_RIGHT "  Dynamic selection"
#define LABEL_STORE_SELECTION "  Store selection"

// utility functions
void ShowAboutGStreamer(bool* p_open);
void ShowAboutOpengl(bool* p_open);
void ShowSandbox(bool* p_open);
void SetMouseCursor(ImVec2 mousepos, View::Cursor c = View::Cursor());
void SetNextWindowVisible(ImVec2 pos, ImVec2 size, float margin = 180.f);

// static objects for multithreaded file dialog
const std::chrono::milliseconds timeout = std::chrono::milliseconds(4);
static std::atomic<bool> fileDialogPending_ = false;

static std::vector< std::future<std::string> > saveSessionFileDialogs;
static std::vector< std::future<std::string> > openSessionFileDialogs;
static std::vector< std::future<std::string> > importSessionFileDialogs;
static std::vector< std::future<std::string> > fileImportFileDialogs;
static std::vector< std::future<std::string> > recentFolderFileDialogs;
static std::vector< std::future<std::string> > recordFolderFileDialogs;

static std::vector< std::future<FrameGrabber *> > _video_recorders;
FrameGrabber *delayTrigger(FrameGrabber *g, std::chrono::milliseconds delay) {
    std::this_thread::sleep_for (delay);
    return g;
}


UserInterface::UserInterface()
{
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

    return true;
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
            // Save Session
            if (shift_modifier_active || Mixer::manager().session()->filename().empty())
                selectSaveFilename();
            else
                Mixer::manager().save();
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_W )) {
            // New Session
            Mixer::manager().close();
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_SPACE )) {
            // New Session
            Mixer::manager().session()->setActive( !Mixer::manager().session()->active() );
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_L )) {
            // Logs
            Settings::application.widget.logs = !Settings::application.widget.logs;
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_T )) {
            // Logs
            Settings::application.widget.toolbox = !Settings::application.widget.toolbox;
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_E )) {
            // Shader Editor
            Settings::application.widget.shader_editor = !Settings::application.widget.shader_editor;
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_D )) {
            // Logs
            Settings::application.widget.preview = !Settings::application.widget.preview;
            if (Settings::application.widget.preview_view != Settings::application.current_view) {
                Settings::application.widget.preview_view = -1;
                Settings::application.widget.preview = true;
            }
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_P )) {
            // Logs
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
                    video_recorder_->stop();
                    video_recorder_ = nullptr;
                }
                else {
                    _video_recorders.emplace_back( std::async(std::launch::async, delayTrigger, new VideoRecorder, std::chrono::seconds(Settings::application.record.delay)) );
                }
            }
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_Y )) {
            Action::manager().snapshot( "Snap" );
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
        else if (ImGui::IsKeyPressed( GLFW_KEY_F ) && shift_modifier_active) {
            Rendering::manager().mainWindow().toggleFullscreen();
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_N ) && shift_modifier_active) {
            Mixer::manager().session()->addNote();
        }

    }
    // No CTRL modifier
    else {
        ctrl_modifier_active = false;

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
        // button esc
        else if (ImGui::IsKeyPressed( GLFW_KEY_ESCAPE )) {
            // 1. escape fullscreen
            if (Rendering::manager().mainWindow().isFullscreen())
                Rendering::manager().mainWindow().exitFullscreen();
            // 2. hide panel
            else if (navigator.pannelVisible())
                navigator.hidePannel();
            // 3. hide windows
            else if (Settings::application.widget.preview || Settings::application.widget.media_player || Settings::application.widget.history)  {
                Settings::application.widget.preview = false;
                Settings::application.widget.media_player = false;
                Settings::application.widget.history = false;
            }
            // 4. cancel selection
            else if (!Mixer::selection().empty()) {
                Mixer::manager().unsetCurrentSource();
                Mixer::selection().clear();
            }
        }
        // button home to toggle menu
        else if (ImGui::IsKeyPressed( GLFW_KEY_HOME ))
            navigator.togglePannelMenu();
        // button home to toggle menu
        else if (ImGui::IsKeyPressed( GLFW_KEY_INSERT ))
            navigator.togglePannelNew();
        // normal keys in workspace // make sure no entry / window box is active
        else if ( !ImGui::IsAnyWindowFocused() ){
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
                Mixer::manager().view()->arrow( glm::vec2(0.f, 1.f) );

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

void UserInterface::selectSaveFilename()
{
    // launch file dialog to select a session filename to save
    if ( saveSessionFileDialogs.empty()) {
        saveSessionFileDialogs.emplace_back( std::async(std::launch::async, DialogToolkit::saveSessionFileDialog, Settings::application.recentSessions.path) );
        fileDialogPending_ = true;
    }
    navigator.hidePannel();
}

void UserInterface::selectOpenFilename()
{
    // launch file dialog to select a session filename to open
    if ( openSessionFileDialogs.empty()) {
        openSessionFileDialogs.emplace_back( std::async(std::launch::async, DialogToolkit::openSessionFileDialog, Settings::application.recentSessions.path) );
        fileDialogPending_ = true;
    }
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

    // handle FileDialog

    // if a file dialog future was registered
    if ( !openSessionFileDialogs.empty() ) {
        // check that file dialog thread finished
        if (openSessionFileDialogs.back().wait_for(timeout) == std::future_status::ready ) {
            // get the filename from this file dialog
            std::string open_filename = openSessionFileDialogs.back().get();
            if (!open_filename.empty()) {
                Mixer::manager().open(open_filename);
                Settings::application.recentSessions.path = SystemToolkit::path_filename(open_filename);
            }
            // done with this file dialog
            openSessionFileDialogs.pop_back();
            fileDialogPending_ = false;
        }
    }
    if ( !importSessionFileDialogs.empty() ) {
        // check that file dialog thread finished
        if (importSessionFileDialogs.back().wait_for(timeout) == std::future_status::ready ) {
            // get the filename from this file dialog
            std::string open_filename = importSessionFileDialogs.back().get();
            if (!open_filename.empty()) {
                Mixer::manager().import(open_filename);
                Settings::application.recentSessions.path = SystemToolkit::path_filename(open_filename);
            }
            // done with this file dialog
            importSessionFileDialogs.pop_back();
            fileDialogPending_ = false;
        }
    }
    if ( !saveSessionFileDialogs.empty() ) {
        // check that file dialog thread finished
        if (saveSessionFileDialogs.back().wait_for(timeout) == std::future_status::ready ) {
            // get the filename from this file dialog
            std::string save_filename = saveSessionFileDialogs.back().get();
            if (!save_filename.empty()) {
                Mixer::manager().saveas(save_filename);
                Settings::application.recentSessions.path = SystemToolkit::path_filename(save_filename);
            }
            // done with this file dialog
            saveSessionFileDialogs.pop_back();
            fileDialogPending_ = false;
        }
    }

    // overlay to ensure file dialog is modal
    if (fileDialogPending_){
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
    // verify the video recorder is valid
    FrameGrabbing::manager().verify(&video_recorder_);
    if (video_recorder_ && video_recorder_->duration() > Settings::application.record.timeout ){
        video_recorder_->stop();
        video_recorder_ = nullptr;
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
        if (Settings::application.widget.history)
            RenderHistory();
        if (Settings::application.widget.media_player && ( Settings::application.widget.media_player_view < 0 ||
            Settings::application.widget.media_player_view == Settings::application.current_view ))
            sourcecontrol.Render();
        if (Settings::application.widget.shader_editor)
            RenderShaderEditor();
        if (Settings::application.widget.logs)
            Log::ShowLogWindow(&Settings::application.widget.logs);        
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
        Mixer::manager().save();

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
    if ( ImGui::MenuItem( ICON_FA_STAR "+ Snapshot", CTRL_MOD "Y") )
        Action::manager().snapshot( "Snap" );
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

    if (ImGui::MenuItem( ICON_FA_FILE_EXPORT " Import")) {
        // launch file dialog to open a session file
        if ( importSessionFileDialogs.empty()) {
            importSessionFileDialogs.emplace_back( std::async(std::launch::async, DialogToolkit::openSessionFileDialog, Settings::application.recentSessions.path) );
            fileDialogPending_ = true;
        }
        navigator.hidePannel();
    }
    if (ImGui::MenuItem( ICON_FA_FILE_DOWNLOAD "  Save", CTRL_MOD "S")) {
        if (Mixer::manager().session()->filename().empty())
            selectSaveFilename();
        else {
            Mixer::manager().save();
            navigator.hidePannel();
        }
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

void UserInterface::RenderHistory()
{
    float history_height = 5.f * ImGui::GetFrameHeightWithSpacing();
    ImVec2 MinWindowSize = ImVec2(250.f, history_height);

    ImGui::SetNextWindowPos(ImVec2(1180, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(MinWindowSize, ImVec2(FLT_MAX, FLT_MAX));
    if ( !ImGui::Begin(IMGUI_TITLE_HISTORY, &Settings::application.widget.history, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar |  ImGuiWindowFlags_NoCollapse ))
    {
        ImGui::End();
        return;
    }

    // menu (no title bar)
    bool tmp = false;
    if (ImGui::BeginMenuBar())
    {
        if (ImGuiToolkit::IconButton(4,16))
            Settings::application.widget.history = false;
        if (ImGui::BeginMenu(IMGUI_TITLE_HISTORY))
        {
            if ( ImGui::MenuItem( ICON_FA_UNDO "  Undo", CTRL_MOD "Z") )
                Action::manager().undo();
            if ( ImGui::MenuItem( ICON_FA_REDO "  Redo", CTRL_MOD "Shift+Z") )
                Action::manager().redo();

            ImGui::MenuItem( ICON_FA_DIRECTIONS " Follow view", nullptr, &Settings::application.action_history_follow_view);

            if ( ImGui::MenuItem( ICON_FA_TIMES "  Close") )
                Settings::application.widget.history = false;

            ImGui::EndMenu();
        }
        if ( ImGui::Selectable(ICON_FA_UNDO, &tmp, ImGuiSelectableFlags_None, ImVec2(20,0)))
            Action::manager().undo();
        if ( ImGui::Selectable(ICON_FA_REDO, &tmp, ImGuiSelectableFlags_None, ImVec2(20,0)))
            Action::manager().redo();

        ImGui::EndMenuBar();
    }

    if (ImGui::ListBoxHeader("##History", ImGui::GetContentRegionAvail() ) )
    {
        for (uint i = Action::manager().max(); i > 0; i--) {

            std::string step_label_ = Action::manager().label(i);

            bool enable = i == Action::manager().current();
            if (ImGui::Selectable( step_label_.c_str(), &enable, ImGuiSelectableFlags_AllowDoubleClick )) {

                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    Action::manager().stepTo(i);
            }
        }
        ImGui::ListBoxFooter();
    }

    ImGui::End();
}

void UserInterface::RenderPreview()
{
#if defined(LINUX)
    bool openInitializeSystemLoopback = false;
#endif

    struct CustomConstraints // Helper functions for aspect-ratio constraints
    {
        static void AspectRatio(ImGuiSizeCallbackData* data) {
            float *ar = (float*) data->UserData;
            data->DesiredSize.y = (data->CurrentSize.x / (*ar)) + 35.f;
        }
    };

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
        if ( !recordFolderFileDialogs.empty() ) {
            // check that file dialog thread finished
            if (recordFolderFileDialogs.back().wait_for(timeout) == std::future_status::ready ) {
                // get the folder from this file dialog
                Settings::application.record.path = recordFolderFileDialogs.back().get();
                // done with this file dialog
                recordFolderFileDialogs.pop_back();
                fileDialogPending_ = false;
            }
        }

        // menu (no title bar)
        if (ImGui::BeginMenuBar())
        {
            if (ImGuiToolkit::IconButton(4,16))
                Settings::application.widget.preview = false;
            if (ImGui::BeginMenu(IMGUI_TITLE_PREVIEW))
            {
                glm::ivec2 p = FrameBuffer::getParametersFromResolution(output->resolution());
                std::ostringstream info;
                info << "Resolution " << output->width() << "x" << output->height();
                if (p.x > -1)
                    info << "  " << FrameBuffer::aspect_ratio_name[p.x] ;
                ImGui::MenuItem(info.str().c_str(), nullptr, false, false);
                // cannot change resolution when recording
                if (video_recorder_ == nullptr && p.y > -1) {
                    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                    if (ImGui::Combo("Height", &p.y, FrameBuffer::resolution_name, IM_ARRAYSIZE(FrameBuffer::resolution_name) ) )
                    {
                        glm::vec3 res = FrameBuffer::getResolutionFromParameters(p.x, p.y);
                        Mixer::manager().session()->setResolution(res);
                    }
                }
                if ( ImGui::MenuItem( ICON_FA_PLUS "  Insert Rendering Source") )
                    Mixer::manager().addSource( Mixer::manager().createSourceRender() );

                if ( ImGui::MenuItem( ICON_FA_WINDOW_RESTORE "  Show output window") )
                    Rendering::manager().outputWindow().show();

                ImGui::Separator();

                bool pinned = Settings::application.widget.preview_view == Settings::application.current_view;
                if ( ImGui::MenuItem( ICON_FA_MAP_PIN "    Pin to current view", nullptr, &pinned) ){
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
                if ( ImGui::MenuItem( ICON_FA_CAMERA_RETRO "  Capture frame (PNG)", CTRL_MOD "Shitf+R") )
                    FrameGrabbing::manager().add(new PNGRecorder);

                // Stop recording menu if main recorder already exists

                if (!_video_recorders.empty()) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_RECORD, 0.8f));
                    ImGui::MenuItem( ICON_FA_SQUARE "  Record starting", CTRL_MOD "R", false, false);
                    ImGui::PopStyleColor(1);
                    static char dummy_str[512];
                    sprintf(dummy_str, "%s", VideoRecorder::profile_name[Settings::application.record.profile]);
                    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.14f, 0.5f));
                    ImGui::InputText("Codec", dummy_str, IM_ARRAYSIZE(dummy_str), ImGuiInputTextFlags_ReadOnly);
                    ImGui::PopStyleColor(1);
                }
                else if (video_recorder_) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_RECORD, 0.8f));
                    if ( ImGui::MenuItem( ICON_FA_SQUARE "  Stop Record", CTRL_MOD "R") ) {
                        video_recorder_->stop();
                        video_recorder_ = nullptr;
                    }
                    ImGui::PopStyleColor(1);
                    static char dummy_str[512];
                    sprintf(dummy_str, "%s", VideoRecorder::profile_name[Settings::application.record.profile]);
                    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.14f, 0.5f));
                    ImGui::InputText("Codec", dummy_str, IM_ARRAYSIZE(dummy_str), ImGuiInputTextFlags_ReadOnly);
                    ImGui::PopStyleColor(1);
                }
                // start recording
                else {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_RECORD, 0.9f));
                    if ( ImGui::MenuItem( ICON_FA_CIRCLE "  Record", CTRL_MOD "R") ) {
                        _video_recorders.emplace_back( std::async(std::launch::async, delayTrigger, new VideoRecorder, std::chrono::seconds(Settings::application.record.delay)) );
                    }
                    ImGui::PopStyleColor(1);
                    // select profile
                    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                    ImGui::Combo("Codec", &Settings::application.record.profile, VideoRecorder::profile_name, IM_ARRAYSIZE(VideoRecorder::profile_name) );
                }
                // Options menu
                ImGui::Separator();
                ImGui::MenuItem("Options", nullptr, false, false);
                {
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
                    if (selected_path > 2) {
                        if (recordFolderFileDialogs.empty()) {
                            recordFolderFileDialogs.emplace_back(  std::async(std::launch::async, DialogToolkit::openFolderDialog, Settings::application.record.path) );
                            fileDialogPending_ = true;
                        }
                    }
                    else if (selected_path > 1)
                        Settings::application.record.path = SystemToolkit::path_filename( Mixer::manager().session()->filename() );
                    else if (selected_path > 0)
                        Settings::application.record.path = SystemToolkit::home_path();

                    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                    ImGui::SliderFloat("Duration", &Settings::application.record.timeout, 1.f, RECORD_MAX_TIMEOUT,
                                       Settings::application.record.timeout < (RECORD_MAX_TIMEOUT - 1.f) ? "%.0f s" : "Until stopped", 3.f);
                    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                    ImGui::SliderInt("Trigger", &Settings::application.record.delay, 0, 5,
                                       Settings::application.record.delay < 1 ? "Immediate" : "After %d s");
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Share stream"))
            {
#if defined(LINUX)
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
                    else {
                        webcam_emulator_->stop();
                        webcam_emulator_ = nullptr;
                    }
                }
#endif
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_STREAM, 0.9f));
                if ( ImGui::MenuItem( ICON_FA_SHARE_ALT "  Accept connections", NULL, &Settings::application.accept_connections) ) {
                    Streaming::manager().enable(Settings::application.accept_connections);
                }
                ImGui::PopStyleColor(1);
                if (Settings::application.accept_connections)
                {
                    static char dummy_str[512];
                    sprintf(dummy_str, "%s", Connection::manager().info().name.c_str());
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
        }
        // recording indicator overlay
        if (video_recorder_)
        {
            float r = ImGui::GetTextLineHeightWithSpacing();
            ImGui::SetCursorScreenPos(ImVec2(draw_pos.x + r, draw_pos.y + r));
            ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_RECORD, 0.8f));
            ImGui::Text(ICON_FA_CIRCLE " %s", video_recorder_->info().c_str() );
            ImGui::PopStyleColor(1);
            ImGui::PopFont();
        }
        else if (!_video_recorders.empty())
        {
            float r = ImGui::GetTextLineHeightWithSpacing();
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
            float r = ImGui::GetTextLineHeightWithSpacing();
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
    static guint64 start_time_1_ = gst_util_get_timestamp ();
    static guint64 start_time_2_ = gst_util_get_timestamp ();

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
                 ICON_FA_HOURGLASS_HALF "  Timers\0"
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
        guint64 time_ = gst_util_get_timestamp ();

        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ImGui::Text("%s", GstToolkit::time_to_string(time_-start_time_1_, GstToolkit::TIME_STRING_FIXED).c_str());
        ImGui::PopFont();
        ImGui::SameLine(0, 10);
        if (ImGuiToolkit::IconButton(12, 14))
            start_time_1_ = time_; // reset timer 1
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ImGui::Text("%s", GstToolkit::time_to_string(time_-start_time_2_, GstToolkit::TIME_STRING_FIXED).c_str());
        ImGui::PopFont();
        ImGui::SameLine(0, 10);
        if (ImGuiToolkit::IconButton(12, 14))
            start_time_2_ = time_; // reset timer 2

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
    if (!ImGui::Begin("About " APP_NAME APP_TITLE, p_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
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
/// SOURCE CONTROLLER
///
SourceController::SourceController() : min_width_(0.f), h_space_(0.f), v_space_(0.f), buttons_height_(0.f),
    timeline_height_(0.f), scrollbar_(0.f), mediaplayer_height_(0.f), buttons_width_(0.f),
    active_label_(LABEL_AUTO_MEDIA_PLAYER), active_selection_(-1),
    selection_context_menu_(false), selection_mediaplayer_(nullptr), selection_target_slower_(0), selection_target_faster_(0),
    mediaplayer_active_(nullptr), mediaplayer_mode_(false), mediaplayer_slider_pressed_(false), mediaplayer_timeline_zoom_(1.f)
{
    info_.setExtendedStringMode();
}


void SourceController::resetActiveSelection()
{
    info_.reset();
    active_selection_ = -1;
    active_label_ = LABEL_AUTO_MEDIA_PLAYER;
}

void SourceController::Render()
{
    // reset on session change
    static Session *__session = nullptr;
    if (Mixer::manager().session() != __session) {
        __session = Mixer::manager().session();
        resetActiveSelection();
    }

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

    // menu (no title bar)
    if (ImGui::BeginMenuBar())
    {
        if (ImGuiToolkit::IconButton(4,16)){
            Settings::application.widget.media_player = false;
            selection_.clear();
        }
        if (ImGui::BeginMenu(IMGUI_TITLE_MEDIAPLAYER))
        {
            if (ImGui::MenuItem( ICON_FA_WIND "  Clear")) {
                selection_.clear();
                resetActiveSelection();
                Mixer::manager().unsetCurrentSource();
                Mixer::selection().clear();
            }

            ImGui::Separator();
            bool pinned = Settings::application.widget.media_player_view == Settings::application.current_view;
            if ( ImGui::MenuItem( ICON_FA_MAP_PIN "    Pin to current view", nullptr, &pinned) ){
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
            int N = Mixer::manager().session()->numPlayGroups();
            bool enabled = !selection_.empty() && active_selection_ < 0;

            // Menu : Dynamic selection
            if (ImGui::MenuItem(LABEL_AUTO_MEDIA_PLAYER))
                resetActiveSelection();
            // Menu : store selection
            if (ImGui::MenuItem(ICON_FA_PLUS_SQUARE LABEL_STORE_SELECTION, NULL, false, enabled))
            {
                active_selection_ = N;
                active_label_ = std::string("Selection #") + std::to_string(active_selection_);
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
                if (ImGui::BeginMenu(ICON_FA_RANDOM "  Auto fading"))
                {
                    const char* names[] = { "250 ms", "500 ms", "1 second", "2 seconds"};
                    for (int i = 0; i < IM_ARRAYSIZE(names); ++i) {
                        if (ImGui::MenuItem(names[i])) {
                            mediaplayer_active_->timeline()->autoFading( 250 * (int ) pow(2, i) );
                            mediaplayer_active_->timeline()->smoothFading( 2 * (i + 1) );
                            std::ostringstream oss;
                            oss << SystemToolkit::base_filename( mediaplayer_active_->filename() );
                            oss << ": Timeline Auto fading " << 250 * (int ) pow(2, i);
                            Action::manager().store(oss.str());
                        }
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu(ICON_FA_CUT "  Auto cut" ))
                {
                    if (ImGuiToolkit::MenuItemIcon(14, 12,  "Cut faded areas" ))
                        if (mediaplayer_active_->timeline()->autoCut()){
                            std::ostringstream oss;
                            oss << SystemToolkit::base_filename( mediaplayer_active_->filename() );
                            oss << ": Cut faded areas";
                            Action::manager().store(oss.str());
                        }
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

void DrawTimeline(const char* label, Timeline *timeline, guint64 time, double width_ratio, float height)
{
    // get window
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return;

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
        return;

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
    ImGui::RenderFrame(bbox.Min, bbox.Max, ImGui::GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);

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

        // iterate: next bbox of section starts at end of current
        section_bbox_min.x = section_bbox_max.x;
    }

    // detect if there is a gap after
    if (e < timeline->duration())
        window->DrawList->AddRectFilled(ImVec2(section_bbox_min.x -2.f, plot_bbox.Min.y), ImVec2(section_bbox_min.x + 2.f, plot_bbox.Max.y), ImGui::GetColorU32(ImGuiCol_TitleBg));


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
                durations.push_back(static_cast<guint64>(static_cast<double>(ms->mediaplayer()->timeline()->sectionsDuration()) / ms->mediaplayer()->playSpeed()));
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
                    DrawTimeline("##timeline_mediaplayer", mp->timeline(), mp->position(), width_ratio / fabs(mp->playSpeed()), framesize.y);

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
    ImGui::SetCursorPosX(rendersize.x - buttons_height_ / 1.4f);
    if (ImGuiToolkit::ButtonIcon(11,14,"Delete" )) {
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
        ImGui::OpenPopup("_speedchange_context_menu");
        selection_context_menu_ = false;
    }
    if (ImGui::BeginPopup("_speedchange_context_menu"))
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
            Mixer::manager().session()->addPlayGroup( ids(playable_only(Mixer::selection().getCopy())) );
        }

        ImGui::PopStyleColor(2);
    }

}

void SourceController::RenderSingleSource(Source *s)
{
    if ( s == nullptr)
        return;

    // in case of a MediaSource
    MediaSource *ms = dynamic_cast<MediaSource *>(s);
    if ( ms != nullptr && s->active() ) {
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

            float tooltip_height = 2.f * ImGui::GetTextLineHeightWithSpacing();
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
        if ( !mediaplayer_active_->hardwareDecoderName().empty()) {
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


    ///
    /// media player timelines
    ///

    // ignore actual play status of mediaplayer when slider is pressed
    if (!mediaplayer_slider_pressed_)
        mediaplayer_mode_ = mediaplayer_active_->isPlaying();

    // seek position
    guint64 seek_t = mediaplayer_active_->position();

    // scrolling sub-window
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1.f, 1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 1.f);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.f, 0.f, 0.f, 0.0f));

    ImVec2 scrollwindow = ImVec2(ImGui::GetContentRegionAvail().x - slider_zoom_width - 3.0,
                                 2.f * timeline_height_ + scrollbar_ );

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
            mediaplayer_slider_pressed_ = ImGuiToolkit::TimelineSlider("##timeline", &seek_t, tl->begin(),
                                                           tl->end(), tl->step(), size.x);

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
    if ( ImGuiToolkit::ButtonIconMultistate(iconsloop, &current_loop) )
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
    for (auto source = selection_.begin(); source != selection_.end(); ++source){
        if ( (*source)->active() ) {
            enabled = true;
            break;
        }
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

    // unique play / pause button for single source
    if (selection_.size() == 1) {
        Source *s = selection_.front();
        if (s->playing()) {
            if (ImGui::Button(ICON_FA_PAUSE) && enabled)
                (s)->play(false);
        } else {
            if (ImGui::Button(ICON_FA_PLAY) && enabled)
                (s)->play(true);
        }
    }
    // separate play & pause buttons for multiple sources (or none)
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

            // clic button to load file
            if ( ImGui::Button( ICON_FA_FILE_EXPORT " Open media", ImVec2(ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN, 0)) ) {
                // launch async call to file dialog and get its future.
                if (fileImportFileDialogs.empty()) {
                    fileImportFileDialogs.emplace_back(  std::async(std::launch::async, DialogToolkit::openMediaFileDialog, Settings::application.recentImport.path) );
                    fileDialogPending_ = true;
                }
            }

            // Indication
            ImGui::SameLine();
            ImGuiToolkit::HelpMarker("Create a source from a file:\n- video (*.mpg, *mov, *.avi, etc.)\n- image (*.jpg, *.png, etc.)\n- vector graphics (*.svg)\n- vimix session (*.mix)\n\n(Equivalent to dropping the file in the workspace)");

            // if a file dialog future was registered
            if ( !fileImportFileDialogs.empty() ) {
                // check that file dialog thread finished
                if (fileImportFileDialogs.back().wait_for(timeout) == std::future_status::ready ) {
                    // get the filename from this file dialog
                    std::string open_filename = fileImportFileDialogs.back().get();
                    // done with this file dialog
                    fileImportFileDialogs.pop_back();
                    fileDialogPending_ = false;
                    // create a source with this file
                    if (open_filename.empty()) {
                        Log::Notify("No file selected.");
                    } else {
                        std::string label = BaseToolkit::transliterate( open_filename );
                        label = BaseToolkit::trunc_string(label, 35);
                        new_source_preview_.setSource( Mixer::manager().createSourceFile(open_filename), label);
                    }
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
            static std::vector< std::future< std::list<std::string> > > _selectedImagesFileDialogs;

            // clic button to load file
            if ( ImGui::Button( ICON_FA_IMAGES " Open images", ImVec2(ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN, 0)) ) {
                _selectedFiles.clear();
                if (_selectedImagesFileDialogs.empty()) {
                    _selectedImagesFileDialogs.emplace_back( std::async(std::launch::async, DialogToolkit::selectImagesFileDialog, Settings::application.recentImport.path) );
                    fileDialogPending_ = true;
                }
            }

            // Indication
            ImGui::SameLine();
            ImGuiToolkit::HelpMarker("Create a source from a sequence of numbered images.");

            // return from thread for folder openning
            if ( !_selectedImagesFileDialogs.empty() ) {
                // check that file dialog thread finished
                if (_selectedImagesFileDialogs.back().wait_for(timeout) == std::future_status::ready ) {
                    // get the filenames from this file dialog
                    _selectedFiles = _selectedImagesFileDialogs.back().get();
                    if (_selectedFiles.empty()) {
                        Log::Notify("No file selected.");
                    } 
                    // done with this file dialog
                    _selectedImagesFileDialogs.pop_back();
                    fileDialogPending_ = false;
                    // ask to reload the preview
                    update_new_source = true;
                }
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
            ImGuiToolkit::HelpMarker("Create a source replicating internal vimix objects.");
        }
        // Generated Source creator
        else if (Settings::application.source.new_type == 3){

            bool update_new_source = false;

            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            if (ImGui::BeginCombo("##Pattern", "Select generator"))
            {
                for (int p = 0; p < Pattern::pattern_types.size(); ++p){
                    if (ImGui::Selectable( Pattern::pattern_types[p].c_str() )) {
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
                                               Pattern::pattern_types[pattern_type]);
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
            ImGuiToolkit::HelpMarker("Create a source getting images from connected devices or machines;\n- webcams or frame grabbers\n- screen capture\n- vimix stream from connected machines");

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
    ImGui::Spacing();
    ImGui::Text("Sessions");
    static bool selection_session_mode_changed = true;
    static int selection_session_mode = 0;

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
        if (ImGui::Selectable( ICON_FA_FOLDER_PLUS " Add Folder") ){
            if (recentFolderFileDialogs.empty()) {
                recentFolderFileDialogs.emplace_back(  std::async(std::launch::async, DialogToolkit::openFolderDialog, Settings::application.recentFolders.path) );
                fileDialogPending_ = true;
            }
        }
        ImGui::EndCombo();
    }

    // return from thread for folder openning
    if ( !recentFolderFileDialogs.empty() ) {
        // check that file dialog thread finished
        if (recentFolderFileDialogs.back().wait_for(timeout) == std::future_status::ready ) {
            // get the filename from this file dialog
            std::string foldername = recentFolderFileDialogs.back().get();
            if (!foldername.empty()) {
                Settings::application.recentFolders.push(foldername);
                Settings::application.recentFolders.path.assign(foldername);
                selection_session_mode = 1;
                selection_session_mode_changed = true;
            }
            // done with this file dialog
            recentFolderFileDialogs.pop_back();
            fileDialogPending_ = false;

        }
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

                    // load info only if changed from the one already displayed
                    if (_displayed_over != _file_over) {
                        _displayed_over = _file_over;
                        SessionInformation info = SessionCreator::info(*_displayed_over);
                        _file_info = info.description;
                        if (info.thumbnail) {
                            // set image content to thumbnail display
                            _file_thumbnail.set( info.thumbnail );
                            delete info.thumbnail;
                        } else
                            _file_thumbnail.reset();
                    }

                    if ( !_file_info.empty()) {

                        ImGui::BeginTooltip();
                        _file_thumbnail.Render(size.x);
                        ImGui::Text("%s", _file_info.c_str());
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
    if ( ImGuiToolkit::IconButton( ICON_FA_FILE )) {
        Mixer::manager().close(Settings::application.smooth_transition );
        hidePannel();
    }
    if (ImGui::IsItemHovered())
        ImGuiToolkit::ToolTip("New session", CTRL_MOD "W");

    ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_bot.y - 2.f * ImGui::GetFrameHeightWithSpacing()));
    ImGuiToolkit::HelpMarker("Select the history of recently\n"
                             "opened files or a folder.\n"
                             "Double-clic a filename to open.\n\n"
                             ICON_FA_ARROW_CIRCLE_RIGHT "  Enable smooth transition to\n"
                             "perform smooth cross fading.");
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
    ImGui::Spacing();
    ImGui::Text("Status");
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::Combo("##SelectHistory", &Settings::application.pannel_history_mode, ICON_FA_STAR " Snapshots\0" ICON_FA_HISTORY " Undo history\0");

    //
    // UNDO History
    if (Settings::application.pannel_history_mode > 0) {

        static uint _over = 0;
        static uint64_t _displayed_over = 0;
        static bool _tooltip = 0;

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
                            _undo_thumbnail.set( im );
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
        ImGuiToolkit::ButtonToggle(ICON_FA_LOCATION_ARROW, &Settings::application.action_history_follow_view);
        if (ImGui::IsItemHovered())
            ImGuiToolkit::ToolTip("Show in view");
    }
    //
    // SNAPSHOTS
    else {
        static uint64_t _over = 0;
        static bool _tooltip = 0;

        // list snapshots
        std::list<uint64_t> snapshots = Action::manager().snapshots();
        pos_top = ImGui::GetCursorPos();
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if ( ImGui::ListBoxHeader("##Snapshots", snapshots.size(), CLAMP(snapshots.size(), 4, 8)) ) {

            static uint64_t _selected = 0;
            static Thumbnail _snap_thumbnail;
            static std::string _snap_label = "";

            int count_over = 0;
            ImVec2 size = ImVec2( ImGui::GetContentRegionAvailWidth(), ImGui::GetTextLineHeight() );
            for (auto snapit = snapshots.begin(); snapit != snapshots.end(); ++snapit)
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
                        FrameBufferImage *im = Action::manager().thumbnail(_over);
                        if (im) {
                            // set image content to thumbnail display
                            _snap_thumbnail.set( im );
                            delete im;
                        }
                        else
                            _snap_thumbnail.reset();
                        current_over = _over;
                    }
                    // draw thumbnail in tooltip
                    ImGui::BeginTooltip();
                    _snap_thumbnail.Render(size.x);
                    ImGui::EndTooltip();
                    ++count_over; // prevents display twice on item overlap
                }
            }

            // context menu on currently open snapshot
            uint64_t current = Action::manager().currentSnapshot();
            if (ImGui::BeginPopup( "MenuSnapshot" ) && current > 0 )
            {
                _selected = current;
                ImGui::TextDisabled("Edit snapshot");
                // snapshot editable label
                ImGui::SetNextItemWidth(size.x);
                if ( ImGuiToolkit::InputText("##Rename", &_snap_label ) )
                    Action::manager().setLabel( current, _snap_label);
                // snapshot thumbnail
                _snap_thumbnail.Render(size.x);
                // snapshot actions
                if (ImGui::Selectable( " " ICON_FA_ANGLE_DOUBLE_RIGHT "      Apply", false, 0, size ))
                    Action::manager().restore();
                if (ImGui::Selectable( ICON_FA_STAR "_    Remove", false, 0, size ))
                    Action::manager().remove();
                if (ImGui::Selectable( ICON_FA_STAR "=    Replace", false, 0, size ))
                    Action::manager().replace();
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
        if ( ImGuiToolkit::IconButton( ICON_FA_STAR "+"))
            Action::manager().snapshot( "Snap" );
        if (ImGui::IsItemHovered())
            ImGuiToolkit::ToolTip("Take Snapshot ", CTRL_MOD "Y");

        ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_bot.y - ImGui::GetFrameHeightWithSpacing()));
        ImGuiToolkit::HelpMarker("Snapshots keeps a list of favorite\n"
                                 "status of the current session.\n"
                                 "Clic an item to preview or edit.\n"
                                 "Double-clic to restore immediately.\n");

//        ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_bot.y - 2.f * ImGui::GetFrameHeightWithSpacing()));
//        ImGuiToolkit::HelpMarker("Snapshots capture the state of the session.\n"
//                                 "Double-clic on a snapshot to restore it.\n\n"
//                                 ICON_FA_ROUTE "  Enable interpolation to animate\n"
//                                 "from current state to snapshot's state.");
//        // toggle button for smooth interpolation
//        ImGui::SetCursorPos( ImVec2( pannel_width_ IMGUI_RIGHT_ALIGN, pos_bot.y - ImGui::GetFrameHeightWithSpacing()) );
//        ImGuiToolkit::ButtonToggle(ICON_FA_ROUTE, &Settings::application.smooth_snapshot);
//        if (ImGui::IsItemHovered())
//            ImGuiToolkit::ToolTip("Snapshot interpolation");

//        if (Action::manager().currentSnapshot() > 0) {
//            ImGui::SetCursorPos( pos_bot );
//            int interpolation = static_cast<int> (Action::manager().interpolation() * 100.f);
//            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
//            if ( ImGui::SliderInt("Animate", &interpolation, 0, 100, "%d %%") )
//                Action::manager().interpolate( static_cast<float> ( interpolation ) * 0.01f );

//        }

        ImGui::SetCursorPos( pos_bot );
    }

    //
    // Buttons to show WINDOWS
    //
    ImGui::Spacing();
    ImGui::Text("Windows");
    ImGui::Spacing();

    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
    std::string tooltip_ = "";

    ImGui::SameLine(0, 20);
    if ( ImGuiToolkit::IconButton( Rendering::manager().mainWindow().isFullscreen() ? ICON_FA_COMPRESS_ALT : ICON_FA_EXPAND_ALT ) )
        Rendering::manager().mainWindow().toggleFullscreen();
    if (ImGui::IsItemHovered())
        tooltip_ = "Fullscreen " CTRL_MOD "Shift+F";

    ImGui::SameLine(0, 40);
    if ( ImGuiToolkit::IconButton( ICON_FA_STICKY_NOTE ) )
        Mixer::manager().session()->addNote();
    if (ImGui::IsItemHovered())
        tooltip_ = "New note " CTRL_MOD "Shift+N";

    ImGui::SameLine(0, 40);
    if ( ImGuiToolkit::IconButton( ICON_FA_PLAY_CIRCLE ) ) {
        Settings::application.widget.media_player = true;
        if (Settings::application.widget.media_player_view != Settings::application.current_view)
            Settings::application.widget.media_player_view = -1;
    }
    if (ImGui::IsItemHovered())
        tooltip_ = "Player       " CTRL_MOD "P";

    ImGui::SameLine(0, 40);
    if ( ImGuiToolkit::IconButton( ICON_FA_DESKTOP ) ) {
        Settings::application.widget.preview = true;
        if (Settings::application.widget.preview_view != Settings::application.current_view)
            Settings::application.widget.preview_view = -1;
    }
    if (ImGui::IsItemHovered())
        tooltip_ = "Output       " CTRL_MOD "D";

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

        // Appearance
//        ImGuiToolkit::Icon(3, 2);
//        ImGui::SameLine(0, 10);
        ImGui::Text("Appearance");
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if ( ImGui::DragFloat("Scale", &Settings::application.scale, 0.01, 0.5f, 2.0f, "%.1f"))
            ImGui::GetIO().FontGlobalScale = Settings::application.scale;
        bool b = ImGui::RadioButton("Blue", &Settings::application.accent_color, 0); ImGui::SameLine();
        bool o = ImGui::RadioButton("Orange", &Settings::application.accent_color, 1); ImGui::SameLine();
        bool g = ImGui::RadioButton("Grey", &Settings::application.accent_color, 2);
        if (b || o || g)
            ImGuiToolkit::SetAccentColor(static_cast<ImGuiToolkit::accent_color>(Settings::application.accent_color));

        // Options
        ImGui::Spacing();
//        ImGuiToolkit::Icon(2, 2);
//        ImGui::SameLine(0, 10);
        ImGui::Text("Options");
        ImGuiToolkit::ButtonSwitch( ICON_FA_MOUSE_POINTER "  Smooth cursor", &Settings::application.smooth_cursor);
        ImGuiToolkit::ButtonSwitch( ICON_FA_TACHOMETER_ALT " Metrics", &Settings::application.widget.stats);

#ifndef NDEBUG
        ImGui::Text("Expert");
//        ImGuiToolkit::ButtonSwitch( IMGUI_TITLE_HISTORY, &Settings::application.widget.history);
        ImGuiToolkit::ButtonSwitch( IMGUI_TITLE_SHADEREDITOR, &Settings::application.widget.shader_editor, CTRL_MOD  "E");
        ImGuiToolkit::ButtonSwitch( IMGUI_TITLE_TOOLBOX, &Settings::application.widget.toolbox, CTRL_MOD  "T");
        ImGuiToolkit::ButtonSwitch( IMGUI_TITLE_LOGS, &Settings::application.widget.logs, CTRL_MOD "L");
#endif

        // system preferences
        ImGui::Spacing();
//#ifdef LINUX
//        ImGuiToolkit::Icon(12, 6);
//#else
//        ImGuiToolkit::Icon(6, 0);
//#endif
//        ImGui::SameLine(0, 10);
        ImGui::Text("System");
        static bool need_restart = false;
        static bool vsync = (Settings::application.render.vsync > 0);
        static bool blit = Settings::application.render.blit;
        static bool multi = (Settings::application.render.multisampling > 0);
        static bool gpu = Settings::application.render.gpu_decoding;
        bool change = false;
        change |= ImGuiToolkit::ButtonSwitch( "Vertical synchronization", &vsync);
        change |= ImGuiToolkit::ButtonSwitch( "Blit framebuffer", &blit);
        change |= ImGuiToolkit::ButtonSwitch( "Antialiasing framebuffer", &multi);
        change |= ImGuiToolkit::ButtonSwitch( ICON_FA_MICROCHIP " Hardware video decoding", &gpu);

        if (change) {
            need_restart = ( vsync != (Settings::application.render.vsync > 0) ||
                 blit != Settings::application.render.blit ||
                 multi != (Settings::application.render.multisampling > 0) ||
                 gpu != Settings::application.render.gpu_decoding );
        }
        if (need_restart) {
            ImGui::Spacing();
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
        ImGui::Spacing();
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
        if ( ImGui::Button( ICON_FA_PLAY_CIRCLE "  Play ", ImVec2(IMGUI_RIGHT_ALIGN, 0)) ){
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
        if ( ImGui::Button( ICON_FA_PLAY_CIRCLE "  Play &  " ICON_FA_FILE_UPLOAD " Open ", ImVec2(ImGui::GetContentRegionAvail().x, 0)) ){
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

void Thumbnail::reset()
{
    aspect_ratio_ = -1.f;
}

void Thumbnail::set(const FrameBufferImage *image)
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
    if (aspect_ratio_>0.f)
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

