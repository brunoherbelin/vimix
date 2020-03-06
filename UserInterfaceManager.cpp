#include <iostream>
#include <cstring>
#include <fstream>
#include <sstream>

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

// generic image loader
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#ifdef _WIN32
#  include <windows.h>
#  include <shellapi.h>
#endif

#include "defines.h"
#include "Log.h"
#include "TextEditor.h"
#include "UserInterfaceManager.h"
#include "RenderingManager.h"
#include "ResourceManager.h"
#include "SettingsManager.h"
#include "FileDialog.h"
#include "ImGuiToolkit.h"

MessageBox UserInterface::warnings;
MainWindow UserInterface::mainwindow;
std::string UserInterface::currentFileDialog = "";
std::string UserInterface::currentTextEdit = "";

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


static void OpenWebpage( const char* url )
{
#ifdef _WIN32
    ShellExecuteA( nullptr, nullptr, url, nullptr, nullptr, 0 );
#elif defined __APPLE__
    char buf[1024];
    sprintf( buf, "open %s", url );
    system( buf );
#else
    char buf[1024];
    sprintf( buf, "xdg-open %s", url );
    system( buf );
#endif
}


bool UserInterface::Init()
{
    if (Rendering::window == nullptr)
        return false;

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.MouseDrawCursor = true;

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(Rendering::window, true);
    ImGui_ImplOpenGL3_Init(Rendering::glsl_version.c_str());

    // Setup Dear ImGui style
    ImGuiToolkit::SetAccentColor(static_cast<ImGuiToolkit::accent_color>(Settings::application.color));

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
            Rendering::Close();
        else if (ImGui::IsKeyPressed( GLFW_KEY_O ))
            UserInterface::OpenFileMedia();
        else if (ImGui::IsKeyPressed( GLFW_KEY_S ))
            std::cerr <<" Save File " << std::endl;  
        else if (ImGui::IsKeyPressed( GLFW_KEY_W ))
            std::cerr <<" Close File " << std::endl; 
        else if (ImGui::IsKeyPressed( GLFW_KEY_L ))
            mainwindow.ToggleLogs();

    }

    // Application F-Keys
    if (ImGui::IsKeyPressed( GLFW_KEY_F12 ))
        Rendering::ToggleFullscreen();
    else if (ImGui::IsKeyPressed( GLFW_KEY_F11 ))
        mainwindow.StartScreenshot();
            
}

void UserInterface::handleMouse()
{

    ImGuiIO& io = ImGui::GetIO(); 

    // if not on any window
    if ( !ImGui::IsAnyWindowHovered() )
    {
        // Mouse wheel over background
        if ( io.MouseWheel > 0) {


        }

        if ( ImGui::IsMouseDown(ImGuiMouseButton_Left)) {

        }

        if ( ImGui::IsMouseReleased(ImGuiMouseButton_Left) )
        {

        }

    }
}

void UserInterface::NewFrame()
{
    // deal with keyboard and mouse events
    handleKeyboard();
    handleMouse();

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Main window
    mainwindow.Render();
}


