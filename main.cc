/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * main.c
 * Copyright (C) Tiziano 2008 <tiziano@tizbac.dyndns.org>
 *
 * main.cpp is free software.
 *
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * main.cpp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with main.cpp.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */
#define flt(x) ((float)x)
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <string>
#include <algorithm>
#include "GlobalStuff.h"
#include <stdarg.h>
#include <float.h>
#include "engine.h"

#include <cmath>
#include "light.h"
#include "shader.h"
#include <GL/glew.h>
#include "Map/SMF/BFGroundTextures.h"
#include "Map/SMF/SmfReadMap.h"
#include "Map/ReadMap.h"
#include <FTGL/ftgl.h>
static engine* Main;
light l0;
gst * gs;
float rxx = 0;
float mappx;
float mappy;
bool lastmr = false;
float ryy = 93.3;
float ts = 6.0;
int mode;
double mx = 0;
double my = 0;
double mz = 0;
GLuint *metaltexture;
float rzz = 246.0;
float pxx = -20.37;
unsigned char * tmpdata;
float pyy = 14.39;
float pzz = 4.32;
unsigned char  *metalmap ;
float brushsize = 1.0;
GLint * landscapecache;
shader groundshader;
char * landscapecachestatus;
class v3{
    public:
        float x;
        float z;
        float y;
        v3()
        {
            this->x = 0;
            this->y = 0;
            this->z = 0;
        }
        v3(float x,float y,float z)
        {
            this->x = x;
            this->y = y;
            this->z = z;

        }
        v3 crossproduct(v3 o2)
        {
            v3 res;
            res.x = y*o2.z-z*o2.y;
            res.y = z*o2.x-x*o2.z;
            res.z = x*o2.y-y*o2.x;
            return (res);

        }
        v3 operator+(v3 o2)
        {
            v3 res;
            res.x = x + o2.x;
            res.y = y + o2.y;
            res.z = z + o2.z;
            return (res);
        }
        v3 operator-(v3 o2)
        {
            v3 res;
            res.x = x - o2.x;
            res.y = y - o2.y;
            res.z = z - o2.z;
            return (res);
        }
        v3 operator*(float m)
        {
            v3 res;
            res.x = x*m;
            res.y = y*m;
            res.z = z*m;
            return (res);
        }
        v3 operator*(v3 o2)
        {
            v3 res;
            res.x = x*o2.x;
            res.y = y*o2.y;
            res.z = z*o2.z;
            return (res);
        }
        float mag()
        {
            return sqrt(x*x+y*y);
        }
};
float GetH(CSmfReadMap* map,int x, int y)
{
  if (x < 0 or y < 0){
    //printf("GetH(%i,%i) : Invalid coords\n" , x,y);
    return 0.0;}
  if (x > gs->mapx or y > gs->mapy){
    //printf("GetH(%i,%i) : Invalid coords\n" , x,y);
    return 0.0;
  }
  int i = (gs->mapx+1)*(y)+(x);
  //printf("GetH(%i,%i) = %f\n" , x,y,map->heightmap[(gs->mapx+1)*y+x]);
  float f = map->heightmap[i];
  if ( f > 20)
    f = 0.0;
  return f;

}
void SetH(CSmfReadMap* map,int x, int y,float value)
{
  if (x < 0 or y < 0){
    //printf("GetH(%i,%i) : Invalid coords\n" , x,y);
    return;}
  if (x > gs->mapx  or y > gs->mapy){
    //printf("SetH(%i,%i) : Invalid coords\n" , x,y);
    return;
  }
  landscapecachestatus[(x-1)*8/1024+(y-1)*8/1024*(gs->mapx+1)*8/1024] = 'u';
  int i = (gs->mapx+1)*(y)+(x);
  //printf("GetH(%i,%i) = %f\n" , x,y,map->heightmap[(gs->mapx+1)*y+x]);
  map->heightmap[i] = value;



}
float GetM(CSmfReadMap* map,int x_, int y_)
{
    int x = x_ /2;
 int y = y_/2;
  if (x < 0 or y < 0){
    //printf("GetM(%i,%i) : Invalid coords\n" , x,y);
    return 0.0;}
  if (x > map->width/2 -1  or y > map->height/2 -1){
    //printf("GetM(%i,%i) : Invalid coords\n" , x,y);
    return 0.0;
  }
  int i = (map->width/2)*(y)+(x);
  //printf("GetH(%i,%i) = %f\n" , x,y,map->heightmap[(gs->mapx+1)*y+x]);
  float f = (int)metalmap[i]/255.0;
 /* if ( f > 0.0)
        printf("GetM(%i,%i) : Out of range: %f\n" , x,y,f);*/
  if ( f > 256)
  {


    f = 0.0;}
  return f;

}
void SetM(CSmfReadMap* map,int x_, int y_,float value)
{
 int x = x_ /2;
 int y = y_ /2;
  if (x < 0 or y < 0){
    //printf("GetH(%i,%i) : Invalid coords\n" , x,y);
    return;}
  if (x > map->width/2 -1 or y > map->height/2 -1){
    //printf("SetH(%i,%i) : Invalid coords\n" , x,y);
    return;
  }
  landscapecachestatus[(x*2)*8/1024+(y*2)*8/1024*(gs->mapx)*8/1024] = 'u';
  int i = (map->width/2)*(y)+(x);
  //printf("GetH(%i,%i) = %f\n" , x,y,map->heightmap[(gs->mapx+1)*y+x]);
  metalmap[i] = max(0.0,min(255.0,value*255.0));



}
float pointdist3d(float ax,float ay,float az,float bx,float by,float bz)
{

  return sqrt((ax-bx)*(ax-bx)+(ay-by)*(ay-by)+(az-bz)*(az-bz));
}
float pointdist2d(float ax,float ay,float bx,float by)
{

    return sqrt((ax-bx)*(ax-bx)+(ay-by)*(ay-by));
}
struct MapHeader {
	char magic[16]; //"spring map file\0"
	int version;		//must be 1 for now
	int mapid;			//sort of a checksum of the file

