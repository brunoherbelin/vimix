#include <iostream>
#include <cstring>
#include <sstream>
#include <thread>

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
#include "TextEditor.h"
#include "UserInterfaceManager.h"
#include "RenderingManager.h"
#include "Resource.h"
#include "FileDialog.h"
#include "Settings.h"
#include "ImGuiToolkit.h"
#include "GstToolkit.h"
#include "Mixer.h"
#include "FrameBuffer.h"
#include "MediaPlayer.h"
#include "PickingVisitor.h"

static std::thread loadThread;
static bool loadThreadDone = false;
static TextEditor editor;


static void NativeOpenFile(std::string ext) 
{ 

     char const * lTheOpenFileName;
     char const * lFilterPatterns[2] = { "*.txt", "*.text" };

     lTheOpenFileName = tinyfd_openFileDialog( "Open a text file", "", 2, lFilterPatterns, nullptr, 0);

     if (!lTheOpenFileName)
     {
        tinyfd_messageBox(APP_TITLE, "Open file name is NULL.             ", "ok", "warning", 1);
     }
     else
     {
        char buf[1024];
        sprintf( buf, "he selected file is :\n %s", lTheOpenFileName );
        tinyfd_messageBox(APP_TITLE, buf, "ok", "info", 1);
     }

     loadThreadDone = true;
}



UserInterface::UserInterface()
{
    currentTextEdit = "";
}

bool UserInterface::Init()
{
    if (Rendering::manager().main_window_ == nullptr)
        return false;

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.MouseDrawCursor = true;

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(Rendering::manager().main_window_, true);
    ImGui_ImplOpenGL3_Init(Rendering::manager().glsl_version.c_str());

    // Setup Dear ImGui style
    ImGuiToolkit::SetAccentColor(static_cast<ImGuiToolkit::accent_color>(Settings::application.accent_color));

    // Load Fonts (using resource manager, a temporary copy of the raw data is necessary)
    ImGuiToolkit::SetFont(ImGuiToolkit::FONT_DEFAULT, "Roboto-Regular", 22);
    ImGuiToolkit::SetFont(ImGuiToolkit::FONT_BOLD, "Roboto-Bold", 22);
    ImGuiToolkit::SetFont(ImGuiToolkit::FONT_ITALIC, "Roboto-Italic", 22);
    ImGuiToolkit::SetFont(ImGuiToolkit::FONT_MONO, "Hack-Regular", 20);
    io.FontGlobalScale = Settings::application.scale;

    // Style
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding.x = 12.f;
    style.WindowPadding.y = 6.f;
    style.FramePadding.x = 10.f;
    style.FramePadding.y = 5.f;
    style.IndentSpacing = 22.f;
    style.ItemSpacing.x = 12.f;
    style.ItemSpacing.y = 4.f;
    style.ItemInnerSpacing.x = 8.f;
    style.ItemInnerSpacing.y = 3.f;
    style.WindowRounding = 8.f;
    style.ChildRounding = 4.f;
    style.FrameRounding = 4.f;
    style.GrabRounding = 2.f;
    style.GrabMinSize = 14.f;
    style.Alpha = 0.92f;

    // prevent bug with imgui clipboard (null at start)
    ImGui::SetClipboardText("");

    return true;
}

void UserInterface::handleKeyboard()
{
    ImGuiIO& io = ImGui::GetIO(); 

    // Application "CTRL +"" Shortcuts
    if ( io.KeyCtrl ) {

        if (ImGui::IsKeyPressed( GLFW_KEY_Q ))  
            Rendering::manager().Close();
        else if (ImGui::IsKeyPressed( GLFW_KEY_O ))
        {

        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_S ))
        {
            std::cerr <<" Save File " << std::endl;
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_W ))
        {
            std::cerr <<" Close File " << std::endl;
        }
        else if (ImGui::IsKeyPressed( GLFW_KEY_L ))
            mainwindow.ToggleLogs();

    }

    // Application F-Keys
    if (ImGui::IsKeyPressed( GLFW_KEY_F1 ))
        Mixer::manager().setCurrentView(View::MIXING);
    if (ImGui::IsKeyPressed( GLFW_KEY_F2 ))
        Mixer::manager().setCurrentView(View::GEOMETRY);
    if (ImGui::IsKeyPressed( GLFW_KEY_F12 ))
        Rendering::manager().ToggleFullscreen();
    else if (ImGui::IsKeyPressed( GLFW_KEY_F11 ))
        mainwindow.StartScreenshot();
            
}

