/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * gl3dengine
 * Copyright (C)  2008 <>
 * 
 * gl3dengine is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * gl3dengine is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdlib.h>
#include <stdio.h>
#include "3doobject.hpp"

bool _3doobject::loadfromfile(char * filename)
{
	FILE * f;
	this->listid = glGenLists(1);
	f = fopen(filename,"r");
	if (!f)
	{
		printf("Cannot open %s\n",filename);
		exit(1);
	}else{
		printf("Opened %s\n",filename);
		
	}
	printf("a\n");
	fseek(f,0,SEEK_SET);
	fread(&this->header,sizeof(_3doheader),1,f);
	
	printf("g\n");
	printf("%s: %i Vertices\n",filename,this->header.vertnumber);
	
	printf("%s: %i Primitives\n",filename,this->header.primnumber);
	this->vertexs = (vertex*)malloc(sizeof(vertex)*this->header.vertnumber);
	fseek(f,this->header.OffsetToVertexArray,SEEK_SET);
	rawvertex tmpvert;
	for ( int i = 0; i < this->header.vertnumber; i++)
	{

		
		fread(&tmpvert,sizeof(rawvertex),1,f);
		//fseek(f,sizeof(vertex),SEEK_CUR);
	
		this->vertexs[i].x = (float)tmpvert.x / 65536.0f;
		this->vertexs[i].y = (float)tmpvert.y  / 65536.0f;
		this->vertexs[i].z = (float)tmpvert.z / 65536.0f;
		//glVertex3f(this->vertexs[i].x,this->vertexs[i].y,this->vertexs[i].z);
		//printf("vertex: x=%f, y=%f, z=%f\n",this->vertexs[i].x,this->vertexs[i].y,this->vertexs[i].z);
	
		
		
	}
	glNewList(this->listid,GL_COMPILE);
	
	
	this->primitives = (primitive*)malloc(sizeof(primitive)*this->header.primnumber);
	fseek(f,this->header.OffsetToPrimitiveArray,SEEK_SET);
	int lastseek;
	this->primdata = (prim*)malloc(sizeof(prim)*this->header.primnumber);
	long a = 0;
	for ( long i = 0; i < this->header.primnumber; i++)
	{

		a = i;
		fread(&this->primitives[a],sizeof(primitive),1,f);
		//fseek(f,sizeof(primitive),SEEK_CUR);
		lastseek = ftell(f);
		vertex * pv;
		long v = 0;
		printf("Primitive: VertCount=%i offset=%i %i\n",this->primitives[i].NumberOfVertexIndexes,this->primitives[i].OffsetToVertexIndexArray,a);
		if (this->primitives[a].NumberOfVertexIndexes == 4){
			glBegin(GL_QUADS);
			pv = (vertex*)malloc(sizeof(vertex)*4);
			}
		if (this->primitives[a].NumberOfVertexIndexes == 3){
			glBegin(GL_TRIANGLES);
			pv = (vertex*)malloc(sizeof(vertex)*3);
			}
		int j = this->primitives[i].OffsetToVertexIndexArray;
		fseek(f,j,SEEK_SET);
		short ti;
		//vertex * pv = (vertex*)malloc(sizeof(vertex)*4);
		for ( int n = 0; n < this->primitives[a].NumberOfVertexIndexes; n++)
		{
			
			fread(&ti,sizeof(short),1,f);
			pv[n] = vertexs[ti];
			//glVertex3f(this->vertexs[ti].x,this->vertexs[ti].y, this->vertexs[ti].z);
			printf("Vertex x=%f,y=%f,z=%f\n",this->vertexs[ti].x,this->vertexs[ti].y, this->vertexs[ti].z);
		}
		// ormalX = (X[A]-X[B])*(X[B]-X[C]); normalY = (Y[A]-Y[B])*(Y[B]-Y[C]); normalZ = (Z[A]-Z[B])*(Z[B]-Z[C]);
		if (this->primitives[a].NumberOfVertexIndexes == 3)
		{
			float nx = (pv[0].x-pv[1].x)*(pv[1].x-pv[2].x);
			float ny = (pv[0].y-pv[1].y)*(pv[1].y-pv[2].y);
			float nz = (pv[0].x-pv[1].z)*(pv[1].x-pv[2].z);
			glNormal3f(nx,ny,nz);
			glVertex3f(pv[0].x,pv[0].y,pv[0].z);
			glVertex3f(pv[1].x,pv[1].y,pv[1].z);
			glVertex3f(pv[2].x,pv[2].y,pv[2].z);
		}
		else{
			float nx = abs((pv[0].x-pv[1].x)*(pv[1].x-pv[2].x)*(pv[2].x-pv[3].x));
			float ny = abs((pv[0].y-pv[1].y)*(pv[1].y-pv[2].y)*(pv[2].y-pv[3].y));
			float nz = abs((pv[0].x-pv[1].z)*(pv[1].x-pv[2].z)*(pv[2].x-pv[3].z));
			glNormal3f(nx,ny,nz);
			glVertex3f(pv[0].x,pv[0].y,pv[0].z);
			glVertex3f(pv[1].x,pv[1].y,pv[1].z);
			glVertex3f(pv[2].x,pv[2].y,pv[2].z);
			glVertex3f(pv[3].x,pv[3].y,pv[3].z);}
		glEnd();
		fseek(f,lastseek,SEEK_SET);
		free(pv);
	}
	
	printf("Done reading vertexs\n");
	

	
	
	
	
	glEndList();
	return true;
}
void _3doobject::Render(float x,float y,float z,float rx,float ry,float rz,float scale)
{
		
		
		glPushMatrix();
		glScalef(scale,scale,scale);
		glTranslatef(x,y,z);
		
		glRotatef(rx,1,0,0);
		glRotatef(ry,0,1,0);
		glRotatef(rz,0,0,1);
		
	/*for (int m = 0;m < this->header.vertnumber;m++)
	{
		glVertex3f(this->vertexs[m].x,this->vertexs[m].y,this->vertexs[m].z);
		
		
	}*/
	glCallList(this->listid);
		glPopMatrix();
}
