#include "engine.h"
#include <cmath>
bool engine::Init3d(int width,int height,int bpp,bool fullscreen)
{
	//this->world = dWorldCreate();
	this->bpp = bpp;
	this->height = height;
	this->width = width;
	this->fullscreen = fullscreen;
	SDL_Init(SDL_INIT_VIDEO);
	this->screen = SDL_SetVideoMode(this->width,this->height,this->bpp,SDL_OPENGL);//Avvia SDL
	glewInit();//Avvia GLEW per poter utilizzare GLSL
	if (this->fullscreen)
		SDL_WM_ToggleFullScreen(this->screen);

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE,5);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,5);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,5);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,16);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
	this->textengine.initfont();
	this->textengine.disph = height;
	this->textengine.dispw = width;
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
            case SDL_MOUSEMOTION:
                this->mousex = ev.motion.x;
                this->mousey = ev.motion.y;
                this->mouserelx = ev.motion.xrel;
                this->mouserely = ev.motion.yrel;
                break;
	    
            case SDL_MOUSEBUTTONDOWN:
		this->wup = ev.button.button == SDL_BUTTON_WHEELUP;
		this->wdown = ev.button.button == SDL_BUTTON_WHEELDOWN; 
	
	     
                this->mouseleft = ev.button.button == SDL_BUTTON_LEFT && ev.button.state == SDL_PRESSED;
                this->mouseright = ev.button.button == SDL_BUTTON_RIGHT && ev.button.state == SDL_PRESSED;
                break;
            case SDL_MOUSEBUTTONUP:
                this->mouseleft = ev.button.button == SDL_BUTTON_LEFT && ev.button.state == SDL_PRESSED;
                this->mouseright = ev.button.button == SDL_BUTTON_RIGHT && ev.button.state == SDL_PRESSED;
                break;

             case SDL_KEYDOWN:
             // printf("keydown %i\n" , ev.key.keysym.sym);
              switch(ev.key.keysym.sym){
                case SDLK_LEFT:
                   this->left = ev.key.state == SDL_PRESSED;
                  break;
                case SDLK_RIGHT:
                  this->right = ev.key.state == SDL_PRESSED;
                  break;
                case SDLK_UP:
                  this->up = ev.key.state == SDL_PRESSED;
                  break;
                case SDLK_DOWN:
                  this->down = ev.key.state == SDL_PRESSED;
                  break;
                default:
                  break;}
            case SDL_KEYUP:
             // printf("keydown %i\n" , ev.key.keysym.sym);
              switch(ev.key.keysym.sym){
                case SDLK_LEFT:
                   this->left = ev.key.state == SDL_PRESSED;
                  break;
                case SDLK_RIGHT:
                  this->right = ev.key.state == SDL_PRESSED;
                  break;
                case SDLK_UP:
                  this->up = ev.key.state == SDL_PRESSED;
                  break;
                case SDLK_DOWN:
                  this->down = ev.key.state == SDL_PRESSED;
                  break;
                default:
                  break;}


        }
	}

}
bool engine::getkey(char key)
{

	this->keys = SDL_GetKeyState(NULL);
	if (this->keys[key])
	{
		return true;
	}else{
		return false;
	}
}
void engine::display(void)
{
	SDL_GL_SwapBuffers();
	this->rendertime = SDL_GetTicks()-this->cleartime;
}
bool engine::Terminate(void)
{
	SDL_Quit();
	//dWorldDestroy(this->world);
	return true;
}
bool engine::ToggleFullScreen(void)
{
	if (this->screen == NULL)
		return false;

	SDL_WM_ToggleFullScreen(this->screen);
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




}
int engine::GetMouseY()
{






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
