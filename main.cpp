
#include <stdio.h>
#include <iostream>

// standalone image loader
#include "stb_image.h"

// Opengl
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>

//  GStreamer
#include <gst/gst.h>
#include <gst/gstbin.h>
#include <gst/gl/gl.h>

// vmix
#include "defines.h"
#include "ImageShader.h"
#include "Settings.h"
#include "Resource.h"
#include "FrameBuffer.h"
#include "RenderingManager.h"
#include "UserInterfaceManager.h"

#include "imgui.h"
#include "ImGuiToolkit.h"
#include "GstToolkit.h"
#include "MediaPlayer.h"
#include "Scene.h"
#include "Primitives.h"
#include "Mesh.h"
#include "ImGuiVisitor.h"
#include "SessionVisitor.h"

#define PI 3.14159265358979323846

// test scene

////    ("file:///home/bhbn/Videos/MOV001.MOD");
////    ("file:///home/bhbn/Videos/TestFormats/Commodore64GameReviewMoondust.flv");
////    ("file:///home/bhbn/Videos/fish.mp4");
////    ("file:///home/bhbn/Videos/jean/Solitude1080p.mov");
////    ("file:///home/bhbn/Videos/TearsOfSteel_720p_h265.mkv");
////    ("file:///home/bhbn/Videos/TestFormats/_h264GoldenLamps.mkv");
////    ("file:///home/bhbn/Videos/TestEncoding/vpxvp9high.webm");
////    ("file:///home/bhbn/Videos/iss.mov");
////    ("file:///home/bhbn/Videos//iss.mov");
////    ("file:///Users/Herbelin/Movies/mp2test.mpg");
///
Scene scene;
FrameBuffer *output;
MediaSurface testnode1("file:///home/bhbn/Videos/iss.mov");
MediaSurface testnode2("file:///home/bhbn/Videos/fish.mp4");
ImageSurface testnode3("images/seed_512.jpg");


