#include <iostream>
#include <cstring>
#include <sstream>
#include <thread>
#include <algorithm>
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

// multiplatform
#include <tinyfiledialogs.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

// generic image loader
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "UserInterfaceManager.h"

#include "defines.h"
#include "Log.h"
#include "SystemToolkit.h"
#include "RenderingManager.h"
#include "Connection.h"
#include "ActionManager.h"
#include "Resource.h"
#include "FileDialog.h"
#include "Settings.h"
#include "SessionCreator.h"
#include "ImGuiToolkit.h"
#include "ImGuiVisitor.h"
#include "GlmToolkit.h"
#include "GstToolkit.h"
#include "Mixer.h"
#include "Recorder.h"
#include "Streamer.h"
#include "Loopback.h"
#include "Selection.h"
#include "FrameBuffer.h"
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "PatternSource.h"
#include "DeviceSource.h"
#include "NetworkSource.h"
#include "StreamSource.h"
#include "PickingVisitor.h"
#include "ImageShader.h"
#include "ImageProcessingShader.h"

#include "TextEditor.h"
static TextEditor editor;

// utility functions
void ShowAboutGStreamer(bool* p_open);
void ShowAboutOpengl(bool* p_open);
void ShowConfig(bool* p_open);
void ShowSandbox(bool* p_open);

// static objects for multithreaded file dialog
const std::chrono::milliseconds timeout = std::chrono::milliseconds(4);
static std::atomic<bool> fileDialogPending_ = false;


static std::vector< std::future<std::string> > saveSessionFileDialogs;
static std::string saveSessionFileDialog(const std::string &path)
{
    std::string filename = "";
    char const * save_file_name;
    char const * save_pattern[1] = { "*.mix" };

    save_file_name = tinyfd_saveFileDialog( "Save a session file", path.c_str(), 1, save_pattern, "vimix session");

    if (save_file_name) {
        filename = std::string(save_file_name);
        std::string extension = filename.substr(filename.find_last_of(".") + 1);
        if (extension != "mix")
            filename += ".mix";
    }

    return filename;
}

static std::vector< std::future<std::string> > openSessionFileDialogs;
static std::vector< std::future<std::string> > importSessionFileDialogs;
static std::string openSessionFileDialog(const std::string &path)
{
    std::string filename = "";
    std::string startpath = SystemToolkit::file_exists(path) ? path : SystemToolkit::home_path();

    char const * open_pattern[18] = { "*.mix" };
    char const * open_file_name;
    open_file_name = tinyfd_openFileDialog( "Import a file", startpath.c_str(), 1, open_pattern, "vimix session", 0);

    if (open_file_name)
        filename = std::string(open_file_name);

    return filename;
}

static std::vector< std::future<std::string> > fileImportFileDialogs;
static std::string ImportFileDialog(const std::string &path)
{
    std::string filename = "";
    std::string startpath = SystemToolkit::file_exists(path) ? path : SystemToolkit::home_path();

    char const * open_pattern[18] = { "*.mix", "*.mp4", "*.mpg",
                                      "*.avi", "*.mov", "*.mkv",
                                      "*.webm", "*.mod", "*.wmv",
                                      "*.mxf", "*.ogg", "*.flv",
                                      "*.asf", "*.jpg", "*.png",
                                      "*.gif", "*.tif", "*.svg" };
    char const * open_file_name;
    open_file_name = tinyfd_openFileDialog( "Import a file", startpath.c_str(), 18, open_pattern, "All supported formats", 0);

    if (open_file_name)
        filename = std::string(open_file_name);

    return filename;
}

static std::vector< std::future<std::string> > recentFolderFileDialogs;
static std::vector< std::future<std::string> > recordFolderFileDialogs;
static std::string FolderDialog(const std::string &path)
{
    std::string foldername = "";
    std::string startpath = SystemToolkit::file_exists(path) ? path : SystemToolkit::home_path();

    char const * open_folder_name;
    open_folder_name = tinyfd_selectFolderDialog("Select a folder", startpath.c_str());

    if (open_folder_name)
        foldername = std::string(open_folder_name);

    return foldername;
}


UserInterface::UserInterface()
{
    ctrl_modifier_active = false;
    alt_modifier_active = false;
    shift_modifier_active = false;
    show_vimix_config = false;
    show_imgui_about = false;
    show_gst_about = false;
    show_opengl_about = false;
    currentTextEdit = "";
    screenshot_step = 0;

    // keep hold on frame grabbers
    video_recorder_ = 0;
    webcam_emulator_ = 0;
}

bool UserInterface::Init()
{
    if (Rendering::manager().mainWindow().window()== nullptr)
        return false;

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
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
    // font for Navigator = 1.5 x base size (with low oversampling)
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
    ImGuiIO& io = ImGui::GetIO();
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
            if (ctrl_modifier_active && !Mixer::manager().session()->filename().empty())
                Mixer::manager().load( Mixer::manager().session()->filename() );
            // CTRL + O : Open session
            else
                selectOpenFilename();
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_S )) {
            // Save Session
            if (Mixer::manager().session()->filename().empty())
                selectSaveFilename();
            else
                Mixer::manager().save();
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_W )) {
            // New Session
            Mixer::manager().close();
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
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_P )) {
            // Logs
            Settings::application.widget.media_player = !Settings::application.widget.media_player;
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
            // toggle recording
            FrameGrabber *rec = FrameGrabbing::manager().get(video_recorder_);
            if (rec) {
                rec->stop();
                video_recorder_ = 0;
            }
            else {
                FrameGrabber *fg = new VideoRecorder;
                video_recorder_ = fg->id();
                FrameGrabbing::manager().add(fg);
            }
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_Z )) {
            if (shift_modifier_active)
                Action::manager().redo();
            else
                Action::manager().undo();
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_C )) {
            std::string clipboard = Mixer::selection().xml();
            if (!clipboard.empty())
                ImGui::SetClipboardText(clipboard.c_str());
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_X )) {
            std::string clipboard = Mixer::selection().xml();
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
            Mixer::manager().setView(View::APPEARANCE);
        else if (ImGui::IsKeyPressed( GLFW_KEY_F12 ))
            StartScreenshot();
        // normal keys // make sure no entry / window box is active
        else if ( !ImGui::IsAnyWindowFocused() ){
            // Backspace to delete source
            if (ImGui::IsKeyPressed( GLFW_KEY_BACKSPACE ) || ImGui::IsKeyPressed( GLFW_KEY_DELETE ))
                Mixer::manager().deleteSelection();
            // button esc to toggle fullscreen
            else if (ImGui::IsKeyPressed( GLFW_KEY_ESCAPE )) {
                if (Rendering::manager().mainWindow().isFullscreen())
                    Rendering::manager().mainWindow().exitFullscreen();
                else if (navigator.pannelVisible())
                    navigator.hidePannel();
                else if (!Mixer::selection().empty()) {
                    Mixer::manager().unsetCurrentSource();
                    Mixer::selection().clear();
                }
                else
                    Mixer::manager().setView(View::MIXING);
            }
            // button home to toggle menu
            else if (ImGui::IsKeyPressed( GLFW_KEY_HOME ))
                navigator.togglePannelMenu();
            // button home to toggle menu
            else if (ImGui::IsKeyPressed( GLFW_KEY_INSERT ))
                navigator.togglePannelNew();
            // button tab to select next
            else if (ImGui::IsKeyPressed( GLFW_KEY_TAB ))
                Mixer::manager().setCurrentNext();
        }
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


void setMouseCursor(ImVec2 mousepos, View::Cursor c = View::Cursor())
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
            setMouseCursor(io.MousePos, c);
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
                    if (s != nullptr) {
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
                        Mixer::manager().setCurrentSource( s );
                        if (navigator.pannelVisible())
                            navigator.showPannelSource( Mixer::manager().indexCurrentSource() );

                        // indicate to view that an action can be initiated (e.g. grab)
                        Mixer::manager().view()->initiate();
                    }
                    // no source is selected
                    else
                        clear_selection = true;
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
//        else if ( ImGui::IsMouseReleased(ImGuiMouseButton_Left) )
//        {
//            view_drag = nullptr;
//            mousedown = false;
//            picked = { nullptr, glm::vec2(0.f) };
//            Mixer::manager().view()->terminate();

//            // special case of one single source in selection : make current after release
//            if (Mixer::selection().size() == 1)
//                Mixer::manager().setCurrentSource( Mixer::selection().front() );
//        }

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
                        ImGui::GetBackgroundDrawList()->AddLine(io.MousePos, start, ImGui::GetColorU32(ImGuiCol_ResizeGripHovered), 5.f);
                    }
                    else
                        mouse_smooth = mousepos;

                    // action on current source
                    Source *current = Mixer::manager().currentSource();
                    if (current)
                    {
                        // grab current sources
                        View::Cursor c = Mixer::manager().view()->grab(current, mouseclic[ImGuiMouseButton_Left], mouse_smooth, picked);
                        // grab others from selection
                        for (auto it = Mixer::selection().begin(); it != Mixer::selection().end(); it++) {
                            if ( *it != current )
                                Mixer::manager().view()->grab(*it, mouseclic[ImGuiMouseButton_Left], mouse_smooth, picked);
                        }
                        setMouseCursor(io.MousePos, c);
                    }
                    // action on other (non-source) elements in the view
                    else //if ( picked.first != nullptr )
                    {
                        View::Cursor c = Mixer::manager().view()->grab(nullptr, mouseclic[ImGuiMouseButton_Left], mouse_smooth, picked);
                        setMouseCursor(io.MousePos, c);
                    }
                }
                // Selection area
                else {
                    ImGui::GetBackgroundDrawList()->AddRect(io.MouseClickedPos[ImGuiMouseButton_Left], io.MousePos,
                                                            ImGui::GetColorU32(ImGuiCol_ResizeGripHovered));
                    ImGui::GetBackgroundDrawList()->AddRectFilled(io.MouseClickedPos[ImGuiMouseButton_Left], io.MousePos,
                                                            ImGui::GetColorU32(ImGuiCol_ResizeGripHovered, 0.3f));

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
        setMouseCursor(io.MousePos);

        // special case of one single source in selection : make current after release
        if (Mixer::selection().size() == 1)
            Mixer::manager().setCurrentSource( Mixer::selection().front() );
    }
}


