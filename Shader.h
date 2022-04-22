#ifndef __SHADER_H_
#define __SHADER_H_

#include <future>
#include <string>
#include <vector>
#include <glm/glm.hpp>

// Forward declare classes referenced
class Visitor;
class FrameBuffer;

class ShadingProgram
{
public:
    // create GLSL Program from resource file (if exist) or code of vertex and fragment shaders
    ShadingProgram(const std::string& vertex = "", const std::string& fragment = "");

    // Update GLSL Program with vertex and fragment program
    // If a promise is given, it is filled during compilation with the compilation log.
    void setShaders(const std::string& vertex, const std::string& fragment, std::promise<std::string> *prom = nullptr);

    void use();
    void compile();
    static void enduse();
    void reset();

	template<typename T> void setUniform(const std::string& name, T val);
	template<typename T> void setUniform(const std::string& name, T val1, T val2);
    template<typename T> void setUniform(const std::string& name, T val1, T val2, T val3);

private:
    unsigned int id_;
    bool need_compile_;
    std::string vertex_;
    std::string fragment_;
    std::promise<std::string> *promise_;

    static ShadingProgram *currentProgram_;
};

class Shader
{
    uint64_t  id_;

public:
    Shader();
    virtual ~Shader() {}

    // unique identifyer generated at instanciation
    inline uint64_t id () const { return id_; }

    virtual void use();
    virtual void reset();
    virtual void accept(Visitor& v);
    void copy(Shader const& S);

    glm::mat4 projection;
    glm::mat4 modelview;
    glm::mat4 iTransform;
    glm::vec4 color;

    typedef enum {
        BLEND_OPACITY = 0,
        BLEND_SCREEN,
        BLEND_SUBTRACT,
        BLEND_MULTIPLY,
        BLEND_SOFT_LIGHT,
        BLEND_HARD_LIGHT,
        BLEND_SOFT_SUBTRACT,
        BLEND_LIGHTEN_ONLY,
        BLEND_NONE
    } BlendMode;
    BlendMode blending;

    static bool force_blending_opacity;

protected:
    ShadingProgram *program_;
};


#endif /* __SHADER_H_ */
