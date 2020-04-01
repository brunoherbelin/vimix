
#include <stdio.h>
#include <iostream>

// standalone image loader
#include "stb_image.h"

// Opengl
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/ext/matrix_transform.hpp>

//  GStreamer
#include <gst/gst.h>
#include <gst/gstbin.h>
#include <gst/gl/gl.h>

// vmix
#include "defines.h"
#include "ImageShader.h"
#include "Settings.h"
#include "Resource.h"
#include "RenderingManager.h"
#include "UserInterfaceManager.h"

#include "imgui.h"
#include "ImGuiToolkit.h"
#include "GstToolkit.h"
#include "MediaPlayer.h"
#include "Scene.h"
#include "Primitives.h"
#include "SessionVisitor.h"

#define PI 3.14159265358979323846

Scene scene;
MediaPlayer testmedia;
MediaPlayer testmedia2("testmedia2");
ImageShader rendering_shader;
unsigned int vbo, vao, ebo;
float texturear = 1.0;
GLuint textureimagepng = 0;


// from https://gstreamer.freedesktop.org/documentation/application-development/advanced/pipeline-manipulation.html?gi-language=c

void drawMediaBackgound()
{
    if ( !testmedia2.isOpen() )
        return;

    //
    // RENDER SOURCE
    //
    testmedia2.Update();

    // use the shader
    rendering_shader.projection = Rendering::manager().Projection();
    rendering_shader.modelview = glm::identity<glm::mat4>();;
    rendering_shader.use();
    // use the media
    testmedia2.Bind();
    // draw the vertex array
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    //
    // GUI to control SOURCE
    //
    {
        ImGui::Begin("Image properties");

        // MODEL VIEW
        static float translation[] = {0.f, 0.f};
        if (ImGuiToolkit::ButtonIcon(6, 15)) {
            translation[0] = 0.f;
            translation[1] = 0.f;
        }
        ImGui::SameLine(0, 10);
        ImGui::SliderFloat2("position", translation, -5.0, 5.0);

        static float rotation = 0.f;
        if (ImGuiToolkit::ButtonIcon(4, 15)) rotation = 0.f;
        ImGui::SameLine(0, 10); 
        ImGui::SliderFloat("rotation", &rotation, 0, 2 * PI);

        static float scale = 1.f;
        if (ImGuiToolkit::ButtonIcon(3, 15))  scale = 1.f;
        ImGui::SameLine(0, 10);
        ImGui::SliderFloat("scale", &scale, 0.1f, 10.f, "%.3f", 3.f);

        // set modelview
//        rendering_shader.setModelview(translation[0], translation[1], rotation, scale, testmedia2.AspectRatio());

        // color picker
        if (ImGuiToolkit::ButtonIcon(16, 8))  rendering_shader.color = glm::vec4(1.f);
        ImGui::SameLine(0, 10);
        ImGui::ColorEdit3("color", glm::value_ptr(rendering_shader.color) ) ;
        // brightness slider
        if (ImGuiToolkit::ButtonIcon(4, 1)) rendering_shader.brightness = 0.f;
        ImGui::SameLine(0, 10);
        ImGui::SliderFloat("brightness", &rendering_shader.brightness, -1.0, 1.0, "%.3f", 2.f);
        // contrast slider
        if (ImGuiToolkit::ButtonIcon(2, 1)) rendering_shader.contrast = 0.f;
        ImGui::SameLine(0, 10);
        ImGui::SliderFloat("contrast", &rendering_shader.contrast, -1.0, 1.0, "%.3f", 2.f);

        ImGui::End();
    }

}