void UserInterface::selectSaveFilename()
{
    // launch file dialog to select a session filename to save
    if ( saveSessionFileDialogs.empty()) {
        saveSessionFileDialogs.emplace_back( std::async(std::launch::async, saveSessionFileDialog, Settings::application.recentSessions.path) );
        fileDialogPending_ = true;
    }
    navigator.hidePannel();
}

void UserInterface::selectOpenFilename()
{
    // launch file dialog to select a session filename to open
    if ( openSessionFileDialogs.empty()) {
        openSessionFileDialogs.emplace_back( std::async(std::launch::async, openSessionFileDialog, Settings::application.recentSessions.path) );
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
    // warning modal dialog
    Log::Render();

    // clear view mode in Transition view
    if ( !Settings::application.transition.hide_windows || Settings::application.current_view < View::TRANSITION) {

        // windows
        if (Settings::application.widget.toolbox)
            toolbox.Render();
        if (Settings::application.widget.preview)
            RenderPreview();
        if (Settings::application.widget.history)
            RenderHistory();
        if (Settings::application.widget.media_player)
            mediacontrol.Render();
        if (Settings::application.widget.shader_editor)
            RenderShaderEditor();
        if (Settings::application.widget.logs)
            Log::ShowLogWindow(&Settings::application.widget.logs);

        // about dialogs
        if (show_vimix_config)
            ShowConfig(&show_vimix_config);
        if (show_imgui_about)
            ImGui::ShowAboutWindow(&show_imgui_about);
        if (show_gst_about)
            ShowAboutGStreamer(&show_gst_about);
        if (show_opengl_about)
            ShowAboutOpengl(&show_opengl_about);
    }

    // stats in the corner
    if (Settings::application.widget.stats)
        ImGuiToolkit::ShowStats(&Settings::application.widget.stats,
                                &Settings::application.widget.stats_corner,
                                &Settings::application.widget.stats_timer);

    // management of video_recorder
    FrameGrabber *rec = FrameGrabbing::manager().get(video_recorder_);
    if (rec && rec->duration() > Settings::application.record.timeout ){
        rec->stop();
        video_recorder_ = 0;
    }

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

void UserInterface::showMenuFile()
{
    if (ImGui::MenuItem( ICON_FA_FILE "  New", CTRL_MOD "W")) {
        Mixer::manager().close();
        navigator.hidePannel();
    }
    ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
    ImGui::Combo("##AR", &Settings::application.render.ratio, FrameBuffer::aspect_ratio_name, IM_ARRAYSIZE(FrameBuffer::aspect_ratio_name) );
    ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
    ImGui::Combo("##HEIGHT", &Settings::application.render.res, FrameBuffer::resolution_name, IM_ARRAYSIZE(FrameBuffer::resolution_name) );

    ImGui::Separator();

    ImGui::MenuItem( ICON_FA_HISTORY " Open last on start", nullptr, &Settings::application.recentSessions.load_at_start);

    if (ImGui::MenuItem( ICON_FA_FILE_UPLOAD "  Open", CTRL_MOD "O"))
        selectOpenFilename();

    if (ImGui::MenuItem( ICON_FA_FILE_EXPORT " Import")) {
        // launch file dialog to open a session file
        if ( importSessionFileDialogs.empty()) {
            importSessionFileDialogs.emplace_back( std::async(std::launch::async, openSessionFileDialog, Settings::application.recentSessions.path) );
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
    if (ImGui::MenuItem( ICON_FA_SAVE "  Save as"))
        selectSaveFilename();

    ImGui::MenuItem( ICON_FA_FILE_DOWNLOAD "  Save on exit", nullptr, &Settings::application.recentSessions.save_on_exit);

    ImGui::Separator();
    if (ImGui::MenuItem( ICON_FA_POWER_OFF " Quit", CTRL_MOD "Q"))
        Rendering::manager().close();

}


ToolBox::ToolBox()
{
    show_demo_window = false;
    show_icons_window = false;
    show_sandbox = false;
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


void ToolBox::Render()
{
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
        ImGui::EndMenuBar();
    }

    static char buf1[128] = "videotestsrc pattern=smpte";
    ImGui::InputText("gstreamer pipeline", buf1, 128);
    if (ImGui::Button("Create Generic Stream Source") )
    {
        Mixer::manager().addSource( Mixer::manager().createSourceStream(buf1) );
    }


    //
    // display histogram of update time and plot framerate
    //
    // keep array of 120 values, i.e. approx 2 seconds of recording
    static float framerate_values[2][120] = {{}};
    static float sum[2] = { 0.f, 0.f };
    static int values_index = 0;

    static float max_fps = 65.f;
    static float min_fps = 40.f;
    static float refresh_rate = -1.f;
    if (refresh_rate < 0.f) {

        const GLFWvidmode* mode = glfwGetVideoMode(Rendering::manager().outputWindow().monitor());
        refresh_rate = float(mode->refreshRate);
        if (Settings::application.render.vsync > 0)
            refresh_rate /= Settings::application.render.vsync;
        else
            refresh_rate = 0.f;
        max_fps = refresh_rate + 5.f;
        min_fps = refresh_rate - 20.f;

        for(int i = 0; i<120; ++i) {
            framerate_values[0][i] = refresh_rate;
            sum[0] += refresh_rate;
        }
    }

    // compute average step 1: remove previous value from the sum
    sum[0] -= framerate_values[0][values_index];
    sum[1] -= framerate_values[1][values_index];

    // store values of FPS and of Mixing update dt
    framerate_values[0][values_index] = MINI(ImGui::GetIO().Framerate, 1000.f);
    framerate_values[1][values_index] = MINI(Mixer::manager().dt(), 100.f);

    // compute average step 2: add current value to the sum
    sum[0] += framerate_values[0][values_index];
    sum[1] += framerate_values[1][values_index];

    // move inside array
    values_index = (values_index+1) % 120;

    // non-vsync fixed FPS : have to calculate plot dimensions based on past values
    if (refresh_rate < 1.f) {
        max_fps = sum[0] / 120.f + 5.f;
        min_fps = sum[0] / 120.f - 20.f;
    }

    // plot values, with title overlay to display the average
    ImVec2 plot_size = ImGui::GetContentRegionAvail();
    plot_size.y *= 0.49;
    char overlay[128];
    sprintf(overlay, "Rendering %.1f FPS", sum[0] / 120.f);
    ImGui::PlotLines("LinesRender", framerate_values[0], 120, values_index, overlay, min_fps, max_fps, plot_size);
    sprintf(overlay, "Update time %.1f ms (%.1f FPS)", sum[1] / 120.f, 120000.f / sum[1]);
    ImGui::PlotHistogram("LinesMixer", framerate_values[1], 120, values_index, overlay, 0.0f, 50.0f, plot_size);

    ImGui::End();

    // About and other utility windows
    if (show_icons_window)
        ImGuiToolkit::ShowIconsWindow(&show_icons_window);    
    if (show_sandbox)
        ShowSandbox(&show_sandbox);
    if (show_demo_window)
        ImGui::ShowDemoWindow(&show_demo_window);

}

void UserInterface::RenderHistory()
{
    float history_height = 5.f * ImGui::GetFrameHeightWithSpacing();
    ImVec2 MinWindowSize = ImVec2(250.f, history_height);

    ImGui::SetNextWindowPos(ImVec2(1180, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(250, 400), ImGuiCond_FirstUseEver);
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
        if (ImGui::BeginMenu(IMGUI_TITLE_HISTORY))
        {
            if ( ImGui::MenuItem( ICON_FA_UNDO "  Undo", CTRL_MOD "Z") )
                Action::manager().undo();
            if ( ImGui::MenuItem( ICON_FA_REDO "  Redo", CTRL_MOD "+Shift Z") )
                Action::manager().redo();

            ImGui::MenuItem( ICON_FA_DIRECTIONS " Follow view", nullptr, &Settings::application.action_history_follow_view);

            if ( ImGui::MenuItem( ICON_FA_TIMES "  Close") )
                Settings::application.widget.history = false;

            ImGui::EndMenu();
        }
        if ( ImGui::Selectable(ICON_FA_UNDO, &tmp, ImGuiSelectableFlags_None, ImVec2(20,0))) {

            Action::manager().undo();
        }
        if ( ImGui::Selectable(ICON_FA_REDO, &tmp, ImGuiSelectableFlags_None, ImVec2(20,0))) {

            Action::manager().redo();
        }

        ImGui::EndMenuBar();
    }

    if (ImGui::ListBoxHeader("##History", ImGui::GetContentRegionAvail() ) )
    {
        for (int i = 1; i <= Action::manager().max(); i++) {

            std::string step_label_ = Action::manager().label(i);

            bool enable = i == Action::manager().current();
            if (ImGui::Selectable( step_label_.c_str(), &enable, ImGuiSelectableFlags_AllowDoubleClick )) {

                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {

                    Action::manager().stepTo(i);
                }
            }
        }
        ImGui::ListBoxFooter();
    }

    ImGui::End();
}

void UserInterface::RenderPreview()
{
    bool openInitializeSystemLoopback = false;
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
        float ar = output->aspectRatio();
        ImGui::SetNextWindowPos(ImVec2(1180, 20), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(400, 260), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(300, 200), ImVec2(FLT_MAX, FLT_MAX), CustomConstraints::AspectRatio, &ar);
        if ( !ImGui::Begin("Preview", &Settings::application.widget.preview,  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar |  ImGuiWindowFlags_NoCollapse ) )
        {
            ImGui::End();
            return;

        }

        FrameGrabber *rec = FrameGrabbing::manager().get(video_recorder_);
        FrameGrabber *cam = FrameGrabbing::manager().get(webcam_emulator_);

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
            if (ImGui::BeginMenu(IMGUI_TITLE_PREVIEW))
            {
                if ( ImGui::MenuItem( ICON_FA_WINDOW_RESTORE "  Show output window") )
                    Rendering::manager().outputWindow().show();

                if ( ImGui::MenuItem( ICON_FA_SHARE_SQUARE "  Create Source") )
                    Mixer::manager().addSource( Mixer::manager().createSourceRender() );

                if ( ImGui::MenuItem( ICON_FA_TIMES "  Close") )
                    Settings::application.widget.preview = false;

                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Record"))
            {
                if ( ImGui::MenuItem( ICON_FA_CAMERA_RETRO "  Capture frame (PNG)") )
                    FrameGrabbing::manager().add(new PNGRecorder);

                // Stop recording menu if main recorder already exists
                if (rec) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_RECORD, 0.8f));
                    if ( ImGui::MenuItem( ICON_FA_SQUARE "  Stop Record", CTRL_MOD "R") ) {
                        rec->stop();
                        video_recorder_ = 0;
                    }
                    ImGui::PopStyleColor(1);
                }
                // start recording
                else {
                    // detecting the absence of video recorder but the variable is still not 0: fix this!
                    if (video_recorder_ > 0)
                        video_recorder_ = 0;
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_RECORD, 0.9f));
                    if ( ImGui::MenuItem( ICON_FA_CIRCLE "  Record", CTRL_MOD "R") ) {
                        FrameGrabber *fg = new VideoRecorder;
                        video_recorder_ = fg->id();
                        FrameGrabbing::manager().add(fg);
                    }
                    ImGui::PopStyleColor(1);
                    // select profile
                    ImGui::SetNextItemWidth(300);
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
                            recordFolderFileDialogs.emplace_back(  std::async(std::launch::async, FolderDialog, Settings::application.record.path) );
                            fileDialogPending_ = true;
                        }
                    }
                    else if (selected_path > 1)
                        Settings::application.record.path = SystemToolkit::path_filename( Mixer::manager().session()->filename() );
                    else if (selected_path > 0)
                        Settings::application.record.path = SystemToolkit::home_path();

                    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
                    ImGui::SliderFloat("Timeout", &Settings::application.record.timeout, 1.f, RECORD_MAX_TIMEOUT,
                                       Settings::application.record.timeout < (RECORD_MAX_TIMEOUT - 1.f) ? "%.0f s" : "None", 3.f);
                }

                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Share stream"))
            {
#if defined(LINUX)
                bool on = cam != nullptr;
                if ( ImGui::MenuItem( ICON_FA_CAMERA "  Emulate video camera", NULL, &on) ) {
                    if (on && cam == nullptr) {
                        if (webcam_emulator_ > 0)
                            webcam_emulator_ = 0;
                        if (Loopback::systemLoopbackInitialized()) {
                            FrameGrabber *fg = new Loopback;
                            webcam_emulator_ = fg->id();
                            FrameGrabbing::manager().add(fg);
                        }
                        else
                            openInitializeSystemLoopback = true;
                    }
                    if (!on && cam != nullptr) {
                        cam->stop();
                        webcam_emulator_ = 0;
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
                        for (auto it = ls.begin(); it != ls.end(); it++)
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
            ImGui::Text(" %d x %d px, %d fps", output->width(), output->height(), int(1000.f / Mixer::manager().dt()) );
        }
        // recording indicator overlay
        if (rec)
        {
            float r = ImGui::GetTextLineHeightWithSpacing();
            ImGui::SetCursorScreenPos(ImVec2(draw_pos.x + r, draw_pos.y + r));
            ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_RECORD, 0.8f));
            ImGui::Text(ICON_FA_CIRCLE " %s", rec->info().c_str() );
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
        ImGui::Text("Install v4l2loopack:");
        ImGui::SetNextItemWidth(600-40);
        ImGui::InputText("##cmd1", dummy_str, IM_ARRAYSIZE(dummy_str), ImGuiInputTextFlags_ReadOnly);
        ImGui::SameLine();
        ImGui::PushID(358794);
        if ( ImGuiToolkit::ButtonIcon(3,6, "Copy to clipboard") )
            ImGui::SetClipboardText(dummy_str);
        ImGui::PopID();

        sprintf(dummy_str, "sudo modprobe v4l2loopback exclusive_caps=1 video_nr=10 card_label=\"vimix loopback\"");
        ImGui::Text("Initialize v4l2loopack:");
        ImGui::SetNextItemWidth(600-40);
        ImGui::InputText("##cmd2", dummy_str, IM_ARRAYSIZE(dummy_str), ImGuiInputTextFlags_ReadOnly);
        ImGui::SameLine();
        ImGui::PushID(899872);
        if ( ImGuiToolkit::ButtonIcon(3,6, "Copy to clipboard") )
            ImGui::SetClipboardText(dummy_str);
        ImGui::PopID();

        ImGui::Separator();
        ImGui::SetItemDefaultFocus();
        if (ImGui::Button("Thank you, I'll do this and try again later.", ImVec2(w, 0)) ) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
#endif
}

