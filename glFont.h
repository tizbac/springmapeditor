#ifndef _GLFONT_H
#define _GLFONT_H
#include <GL/glew.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <string>

struct GlyphInfo;
class CFontTextureRenderer
{
public:
	CFontTextureRenderer(int width, int height);
	~CFontTextureRenderer();
	void AddGlyph(FT_GlyphSlot slot, int &outX, int &outY);
	GLuint CreateTexture();
	unsigned char* buffer;
	unsigned char* cur;
	int width, height;
private:
	
	
	int curX, curY;
	int curHeight;               // height of highest glyph in current line

	void BreakLine();
};

class CglFont
{
public:
	static CglFont* TryConstructFont(std::string fontFile, int start, int end, float size);

	void glWorldPrint(const char* str) const;
	void glPrintAt(GLfloat x, GLfloat y, float s, const char* str);
	void glPrintColorAt(GLfloat x, GLfloat y, float s, const char* str);
	void glFormatAt(GLfloat x, GLfloat y, float s, const char* fmt, ...);
	void glPrintRight (float x,float y, float s, const char* fmt,...);
	void glPrintCentered (float x,float y, float s, const char* fmt,...);
	void Outline(bool enable, const float* color, const float* outlineColor = NULL); ///< Enable outlining for the next drawing operation.
	void OutlineS(bool enable, const float* outlineColor = NULL); ///< Enable outlining for the next drawing operation.

	// The "outlined" functions are for convenience; their functionality may be obtained by calling Outline() before rendering text.
	void glPrintOutlinedAt(float x, float y, float scale, const char* text, const float* normalColor);
	void glPrintOutlinedRight(float x, float y, float scale, const char* text, const float* normalColor);
	void glPrintOutlinedCentered(float x, float y, float scale, const char* text, const float* normalColor);

	float CalcTextWidth (const char* txt) const;
	float CalcCharWidth (int c) const;
	float CalcTextHeight(const char* text) const;
	inline float GetHeight() const { return fontHeight; }
	CFontTextureRenderer* rend;
	CglFont(int start, int end, const char* fontfile, float size, int texWidth, int texHeight);
	~CglFont();
	GLuint fontTexture;
	int GetCharStart() const { return charstart; }
	int GetCharEnd()   const { return charend; }
	float RenderString(float x, float y, float s, const char* text) const;
	float RenderStringOutlined(float x, float y, float s, const char* text);
private:
	
	void WorldChar(int ch) const;

	int chars;
	int charstart;
	int charend;
	
	float fontHeight;
	bool outline;
	const float* color;
	const float* outlineColor;
	float defaultColor[4];
	GlyphInfo* glyphs;

	static const float* ChooseOutlineColor(const float* textColor);
};

extern CglFont* font;
extern CglFont* smallFont;

#endif /* _GLFONT_H */
