/*
 * shader.h - OpenGL 3 core profile shader program wrapper.
 */
#ifndef SHADER_H
#define SHADER_H

#include <GL/glew.h>

#include <map>
#include <string>

#include "mat4.h"

class Shader {
	GLuint program = 0;
	std::map<std::string, GLint> uniforms;

  public:
	~Shader();

	// Returns false and prints the compiler/linker log on failure.
	bool createFromFiles(const std::string &vertexFile, const std::string &fragmentFile);
	bool createFromSources(const std::string &vertexSource, const std::string &fragmentSource);

	GLuint programId() const
	{
		return program;
	}
	void use() const;

	void set(const char *name, float value);
	void set(const char *name, int value);
	void set(const char *name, float x, float y);
	void set(const char *name, float x, float y, float z);
	void set(const char *name, const Mat4 &matrix);
};

#endif // SHADER_H
