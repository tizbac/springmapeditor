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
#include "GL/glew.h"
#ifndef _3DOOBJECT_HPP_
#define _3DOOBJECT_HPP_
struct vertex{
	float x;
	float y;
	float z;
	int xi;
	int yi;
	int zi;
	float tx;
	float ty;
	float tz;
	float ny;
	float nx;
	float nz;
};
struct rawvertex{
	long x;
	long y;
	long z;
};
struct primitive{
	long PaletteEntry;
	long NumberOfVertexIndexes;
	long Always_0;
	long OffsetToVertexIndexArray;
	long OffsetToTextureName;
	long Unknown_1;
	long Unknown_2;
	long Unknown_3;
	
	
	
	
};
struct _3doheader{
	long version;
	long vertnumber;
	long primnumber;
	long selprimitive;
	long xpa;
	long ypa;
	long zpa;
	long OffsetToObjectName;
	long Always_0;
	long OffsetToVertexArray;
	long OffsetToPrimitiveArray;
	long OffsetToSiblingObject;
	long OffsetToChildObject;
};
	
struct prim{
	vertex * verts;
	int type;
	char * texturename;
};

class _3doobject
{
	
public:
	bool loadfromfile(char * filename);
	vertex * vertexs;
	primitive * primitives;
	prim * primdata;
	_3doheader header;
	void Render(float x,float y,float z,float rx,float ry,float rz,float scale);
protected:

private:
	GLuint listid;
};

#endif // _3DOOBJECT_HPP_
