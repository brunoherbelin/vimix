/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2022 Bruno Herbelin <bruno.herbelin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
**/

#include <fstream>
#include <sstream>
#include <iostream>
#include <cstring>

//  Desktop OpenGL function loader
#include <glad/glad.h>

// standalone image loader
#include <stb_image.h>

// CMake Ressource Compiler
#include <cmrc/cmrc.hpp>
CMRC_DECLARE(vmix);

#include "defines.h"
#include "Log.h"
#include "Resource.h"


std::map<std::string, uint> textureIndex;
std::map<std::string, float> textureAspectRatio;

// opengl texture
uint Resource::getTextureBlack()
{
    static uint tex_index_black = 0;

    // generate texture (once)
    if (tex_index_black == 0) {
        glGenTextures(1, &tex_index_black);
        glBindTexture( GL_TEXTURE_2D, tex_index_black);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        unsigned char clearColor[4] = {0, 0, 0, 255};
        // texture with one black pixel
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 1, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, clearColor);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    return tex_index_black;
}

uint Resource::getTextureWhite()
{
    static uint tex_index_white = 0;

    // generate texture (once)
    if (tex_index_white == 0) {
        glGenTextures(1, &tex_index_white);
        glBindTexture( GL_TEXTURE_2D, tex_index_white);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        unsigned char clearColor[4] = {255, 255, 255, 255};
        // texture with one black pixel
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 1, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, clearColor);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    return tex_index_white;
}

uint Resource::getTextureTransparent()
{
    static uint tex_index_transparent = 0;

    // generate texture (once)
    if (tex_index_transparent == 0) {
        glGenTextures(1, &tex_index_transparent);
        glBindTexture( GL_TEXTURE_2D, tex_index_transparent);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        unsigned char clearColor[4] = {0, 0, 0, 0};
        // texture with one black pixel
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 1, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, clearColor);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    return tex_index_transparent;
}

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
    catch (const std::system_error &e) {
        Log::Error("Could not access resource %s", std::string(path).c_str());
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
    catch (const std::system_error &e) {
        Log::Error("Could not access resource %s", std::string(path).c_str());
	}

	return file_stream.str();
}


#define FOURCC_DXT1 0x31545844 // Equivalent to "DXT1" in ASCII
#define FOURCC_DXT3 0x33545844 // Equivalent to "DXT3" in ASCII
#define FOURCC_DXT5 0x35545844 // Equivalent to "DXT5" in ASCII

uint Resource::getTextureDDS(const std::string& path, float *aspect_ratio)
{
	GLuint textureID = 0;

    if (textureIndex.count(path) > 0){
        if (aspect_ratio) *aspect_ratio = textureAspectRatio[path];
        return textureIndex[path];
    }

    // Get the pointer
    size_t size = 0;
    const char *fp = getData(path, &size);
    if ( size==0 ){
        Log::Error("Could not open resource %s: empty?", std::string(path).c_str());
        return 0;
    }

	// verify the type of file = bytes [0 - 3]
    const char *filecode = fp;
    if (strncmp(filecode, "DDS ", 4) != 0){
        Log::Error("Could not open DDS resource %s: wrong format.", std::string(path).c_str());
        return 0;
    }

	// get the surface desc = bytes [4 - 127]
    const char *header = fp + 4;
    uint height      = *(uint*)&(header[8 ]);
    uint width	     = *(uint*)&(header[12]);
    uint linearSize	 = *(uint*)&(header[16]);
    uint mipMapCount = *(uint*)&(header[24]);
    uint fourCC      = *(uint*)&(header[80]);

    // how big is it going to be including all mipmaps?
    uint bufsize = mipMapCount > 1 ? linearSize * 2 : linearSize;

	// get the buffer = bytes [128 - ]
	const char *buffer = fp + 128;

    uint format = 0;
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
        Log::Error("Could not open DDS resource %s: Not a DXT1, DXT3 or DXT5 texture.",std::string(path).c_str());
        return 0;
        }
	}

    if (height == 0 || bufsize == 0){
        Log::Error("Invalid image in resource %s", std::string(path).c_str());
        return 0;
    }
    float ar = static_cast<float>(width) / static_cast<float>(height);

	// Create one OpenGL texture
	glGenTextures(1, &textureID);

	// Bind the newly created texture
	glBindTexture(GL_TEXTURE_2D, textureID);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    uint blockSize = (format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) ? 8 : 16;
    uint offset = 0;

	// load the mipmaps
    for (uint level = 0; level < mipMapCount && (width || height); ++level)
	{
        uint s = ((width+3)/4)*((height+3)/4)*blockSize;
        glCompressedTexImage2D(GL_TEXTURE_2D, level, format, width, height, 0, s, buffer + offset);
		glGenerateMipmap(GL_TEXTURE_2D);

        offset += s;
		width  /= 2;
		height /= 2;

		// Deal with Non-Power-Of-Two textures.
		if(width < 1) width = 1;
		if(height < 1) height = 1;

	}
    glBindTexture(GL_TEXTURE_2D, 0);

    // remember to avoid openning the same resource twice
    textureIndex[path] = textureID;
    textureAspectRatio[path] = ar;

    // return values
    if (aspect_ratio) *aspect_ratio = ar;
    return textureID;
}


uint Resource::getTextureImage(const std::string& path, float *aspect_ratio)
{
    std::string ext = path.substr(path.find_last_of(".") + 1);
    if (ext=="dds"){
        return getTextureDDS(path, aspect_ratio);
    }

    // return previously opened resource if already open
    if (textureIndex.count(path) > 0) {
        if (aspect_ratio) *aspect_ratio = textureAspectRatio[path];
		return textureIndex[path];
    }

    GLuint textureID = 0;
    float ar = 1.0;
 	int w, h, n;
    unsigned char* img = nullptr;

    // Get the pointer
    size_t size = 0;
    const char *fp = getData(path, &size);
    if ( size==0 ){
        Log::Error("Could not open resource %s: empty?",std::string(path).c_str());
        return 0;
    }

	img = stbi_load_from_memory(reinterpret_cast<const unsigned char *>(fp), size, &w, &h, &n, 4);
	if (img == NULL) {
        Log::Error("Failed to open resource %s: %s", std::string(path).c_str(), stbi_failure_reason() );
	 	return 0;
	}
    if (h == 0){
        Log::Error("Invalid image in resource %s", std::string(path).c_str());
        stbi_image_free(img);
        return 0;
    }
    ar = static_cast<float>(w) / static_cast<float>(h);

    glGenTextures(1, &textureID);
    glBindTexture( GL_TEXTURE_2D, textureID);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, w, h);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, img);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // free memory
	stbi_image_free(img);

    // remember to avoid openning the same resource twice
	textureIndex[path] = textureID;
    textureAspectRatio[path] = ar;

    // return values
    if (aspect_ratio) *aspect_ratio = ar;
	return textureID;
}

std::string Resource::listDirectory()
{
    // enter directory
    auto fs = cmrc::vmix::get_filesystem();
	cmrc::directory_iterator it = fs.iterate_directory("");
	cmrc::directory_iterator itend = it.end();

    std::string ls;

	// loop over all icons
	while(it != itend){

		cmrc::directory_entry file = *it;

		if (file.is_file()) {
            ls += file.filename() + ", ";
		}

		it++;
	}

    return ls;
}

bool Resource::hasPath(const std::string& path)
{
    auto fs = cmrc::vmix::get_filesystem();
    return fs.exists(path);
}
