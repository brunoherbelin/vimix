#ifndef IMAGEFILTER_H
#define IMAGEFILTER_H

#include <map>
#include <list>
#include <string>
#include <glm/glm.hpp>
#include <glib/gtimer.h>

#include "ImageShader.h"

class Surface;
class FrameBuffer;
class ImageFilteringShader;


class ImageFilter
{
    std::string name_;

    // code resource file or GLSL (ShaderToy style)
    std::pair< std::string, std::string > code_;

    // list of parameters (uniforms names and values)
    std::map< std::string, float > parameters_;

public:

    ImageFilter();
    ImageFilter(const std::string &name, const std::string &first_pass, const std::string &second_pass,
                const std::map<std::string, float> &parameters);
    ImageFilter(const ImageFilter &other);

    ImageFilter& operator= (const ImageFilter& other);
    bool operator!= (const ImageFilter& other) const;

    // get the name of the filter
    inline std::string name() const { return name_; }

    // set the code of the filter
    inline void setCode(const std::pair< std::string, std::string > &code) { code_ = code; }

    // get the code of the filter
    std::pair< std::string, std::string > code();

    // set the list of parameters of the filter
    inline void setParameters(const std::map< std::string, float > &parameters) { parameters_ = parameters; }

    // get the list of parameters  of the filter
    inline std::map< std::string, float > parameters() const { return parameters_; }

    // set a value of a parameter
    inline void setParameter(const std::string &p, float value) { parameters_[p] = value; }

    // globals
    static std::string getFilterCodeInputs();
    static std::string getFilterCodeDefault();
    static std::list< ImageFilter > presets;
};

class ImageFilteringShader;

class ImageFilterRenderer
{
    ImageFilter filter_;
    bool enabled_;

    std::pair< Surface *, Surface *> surfaces_;
    std::pair< FrameBuffer *, FrameBuffer * > buffers_;
    std::pair< ImageFilteringShader *, ImageFilteringShader *> shaders_;

public:

    // instanciate an image filter at given resolution, with alpha channel
    ImageFilterRenderer(glm::vec3 resolution);
    ~ImageFilterRenderer();

    inline void setEnabled (bool on) { enabled_ = on; }
    inline bool enabled () const { return enabled_; }

    // set the texture to draw into the framebuffer
    void setInputTexture(uint t);

    // draw the input texture with filter on the framebuffer
    void draw();

    // get the texture id of the rendered framebuffer
    uint getOutputTexture() const;

    // set the filter
    void setFilter(const ImageFilter &f, std::promise<std::string> *ret = nullptr);

    // get copy of the filter
    ImageFilter filter() const;

};


#endif // IMAGEFILTER_H