	int mapx;				//must be divisible by 128
	int mapy;				//must be divisible by 128
	int squareSize;	//distance between vertices. must be 8
	int texelPerSquare;		//number of texels per square, must be 8 for now
	int tilesize;		//number of texels in a tile, must be 32 for now
	float minHeight;		//height value that 0 in the heightmap corresponds to
	float maxHeight;		//height value that 0xffff in the heightmap corresponds to

	int heightmapPtr;		//file offset to elevation data (short int[(mapy+1)*(mapx+1)])
	int typeMapPtr;			//file offset to typedata (unsigned char[mapy/2 * mapx/2])
	int tilesPtr;				//file offset to tile data (see MapTileHeader)
	int minimapPtr;			//file offset to minimap (always 1024*1024 dxt1 compresed data with 9 mipmap sublevels)
	int metalmapPtr;		//file offset to metalmap (unsigned char[mapx/2 * mapy/2])
	int featurePtr;			//file offset to feature data	(see MapFeatureHeader)

	int numExtraHeaders;		//numbers of extra headers following main header
};
void drawsquare(int x, int y,CSmfReadMap* map,CBFGroundTextures* tex,float tilesize)
{
    unsigned char tval;
  int X,Y = 0;
  float cx,cy,z1,z2,z3,z4,y1,y2,x1,x2,sqsize;
  Main->PushTransform(x*2,y*2,0,0,0,0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D,tex->squares[y*(map->width*8/1024)+x].texture);
  groundshader.setuniformint("maintex",0);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D,metaltexture[y*map->width*8/1024+x]);
  groundshader.setuniformint("metaltex",1);
  glActiveTexture(GL_TEXTURE0);
   groundshader.setuniformfloat("scale",1.0);
                 groundshader.setuniformfloat("tx",0);
                  groundshader.setuniformfloat("ty",0);
 groundshader.setuniformint("mode",mode);


  switch ( mode ){
      case 0: //Main texture

        glMatrixMode(GL_TEXTURE);
        glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);
        break;
      case 1: // Metal map


         glDisable(GL_LIGHTING);
        /*glMatrixMode(GL_TEXTURE);
        glLoadIdentity();
        glScalef(1.0/map->width/2,1.0/map->height/2,1.0/map->height/2);
        glTranslatef((float)x*(1.0/map->width/2*map->width/2),(float)y*(1.0/map->height/2*map->height/2),0);
        glMatrixMode(GL_MODELVIEW);*/
        break;

  }
            /*if ( tilesize != pow(2,max(4.0,(double)(min(10,(int)(pointdist2d(x*2+1,y*2+1,-pxx,-pzz)))))))
            {
			 tilesize = pow(2,max(4.0,(double)(min(10,3*(int)(pointdist2d(x*2+1,y*2+1,-pxx,-pzz))))));
			 for ( int i = 0; i < map->height*8/1024*map->width*8/1024; i++)
                landscapecachestatus[i] = 'u';
            }*/
            tilesize = pow(2,ts);
                if (Main->pointinview(x*2,y*2,0) || Main->pointinview(x*2+2,y*2+2,0) || Main->pointinview(x*2+2,y*2,0) || Main->pointinview(x*2,y*2+2,0))
                {
            if ( landscapecachestatus[y*map->width*8/1024+x] == 'e' or landscapecachestatus[y*map->width*8/1024+x] == 'u' )
            {
            if (mode == 1)
            {
            int mmapsqsize = (map->header.mapx/2)/(map->width*8/1024);
            //printf("Generating metal texture[%i]...\n",y*map->width*8/1024+x);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D,metaltexture[y*map->width*8/1024+x]);

            for ( int yb = 0; yb < mmapsqsize && yb + y < map->height/2; yb++)
            {
                for ( int xb = 0; xb < mmapsqsize && xb + x < map->width/2; xb++)
                {
                   // printf("Read metalmap[%i]\n",(map->header.mapy/2*(ya*1024/8+yb))+(xa+xb));
                    //tval = metalmap[(map->width/2*(y*1024/8+yb))+(x*1024/8+xb)];
                    tval = (unsigned char)(255.0*GetM(map,x*1024/8+xb*2,y*1024/8+yb*2));
                   // printf("Write tmpdata[%i] = %i\n",mmapsqsize*yb+xb,(int)tval);
                    tmpdata[mmapsqsize*yb+xb] = tval;

                }
            }
            gluBuild2DMipmaps(GL_TEXTURE_2D,1,mmapsqsize,mmapsqsize,GL_LUMINANCE,GL_UNSIGNED_BYTE,tmpdata);
            glActiveTexture(GL_TEXTURE0);
            //printf("Generated metal textures succesfully\n");
            }
            if ( mode == 0)
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D,tex->squares[y*(map->width*8/1024)+x].texture);

             glNewList(landscapecache[y*map->width*8/1024+x],GL_COMPILE_AND_EXECUTE);
			  //printf("tilesize=%f\n" , tilesize);
			  if ( mode == 1 )
                {

                }
			  for ( Y = 0; Y < 1024.0/tilesize; Y++)
			    for ( X = 0; X < 1024.0/tilesize; X++)
			    {
			      sqsize = 1024.0/tilesize;


			    /*glMatrixMode(GL_TEXTURE);
                    glLoadIdentity();

			      glScalef(1.0/sqsize,1.0/sqsize,1.0/sqsize);
			      glTranslatef((float)X*(1.0/sqsize*sqsize),(float)Y*(1.0/sqsize*sqsize),0);


			     glMatrixMode(GL_MODELVIEW);*/

			     // glShadeModel(GL_SMOOTH);
			      //Main->PushTransform(X*(1.0/sqsize*2.0),Y*(1.0/sqsize*2.0),0,0,0,0);
			      glBegin(GL_TRIANGLES);
			      glTexCoord2f(0,0);
			      cx = X*(tilesize*4)/tilesize*(tilesize/32.0);
			      cy = Y*(tilesize*4)/tilesize*(tilesize/32.0);
			       /*glVertex3f(-1.0/sqsize,-1.0/sqsize,GetH(map,1.0/sqsize+x*128-1+cx,-1.0/sqsize+y*128-1+cy));
			      glTexCoord2f(1.0,0);
			      glVertex3f(1.0/sqsize,-1.0/sqsize,GetH(map,-1.0/sqsize+x*128-1+cx,-1.0/sqsize+y*128-1+cy));
			      glTexCoord2f(1.0,1.0);
			      glVertex3f(1.0/sqsize,1.0/sqsize,GetH(map,1.0/sqsize+x*128+cx,1.0/sqsize+y*128+cy));
			      glTexCoord2f(0,1.0);
			      glVertex3f(-1.0/sqsize,1.0/sqsize,GetH(map,-1.0/sqsize+x*128+cx,1.0/sqsize+y*128+cy));*/
                    z1 = GetH(map,1.0/sqsize+x*128+cx,-1.0/sqsize+y*128+cy);
                    z2 = GetH(map,1.0/sqsize+x*128+tilesize/8+cx,-1.0/sqsize+y*128+cy);
                    z3 = GetH(map,1.0/sqsize+x*128+tilesize/8+cx,-1.0/sqsize+y*128+tilesize/8+cy);
                    z4 = GetH(map,1.0/sqsize+x*128+cx,-1.0/sqsize+y*128+tilesize/8+cy);

                    x1 = X*(1.0/sqsize*2.0);
                    x2 = (X+1)*(1.0/sqsize*2.0);
                    y1 = Y*(1.0/sqsize*2.0);
                    y2 = (Y+1)*(1.0/sqsize*2.0);

                    // --1--
                    v3 v1;
                    v3 v2;
                    v3 n;
                    v1.x = x2-x1;v1.y = 0;v1.z = z2-z1;
                    v2.x = x2-x1;v2.y = y2-y1;v2.z = z3-z1;
                    n = v1.crossproduct(v2);
                    glNormal3f(n.x,n.y,n.z);
                    glTexCoord2f((float)X*(1.0/sqsize), (float)Y*(1.0/sqsize));
                    glVertex3f(x1, y1, z1);
                    glTexCoord2f((float)X*(1.0/sqsize)+1.0/sqsize, (float)Y*(1.0/sqsize));
                    glVertex3f(x2, y1, z2);
                    glTexCoord2f((float)X*(1.0/sqsize)+1.0/sqsize, (float)Y*(1.0/sqsize)+1.0/sqsize);
                    glVertex3f(x2, y2, z3);
                    //poly += 1;

                    // --2--
                    v1.x = x1-x2;v1.y = 0;v1.z = z4-z3;
                    v2.x = x1-x2;v2.y = y1-y2;v2.z = z1-z3;
                    n = v1.crossproduct(v2);
                    glNormal3f(n.x,n.y,n.z);
                    glTexCoord2f((float)X*(1.0/sqsize)+1.0/sqsize, (float)Y*(1.0/sqsize)+1.0/sqsize);
                    glVertex3f(x2, y2, z3);
                    glTexCoord2f((float)X*(1.0/sqsize), (float)Y*(1.0/sqsize)+1.0/sqsize);
                    glVertex3f(x1, y2, z4);
                    glTexCoord2f((float)X*(1.0/sqsize), (float)Y*(1.0/sqsize));
                    glVertex3f(x1, y1, z1);
                    //poly += 1;

			      glEnd();

			      //Main->PopTransform();

			    }
			    glEndList();
			    landscapecachestatus[y*map->width*8/1024+x] = 'o';
            }else{
			    glCallList(landscapecache[y*map->width*8/1024+x]);}
                }
            switch ( mode ){
      case 1: // Metal map


        glEnable(GL_LIGHTING);
        break;

  }
  glBindTexture(GL_TEXTURE_2D,0);
			  Main->PopTransform();
}
static std::string FindFont()
{
	const char* env = getenv("SPRINGMAPEDITOR_FONT");
	if (env && *env && access(env, R_OK) == 0)
		return env;

	static const char* candidates[] = {
		"Vera.ttf",
		"assets/Vera.ttf",
		"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
		"/usr/share/fonts/TTF/DejaVuSans.ttf",
		"/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
	};
	for (const char* path : candidates)
	{
		if (access(path, R_OK) == 0)
			return path;
	}
	return "";
}

