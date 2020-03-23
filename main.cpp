
#include <stdio.h>
#include <iostream>

// standalone image loader
#include "stb_image.h"

// Opengl
#include <glad/glad.h>
#include <glm/glm.hpp> 

//  GStreamer
#include <gst/gst.h>
#include <gst/gstbin.h>
#include <gst/gl/gl.h>

// vmix
#include "defines.h"
#include "Shader.h"
#include "Settings.h"
#include "Resource.h"
#include "RenderingManager.h"
#include "UserInterfaceManager.h"

#include "imgui.h"
#include "ImGuiToolkit.h"
#include "GstToolkit.h"
#include "MediaPlayer.h"

#define PI 3.14159265358979323846

MediaPlayer testmedia;
MediaPlayer testmedia2("testmedia2");
Shader rendering_shader;
unsigned int vbo, vao, ebo;
float texturear = 1.0;
GLuint textureimagepng = 0;

void create_square(unsigned int &vbo, unsigned int &vao, unsigned int &ebo)
{

	// create the triangle
	float _vertices[] = {
		-1.f, -1.f, 0.0f, // position vertex 3
		0.0f, 1.0f, 0.0f,	 // uv vertex 3
		-1.f, 1.f, 0.0f,	// position vertex 0
		0.0f, 0.0f, 0.0f,	 // uv vertex 0
		1.f, -1.f, 0.0f,  // position vertex 2
		1.0f, 1.0f, 0.0f,	 // uv vertex 2
		1.f, 1.f, 0.0f,	// position vertex 1
		1.0f, 0.0f, 0.0f,	 // uv vertex 1
	};
	unsigned int _indices[] = { 0, 1, 2, 3 };
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glGenBuffers(1, &ebo);
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(_vertices), _vertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(_indices), _indices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

}

GLuint loadPNG(const char *imagepath, float *aspectrato)
{
    int w, h, n, image;
    /* FIXME: remove once the example supports gl3 and/or gles2 */
    g_setenv ("GST_GL_API", "opengl", FALSE);
	unsigned char* img;
    *aspectrato = 1.f;

	img = stbi_load(imagepath, &w, &h, &n, 3);
	if (img == NULL) {
		printf("Failed to load png - %s\n", stbi_failure_reason());
	 return 0;
	}
    GLuint tex;
    glGenTextures(1, &tex);

	glBindTexture( GL_TEXTURE_2D, tex);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 3);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    //glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, img);

	stbi_image_free(img);
	glBindTexture( GL_TEXTURE_2D, 0);

    *aspectrato = static_cast<float>(w) / static_cast<float>(h);
    return tex;
}

// from https://gstreamer.freedesktop.org/documentation/application-development/advanced/pipeline-manipulation.html?gi-language=c

