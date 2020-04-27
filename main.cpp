
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


void drawCustomGui()
{

    ImGui::SetNextWindowPos(ImVec2(200, 200), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);

    if ( !ImGui::Begin("Custom"))
    {
        ImGui::End();
        return;
    }


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

        static char buf5[128];
        sprintf ( buf5, "%s", s->name().c_str() );
        ImGui::SetNextItemWidth(-100);
        if (ImGui::InputText("Name", buf5, 64, ImGuiInputTextFlags_CharsNoBlank)){
            s->rename(buf5);
        }

        s->mixingShader()->accept(v);

        float width = ImGui::GetContentRegionAvail().x - 100;
        ImVec2 imagesize ( width, width / s->frame()->aspectRatio());
        ImGui::Image((void*)(uintptr_t) s->frame()->texture(), imagesize);

        s->processingShader()->accept(v);
    }

    ImGui::End();
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

    // custom test window
//    Rendering::manager().PushBackDrawCallback(drawCustomGui);


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
