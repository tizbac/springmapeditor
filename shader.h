#include <GL/glew.h>
#include <iostream>
class shader{
	GLuint program;
	GLuint fs;
	GLuint vs;
	public:
	void useshader(void);
	void setuniformfloat(char *uniname,float value);
	void setuniform2to4f(char *uniname,float* value);
	void setuniformsampler(char *uniname,int value);
	void setuniformint(char *uniname,int value);
	void createfromfiles(char * fsfile,char * vsfile);
};