void UserInterface::handleMouse()
{

    ImGuiIO& io = ImGui::GetIO();
    glm::vec2 mousepos(io.MousePos.x * io.DisplayFramebufferScale.y, io.MousePos.y* io.DisplayFramebufferScale.x);
    static glm::vec2 mouseclic[2];
    mouseclic[ImGuiMouseButton_Left] = glm::vec2(io.MouseClickedPos[ImGuiMouseButton_Left].x * io.DisplayFramebufferScale.y, io.MouseClickedPos[ImGuiMouseButton_Left].y* io.DisplayFramebufferScale.x);
    mouseclic[ImGuiMouseButton_Right] = glm::vec2(io.MouseClickedPos[ImGuiMouseButton_Right].x * io.DisplayFramebufferScale.y, io.MouseClickedPos[ImGuiMouseButton_Right].y* io.DisplayFramebufferScale.x);

    // if not on any window
    if ( !ImGui::IsAnyWindowHovered() && !ImGui::IsAnyWindowFocused() )
    {
        ImGui::FocusWindow(0);
        //
        // Mouse wheel over background
        //
        if ( io.MouseWheel != 0) {
            // scroll => zoom current view
            Mixer::manager().currentView()->zoom( io.MouseWheel );
        }
        //
        // RIGHT Mouse button
        //

        if ( ImGui::IsMouseDragging(ImGuiMouseButton_Right, 10.0f) )
        {
            // right mouse drag => drag current view
            Mixer::manager().currentView()->drag( mouseclic[ImGuiMouseButton_Right], mousepos);

            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
        }
        else {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);

            if ( ImGui::IsMouseDown(ImGuiMouseButton_Right)) {

                // TODO CONTEXT MENU

//                glm::vec3 point = Rendering::manager().unProject(mousepos, Mixer::manager().currentView()->scene.root()->transform_ );


            }
        }

        //
        // LEFT Mouse button
        //
        if ( ImGui::IsMouseDragging(ImGuiMouseButton_Left, 10.0f) )
        {
            Source *current = Mixer::manager().currentSource();
            if (current)
            {
                // drag current source
                Mixer::manager().currentView()->grab( mouseclic[ImGuiMouseButton_Left], mousepos, current);

            }
            else {

//                Log::Info("Mouse drag (%.1f,%.1f)(%.1f,%.1f)", io.MouseClickedPos[0].x, io.MouseClickedPos[0].y, io.MousePos.x, io.MousePos.y);
                // Selection area
                ImGui::GetBackgroundDrawList()->AddRect(io.MouseClickedPos[ImGuiMouseButton_Left], io.MousePos,
                        ImGui::GetColorU32(ImGuiCol_ResizeGripHovered));
                ImGui::GetBackgroundDrawList()->AddRectFilled(io.MouseClickedPos[ImGuiMouseButton_Left], io.MousePos,
                        ImGui::GetColorU32(ImGuiCol_ResizeGripHovered, 0.3f));
            }
        }
        else if ( ImGui::IsMouseDown(ImGuiMouseButton_Left)) {

            // get coordinate in world view of mouse cursor
            glm::vec3 point = Rendering::manager().unProject(mousepos);

            // picking visitor traverses the scene
            PickingVisitor pv(point);
            Mixer::manager().currentView()->scene.accept(pv);

            // picking visitor found nodes?
            if (pv.picked().empty())
                Mixer::manager().unsetCurrentSource();
            else
                Mixer::manager().setCurrentSource(pv.picked().back());

        }
        else if ( ImGui::IsMouseReleased(ImGuiMouseButton_Left) )
        {


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

    // Main window
    mainwindow.Render();

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

    FileDialog::RenderCurrent();

    // warning modal dialog
    Log::Render();

    // windows
    if (Settings::application.preview)
        RenderPreview();
    if (Settings::application.media_player)
        RenderMediaPlayer();
    if (Settings::application.shader_editor)
        RenderShaderEditor();

    // all IMGUI Rendering
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void UserInterface::Terminate()
{
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}



MainWindow::MainWindow()
{
    show_overlay_stats = false;
    show_app_about = false;
    show_gst_about = false;
    show_opengl_about = false;
    show_demo_window = false;
    show_logs_window = false;
    show_icons_window = false;
    screenshot_step = 0;
}

void MainWindow::ShowStats(bool* p_open)
{
    const float DISTANCE = 10.0f;
    static int corner = 1;
    ImGuiIO& io = ImGui::GetIO();
    if (corner != -1)
    {
        ImVec2 window_pos = ImVec2((corner & 1) ? io.DisplaySize.x - DISTANCE : DISTANCE, (corner & 2) ? io.DisplaySize.y - DISTANCE : DISTANCE);
        ImVec2 window_pos_pivot = ImVec2((corner & 1) ? 1.0f : 0.0f, (corner & 2) ? 1.0f : 0.0f);
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
    }

    ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background

    if (ImGui::Begin("v-mix statistics", NULL, (corner != -1 ? ImGuiWindowFlags_NoMove : 0) | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
    {

        ImGuiToolkit::PushFont(ImGuiToolkit::FONT_MONO);
        if (ImGui::IsMousePosValid())
            ImGui::Text("Mouse  (%.1f,%.1f)", io.MousePos.x, io.MousePos.y);
        else
            ImGui::Text("Mouse  <invalid>");

        ImGui::Text("Window  (%.1f,%.1f)", io.DisplaySize.x, io.DisplaySize.y);
        ImGui::Text("Rendering %.1f FPS", io.Framerate);
        ImGui::PopFont();

        if (ImGui::BeginPopupContextWindow())
        {
            if (ImGui::MenuItem("Custom",       NULL, corner == -1)) corner = -1;
            if (ImGui::MenuItem("Top-left",     NULL, corner == 0)) corner = 0;
            if (ImGui::MenuItem("Top-right",    NULL, corner == 1)) corner = 1;
            if (ImGui::MenuItem("Bottom-left",  NULL, corner == 2)) corner = 2;
            if (ImGui::MenuItem("Bottom-right", NULL, corner == 3)) corner = 3;
            if (p_open && ImGui::MenuItem("Close")) *p_open = false;
            ImGui::EndPopup();
        }
    }
    ImGui::End();
}

void MainWindow::ToggleLogs() 
{
    show_logs_window = !show_logs_window;
}

void MainWindow::StartScreenshot()
{
    screenshot_step = 1;
}

static void ShowAboutOpengl(bool* p_open)
{
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
    ImGuiToolkit::ButtonOpenWebpage("https://www.opengl.org");
    ImGui::SameLine();

//    static std::string allextensions( glGetString(GL_EXTENSIONS) );

    static bool show_opengl_info = false;
    ImGuiToolkit::ButtonIconToggle(10,0,13,14,&show_opengl_info);
    ImGui::SameLine(); ImGui::Text("Details");
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



static void ShowAboutGStreamer(bool* p_open)
{
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
    ImGuiToolkit::ButtonOpenWebpage("https://gstreamer.freedesktop.org/");
    ImGui::SameLine();

    static bool show_config_info = false;
    ImGuiToolkit::ButtonIconToggle(10,0,13,14,&show_config_info);
    ImGui::SameLine(); ImGui::Text("Details");
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


void MainWindow::Render()
{
    // We specify a default position/size in case there's no data in the .ini file. Typically this isn't required! We only do it to make the Demo applications a little more welcoming.
    ImGui::SetNextWindowPos(ImVec2(40, 40), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);

    ImGui::Begin(IMGUI_TITLE_MAINWINDOW, NULL, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoCollapse);

    // Menu Bar
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem( ICON_FA_FILE_UPLOAD "  Open", "Ctrl+O")) {
//                UserInterface::manager().OpenFileMedia();
            }
            if (ImGui::MenuItem( ICON_FA_FILE_DOWNLOAD "  Save", "Ctrl+S")) {
//                UserInterface::manager().OpenFileMedia();
            }
            if (ImGui::MenuItem( ICON_FA_FILE "  Close", "Ctrl+W")) {

            }
            if (ImGui::MenuItem( ICON_FA_POWER_OFF " Quit", "Ctrl+Q")) {
                Rendering::manager().Close();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Windows"))
        {
            ImGui::MenuItem( IMGUI_TITLE_PREVIEW, NULL, &Settings::application.preview);
            ImGui::MenuItem( IMGUI_TITLE_MEDIAPLAYER, NULL, &Settings::application.media_player);
            ImGui::MenuItem( IMGUI_TITLE_SHADEREDITOR, NULL, &Settings::application.shader_editor);
            ImGui::MenuItem("Appearance", NULL, false, false);
            ImGui::SetNextItemWidth(200);
            if ( ImGui::SliderFloat("Scale", &Settings::application.scale, 0.8f, 1.2f, "%.1f"))
                ImGui::GetIO().FontGlobalScale = Settings::application.scale;

            ImGui::SetNextItemWidth(200);
            if ( ImGui::Combo("Color", &Settings::application.accent_color, "Blue\0Orange\0Grey\0\0"))
                ImGuiToolkit::SetAccentColor(static_cast<ImGuiToolkit::accent_color>(Settings::application.accent_color));
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Tools"))
        {
            ImGui::MenuItem( ICON_FA_LIST "  Logs", "Ctrl+L", &show_logs_window);
            ImGui::MenuItem( ICON_FA_TACHOMETER_ALT " Metrics", NULL, &show_overlay_stats);
            if ( ImGui::MenuItem( ICON_FA_CAMERA_RETRO "  Screenshot", NULL) )
                StartScreenshot();

            ImGui::MenuItem("About", NULL, false, false);
            ImGui::MenuItem("About ImGui", NULL, &show_app_about);
            ImGui::MenuItem("About OpenGL", NULL, &show_opengl_about);
            ImGui::MenuItem("About GStreamer", NULL, &show_gst_about);

            ImGui::MenuItem("Dev", NULL, false, false);
            ImGui::MenuItem("Icons", NULL, &show_icons_window);
            ImGui::MenuItem("Demo ImGui", NULL, &show_demo_window);

            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // Select view

    bool selected_view[4] = { false, false, false, false };
    selected_view[ Settings::application.current_view ] = true;

//    if ( ImGuiToolkit::ButtonToggle())
    float width = ImGui::GetContentRegionAvail().x / 3;
    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.50f, 0.50f));
    if (ImGui::Selectable( ICON_FA_BULLSEYE " Mixing", &selected_view[1], 0, ImVec2(width,50)))
    {
        Mixer::manager().setCurrentView(View::MIXING);
    }
    ImGui::SameLine();
    if (ImGui::Selectable( ICON_FA_SIGN " Geometry", &selected_view[2], 0, ImVec2(width,50)))
    {
        Mixer::manager().setCurrentView(View::GEOMETRY);
    }
    ImGui::SameLine();
    if (ImGui::Selectable( ICON_FA_BARS " Layers", &selected_view[3], 0, ImVec2(width,50)))
    {

    }
    ImGui::PopStyleVar();




    // TEMPLATE CODE FOR FILE BROWSER
//    if( ImGui::Button( ICON_FA_FOLDER_OPEN " Open File" ) && !loadThread.joinable())
//    {
//        loadThreadDone = false;
//        loadThread = std::thread(NativeOpenFile, "hello");
//    }

//    if( loadThreadDone && loadThread.joinable() ) {
//        loadThread.join();
//        // TODO : deal with filename
//    }

    ImGui::End(); // "v-mix"

    if (show_logs_window)
        Log::ShowLogWindow(&show_logs_window);
    if (show_overlay_stats)
        ShowStats(&show_overlay_stats);

    if (show_icons_window)
        ImGuiToolkit::ShowIconsWindow(&show_icons_window);
    if (show_demo_window)
        ImGui::ShowDemoWindow(&show_demo_window);
    if (show_app_about)
        ImGui::ShowAboutWindow(&show_app_about);
    if (show_gst_about)
        ShowAboutGStreamer(&show_gst_about);
    if (show_opengl_about)
        ShowAboutOpengl(&show_opengl_about);

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
                Rendering::manager().RequestScreenshot();
                screenshot_step = 3;
            break;
            case 3:
            {
                if ( Rendering::manager().CurrentScreenshot()->IsFull() ){
                    std::time_t t = std::time(0);   // get time now
                    std::tm* now = std::localtime(&t);
                    std::string filename =  GstToolkit::date_time_string() + "_vmixcapture.png";
                    Rendering::manager().CurrentScreenshot()->SaveFile( filename.c_str() );
                    Rendering::manager().CurrentScreenshot()->Clear();
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

void UserInterface::RenderPreview()
{
    FrameBuffer *output = Mixer::manager().frame();
    if (output)
    {
        ImGui::SetNextWindowPos(ImVec2(100, 300), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(640, 480), ImGuiCond_FirstUseEver);
        ImGui::Begin(ICON_FA_LAPTOP " Preview", &Settings::application.preview,  ImGuiWindowFlags_NoScrollbar);
        float width = ImGui::GetContentRegionAvail().x;

        ImVec2 imagesize ( width, width / output->aspectRatio());
        ImGui::Image((void*)(intptr_t)output->texture(), imagesize, ImVec2(0.f, 0.f), ImVec2(1.f, -1.f));

        ImGui::End();
    }
}

void UserInterface::RenderMediaPlayer()
{
    bool show = false;
    MediaPlayer *mp = nullptr;
    MediaSource *s = nullptr;
    if ( Mixer::manager().currentSource()) {
        s = static_cast<MediaSource *>(Mixer::manager().currentSource());
        if (s) {
            mp = s->mediaplayer();
            if (mp && mp->isOpen())
                show = true;
        }
    }

    ImGui::SetNextWindowPos(ImVec2(200, 200), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);

    if ( !ImGui::Begin(IMGUI_TITLE_MEDIAPLAYER, &Settings::application.media_player,  ImGuiWindowFlags_NoScrollbar) || !show)
    {
        ImGui::End();
        return;
    }

    float width = ImGui::GetContentRegionAvail().x;
    float spacing = ImGui::GetStyle().ItemInnerSpacing.x;

    ImVec2 imagesize ( width, width / mp->aspectRatio());
    ImGui::Image((void*)(uintptr_t)mp->texture(), imagesize);
    if (ImGui::IsItemHovered()) {
        ImGui::SameLine(-1);
        ImGui::Text("    %s %d x %d\n    Framerate %.2f / %.2f", mp->codec().c_str(), mp->width(), mp->height(), mp->updateFrameRate() , mp->frameRate() );
    }

    if (ImGui::Button(ICON_FA_FAST_BACKWARD))
        mp->rewind();
    ImGui::SameLine(0, spacing);

    // remember playing mode of the GUI
    bool media_playing_mode = mp->isPlaying();

    // display buttons Play/Stop depending on current playing mode
    if (media_playing_mode) {

        if (ImGui::Button(ICON_FA_STOP " Stop"))
            media_playing_mode = false;
        ImGui::SameLine(0, spacing);

        ImGui::PushButtonRepeat(true);
         if (ImGui::Button(ICON_FA_FORWARD))
            mp->fastForward ();
        ImGui::PopButtonRepeat();
    }
    else {

        if (ImGui::Button(ICON_FA_PLAY "  Play"))
            media_playing_mode = true;
        ImGui::SameLine(0, spacing);

        ImGui::PushButtonRepeat(true);
        if (ImGui::Button(ICON_FA_STEP_FORWARD))
            mp->seekNextFrame();
        ImGui::PopButtonRepeat();
    }

    ImGui::SameLine(0, spacing * 4.f);

    static int current_loop = 0;
    static std::vector< std::pair<int, int> > iconsloop = { {0,15}, {1,15}, {19,14} };
    current_loop = (int) mp->loop();
    if ( ImGuiToolkit::ButtonIconMultistate(iconsloop, &current_loop) )
        mp->setLoop( (MediaPlayer::LoopMode) current_loop );

    float speed = static_cast<float>(mp->playSpeed());
    ImGui::SameLine(0, spacing);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 40.0);
    // ImGui::SetNextItemWidth(width - 90.0);
    if (ImGui::DragFloat( "##Speed", &speed, 0.01f, -10.f, 10.f, "Speed x %.1f", 2.f))
        mp->setPlaySpeed( static_cast<double>(speed) );
    ImGui::SameLine(0, spacing);
    if (ImGuiToolkit::ButtonIcon(12, 14)) {
        speed = 1.f;
        mp->setPlaySpeed( static_cast<double>(speed) );
        mp->setLoop( MediaPlayer::LOOP_REWIND );
    }

    guint64 current_t = mp->position();
    guint64 seek_t = current_t;

    bool slider_pressed = ImGuiToolkit::TimelineSlider( "simpletimeline", &seek_t,
                                                        mp->duration(), mp->frameDuration());

    // if the seek target time is different from the current position time
    // (i.e. the difference is less than one frame)
    if ( ABS_DIFF (current_t, seek_t) > mp->frameDuration() ) {

        // request seek (ASYNC)
        mp->seekTo(seek_t);
    }

    // play/stop command should be following the playing mode (buttons)
    // AND force to stop when the slider is pressed
    bool media_play = media_playing_mode & (!slider_pressed);

    // apply play action to media only if status should change
    // NB: The seek command performed an ASYNC state change, but
    // gst_element_get_state called in isPlaying() will wait for the state change to complete.
    if ( mp->isPlaying(true) != media_play ) {
        mp->play( media_play );
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

    ImGui::Begin(IMGUI_TITLE_SHADEREDITOR, &Settings::application.shader_editor, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_MenuBar);
    ImGui::SetWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Edit"))
        {
            bool ro = editor.IsReadOnly();
            if (ImGui::MenuItem("Read-only mode", nullptr, &ro))
                editor.SetReadOnly(ro);
            ImGui::Separator();

            if (ImGui::MenuItem( ICON_FA_UNDO " Undo", "Ctrl-Z", nullptr, !ro && editor.CanUndo()))
                editor.Undo();
            if (ImGui::MenuItem( ICON_FA_REDO " Redo", "Ctrl-Y", nullptr, !ro && editor.CanRedo()))
                editor.Redo();

            ImGui::Separator();

            if (ImGui::MenuItem( ICON_FA_COPY " Copy", "Ctrl-C", nullptr, editor.HasSelection()))
                editor.Copy();
            if (ImGui::MenuItem( ICON_FA_CUT " Cut", "Ctrl-X", nullptr, !ro && editor.HasSelection()))
                editor.Cut();
            if (ImGui::MenuItem( ICON_FA_ERASER " Delete", "Del", nullptr, !ro && editor.HasSelection()))
                editor.Delete();
            if (ImGui::MenuItem( ICON_FA_PASTE " Paste", "Ctrl-V", nullptr, !ro && ImGui::GetClipboardText() != nullptr))
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

