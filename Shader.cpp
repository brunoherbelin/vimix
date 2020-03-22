#include "Shader.h"
#include "Resource.h"

#include <fstream>
#include <sstream>
#include <iostream>

#include <glad/glad.h> 
#include <GLFW/glfw3.h>
#include <glm/glm.hpp> 
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

Shader::Shader() {
}

void Shader::load(const std::string& vertex_file, const std::string& fragment_file) {
	
	init(Resource::getText(vertex_file), Resource::getText(fragment_file));
}

void Shader::init(const std::string& vertex_code, const std::string& fragment_code) {
	vertex_code_ = vertex_code;
	fragment_code_ = fragment_code;
	compile();
	link();
}

void Shader::compile() {
	const char* vcode = vertex_code_.c_str();
	vertex_id_ = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex_id_, 1, &vcode, NULL);
	glCompileShader(vertex_id_);

	const char* fcode = fragment_code_.c_str();
	fragment_id_ = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment_id_, 1, &fcode, NULL);
	glCompileShader(fragment_id_);
	checkCompileErr();
}

void Shader::link() {
	id_ = glCreateProgram();
	glAttachShader(id_, vertex_id_);
	glAttachShader(id_, fragment_id_);
	glLinkProgram(id_);
	checkLinkingErr();
	glDeleteShader(vertex_id_);
	glDeleteShader(fragment_id_);
}

void Shader::use() {
    glUseProgram(id_);
}

void Shader::enduse() {
    glUseProgram(0);
}

template<>
void Shader::setUniform<int>(const std::string& name, int val) {
	glUniform1i(glGetUniformLocation(id_, name.c_str()), val);
}

template<>
void Shader::setUniform<bool>(const std::string& name, bool val) {
	glUniform1i(glGetUniformLocation(id_, name.c_str()), val);
}

template<>
void Shader::setUniform<float>(const std::string& name, float val) {
	glUniform1f(glGetUniformLocation(id_, name.c_str()), val);
}

template<>
void Shader::setUniform<glm::mat4>(const std::string& name, glm::mat4 val) {
	glm::mat4 m(val);
    // std::cout << glm::to_string(m) << std::endl;
	glUniformMatrix4fv(glGetUniformLocation(id_, name.c_str()), 1, GL_FALSE, glm::value_ptr(m));
}

template<>
void Shader::setUniform<float>(const std::string& name, float val1, float val2) {
	glUniform2f(glGetUniformLocation(id_, name.c_str()), val1, val2);
}

template<>
void Shader::setUniform<float>(const std::string& name, float val1, float val2, float val3) {
	glUniform3f(glGetUniformLocation(id_, name.c_str()), val1, val2, val3);
}

// template<>
// void Shader::setUniform<float*>(const std::string& name, float* val) {
// 	glUniformMatrix4fv(glGetUniformLocation(id_, name.c_str()), 1, GL_FALSE, val);
// }

void Shader::checkCompileErr() {
    int success;
    char infoLog[1024];
    glGetShaderiv(vertex_id_, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertex_id_, 1024, NULL, infoLog);
        std::cout << "Error compiling Vertex Shader:\n" << infoLog << std::endl;
		std::cout << vertex_code_ << std::endl;
    }
	glGetShaderiv(fragment_id_, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(fragment_id_, 1024, NULL, infoLog);
		std::cout << "Error compiling Fragment Shader:\n" << infoLog << std::endl;
		std::cout << fragment_code_ << std::endl;
	}
}

void Shader::checkLinkingErr() {
	int success;
	char infoLog[1024];
	glGetProgramiv(id_, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(id_, 1024, NULL, infoLog);
		std::cout << "Error Linking Shader Program:\n" << infoLog << std::endl;
	}
}
