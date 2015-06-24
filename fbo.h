
#include <GL/glew.h>
//#include <stdio.h>
//#include <unistd.h>
class FBO{
	public:
	int w;
	int h;
	GLuint fbo;
	GLuint texture;
	GLuint depthbuffer;
	void BindFBO(void);
	void UnBindFBO(void);
	void Create(int width,int height);
	
	
	
	
};
