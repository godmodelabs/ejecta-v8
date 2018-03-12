#include "EJCanvasContextScreen.h"

#define LOG_TAG "EJCanvasContextScreen"
#include "stdlib.h"
#include "NdkMisc.h"

#include "GLcompat.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// #include "EJApp.h"


EJCanvasContextScreen::EJCanvasContextScreen(short widthp, short heightp) : EJCanvasContext(widthp, heightp) {
	// Work out the final screen size - this takes the scalingMode, canvas size,
	// screen size and retina properties into account

	backingStoreRatio = 1.0f;
	bufferWidth = viewportWidth = width;
	bufferHeight = viewportHeight = height;

	LOGI("Creating ScreenCanvas: size: %dx%d", width, height);
	// Parent constructor creates the frame- and renderbuffers


	// Set up the renderbuffer and some initial OpenGL properties
	// [glview.context renderbufferStorage:GL_RENDERBUFFER fromDrawable:(CAEAGLLayer *)glview.layer];
	COMPAT_glFramebufferRenderbuffer(COMPAT_GL_FRAMEBUFFER, COMPAT_GL_COLOR_ATTACHMENT0, COMPAT_GL_RENDERBUFFER, viewRenderBuffer);

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	glDisable(GL_DITHER);

	glEnable(GL_BLEND);

	this->prepare();
}

void EJCanvasContextScreen::prepare () {
	this->EJCanvasContext::prepare();

	// Flip the screen - OpenGL has the origin in the bottom left corner. We want the top left.
	glTranslatef(0, height, 0);
	glScalef( 1, -1, 1 );
}

EJImageData* EJCanvasContextScreen::getImageDataSx (float sx, float sy, float sw, float sh)  {
	// FIXME: This takes care of the flipped pixel layout and the internal scaling.
	// The latter will mush pixel; not sure how to fix it - print warning instead.

	this->flushBuffers();

	// Read pixels; take care of the upside down screen layout and the backingStoreRatio
	int internalWidth = sw * backingStoreRatio;
	int internalHeight = sh * backingStoreRatio;
	int internalX = sx * backingStoreRatio;
	int internalY = (height-sy-sh) * backingStoreRatio;

	EJColorRGBA * internalPixels = (EJColorRGBA *)malloc(internalWidth * internalHeight * sizeof(EJColorRGBA));
	glReadPixels (internalX, internalY, internalWidth, internalHeight, GL_RGBA, GL_UNSIGNED_BYTE, internalPixels);

	// Flip and scale pixels to requested size
	EJColorRGBA* pixels = (EJColorRGBA *)malloc (sw * sh * sizeof(EJColorRGBA));
	int index = 0;
	for( int y = 0; y < sh; y++ ) {
		for( int x = 0; x < sw; x++ ) {
			int internalIndex = (int)((sh-y-1) * backingStoreRatio) * internalWidth + (int)(x * backingStoreRatio);
			pixels[ index ] = internalPixels[ internalIndex ];
			index++;
		}
	}
	free(internalPixels);

	return EJImageData::initWithWidth (sw, sh, (GLubyte *)pixels);
}

void EJCanvasContextScreen::finish () {
	glFinish();
}

void EJCanvasContextScreen::resetGLContext () {
	// [glview resetContext];
}

void EJCanvasContextScreen::present () {
	this->flushBuffers();

#if 0
	if( msaaEnabled ) {
		//Bind the MSAA and View frameBuffers and resolve
		glBindFramebuffer(GL_READ_FRAMEBUFFER_APPLE, msaaFrameBuffer);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER_APPLE, viewFrameBuffer);
		glResolveMultisampleFramebufferAPPLE();

		glBindRenderbuffer(GL_RENDERBUFFER, viewRenderBuffer);
		[glview.context presentRenderbuffer:GL_RENDERBUFFER];
		glBindFramebuffer(GL_FRAMEBUFFER, msaaFrameBuffer);
	}
	else {
#endif
}