void UserInterface::showMediaPlayer(MediaPlayer *mp)
{
    Settings::application.widget.media_player = true;
    mediacontrol.setMediaPlayer(mp);
}

#define LABEL_AUTO_MEDIA_PLAYER "Active source"

MediaController::MediaController() : mp_(nullptr), current_(LABEL_AUTO_MEDIA_PLAYER),
    follow_active_source_(true), media_playing_mode_(false), slider_pressed_(false)
{
}

void MediaController::setMediaPlayer(MediaPlayer *mp)
{
    if (mp && mp->isOpen()) {
        mp_ = mp;
        current_ = SystemToolkit::base_filename(mp_->filename());
        follow_active_source_ = false;
        media_playing_mode_ = mp_->isPlaying();
    }
    else {
        mp_ = nullptr;
        current_ = LABEL_AUTO_MEDIA_PLAYER;
        follow_active_source_ = true;
        media_playing_mode_ = false;
    }
}

void MediaController::followCurrentSource()
{
    Source *s = Mixer::manager().currentSource();
    if ( s != nullptr) {
        MediaSource *ms = dynamic_cast<MediaSource *>(s);
        if (ms) {
            // update the internal mediaplayer if changed
            if (mp_ != ms->mediaplayer()) {
                mp_ = ms->mediaplayer();
                media_playing_mode_ = mp_->isPlaying();
            }
        } else {
            mp_ = nullptr;
            media_playing_mode_ = false;
        }
    }
}


