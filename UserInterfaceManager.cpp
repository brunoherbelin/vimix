#include <iostream>
#include <cstring>
#include <sstream>
#include <thread>
#include <algorithm>

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

#include "defines.h"
#include "Log.h"
#include "SystemToolkit.h"
#include "UserInterfaceManager.h"
#include "RenderingManager.h"
#include "Resource.h"
#include "FileDialog.h"
#include "Settings.h"
#include "SessionCreator.h"
#include "ImGuiToolkit.h"
#include "ImGuiVisitor.h"
#include "GstToolkit.h"
#include "Mixer.h"
#include "Selection.h"
#include "FrameBuffer.h"
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "PickingVisitor.h"
#include "ImageShader.h"
#include "ImageProcessingShader.h"

#include "TextEditor.h"
static TextEditor editor;

// utility functions
void ShowAboutGStreamer(bool* p_open);
void ShowAboutOpengl(bool* p_open);
void ShowAbout(bool* p_open);

// static objects for multithreaded file dialog
static std::atomic<bool> fileDialogPending_ = false;
static std::atomic<bool> sessionFileDialogLoadFinished_ = false;
static std::atomic<bool> sessionFileDialogImport_ = false;
static std::atomic<bool> sessionFileDialogSaveFinished_ = false;
static std::string sessionFileDialogFilename_ = "";

static void SessionFileDialogOpen(std::string path)
{
     fileDialogPending_ = true;
     sessionFileDialogLoadFinished_ = false;

     char const * open_file_name;
     char const * open_pattern[1] = { "*.mix" };

     open_file_name = tinyfd_openFileDialog( "Open a session file", path.c_str(), 1, open_pattern, "vimix session", 0);

     if (!open_file_name)
        sessionFileDialogFilename_ = "";
     else
        sessionFileDialogFilename_ = std::string( open_file_name );

     sessionFileDialogLoadFinished_ = true;
}

static void SessionFileDialogSave(std::string path)
{
     fileDialogPending_ = true;
     sessionFileDialogSaveFinished_ = false;

     char const * save_file_name;
     char const * save_pattern[1] = { "*.mix" };

     save_file_name = tinyfd_saveFileDialog( "Save a session file", path.c_str(), 1, save_pattern, "vimix session");

     if (!save_file_name)
        sessionFileDialogFilename_ = "";
     else
     {
        sessionFileDialogFilename_ = std::string( save_file_name );
        // check extension
        std::string extension = sessionFileDialogFilename_.substr(sessionFileDialogFilename_.find_last_of(".") + 1);
        if (extension != "mix")
            sessionFileDialogFilename_ += ".mix";
     }

     sessionFileDialogSaveFinished_ = true;
}

static void ImportFileDialogOpen(char *filename, std::atomic<bool> *success, const std::string &path)
{
    if (fileDialogPending_)
        return;
    fileDialogPending_ = true;

    char const * open_pattern[18] = { "*.mix", "*.mp4", "*.mpg", "*.avi", "*.mov", "*.mkv",  "*.webm", "*.mod", "*.wmv", "*.mxf", "*.ogg", "*.flv", "*.asf", "*.jpg", "*.png", "*.gif", "*.tif", "*.svg" };
    char const * open_file_name;

    if (SystemToolkit::file_exists(path))
        open_file_name = tinyfd_openFileDialog( "Import a file", path.c_str(), 18, open_pattern, "All supported formats", 0);
    else
        open_file_name = tinyfd_openFileDialog( "Import a file", SystemToolkit::home_path().c_str(), 18, open_pattern, "All supported formats", 0);

    if (open_file_name) {
        sprintf(filename, "%s", open_file_name);
        *success = true;
    }
    else {
        *success = false;
    }

    fileDialogPending_ = false;
}

static void FolderDialogOpen(char *folder, std::atomic<bool> *success, const std::string &path)
{
    if (fileDialogPending_)
        return;
    fileDialogPending_ = true;

     char const * open_file_name;
     if (SystemToolkit::file_exists(path))
         open_file_name = tinyfd_selectFolderDialog("Select a folder", path.c_str());
     else
         open_file_name = tinyfd_selectFolderDialog("Select a folder", SystemToolkit::home_path().c_str());

     if (open_file_name) {
         sprintf(folder, "%s", open_file_name);
         *success = true;
     }
     else {
         *success = false;
     }

     fileDialogPending_ = false;
}


UserInterface::UserInterface()
{
    show_about = false;
    show_imgui_about = false;
    show_gst_about = false;
    show_opengl_about = false;
    currentTextEdit = "";
    screenshot_step = 0;
}