// Look for a runtime asset next to the working directory first, then next to
// the executable. Returns the original name if it cannot be found.
static std::string FindAsset(const char* name)
{
	if (access(name, R_OK) == 0)
		return name;

	const char* base = SDL_GetBasePath();
	if (base)
	{
		std::string path = base;
		SDL_free((void*)base);
		path += name;
		if (access(path.c_str(), R_OK) == 0)
			return path;
	}
	return name;
}

static volatile sig_atomic_t interrupt_requested = 0;

static void handle_interrupt(int sig)
{
	(void)sig;
	interrupt_requested = 1;
}

int main(int argc, char ** argv)
{
	char txt[1024];
	bool quite = false;
	float p0,p1,p2,p3,cmod;
	bool checkOnly = (argc > 1 && strcmp(argv[1], "--check") == 0);
	if (!checkOnly && argc < 2)
	{
		fprintf(stderr, "Usage: %s <map.smf> [map directory]\n", argv[0]);
		fprintf(stderr, "       %s --check    (initialize video/OpenGL and exit)\n", argv[0]);
		return 1;
	}
	mode = 0; //Main texture
	const char * smffile = checkOnly ? NULL : argv[1];
	std::string smfpathstorage;
	if (argc > 2 && !checkOnly)
	{
		smfpathstorage = argv[2];
	}
	else if (!checkOnly)
	{
		std::string mapfile = smffile;
		size_t slash = mapfile.find_last_of("/\\");
		smfpathstorage = (slash == std::string::npos) ? "." : mapfile.substr(0, slash);
	}
	const char * smfpath = smfpathstorage.c_str();
	Main = new engine;
	gs = new gst;
	std::string fontpath = FindFont();
	bool haveFont = !fontpath.empty();
	FTGLPixmapFont font(haveFont ? fontpath.c_str() : "");
	if (haveFont)
	{
		font.FaceSize(12);
	}
	else
	{
		fprintf(stderr, "Warning: no TrueType font found, on-screen text disabled. "
		                "Set SPRINGMAPEDITOR_FONT to a .ttf file.\n");
	}

	if (!Main->Init3d(1024,768,8,false))
	{
		fprintf(stderr, "Failed to initialize the video subsystem.\n");
		return 1;
	}
	if (checkOnly)
	{
		printf("SDL video driver: %s\n", SDL_GetCurrentVideoDriver());
		printf("OpenGL vendor:    %s\n", glGetString(GL_VENDOR));
		printf("OpenGL renderer:  %s\n", glGetString(GL_RENDERER));
		printf("OpenGL version:   %s\n", glGetString(GL_VERSION));
		Main->Terminate();
		return 0;
	}
	//SDL_WM_GrabInput(SDL_GRAB_ON);
	//font->TryConstructFont("Luxy.ttf",32,255,0.027f);
	l0.lightinit(GL_LIGHT0,-50,50,50,1,1,1,1);

	float sqsize;
	float hval = 0;
	float tilesize = 64.0;
	int poly = 0;
	float tilesize2 = 16.0;
	CSmfReadMap* map;
	
	map = (CSmfReadMap*)(CReadMap::LoadMap(smffile,smfpath));
	if (map == NULL)
	{
		fprintf(stderr, "Failed to load map '%s' from '%s'\n", smffile, smfpath);
		Main->Terminate();
		return 1;
	}
    int mw = map->width;
    int mh = map->height;
	std::string fragmentShader = FindAsset("groundshader.frag");
	std::string vertexShader = FindAsset("groundshader.vert");
	groundshader.createfromfiles(fragmentShader.data(), vertexShader.data());
	//cout << gluErrorString(glGetError()) << endl;
	landscapecache = (GLint*)malloc(map->height*8/1024*map->width*8/1024*sizeof(GLint));
	landscapecachestatus = (char*)malloc(map->height*8/1024*map->width*8/1024+1);
	landscapecache[0] = glGenLists(map->height*8/1024*map->width*8/1024);

	for ( int i = 0; i < map->height*8/1024*map->width*8/1024; i++)
	{
	    if ( i > 0)
            landscapecache[i] = landscapecache[0] + i;
        landscapecachestatus[i] = 'e'; // Empty
	}
	landscapecachestatus[map->height*8/1024*map->width*8/1024] = 0;
	metaltexture = (GLuint*)malloc(sizeof(GLuint)*map->height*8/1024*map->width*8/1024);

	glGenTextures(map->height*8/1024*map->width*8/1024,metaltexture);

	//cout << map->height*8/1024*map->width*8/1024 << endl;
    unsigned char tval;

	MapBitmapInfo mbi;
    int mmapsqsize = (int)((float)map->header.mapx/2.0/(map->width*8.0/1024.0));
	printf("Metal map square size: %i\n", mmapsqsize);
	metalmap = map->GetInfoMap ("metal", &mbi);

    tmpdata = (unsigned char*)malloc(sizeof(unsigned char)*mmapsqsize*mmapsqsize);
	/*
	unsigned char * tmpdata;
	tmpdata = (unsigned char*)malloc(sizeof(unsigned char)*mmapsqsize*mmapsqsize);
	for ( int ya = 0; ya < map->height*8/1024; ya++)
	{
	    for ( int xa = 0; xa < map->width*8/1024; xa++)
        {
            printf("Generating metal texture[%i]...\n",ya*map->width*8/1024+xa);
            glBindTexture(GL_TEXTURE_2D,metaltexture[ya*map->width*8/1024+xa]);
            for ( int yb = 0; yb < mmapsqsize && yb + ya < map->header.mapy/2; yb++)
            {
                for ( int xb = 0; xb < mmapsqsize && xb + xa < map->header.mapx/2; xb++)
                {
                   // printf("Read metalmap[%i]\n",(map->header.mapy/2*(ya*1024/8+yb))+(xa+xb));
                    tval = metalmap[(map->header.mapx/2*(ya*1024/8+yb))+(xa*1024/8+xb)];
                   // printf("Write tmpdata[%i] = %i\n",mmapsqsize*yb+xb,(int)tval);
                    tmpdata[mmapsqsize*yb+xb] = tval;

                }
            }
            gluBuild2DMipmaps(GL_TEXTURE_2D,1,mmapsqsize,mmapsqsize,GL_LUMINANCE,GL_UNSIGNED_BYTE,tmpdata);
        }
	}
	printf("Generated metal textures succesfully");*/

	//glTexImage2D(GL_TEXTURE_2D,0,1,mbi.width,mbi.height,0,GL_LUMINANCE,GL_UNSIGNED_BYTE,metalmap);

	glEnable(GL_BLEND);
	glEnable(GL_LIGHTING);
	glEnable(GL_NORMALIZE);
	glEnable(GL_TEXTURE_2D);
	CBFGroundTextures * tex;
	tex = new CBFGroundTextures((CSmfReadMap*)map);
	printf("Entering main loop\n");
	int x,y,z,X,Y,Z;
	int MAP_SIZEX = map->width+1;
	int MAP_SIZEY = map->height+1;
	int STEP_SIZE = 16;
	
	float cval = 0.0;

	SDL_SetWindowRelativeMouseMode(Main->screen, true);

	Main->SetCameraPos(0,0,-10);
	pxx = -map->width*8/1024;
	pzz = -map->height*8/1024;
	signal(SIGINT, handle_interrupt);
	signal(SIGTERM, handle_interrupt);

	while (quite == false && !Main->quit && !interrupt_requested)
	{
	    
	    //printf("Begin frame\n");
		Main->clearscreen(0.5,0.5,0.5,1.0);
		glLoadIdentity();
		/*glDrawPixels( font->rend->width,
			     font->rend->height,
			     GL_LUMINANCE_ALPHA,
			     GL_UNSIGNED_BYTE,
			     font->rend->buffer);*/
        Main->PushTransform(0,0,0,90.0,0,0);// Render a rectangle offscreen to make editing easier, by haveing flat selection plane
		Main->ExtractFrustum();
        glBegin(GL_QUADS);
		glVertex3f(0.0,0.0,0.01);
		glVertex3f(map->width*8/1024*2,0.0,0.0);
		glVertex3f(map->width*8/1024*2,map->height*8/1024*2,0);
		glVertex3f(0.0,map->height*8/1024*2,0);
		glEnd();
		Main->PopTransform();
		Main->Make2DPositionInto3DPosition((float)Main->mousex,(float)Main->mousey,&mx,&my,&mz);
		Main->clearscreen(0.5,0.5,0.5,1.0);
		
		//font->RenderString(rxx,ryy,6.0,txt);
		if ( Main->getkey(SDL_SCANCODE_ESCAPE))
			quite = true;

		if ( Main->up)
		{
			pxx += 20 / max(1.0,(double)Main->getfps());
		}
		//printf("pxx=%f\n",pxx);
		if ( Main->getkey(SDL_SCANCODE_Q))
		{
			pyy += 20 / max(1.0,(double)Main->getfps());
		}
		for ( int i = 0; i < 9; i++)
        {
            if (Main->getkey((SDL_Scancode)(SDL_SCANCODE_0 + i)))
            {
                for ( int i2 = 0; i2 < map->height*8/1024*map->width*8/1024; i2++)
                    landscapecachestatus[i2] = 'u';
                mode = i;
            }
        }
		if ( Main->left)
		{
			pzz += 20 / max(1.0,(double)Main->getfps());
		}
		if ( Main->down)
		{
			pxx -= 20 / max(1.0,(double)Main->getfps());
		}
		if ( Main->getkey(SDL_SCANCODE_E))
		{
			pyy -= 20 / max(1.0,(double)Main->getfps());
		}
		if ( Main->right)
		{
			pzz -= 20 / max(1.0,(double)Main->getfps());
		}
		
		if ( Main->getkey(SDL_SCANCODE_L))
		{
			if (mode == 1)
				glDrawPixels(mbi.width,mbi.height,GL_LUMINANCE,GL_UNSIGNED_BYTE,metalmap);
			else if (mode == 0)
				glDrawPixels(gs->mapx+1,gs->mapy+1,GL_LUMINANCE,GL_FLOAT,map->heightmap);
		}
		
        if ( Main->getkey(SDL_SCANCODE_A) and brushsize < 100)
		{
			brushsize += 5 / max(1.0,(double)Main->getfps());
		}
		if ( Main->getkey(SDL_SCANCODE_Z) and brushsize > 0)
		{
			brushsize -= 5 / max(1.0,(double)Main->getfps());
		}
		if ( Main->getkey(SDL_SCANCODE_C) and hval < FLT_MAX)
		{
			hval += 5 / max(1.0,(double)Main->getfps());
		}
		if ( Main->getkey(SDL_SCANCODE_X) and brushsize > 0)
		{
			hval -= 5 / max(1.0,(double)Main->getfps());
		}
		if ( Main->getkey(SDL_SCANCODE_B) and brushsize > 0)
		{
			hval = cval;
		}
		
		if ( Main->mouseleft )
		{
            SDL_HideCursor();
	    
            ryy += (float)Main->mouserelx/10.0;
            rzz += (float)Main->mouserely/10.0;
            //printf("Mouse xrel=%i Mouse yrel=%i\n",Main->mouserelx,Main->mouserely);
		}else{
		    SDL_ShowCursor();
		}
		if ( Main->getkey(SDL_SCANCODE_T) and ts > 4)
		{

			ts -= 1;
			for ( int i = 0; i < map->height*8/1024*map->width*8/1024; i++)
                landscapecachestatus[i] = 'u';

		}
		if ( Main->getkey(SDL_SCANCODE_Y) and ts < 10)
		{
			ts += 1;
			for ( int i = 0; i < map->height*8/1024*map->width*8/1024; i++)
                landscapecachestatus[i] = 'u';
		}


		if ( Main->getkey(SDL_SCANCODE_W))
		{
			glPolygonMode(GL_FRONT,GL_LINE);
			glPolygonMode(GL_BACK,GL_LINE);
		}else{
			glPolygonMode(GL_FRONT,GL_FILL);
			glPolygonMode(GL_BACK,GL_FILL);
		}

		//glColorMaterial(GL_FRONT_AND_BACK,GL_DIFFUSE);
		if ( Main->getkey(SDL_SCANCODE_F))
		{
			Main->ToggleFullScreen();

		}
        Main->SetCameraRotation(rxx,ryy,rzz);
		Main->SetCameraPos(pxx,pyy,pzz);
        l0.setpos(0,-10,0);
Main->PushTransform(0,0,0,90.0,0,0);

      //  mh = 128;
	for ( int y_ = 0; y_ < mh*8/1024;y_++)
		{

			for (  int x_ = 0; x_ < mw*8/1024;x_++)
			{

              groundshader.useshader();
              //printf("drawsquare(%i,%i,map,tex,pow(2,%f));\n",x,y,ts);
			  drawsquare(x_,y_,map,tex,pow(2,ts));
			  glUseProgram(0);
			}
		}

		glBindTexture(GL_TEXTURE_2D,0);
		glEnable(GL_BLEND);		// Turn Blending On
        //glDisable(GL_DEPTH_TEST);
        glBlendFunc(GL_SRC_ALPHA,GL_ONE);
        glDisable(GL_LIGHTING);
		glBegin(GL_QUADS);
		glColor4f(0.0,0.4,1.0,0.4);
		glVertex3f(0.0,0.0,0.01);
		glVertex3f(map->width*8/1024*2,0.0,0.01);
		glVertex3f(map->width*8/1024*2,map->height*8/1024*2,0.01);
		glVertex3f(0.0,map->height*8/1024*2,0.01);

		glEnd();
        glDisable(GL_BLEND);
        	// Turn Blending Off
        //glEnable(GL_DEPTH_TEST);
		glColor4f(1,1,1,1);
        Main->PopTransform();

        Main->PushTransform((float)mx,0,(float)mz,0,0,0);
        glPolygonMode(GL_FRONT,GL_LINE);
        glPolygonMode(GL_BACK,GL_LINE);
        glBegin(GL_POLYGON);
        glColor3f(Main->getkey(SDL_SCANCODE_S) ? 0.0 : 1.0,Main->getkey(SDL_SCANCODE_S) ? 1.0 : 0.0,0.0);
        mappx = mx*1024/2;
        mappy = mz*1024/2;
        int hmapx = mappx/8;
        int hmapy = mappy/8;
        int mmapx = mappx/8;
        int mmapy = mappy/8;

        for (int k = 0; k < 360; k += 360/64)
        {
            float xx1 = sin((float)k/360.0*2*3.141592653)*brushsize;
            float yy1 = cos((float)k/360.0*2*3.141592653)*brushsize;
            glVertex3f(xx1,-(GetH(map,hmapx+xx1*1024/8/2,hmapy+yy1*1024/8/2)+0.1),yy1);

        }
        glColor4f(0,1.0,1.0,0.2);
        glEnd();
        glPolygonMode(GL_FRONT,GL_LINE);
        glPolygonMode(GL_BACK,GL_FILL);
        glEnable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glBegin(GL_POLYGON);
        for (int k = 0; k < 360; k += 360/64)
        {
            float xx1 = sin((float)k/360.0*2*3.141592653)*brushsize;
            float yy1 = cos((float)k/360.0*2*3.141592653)*brushsize;
            glVertex3f(xx1,-(GetH(map,hmapx+xx1*1024/8/2,hmapy+yy1*1024/8/2)+0.1),yy1);

        }
        glEnd();
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glPolygonMode(GL_FRONT,GL_FILL);
        Main->PopTransform();
        glColor4f(1,1.0,1.0,1.0);
        glEnable(GL_LIGHTING);
       /* Main->PushTransform((float)mx,0,(float)mz,0,0,0);
        glBegin(GL_POLYGON);
        glColor3f(1.0,1.0,0.0);
        for (int k = 0; k < 360; k += 360/16)
        {

            glVertex3f(sin((float)k/360.0*2*3.141592653)*brushsize,0,cos((float)k/360.0*2*3.141592653)*brushsize);

        }
        glColor3f(1.0,1.0,1.0);
        glEnd();

        Main->PopTransform();*/

	if (Main->getwup())
	{
	  if (mode == 0)
            {
            int reald = brushsize * 1024/8;
            float rat = 0.0;

            for ( int ay = 0; ay < reald; ay++)
            {
                for ( int ax = 0; ax < reald; ax++)
                {

                        int bx = reald/2;
                        int by = reald/2;
			float cv2;
                        float cr = sqrt((float)((ax-bx)*(ax-bx)+(ay-by)*(ay-by)));
                       // printf("%f <= %f\n", cr,(float)reald/2);
                        if ( cr <= (float)reald/2 )
                        {
                            if (hmapx-reald/2+ax <= gs->mapx+1 && hmapy-reald/2+ay <= gs->mapy+1)
                            {
                           // printf("Seth\n");
                            if ( not Main->getkey(SDL_SCANCODE_S) ) // Smooth
			    {
				cv2 = GetH(map,hmapx-reald/2+ax,hmapy-reald/2+ay);
				
                                SetH(map,hmapx-reald/2+ax,hmapy-reald/2+ay,cv2+0.1);
			    }
			    }
			}
		}
	    }
	    }else if ( mode == 1)
    {

        int reald = brushsize * 1024/8;
            float rat = 0.0;

            for ( int ay = 0; ay < reald; ay++)
            {
                for ( int ax = 0; ax < reald; ax++)
                {

                        int bx = reald/2;
                        int by = reald/2;
			float cv2;
                        float cr = sqrt((float)((ax-bx)*(ax-bx)+(ay-by)*(ay-by)));
                       // printf("%f <= %f\n", cr,(float)reald/2);
                        if ( cr <= (float)reald/2 )
                        {
                            if (mmapx-reald/2+ax <= map->header.mapx && hmapy-reald/2+ay <= map->header.mapy)
                            {
                           // printf("Seth\n");
                            if ( not Main->getkey(SDL_SCANCODE_S) ) // Smooth
			    {
				cv2 = GetM(map,mmapx-reald/2+ax,mmapy-reald/2+ay);
                                SetM(map,mmapx-reald/2+ax,mmapy-reald/2+ay,cv2+0.1);
			    }
			    }
			}
		}
	    }
	}
	}
	if (Main->getwdown())
	{
	  if (mode == 0)
            {
            int reald = brushsize * 1024/8;
            float rat = 0.0;

            for ( int ay = 0; ay < reald; ay++)
            {
                for ( int ax = 0; ax < reald; ax++)
                {

                        int bx = reald/2;
                        int by = reald/2;
			float cv2;
                        float cr = sqrt((float)((ax-bx)*(ax-bx)+(ay-by)*(ay-by)));
                       // printf("%f <= %f\n", cr,(float)reald/2);
                        if ( cr <= (float)reald/2 )
                        {
                            if (hmapx-reald/2+ax <= gs->mapx+1 && hmapy-reald/2+ay <= gs->mapy+1)
                            {
                           // printf("Seth\n");
                            if ( not Main->getkey(SDL_SCANCODE_S) ) // Smooth
			    {
				cv2 = GetH(map,hmapx-reald/2+ax,hmapy-reald/2+ay);
				
                                SetH(map,hmapx-reald/2+ax,hmapy-reald/2+ay,cv2-0.1);
			    }
			    }
			}
		}
	    }
	    }else if ( mode == 1)
    {

        int reald = brushsize * 1024/8;
            float rat = 0.0;

            for ( int ay = 0; ay < reald; ay++)
            {
                for ( int ax = 0; ax < reald; ax++)
                {

                        int bx = reald/2;
                        int by = reald/2;
			float cv2;
                        float cr = sqrt((float)((ax-bx)*(ax-bx)+(ay-by)*(ay-by)));
                       // printf("%f <= %f\n", cr,(float)reald/2);
                        if ( cr <= (float)reald/2 )
                        {
                            if (mmapx-reald/2+ax <= map->header.mapx && hmapy-reald/2+ay <= map->header.mapy)
                            {
                           // printf("Seth\n");
                            if ( not Main->getkey(SDL_SCANCODE_S) ) // Smooth
			    {
				cv2 = GetM(map,mmapx-reald/2+ax,mmapy-reald/2+ay);
                                SetM(map,mmapx-reald/2+ax,mmapy-reald/2+ay,cv2-0.1);
			    }
			    }
			}
		}
	    }
	}
	}

        if (Main->mouseright)
        {

            if (mode == 0)
            {
            int reald = brushsize * 1024/8;
            float rat = 0.0;

            for ( int ay = 0; ay < reald; ay++)
            {
                for ( int ax = 0; ax < reald; ax++)
                {

                        int bx = reald/2;
                        int by = reald/2;

                        float cr = sqrt((float)((ax-bx)*(ax-bx)+(ay-by)*(ay-by)));
                       // printf("%f <= %f\n", cr,(float)reald/2);
                        if ( cr <= (float)reald/2 )
                        {
                            if (hmapx-reald/2+ax <= gs->mapx+1 && hmapy-reald/2+ay <= gs->mapy+1)
                            {
                           // printf("Seth\n");
                            if ( not Main->getkey(SDL_SCANCODE_S) ) // Smooth
                                SetH(map,hmapx-reald/2+ax,hmapy-reald/2+ay,hval);
                            else
                            {
                                //rat = -((float)reald/2 - cr)/(reald/2);
                                p2 = GetH(map,hmapx-reald/2+ax+1,hmapy-reald/2+ay);
                                p3 = GetH(map,hmapx-reald/2+ax,hmapy-reald/2+ay+1);
                                p1 = GetH(map,hmapx-reald/2+ax,hmapy-reald/2+ay-1);
                                p0 = GetH(map,hmapx-reald/2+ax-1,hmapy-reald/2+ay);
                                cmod = (p0 + p1 + p2 + p3) / 4;
                                SetH(map,hmapx-reald/2+ax,hmapy-reald/2+ay,cmod);
                            }

                    }
                    }
                }

            }

        }else if ( mode == 1)
    {

        int reald = brushsize * 1024/8;
            float rat = 0.0;

            for ( int ay = 0; ay < reald; ay++)
            {
                for ( int ax = 0; ax < reald; ax++)
                {

                        int bx = reald/2;
                        int by = reald/2;

                        float cr = sqrt((float)((ax-bx)*(ax-bx)+(ay-by)*(ay-by)));
                       // printf("%f <= %f\n", cr,(float)reald/2);
                        if ( cr <= (float)reald/2 )
                        {
                            if (mmapx-reald/2+ax <= map->header.mapx && hmapy-reald/2+ay <= map->header.mapy)
                            {
                           // printf("Seth\n");
                            if ( not Main->getkey(SDL_SCANCODE_S) ) // Smooth
                                SetM(map,mmapx-reald/2+ax,mmapy-reald/2+ay,hval);
                            else
                            {
                                //rat = -((float)reald/2 - cr)/(reald/2);
                                p2 = GetM(map,(mmapx-reald/2+ax-2),(mmapy-reald/2+ay-2));
                                p3 = GetM(map,(mmapx-reald/2+ax+1),(mmapy-reald/2+ay-2));
                                p1 = GetM(map,(mmapx-reald/2+ax+1),(mmapy-reald/2+ay+1));
                                p0 = GetM(map,(mmapx-reald/2+ax-2),(mmapy-reald/2+ay+1));

                                cmod = (p0 + p1 + p2 + p3) / 4;
                               // printf("p0=%f,p1=%f,p2=%f,p3=%f,cmod=%f\n",p0,p1,p2,p3,cmod);
                                SetM(map,mmapx-reald/2+ax,mmapy-reald/2+ay,cmod);
                            }

                    }
                    }
                }

            }




    }
        }

        lastmr = Main->mouseright;
		//usleep(8000);
		//SDL_WarpMouse(Main->width / 2, Main->height /2); Fail
		 cval = GetH(map,hmapx,hmapy);
        memset(txt,0x0,1024);
       // printf("%i\n",(int)time(NULL));
        /*if ( Main->getfps() > 60  and ts > 3.0 and time(NULL) - lastchange > 1){
            ts -= 1;
            for ( int i = 0; i < map->height*8/1024*map->width*8/1024; i++)
                landscapecachestatus[i] = 'u';
            lastchange = time(NULL);
        }
        if ( Main->getfps() < 20  and ts < 10.0 and time(NULL) - lastchange > 1){
            ts += 1;
            for ( int i = 0; i < map->height*8/1024*map->width*8/1024; i++)
                landscapecachestatus[i] = 'u';
            lastchange = time(NULL);
        }*/
		sprintf(txt,"%f FPS XPosition = %f,YPosition = %f,ZPosition = %f,RotX= %f,RotY= %f,RotZ = %f %i Polygons",Main->getfps(),pxx,pyy,pzz,rxx,ryy,rzz,poly);
		if (haveFont)
		{
			font.Render(txt);
		}
		poly = 0;

		sprintf(txt,"Brushsize: %f , MAPX = %f , MAPY = %f, Height = %f , Set height = %f",brushsize,mappx,mappy,cval,hval);
		if (haveFont)
		{
			font.Render(txt,-1,FTPoint(0,20));
		}
		sprintf(txt,"Cache status = %s",landscapecachestatus);
		if (haveFont)
		{
			font.Render(txt,-1,FTPoint(0,40));
		}
		Main->display();
	}
	
	SDL_SetWindowRelativeMouseMode(Main->screen, false);
	Main->Terminate();
	bool save = false;
	
	while ( true )
	{
        printf("Save changes? [y/n] ");
        fflush(stdout);
        int ans = getchar();
        if (ans == 'y')
        {
            save = true;
            break;
        }
        if (ans == 'n' || ans == EOF)
        {
            save = false;
            break;
        }
	}
	if ( save )
	{
        printf("Writing new maps to smf file...\n");
        MapHeader maph;
        FILE * mapfout = fopen(smffile,"rb+");
        if ( ! mapfout )
        {
        	printf("Error, cannot open the map file to save data\n");
        	return 1;
        }
        if (fread(&maph,sizeof(maph),1,mapfout) != 1)
        {
        	printf("Error, cannot read the map header\n");
        	fclose(mapfout);
        	return 1;
        }
        printf("Header Magic: %s\n",maph.magic);
        printf("metalmapPtr: %i      MAPX=%i    MAPY=%i\n",maph.metalmapPtr,maph.mapx,maph.mapy);
        printf("typemapPtr: %i      MAPX=%i    MAPY=%i\n",maph.typeMapPtr,maph.mapx,maph.mapy);
        printf("heightmapPtr: %i      MAPX=%i    MAPY=%i\n",maph.heightmapPtr,maph.mapx,maph.mapy);
        printf("minimapPtr: %i      MAPX=%i    MAPY=%i\n",maph.minimapPtr,maph.mapx,maph.mapy);

        unsigned int datasize = (maph.mapx+1)*(maph.mapy+1)*sizeof(unsigned short);
        printf("(%i+1)*(%i+1)*%i=%i\n",maph.mapx,maph.mapy,(int)sizeof(unsigned short),datasize);
        unsigned short *hmapdata = (unsigned short*)malloc(datasize);
        printf("Converting height map...\n");
        for ( y = 0; y < maph.mapy+1;y++)
        {
            for ( x = 0; x < maph.mapx+1;x++)
            {
                double h = max(0.0, min(1.0, (double)map->heightmap[y*(maph.mapx+1)+x]));
                hmapdata[y*(maph.mapx+1)+x] = (unsigned short)min(65535.0, 65536.0*h);
            }
        }
        printf("Done converting heightmap\n");
        fseek(mapfout,maph.heightmapPtr,SEEK_SET);
        fwrite(hmapdata,datasize,1,mapfout);
        fseek(mapfout,maph.metalmapPtr,SEEK_SET);
        fwrite(metalmap,map->width/2*map->height/2,1,mapfout);
        fclose(mapfout);
        free(hmapdata);
        printf("Save complete\n");
	}

	int squares = map->height * 8 / 1024 * map->width * 8 / 1024;
	glDeleteTextures(squares, metaltexture);
	glDeleteLists(landscapecache[0], squares);
	free(metaltexture);
	free(landscapecache);
	free(tmpdata);
	map->FreeInfoMap("metal", metalmap);
	free(landscapecachestatus);
	delete tex;
	delete map;
	delete Main;
	delete gs;
	printf("Exiting\n");
   
	return 0;
}

