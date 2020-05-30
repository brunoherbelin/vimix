
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
#include "Settings.h"
#include "Mixer.h"
#include "RenderingManager.h"
#include "UserInterfaceManager.h"


void drawScene()
{
    Mixer::manager().draw();
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
    if ( !Rendering::manager().init() )
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
#else
    gst_debug_set_default_threshold (GST_LEVEL_ERROR);
    gst_debug_set_active(FALSE);
#endif

//     test text editor
//    UserInterface::manager().fillShaderEditor( Resource::getText("shaders/image.fs") );

    // draw the scene
    Rendering::manager().pushFrontDrawCallback(drawScene);

    ///
    /// Main LOOP
    ///
    while ( Rendering::manager().isActive() )
    {
        Mixer::manager().update();

        Rendering::manager().draw();
    }

    ///
    /// UI TERMINATE
    ///
    UserInterface::manager().Terminate();

    ///
    /// RENDERING TERMINATE
    ///
    Rendering::manager().terminate();

    ///
    /// Settings
    ///
    Settings::Save();

    /// ok
    return 0;
}
