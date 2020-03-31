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


