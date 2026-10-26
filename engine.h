#include <SDL3/SDL.h>
#include <GL/glew.h>
#include "font.h"
class engine{
    public:
	int height;
	int width;
	int bpp;
	float fps;
	float camx;
	float camy;
	float camz;
    float camrx;
	float camry;
	bool mouseleft;
	bool mouseright;
	bool mousecenter;
	GLdouble proj[16];
	GLint view[3];
	float camrz;
	int mousex;
	int mousey;
	int mouserelx;
	int mouserely;
	bool up;
	bool wup;
	bool down;
	bool wdown;
	bool left;
	bool right;
	float frustum[6][4];
	SDL_Event ev;
	Uint64 cleartime;
	Uint64 rendertime;
	bool fullscreen;
	bool quit;
	const bool *keys;
	public:
	oglstr textengine;
	SDL_Window* screen;
	SDL_GLContext context;
	bool getkey(SDL_Scancode key);
	float getfps(void);
	void ExtractFrustum();
	bool Init3d(int width = 800,int height = 600,int bpp = 24,bool fullscreen = false);
	void clearscreen(float r = 0.0,float g = 0.0,float b = 0.0,float a = 1.0);
	void PushTransform(float x,float y,float z,float rx,float ry,float rz);
	bool pointinview(float x, float y , float z);
	void Transform(float x,float y,float z,float rx,float ry,float rz);
	void SetCameraPos(float x,float y,float z);
	void OrthoMode();
	void NormalMode();
	void SetCameraRotation(float x,float y,float z);
	void Make2DPositionInto3DPosition (float x, float y, double *ox, double *oy, double *oz)
    {
        GLint viewport[4];
        GLdouble modelview[16],projection[16];
        GLfloat wx=x,wy,wz;

        glGetIntegerv(GL_VIEWPORT,viewport);
        y=viewport[3]-y;
        wy=y;
        glGetDoublev(GL_MODELVIEW_MATRIX,modelview);
        glGetDoublev(GL_PROJECTION_MATRIX,projection);
        glReadPixels(x,y,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&wz);
        gluUnProject(wx,wy,wz,modelview,projection,viewport,ox,oy,oz);
    }
	bool getwup()
	{
	  if ( wup )
	  {
	    wup = false;
	    return true;
	  }
	  return false;
	    
	  
	}
	bool getwdown()
	{
	  if ( wdown )
	  {
	    wdown = false;
	    return true;
	  }
	  return false;
	    
	  
	}
	int GetMouseX();
	int GetMouseY();
	void PopTransform();
	void display(void);
	bool Terminate(void);
	bool ToggleFullScreen(void);
};