bool UserInterface::Init()
{
    if (Rendering::manager().mainWindow().window()== nullptr)
        return false;

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.MouseDrawCursor = true;
    io.FontGlobalScale = Settings::application.scale;

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(Rendering::manager().mainWindow().window(), true);
    ImGui_ImplOpenGL3_Init(Rendering::manager().glsl_version.c_str());

    // Setup Dear ImGui style
    ImGuiToolkit::SetAccentColor(static_cast<ImGuiToolkit::accent_color>(Settings::application.accent_color));

    //  Estalish the base size from the resolution of the monitor
    float base_font_size =  float(Rendering::manager().mainWindow().maxHeight()) / 100.f ;
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
    auto ctrl = io.ConfigMacOSXBehaviors ? io.KeySuper : io.KeyCtrl;

    // Application "CTRL +"" Shortcuts
    if ( ctrl ) {

        keyboard_modifier_active = true;

        if (ImGui::IsKeyPressed( GLFW_KEY_Q ))  {
            // Quit
            Rendering::manager().close();
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_O )) {
            // Open session
            sessionFileDialogImport_ = false;
            std::thread (SessionFileDialogOpen, Settings::application.recentSessions.path).detach();
            navigator.hidePannel();
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_S )) {
            // Save Session
            if (Mixer::manager().session()->filename().empty())
                std::thread (SessionFileDialogSave, Settings::application.recentSessions.path).detach();
            else
                Mixer::manager().save();
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_W )) {
            // New Session
            Mixer::manager().clear();
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
        else if (ImGui::IsKeyPressed( GLFW_KEY_P )) {
            // Logs
            Settings::application.widget.preview = !Settings::application.widget.preview;
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_M )) {
            // Logs
            Settings::application.widget.media_player = !Settings::application.widget.media_player;
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_A )) {
            // select all
            Mixer::manager().view()->selectAll();
        }

    }
    // No CTRL modifier
    else {
        keyboard_modifier_active = false;

        // Application F-Keys
        if (ImGui::IsKeyPressed( GLFW_KEY_F1 ))
            Mixer::manager().setView(View::MIXING);
        else if (ImGui::IsKeyPressed( GLFW_KEY_F2 ))
            Mixer::manager().setView(View::GEOMETRY);
        else if (ImGui::IsKeyPressed( GLFW_KEY_F3 ))
            Mixer::manager().setView(View::LAYER);
//        else if (ImGui::IsKeyPressed( GLFW_KEY_F4 )) // TODO REMOVE (DEBUG ONLY)
//            Mixer::manager().setView(View::TRANSITION);
        else if (ImGui::IsKeyPressed( GLFW_KEY_F11 ))
            Rendering::manager().mainWindow().toggleFullscreen();
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
                    Rendering::manager().mainWindow().setFullscreen(nullptr);
                else if (!Mixer::selection().empty())
                    Mixer::selection().clear();
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

}


