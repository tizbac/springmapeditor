// ReadMap.cpp: implementation of the CReadMap class.
//
//////////////////////////////////////////////////////////////////////

#include "ReadMap.h"
#include "GlobalStuff.h"
#include "SMF/SmfReadMap.h"

#include <cstdio>
#include <GL/glew.h>
#include <string>

using namespace std;

CReadMap* CReadMap::LoadMap (const std::string& mapname, string path)
{
	if (mapname.length() < 3)
	{
		printf("MapName too short\n");
		return 0;
	}

	printf("Loading %s\n", mapname.c_str());

	CReadMap* rm = new CSmfReadMap(mapname, path);
	if (!rm)
		return 0;

	return rm;
}

CReadMap* CReadMap::LoadMap (const std::string& mapname)
{
	return CReadMap::LoadMap(mapname, ".");
}

CReadMap::CReadMap() :
	width(0),
	height(0)
{
}

CReadMap::~CReadMap()
{
}
