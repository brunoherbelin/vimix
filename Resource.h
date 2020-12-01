#ifndef __RSC_MANAGER_H_
#define __RSC_MANAGER_H_

#include <string>
#include <map>
#ifdef __APPLE__
#include <sys/types.h>
#endif

namespace Resource
{

    // Support all text files
    // return file tyext content as one string
    std::string getText(const std::string& path);

    // Support DDS files, DXT1, DXT5 and DXT5 
    // Returns the OpenGL generated Texture index
    uint getTextureDDS(const std::string& path, float *aspect_ratio = nullptr);

    // Support PNG, JPEG, TGA, BMP, PSD, GIF, HDR, PIC, PNM
    // Returns the OpenGL generated Texture index
    uint getTextureImage(const std::string& path, float *aspect_ratio = nullptr);

    // Returns the OpenGL generated Texture index for an empty 1x1 black opaque pixel texture
    uint getTextureBlack();

    // Returns the OpenGL generated Texture index for an empty 1x1 white opaque pixel texture
    uint getTextureWhite();

    // Returns the OpenGL generated Texture index for an empty 1x1 back transparent pixel texture
    uint getTextureTransparent();

    // Generic access to pointer to data
    const char *getData(const std::string& path, size_t* out_file_size);

    // list files in resource directory
    std::string listDirectory();

}

#endif /* __RSC_MANAGER_H_ */
