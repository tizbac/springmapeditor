#include "engine.h"
#include <cmath>
#include <cstdio>
bool engine::Init3d(int width,int height,int bpp,bool fullscreen)
{
	
	//this->world = dWorldCreate();
	this->bpp = bpp;
	this->height = height;
	this->width = width;
	this->fullscreen = fullscreen;
	this->quit = false;

	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return false;
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE,5);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,5);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,5);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,16);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);

	this->screen = SDL_CreateWindow("Spring Map Editor", this->width, this->height,
	                                SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	if (this->screen == NULL)
	{
		fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
		return false;
	}

	this->context = SDL_GL_CreateContext(this->screen);
	if (this->context == NULL)
	{
		fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
		return false;
	}

	SDL_GL_SetSwapInterval(1);

	if (this->fullscreen)
		SDL_SetWindowFullscreen(this->screen, true);

	glewExperimental = GL_TRUE;
	GLenum glewStatus = glewInit();
	if (glewStatus != GLEW_OK)
	{
		fprintf(stderr, "glewInit failed: %s\n", glewGetErrorString(glewStatus));
		return false;
	}
	// GLEW can leave a benign GL_INVALID_ENUM behind on core-ish contexts.
	while (glGetError() != GL_NO_ERROR) {}
	glShadeModel(GL_SMOOTH);
	glEnable(GL_DEPTH_TEST);//Abilita il controllo della profondità
	//glEnable(GL_CULL_FACE);//Abilita il rendering solo per le facce frontali
	glCullFace(GL_BACK);
	//glEnable(GL_COLOR_MATERIAL);//Abilita i materiali personalizzati
	glClearColor(0.0,0.0,0.0,1);
	glEnable(GL_LIGHTING);//Illuminazione
	glDisable(GL_TEXTURE_2D);//texture
	glEnable(GL_NORMALIZE);
	glShadeModel(GL_SMOOTH);//Goraud shading
	GLfloat h = (GLfloat) this->height / (GLfloat) this->width;
	glViewport(0,0,this->width,this->height);
	glGetIntegerv(GL_VIEWPORT,this->view);
	glMatrixMode(GL_PROJECTION);//Imposta la proiezione
	glLoadIdentity();

	gluPerspective(45.0f,(GLfloat)this->width/(GLfloat)this->height,0.1f,1000.0f);
	this->up = false;
	this->down = false;
	this->left = false;
	this->right = false;
	this->mouseleft = false;
	this->mouseright = false;
	glMatrixMode(GL_MODELVIEW);
	glGetDoublev(GL_PROJECTION_MATRIX, this->proj);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT,GL_NICEST);
	//glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glPolygonOffset(-3.0f, -3.0f);
	glLoadIdentity();
	return true;
}
void engine::ExtractFrustum()
{
	float   proj[16];
	float   modl[16];
	float   clip[16];
	float   t;

	/* Get the current PROJECTION matrix from OpenGL */
	glGetFloatv( GL_PROJECTION_MATRIX, proj );

	/* Get the current MODELVIEW matrix from OpenGL */
	glGetFloatv( GL_MODELVIEW_MATRIX, modl );

	/* Combine the two matrices (multiply projection by modelview) */
	clip[ 0] = modl[ 0] * proj[ 0] + modl[ 1] * proj[ 4] + modl[ 2] * proj[ 8] + modl[ 3] * proj[12];
	clip[ 1] = modl[ 0] * proj[ 1] + modl[ 1] * proj[ 5] + modl[ 2] * proj[ 9] + modl[ 3] * proj[13];
	clip[ 2] = modl[ 0] * proj[ 2] + modl[ 1] * proj[ 6] + modl[ 2] * proj[10] + modl[ 3] * proj[14];
	clip[ 3] = modl[ 0] * proj[ 3] + modl[ 1] * proj[ 7] + modl[ 2] * proj[11] + modl[ 3] * proj[15];

	clip[ 4] = modl[ 4] * proj[ 0] + modl[ 5] * proj[ 4] + modl[ 6] * proj[ 8] + modl[ 7] * proj[12];
	clip[ 5] = modl[ 4] * proj[ 1] + modl[ 5] * proj[ 5] + modl[ 6] * proj[ 9] + modl[ 7] * proj[13];
	clip[ 6] = modl[ 4] * proj[ 2] + modl[ 5] * proj[ 6] + modl[ 6] * proj[10] + modl[ 7] * proj[14];
	clip[ 7] = modl[ 4] * proj[ 3] + modl[ 5] * proj[ 7] + modl[ 6] * proj[11] + modl[ 7] * proj[15];

	clip[ 8] = modl[ 8] * proj[ 0] + modl[ 9] * proj[ 4] + modl[10] * proj[ 8] + modl[11] * proj[12];
	clip[ 9] = modl[ 8] * proj[ 1] + modl[ 9] * proj[ 5] + modl[10] * proj[ 9] + modl[11] * proj[13];
	clip[10] = modl[ 8] * proj[ 2] + modl[ 9] * proj[ 6] + modl[10] * proj[10] + modl[11] * proj[14];
	clip[11] = modl[ 8] * proj[ 3] + modl[ 9] * proj[ 7] + modl[10] * proj[11] + modl[11] * proj[15];

	clip[12] = modl[12] * proj[ 0] + modl[13] * proj[ 4] + modl[14] * proj[ 8] + modl[15] * proj[12];
	clip[13] = modl[12] * proj[ 1] + modl[13] * proj[ 5] + modl[14] * proj[ 9] + modl[15] * proj[13];
	clip[14] = modl[12] * proj[ 2] + modl[13] * proj[ 6] + modl[14] * proj[10] + modl[15] * proj[14];
	clip[15] = modl[12] * proj[ 3] + modl[13] * proj[ 7] + modl[14] * proj[11] + modl[15] * proj[15];

	/* Extract the numbers for the RIGHT plane */
	frustum[0][0] = clip[ 3] - clip[ 0];
	frustum[0][1] = clip[ 7] - clip[ 4];
	frustum[0][2] = clip[11] - clip[ 8];
	frustum[0][3] = clip[15] - clip[12];

	/* Normalize the result */
	t = sqrt( frustum[0][0] * frustum[0][0] + frustum[0][1] * frustum[0][1] + frustum[0][2] * frustum[0][2] );
	frustum[0][0] /= t;
	frustum[0][1] /= t;
	frustum[0][2] /= t;
	frustum[0][3] /= t;

	/* Extract the numbers for the LEFT plane */
	frustum[1][0] = clip[ 3] + clip[ 0];
	frustum[1][1] = clip[ 7] + clip[ 4];
	frustum[1][2] = clip[11] + clip[ 8];
	frustum[1][3] = clip[15] + clip[12];

	/* Normalize the result */
	t = sqrt( frustum[1][0] * frustum[1][0] + frustum[1][1] * frustum[1][1] + frustum[1][2] * frustum[1][2] );
	frustum[1][0] /= t;
	frustum[1][1] /= t;
	frustum[1][2] /= t;
	frustum[1][3] /= t;

	/* Extract the BOTTOM plane */
	frustum[2][0] = clip[ 3] + clip[ 1];
	frustum[2][1] = clip[ 7] + clip[ 5];
	frustum[2][2] = clip[11] + clip[ 9];
	frustum[2][3] = clip[15] + clip[13];

	/* Normalize the result */
	t = sqrt( frustum[2][0] * frustum[2][0] + frustum[2][1] * frustum[2][1] + frustum[2][2] * frustum[2][2] );
	frustum[2][0] /= t;
	frustum[2][1] /= t;
	frustum[2][2] /= t;
	frustum[2][3] /= t;

	/* Extract the TOP plane */
	frustum[3][0] = clip[ 3] - clip[ 1];
	frustum[3][1] = clip[ 7] - clip[ 5];
	frustum[3][2] = clip[11] - clip[ 9];
	frustum[3][3] = clip[15] - clip[13];

	/* Normalize the result */
	t = sqrt( frustum[3][0] * frustum[3][0] + frustum[3][1] * frustum[3][1] + frustum[3][2] * frustum[3][2] );
	frustum[3][0] /= t;
	frustum[3][1] /= t;
	frustum[3][2] /= t;
	frustum[3][3] /= t;

	/* Extract the FAR plane */
	frustum[4][0] = clip[ 3] - clip[ 2];
	frustum[4][1] = clip[ 7] - clip[ 6];
	frustum[4][2] = clip[11] - clip[10];
	frustum[4][3] = clip[15] - clip[14];

	/* Normalize the result */
	t = sqrt( frustum[4][0] * frustum[4][0] + frustum[4][1] * frustum[4][1] + frustum[4][2] * frustum[4][2] );
	frustum[4][0] /= t;
	frustum[4][1] /= t;
	frustum[4][2] /= t;
	frustum[4][3] /= t;

	/* Extract the NEAR plane */
	frustum[5][0] = clip[ 3] + clip[ 2];
	frustum[5][1] = clip[ 7] + clip[ 6];
	frustum[5][2] = clip[11] + clip[10];
	frustum[5][3] = clip[15] + clip[14];

	/* Normalize the result */
	t = sqrt( frustum[5][0] * frustum[5][0] + frustum[5][1] * frustum[5][1] + frustum[5][2] * frustum[5][2] );
	frustum[5][0] /= t;
	frustum[5][1] /= t;
	frustum[5][2] /= t;
	frustum[5][3] /= t;
}
bool engine::pointinview( float x, float y, float z )
{
	int p;

	for( p = 0; p < 6; p++ )
		if( frustum[p][0] * x + frustum[p][1] * y + frustum[p][2] * z + frustum[p][3] <= 0 )
			return false;
	return true;
}
void engine::clearscreen(float r,float g,float b,float a)
{
	this->cleartime = SDL_GetTicks();
	glClearColor(r,g,b,a);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    this->mouserelx = 0;
    this->mouserely = 0;

	while ( SDL_PollEvent(&this->ev)){
        switch (this->ev.type)
        {
            case SDL_EVENT_MOUSE_MOTION:
                this->mousex = (int)ev.motion.x;
                this->mousey = (int)ev.motion.y;
                this->mouserelx = (int)ev.motion.xrel;
                this->mouserely = (int)ev.motion.yrel;
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (ev.button.button == SDL_BUTTON_LEFT)
                    this->mouseleft = ev.button.down;
                else if (ev.button.button == SDL_BUTTON_RIGHT)
                    this->mouseright = ev.button.down;
                else if (ev.button.button == SDL_BUTTON_MIDDLE)
                    this->mousecenter = ev.button.down;
                break;

            case SDL_EVENT_MOUSE_WHEEL:
                this->wup = ev.wheel.y > 0;
                this->wdown = ev.wheel.y < 0;
                break;

            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
                switch (ev.key.scancode) {
                case SDL_SCANCODE_LEFT:
                    this->left = ev.key.down;
                    break;
                case SDL_SCANCODE_RIGHT:
                    this->right = ev.key.down;
                    break;
                case SDL_SCANCODE_UP:
                    this->up = ev.key.down;
                    break;
                case SDL_SCANCODE_DOWN:
                    this->down = ev.key.down;
                    break;
                default:
                    break;
                }
                break;

            case SDL_EVENT_QUIT:
                this->quit = true;
                break;

            default:
                break;
        }
	}

}
bool engine::getkey(SDL_Scancode key)
{

	this->keys = SDL_GetKeyboardState(NULL);
	return this->keys[key];
}
void engine::display(void)
{
	SDL_GL_SwapWindow(screen);
	this->rendertime = SDL_GetTicks()-this->cleartime;
}
bool engine::Terminate(void)
{
	if (this->context)
	{
		SDL_GL_DestroyContext(this->context);
		this->context = NULL;
	}
	if (this->screen)
	{
		SDL_DestroyWindow(this->screen);
		this->screen = NULL;
	}
	SDL_Quit();
	return true;
}
bool engine::ToggleFullScreen(void)
{
	if (this->screen == NULL)
		return false;

	this->fullscreen = !this->fullscreen;
	SDL_SetWindowFullscreen(this->screen, this->fullscreen);
	return true;
}
float engine::getfps(void)
{
	float tmp = 0.0;
	tmp = (this->rendertime > 0) ? 1000.0f / rendertime : 0.0f;
	return tmp;
}
void engine::PushTransform(float x,float y,float z,float rx,float ry,float rz)
{


		glPushMatrix();
		glTranslatef(x,y,z);
		glRotatef(rx,1,0,0);
		glRotatef(ry,0,1,0);
		glRotatef(rz,0,0,1);


}
void engine::Transform(float x,float y,float z,float rx,float ry,float rz)
{



		glTranslatef(x,y,z);
		glRotatef(rx,1,0,0);
		glRotatef(ry,0,1,0);
		glRotatef(rz,0,0,1);


}
void engine::PopTransform()
{
  glPopMatrix();

}
void engine::SetCameraPos(float x,float y,float z)
{


    this->camx = x;
    this->camy = y;
    this->camz = z;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
	gluPerspective(45.0f,(GLfloat)this->width/(GLfloat)this->height,0.1f,1000.0f);
    glRotatef(this->camrx,1.0,0.0,0.0);
    glRotatef(this->camry,0.0,1.0,0.0);
    glRotatef(this->camrz,0.0,0.0,1.0);
    glTranslatef(this->camx,this->camy,this->camz);
    glGetDoublev(GL_PROJECTION_MATRIX, this->proj);
    glMatrixMode(GL_MODELVIEW);



}
void engine::SetCameraRotation(float x,float y,float z)
{

    this->camrx = x;
    this->camry = y;
    this->camrz = z;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
	gluPerspective(45.0f,(GLfloat)this->width/(GLfloat)this->height,0.1f,1000.0f);
    glRotatef(this->camrx,1.0,0.0,0.0);
    glRotatef(this->camry,0.0,1.0,0.0);
    glRotatef(this->camrz,0.0,0.0,1.0);
    glTranslatef(this->camx,this->camy,this->camz);
    glGetDoublev(GL_PROJECTION_MATRIX, this->proj);
    glMatrixMode(GL_MODELVIEW);


}
int engine::GetMouseX()
{
	return this->mousex;
}
int engine::GetMouseY()
{
	return this->mousey;
}
void engine::OrthoMode()
{
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
  gluOrtho2D(0,this->width,0,this->height);
  glMatrixMode(GL_MODELVIEW);
  glDisable(GL_LIGHTING);
  ExtractFrustum();
}
void engine::NormalMode()
{
 glEnable(GL_LIGHTING);
   glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(45.0f,(GLfloat)this->width/(GLfloat)this->height,0.1f,1000.0f);
  glRotatef(this->camrx,1.0,0.0,0.0);
  glRotatef(this->camry,0.0,1.0,0.0);
  glRotatef(this->camrz,0.0,0.0,1.0);
  glTranslatef(-this->camx,-this->camy,-this->camz);
  
  glGetDoublev(GL_PROJECTION_MATRIX, this->proj);
  glMatrixMode(GL_MODELVIEW);
    ExtractFrustum();
}
