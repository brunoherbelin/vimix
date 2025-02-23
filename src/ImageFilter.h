#ifndef IMAGEFILTER_H
#define IMAGEFILTER_H

#include <map>
#include <list>
#include <vector>
#include <string>
#include <future>

#include <glib.h>
#include <glm/glm.hpp>

#include "ImageShader.h"
#include "FrameBufferFilter.h"

class FilteringProgram
{
    std::string name_;
    std::string filename_;

    // code : resource file or GLSL (ShaderToy style)
    std::pair< std::string, std::string > code_;

    // true if code is given for second pass
    bool two_pass_filter_;

    // list of parameters : uniforms names and values
    std::map< std::string, float > parameters_;

public:

    FilteringProgram();
    FilteringProgram(const std::string &name, const std::string &first_pass, const std::string &second_pass,
                     const std::map<std::string, float> &parameters, const std::string &filename = std::string());
    FilteringProgram(const FilteringProgram &other);

    FilteringProgram& operator= (const FilteringProgram& other);
    bool operator!= (const FilteringProgram& other) const;

    // get the name
    inline void setName(const std::string &name) { name_ = name; }
    inline std::string name() const { return name_; }

    // get the filename
    inline void resetFilename() { filename_ = std::string(); }
    inline void setFilename(const std::string &filename) { filename_ = filename; }
    inline std::string filename() const { return filename_; }

    // set the code
    inline void setCode(const std::pair< std::string, std::string > &code) { code_ = code; }

    // get the code
    std::pair< std::string, std::string > code();

    // if has second pass
    inline bool isTwoPass() const { return two_pass_filter_; }

    // set the list of parameters
    inline void setParameters(const std::map< std::string, float > &parameters) { parameters_ = parameters; }

    // get the list of parameters
    inline std::map< std::string, float > parameters() const { return parameters_; }

    // set the value of a parameter
    inline void clearParameters() { parameters_.clear(); }
    inline void setParameter(const std::string &p, float value) { parameters_[p] = value; }
    bool hasParameter(const std::string &p);
    void removeParameter(const std::string &p);

    // globals
    static std::string getFilterCodeInputs();
    static std::string getFilterCodeDefault();
    static std::list< FilteringProgram > example_filters;
    static std::list< FilteringProgram > example_patterns;
    static glm::vec4 iMouse;
};

class Surface;
class FrameBuffer;

class ImageFilteringShader : public ImageShader
{
    // GLSL Program
    ShadingProgram custom_shading_;

    // fragment shader GLSL code
    std::string shader_code_;
    std::string code_;

public:
    // for iTimedelta
    GTimer *timer_;
    double iTime_;
    uint iFrame_;

    // list of uniforms to control shader
    std::map< std::string, float > uniforms_;
    bool uniforms_changed_;

    ImageFilteringShader();
    ~ImageFilteringShader();

    void update(float dt);

    void use() override;
    void reset() override;
    void copy(ImageFilteringShader const& S);

    // set the code of the filter
    void setCode(const std::string &code, std::promise<std::string> *ret = nullptr);

};


class ImageFilter : public FrameBufferFilter
{
    FilteringProgram program_;

public:

    // instanciate an image filter at given resolution
    ImageFilter();
    virtual ~ImageFilter();

    // set the program
    void setProgram(const FilteringProgram &f, std::promise<std::string> *ret = nullptr);
    // get copy of the program
    FilteringProgram program() const;

    // update parameters of program
    void setProgramParameters(const std::map< std::string, float > &parameters);
    void setProgramParameter(const std::string &p, float value);

    // implementation of FrameBufferFilter
    Type type() const override { return FrameBufferFilter::FILTER_IMAGE; }
    uint texture () const override;
    glm::vec3 resolution () const override;
    void update (float dt) override;
    double updateTime () override;
    void reset () override;
    void draw   (FrameBuffer *input) override;
    void accept (Visitor& v) override;

protected:

    std::pair< Surface *, Surface *> surfaces_;
    std::pair< FrameBuffer *, FrameBuffer * > buffers_;
    std::pair< ImageFilteringShader *, ImageFilteringShader *> shaders_;
    void updateParameters();
    bool channel1_output_session;
};


class ResampleFilter : public ImageFilter
{
public:

    ResampleFilter();

    // Factors of resampling
    typedef enum {
        RESAMPLE_DOUBLE = 0,
        RESAMPLE_HALF,
        RESAMPLE_QUARTER,
        RESAMPLE_INVALID
    } ResampleFactor;
    static const char* factor_label[RESAMPLE_INVALID];
    ResampleFactor factor () const { return factor_; }
    bool setFactor(const std::string &label);
    void setFactor(int factor);

