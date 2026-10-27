/*
 * shader.cxx - OpenGL 3 core profile shader program wrapper.
 */
#include "shader.h"

#include <cstdio>
#include <fstream>
#include <sstream>

static std::string readFile(const std::string &path)
{
	std::ifstream in(path);
	if (!in)
	{
		fprintf(stderr, "Cannot open shader file %s\n", path.c_str());
		return std::string();
	}
	std::ostringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

static bool shaderStatus(GLuint object)
{
	int ok = 0;
	glGetShaderiv(object, GL_COMPILE_STATUS, &ok);
	if (!ok)
	{
		char log[2048];
		glGetShaderInfoLog(object, sizeof(log), nullptr, log);
		fprintf(stderr, "Shader compile error: %s\n", log);
	}
	return ok != 0;
}

static bool programStatus(GLuint program)
{
	int ok = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &ok);
	if (!ok)
	{
		char log[2048];
		glGetProgramInfoLog(program, sizeof(log), nullptr, log);
		fprintf(stderr, "Shader link error: %s\n", log);
	}
	return ok != 0;
}

Shader::~Shader()
{
	if (program)
		glDeleteProgram(program);
}

bool Shader::createFromSources(const std::string &vertexSource, const std::string &fragmentSource)
{
	if (vertexSource.empty() || fragmentSource.empty())
		return false;

	if (program)
	{
		glDeleteProgram(program);
		program = 0;
		uniforms.clear();
	}

	const char *vsSource = vertexSource.c_str();
	const char *fsSource = fragmentSource.c_str();

	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vs, 1, &vsSource, nullptr);
	glCompileShader(vs);
	if (!shaderStatus(vs))
	{
		glDeleteShader(vs);
		return false;
	}

	GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fs, 1, &fsSource, nullptr);
	glCompileShader(fs);
	if (!shaderStatus(fs))
	{
		glDeleteShader(vs);
		glDeleteShader(fs);
		return false;
	}

	program = glCreateProgram();
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);

	glDeleteShader(vs);
	glDeleteShader(fs);

	return programStatus(program);
}

bool Shader::createFromFiles(const std::string &vertexFile, const std::string &fragmentFile)
{
	return createFromSources(readFile(vertexFile), readFile(fragmentFile));
}

void Shader::use() const
{
	if (program)
		glUseProgram(program);
}

void Shader::set(const char *name, float value)
{
	std::map<std::string, GLint>::iterator it = uniforms.find(name);
	if (it == uniforms.end())
	{
		GLint loc = glGetUniformLocation(program, name);
		it = uniforms.insert(std::make_pair(name, loc)).first;
	}
	if (it->second >= 0)
		glUniform1f(it->second, value);
}

void Shader::set(const char *name, int value)
{
	std::map<std::string, GLint>::iterator it = uniforms.find(name);
	if (it == uniforms.end())
	{
		GLint loc = glGetUniformLocation(program, name);
		it = uniforms.insert(std::make_pair(name, loc)).first;
	}
	if (it->second >= 0)
		glUniform1i(it->second, value);
}

void Shader::set(const char *name, float x, float y)
{
	std::map<std::string, GLint>::iterator it = uniforms.find(name);
	if (it == uniforms.end())
	{
		GLint loc = glGetUniformLocation(program, name);
		it = uniforms.insert(std::make_pair(name, loc)).first;
	}
	if (it->second >= 0)
		glUniform2f(it->second, x, y);
}

void Shader::set(const char *name, float x, float y, float z)
{
	std::map<std::string, GLint>::iterator it = uniforms.find(name);
	if (it == uniforms.end())
	{
		GLint loc = glGetUniformLocation(program, name);
		it = uniforms.insert(std::make_pair(name, loc)).first;
	}
	if (it->second >= 0)
		glUniform3f(it->second, x, y, z);
}

void Shader::set(const char *name, const Mat4 &matrix)
{
	std::map<std::string, GLint>::iterator it = uniforms.find(name);
	if (it == uniforms.end())
	{
		GLint loc = glGetUniformLocation(program, name);
		it = uniforms.insert(std::make_pair(name, loc)).first;
	}
	if (it->second >= 0)
		glUniformMatrix4fv(it->second, 1, GL_FALSE, matrix.m);
}
