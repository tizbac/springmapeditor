// SmfReadMap.cpp: implementation of the CSmfReadMap class.
//
//////////////////////////////////////////////////////////////////////

#include "SmfReadMap.h"
#include "mapfile.h"
#include "bitops.h"
#include "GlobalStuff.h"

#include <GL/glew.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace std;

CSmfReadMap::CSmfReadMap(std::string mapname, std::string mpath)
	: header(), mapf(new fakevfs), usePBO(true), anisotropy(0.0f), heightmap(NULL)
{
	printf("Opening map file\n");

	mapf->f = fopen(mapname.c_str(), "rb");
	mapf->cpath = mpath;

	if (!mapf->f)
	{
		printf("Cannot find map %s: %s\n", mapname.c_str(), strerror(errno));
		return;
	}
	printf("File opened\n");

	READPTR_MAPHEADER(header, mapf);

	if (strcmp(header.magic, "spring map file") != 0 || header.version != 1 ||
		header.tilesize != 32 || header.texelPerSquare != 8 || header.squareSize != 8)
	{
		printf("Incorrect map file %s\n", mapname.c_str());
		printf("Header is \"%s\" instead of \"spring map file\"\n", header.magic);
		exit(1);
	}

	width = header.mapx;
	height = header.mapy;

	gs->mapx = header.mapx;
	gs->mapy = header.mapy;
	gs->mapSquares = gs->mapx * gs->mapy;
	gs->hmapx = gs->mapx / 2;
	gs->hmapy = gs->mapy / 2;
	gs->pwr2mapx = next_power_of_2(gs->mapx);
	gs->pwr2mapy = next_power_of_2(gs->mapy);

	float3::maxxpos = gs->mapx * SQUARE_SIZE_ - 1;
	float3::maxzpos = gs->mapy * SQUARE_SIZE_ - 1;

	// The stored height map is 16 bit and is normalised to [0, 1].
	const float base = 0.0;
	const float mod = 1.0 / 65536.0;

	const int hmx = gs->mapx + 1;
	const int hmy = gs->mapy + 1;

	heightmap = new float[hmx * hmy];
	unsigned short* temphm = new unsigned short[hmx * hmy];

	mapf->Seek(header.heightmapPtr);
	mapf->Read(temphm, hmx * hmy * 2);

	for (int y = 0; y < hmx * hmy; ++y)
		heightmap[y] = base + swabword(temphm[y]) * mod;

	delete[] temphm;
}

CSmfReadMap::~CSmfReadMap(void)
{
	delete mapf;
	delete[] heightmap;
}

unsigned char *CSmfReadMap::GetInfoMap (const std::string& name, MapBitmapInfo* bmInfo)
{
	if (name == "metal")
	{
		bmInfo->width = header.mapx / 2;
		bmInfo->height = header.mapy / 2;
		printf("Metal map %ix%i\n", bmInfo->width, bmInfo->height);

		unsigned char* data = new unsigned char[bmInfo->width * bmInfo->height];
		mapf->Seek(header.metalmapPtr);
		mapf->Read(data, header.mapx / 2 * header.mapy / 2);
		return data;
	}
	else if (name == "type")
	{
		bmInfo->width = header.mapx / 2;
		bmInfo->height = header.mapy / 2;

		unsigned char* data = new unsigned char[bmInfo->width * bmInfo->height];
		mapf->Seek(header.typeMapPtr);
		mapf->Read(data, gs->mapx * gs->mapy / 4);
		return data;
	}
	else if (name == "height")
	{
		bmInfo->width = header.mapx + 1;
		bmInfo->height = header.mapy + 1;
		printf("Height map is %i x %i\n", header.mapx + 1, header.mapy + 1);

		short int* data = new short int[bmInfo->width * bmInfo->height];
		mapf->Seek(header.heightmapPtr);
		mapf->Read(data, (gs->mapx + 1) * (gs->mapy + 1));
		return (unsigned char*)data;
	}

	return nullptr;
}

void CSmfReadMap::FreeInfoMap (const std::string& name, unsigned char *data)
{
	delete[] data;
}