void MediaController::Render()
{
    float toolbar_height = 2.5 * ImGui::GetFrameHeightWithSpacing();
    ImVec2 MinWindowSize = ImVec2(350.f, 1.2f * ImGui::GetFrameHeightWithSpacing() + toolbar_height * (Settings::application.widget.media_player_view? 2.f : 1.f));

    ImGui::SetNextWindowPos(ImVec2(1180, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(MinWindowSize, ImVec2(FLT_MAX, FLT_MAX));
    if ( !ImGui::Begin(IMGUI_TITLE_MEDIAPLAYER, &Settings::application.widget.media_player, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar |  ImGuiWindowFlags_NoCollapse ))
    {
        ImGui::End();
        return;
    }

    // verify that mp is still registered
    if ( std::find(MediaPlayer::begin(),MediaPlayer::end(), mp_ ) == MediaPlayer::end() ) {
        setMediaPlayer();
    }

    // menu (no title bar)
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu(IMGUI_TITLE_MEDIAPLAYER))
        {
            ImGui::MenuItem( ICON_FA_EYE " Preview", nullptr, &Settings::application.widget.media_player_view);

            if ( ImGui::MenuItem( ICON_FA_TIMES "  Close") )
                Settings::application.widget.media_player = false;

            ImGui::EndMenu();
        }
        if (mp_ && current_ != LABEL_AUTO_MEDIA_PLAYER && MediaPlayer::registered().size() > 1) {
            bool tmp = false;
            if ( ImGui::Selectable(ICON_FA_CHEVRON_LEFT, &tmp, ImGuiSelectableFlags_None, ImVec2(10,0))) {

                auto mpit = std::find(MediaPlayer::begin(),MediaPlayer::end(), mp_ );
                if (mpit == MediaPlayer::begin()) {
                    mpit = MediaPlayer::end();
                }
                mpit--;
                setMediaPlayer(*mpit);

            }
            if ( ImGui::Selectable(ICON_FA_CHEVRON_RIGHT, &tmp, ImGuiSelectableFlags_None, ImVec2(10,0))) {

                auto mpit = std::find(MediaPlayer::begin(),MediaPlayer::end(), mp_ );
                mpit++;
                if (mpit == MediaPlayer::end()) {
                    mpit = MediaPlayer::begin();
                }
                setMediaPlayer(*mpit);
            }
        }
        if (ImGui::BeginMenu(current_.c_str()))
        {
            if (ImGui::MenuItem(LABEL_AUTO_MEDIA_PLAYER))
                setMediaPlayer();

            // display list of available media
            for (auto mpit = MediaPlayer::begin(); mpit != MediaPlayer::end(); mpit++ )
            {
                std::string label = (*mpit)->filename();
                if (ImGui::MenuItem( label.c_str() ))
                    setMediaPlayer(*mpit);
            }

            ImGui::EndMenu();
        }


        ImGui::EndMenuBar();
    }

    // mode auto : select the media player from active source
    if (follow_active_source_)
        followCurrentSource();

    // Something to show ?
    if (mp_ && mp_->isOpen())
    {
        static float timeline_zoom = 1.f;
        const float width = ImGui::GetContentRegionAvail().x;
        const float timeline_height = 2.f * (ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y);
        const float segments_height = timeline_height;
        const float slider_zoom_width = segments_height / 2.f;

        if (Settings::application.widget.media_player_view)
        {
            // set an image height to fill the vertical space, minus the height of control bar
            float image_height = ImGui::GetContentRegionAvail().y;
            if ( !mp_->isImage() ) {
                // leave space for buttons and timelines
                image_height -= ImGui::GetFrameHeight() + timeline_height + segments_height ;
                // leave space for scrollbar & spacing
                image_height -= ImGui::GetStyle().ScrollbarSize + 3.f * ImGui::GetStyle().ItemSpacing.y;
            }

            // display media
            ImVec2 imagesize ( image_height * mp_->aspectRatio(), image_height);
            ImVec2 tooltip_pos = ImGui::GetCursorScreenPos();
            ImGui::SetCursorPosX(ImGui::GetCursorPos().x + (ImGui::GetContentRegionAvail().x - imagesize.x) / 2.0);
            ImGui::Image((void*)(uintptr_t)mp_->texture(), imagesize);
            ImVec2 return_to_pos = ImGui::GetCursorPos();

            // display media information
            if (ImGui::IsItemHovered()) {

                float tooltip_height = 3.f * ImGui::GetTextLineHeightWithSpacing();

                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                draw_list->AddRectFilled(ImVec2(tooltip_pos.x - 10.f, tooltip_pos.y),
                                         ImVec2(tooltip_pos.x + width + 10.f, tooltip_pos.y + tooltip_height), IMGUI_COLOR_OVERLAY);

                ImGui::SetCursorScreenPos(tooltip_pos);
                ImGui::Text(" %s", mp_->filename().c_str());
                ImGui::Text(" %s", mp_->codec().c_str());
                if ( mp_->frameRate() > 0.f )
                    ImGui::Text(" %d x %d px, %.2f / %.2f fps", mp_->width(), mp_->height(), mp_->updateFrameRate() , mp_->frameRate() );
                else
                    ImGui::Text(" %d x %d px", mp_->width(), mp_->height());

            }

            // display time
            if ( !mp_->isImage() ) {
                ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
                ImGui::SetCursorPos( ImVec2(return_to_pos.x + 5, return_to_pos.y - ImGui::GetTextLineHeightWithSpacing()) );
                ImGui::Text("%s", GstToolkit::time_to_string(mp_->position(), GstToolkit::TIME_STRING_FIXED).c_str());
                ImGui::PopFont();
            }

            ImGui::SetCursorPos(return_to_pos);
        }

        // Control bar
        if ( mp_->isEnabled() && !mp_->isImage()) {

            float spacing = ImGui::GetStyle().ItemInnerSpacing.x;

            if (ImGui::Button(mp_->playSpeed() > 0 ? ICON_FA_FAST_BACKWARD :ICON_FA_FAST_FORWARD))
                mp_->rewind();
            ImGui::SameLine(0, spacing);

            // ignore actual play status of mediaplayer when slider is pressed
            if (!slider_pressed_)
                media_playing_mode_ = mp_->isPlaying();

            // display buttons Play/Stop depending on current playing mode
            if (media_playing_mode_) {
                if (ImGui::Button(ICON_FA_PAUSE "  Pause", ImVec2(100, 0)))
                    media_playing_mode_ = false;
                ImGui::SameLine(0, spacing);

                ImGui::PushButtonRepeat(true);
                if (ImGui::Button( mp_->playSpeed() < 0 ? ICON_FA_BACKWARD :ICON_FA_FORWARD))
                    mp_->jump ();
                ImGui::PopButtonRepeat();
            }
            else {
                if (ImGui::Button(mp_->playSpeed() < 0 ? ICON_FA_LESS_THAN "  Play": ICON_FA_GREATER_THAN "  Play", ImVec2(100, 0)))
                    media_playing_mode_ = true;
                ImGui::SameLine(0, spacing);

                ImGui::PushButtonRepeat(true);
                if (ImGui::Button( mp_->playSpeed() < 0 ? ICON_FA_STEP_BACKWARD : ICON_FA_STEP_FORWARD))
                    mp_->step();
                ImGui::PopButtonRepeat();
            }

            // loop modes button
            ImGui::SameLine(0, spacing);
            static int current_loop = 0;
            static std::vector< std::pair<int, int> > iconsloop = { {0,15}, {1,15}, {19,14} };
            current_loop = (int) mp_->loop();
            if ( ImGuiToolkit::ButtonIconMultistate(iconsloop, &current_loop) )
                mp_->setLoop( (MediaPlayer::LoopMode) current_loop );

            // speed slider
            ImGui::SameLine(0, MAX(spacing * 4.f, width - 400.f) );
            float speed = static_cast<float>(mp_->playSpeed());
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 40.0);
            if (ImGui::DragFloat( "##Speed", &speed, 0.01f, -10.f, 10.f, "Speed x %.1f", 2.f))
                mp_->setPlaySpeed( static_cast<double>(speed) );
//            if (ImGui::IsItemDeactivatedAfterEdit())
//                Action::manager().store("Play Speed", mp_->id());
            // TODO: avoid seek every time speed is set

            // Timeline popup menu
            ImGui::SameLine(0, spacing);
            if (ImGuiToolkit::ButtonIcon(5, 8))
                ImGui::OpenPopup( "MenuTimeline" );
            if (ImGui::BeginPopup( "MenuTimeline" )) {
                if (ImGui::Selectable( "Reset Speed" )){
                    speed = 1.f;
                    mp_->setPlaySpeed( static_cast<double>(speed) );
                }
                if (ImGui::Selectable( "Reset Timeline" )){
                    timeline_zoom = 1.f;
                    mp_->timeline()->clearFading();
                    mp_->timeline()->clearGaps();
                    Action::manager().store("Timeline Reset", mp_->id());
                }
                ImGui::Separator();
                ImGui::SetNextItemWidth(150);
                int smoothcurve = 0;
                if (ImGui::Combo("##SmoothCurve", &smoothcurve, "Smooth curve\0Just a little\0A bit more\0Quite a lot\0") ){
                    mp_->timeline()->smoothFading( 10 * (int) pow(4, smoothcurve-1) );
                    Action::manager().store("Timeline Smooth curve", mp_->id());
                }
                ImGui::SetNextItemWidth(150);
                int autofade = 0;
                if (ImGui::Combo("##Autofade", &autofade, "Auto fading\0 250 ms\0 500 ms\0 1 second\0 2 seconds\0") ){
                    mp_->timeline()->autoFading( 250 * (int ) pow(2, autofade-1) );
                    mp_->timeline()->smoothFading( 10 * autofade );
                    Action::manager().store("Timeline Auto fading", mp_->id());
                }
                ImGui::EndPopup();
            }

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3.f, 3.f));

            guint64 current_t = mp_->position();
            guint64 seek_t = current_t;

            // scrolling sub-window
            ImGui::BeginChild("##scrolling",
                              ImVec2(ImGui::GetContentRegionAvail().x - slider_zoom_width - 3.0,
                                     timeline_height + segments_height + ImGui::GetStyle().ScrollbarSize ),
                              false, ImGuiWindowFlags_HorizontalScrollbar);
            {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1.f, 1.f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 1.f);
                ImVec2 size = ImGui::CalcItemSize(ImVec2(-FLT_MIN, 0.0f), ImGui::CalcItemWidth(), segments_height -1);
                size.x *= timeline_zoom;

                bool released = false;
                if ( ImGuiToolkit::EditPlotHistoLines("##TimelineArray",
                                                      mp_->timeline()->gapsArray(),
                                                      mp_->timeline()->fadingArray(),
                                                      MAX_TIMELINE_ARRAY, 0.f, 1.f, &released, size) ) {
                    mp_->timeline()->update();
                }
                else if (released) {
                    Action::manager().store("Timeline change", mp_->id());
                }

                // custom timeline slider
                slider_pressed_ = ImGuiToolkit::TimelineSlider("##timeline", &seek_t, mp_->timeline()->begin(),
                                                               mp_->timeline()->end(), mp_->timeline()->step(), size.x);

                ImGui::PopStyleVar(2);
            }
            ImGui::EndChild();

            // zoom slider
            ImGui::SameLine();
            ImGui::VSliderFloat("##TimelineZoom", ImVec2(slider_zoom_width, timeline_height + segments_height), &timeline_zoom, 1.0, 5.f, "");
            ImGui::PopStyleVar();

            // request seek (ASYNC)
            if ( slider_pressed_ && mp_->go_to(seek_t) )
                slider_pressed_ = false;

            // play/stop command should be following the playing mode (buttons)
            // AND force to stop when the slider is pressed
            bool media_play = media_playing_mode_ & (!slider_pressed_);

            // apply play action to media only if status should change
            if ( mp_->isPlaying() != media_play ) {
                mp_->play( media_play );
            }

        }
    }
    else {
        ImVec2 center = ImGui::GetContentRegionAvail() * ImVec2(0.5f, 0.5f);
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_ITALIC);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6, 0.6, 0.6, 0.5f));
        center.x -= ImGui::GetTextLineHeight() * 2.f;
        center.y += ImGui::GetTextLineHeight() * 0.5f;
        ImGui::SetCursorPos(center);
        ImGui::Text("No media");
        ImGui::PopFont();
        ImGui::PopStyleColor(1);
    }

    ImGui::End();
}

