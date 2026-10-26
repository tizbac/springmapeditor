#include <GL/glew.h>
#include <iostream>
class shader{
	GLuint program;
	GLuint fs;
	GLuint vs;
	public:
	void useshader(void);
	void setuniformfloat(const char *uniname,float value);
	void setuniform2to4f(const char *uniname,float* value);
	void setuniformsampler(const char *uniname,int value);
	void setuniformint(const char *uniname,int value);
	void createfromfiles(const char * fsfile,const char * vsfile);
};