void drawMediaBackgound()
{
    if ( !testmedia2.isOpen() )
        return;

    // rendering TRIANGLE geometries
    rendering_shader.use();
    rendering_shader.setUniform("render_projection", Rendering::manager().Projection());
    
    testmedia2.Update();
    testmedia2.Bind();

    // rendering_shader.setUniform("aspectratio", texturear);
    // glBindTexture(GL_TEXTURE_2D, textureimagepng);

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D, 0);

    // render TRIANGLE GUI
    {
        ImGui::Begin("Image properties");

        static float translation[] = {0.f, 0.f};
        if (ImGuiToolkit::ButtonIcon(6, 15)) {
            translation[0] = 0.f;
            translation[1] = 0.f;
        }
        ImGui::SameLine(0, 10);
        ImGui::SliderFloat2("position", translation, -5.0, 5.0);

        static float rotation = 0.f;
        if (ImGuiToolkit::ButtonIcon(4, 15))
            rotation = 0.f;
        ImGui::SameLine(0, 10); 
        ImGui::SliderFloat("rotation", &rotation, 0, 2 * PI);

        static float scale = 1.f;
        if (ImGuiToolkit::ButtonIcon(3, 15))
            scale = 1.f;
        ImGui::SameLine(0, 10);
        ImGui::SliderFloat("scale", &scale, 0.1f, 10.f, "%.3f", 3.f);

        // color picker
        static float color[4] = { 1.0f,1.0f,1.0f,1.0f };
        if (ImGuiToolkit::ButtonIcon(16, 8)) {
            color[0] = 1.f;
            color[1] = 1.f;
            color[2] = 1.f;
            color[3] = 1.f;
        }
        ImGui::SameLine(0, 10);
        ImGui::ColorEdit3("color", color);

        static float brightness = 0.0;
        if (ImGuiToolkit::ButtonIcon(4, 1))
            brightness = 0.f;
        ImGui::SameLine(0, 10);
        ImGui::SliderFloat("brightness", &brightness, -1.0, 1.0, "%.3f", 2.f);

        static float contrast = 0.0;
        if (ImGuiToolkit::ButtonIcon(2, 1))
            contrast = 0.f;
        ImGui::SameLine(0, 10);
        ImGui::SliderFloat("contrast", &contrast, -1.0, 1.0, "%.3f", 2.f);

        // pass the parameters to the shader
        rendering_shader.setUniform("scale", scale);
        rendering_shader.setUniform("rotation", rotation);
        rendering_shader.setUniform("translation", translation[0], translation[1]);
        rendering_shader.setUniform("color", color[0], color[1], color[2]);
        rendering_shader.setUniform("brightness", brightness);
        rendering_shader.setUniform("contrast", contrast);
        rendering_shader.setUniform("aspectratio", testmedia2.AspectRatio());

        ImGui::End();
    }

    // // add gui elements to vmix main window
    // ImGui::Begin("v-mix");
    // static float zoom = 0.3f;
    // if (ImGuiToolkit::ButtonIcon(5, 7))
    //     zoom = 0.3f;
    // ImGui::SameLine(0, 10);
    // if (ImGui::SliderFloat("zoom", &zoom, 0.1f, 10.f, "%.4f", 3.f))
    //     UserInterface::Log("Zoom %f", zoom);
    // rendering_shader.setUniform("render_zoom", zoom);
    // ImGui::End();


    Shader::enduse();

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

    // 1
//     testmedia.Open("file:///home/bhbn/Videos/MOV001.MOD");
//     testmedia.Open("file:///home/bhbn/Videos/TestFormats/Commodore64GameReviewMoondust.flv");
//     testmedia.Open("file:///home/bhbn/Videos/fish.mp4");
//    testmedia.Open("file:///home/bhbn/Videos/jean/Solitude1080p.mov");
    testmedia.Open("file:///home/bhbn/Videos/TearsOfSteel_720p_h265.mkv");
//    testmedia.Open("file:///home/bhbn/Videos/TestFormats/_h264GoldenLamps.mkv");
//    testmedia.Open("file:///home/bhbn/Videos/TestEncoding/vpxvp9high.webm");

//    testmedia.Open("file:///home/bhbn/Videos/iss.mov");
    testmedia.Play(false);

    // Add draw callbacks to the Rendering
    Rendering::manager().AddDrawCallback(drawMediaPlayer);

    // 2
    //    testmedia2.Open("file:///home/bhbn/Images/Butterfly.gif");
//        testmedia2.Open("file:///home/bhbn/Images/Scan-090614-0022.jpg");
    testmedia2.Open("file:///home/bhbn/Images/svg/abstract.svg");
//    testmedia2.Open("file:///home/bhbn/Images/4k/colors-3840x2160-splash-4k-18458.jpg");
    // testmedia2.Open("file:///home/bhbn/Videos/Upgrade.2018.720p.AMZN.WEB-DL.DDP5.1.H.264-NTG.m4v");
    testmedia2.Play(true);
    // create our geometries
	create_square(vbo, vao, ebo);
    // Add draw callbacks to the Rendering
    Rendering::manager().AddDrawCallback(drawMediaBackgound);

    // // load an image
//    textureimagepng = loadPNG("/home/bhbn/Videos/iss_snap.png", &texturear);

	// init shader
    rendering_shader.load("shaders/texture-shader.vs", "shaders/texture-shader.fs");

    UserInterface::manager().OpenTextEditor( Resource::getText("shaders/texture-shader.fs") );

    ///
    /// Main LOOP
    ///
    while ( Rendering::manager().isActive() )
    {
        Rendering::manager().Draw();
    }

    testmedia.Close();
    testmedia2.Close();

    UserInterface::manager().Terminate();
    Rendering::manager().Terminate();

    Settings::Save();

    return 0;
}