void UserInterface::Render()
{	
    ImVec2 geometry(static_cast<float>(Rendering::render_width), static_cast<float>(Rendering::render_height));

    // warning modal dialog
    warnings.Render( geometry.x * 0.4f ); // 40% width

    // file modal dialog
    geometry.x *= 0.4f;
    geometry.y *= 0.4f;
    if ( !currentFileDialog.empty() && FileDialog::Instance()->Render(currentFileDialog.c_str(), geometry)) {
        if (FileDialog::Instance()->IsOk == true) {
            std::string filePathNameText = FileDialog::Instance()->GetFilepathName();
            // done
            currentFileDialog = "";
        }
        FileDialog::Instance()->CloseDialog(currentFileDialog.c_str());
    }

    // Rendering 
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

void UserInterface::Error(const string& text, const string& title)
{
    // Show NON Graphical dialog (using multiplatform tiny file dialog )
    string t = title.empty() ? APP_TITLE : title;
    tinyfd_messageBox( t.c_str(), text.c_str(), "ok", "error", 0);

}

void UserInterface::Warning(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    ImGuiTextBuffer buf;
    buf.appendfv(fmt, args);
    warnings.messages.push_back(buf.c_str());
    Log::Info(buf.c_str());

    va_end(args);
}

MessageBox::MessageBox() 
{

}

void MessageBox::Render(float width)
{
    if (messages.empty())
        return;    

    ImGui::OpenPopup("Warning");
    if (ImGui::BeginPopupModal("Warning", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGuiToolkit::Icon(9, 4);
        ImGui::SameLine(0, 10);
        ImGui::TextColored(ImVec4(1.0f,0.6f,0.0f,1.0f), "%ld error(s) occured.\n\n", messages.size());

        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + width);
        for (list<string>::iterator it=messages.begin(); it != messages.end(); ++it) {
            ImGui::Text("%s\n", (*it).c_str()); 
            ImGui::Separator();
        }
        ImGui::PopTextWrapPos();

        ImGui::Dummy(ImVec2(width * 0.8f, 0)); ImGui::SameLine(); // right align
        if (ImGui::Button("Ok", ImVec2(width * 0.2f, 0))) { 
            ImGui::CloseCurrentPopup(); 
            // messages have been seen
            messages.clear();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::EndPopup();
    }
}

// template info panel
inline void InfosPane(std::string vFilter, bool *vCantContinue) 
// if vCantContinue is false, the user cant validate the dialog
{
	ImGui::TextColored(ImVec4(0, 1, 1, 1), "Infos Pane");
	
	ImGui::Text("Selected Filter : %s", vFilter.c_str());

    // File Manager information pannel
    bool canValidateDialog = false;
	ImGui::Checkbox("if not checked you cant validate the dialog", &canValidateDialog);

	if (vCantContinue)
	    *vCantContinue = canValidateDialog;
}

inline void TextInfosPane(std::string vFilter, bool *vCantContinue) // if vCantContinue is false, the user cant validate the dialog
{
	ImGui::TextColored(ImVec4(0, 1, 1, 1), "Text");
	
    static std::string filepathcurrent;
    static std::string text;

    // if different file selected
    if ( filepathcurrent.compare(FileDialog::Instance()->GetFilepathName()) != 0)
    {
        filepathcurrent = FileDialog::Instance()->GetFilepathName();	
        
        // fill text with file content
        std::ifstream textfilestream(filepathcurrent, std::ios::in);
        if(textfilestream.is_open()){
            std::stringstream sstr;
            sstr << textfilestream.rdbuf();
            text = sstr.str();
            textfilestream.close();
        } 
        else
        {
            // empty text
            text = "";
        }
        
    }

    // show text
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 340);
    ImGui::Text("%s", text.c_str());

    // release Ok button if text is not empty
	if (vCantContinue)
	    *vCantContinue = text.size() > 0;
}

inline void ImageInfosPane(std::string vFilter, bool *vCantContinue) // if vCantContinue is false, the user cant validate the dialog
{
    // opengl texture
    static GLuint tex = 0;
    static std::string filepathcurrent;
    static std::string message = "Please select an image (" + vFilter + ").";
    static unsigned char* img = NULL;
    static ImVec2 image_size(330, 330);

    // generate texture (once) & clear
    if (tex == 0) {
        glGenTextures(1, &tex);
        glBindTexture( GL_TEXTURE_2D, tex);
        unsigned char clearColor[4] = {0};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, clearColor);
    }

    // if different file selected
    if ( filepathcurrent.compare(FileDialog::Instance()->GetFilepathName()) != 0)
    {
        // remember path
        filepathcurrent = FileDialog::Instance()->GetFilepathName();	

        // prepare texture 
        glBindTexture( GL_TEXTURE_2D, tex);

        // load image
        int w, h, n;
        img = stbi_load(filepathcurrent.c_str(), &w, &h, &n, 4);
        if (img != NULL) {
            
            // apply img to texture
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);

            // adjust image display aspect ratio
            image_size.y =  image_size.x * static_cast<float>(h) / static_cast<float>(w);

            // free loaded image
            stbi_image_free(img);

            message = FileDialog::Instance()->GetCurrentFileName() + "(" + std::to_string(w) + "x" + std::to_string(h) + ")";
        }
        else {
            // clear image
            unsigned char clearColor[4] = {0};
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, clearColor);

            message = "Please select an image (" + vFilter + ").";
        }

    }

    // draw text and image
	ImGui::TextColored(ImVec4(0, 1, 1, 1), "%s", message.c_str());
    ImGui::Image((void*)(intptr_t)tex, image_size);

    // release Ok button if image is not null
	if (vCantContinue)
	    *vCantContinue = img!=NULL;
}

