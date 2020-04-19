
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
#include "GstToolkit.h"

// imgui
#include "imgui.h"
#include "ImGuiToolkit.h"
#include "ImGuiVisitor.h"

// vmix
#include "defines.h"
#include "Settings.h"

// mixing
#include "Mixer.h"
#include "Source.h"
#include "RenderingManager.h"
#include "UserInterfaceManager.h"
#include "FrameBuffer.h"

#include "MediaPlayer.h"
#include "Scene.h"
#include "Primitives.h"
#include "Mesh.h"
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



void drawMediaPlayer()
{
    static GstClockTime begin = GST_CLOCK_TIME_NONE;
    static GstClockTime end = GST_CLOCK_TIME_NONE;

    if ( !Mixer::manager().currentSource())
        return;

    MediaSource *s = static_cast<MediaSource *>(Mixer::manager().currentSource());
    if ( !s )
        return;

    MediaPlayer *mp = s->mediaplayer();
    if ( !mp || !mp->isOpen() )
        return;

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
    Mixer::manager().update();

    Mixer::manager().draw();

    // draw GUI tree scene
    ImGui::Begin(IMGUI_TITLE_MAINWINDOW);
    static ImGuiVisitor v;
//    Mixer::manager().currentView()->scene.accept(v);
    Mixer::manager().getView(View::RENDERING)->scene.accept(v);
    ImGui::End();
}


void drawPreview()
{
    FrameBuffer *output = Mixer::manager().frame();
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
    Mixer::manager().setCurrentView(View::MIXING);
    Rendering::manager().PushFrontDrawCallback(drawScene);

    // init elements to the scene
    //testnode3.getShader()->blending = Shader::BLEND_OPACITY;

    Mixer::manager().createSourceMedia("file:///home/bhbn/Videos/iss.mov");
    Mixer::manager().createSourceMedia("file:///home/bhbn/Videos/fish.mp4");


//    Animation A;
//    A.translation_ = glm::vec3(0.f, 0.f, 3.f);
//    A.speed_ = 0.1f;
//    A.axis_ = glm::vec3(1.f, 1.f, 1.f);
//    Mesh P("mesh/point.ply");
//    P.scale_ = glm::vec3(0.15f);
//    A.addChild(&P);
//    Mixer::manager().currentView()->scene.root()->addChild(&A);

    // preview window
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


    UserInterface::manager().Terminate();
    Rendering::manager().Terminate();

    Settings::Save();

    return 0;
}