    // implementation of FrameBufferFilter
    Type type() const override { return FrameBufferFilter::FILTER_RESAMPLE; }

    void draw   (FrameBuffer *input) override;
    void accept (Visitor& v) override;

private:
    ResampleFactor factor_;
    static std::vector< FilteringProgram > programs_;
};



class BlurFilter : public ImageFilter
{
public:

    BlurFilter();
    virtual ~BlurFilter();

    // Algorithms used for blur
    typedef enum {
        BLUR_GAUSSIAN = 0,
        BLUR_HASH,
        BLUR_OPENNING,
        BLUR_CLOSING,
        BLUR_FAST,
        BLUR_INVALID
    } BlurMethod;
    static const char* method_label[BLUR_INVALID];
    BlurMethod method () const { return method_; }
    bool setMethod(const std::string &label);
    void setMethod(int method);

    // implementation of FrameBufferFilter
    Type type() const override { return FrameBufferFilter::FILTER_BLUR; }

    void draw   (FrameBuffer *input) override;
    void accept (Visitor& v) override;

private:
    BlurMethod method_;
    static std::vector< FilteringProgram > programs_;

    Surface *mipmap_surface_;
    FrameBuffer *mipmap_buffer_;
};


class SharpenFilter : public ImageFilter
{
public:

    SharpenFilter();

    // Algorithms used for sharpen
    typedef enum {
        SHARPEN_MASK = 0,
        SHARPEN_CONVOLUTION,
        SHARPEN_EDGE,
        SHARPEN_WHITEHAT,
        SHARPEN_BLACKHAT,
        SHARPEN_INVALID
    } SharpenMethod;
    static const char* method_label[SHARPEN_INVALID];
    SharpenMethod method () const { return method_; }
    bool setMethod(const std::string &label);
    void setMethod(int method);

    // implementation of FrameBufferFilter
    Type type() const override { return FrameBufferFilter::FILTER_SHARPEN; }

    void draw   (FrameBuffer *input) override;
    void accept (Visitor& v) override;

private:
    SharpenMethod method_;
    static std::vector< FilteringProgram > programs_;
};


class SmoothFilter : public ImageFilter
{
public:

    SmoothFilter();

    // Algorithms used for smoothing
    typedef enum {
        SMOOTH_BILINEAR = 0,
        SMOOTH_KUWAHARA,
        SMOOTH_OPENING,
        SMOOTH_CLOSING,
        SMOOTH_EROSION,
        SMOOTH_DILATION,
        SMOOTH_DENOISE,
        SMOOTH_ADDNOISE,
        SMOOTH_ADDGRAIN,
        SMOOTH_INVALID
    } SmoothMethod;
    static const char* method_label[SMOOTH_INVALID];
    static std::vector< FilteringProgram > programs_;

    SmoothMethod method () const { return method_; }
    bool setMethod(const std::string &label);
    void setMethod(int method);

    // implementation of FrameBufferFilter
    Type type() const override { return FrameBufferFilter::FILTER_SMOOTH; }

    void draw   (FrameBuffer *input) override;
    void accept (Visitor& v) override;

private:
    SmoothMethod method_;
};


class EdgeFilter : public ImageFilter
{
public:

    EdgeFilter();

    // Algorithms used for edge detection
    typedef enum {
        EDGE_SOBEL = 0,
        EDGE_FREICHEN,
        EDGE_THRESHOLD,
        EDGE_CONTOUR,
        EDGE_INVALID
    } EdgeMethod;
    static const char* method_label[EDGE_INVALID];
    EdgeMethod method () const { return method_; }
    bool setMethod(const std::string &label);
    void setMethod(int method);

    // implementation of FrameBufferFilter
    Type type() const override { return FrameBufferFilter::FILTER_EDGE; }

    void draw   (FrameBuffer *input) override;
    void accept (Visitor& v) override;

private:
    EdgeMethod method_;
    static std::vector< FilteringProgram > programs_;
};




class AlphaFilter : public ImageFilter
{
public:

    AlphaFilter();

    // Operations on alpha
    typedef enum {
        ALPHA_CHROMAKEY = 0,
        ALPHA_LUMAKEY,
        ALPHA_FILL,
        ALPHA_INVALID
    } AlphaOperation;
    static const char* operation_label[ALPHA_INVALID];
    AlphaOperation operation () const { return operation_; }
    bool setOperation(const std::string &label);
    void setOperation(int op);

    // implementation of FrameBufferFilter
    Type type() const override { return FrameBufferFilter::FILTER_ALPHA; }

    void draw   (FrameBuffer *input) override;
    void accept (Visitor& v) override;

private:
    AlphaOperation operation_;
    static std::vector< FilteringProgram > programs_;
};



#endif // IMAGEFILTER_H