void UserInterface::OpenFileText()
{
    currentFileDialog = "ChooseFileText";

    FileDialog::Instance()->ClearFilterColor();
    FileDialog::Instance()->SetFilterColor(".cpp", ImVec4(1,1,0,0.5));
    FileDialog::Instance()->SetFilterColor(".h", ImVec4(0,1,0,0.5));
    FileDialog::Instance()->SetFilterColor(".hpp", ImVec4(0,0,1,0.5));

    FileDialog::Instance()->OpenDialog(currentFileDialog.c_str(), "Open Text File", ".cpp\0.h\0.hpp\0\0", ".", "", 
            std::bind(&TextInfosPane, std::placeholders::_1, std::placeholders::_2), 350, "Text info");

}

void UserInterface::OpenFileImage()
{
    currentFileDialog = "ChooseFileImage";

    FileDialog::Instance()->ClearFilterColor();
    FileDialog::Instance()->SetFilterColor(".png", ImVec4(0,1,1,1.0));
    FileDialog::Instance()->SetFilterColor(".jpg", ImVec4(0,1,1,1.0));
    FileDialog::Instance()->SetFilterColor(".gif", ImVec4(0,1,1,1.0));

    FileDialog::Instance()->OpenDialog(currentFileDialog.c_str(), "Open Image File", ".*\0.png\0.jpg\0.gif\0\0", ".", "", 
            std::bind(&ImageInfosPane, std::placeholders::_1, std::placeholders::_2), 350, "Image info");

}

void UserInterface::OpenFileMedia()
{
    currentFileDialog = "ChooseFileMedia";

    FileDialog::Instance()->ClearFilterColor();
    FileDialog::Instance()->SetFilterColor(".mp4", ImVec4(0,1,1,1.0));
    FileDialog::Instance()->SetFilterColor(".avi", ImVec4(0,1,1,1.0));
    FileDialog::Instance()->SetFilterColor(".mov", ImVec4(0,1,1,1.0));

    FileDialog::Instance()->OpenDialog(currentFileDialog.c_str(), "Open Media File", ".*\0.mp4\0.avi\0.mov\0\0", ".", "", "Media");

}


