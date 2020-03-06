#ifndef vlmixer_rsc_manager
#define vlmixer_rsc_manager

#include <string>
#include <map>

class Resource
{

    static std::map<std::string, unsigned int> textureIndex;

    static void listFiles();

public:

    // Support all text files
    // return file tyext content as one string
    static std::string getText(const std::string& path);

    // Support DDS files, DXT1, DXT5 and DXT5 
    // Returns the OpenGL generated Texture index
    static unsigned int getTextureDDS(const std::string& path);

    // Support PNG, JPEG, TGA, BMP, PSD, GIF, HDR, PIC, PNM
    // Returns the OpenGL generated Texture index
    static unsigned int getTextureImage(const std::string& path);

    // Generic access to pointer to data
    static const char *getData(const std::string& path, size_t* out_file_size);

};

#endif /* vlmixer_rsc_manager */