void UserInterface::fillShaderEditor(std::string text)
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
    }

    // remember text
    currentTextEdit = text;
    // fill editor
    editor.SetText(currentTextEdit);
}

void UserInterface::RenderShaderEditor()
{
    static bool show_statusbar = true;

    ImGui::Begin(IMGUI_TITLE_SHADEREDITOR, &Settings::application.widget.shader_editor, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_MenuBar);
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


void UserInterface::showPannel(int id)
{
    if (id == NAV_MENU)
        navigator.togglePannelMenu();
    else if (id == NAV_NEW)
        navigator.togglePannelNew();
    else
        navigator.showPannelSource(id);
}

Navigator::Navigator()
{
    // default geometry
    width_ = 100;
    pannel_width_ = 5.f * width_;
    height_ = 100;
    padding_width_ = 100;

    // clean start
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
}

void Navigator::clearButtonSelection()
{
    // clear all buttons
    for(int i=0; i<NAV_COUNT; ++i)
        selected_button[i] = false;

    // clear new source pannel
    new_source_preview_.setSource();
    pattern_type = -1;
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
}

void Navigator::Render()
{
    std::string about = "";
    static uint count_about = 0;

    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(COLOR_NAVIGATOR, 1.f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(COLOR_NAVIGATOR, 1.f));

    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.50f, 0.50f));

    // calculate size of items based on text size and display dimensions
    width_ = 2.f *  ImGui::GetTextLineHeightWithSpacing();          // dimension of left bar depends on FONT_LARGE
    pannel_width_ = 5.f * width_;                                    // pannel is 5x the bar
    padding_width_ = 2.f * style.WindowPadding.x;                   // panning for alighment
    height_ = io.DisplaySize.y;                                     // cover vertically
    sourcelist_height_ = height_ - 8.f * ImGui::GetTextLineHeight(); // space for 4 icons of view
    float icon_width = width_ - 2.f * style.WindowPadding.x;        // icons keep padding
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
                {
                    //            Mixer::manager().unsetCurrentSource();
                    applyButtonSelection(NAV_MENU);
                }
                if (ImGui::IsItemHovered())
                    about = "Main menu [Home]";

                // the list of INITIALS for sources
                int index = 0;
                SourceList::iterator iter;
                for (iter = Mixer::manager().session()->begin(); iter != Mixer::manager().session()->end(); iter++, index++)
                {
                    // draw an indicator for current source
                    if ( (*iter)->mode() >= Source::SELECTED ){
                        ImDrawList* draw_list = ImGui::GetWindowDrawList();
                        ImVec2 p1 = ImGui::GetCursorScreenPos() + ImVec2(icon_width, 0.5f * icon_width);
                        ImVec2 p2 = ImVec2(p1.x + 2.f, p1.y + 2.f);
                        const ImU32 color = ImGui::GetColorU32( style.Colors[ImGuiCol_Text] );
                        if ((*iter)->mode() == Source::CURRENT)  {
                            p1 = ImGui::GetCursorScreenPos() + ImVec2(icon_width, 0);
                            p2 = ImVec2(p1.x + 2.f, p1.y + icon_width);
                        }
                        draw_list->AddRect(p1, p2, color, 0.0f,  0, 3.f);
                    }
                    // draw select box
                    ImGui::PushID(std::to_string((*iter)->group(View::RENDERING)->id()).c_str());
                    if (ImGui::Selectable( (*iter)->initials(), &selected_button[index], 0, iconsize))
                    {
                        applyButtonSelection(index);
                        if (selected_button[index])
                            Mixer::manager().setCurrentIndex(index);
                    }
                    ImGui::PopID();
                }

                // the "+" icon for action of creating new source
                if (ImGui::Selectable( ICON_FA_PLUS, &selected_button[NAV_NEW], 0, iconsize))
                {
                    Mixer::manager().unsetCurrentSource();
                    applyButtonSelection(NAV_NEW);
                }
                if (ImGui::IsItemHovered())
                    about = "New Source [Ins]";

            }
            else {
                // the ">" icon for transition menu
                if (ImGui::Selectable( ICON_FA_ARROW_CIRCLE_RIGHT, &selected_button[NAV_TRANS], 0, iconsize))
                {
                    //            Mixer::manager().unsetCurrentSource();
                    applyButtonSelection(NAV_TRANS);
                }
            }
        }
        ImGui::End();

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
            about = "Mixing [F1]";
        if (ImGui::Selectable( ICON_FA_OBJECT_UNGROUP , &selected_view[2], 0, iconsize))
        {
            if (ImGui::IsItemHovered())
            Mixer::manager().setView(View::GEOMETRY);
            view_pannel_visible = previous_view == Settings::application.current_view;
        }
        if (ImGui::IsItemHovered())
            about = "Geometry [F2]";
        if (ImGui::Selectable( ICON_FA_LAYER_GROUP, &selected_view[3], 0, iconsize))
        {
            Mixer::manager().setView(View::LAYER);
            view_pannel_visible = previous_view == Settings::application.current_view;
        }
        if (ImGui::IsItemHovered())
            about = "Layers [F3]";
        if (ImGui::Selectable( ICON_FA_VECTOR_SQUARE, &selected_view[4], 0, iconsize))
        {
            Mixer::manager().setView(View::APPEARANCE);
            view_pannel_visible = previous_view == Settings::application.current_view;
        }
        if (ImGui::IsItemHovered())
            about = "Source apppearance [F4]";


        ImGui::End();
    }

    if (!about.empty()) {
        count_about++;
        if (count_about > 100) {
            ImGuiToolkit::PushFont(ImGuiToolkit::FONT_DEFAULT);
            ImGui::BeginTooltip();
            ImGui::Text("%s", about.c_str());
            ImGui::EndTooltip();
            ImGui::PopFont();
        }
    }
    else
        count_about = 0;

    if ( view_pannel_visible && !pannel_visible_ )
        RenderViewPannel( ImVec2(width_, sourcelist_height_), ImVec2(width_*0.8f, height_ - sourcelist_height_) );

    ImGui::PopStyleVar();
    ImGui::PopFont();

    if ( Settings::application.pannel_stick || pannel_visible_){
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
        bool dumm = true;
        ImGui::SetCursorPosX(10.f);
        ImGui::SetCursorPosY(10.f);
        if (ImGuiToolkit::IconToggle(4,7,5,7, &dumm)) {
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
        ImGui::SetCursorPosY(10);
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ImGui::Text("Source");
        ImGui::PopFont();

        ImGui::SetCursorPos(ImVec2(pannel_width_  - 35.f, 15.f));
        const char *tooltip[2] = {"Pin pannel\nCurrent: double-clic on source", "Un-pin Pannel\nCurrent: single-clic on source"};
        ImGuiToolkit::IconToggle(5,2,4,2, &Settings::application.pannel_stick, tooltip );

        static char buf5[128];
        sprintf ( buf5, "%s", s->name().c_str() );
        ImGui::SetCursorPosY(width_);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if (ImGui::InputText("Name", buf5, 64, ImGuiInputTextFlags_CharsNoBlank)){
            Mixer::manager().renameSource(s, buf5);
        }
        // Source pannel
        static ImGuiVisitor v;
        s->accept(v);
        // ensure change is applied // TODO : touch() in visitor ? [avoid here as it forces useless update]
//        s->touch();
        // delete button
        ImGui::Text(" ");
        // Action on source
        if ( ImGui::Button( ICON_FA_SHARE_SQUARE " Clone", ImVec2(ImGui::GetContentRegionAvail().x, 0)) )
            Mixer::manager().addSource ( Mixer::manager().createSourceClone() );
        if ( ImGui::Button( ICON_FA_BACKSPACE " Delete", ImVec2(ImGui::GetContentRegionAvail().x, 0)) )
//            ImGui::OpenPopup("confirm_delete_popup");
//        if (ImGui::BeginPopup("confirm_delete_popup"))
//        {
//            ImGui::Text("Delete '%s'", s->name().c_str());
//            if (ImGui::Button( ICON_FA_BACKSPACE " Yes, delete"))
                Mixer::manager().deleteSource(s);
//            ImGui::EndPopup();
//        }
    }
    ImGui::End();    
}


