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

using namespace v8;

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
	clipY (state2->clipY1, state2->clipY2);
}

void BGJSCanvasContext::clipY (float y, float y2) {

	flushBuffers();

	state2->clipY1 = y;
	state2->clipY2 = y2;

	if(y==y2) {
		// LOGD("Clipping disabled");
		glDisable(GL_SCISSOR_TEST);
		return;
	}

	y = viewportHeight - EJVector2ApplyTransform( EJVector2Make(0, y), state->transform).y *backingStoreRatio;
	y2 = viewportHeight - EJVector2ApplyTransform( EJVector2Make(0, y2), state->transform).y *backingStoreRatio;

	// LOGD("Clipping y=%f, y2=%f, so going from %f to %f", state2->clipY1, state2->clipY2, y2, (y-y2));

	glEnable(GL_SCISSOR_TEST);
	glScissor(0,y2,5000,y-y2);
	checkGlError("glScissor clipY");
}


void BGJSCanvasContext::resize (int widthp, int heightp, bool resizeOnly) {

	viewportWidth = width = widthp;
	viewportHeight = height = heightp;

	bufferHeight = viewportHeight; //  *= backingStoreRatio;
	bufferWidth = viewportWidth; //  *= backingStoreRatio;

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