MainWindow::MainWindow()
{
    show_overlay_stats = false;
    show_app_about = false;
    show_demo_window = false;
    show_logs_window = false;
    show_icons_window = false;
    show_editor_window = false;
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

void MainWindow::ToggleStats() 
{
    show_overlay_stats = !show_overlay_stats;
}

void MainWindow::StartScreenshot()
{
    screenshot_step = 1;
}

void MainWindow::Render()
{
    // We specify a default position/size in case there's no data in the .ini file. Typically this isn't required! We only do it to make the Demo applications a little more welcoming.
    ImGui::SetNextWindowPos(ImVec2(40, 40), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);


    ImGui::Begin( ICON_FA_DOT_CIRCLE " v-mix", NULL, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoCollapse);  

    // Menu Bar
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem( ICON_FA_FILE "  New")) {

            }
            if (ImGui::MenuItem( ICON_FA_FILE_UPLOAD "  Open", "Ctrl+O")) {
                UserInterface::OpenFileMedia();
            }
            if (ImGui::MenuItem( ICON_FA_POWER_OFF " Quit", "Ctrl+Q")) {
                Rendering::Close();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Appearance"))
        {
            ImGui::SetNextItemWidth(200);
            if ( ImGui::SliderFloat("Scale", &Settings::application.scale, 0.8f, 1.2f, "%.1f"))
                ImGui::GetIO().FontGlobalScale = Settings::application.scale;

            ImGui::SetNextItemWidth(200);
            if ( ImGui::Combo("Color", &Settings::application.color, "Blue\0Orange\0Grey\0\0"))
                ImGuiToolkit::SetAccentColor(static_cast<ImGuiToolkit::accent_color>(Settings::application.color));
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Tools"))
        {
            ImGui::MenuItem( ICON_FA_CODE " Shader Editor", NULL, &show_editor_window);
            ImGui::MenuItem( ICON_FA_LIST "  Logs", "Ctrl+L", &show_logs_window);
            ImGui::MenuItem( ICON_FA_TACHOMETER_ALT " Metrics", NULL, &show_overlay_stats);
            if ( ImGui::MenuItem( ICON_FA_CAMERA_RETRO "  Screenshot", NULL) )
                StartScreenshot();

            ImGui::MenuItem("--", NULL, false, false);
            ImGui::MenuItem("Icons", NULL, &show_icons_window);
            ImGui::MenuItem("Demo ImGui", NULL, &show_demo_window);
            ImGui::MenuItem("About ImGui", NULL, &show_app_about);
            if ( ImGui::MenuItem("About Gstreamer", NULL) )
                OpenWebpage("https://gstreamer.freedesktop.org/");
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // content
    ImGuiToolkit::Icon(7, 1);
    ImGui::SameLine(0, 10);

    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_ITALIC);
    ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
    ImGui::PopFont();

    if( ImGui::Button( ICON_FA_FOLDER_OPEN " Open File" ) && !loadThread.joinable())
    {        
        loadThreadDone = false;
        loadThread = std::thread(NativeOpenFile, "hello");
    }

    if( loadThreadDone && loadThread.joinable() ) { 
        loadThread.join(); 
        
        // TODO : deal with filename    
    }

    ImGui::End(); // "v-mix"

    if (show_logs_window)
        Log::ShowLogWindow(&show_logs_window);
    if (show_overlay_stats)
        ShowStats(&show_overlay_stats);
    if (show_editor_window)
        drawTextEditor(&show_editor_window);
    if (show_icons_window)
        ImGuiToolkit::ShowIconsWindow(&show_icons_window);
    if (show_demo_window)
        ImGui::ShowDemoWindow(&show_demo_window);
    if (show_app_about)               
        ImGui::ShowAboutWindow(&show_app_about); 

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
                Rendering::RequestScreenshot();
                screenshot_step = 3;
            break;
            case 3:
            {
                if ( Rendering::CurrentScreenshot()->IsFull() ){
                    std::time_t t = std::time(0);   // get time now
                    std::tm* now = std::localtime(&t);
                    std::string filename =  ImGuiToolkit::DateTime() + "_vmixcapture.png";
                    Rendering::CurrentScreenshot()->SaveFile( filename.c_str() );
                    Rendering::CurrentScreenshot()->Clear();
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

void UserInterface::OpenTextEditor(std::string text)
{
    currentTextEdit = text;

    auto lang = TextEditor::LanguageDefinition::GLSL();
	editor.SetLanguageDefinition(lang);

    editor.SetText(currentTextEdit);
}

void MainWindow::drawTextEditor(bool* p_open)
{
    static bool show_statusbar = true;
    ImGui::Begin( ICON_FA_CODE " Shader Editor", p_open, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_MenuBar);
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
    editor.Render("TextEditor");
    ImGui::PopFont();

    ImGui::End();

}

