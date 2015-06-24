#include "2dstuff.h"
#include <stdio.h>
#define flt(x) ((float)x)
void draw2drect(int sw,int sh,int x1,int y1,int x2,int y2,bool filled)
{
  //printf("2drect : %i,%i,%i,%i,%i,%i\n",sw,sh,x1,y1,x2,y2);
  
  
  if (filled)
  {
    glPolygonMode(GL_FRONT,GL_FILL);
    glPolygonMode(GL_BACK,GL_FILL);
  }
  else
  {
    glPolygonMode(GL_FRONT,GL_LINE);
    glPolygonMode(GL_BACK,GL_LINE);
  }
  glBegin(GL_POLYGON);
  glVertex3i(x1,y1,0.0);
  glVertex3i(x2,y1,0.0);
  glVertex3i(x2,y2,0.0);
  glVertex3i(x1,y2,0.0);
  glEnd();
  glPolygonMode(GL_FRONT,GL_FILL);
  glPolygonMode(GL_BACK,GL_FILL);
  
}