void drawMediaPlayer()
{
    static GstClockTime begin = GST_CLOCK_TIME_NONE;
    static GstClockTime end = GST_CLOCK_TIME_NONE;

    if ( !testnode2.getMediaPlayer() || !testnode2.getMediaPlayer()->isOpen() )
        return;

    MediaPlayer *mp = testnode2.getMediaPlayer();

    ImGui::SetNextWindowPos(ImVec2(200, 200), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);
    ImGui::Begin(IMGUI_TITLE_MEDIAPLAYER);
    float width = ImGui::GetContentRegionAvail().x;
    float spacing = ImGui::GetStyle().ItemInnerSpacing.x;

    ImVec2 imagesize ( width, width / mp->aspectRatio());
    ImGui::Image((void*)(intptr_t)mp->texture(), imagesize);

    if (ImGui::Button(ICON_FA_FAST_BACKWARD))
        mp->rewind();
    ImGui::SameLine(0, spacing);

    // remember playing mode of the GUI
    static bool media_playing_mode = mp->isPlaying();

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

    ImGui::SameLine(0, spacing * 2.f);
    ImGui::Dummy(ImVec2(width - 700.0, 0));  // right align

    static int current_loop = 1;
    static const char* loop_names[3] = { "Stop", "Rewind", "Bounce" };
    const char* current_loop_name = loop_names[current_loop];
    ImGui::SameLine(0, spacing);
    if (current_loop == 0)  ImGuiToolkit::Icon(0, 15);
    else if (current_loop == 1)  ImGuiToolkit::Icon(1, 15);
    else ImGuiToolkit::Icon(19, 14);
    ImGui::SameLine(0, spacing);
    ImGui::SetNextItemWidth(90);
    if ( ImGui::SliderInt("", &current_loop, 0, 2, current_loop_name) )
        mp->setLoop( (MediaPlayer::LoopMode) current_loop );

    float speed = static_cast<float>(mp->playSpeed());
    ImGui::SameLine(0, spacing);
    ImGui::SetNextItemWidth(270);
    // ImGui::SetNextItemWidth(width - 90.0);
    if (ImGui::SliderFloat( "Speed", &speed, -10.f, 10.f, "x %.1f", 2.f))
        mp->setPlaySpeed( static_cast<double>(speed) );
    ImGui::SameLine(0, spacing);
    if (ImGuiToolkit::ButtonIcon(19, 15)) {
        speed = 1.f;
        mp->setPlaySpeed( static_cast<double>(speed) );
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
    if ( mp->isPlaying() != media_play ) {
        mp->play( media_play );
    }

    // display info
    ImGui::Text("%s %d x %d", mp->codec().c_str(), mp->width(), mp->height());
    ImGui::Text("Framerate %.2f / %.2f", mp->updateFrameRate() , mp->frameRate() );

    ImGui::End();
}


void drawScene()
{
    // compute dt
    static gint64 last_time = gst_util_get_timestamp ();
    gint64 current_time = gst_util_get_timestamp ();
    gint64 dt = current_time - last_time;
    last_time = current_time;

    // recursive update from root of scene
    scene.root_.update( static_cast<float>( GST_TIME_AS_MSECONDS(dt)) * 0.001f );

    // draw in output frame buffer
    glm::mat4 MV = glm::mat4(1.f);
    glm::mat4 P  = glm::scale( glm::ortho(-5.f, 5.f, -5.f, 5.f), glm::vec3(1.f, output->aspectRatio(), 1.f));
    output->begin();
    scene.root_.draw(MV, P);
    output->end();

    // draw in main view
    scene.root_.draw(MV, Rendering::manager().Projection());

    // draw GUI tree scene
    ImGui::Begin(IMGUI_TITLE_MAINWINDOW);
    static ImGuiVisitor v;
    scene.accept(v);
    ImGui::End();
}


void drawPreview()
{
    if (output)
    {
        ImGui::SetNextWindowPos(ImVec2(100, 300), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(640, 480), ImGuiCond_FirstUseEver);
        ImGui::Begin(ICON_FA_LAPTOP " Preview");
        float width = ImGui::GetContentRegionAvail().x;

        ImVec2 imagesize ( width, width / output->aspectRatio());
        ImGui::Image((void*)(intptr_t)output->texture(), imagesize, ImVec2(0.f, 0.f), ImVec2(1.f, -1.f));

        ImGui::End();
    }


}


int main(int, char**)
{
    ///
    /// Settings
    ///
    Settings::Load();

    ///
    /// RENDERING INIT
    ///
    if ( !Rendering::manager().Init() )
        return 1;

    ///
    /// UI INIT
    ///
    if ( !UserInterface::manager().Init() )
        return 1;

    ///
    /// GStreamer
    ///
#ifndef NDEBUG
    gst_debug_set_default_threshold (GST_LEVEL_WARNING);
    gst_debug_set_active(TRUE);
#endif

//     test text editor
//    UserInterface::manager().OpenTextEditor( Resource::getText("shaders/texture-shader.fs") );

    // init the scene
    Rendering::manager().PushFrontDrawCallback(drawScene);

    // init elements to the scene
    //testnode3.getShader()->blending = Shader::BLEND_OPACITY;

    Mesh disk("mesh/disk.ply", "images/transparencygrid.png");

    glm::vec4 pink( 0.8f, 0.f, 0.8f, 1.f );
    LineCircle circle(pink, 5);

    glm::vec4 color( 0.8f, 0.8f, 0.f, 1.f);
    LineSquare border(color, 5);
    Mesh shadow("mesh/shadow.ply", "mesh/shadow.png");

    Group g1;
    g1.translation_ = glm::vec3(1.f, 1.f, 0.2f);
    g1.scale_ = glm::vec3(0.8f, 0.8f, 1.f);

    Group g2;
    g2.translation_ = glm::vec3(-1.f, -1.f, 0.4f);

    Animation A;
    A.speed_ = 0.05f;
    A.axis_ = glm::vec3(1.f, 1.f, 1.f);

//    std::vector<glm::vec3> pts = std::vector<glm::vec3> { glm::vec3( 0.f, 0.f, 0.f ) };
//    Points P(pts, pink);
//    P.setPointSize(60);
    Mesh P("mesh/target.ply");
    P.scale_ = glm::vec3(0.15f);

    Mesh meshicon("mesh/icon_video.ply");

    // build tree
    scene.root_.addChild(&disk);
    scene.root_.addChild(&circle);

    g1.addChild(&testnode3);
    g1.addChild(&border);
    g1.addChild(&shadow);
    scene.root_.addChild(&g1);

    g2.addChild(&testnode1);
    g2.addChild(&border);
    g2.addChild(&shadow);
    g2.addChild(&meshicon);
    scene.root_.addChild(&g2);

    A.addChild(&P);
    scene.root_.addChild(&A);

    // init output FBO
    output = new FrameBuffer(1280, 720);
    Rendering::manager().PushBackDrawCallback(drawPreview);

    // add media player
    Rendering::manager().PushBackDrawCallback(drawMediaPlayer);

    ///
    /// Main LOOP
    ///
    while ( Rendering::manager().isActive() )
    {
        Rendering::manager().Draw();
    }


        SessionVisitor savetoxml;
        scene.accept(savetoxml);
        savetoxml.save("/home/bhbn/test.vmx");

    UserInterface::manager().Terminate();
    Rendering::manager().Terminate();

    Settings::Save();

    return 0;
}
