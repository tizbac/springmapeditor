#ifndef READMAP_H
#define READMAP_H

// ReadMap.h: interface for the CReadMap class.
//
//////////////////////////////////////////////////////////////////////

#include <GL/glew.h>
#include <string>

// Size, in world units, of one map square.
#define SQUARE_SIZE_ 32

struct MapBitmapInfo
{
	int width, height;
};


class CReadMap
{
public:
	virtual ~CReadMap();

	static CReadMap* LoadMap (const std::string& mapname);
	static CReadMap* LoadMap (const std::string& mapname, std::string path);

	/** The raw height map, one value per map vertex.
	    Values are normalised to [0, 1] (0 = water, 1 = highest ground). */
	virtual float* GetHeightmap() = 0;

	/** Extra per-map bitmaps such as "metal" or "type".
	    The caller must release the returned buffer with FreeInfoMap. */
	virtual unsigned char* GetInfoMap (const std::string& name, MapBitmapInfo* bm) = 0;
	virtual void FreeInfoMap (const std::string& name, unsigned char *data) = 0;

	int width, height;

protected:
	CReadMap(); // use LoadMap
};

#endif /* READMAP_H */
