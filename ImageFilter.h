#ifndef IMAGEFILTER_H
#define IMAGEFILTER_H

#include <map>
#include <list>
#include <string>
#include <future>

#include <glm/glm.hpp>

#include "FrameBufferFilter.h"

class FilteringProgram
{
    std::string name_;

    // code : resource file or GLSL (ShaderToy style)
    std::pair< std::string, std::string > code_;

    // true if code is given for second pass
    bool two_pass_filter_;

    // list of parameters : uniforms names and values
    std::map< std::string, float > parameters_;

public:

    FilteringProgram();
    FilteringProgram(const std::string &name, const std::string &first_pass, const std::string &second_pass,
                     const std::map<std::string, float> &parameters);
    FilteringProgram(const FilteringProgram &other);

    FilteringProgram& operator= (const FilteringProgram& other);
    bool operator!= (const FilteringProgram& other) const;

    // get the name
    inline std::string name() const { return name_; }

    // set the code
    inline void setCode(const std::pair< std::string, std::string > &code) { code_ = code; }

    // get the code
    std::pair< std::string, std::string > code();

    // if has second pass
    bool isTwoPass() const { return two_pass_filter_; }

    // set the list of parameters
    inline void setParameters(const std::map< std::string, float > &parameters) { parameters_ = parameters; }

    // get the list of parameters
    inline std::map< std::string, float > parameters() const { return parameters_; }

    // set the value of a parameter
    inline void setParameter(const std::string &p, float value) { parameters_[p] = value; }

    // globals
    static std::string getFilterCodeInputs();
    static std::string getFilterCodeDefault();
    static std::list< FilteringProgram > presets;
};

class Surface;
class FrameBuffer;
class ImageFilteringShader;

class ImageFilter : public FrameBufferFilter
{
    FilteringProgram program_;

    std::pair< Surface *, Surface *> surfaces_;
    std::pair< FrameBuffer *, FrameBuffer * > buffers_;
    std::pair< ImageFilteringShader *, ImageFilteringShader *> shaders_;

public:

    // instanciate an image filter at given resolution
    ImageFilter();
    ~ImageFilter();

    // set the program
    void setProgram(const FilteringProgram &f, std::promise<std::string> *ret = nullptr);

    // get copy of the program
    FilteringProgram program() const;

    // implementation of FrameBufferFilter
    Type type() const { return FrameBufferFilter::FILTER_IMAGE; }
    uint texture () const override;
    glm::vec3 resolution () const override;
    void update (float dt) override;
    void draw   (FrameBuffer *input) override;
    void accept (Visitor& v) override;
};


#endif // IMAGEFILTER_H
