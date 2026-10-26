#ifndef SMFREADMAP_H
#define SMFREADMAP_H

#include "byteorder.h"
#include "float3.h"
#include "Map/ReadMap.h"
#include "mapfile.h"

#include <cstdio>
#include <string>

using namespace std;

/** Simple buffered reader for the .smf file and the referenced tile files. */
class fakevfs
{
public:
	int cseek;
	int fsize;
	string cpath;
	FILE * f;

	fakevfs() : cseek(0), fsize(0), f(NULL) {}
	~fakevfs() { if (f) fclose(f); }

	int Read(void * dest, int size)
	{
		if (!this->f)
		{
			printf("Invalid file handler\n");
			return 1;
		}
		fread(dest, size, 1, this->f);
		this->cseek += size;
		fseek(this->f, this->cseek, SEEK_SET);
		return 0;
	}

	int Seek(int pos)
	{
		if (!this->f)
		{
			printf("Invalid file handler\n");
			return 1;
		}

		fseek(this->f, 0, SEEK_END);
		this->fsize = ftell(this->f);

		if (pos > this->fsize)
		{
			printf("Invalid seek %i , size is %i\n", pos, this->fsize);
			return 1;
		}

		this->cseek = pos;
		fseek(this->f, this->cseek, SEEK_SET);
		return 0;
	}

	int Close(void)
	{
		if (this->f)
		{
			fclose(this->f);
			this->f = NULL;
		}
		return 0;
	}

	int GetPos(void) { return cseek; }
};


class CSmfReadMap : public CReadMap
{
public:
	CSmfReadMap(std::string mapname, std::string path);
	~CSmfReadMap(void);

	float* GetHeightmap() { return heightmap; }

	unsigned char *GetInfoMap (const std::string& name, MapBitmapInfo* bm);
	void FreeInfoMap (const std::string& name, unsigned char *data);

	SMFHeader header;
	fakevfs* mapf;
	bool usePBO;
	float anisotropy;
	float* heightmap;
};

#endif
