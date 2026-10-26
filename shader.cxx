#include "shader.h"
#include <stdio.h>
#include <stdlib.h>
char *file2string(const char *path)
{
	FILE *fd;
	long len,r;
	char *str;
	if (!(fd = fopen(path,"r")))
	{
		fprintf(stderr,"Cannot open file %s\n",path);
		return NULL;
	}
	fseek(fd,0,SEEK_END);
	len = ftell(fd);
	printf("Shader %ld bytes\n",len);
	if(!(str = (char*)malloc((len+1)*sizeof(char))))
	{
		printf("Cannot allocate memory");
		fclose(fd);
		return NULL;
	}
	fseek(fd,0,SEEK_SET);
	r = fread(str,sizeof(char),len,fd);
	str[r] = '\0';
	fclose(fd);
	return str;
}
void PrintLog(GLuint obj)
{
	int inlogl = 0;
	char infoLog[1024];
	if (glIsShader(obj))
		glGetShaderInfoLog(obj,1024,&inlogl,infoLog);
	else
		glGetProgramInfoLog(obj,1024,&inlogl,infoLog);
	if (inlogl > 0)
		printf("%s\n",infoLog);
}

void shader::createfromfiles(const char * fsfile,const char * vsfile)
{
	const char * fss = file2string(fsfile);
	const char * vss = file2string(vsfile);
	if (GLEW_VERSION_2_0)
		printf("OpenGL 2.0\n");
	else
		printf("DANGER: No OpenGL 2.0\n");
	this->vs = glCreateShader(GL_VERTEX_SHADER);
	this->fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(this->vs,1,&vss,NULL);
	glShaderSource(this->fs,1,&fss,NULL);
	glCompileShader(this->vs);
	PrintLog(this->vs);
	glCompileShader(this->fs);
		PrintLog(this->fs);
	this->program = glCreateProgram();
	glAttachShader(this->program,this->vs);
	glAttachShader(this->program,this->fs);
	glLinkProgram(this->program);
	PrintLog(this->program);
	glUseProgram(0);
}
void shader::useshader(void)
{
	glUseProgram(this->program);
}
void shader::setuniformfloat(const char *uniname,float value)
{
	glUseProgram(this->program);
	GLint loc;
	loc = glGetUniformLocation(this->program,uniname);
	glUniform1f(loc,value);
	//glUseProgram(0);
}
void shader::setuniform2to4f(const char *uniname,float* value)
{
	glUseProgram(this->program);
	GLint loc;
	loc = glGetUniformLocation(this->program,uniname);
	/*if (sizeof(value) == 1)
		glUniform1f(loc,value[0]);
	if (sizeof(value) == 2)
		glUniform2f(loc,value[0],value[1]);
	if (sizeof(value) == 3)
		glUniform3f(loc,value[0],value[1],value[2]);
	if (sizeof(value) == 4)
		glUniform4f(loc,value[0],value[1],value[2],value[3]); EPIC FAIL*/
	glUseProgram(0);

}
void shader::setuniformsampler(const char *uniname,int value)
{
	glUseProgram(this->program);
	GLint loc;
	loc = glGetUniformLocation(this->program,uniname);
	glUniform1i(loc,value);

}
void shader::setuniformint(const char *uniname,int value)
{
    glUseProgram(this->program);
	GLint loc;
	loc = glGetUniformLocation(this->program,uniname);
	glUniform1i(loc,value);



}
