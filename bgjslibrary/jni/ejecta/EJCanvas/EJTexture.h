#ifndef __EJTEXTURE_H
#define __EJTEXTURE_H	1

#include "GLcompat.h"


using namespace std;

class EJTexture {
public:
	// members
	short width, height, realWidth, realHeight;
	GLuint textureId;

	// methods
	static EJTexture* initWithPath (const char* path);
	static EJTexture* initWithWidth (int width, int height, GLenum format);
	static EJTexture* initWithWidth (int width, int height);
	static EJTexture* initWithWidth (int width, int height, GLubyte* pixels);
	static EJTexture* initWithWidth (int widthp, int heightp, GLubyte* pixels, GLenum format, size_t bytePerPixel);

	~EJTexture();

	void setWidth (int width, int height);
	void createTextureWithPixels (GLubyte *pixels, GLenum format);
	void updateTextureWithPixels (GLubyte *pixels, int x, int y, int width, int height);

	GLubyte *loadPixelsFromPath (const char* path);
	void bind();

	static void setSmoothScaling(bool smoothScaling);
	static bool smoothScaling();

private:
	const char* fullPath;
	GLenum format;
	GLubyte *loadPixelsWithLodePNGFromPath (const char* path);
};

#endif
