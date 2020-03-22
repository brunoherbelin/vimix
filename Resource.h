#ifndef __RSC_MANAGER_H_
#define __RSC_MANAGER_H_

#include <string>
#include <map>

namespace Resource
{

    // Support all text files
    // return file tyext content as one string
    std::string getText(const std::string& path);

    // Support DDS files, DXT1, DXT5 and DXT5 
    // Returns the OpenGL generated Texture index
    unsigned int getTextureDDS(const std::string& path);

    // Support PNG, JPEG, TGA, BMP, PSD, GIF, HDR, PIC, PNM
    // Returns the OpenGL generated Texture index
    unsigned int getTextureImage(const std::string& path);

    // Generic access to pointer to data
    const char *getData(const std::string& path, size_t* out_file_size);

    // list files in resource directory
    std::string listDirectory();

}

#endif /* __RSC_MANAGER_H_ */