void drawMediaPlayer()
{
    static GstClockTime begin = GST_CLOCK_TIME_NONE;
    static GstClockTime end = GST_CLOCK_TIME_NONE;

    if ( !testmedia.isOpen() )
        return;

    testmedia.Update();

    if ( begin == GST_CLOCK_TIME_NONE) {

        begin = testmedia.Duration() / 5;
        end = testmedia.Duration() / 3;
//        if ( testmedia.addPlaySegment(begin, end) )
//            std::cerr << " - first segment " << std::endl;

//        begin *= 2;
//        end *= 2;
//        if ( testmedia.addPlaySegment(begin, end) )
//            std::cerr << " - second segment " << std::endl;

    }

    ImGui::Begin("Media Player");
    float width = ImGui::GetContentRegionAvail().x;
    float spacing = ImGui::GetStyle().ItemInnerSpacing.x;

    ImVec2 imagesize ( width, width / testmedia.AspectRatio());
    ImGui::Image((void*)(intptr_t)testmedia.Texture(), imagesize);

    if (ImGui::Button(ICON_FA_FAST_BACKWARD))
        testmedia.Rewind();
    ImGui::SameLine(0, spacing);

    // remember playing mode of the GUI
    static bool media_playing_mode = testmedia.isPlaying();

    // display buttons Play/Stop depending on current playing mode
    if (media_playing_mode) {

        if (ImGui::Button(ICON_FA_STOP " Stop"))
            media_playing_mode = false;
        ImGui::SameLine(0, spacing);

        ImGui::PushButtonRepeat(true);
         if (ImGui::Button(ICON_FA_FORWARD))
            testmedia.FastForward ();
        ImGui::PopButtonRepeat();
    }
    else {

        if (ImGui::Button(ICON_FA_PLAY "  Play"))
            media_playing_mode = true;
        ImGui::SameLine(0, spacing);

        ImGui::PushButtonRepeat(true);
        if (ImGui::Button(ICON_FA_STEP_FORWARD))
            testmedia.SeekNextFrame();
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
        testmedia.setLoop( (MediaPlayer::LoopMode) current_loop );

    float speed = static_cast<float>(testmedia.PlaySpeed());
    ImGui::SameLine(0, spacing);
    ImGui::SetNextItemWidth(270);
    // ImGui::SetNextItemWidth(width - 90.0);
    if (ImGui::SliderFloat( "Speed", &speed, -10.f, 10.f, "x %.1f", 2.f))
        testmedia.SetPlaySpeed( static_cast<double>(speed) );
    ImGui::SameLine(0, spacing);
    if (ImGuiToolkit::ButtonIcon(19, 15)) {
        speed = 1.f;
        testmedia.SetPlaySpeed( static_cast<double>(speed) );
    }

    guint64 current_t = testmedia.Position();
    guint64 seek_t = current_t;

    bool slider_pressed = ImGuiToolkit::TimelineSlider( "simpletimeline", &seek_t,
                                                        testmedia.Duration(), testmedia.FrameDuration());

//    std::list<std::pair<guint64, guint64> > segments = testmedia.getPlaySegments();
//    bool slider_pressed = ImGuiToolkit::TimelineSliderEdit( "timeline", &seek_t, testmedia.Duration(),
//                                                        testmedia.FrameDuration(), segments);

//    if (!segments.empty()) {
//        // segments have been modified
//        g_print("Segments modified \n");

//        for (std::list< std::pair<guint64, guint64> >::iterator s = segments.begin(); s != segments.end(); s++){
//            MediaSegment newsegment(s->first, s->second);
//            testmedia.removeAllPlaySegmentOverlap(newsegment);
//            if (!testmedia.addPlaySegment(newsegment))
//                g_print("new Segment could not be added \n");
//        }
//    }

    // if the seek target time is different from the current position time
    // (i.e. the difference is less than one frame)
    if ( ABS_DIFF (current_t, seek_t) > testmedia.FrameDuration() ) {

        // request seek (ASYNC)
        testmedia.SeekTo(seek_t);
    }

    // play/stop command should be following the playing mode (buttons)
    // AND force to stop when the slider is pressed
    bool media_play = media_playing_mode & (!slider_pressed);

    // apply play action to media only if status should change
    // NB: The seek command performed an ASYNC state change, but
    // gst_element_get_state called in isPlaying() will wait for the state change to complete.
    if ( testmedia.isPlaying() != media_play ) {
        testmedia.Play( media_play );
    }

    // display info
    ImGui::Text("%s %d x %d", testmedia.Codec().c_str(), testmedia.Width(), testmedia.Height());
    ImGui::Text("Framerate %.2f / %.2f", testmedia.UpdateFrameRate() , testmedia.FrameRate() );

    ImGui::End();
}


void drawScene()
{
    static gint64 last_time = gst_util_get_timestamp ();
    gint64 current_time = gst_util_get_timestamp ();
    gint64 dt = current_time - last_time;

    glm::mat4 mv = glm::identity<glm::mat4>();
//    glm::mat4 View = glm::translate(glm::identity<glm::mat4>(), glm::vec3(0, 0, 0.f));
//    View = glm::rotate(View, 0.f, glm::vec3(0.0f, 0.0f, 1.0f));
//    glm::mat4 Model = glm::scale(glm::identity<glm::mat4>(), glm::vec3(1, 1, 1));
//    mv = View * Model;

    scene.root_.update( static_cast<float>( GST_TIME_AS_MSECONDS(dt)) * 0.001f );
    scene.root_.draw(mv, Rendering::manager().Projection());

    last_time = current_time;
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

////     testmedia.Open("file:///home/bhbn/Videos/MOV001.MOD");
////     testmedia.Open("file:///home/bhbn/Videos/TestFormats/Commodore64GameReviewMoondust.flv");
////     testmedia.Open("file:///home/bhbn/Videos/fish.mp4");
////    testmedia.Open("file:///home/bhbn/Videos/jean/Solitude1080p.mov");
//    testmedia.Open("file:///home/bhbn/Videos/TearsOfSteel_720p_h265.mkv");
////    testmedia.Open("file:///home/bhbn/Videos/TestFormats/_h264GoldenLamps.mkv");
////    testmedia.Open("file:///home/bhbn/Videos/TestEncoding/vpxvp9high.webm");
////    testmedia.Open("file:///home/bhbn/Videos/iss.mov");
//    testmedia.Play(false);

    // Add draw callbacks to the Rendering
//    Rendering::manager().AddDrawCallback(drawMediaPlayer);

////    testmedia2.Open("file:///home/bhbn/Images/Butterfly.gif");
////    testmedia2.Open("file:///home/bhbn/Images/Scan-090614-0022.jpg");
////    testmedia2.Open("file:///home/bhbn/Images/svg/abstract.svg");
//    testmedia2.Open("file:///home/bhbn/Videos/iss.mov");
////    testmedia2.Open("file:///home/bhbn/Images/4k/colors-3840x2160-splash-4k-18458.jpg");
////    testmedia2.Open("file:///home/bhbn/Videos/Upgrade.2018.720p.AMZN.WEB-DL.DDP5.1.H.264-NTG.m4v");
//    testmedia2.Play(true);
//    // create our geometries
//    create_square_glm(vbo, vao, ebo);
//    // Add draw callbacks to the Rendering
//    Rendering::manager().AddDrawCallback(drawMediaBackgound);

//     test text editor
//    UserInterface::manager().OpenTextEditor( Resource::getText("shaders/texture-shader.fs") );

    // init the scene
    scene.root_.init();
    Rendering::manager().AddDrawCallback(drawScene);

    // create and add a elements to the scene

    MediaRectangle testnode1("file:///home/bhbn/Videos/iss.mov");
    testnode1.init();

    MediaRectangle testnode2("file:///home/bhbn/Videos/fish.mp4");
    testnode2.init();

    TexturedRectangle testnode3("images/v-mix_256x256.png");
    testnode3.init();
    testnode3.getShader()->blending = Shader::BLEND_SUBSTRACT;

    std::vector<glm::vec3> points;
    points.push_back( glm::vec3( -1.f, -1.f, 0.f ) );
    points.push_back( glm::vec3( -1.f, 1.f, 0.f ) );
    points.push_back( glm::vec3( 1.f, 1.f, 0.f ) );
    points.push_back( glm::vec3( 1.f, -1.f, 0.f ) );
    glm::vec3 color( 1.f, 1.f, 0.f );
    LineStrip border(points, color);
    border.init();

    Group g1;
    g1.init();
    g1.transform_ = glm::translate(glm::identity<glm::mat4>(), glm::vec3(1.f, 1.f, 0.f));

    Switch g2;
    g2.init();
    g2.transform_ = glm::translate(glm::identity<glm::mat4>(), glm::vec3(-1.f, -1.f, 0.f));


    // build tree
    g1.addChild(&testnode1);
    g1.addChild(&testnode3);
    scene.root_.addChild(&g1);
    g2.addChild(&testnode2);
    g2.addChild(&border);
    g2.setActiveIndex(1);
    scene.root_.addChild(&g2);

    ///
    /// Main LOOP
    ///
    while ( Rendering::manager().isActive() )
    {
        Rendering::manager().Draw();
    }

    testmedia.Close();
    testmedia2.Close();

    SessionVisitor savetoxml("/home/bhbn/test.vmx");
    scene.accept(savetoxml);

    UserInterface::manager().Terminate();
    Rendering::manager().Terminate();

    Settings::Save();

    return 0;
}