void setMouseCursor(ImVec2 mousepos, View::Cursor c = View::Cursor())
{
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
    glm::vec2 mousepos(io.MousePos.x * io.DisplayFramebufferScale.y, io.MousePos.y* io.DisplayFramebufferScale.x);
    mousepos = glm::clamp(mousepos, glm::vec2(0.f), glm::vec2(io.DisplaySize.x * io.DisplayFramebufferScale.x, io.DisplaySize.y * io.DisplayFramebufferScale.y));
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
        else if ( ImGui::IsMouseDown(ImGuiMouseButton_Right)) {

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

                // ask the view what was picked
                picked = Mixer::manager().view()->pick(mousepos);

                // if nothing picked,
                if ( picked.first == nullptr ) {
                    // unset current
                    Mixer::manager().unsetCurrentSource();
                    navigator.hidePannel();
                    // clear selection
                    Mixer::selection().clear();
                }
                // something was picked
                else {
                    // get if a source was picked
                    Source *s = Mixer::manager().findSource(picked.first);
                    if (s != nullptr) {

                        // CTRL + clic = multiple selection
                        if (keyboard_modifier_active) {
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

                        Mixer::manager().setCurrentSource( s );
                        if (navigator.pannelVisible())
                            navigator.showPannelSource( Mixer::manager().indexCurrentSource() );

                        // indicate to view that an action can be initiated (e.g. grab)
                        Mixer::manager().view()->initiate();
                    }
                }
            }
        }
        else if ( ImGui::IsMouseReleased(ImGuiMouseButton_Left) )
        {
            view_drag = nullptr;
            mousedown = false;
            picked = { nullptr, glm::vec2(0.f) };
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
            if(view_drag == nullptr)
                view_drag = Mixer::manager().view();

            // only operate if the view didn't change
            if (view_drag == Mixer::manager().view()) {

                // action on current source
                Source *current = Mixer::manager().currentSource();
                if (current)
                {
                    // grab selected sources (current is also selected by default)
                    View::Cursor c = View::Cursor_Arrow;
                    for (auto it = Mixer::selection().begin(); it != Mixer::selection().end(); it++)
                        c = Mixer::manager().view()->grab(*it, mouseclic[ImGuiMouseButton_Left], mousepos, picked);
                    setMouseCursor(io.MousePos, c);
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
    if (sessionFileDialogLoadFinished_) {
        sessionFileDialogLoadFinished_ = false;
        if (!sessionFileDialogFilename_.empty()) {
            if (sessionFileDialogImport_)
                Mixer::manager().import(sessionFileDialogFilename_);
            else
                Mixer::manager().open(sessionFileDialogFilename_);
            Settings::application.recentSessions.path = SystemToolkit::path_filename(sessionFileDialogFilename_);
        }
        fileDialogPending_ = false;
    }
    if (sessionFileDialogSaveFinished_) {
        sessionFileDialogSaveFinished_ = false;
        if (!sessionFileDialogFilename_.empty()) {
            Mixer::manager().saveas(sessionFileDialogFilename_);
            Settings::application.recentSessions.path = SystemToolkit::path_filename(sessionFileDialogFilename_);
        }
        fileDialogPending_ = false;
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
//    ImVec2 geometry(static_cast<float>(Rendering::manager().Width()), static_cast<float>(Rendering::manager().Height()));
//    // file modal dialog
//    geometry.x *= 0.4f;
//    geometry.y *= 0.4f;
//    if ( !currentFileDialog.empty() && FileDialog::Instance()->Render(currentFileDialog.c_str(), geometry)) {
//        if (FileDialog::Instance()->IsOk == true) {
//            std::string filePathNameText = FileDialog::Instance()->GetFilepathName();
//            // done
//            currentFileDialog = "";
//        }
//        FileDialog::Instance()->CloseDialog(currentFileDialog.c_str());
//    }
//    FileDialog::RenderCurrent();

    // warning modal dialog
    Log::Render();

    // windows
    if (Settings::application.widget.toolbox)
        toolbox.Render();
    if (Settings::application.widget.preview)
        RenderPreview();
    if (Settings::application.widget.media_player)
        mediacontrol.Render();
    if (Settings::application.widget.shader_editor)
        RenderShaderEditor();
    if (Settings::application.widget.stats)
        ImGuiToolkit::ShowStats(&Settings::application.widget.stats, &Settings::application.widget.stats_corner);
    if (Settings::application.widget.logs)
        Log::ShowLogWindow(&Settings::application.widget.logs);

    // about
    if (show_about)
        ShowAbout(&show_about);
    if (show_imgui_about)
        ImGui::ShowAboutWindow(&show_imgui_about);
    if (show_gst_about)
        ShowAboutGStreamer(&show_gst_about);
    if (show_opengl_about)
        ShowAboutOpengl(&show_opengl_about);

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

void UserInterface::showMenuOptions()
{
    ImGui::MenuItem( ICON_FA_ARROW_CIRCLE_RIGHT "  Smooth transition", nullptr, &Settings::application.smooth_transition);

    ImGui::Separator();
    ImGui::MenuItem( ICON_FA_HISTORY " Restore session on start", nullptr, &Settings::application.recentSessions.load_at_start);
    ImGui::MenuItem( ICON_FA_FILE_DOWNLOAD "  Save on exit", nullptr, &Settings::application.recentSessions.save_on_exit);
}

void UserInterface::showMenuFile()
{
    if (ImGui::MenuItem( ICON_FA_FILE "  New", CTRL_MOD "W")) {
        Mixer::manager().clear();
        navigator.hidePannel();
    }
    ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
    ImGui::Combo("##AR", &Settings::application.render.ratio, FrameBuffer::aspect_ratio_name, IM_ARRAYSIZE(FrameBuffer::aspect_ratio_name) );
    ImGui::SetNextItemWidth( ImGui::GetContentRegionAvail().x );
    ImGui::Combo("##HEIGHT", &Settings::application.render.res, FrameBuffer::resolution_name, IM_ARRAYSIZE(FrameBuffer::resolution_name) );

    ImGui::Separator();

    if (ImGui::MenuItem( ICON_FA_FILE_UPLOAD "  Open", CTRL_MOD "O")) {
        // launch file dialog to open a session file
        sessionFileDialogImport_ = false;
        std::thread (SessionFileDialogOpen, Settings::application.recentSessions.path).detach();
        navigator.hidePannel();
    }
    if (ImGui::MenuItem( ICON_FA_FILE_EXPORT " Import")) {
        // launch file dialog to open a session file
        sessionFileDialogImport_ = true;
        std::thread (SessionFileDialogOpen, Settings::application.recentSessions.path).detach();
        navigator.hidePannel();
    }
    if (ImGui::MenuItem( ICON_FA_FILE_DOWNLOAD "  Save", CTRL_MOD "S")) {
        if (Mixer::manager().session()->filename().empty())
            std::thread (SessionFileDialogSave, Settings::application.recentSessions.path).detach();
        else
            Mixer::manager().save();
        navigator.hidePannel();
    }
    if (ImGui::MenuItem( ICON_FA_SAVE "  Save as")) {
        std::thread (SessionFileDialogSave, Settings::application.recentSessions.path).detach();
        navigator.hidePannel();
    }

    ImGui::Separator();
    if (ImGui::MenuItem( ICON_FA_POWER_OFF " Quit", CTRL_MOD "Q")) {
        Rendering::manager().close();
    }
}


ToolBox::ToolBox()
{
    show_demo_window = false;
    show_icons_window = false;
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
            if ( ImGui::MenuItem( ICON_FA_CAMERA_RETRO "  Screenshot") )
                UserInterface::manager().StartScreenshot();

            ImGui::MenuItem("Blit", nullptr, &Settings::application.render.blit);
            ImGui::MenuItem("Multisampling", nullptr, (Settings::application.render.multisampling > 0), false);

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Dev"))
        {
            ImGui::MenuItem("Icons", nullptr, &show_icons_window);
            ImGui::MenuItem("Demo ImGui", nullptr, &show_demo_window);

            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }


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
    if (show_demo_window)
        ImGui::ShowDemoWindow(&show_demo_window);

}

void UserInterface::RenderPreview()
{
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
        // menu (no title bar)
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu(ICON_FA_LAPTOP " Preview"))
            {
                if ( ImGui::MenuItem( ICON_FA_WINDOW_RESTORE "  Show output window") )
                    Rendering::manager().outputWindow().show();

                if ( ImGui::MenuItem( ICON_FA_TIMES "  Close") )
                    Settings::application.widget.preview = false;

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
        if (ImGui::IsItemHovered()) {

            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            draw_list->AddRectFilled(draw_pos,  ImVec2(draw_pos.x + width, draw_pos.y + ImGui::GetTextLineHeightWithSpacing()), IM_COL32(5, 5, 5, 100));

            ImGui::SetCursorScreenPos(draw_pos);
            ImGui::Text(" %d x %d px, %d fps", output->width(), output->height(), int(1000.f / Mixer::manager().dt()) );
        }

        ImGui::End();
    }
}

void UserInterface::showMediaPlayer(MediaPlayer *mp)
{
    Settings::application.widget.media_player = true;
    mediacontrol.setMediaPlayer(mp);
}

#define LABEL_AUTO_MEDIA_PLAYER "Play active source"

MediaController::MediaController() : mp_(nullptr), current_(LABEL_AUTO_MEDIA_PLAYER), follow_active_source_(true), media_playing_mode_(false)
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
        if (ImGui::BeginMenu(ICON_FA_FILM))
        {
            ImGui::MenuItem( ICON_FA_EYE " Preview", nullptr, &Settings::application.widget.media_player_view);

            if ( ImGui::MenuItem( ICON_FA_TIMES "  Close") )
                Settings::application.widget.media_player = false;

            ImGui::EndMenu();
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
        float width = ImGui::GetContentRegionAvail().x;

        if (Settings::application.widget.media_player_view) {
            // set an image height to fill the vertical space, minus the height of control bar
            float image_height = ImGui::GetContentRegionAvail().y;
            if (mp_->duration() != GST_CLOCK_TIME_NONE)
                image_height -= 3.f * ImGui::GetFrameHeight();

            // display media
            ImVec2 imagesize ( image_height * mp_->aspectRatio(), image_height);
            ImVec2 tooltip_pos = ImGui::GetCursorScreenPos();
            ImGui::SetCursorPosX(ImGui::GetCursorPos().x + (ImGui::GetContentRegionAvail().x - imagesize.x) / 2.0);
            ImGui::Image((void*)(uintptr_t)mp_->texture(), imagesize);
            ImVec2 return_to_pos = ImGui::GetCursorPos();
            if (ImGui::IsItemHovered()) {

                float tooltip_height = (follow_active_source_? 3.f:2.f)* ImGui::GetTextLineHeightWithSpacing();

                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                draw_list->AddRectFilled(tooltip_pos,  ImVec2(tooltip_pos.x + width, tooltip_pos.y + tooltip_height), IM_COL32(5, 5, 5, 100));

                ImGui::SetCursorScreenPos(tooltip_pos);
                if (follow_active_source_)
                    ImGui::Text(" %s", mp_->filename().c_str());
                ImGui::Text(" %s", mp_->codec().c_str());
                if ( mp_->frameRate() > 0.f )
                    ImGui::Text(" %d x %d px, %.2f / %.2f fps", mp_->width(), mp_->height(), mp_->updateFrameRate() , mp_->frameRate() );
                else
                    ImGui::Text(" %d x %d px", mp_->width(), mp_->height());

            }
            ImGui::SetCursorPos(return_to_pos);
        }

        // Control bar
        if ( mp_->isEnabled() && mp_->duration() != GST_CLOCK_TIME_NONE) {

            float spacing = ImGui::GetStyle().ItemInnerSpacing.x;

            if (ImGui::Button(ICON_FA_FAST_BACKWARD))
                mp_->rewind();
            ImGui::SameLine(0, spacing);

            // display buttons Play/Stop depending on current playing mode
            if (media_playing_mode_) {

                if (ImGui::Button(ICON_FA_STOP " Stop"))
                    media_playing_mode_ = false;
                ImGui::SameLine(0, spacing);

                ImGui::PushButtonRepeat(true);
                if (ImGui::Button(ICON_FA_FORWARD))
                    mp_->fastForward ();
                ImGui::PopButtonRepeat();
            }
            else {

                if (ImGui::Button(ICON_FA_PLAY "  Play"))
                    media_playing_mode_ = true;
                ImGui::SameLine(0, spacing);

                ImGui::PushButtonRepeat(true);
                if (ImGui::Button(ICON_FA_STEP_FORWARD))
                    mp_->seekNextFrame();
                ImGui::PopButtonRepeat();
            }

            ImGui::SameLine(0, MAX(spacing * 4.f, width - 400.f) );

            // loop modes button
            static int current_loop = 0;
            static std::vector< std::pair<int, int> > iconsloop = { {0,15}, {1,15}, {19,14} };
            current_loop = (int) mp_->loop();
            if ( ImGuiToolkit::ButtonIconMultistate(iconsloop, &current_loop) )
                mp_->setLoop( (MediaPlayer::LoopMode) current_loop );
            // speed slider
            float speed = static_cast<float>(mp_->playSpeed());
            ImGui::SameLine(0, spacing);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 40.0);
            // ImGui::SetNextItemWidth(width - 90.0);
            if (ImGui::DragFloat( "##Speed", &speed, 0.01f, -10.f, 10.f, "Speed x %.1f", 2.f))
                mp_->setPlaySpeed( static_cast<double>(speed) );
            // reset play to x1
            ImGui::SameLine(0, spacing);
            if (ImGuiToolkit::ButtonIcon(19, 15)) {
                speed = 1.f;
                mp_->setPlaySpeed( static_cast<double>(speed) );
                mp_->setLoop( MediaPlayer::LOOP_REWIND );
            }

            // custom timeline slider
            guint64 current_t = mp_->position();
            guint64 seek_t = current_t;
            bool slider_pressed = ImGuiToolkit::TimelineSlider( "simpletimeline", &seek_t,
                                                                mp_->duration(), mp_->frameDuration());
            // if the seek target time is different from the current position time
            // (i.e. the difference is less than one frame)
            if ( ABS_DIFF (current_t, seek_t) > mp_->frameDuration() ) {
                // request seek (ASYNC)
                mp_->seekTo(seek_t);
                slider_pressed = false;
            }
            // play/stop command should be following the playing mode (buttons)
            // AND force to stop when the slider is pressed
            bool media_play = media_playing_mode_ & (!slider_pressed);

            // apply play action to media only if status should change
            // NB: The seek command performed an ASYNC state change, but
            // gst_element_get_state called in isPlaying(true) will wait
            // for the state change to complete : do not remove it!
            if ( mp_->isPlaying(true) != media_play ) {
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
        ImGui::Text("No selection");
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
    new_source_type_ = 0;

    // clean start
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
    sprintf(file_browser_path_, " ");
    new_source_preview_.setSource();
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
}

void Navigator::Render()
{
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
    sourcelist_height_ = height_ - 6.f * ImGui::GetTextLineHeight(); // space for 3 icons of view
    float icon_width = width_ - 2.f * style.WindowPadding.x;        // icons keep padding
    ImVec2 iconsize(icon_width, icon_width);

        // Left bar top
        ImGui::SetNextWindowPos( ImVec2(0, 0), ImGuiCond_Always );
        ImGui::SetNextWindowSize( ImVec2(width_, sourcelist_height_), ImGuiCond_Always );
        ImGui::SetNextWindowBgAlpha(0.95f); // Transparent background
        if (ImGui::Begin( ICON_FA_BARS " Navigator", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |  ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing))
        {
            if (Settings::application.current_view < 4) {

                // the "=" icon for menu
                if (ImGui::Selectable( ICON_FA_BARS, &selected_button[NAV_MENU], 0, iconsize))
                {
                    //            Mixer::manager().unsetCurrentSource();
                    applyButtonSelection(NAV_MENU);
                }

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
                    if (ImGui::Selectable( (*iter)->initials(), &selected_button[index], 0, iconsize))
                    {
                        applyButtonSelection(index);
                        if (selected_button[index])
                            Mixer::manager().setCurrentSource(index);
                    }
                }

                // the "+" icon for action of creating new source
                if (ImGui::Selectable( ICON_FA_PLUS, &selected_button[NAV_NEW], 0, iconsize))
                {
                    Mixer::manager().unsetCurrentSource();
                    applyButtonSelection(NAV_NEW);
                }

            }
            else {
                // the "=" icon for menu
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
        if (ImGui::Selectable( ICON_FA_BULLSEYE, &selected_view[1], 0, iconsize))
        {
            Mixer::manager().setView(View::MIXING);
        }
        if (ImGui::Selectable( ICON_FA_OBJECT_UNGROUP , &selected_view[2], 0, iconsize))
        {
            Mixer::manager().setView(View::GEOMETRY);
        }
        if (ImGui::Selectable( ICON_FA_IMAGES, &selected_view[3], 0, iconsize))
//            if (ImGui::Selectable( ICON_FA_LAYER_GROUP, &selected_view[3], 0, iconsize))
        {
            Mixer::manager().setView(View::LAYER);
        }

    }
    ImGui::End();

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
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();

}

// Source pannel : *s was checked before
void Navigator::RenderSourcePannel(Source *s)
{
    if (s == nullptr || Settings::application.current_view >3)
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
        // ensure change is applied
        s->touch();
        // delete button
        ImGui::Text(" ");
        // Action on source
        if ( ImGui::Button( ICON_FA_SHARE_SQUARE " Clone", ImVec2(ImGui::GetContentRegionAvail().x, 0)) )
            Mixer::manager().addSource ( Mixer::manager().createSourceClone() );
        if ( ImGui::Button( ICON_FA_BACKSPACE " Delete", ImVec2(ImGui::GetContentRegionAvail().x, 0)) ) {
            Mixer::manager().deleteSource(s);
        }
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

void SourcePreview::draw(float width)
{
    if(source_) {
        // cancel if failed
        if (source_->failed())
            setSource();
        else
        {
            // update source
            ImGuiIO& io = ImGui::GetIO();
            source_->update( 1.f / io.Framerate);
            // render framebuffer
            source_->render();
            // draw preview
            ImVec2 preview_size(width, width / source_->frame()->aspectRatio());
            ImGui::Image((void*)(uintptr_t) source_->frame()->texture(), preview_size);
            ImGui::Text("%s ", label_.c_str());
        }
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
        ImGui::Text("New Source");
        ImGui::PopFont();

        ImGui::SetCursorPosY(width_);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);

        static const char* origin_names[2] = { ICON_FA_FILE " File", ICON_FA_SITEMAP " Internal" };
        // TODO IMPLEMENT EXTERNAL SOURCES static const char* origin_names[3] = { ICON_FA_FILE " File", ICON_FA_SITEMAP " Internal", ICON_FA_PLUG " External" };
        if (ImGui::Combo("Origin", &new_source_type_, origin_names, IM_ARRAYSIZE(origin_names)) )
            new_source_preview_.setSource();

        // File Source creation
        if (new_source_type_ == 0) {

            ImGui::SetCursorPosY(2.f * width_);

            // browse for a filename
            static std::atomic<bool> file_selected = false;
            if ( ImGui::Button( ICON_FA_FILE_IMPORT " Open file", ImVec2(ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN, 0)) ) {
                // clear string before selection
                file_selected = false;
                std::thread (ImportFileDialogOpen, file_browser_path_, &file_selected, Settings::application.recentImport.path).detach();
            }
            // Indication
            ImGui::SameLine();
            ImGuiToolkit::HelpMarker("Create a source from a file:\n- Video (*.mpg, *mov, *.avi, etc.)\n- Image (*.jpg, *.png, etc.)\n- Vector graphics (*.svg)\n- vimix session (*.mix)\n\nEquivalent to dropping the file in the workspace.");

            if ( file_selected ) {
                file_selected = false;
                std::string open_filename(file_browser_path_);
                // create a source with this file
                std::string label = open_filename.substr( open_filename.size() - MIN( 35, open_filename.size()) );
                new_source_preview_.setSource( Mixer::manager().createSourceFile(open_filename), label);
            }

            // combo of recent media filenames
            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            if (ImGui::BeginCombo("##RecentImport", "Recent files"))
            {
                for (auto path = Settings::application.recentImport.filenames.begin();
                     path != Settings::application.recentImport.filenames.end(); path++ )
                {
                    std::string label = path->substr( path->size() - MIN( 35, path->size()) );
                    if (ImGui::Selectable( label.c_str() )) {
                        new_source_preview_.setSource( Mixer::manager().createSourceFile(path->c_str()), label);
                    }
                }
                ImGui::EndCombo();
            }
            // if a new source was added
            if (new_source_preview_.ready()) {
                // show preview
                new_source_preview_.draw(ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN);
                // or press Validate button
                ImGui::Text(" ");
                if ( ImGui::Button(ICON_FA_CHECK "  Create", ImVec2(pannel_width_ - padding_width_, 0)) ) {
                    Mixer::manager().addSource(new_source_preview_.getSource());
                    selected_button[NAV_NEW] = false;
                }
            }
        }
        // Software Source creator
        else if (new_source_type_ == 1){

            ImGui::SetCursorPosY(2.f * width_);

            // fill new_source_preview with a new source
            ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
            if (ImGui::BeginCombo("##Source", "Select input"))
            {
                std::string label = "Rendering output";
                if (ImGui::Selectable( label.c_str() )) {
                    new_source_preview_.setSource( Mixer::manager().createSourceRender(), label);
                }
                SourceList::iterator iter;
                for (iter = Mixer::manager().session()->begin(); iter != Mixer::manager().session()->end(); iter++)
                {
                    label = std::string("Clone of ") + (*iter)->name();
                    if (ImGui::Selectable( label.c_str() )) {
                        new_source_preview_.setSource( Mixer::manager().createSourceClone((*iter)->name()),label);
                    }
                }
                ImGui::EndCombo();
            }

            // Indication
            ImGui::SameLine();
            ImGuiToolkit::HelpMarker("Create a source generating images\nfrom internal vimix objects\nor from software algorithms.");

            // if a new source was added
            if (new_source_preview_.ready()) {
                // show preview
                new_source_preview_.draw(ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN);
                // ask to import the source in the mixer
                ImGui::Text(" ");
                if ( ImGui::Button( ICON_FA_CHECK "  Create", ImVec2(pannel_width_ - padding_width_, 0)) ) {
                    Mixer::manager().addSource(new_source_preview_.getSource());
                    selected_button[NAV_NEW] = false;
                }
            }
        }
        // Hardware
        else {
            // helper
            ImGui::SetCursorPosX(pannel_width_ - 30 + IMGUI_RIGHT_ALIGN);
            ImGuiToolkit::HelpMarker("Create a source capturing images\nfrom external devices or network.");
        }

    }
    ImGui::End();
}


void Navigator::RenderTransitionPannel()
{
    if (Settings::application.current_view < 4) {
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
        ImGuiToolkit::ButtonSwitch( ICON_FA_SIGN_IN_ALT " Automatic open", &Settings::application.transition.auto_open);
//        ImGuiToolkit::ButtonSwitch( ICON_FA_SIGN_OUT_ALT " Automatic start", &Settings::application.transition.auto_start);

        // Transition options
        ImGui::Text(" ");
        ImGui::Text("Animation");
        if (ImGuiToolkit::ButtonIcon(4, 13)) Settings::application.transition.duration = 1.f;
        ImGui::SameLine(0, 10);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ImGui::SliderFloat("Duration", &Settings::application.transition.duration, TRANSITION_MIN_DURATION, TRANSITION_MAX_DURATION, "%.1f s");
        if (ImGuiToolkit::ButtonIcon(9, 1)) Settings::application.transition.profile = 0;
        ImGui::SameLine(0, 10);
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        ImGui::Combo("Curve", &Settings::application.transition.profile, "Linear\0Quadratic\0");

        ImGui::Text(" ");
        if ( ImGui::Button( ICON_FA_PLAY " Play ", ImVec2(IMGUI_RIGHT_ALIGN, 0)) ){
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

        // TODO fixe fullscreen for OSX :(
#ifndef APPLE
        // Icon to switch fullscreen
        ImGui::SetCursorPos(ImVec2(pannel_width_  - 35.f, 15.f));
        const char *tooltip[2] = {"Enter Fullscreen", "Exit Fullscreen"};
        bool fs = Rendering::manager().mainWindow().isFullscreen();
        if ( ImGuiToolkit::IconToggle(3,15,2,15, &fs, tooltip ) ) {
            Rendering::manager().mainWindow().toggleFullscreen();
            ImGui::End();
            return;
        }
#endif
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

        ImGui::SetCursorPosX(pannel_width_ IMGUI_RIGHT_ALIGN);
        if (ImGui::BeginMenu("Option"))
        {
            UserInterface::manager().showMenuOptions();
            ImGui::EndMenu();
        }

        static bool selection_session_mode_changed = true;
        static int selection_session_mode = 0;

        //
        // Session quick selection pannel
        //

        // return from thread for folder openning
        static std::atomic<bool> folder_selected = false;
        if (folder_selected)  {
            folder_selected = false;
            selection_session_mode = 1;
            selection_session_mode_changed = true;
            Settings::application.recentFolders.push(file_browser_path_);
            Settings::application.recentFolders.path.assign(file_browser_path_);
        }

        // Show combo box of quick selection modes
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if (ImGui::BeginCombo("##SelectionSession", SystemToolkit::trunc_filename(Settings::application.recentFolders.path, 25).c_str() )) {

            // Option 0 : recent files
            if (ImGui::Selectable( ICON_FA_HISTORY " Recent Files") ) {
                 Settings::application.recentFolders.path = "Recent Files";
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
                std::thread (FolderDialogOpen, file_browser_path_, &folder_selected, Settings::application.recentFolders.path).detach();
            }
            ImGui::EndCombo();
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
                    Settings::application.recentFolders.path.assign("Recent Files");
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
                Settings::application.recentSessions.valid_file = false;
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
        ImGui::ListBoxHeader("##Sessions", 7);
        static std::string file_info = "";
        static std::list<std::string>::iterator file_selected = sessions_list.end();
        for(auto filename = sessions_list.begin(); filename != sessions_list.end(); filename++) {
            if (ImGui::Selectable( SystemToolkit::filename(*filename).c_str(), false, ImGuiSelectableFlags_AllowDoubleClick )) {
                if (ImGui::IsMouseDoubleClicked(0)) {
                    Mixer::manager().open( *filename );
                    session_selected = true;
                }
                else  {
                    file_info = SessionCreator::info(*filename);
                    file_selected = filename;
                }
            }
            if (ImGui::IsItemHovered() && file_selected != filename) {
                file_info.clear();
                file_selected = sessions_list.end();
            }
        }
        ImGui::ListBoxFooter();
        if (!file_info.empty()) {
            ImGui::BeginTooltip();
            ImGui::Text("%s", file_info.c_str());
            ImGui::EndTooltip();
        }

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

        // Continue Main pannel
        // WINDOWS
        ImGui::Text(" ");
        ImGui::Text("Windows");
        ImGuiToolkit::ButtonSwitch( IMGUI_TITLE_PREVIEW, &Settings::application.widget.preview, CTRL_MOD "P");
        ImGuiToolkit::ButtonSwitch( IMGUI_TITLE_MEDIAPLAYER, &Settings::application.widget.media_player, CTRL_MOD "M");
#ifndef NDEBUG
        ImGuiToolkit::ButtonSwitch( IMGUI_TITLE_SHADEREDITOR, &Settings::application.widget.shader_editor, CTRL_MOD  "E");
        ImGuiToolkit::ButtonSwitch( IMGUI_TITLE_TOOLBOX, &Settings::application.widget.toolbox, CTRL_MOD  "T");
        ImGuiToolkit::ButtonSwitch( ICON_FA_LIST " Logs", &Settings::application.widget.logs, CTRL_MOD "L");
#endif
        ImGuiToolkit::ButtonSwitch( ICON_FA_TACHOMETER_ALT " Metrics", &Settings::application.widget.stats);

        // Settings application appearance
        ImGui::Text("  ");
        ImGui::Text("Appearance");
        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
        if ( ImGui::DragFloat("Scale", &Settings::application.scale, 0.01, 0.8f, 1.2f, "%.1f"))
            ImGui::GetIO().FontGlobalScale = Settings::application.scale;

//        ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
//        if ( ImGui::Combo("Accent", &Settings::application.accent_color, "Blue\0Orange\0Grey\0\0"))
//            ImGuiToolkit::SetAccentColor(static_cast<ImGuiToolkit::accent_color>(Settings::application.accent_color));
        bool b = ImGui::RadioButton("Blue", &Settings::application.accent_color, 0); ImGui::SameLine();
        bool o = ImGui::RadioButton("Orange", &Settings::application.accent_color, 1); ImGui::SameLine();
        bool g = ImGui::RadioButton("Grey", &Settings::application.accent_color, 2);
        if (b || o || g)
            ImGuiToolkit::SetAccentColor(static_cast<ImGuiToolkit::accent_color>(Settings::application.accent_color));

        // Bottom aligned Logo & About
        static unsigned int vimixicon = Resource::getTextureImage("images/vimix_256x256.png");
        static float h = 4.f * ImGui::GetTextLineHeightWithSpacing();
        if ( ImGui::GetCursorPosY() + h + 128.f < height_ ) {
            ImGui::SetCursorPos(ImVec2(pannel_width_ / 2.f - 64.f, height_ -h - 128.f));
            ImGui::Image((void*)(intptr_t)vimixicon, ImVec2(128, 128));
            ImGui::Text("");
        }
        else {
            ImGui::SetCursorPosY(height_ -h);
            ImGui::Text("About");
        }
        if ( ImGui::Button( ICON_FA_CROW " About vimix", ImVec2(ImGui::GetContentRegionAvail().x, 0)) )
            UserInterface::manager().show_about = true;
        if ( ImGui::Button("    ImGui    "))
            UserInterface::manager().show_imgui_about = true;
        ImGui::SameLine();
        if ( ImGui::Button(" GStreamer "))
            UserInterface::manager().show_gst_about = true;
        ImGui::SameLine();
        if ( ImGui::Button("OpenGL", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
            UserInterface::manager().show_opengl_about = true;

    }
    ImGui::End();
}


void ShowAbout(bool* p_open)
{
    ImGui::SetNextWindowPos(ImVec2(1000, 20), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("About " APP_TITLE, p_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::End();
        return;
    }

    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_BOLD);
    ImGui::Text("%s %d.%d", APP_NAME, APP_VERSION_MAJOR, APP_VERSION_MINOR);
    ImGui::PopFont();
    ImGui::Separator();
    ImGui::Text("vimix is a video mixing software for live performance.");
    ImGui::Text("vimix is licensed under the GNU GPL version 3. Copyright 2019-2020 Bruno Herbelin.");
    ImGuiToolkit::ButtonOpenUrl("https://github.com/brunoherbelin/v-mix");

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

    ImGui::SetNextWindowPos(ImVec2(430, 20), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("About Gstreamer", p_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::End();
        return;
    }

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

        // filter
        if ( filter.empty() )
            filteredlist = GstToolkit::all_plugins();
        else {
            std::list<std::string> plist = GstToolkit::all_plugins();
            for (auto const& i: plist) {
                // add plugin if plugin name match
                if ( i.find(filter) != std::string::npos )
                    filteredlist.push_back( i.c_str() );
                // check in features
                std::list<std::string> flist = GstToolkit::all_plugin_features(i);
                for (auto const& j: flist) {
                    // add plugin if feature name matches
                    if ( j.find(filter) != std::string::npos )
                        filteredlist.push_back( i.c_str() );
                }
            }
        }

        // display list
        for (auto const& t: filteredlist) {
            ImGui::Text("> %s", t.c_str());
            std::list<std::string> flist = GstToolkit::all_plugin_features(t);
            for (auto const& j: flist) {
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
