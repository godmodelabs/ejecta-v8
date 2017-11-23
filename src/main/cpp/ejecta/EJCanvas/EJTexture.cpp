#include "EJTexture.h"
#include "lodepng.h"
#include "stdlib.h"

#include "mallocdebug.h"

#define LOG_TAG "EJTexture"

#include <math.h>
#include "NdkMisc.h"

#include <GLES/gl.h>
#include <GLES/glext.h>
#include <stdlib.h>

static GLint textureFilter = GL_LINEAR;

bool EJTexture::smoothScaling() {
	return (textureFilter == GL_LINEAR);
}

void EJTexture::setSmoothScaling (bool smoothScaling) {
	textureFilter = smoothScaling ? GL_LINEAR : GL_NEAREST;
}



EJTexture* EJTexture::initWithPath(const char* path) {
	// Load directly (blocking)
	EJTexture* self = new EJTexture();
	GLubyte *pixels = self->loadPixelsFromPath(path);
	self->createTextureWithPixels(pixels, GL_RGBA);

	return self;
}


EJTexture* EJTexture::initWithWidth (int width, int height, GLenum format) {
	// Create an empty texture
	EJTexture* self = new EJTexture();
	self->fullPath = strdup("[Empty]");
	self->setWidth(width, height);
	self->createTextureWithPixels(NULL, format);

	return self;
}

EJTexture* EJTexture::initWithWidth (int width, int height) {
	// Create an empty RGBA texture
	return EJTexture::initWithWidth(width, height, GL_RGBA);
}

EJTexture* EJTexture::initWithWidth (int widthp, int heightp, GLubyte* pixels) {
	// Creates a texture with the given pixels

	EJTexture* self = new EJTexture();
	self->fullPath = strdup("[From Pixels]");
	self->setWidth(widthp, heightp);

	if( widthp != self->realWidth || heightp != self->realHeight ) {
		GLubyte * pixelsPow2 = (GLubyte *)malloc( self->realWidth * self->realHeight * 4 );
		memset( pixelsPow2, 0, self->realWidth * self->realHeight * 4 );
		for( int y = 0; y < heightp; y++ ) {
			memcpy( &pixelsPow2[y*self->realWidth*4], &pixels[y*widthp*4], widthp * 4 );
		}
		self->createTextureWithPixels(pixelsPow2, GL_RGBA);
		free(pixelsPow2);
	}
	else {
		self->createTextureWithPixels(pixels, GL_RGBA);
	}
	return self;
}

EJTexture* EJTexture::initWithWidth (int widthp, int heightp, GLubyte* pixels, GLenum format, size_t bytePerPixel) {
	// Creates a texture with the given pixels

	EJTexture* self = new EJTexture();
	self->fullPath = strdup("[From Pixels]");
	self->setWidth(widthp, heightp);

	if( widthp != self->realWidth || heightp != self->realHeight ) {
		GLubyte * pixelsPow2 = (GLubyte *)malloc( self->realWidth * self->realHeight * bytePerPixel );
		memset( pixelsPow2, 0, self->realWidth * self->realHeight * bytePerPixel );
		for( int y = 0; y < heightp; y++ ) {
			memcpy( &pixelsPow2[y*self->realWidth*bytePerPixel], &pixels[y*widthp*bytePerPixel], widthp * bytePerPixel );
		}
		self->createTextureWithPixels(pixelsPow2, format);
		free(pixelsPow2);
	}
	else {
		self->createTextureWithPixels(pixels, format);
	}
	return self;
}

EJTexture::~EJTexture() {
	glDeleteTextures (1, &textureId);
}

static int findNextPot(int size) {
	int pot = 1;

	while (pot < size) {
		pot *= 2;
	}
	return pot;
}

void EJTexture::setWidth (int heightp, int widthp) {
	width = widthp;
	height = heightp;



	// The internal (real) size of the texture needs to be a power of two
	realWidth = findNextPot(width);
	realHeight = findNextPot(height);
	/* realWidth = pow(2, ceil (log2 (width)));
	realHeight = pow(2, ceil (log2 (height))); */
}


void EJTexture::createTextureWithPixels (GLubyte *pixels, GLenum formatp) {
	// Release previous texture if we had one
	if( textureId ) {
		glDeleteTextures( 1, &textureId );
		textureId = 0;
	}

	GLint maxTextureSize;
	glGetIntegerv( GL_MAX_TEXTURE_SIZE, &maxTextureSize );

	if( realWidth > maxTextureSize || realHeight > maxTextureSize ) {
		LOGI("Warning: Image %s larger than MAX_TEXTURE_SIZE (%d)", fullPath, maxTextureSize);
	}
	format = formatp;

	bool wasEnabled = true; // glIsEnabled(GL_TEXTURE_2D);
	int boundTexture = 0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &boundTexture);

	glEnable(GL_TEXTURE_2D);
	glGenTextures(1, &textureId);

	// LOGD ("new textureId %u", textureId);
	glBindTexture(GL_TEXTURE_2D, textureId);
	glTexImage2D(GL_TEXTURE_2D, 0, format, realWidth, realHeight, 0, format, GL_UNSIGNED_BYTE, pixels);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, textureFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, textureFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_2D, boundTexture);
	if( !wasEnabled ) {	glDisable(GL_TEXTURE_2D); }
}

void EJTexture::updateTextureWithPixels (GLubyte *pixels, int x, int y, int subWidth, int subHeight) {
	if( !textureId ) { LOGI("No texture to update. Call createTexture... first");	return; }

	bool wasEnabled = glIsEnabled(GL_TEXTURE_2D);
	int boundTexture = 0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &boundTexture);

	glBindTexture(GL_TEXTURE_2D, textureId);
	glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, subWidth, subHeight, format, GL_UNSIGNED_BYTE, pixels);

	glBindTexture(GL_TEXTURE_2D, boundTexture);
	if( !wasEnabled ) {	glDisable(GL_TEXTURE_2D); }
}

GLubyte *EJTexture::loadPixelsFromPath (const char* path) {
	// All CGImage functions return pixels with premultiplied alpha and there's no
	// way to opt-out - thanks Apple, awesome idea.
	// So, for PNG images we use the lodepng library instead.
	return this->loadPixelsWithLodePNGFromPath(path);

	/* if (path.substr(path.length() - 4, 4).compare("png") == 0) {
		this->loadPixelsWit
	} */
}

GLubyte *EJTexture::loadPixelsWithLodePNGFromPath (const char* path) {
	unsigned int w, h;
	unsigned char * origPixels = NULL;
	unsigned int error = lodepng_decode32_file(&origPixels, &w, &h, path);

	if( error ) {
		LOGE("Error Loading image %s - %u: %s", path, error, lodepng_error_text(error));
		return origPixels;
	}

	this->setWidth(w, h);

	// If the image is already in the correct (power of 2) size, just return
	// the original pixels unmodified
	if( width == realWidth && height == realHeight ) {
		return origPixels;
	}

	// Copy the original pixels into the upper left corner of a larger
	// (power of 2) pixel buffer, free the original pixels and return
	// the larger buffer
	else {
		GLubyte * pixels = (GLubyte*)malloc( realWidth * realHeight * 4 );
		memset(pixels, 0x00, realWidth * realHeight * 4 );

		for( int y = 0; y < height; y++ ) {
			memcpy( &pixels[y*realWidth*4], &origPixels[y*width*4], width*4 );
		}

		free( origPixels );
		return pixels;
	}
}

void EJTexture::bind() {
	glBindTexture(GL_TEXTURE_2D, textureId);
}
