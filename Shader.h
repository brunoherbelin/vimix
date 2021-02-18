#ifndef __SHADER_H_
#define __SHADER_H_

#include <string>
#include <vector>
#include <glm/glm.hpp>

// Forward declare classes referenced
class Visitor;
class FrameBuffer;

class ShadingProgram
{
public:
    ShadingProgram(const std::string& vertex_file, const std::string& fragment_file);
    void init();
    bool initialized();
    void use();
	template<typename T> void setUniform(const std::string& name, T val);
	template<typename T> void setUniform(const std::string& name, T val1, T val2);
	template<typename T> void setUniform(const std::string& name, T val1, T val2, T val3);

	static void enduse();

private:
    void checkCompileErr();
    void checkLinkingErr();
    void compile();
    void link();
    unsigned int vertex_id_, fragment_id_, id_;
    std::string vertex_code_;
    std::string fragment_code_;
    std::string vertex_file_;
    std::string fragment_file_;

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

    void operator = (const Shader &D );

    glm::mat4 projection;
    glm::mat4 modelview;
    glm::mat4 iTransform;
    glm::vec4 color;

    typedef enum {
        BLEND_OPACITY = 0,
        BLEND_SCREEN,
        BLEND_SUBSTRACT,
        BLEND_MULTIPLY,
        BLEND_SOFT_LIGHT,
        BLEND_SOFT_SUBTRACT,
        BLEND_CUSTOM
    } BlendMode;
    BlendMode blending;

    static bool force_blending_opacity;

protected:
    ShadingProgram *program_;

};


#endif /* __SHADER_H_ */
