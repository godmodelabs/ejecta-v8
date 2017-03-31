#include "BGJSCanvasContext.h"

#include "BGJSContext.h"
// #include "../ClientAndroid.h"

#include "mallocdebug.h"
#include "v8.h"

#include "../ejecta/EJCanvas/GLcompat.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define LOG_TAG "BGJSCanvasContext"

// #define DEBUG_GL 	1
#undef DEBUG_GL
// #define DEBUG 1
#undef DEBUG

using namespace v8;

/**
 * BGJSCanvasContext
 * Manages a JS Canvas object and its OpenGL state. Loosely based on EJCanvasContext
 *
 * Copyright 2014 Kevin Read <me@kevin-read.com> and BörseGo AG (https://github.com/godmodelabs/ejecta-v8/)
 * Licensed under the MIT license.
 */


static void checkGlError(const char* op) {
#ifdef DEBUG_GL
	for (GLint error = glGetError(); error; error = glGetError()) {
		LOGI("after %s() glError (0x%x)\n", op, error);
	}
#endif
}


BGJSCanvasContext::BGJSCanvasContext(int width, int height) : EJCanvasContext(width, height) {
	// allocate renderbuffer storage
	// glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, viewRenderBuffer);

	stencilBuffer = 0;
	height = 0;
	width = 0;

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	glDisable(GL_DITHER);
	bzero(stateStack2, sizeof(stateStack2));

	glEnable(GL_BLEND);
	checkGlError("glEnable GL_BLEND");

	EJCanvasContext::prepare();
	// setFont("arial", 20);
}

void BGJSCanvasContext::save() {
	stateStack2[stateIndex+1] = stateStack2[stateIndex];

	EJCanvasContext::save();

	state2 = &stateStack2[stateIndex];
}

void BGJSCanvasContext::restore() {
	EJCanvasContext::restore();
	state2 = &stateStack2[stateIndex];
	if (!state2->clip) {
		glDisable(GL_SCISSOR_TEST);
	} else {
		clipRect(state2->clipRect);
	}
}

void BGJSCanvasContext::clipY (float y, float y2) {

	CGRect rect;
	rect.origin.x = 0;
	rect.origin.y = y;
	rect.size.width = 5000;
	rect.size.height = y2 - y;

	this->clipRect(rect);
}

void BGJSCanvasContext::clipRect(CGRect rect) {

	flushBuffers();

	float x, y, x2, y2;

    state2->clipRect = rect;
    state2->clip = true;
    
    // this code will obviously NOT work if the transform includes rotation
    // but it WILL work with translation; charting does not rotate clipping regions,
    // and we would not be able to use the scissor test for that anyway so thats fine for now
    
    // opengl (0,0) is bottom left, canvas (0,0) is top left
    // => invert y
    y = viewportHeight - EJVector2ApplyTransform( EJVector2Make(0, rect.origin.y), state->transform).y; // *backingStoreRatio;
    y2 = viewportHeight - EJVector2ApplyTransform( EJVector2Make(0, rect.origin.y+rect.size.height), state->transform).y; // *backingStoreRatio;
    
    x =  EJVector2ApplyTransform( EJVector2Make(rect.origin.x, 0), state->transform).x; // *backingStoreRatio;
    x2 = EJVector2ApplyTransform( EJVector2Make(rect.origin.x + rect.size.width, 0), state->transform).x; // *backingStoreRatio;
    
    glEnable(GL_SCISSOR_TEST);
    glScissor(x,y2,x2-x,y-y2);
    checkGlError("glScissor clipRect");
}


