/*
 * TextureBrowser: loads ground textures from a directory given with
 * -textures <directory> and lets the user pick one for texture painting.
 */
#ifndef TEXTUREBROWSER_H
#define TEXTUREBROWSER_H

#include <GL/glew.h>

#include <string>
#include <vector>

struct GroundTexture
{
	std::string name;
	int w = 0;
	int h = 0;
	std::vector<unsigned char> rgba; // top down RGBA
	GLuint glTex = 0;
};

class TextureBrowser
{
public:
	TextureBrowser();
	~TextureBrowser();

	// Loads every image file in dir. Returns the number of textures loaded.
	int LoadDirectory(const std::string& dir);

	int count() const { return (int)textures.size(); }
	bool empty() const { return textures.empty(); }
	int selected = 0;

	void select(int index);
	const GroundTexture* current() const;
	const GroundTexture& at(int index) const { return textures[index]; }

	// Sample the colour of texture index at map texel coordinates. The image
	// is tiled every `scale` map texels (scale = 1 means one image texel per
	// map texel).
	void sample(int index, double mapTexelX, double mapTexelY, double scale, unsigned char out[4]) const;

	void DestroyGLTextures();

private:
	std::vector<GroundTexture> textures;
};

// Decode an image file into top down RGBA. Uses FreeImage when available,
// otherwise falls back to the SDL BMP loader.
bool load_image_rgba(const std::string& path, std::vector<unsigned char>& rgba, int& w, int& h);

#endif // TEXTUREBROWSER_H