SourcePreview::SourcePreview() : source_(nullptr), label_("")
{

}

void SourcePreview::setSource(Source *s, std::string label)
{
    if(source_)
        delete source_;

    source_ = s;
    label_ = label;
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
            bool active = source_->active();
            // update source
            source_->update( Mixer::manager().dt());
            // render framebuffer
            source_->render();

            // draw preview
            FrameBuffer *frame = source_->frame();
            ImVec2 preview_size(width, width / frame->aspectRatio());
            ImGui::Image((void*)(uintptr_t) frame->texture(), preview_size);

            if (controlbutton && source_->ready()) {
                ImVec2 pos = ImGui::GetCursorPos();
                ImGui::SameLine();
                if (ImGuiToolkit::IconToggle(9,7,1,8, &active))
                    source_->setActive(active);
                ImGui::SetCursorPos(pos);
            }
            ImGuiToolkit::Icon(source_->icon().x, source_->icon().y);
            ImGui::SameLine(0, 10);
            ImGui::Text("%s ", label_.c_str());
            ImGui::Text("%d x %d %s", frame->width(), frame->height(), frame->use_alpha() ? "RGBA" : "RGB");
        }
    }
}

bool SourcePreview::ready() const
{
    return source_ != nullptr && source_->ready();
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
        ImGui::Text("New Source");
        ImGui::PopFont();

        ImGui::SetCursorPosY(width_);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);

        static const char* origin_names[4] = { ICON_FA_PHOTO_VIDEO "  File",
                                               ICON_FA_SYNC "   Internal",
                                               ICON_FA_COG "   Generated",
                                               ICON_FA_PLUG "    Connected"
                                             };
        // TODO IMPLEMENT EXTERNAL SOURCES static const char* origin_names[3] = { ICON_FA_FILE " File", ICON_FA_SITEMAP " Internal", ICON_FA_PLUG " External" };
        if (ImGui::Combo("##Origin", &Settings::application.source.new_type, origin_names, IM_ARRAYSIZE(origin_names)) )
            new_source_preview_.setSource();

        // File Source creation
        if (Settings::application.source.new_type == 0) {

            ImGui::SetCursorPosY(2.f * width_);

            // clic button to load file
            if ( ImGui::Button( ICON_FA_FILE_EXPORT " Open file", ImVec2(ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN, 0)) ) {
                // launch async call to file dialog and get its future.
                if (fileImportFileDialogs.empty()) {
                    fileImportFileDialogs.emplace_back(  std::async(std::launch::async, ImportFileDialog, Settings::application.recentImport.path) );
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
                    std::string label = open_filename.substr( open_filename.size() - MIN( 35, open_filename.size()) );
                    new_source_preview_.setSource( Mixer::manager().createSourceFile(open_filename), label);
                }
            }

            // combo of recent media filenames
            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            if (ImGui::BeginCombo("##RecentImport", IMGUI_LABEL_RECENT_FILES))
            {
                std::list<std::string> recent = Settings::application.recentImport.filenames;
                for (std::list<std::string>::iterator path = recent.begin(); path != recent.end(); path++ )
                {
                    std::string recentpath(*path);
                    if ( SystemToolkit::file_exists(recentpath)) {
                        std::string label = SystemToolkit::trunc_filename(recentpath, 35);
                        if (ImGui::Selectable( label.c_str() )) {
                            new_source_preview_.setSource( Mixer::manager().createSourceFile(recentpath.c_str()), label);
                        }
                    }
                }
                ImGui::EndCombo();
            }
        }
        // Internal Source creator
        else if (Settings::application.source.new_type == 1){

            ImGui::SetCursorPosY(2.f * width_);

            // fill new_source_preview with a new source
            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            if (ImGui::BeginCombo("##Source", "Select object"))
            {
                std::string label = "Rendering output";
                if (ImGui::Selectable( label.c_str() )) {
                    new_source_preview_.setSource( Mixer::manager().createSourceRender(), label);
                }
                SourceList::iterator iter;
                for (iter = Mixer::manager().session()->begin(); iter != Mixer::manager().session()->end(); iter++)
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
        else if (Settings::application.source.new_type == 2){

            ImGui::SetCursorPosY(2.f * width_);

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
        else if (Settings::application.source.new_type == 3){

            ImGui::SetCursorPosY(2.f * width_);

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
            new_source_preview_.Render(ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN, Settings::application.source.new_type != 1);
            // ask to import the source in the mixer
            ImGui::NewLine();
            if (new_source_preview_.ready() && ImGui::Button( ICON_FA_CHECK "  Create", ImVec2(pannel_width_ - padding_width_, 0)) ) {
                Mixer::manager().addSource(new_source_preview_.getSource());
                selected_button[NAV_NEW] = false;
            }
        }

    }
    ImGui::End();
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
        ImGui::SetCursorPosY(10);
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ImGui::Text("Transition");
        ImGui::PopFont();

        // Session menu
        ImGui::SetCursorPosY(width_);
        ImGui::Text("Behavior");
        ImGuiToolkit::ButtonSwitch( ICON_FA_RANDOM " Cross fading", &Settings::application.transition.cross_fade);
        ImGuiToolkit::ButtonSwitch( ICON_FA_CROSSHAIRS " Open on target", &Settings::application.transition.auto_open);
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

        ImGui::Spacing();
        if ( ImGui::Button( ICON_FA_SIGN_IN_ALT " Play ", ImVec2(IMGUI_RIGHT_ALIGN, 0)) ){
            TransitionView *tv = static_cast<TransitionView *>(Mixer::manager().view(View::TRANSITION));
            if (tv) tv->play(true);
        }
        if ( ImGui::Button( ICON_FA_DOOR_OPEN " Exit", ImVec2(IMGUI_RIGHT_ALIGN, 0)) )
            Mixer::manager().setView(View::MIXING);
        ImGui::SameLine();
        ImGuiToolkit::HelpMarker("Exit transition leaves the output 'as is',\nwith the newly openned session as a source\nif the transition is not finished.");

    }
    ImGui::End();
}

