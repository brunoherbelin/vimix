
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
#include "ImageProcessingShader.h"

#include "MediaPlayer.h"
#include "Scene.h"
#include "Primitives.h"
#include "Mesh.h"
#include "SessionVisitor.h"

#define PI 3.14159265358979323846


void drawMediaPlayer()
{
    static bool opened = true;
    static GstClockTime begin = GST_CLOCK_TIME_NONE;
    static GstClockTime end = GST_CLOCK_TIME_NONE;

    bool show = false;
    MediaPlayer *mp = nullptr;
    if ( Mixer::manager().currentSource()) {
        MediaSource *s = static_cast<MediaSource *>(Mixer::manager().currentSource());
        if (s) {
            mp = s->mediaplayer();
            if (mp && mp->isOpen())
                show = true;
        }
    }

    ImGui::SetNextWindowPos(ImVec2(200, 200), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);

    if ( !ImGui::Begin(IMGUI_TITLE_MEDIAPLAYER, &opened) || !show)
    {
        ImGui::End();
        return;
    }

    float width = ImGui::GetContentRegionAvail().x;
    float spacing = ImGui::GetStyle().ItemInnerSpacing.x;

    ImVec2 imagesize ( width, width / mp->aspectRatio());
    ImGui::Image((void*)(intptr_t)mp->texture(), imagesize);

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
//    ImGui::Dummy(ImVec2(width - 700.0, 0));  // right align

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
    if ( mp->isPlaying() != media_play ) {
        mp->play( media_play );
    }

    // display info
//    ImGui::Text("%s %d x %d", mp->codec().c_str(), mp->width(), mp->height());
//    ImGui::Text("Framerate %.2f / %.2f", mp->updateFrameRate() , mp->frameRate() );

    ImGui::End();
}


void drawScene()
{
//    Mixer::manager().update();

    Mixer::manager().draw();

    // draw GUI tree scene
    ImGui::Begin(IMGUI_TITLE_MAINWINDOW);
    static ImGuiVisitor v;
//    Mixer::manager().currentView()->scene.accept(v);
//    Mixer::manager().getView(View::RENDERING)->scene.accept(v);

    Source *s = Mixer::manager().currentSource();
    if ( s != nullptr) {
        s->shader()->accept(v);
    }

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
        Mixer::manager().update();

        Rendering::manager().Draw();
    }


    UserInterface::manager().Terminate();
    Rendering::manager().Terminate();

    Settings::Save();

    return 0;
}
