
#include <GL/glew.h>
#include "BFGroundTextures.h"



#include "mapfile.h"
#include "GlobalStuff.h"
#include "SmfReadMap.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

using std::string;
using std::max;

CBFGroundTextures::CBFGroundTextures(CSmfReadMap* rm)
{
	usePBO = false;
	if (GLEW_EXT_pixel_buffer_object && rm->usePBO) {
		glGenBuffers(10, pboIDs);
		currentPBO=0;
		usePBO = true;
	}

	fakevfs* ifs = rm->mapf;
	map = rm;

	bigSquareSize = 128;
	numBigTexX = gs->mapx / bigSquareSize;
	numBigTexY = gs->mapy / bigSquareSize;

	SMFHeader* header = &map->header;
	ifs->Seek(header->tilesPtr);

	tileSize = header->tilesize;

	MapTileHeader tileHeader;
	READPTR_MAPTILEHEADER(tileHeader, rm->mapf);

	tileMap = new int[(header->mapx * header->mapy) / 16];
	tiles = new char[tileHeader.numTiles * SMALL_TILE_SIZE];
	int curTile = 0;

	for (int a = 0; a < tileHeader.numTileFiles; ++a) {
		printf("Loading tile file\n");

		int size;
		ifs->Read(&size, 4);
		size = swabdword(size);
		string name;

		while (true) {
			char ch;
			ifs->Read(&ch, 1);
			/* char, no swab */
			if (ch == 0)
				break;

			name += ch;
		}
		
		// The map directory is normally passed with a trailing slash, but accept
		// both cases and make sure the separator is present.
		string dir = rm->mapf->cpath;
		if (!dir.empty() && dir[dir.size() - 1] != '/' && dir[dir.size() - 1] != '\\')
			dir += '/';
		name = dir + name;
		fakevfs * tileFile= new fakevfs;
		tileFile->f = fopen(name.c_str(),"rb");
		tileFile->Seek(0);

		

		if (!tileFile->f) {
			printf("Couldnt find tile file %s\n", name.c_str());
			memset(&tiles[curTile * SMALL_TILE_SIZE], 0xaa, size * SMALL_TILE_SIZE);
			curTile += size;
			delete tileFile;
			continue;
		}

		printf("Reading tiles\n");

		TileFileHeader tfh;
		READ_TILEFILEHEADER(tfh, tileFile);

		if (strcmp(tfh.magic, "spring tilefile") != 0 || tfh.version != 1 || tfh.tileSize != 32 || tfh.compressionType != 1) {
			char t[500];
			sprintf(t,"Error couldnt open tile file %s\n", name.c_str());
			printf("%s\ntfh.magic=%s tfh.version = %i tfh.tileSize = %i, tfh.compressionType = %i\n",t,tfh.magic,tfh.version,tfh.tileSize,tfh.compressionType);
			exit(0);
		}

		for (int b = 0; b < size; ++b) {
			tileFile->Read(&tiles[curTile * SMALL_TILE_SIZE], SMALL_TILE_SIZE);
			curTile++;
		}
		delete tileFile;
	}

	printf("Reading tile map\n");

	int count = (header->mapx * header->mapy) / 16;
	ifs->Read(tileMap, count * sizeof(int));

	for (int i = 0; i < count; i++) {
		tileMap[i] = swabdword(tileMap[i]);
	}

	tileMapXSize = header->mapx / 4;
	tileMapYSize = header->mapy / 4;

	squares = new GroundSquare[numBigTexX * numBigTexY];

	for (int y = 0; y < numBigTexY; ++y) {
		for (int x = 0; x < numBigTexX; ++x) {
			GroundSquare* square = &squares[y * numBigTexX + x];
			square->texLevel = 1;
			square->lastUsed = -100;
			LoadSquare(x, y, 0);
		}
	}
}

CBFGroundTextures::~CBFGroundTextures(void)
{
	for (int i = 0; i < numBigTexX * numBigTexY; ++i) {
		glDeleteTextures(1, &squares[i].texture);
	}

	delete[] squares;
	delete[] tileMap;
	delete[] tiles;

	if (usePBO) {
		glDeleteBuffers(10,pboIDs);
	}
}


void CBFGroundTextures::SetTexture(int x, int y)
{
	GroundSquare* square = &squares[y * numBigTexX + x];
	glBindTexture(GL_TEXTURE_2D, square->texture);
	square->lastUsed = gs->frameNum;
}

inline bool CBFGroundTextures::TexSquareInView(int btx, int bty) {


	return false;
}

void CBFGroundTextures::DrawUpdate(void)
{

}

int tileoffset[] = {0, 512, 640, 672};

void CBFGroundTextures::LoadSquare(int x, int y, int level)
{
	int size = 1024 >> level;

	GLubyte* buf=NULL; bool usedPBO=false;
	//printf("01\n");
	if (usePBO) {
		if (currentPBO > 9) currentPBO=0;
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pboIDs[currentPBO++]);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, size * size / 2, 0, GL_STREAM_DRAW);

		//map the buffer object into client's memory
		buf = (GLubyte*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
		usedPBO = true;
	}
	//printf("02\n");
	if(buf==NULL) {
		buf = new GLubyte[size * size / 2];
		usedPBO = false;
	}
	//printf("03\n");
	GroundSquare* square = &squares[y * numBigTexX + x];
	square->texLevel = level;
	//printf("04\n");
	int numblocks = 8 / (1 << level);
	//printf("05\n");
	for (int y1 = 0; y1 < 32; y1++) {
		for (int x1 = 0; x1 < 32; x1++) {
			

			char* tile = &tiles[tileMap[(x1 + x * 32) + (y1 + y * 32) * tileMapXSize] * SMALL_TILE_SIZE + tileoffset[level]];
			//printf("char* tile = &tiles[tileMap[(x1 + x * 32) + (y1 + y * 32) * tileMapXSize] * SMALL_TILE_SIZE + tileoffset[level]];\n");
			for (int yt = 0; yt < numblocks; yt++) {
				for (int xt = 0; xt < numblocks; xt++) {
					//printf("a\n");
					GLint* sbuf = (GLint*)&tile[(xt + yt * numblocks) * 8];
					//printf("b\n");
					GLint* dbuf = (GLint*)&buf[(x1 * numblocks + xt + (y1 * numblocks + yt) * (numblocks * 32)) * 8];
					//printf("c\n");
					//copy 2x 4 bytes at once
					dbuf[0] = sbuf[0];
					//printf("d\n");
					dbuf[1] = sbuf[1];
					//printf("e\n");
				}
			}
		}
	}
	//printf("06\n");
	glGenTextures(1, &square->texture);
	//printf("07\n");
	glBindTexture(GL_TEXTURE_2D, square->texture);
	//printf("08\n");
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_PRIORITY, 1);
	//printf("09\n");
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	//printf("10\n");
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//printf("11\n");
	if (GLEW_EXT_texture_edge_clamp) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	if (map->anisotropy != 0.0f)
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, map->anisotropy);

	if (usedPBO) {
		glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
		glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, size, size, 0, size * size / 2, 0);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, 0, 0, GL_STREAM_DRAW); //free it
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
		
	}else{
		glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, size, size, 0, size * size / 2, buf);
		delete[] buf;
	}
}