void BGJSCanvasContext::resize (int widthp, int heightp, bool resizeOnly) {

	viewportWidth = width = widthp;
	viewportHeight = height = heightp;

	bufferHeight = viewportHeight; //  *= backingStoreRatio;
	bufferWidth = viewportWidth; //  *= backingStoreRatio;

	#ifdef DEBUG
		LOGD("resize to %dx%d, buffer %dx%d", viewportWidth, viewportHeight, bufferWidth, bufferHeight);
	#endif

	// reallocate color buffer backing based on the current layer size
#ifndef SIMPLE_STENCIL
    // glBindRenderbufferOES(COMPAT_GL_RENDERBUFFER, viewRenderBuffer);
    // [glContext renderbufferStorage:GL_RENDERBUFFER fromDrawable:layer];

	// reallocate msaa buffer backing
	if(msaaEnabled) {
		glBindRenderbufferOES(COMPAT_GL_RENDERBUFFER, msaaRenderBuffer);
		checkGlError("glBindRenderBufferOES(resize)");
		// glRenderbufferStorageMultisampleAPPLE(GL_RENDERBUFFER, msaaSamples, GL_RGBA8_OES, bufferWidth, bufferHeight);
	}
#endif
	prepare();

#ifndef SIMPLE_STENCIL
	// recreate stencil buffer
	if(stencilBuffer) {
		LOGD("Deleting stencil buffer %u", stencilBuffer);
		glDeleteRenderbuffersOES(1, &stencilBuffer);
		checkGlError("glDeleteRenderbuffersOES(resize)");
		stencilBuffer = 0;
		createStencilBufferOnce();
	}
#endif

     glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    checkGlError("glClear(resize)");

    if (!resizeOnly) {
    	glTranslatef(0, heightp, 0);
    } else {
    	glTranslatef(0, heightp, 0);
    }
	glScalef( 1, -1, 1 );
}

void BGJSCanvasContext::startRendering() {

	//------------------------------
	// bind framebuffer
	//------------------------------
#ifndef SIMPLE_STENCIL
	// glBindFramebufferOES(GL_FRAMEBUFFER_OES, msaaEnabled?msaaFrameBuffer:viewFrameBuffer);
#endif


	glStencilMask(0xff);
	glClear(GL_STENCIL_BUFFER_BIT);
	// glClear(GL_STENCIL_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
	checkGlError("glClear(startRendering)");

	_isRendering = YES;

	setState();
}

void BGJSCanvasContext::endRendering() {
	_isRendering = NO;

	flushBuffers();

#ifndef SIMPLE_STENCIL
	//------------------------------
	// resolve MSAA
	//------------------------------
	if(msaaEnabled) {
		// Apple (and the khronos group) encourages you to discard depth // render buffer contents whenever is possible
		/* GLenum attachments[] = {GL_DEPTH_ATTACHMENT_OES,GL_STENCIL_ATTACHMENT_OES};
		glDiscardFramebufferEXT(COMPAT_GL_FRAMEBUFFER, 2, attachments);
		//Bind both MSAA and View FrameBuffers.
		glBindFramebufferOES(GL_READ_FRAMEBUFFER_APPLE, msaaFrameBuffer);
		glBindFramebufferOES(GL_DRAW_FRAMEBUFFER_APPLE, viewFrameBuffer);
		// Call a resolve to combine both buffers
		glResolveMultisampleFramebufferAPPLE();

		glBindRenderbufferOES(GL_RENDERBUFFER_OES, viewRenderBuffer);

	//------------------------------
	// discard unneeded buffers
	//------------------------------
		const GLenum discards[]  = {GL_STENCIL_ATTACHMENT_OES};
		glBindFramebuffer(GL_FRAMEBUFFER, msaaFrameBuffer);
		glDiscardFramebufferEXT(GL_FRAMEBUFFER,2,discards); */
	} else {
		const GLenum discards[]  = {GL_STENCIL_ATTACHMENT_OES};
		// COMPAT_glBindFramebuffer(COMPAT_GL_FRAMEBUFFER, viewFrameBuffer);
		// TODO: Can we do this somehow?
		// glDiscardFramebufferEXT(COMPAT_GL_FRAMEBUFFER,1,discards);
	}
#endif
	//------------------------------
	// Present final image to screen
	//------------------------------
	// [glContext presentRenderbuffer:GL_RENDERBUFFER_OES];
}
