#ifndef __SHADER_H_
#define __SHADER_H_

#include <string>
#include <vector>
#include <glm/glm.hpp>

class ShadingProgram
{
public:
    ShadingProgram();
	void init(const std::string& vertex_code, const std::string& fragment_code);
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

    static ShadingProgram *_currentProgram;
};

class Shader
{
public:
    Shader();
    virtual ~Shader() {}

    virtual void use();
    virtual void reset();

    glm::mat4 modelview;
    glm::vec4 color;

    void setModelview(float x, float y, float angle, float scale, float aspect_ratio);

protected:
    ShadingProgram program_;
    std::string vertex_file;
    std::string fragment_file;
};

#endif /* __SHADER_H_ */