void Navigator::RenderMainPannel()
{
    // Next window is a side pannel
    ImGui::SetNextWindowPos( ImVec2(width_, 0), ImGuiCond_Always );
    ImGui::SetNextWindowSize( ImVec2(pannel_width_, height_), ImGuiCond_Always );
    ImGui::SetNextWindowBgAlpha(0.85f); // Transparent background
    if (ImGui::Begin("##navigatorMain", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |  ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
    {
        // TITLE
        ImGui::SetCursorPosY(10);
        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
        ImGui::Text(APP_NAME);
        ImGui::PopFont();

        // Icon to switch fullscreen
        ImGui::SetCursorPos(ImVec2(pannel_width_  - 35.f, 15.f));
        const char *tooltip[2] = {"Enter Fullscreen (" CTRL_MOD "Shift+F)", "Exit Fullscreen (" CTRL_MOD "Shift+F)"};
        bool fs = Rendering::manager().mainWindow().isFullscreen();
        if ( ImGuiToolkit::IconToggle(3,15,2,15, &fs, tooltip ) ) {
            Rendering::manager().mainWindow().toggleFullscreen();
        }
        // Session menu
        ImGui::SetCursorPosY(width_);
        ImGui::Text("Session");
        ImGui::SameLine();
        ImGui::SetCursorPosX(pannel_width_ IMGUI_RIGHT_ALIGN);
        if (ImGui::BeginMenu("File"))
        {
            UserInterface::manager().showMenuFile();
            ImGui::EndMenu();
        }

        static bool selection_session_mode_changed = true;
        static int selection_session_mode = 0;

        //
        // Session quick selection pannel
        //

        // Show combo box of quick selection modes
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if (ImGui::BeginCombo("##SelectionSession", SystemToolkit::trunc_filename(Settings::application.recentFolders.path, 25).c_str() )) {

            // Option 0 : recent files
            if (ImGui::Selectable( ICON_FA_HISTORY IMGUI_LABEL_RECENT_FILES) ) {
                 Settings::application.recentFolders.path = IMGUI_LABEL_RECENT_FILES;
                 selection_session_mode = 0;
                 selection_session_mode_changed = true;
            }
            // Options 1 : known folders
            for(auto foldername = Settings::application.recentFolders.filenames.begin();
                foldername != Settings::application.recentFolders.filenames.end(); foldername++) {
                std::string f = std::string(ICON_FA_FOLDER) + " " + SystemToolkit::trunc_filename( *foldername, 40);
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
                    recentFolderFileDialogs.emplace_back(  std::async(std::launch::async, FolderDialog, Settings::application.recentFolders.path) );
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
        ImVec2 pos = ImGui::GetCursorPos();
        ImGui::SameLine();
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.7);
        bool reset = false;
        if ( selection_session_mode == 1) {
            const char *tooltip[2] = {"Discard folder", "Discard folder"};
            if (ImGuiToolkit::IconToggle(12,14,11,14, &reset, tooltip)) {
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
            const char *tooltip[2] = {"Clear history", "Clear history"};
            if (ImGuiToolkit::IconToggle(12,14,11,14, &reset, tooltip)) {
                Settings::application.recentSessions.filenames.clear();
                Settings::application.recentSessions.front_is_valid = false;
                // reload the list next time
                selection_session_mode_changed = true;
            }
        }
        ImGui::PopStyleVar();
        ImGui::SetCursorPos(pos);

        // fill the session list depending on the mode
        static std::list<std::string> sessions_list;
        // change session list if changed
        if (selection_session_mode_changed) {

            // selection MODE 0 ; RECENT sessions
            if ( selection_session_mode == 0) {
                // show list of recent sessions
                sessions_list = Settings::application.recentSessions.filenames;
            }
            // selection MODE 1 : LIST FOLDER
            else if ( selection_session_mode == 1) {
                // show list of vimix files in folder
                sessions_list = SystemToolkit::list_directory( Settings::application.recentFolders.path, "mix");
            }
            // indicate the list changed (do not change at every frame)
            selection_session_mode_changed = false;
        }

        // display the sessions list and detect if one was selected (double clic)
        bool session_selected = false;
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ImGui::ListBoxHeader("##Sessions", 5);
        static std::string file_info = "";
        static std::list<std::string>::iterator file_selected = sessions_list.end();
        for(auto it = sessions_list.begin(); it != sessions_list.end(); it++) {
            std::string sessionfilename(*it);
            if (sessionfilename.empty())
                break;
            std::string shortname = SystemToolkit::filename(*it);
            if (ImGui::Selectable( shortname.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick )) {
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) || file_selected == it) {
                    Mixer::manager().open( sessionfilename );
                    session_selected = true;
                    file_info.clear();
                }
                else  {
                    file_info = SessionCreator::info(sessionfilename);
                    if (file_info.empty()) {
                        // failed : remove from recent
                        if ( selection_session_mode == 0) {
                            Settings::application.recentSessions.filenames.remove(sessionfilename);
                            selection_session_mode_changed = true;
                        }
                    }
                    else
                        file_selected = it;
                }
            }
            if (ImGui::IsItemHovered()) {
                if (file_selected != it) {
                    file_info.clear();
                    file_selected = sessions_list.end();
                }
                if (!file_info.empty()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", file_info.c_str());
                    ImGui::EndTooltip();
                }
            }
        }
        ImGui::ListBoxFooter();

        pos = ImGui::GetCursorPos();
        ImGui::SameLine();
        ImGuiToolkit::HelpMarker("Quick access to Session files;\nSelect the history of recently\nopened files or a folder, and\ndouble-clic a filename to open.");
        ImGui::SetCursorPos(pos);

        // done the selection !
        if (session_selected) {
            // close pannel
            file_info.clear();
            hidePannel();
            // reload the list next time
            selection_session_mode_changed = true;
        }

        // options session
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Text("Options");
        ImGuiToolkit::ButtonSwitch( ICON_FA_ARROW_CIRCLE_RIGHT "  Smooth transition", &Settings::application.smooth_transition);
        ImGuiToolkit::ButtonSwitch( ICON_FA_MOUSE_POINTER "  Smooth cursor", &Settings::application.smooth_cursor);


        // Continue Main pannel
        // WINDOWS
        ImGui::Spacing();
        ImGui::Text("Windows");
        ImGuiToolkit::ButtonSwitch( IMGUI_TITLE_PREVIEW, &Settings::application.widget.preview, CTRL_MOD "D");
        ImGuiToolkit::ButtonSwitch( IMGUI_TITLE_MEDIAPLAYER, &Settings::application.widget.media_player, CTRL_MOD "P");
#ifndef NDEBUG
        ImGuiToolkit::ButtonSwitch( IMGUI_TITLE_SHADEREDITOR, &Settings::application.widget.shader_editor, CTRL_MOD  "E");
        ImGuiToolkit::ButtonSwitch( IMGUI_TITLE_TOOLBOX, &Settings::application.widget.toolbox, CTRL_MOD  "T");
        ImGuiToolkit::ButtonSwitch( ICON_FA_LIST " Logs", &Settings::application.widget.logs, CTRL_MOD "L");
#endif
        ImGuiToolkit::ButtonSwitch( ICON_FA_HISTORY " History", &Settings::application.widget.history);
        ImGuiToolkit::ButtonSwitch( ICON_FA_TACHOMETER_ALT " Metrics", &Settings::application.widget.stats);

        // Settings application appearance
        ImGui::Spacing();
        ImGui::Text("Appearance");
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if ( ImGui::DragFloat("Scale", &Settings::application.scale, 0.01, 0.5f, 2.0f, "%.1f"))
            ImGui::GetIO().FontGlobalScale = Settings::application.scale;

//        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
//        if ( ImGui::Combo("Accent", &Settings::application.accent_color, "Blue\0Orange\0Grey\0\0"))
//            ImGuiToolkit::SetAccentColor(static_cast<ImGuiToolkit::accent_color>(Settings::application.accent_color));
        bool b = ImGui::RadioButton("Blue", &Settings::application.accent_color, 0); ImGui::SameLine();
        bool o = ImGui::RadioButton("Orange", &Settings::application.accent_color, 1); ImGui::SameLine();
        bool g = ImGui::RadioButton("Grey", &Settings::application.accent_color, 2);
        if (b || o || g)
            ImGuiToolkit::SetAccentColor(static_cast<ImGuiToolkit::accent_color>(Settings::application.accent_color));

        static unsigned int vimixicon = Resource::getTextureImage("images/vimix_256x256.png");
        static float height_about = 3.f * ImGui::GetTextLineHeightWithSpacing();
        bool show_icon = ImGui::GetCursorPosY() + height_about + 128.f < height_ ;
        bool show_about = ImGui::GetCursorPosY() + height_about < height_ ;

        // Bottom aligned About buttons
        if ( show_about) {
            // Bottom aligned Logo
            if ( show_icon ) {
                ImGui::SetCursorPos(ImVec2(pannel_width_ / 2.f - 64.f, height_ -height_about - 128.f));
                ImGui::Image((void*)(intptr_t)vimixicon, ImVec2(128, 128));
            }
            else {
                ImGui::SetCursorPosY(height_ -height_about);
            }
            ImGui::Spacing();
            if ( ImGui::Button( ICON_FA_CROW " vimix", ImVec2(ImGui::GetContentRegionAvail().x, 0)) )
                UserInterface::manager().show_vimix_config = true;
            if ( ImGui::Button("    ImGui    "))
                UserInterface::manager().show_imgui_about = true;
            ImGui::SameLine();
            if ( ImGui::Button(" GStreamer "))
                UserInterface::manager().show_gst_about = true;
            ImGui::SameLine();
            if ( ImGui::Button("OpenGL", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
                UserInterface::manager().show_opengl_about = true;
        }

    }
    ImGui::End();
}


namespace ImGui
{

int hover(const char *label)
{
    const ImGuiStyle& Style = GetStyle();
    ImGuiWindow* Window = GetCurrentWindow();
    if (Window->SkipItems)
        return 0;

    int hovered = IsItemActive() || IsItemHovered();
    Dummy(ImVec2(0,3));

    // prepare canvas
    const float avail = GetContentRegionAvailWidth();
    const float dim = ImMin(avail, 128.f);
    ImVec2 Canvas(dim, dim);

    ImRect bb(Window->DC.CursorPos, Window->DC.CursorPos + Canvas);
    const ImGuiID id = Window->GetID(label);
    ItemSize(bb);
    if (!ItemAdd(bb, id))
        return 0;

    hovered |= 0 != IsItemClicked();

    RenderFrame(bb.Min, bb.Max, GetColorU32(ImGuiCol_FrameBg, 1), true, Style.FrameRounding);


    return 1;
}

}

#define SEGMENT_ARRAY_MAX 1000


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

//    const guint64 duration = GST_SECOND * 6;
//    const guint64 step = GST_MSECOND * 20;
//    static guint64 t = 0;

//    static float *arr_lines = nullptr;
//    static float *arr_histo = nullptr;
//    static uint array_size = 200;

//    if (arr_lines == nullptr) {

//        arr_lines = (float *) malloc(array_size * sizeof(float));
//        arr_histo = (float *) malloc(array_size * sizeof(float));

//        for (int i = 0; i < array_size; ++i) {
//            arr_lines[i] = 1.f;
//            arr_histo[i] = 0.f;
//        }
//    }

//    // scrolling sub-window
//    ImGui::BeginChild("##scrolling",
//                      ImVec2(ImGui::GetContentRegionAvail().x, 250),
//                      false, ImGuiWindowFlags_HorizontalScrollbar);


//    if (arr_lines != nullptr)
//    {

//        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1, 1));
//        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 1.f);

//        ImVec2 size = ImGui::CalcItemSize(ImVec2(-FLT_MIN, 0.0f), ImGui::CalcItemWidth(), 40);
//        size.x *= 1.f;

////        // draw position when entering
////        ImVec2 draw_pos = ImGui::GetCursorPos();

//////        // capture user input
//////        uint press_index = array_size-1;
//////        float val = 0.f;
//////        bool pressed = false;

//////        uint starting_index = press_index;
//////        pressed = ImGuiToolkit::InvisibleDoubleSliderFloat("test", &press_index, &val, 0, array_size-1, size);
//////        if (pressed)
//////        {
//////            for (int i = MIN(starting_index, press_index); i < MAX(starting_index, press_index); ++i)
//////                arr[i] = val;

////////            starting_index = press_index;
//////        }

////        float x = -1.f;
////        float y = -1.f;
////        bool clicked = ImGuiToolkit::InvisibleCoordinatesFloat("test", &x, &y, size);
////        if (clicked) {
////            Log::Info("clic %f %f  in [%f  %f]", x, y, size.x, size.y);
////        }


////        // back to
////        ImGui::SetCursorPos(draw_pos);
////        // plot lines
////        ImGui::PlotLines("Lines", arr, array_size-1, 0, NULL, 0.0f, 1.0f, size);

//////        size.y = 20;
//////        ImGui::PlotHistogram("Hisfd", arr, array_size-1, 0, NULL, 0.0f, 1.0f, size);
//        bool r = false;
//        ImGuiToolkit::EditPlotHistoLines("Alpha", arr_histo, arr_lines, array_size, 0.f, 1.f, &r, size);

//        bool slider_pressed = ImGuiToolkit::TimelineSlider("timeline", &t, 0, duration, step, size.x);

//        ImGui::PopStyleVar(2);

//        ImGui::Text("Timeline t %" GST_STIME_FORMAT "\n", GST_STIME_ARGS(t));
//        ImGui::Text("Timeline Pressed %s", slider_pressed ? "on" : "off");

//        static int w = 0;
//        ImGui::SetNextItemWidth(size.x);
//        ImGui::SliderInt("##int", &w, 0, array_size-1);

//    }

//    ImGui::EndChild();

    static char str0[128] = "àöäüèáû вторая строчка";
    ImGui::InputText("input text", str0, IM_ARRAYSIZE(str0));
    std::string tra = SystemToolkit::transliterate(std::string(str0));
    ImGui::Text("Transliteration: '%s'", tra.c_str());

    ImGui::End();
}

void ShowConfig(bool* p_open)
{
    ImGui::SetNextWindowPos(ImVec2(1000, 20), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("About " APP_NAME APP_TITLE, p_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::End();
        return;
    }

    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_BOLD);
    ImGui::Text("%s %d.%d", APP_NAME, APP_VERSION_MAJOR, APP_VERSION_MINOR);
    ImGui::PopFont();
    ImGui::Separator();
    ImGui::Text("vimix performs graphical mixing and blending of\nseveral movie clips and computer generated graphics,\nwith image processing effects in real-time.");
    ImGui::Text("\nvimix is licensed under the GNU GPL version 3.\nCopyright 2019-2020 Bruno Herbelin.");
    ImGuiToolkit::ButtonOpenUrl("https://brunoherbelin.github.io/vimix/");
    ImGui::SameLine();

    static bool show_config = false;
    ImGui::SetNextItemWidth(-100.f);
    ImGui::Text("          Details");
    ImGui::SameLine();

    ImGuiToolkit::IconToggle(10,0,11,0,&show_config);
    if (show_config)
    {
        ImGui::Text("\nOpenGL options (enable all for optimal performance).");
        ImGui::Checkbox("Blit framebuffer (fast draw to output)", &Settings::application.render.blit);
        bool multi = (Settings::application.render.multisampling > 0);
        ImGui::Checkbox("Antialiasing framebuffer (fast multisampling)", &multi);
        Settings::application.render.multisampling = multi ? 3 : 0;
        bool vsync = (Settings::application.render.vsync < 2);
        ImGui::Checkbox("Sync refresh with monitor (v-sync 60Hz)", &vsync);
        Settings::application.render.vsync = vsync ? 1 : 2;
        ImGui::Text( ICON_FA_EXCLAMATION "  Restart the application for change to take effect.");
    }

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
    ImGuiToolkit::ButtonOpenUrl("https://www.opengl.org");
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
    ImGui::Begin("About Gstreamer", p_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);

    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_BOLD);
    ImGui::Text("GStreamer %s", GstToolkit::gst_version().c_str());
    ImGui::PopFont();
    ImGui::Separator();
    ImGui::Text("A flexible, fast and multiplatform multimedia framework.");
    ImGui::Text("GStreamer is licensed under the LGPL License.");
    ImGuiToolkit::ButtonOpenUrl("https://gstreamer.freedesktop.org/");
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
