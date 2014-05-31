#include "EJImageData.h"
#include <stdlib.h>

 EJImageData* EJImageData::initWithWidth (int widthp, int heightp, GLubyte *pixelsp) {
	EJImageData* self = new EJImageData();
	self->width = widthp;
	self->height = heightp;
	self->pixels = pixelsp;

	return self;
}

EJImageData::~EJImageData() {
	if (pixels != NULL) {
		free(pixels);
	}
}


EJTexture *EJImageData::getTexture () {
	EJTexture *texture = EJTexture::initWithWidth(width, height, pixels);
	return texture;
}
