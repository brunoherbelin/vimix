#include "defines.h"
#include "ResourceManager.h"
#include "Log.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <cstring>

//  Desktop OpenGL function loader
#include <glad/glad.h>

// multiplatform message box
#include <tinyfiledialogs.h>

// standalone image loader
#include "stb_image.h"

// CMake Ressource Compiler
#include <cmrc/cmrc.hpp>
CMRC_DECLARE(vmix);


std::map<std::string, unsigned int> Resource::textureIndex;

const char *Resource::getData(const std::string& path, size_t* out_file_size){

    auto  fs = cmrc::vmix::get_filesystem();
    cmrc::file file;
    const char* data = nullptr;
    *out_file_size = 0;

    try {
        file = fs.open(path.c_str());
        const auto rc_size   = std::distance(file.begin(), file.end());
        *out_file_size = rc_size;
        cmrc::file::iterator it = file.begin();
        data = static_cast<const char *>(it);
    }
	catch (std::system_error e) {
        std::string msg = "Could not access ressource " + std::string(path);
        tinyfd_messageBox(APP_TITLE, msg.c_str(), "ok", "error", 0);
	}

    return data;
}

std::string Resource::getText(const std::string& path){

    auto fs = cmrc::vmix::get_filesystem();
    cmrc::file file;
	std::stringstream file_stream;
	try {
		file = fs.open(path.c_str());
    	file_stream << std::string(file.begin(), file.end()) << std::endl;
    }
	catch (std::system_error e) {
        std::string msg = "Could not access ressource " + std::string(path);
        tinyfd_messageBox(APP_TITLE, msg.c_str(), "ok", "error", 0);
	}

	return file_stream.str();
}


#define FOURCC_DXT1 0x31545844 // Equivalent to "DXT1" in ASCII
#define FOURCC_DXT3 0x33545844 // Equivalent to "DXT3" in ASCII
#define FOURCC_DXT5 0x35545844 // Equivalent to "DXT5" in ASCII

unsigned int Resource::getTextureDDS(const std::string& path)
{
	GLuint textureID = 0;

	if (textureIndex.count(path) > 0)
		return textureIndex[path];

    // Get the pointer
    size_t size = 0;
    const char *fp = getData(path, &size);
    if ( size==0 ){
        std::string msg = "Empty ressource " + std::string(path);
        tinyfd_messageBox(APP_TITLE, msg.c_str(), "ok", "error", 0);
        return 0;
    }

	// verify the type of file = bytes [0 - 3]
    const char *filecode = fp;
    if (strncmp(filecode, "DDS ", 4) != 0){
        std::string msg = "Not a DDS image " + std::string(path);
        tinyfd_messageBox(APP_TITLE, msg.c_str(), "ok", "error", 0);
        return 0;
    }

	// get the surface desc = bytes [4 - 127]
    const char *header = fp + 4;
	unsigned int height      = *(unsigned int*)&(header[8 ]);
	unsigned int width	     = *(unsigned int*)&(header[12]);
	unsigned int linearSize	 = *(unsigned int*)&(header[16]);
	unsigned int mipMapCount = *(unsigned int*)&(header[24]);
	unsigned int fourCC      = *(unsigned int*)&(header[80]);

	// how big is it going to be including all mipmaps?
	unsigned int bufsize;
	bufsize = mipMapCount > 1 ? linearSize * 2 : linearSize;

	// get the buffer = bytes [128 - ]
	const char *buffer = fp + 128;

	unsigned int components  = (fourCC == FOURCC_DXT1) ? 3 : 4;
	unsigned int format;
	switch(fourCC)
	{
	case FOURCC_DXT1:
		format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
		break;
	case FOURCC_DXT3:
		format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
		break;
	case FOURCC_DXT5:
		format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
		break;
	default:
        {
            std::string msg = "Not a DXT1, DXT3 or DXT5 texture " + std::string(path);
            tinyfd_messageBox(APP_TITLE, msg.c_str(), "ok", "error", 0);
            return 0;
        }
	}

	// Create one OpenGL texture
	glGenTextures(1, &textureID);

	// Bind the newly created texture
	glBindTexture(GL_TEXTURE_2D, textureID);
	glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	unsigned int blockSize = (format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) ? 8 : 16;
	unsigned int offset = 0;

	// load the mipmaps
	for (unsigned int level = 0; level < mipMapCount && (width || height); ++level)
	{
		unsigned int size = ((width+3)/4)*((height+3)/4)*blockSize;
		glCompressedTexImage2D(GL_TEXTURE_2D, level, format, width, height,
			0, size, buffer + offset);
		glGenerateMipmap(GL_TEXTURE_2D);

		offset += size;
		width  /= 2;
		height /= 2;

		// Deal with Non-Power-Of-Two textures.
		if(width < 1) width = 1;
		if(height < 1) height = 1;

	}

	textureIndex[path] = textureID;
	return textureID;
}


unsigned int Resource::getTextureImage(const std::string& path)
{
	GLuint textureID = 0;

	if (textureIndex.count(path) > 0)
		return textureIndex[path];

 	int w, h, n;
	unsigned char* img;

    // Get the pointer
    size_t size = 0;
    const char *fp = getData(path, &size);
    if ( size==0 ){
        std::string msg = "Empty ressource " + std::string(path);
        tinyfd_messageBox(APP_TITLE, msg.c_str(), "ok", "error", 0);
        return 0;
    }

	img = stbi_load_from_memory(reinterpret_cast<const unsigned char *>(fp), size, &w, &h, &n, 4);
	if (img == NULL) {
        std::string msg = "Failed to load " + std::string(path) + " : " + std::string(stbi_failure_reason());
        tinyfd_messageBox(APP_TITLE, msg.c_str(), "ok", "error", 0);
	 	return 0;
	}

    glGenTextures(1, &textureID);
	glBindTexture( GL_TEXTURE_2D, textureID);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    //glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);

	stbi_image_free(img);

	textureIndex[path] = textureID;
	return textureID;
}

void Resource::listFiles()
{
	// enter icons directory
    auto fs = cmrc::vmix::get_filesystem();
    cmrc::file file;
	cmrc::directory_iterator it = fs.iterate_directory("");
	cmrc::directory_iterator itend = it.end();

	// loop over all icons
	while(it != itend){

		cmrc::directory_entry file = *it;

		if (file.is_file()) {
			std::cerr << "Found file " << file.filename()  << std::endl;

		}

		it++;
	}
}
