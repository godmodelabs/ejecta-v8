#include "EJTexture.h"

#ifndef __EJIMAGEDATA_H__
#define __EJIMAGEDATA_H__ 1


class EJImageData {
public:
	int width, height;
	GLubyte *pixels;

	static EJImageData* initWithWidth (int width, int height, GLubyte *pixels);
	EJTexture* getTexture();
	~EJImageData();
};

#endif
