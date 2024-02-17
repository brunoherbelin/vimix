
// old stuff
ImageShader rendering_shader;
MediaPlayer testmedia;
MediaPlayer testmedia2("testmedia2");
unsigned int vbo, vao, ebo;
float texturear = 1.0;
GLuint textureimagepng = 0;


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


void create_square_glm(unsigned int &vbo, unsigned int &vao, unsigned int &ebo)
{
    std::vector<glm::vec3>     points_;
    std::vector<glm::vec3>     colors_;
    std::vector<glm::vec2>     texCoords_;
    std::vector<unsigned int>  indices_;

    // point 0
    points_.push_back( glm::vec3( -1.f, -1.f, 0.f ) );
    colors_.push_back( glm::vec3( 1.f, 0.f, 0.f ) );
    texCoords_.push_back( glm::vec2( 0.f, 1.f ) );
    // point 1
    points_.push_back( glm::vec3( -1.f, 1.f, 0.f ) );
    colors_.push_back( glm::vec3( 1.f, 1.f, 1.f ) );
    texCoords_.push_back( glm::vec2( 0.f, 0.f ) );
    // point 2
    points_.push_back( glm::vec3( 1.f, -1.f, 0.f ) );
    colors_.push_back( glm::vec3( 1.f, 1.f, 1.f ) );
    texCoords_.push_back( glm::vec2( 1.f, 1.f ) );
    // point 3
    points_.push_back( glm::vec3( 1.f, 1.f, 0.f ) );
    colors_.push_back( glm::vec3( 0.f, 1.f, 0.f ) );
    texCoords_.push_back( glm::vec2( 1.f, 0.f ) );
    // indices
    indices_.push_back ( 0 );
    indices_.push_back ( 1 );
    indices_.push_back ( 2 );
    indices_.push_back ( 3 );

    // create the opengl objects
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glBindVertexArray(vao);

    // compute the memory needs for points normals and indicies
    std::size_t sizeofPoints = sizeof(glm::vec3) * points_.size();
    std::size_t sizeofColors = sizeof(glm::vec3) * colors_.size();
    std::size_t sizeofTexCoords = sizeof(glm::vec2) * texCoords_.size();

    // bind vertext array and its buffer
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeofPoints + sizeofColors + sizeofTexCoords, NULL, GL_STATIC_DRAW);
    glBufferSubData( GL_ARRAY_BUFFER, 0, sizeofPoints, &points_[0] );
    glBufferSubData( GL_ARRAY_BUFFER, sizeofPoints, sizeofColors, &colors_[0] );
    glBufferSubData( GL_ARRAY_BUFFER, sizeofPoints + sizeofColors, sizeofTexCoords, &texCoords_[0] );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    int sizeofIndices = indices_.size()*sizeof(unsigned int);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeofIndices, &(indices_[0]), GL_STATIC_DRAW);

    // explain how to read attributes 0, 1 and 2 (for point, color and textcoord respectively)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void *)(sizeofPoints) );
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void *)(sizeofPoints + sizeofColors) );
    glEnableVertexAttribArray(2);

    // done
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

}

// from https://gstreamer.freedesktop.org/documentation/application-development/advanced/pipeline-manipulation.html?gi-language=c

void drawMediaBackgound()
{
    if ( !testmedia2.isOpen() )
        return;

    //
    // RENDER SOURCE
    //
    testmedia2.update();

    // use the shader
    rendering_shader.projection = Rendering::manager().Projection();
    rendering_shader.modelview = glm::identity<glm::mat4>();;
    rendering_shader.use();
    // use the media
    testmedia2.bind();
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

    testmedia.update();

    if ( begin == GST_CLOCK_TIME_NONE) {

        begin = testmedia.duration() / 5;
        end = testmedia.duration() / 3;
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

    ImVec2 imagesize ( width, width / testmedia.aspectRatio());
    ImGui::Image((void*)(intptr_t)testmedia.texture(), imagesize);

    if (ImGui::Button(ICON_FA_FAST_BACKWARD))
        testmedia.rewind();
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
            testmedia.fastForward ();
        ImGui::PopButtonRepeat();
    }
    else {

        if (ImGui::Button(ICON_FA_PLAY "  Play"))
            media_playing_mode = true;
        ImGui::SameLine(0, spacing);

        ImGui::PushButtonRepeat(true);
        if (ImGui::Button(ICON_FA_STEP_FORWARD))
            testmedia.seekNextFrame();
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

    float speed = static_cast<float>(testmedia.playSpeed());
    ImGui::SameLine(0, spacing);
    ImGui::SetNextItemWidth(270);
    // ImGui::SetNextItemWidth(width - 90.0);
    if (ImGui::SliderFloat( "Speed", &speed, -10.f, 10.f, "x %.2f", 2.f))
        testmedia.setPlaySpeed( static_cast<double>(speed) );
    ImGui::SameLine(0, spacing);
    if (ImGuiToolkit::ButtonIcon(19, 15)) {
        speed = 1.f;
        testmedia.setPlaySpeed( static_cast<double>(speed) );
    }

    guint64 current_t = testmedia.position();
    guint64 seek_t = current_t;

    bool slider_pressed = ImGuiToolkit::TimelineSlider( "simpletimeline", &seek_t,
                                                        testmedia.duration(), testmedia.frameDuration());

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
    if ( ABS_DIFF (current_t, seek_t) > testmedia.frameDuration() ) {

        // request seek (ASYNC)
        testmedia.seekTo(seek_t);
    }

    // play/stop command should be following the playing mode (buttons)
    // AND force to stop when the slider is pressed
    bool media_play = media_playing_mode & (!slider_pressed);

    // apply play action to media only if status should change
    // NB: The seek command performed an ASYNC state change, but
    // gst_element_get_state called in isPlaying() will wait for the state change to complete.
    if ( testmedia.isPlaying() != media_play ) {
        testmedia.play( media_play );
    }

    // display info
    ImGui::Text("%s %d x %d", testmedia.codec().c_str(), testmedia.width(), testmedia.height());
    ImGui::Text("Framerate %.2f / %.2f", testmedia.updateFrameRate() , testmedia.frameRate() );

    ImGui::End();
}


//        // get second argument : can be string, float or nothing
//        bool argloop = false;
//        while (!argloop) {
//            // work on copy of arguments
//            osc::ReceivedMessageArgumentStream __args = arguments;
//            // no argument
//            if (__args.Eos()) {
//                filter_arg_type = osc::NIL_TYPE_TAG;
//                argloop = true;
//            }
//            // try with float (last try)
//            if (filter_arg_type == osc::FLOAT_TYPE_TAG) {
//                try {
//                    __args >> filter_value >> osc::EndMessage;
//                } catch (osc::WrongArgumentTypeException &) {
//                    filter_arg_type = osc::NIL_TYPE_TAG;
//                }
//                // done!
//                argloop = true;
//            }
//            // try with string (first try)
//            if (filter_arg_type == osc::STRING_TYPE_TAG) {
//                try {
//                    const char *m = nullptr;
//                    __args >> m >> osc::EndMessage;
//                    // done!
//                    filter_method = std::string(m);
//                    std::transform(filter_method.begin(),
//                                   filter_method.end(),
//                                   filter_method.begin(),
//                                   ::tolower);
//                    argloop = true;
//                } catch (osc::WrongArgumentTypeException &) {
//                    // next: try with float
//                    filter_arg_type = osc::FLOAT_TYPE_TAG;
//                }
//            }
//        }
