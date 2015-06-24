#include "fbo.h"
#include <stdio.h>
void FBO::Create(int  width,int height)
{
	glGenTextures(1,&this->texture);
	glBindTexture(GL_TEXTURE_2D,this->texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,  width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glGenerateMipmapEXT(GL_TEXTURE_2D);

	glGenFramebuffersEXT(1, &this->fbo);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, this->fbo);
	glGenRenderbuffersEXT(1, &this->depthbuffer);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, this->depthbuffer);
	
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, this->texture, 0);
	glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT, width,height);
	this->w = width;
	this->h = height;
}
void FBO::BindFBO(void)
{
	printf("BindFBO\n");
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, this->fbo);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, this->depthbuffer);
	
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, this->depthbuffer);
	glPushAttrib(GL_VIEWPORT_BIT);
	glViewport(0,0,this->w, this->h);
	printf("OK BindFBO\n");


	
}
void FBO::UnBindFBO(void)
{
	printf("UnBindFBO\n");
	glPopAttrib();

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	printf("OK UnBindFBO\n");
	
}