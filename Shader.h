#ifndef __SHADER_H_
#define __SHADER_H_

#include <string>
#include <vector>
#include <glm/glm.hpp>

// Forward declare classes referenced
class Visitor;

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
    int       id_;

public:
    Shader();
//    virtual ~Shader() {}

    // unique identifyer generated at instanciation
    inline int id () const { return id_; }

    virtual void use();
    virtual void reset();
    virtual void accept(Visitor& v);

    glm::mat4 projection;
    glm::mat4 modelview;
    glm::vec4 color;

    typedef enum {
        BLEND_OPACITY = 0,
        BLEND_ADD,
        BLEND_SUBSTRACT,
        BLEND_LAYER_ADD,
        BLEND_LAYER_SUBSTRACT,
        BLEND_CUSTOM
    } BlendMode;
    BlendMode blending;

protected:
    ShadingProgram *program_;
    glm::vec3 iResolution;

};


#endif /* __SHADER_H_ */
