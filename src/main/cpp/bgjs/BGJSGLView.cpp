//
//  BGJSGLView.m
//  pushlib
//
//  Created by Kevin Read
//
//

#include "BGJSGLView.h"
#include "BGJSCanvasContext.h"

#include "GLcompat.h"

#include <EGL/egl.h>
#include <string.h>

#include <v8.h>

#include "os-detection.h"

#undef DEBUG
// #define DEBUG 1
//#undef DEBUG
// #define DEBUG_GL 0
#undef DEBUG_GL
#define LOG_TAG "BGJSGLView"

using namespace v8;

/**
 * BGJSGLView
 * Wrapper class around native windows that expose OpenGL operations
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

BGJSGLView::BGJSGLView(v8::Isolate* isolate, const BGJSContext *ctx, float pixelRatio, bool doNoClearOnFlip, int width, int height) :
		BGJSView(isolate, ctx, pixelRatio, doNoClearOnFlip) {

	_firstFrameRequest = 0;
	_nextFrameRequest = 0;
	noFlushOnRedraw = false;

	const char* eglVersion = eglQueryString(eglGetCurrentDisplay(), EGL_VERSION);
	LOGD("egl version %s", eglVersion);
	// bzero (_frameRequests, sizeof(_frameRequests));

	context2d = new BGJSCanvasContext(width, height);
	context2d->backingStoreRatio = pixelRatio;
#ifdef DEBUG
	LOGI("pixel Ratio %f", pixelRatio);
#endif
	context2d->create();
}

BGJSGLView::~BGJSGLView() {
	if (context2d) {
		delete (context2d);
	}
}

#ifdef ANDROID
void BGJSGLView::setJavaGl(JNIEnv* env, jobject javaGlView) {
	this->_javaGlView = javaGlView;
	jclass clazz = env->GetObjectClass(javaGlView);
	// this->_javaRedrawMid = env->GetMethodID(clazz, "requestRender", "()V");
}

#endif

void BGJSGLView::prepareRedraw() {
	context2d->startRendering();
}

static unsigned int nextPowerOf2(unsigned int n)
{
    unsigned int p = 1;
    if (n && !(n & (n - 1)))
        return n;

    while (p < n) {
        p <<= 1;
    }
    return p;
}

void BGJSGLView::endRedraw() {
	context2d->endRendering();
	if (!noFlushOnRedraw) {
	    this->swapBuffers();
	}
}

void BGJSGLView::endRedrawNoSwap() {
	context2d->endRendering();
}

void BGJSGLView::setTouchPosition(int x, int y) {
    // A NOP. Subclasses might be interested in this though
}

void BGJSGLView::swapBuffers() {
	// At least HC on Tegra 2 doesn't like this
	EGLDisplay display = eglGetCurrentDisplay();
	EGLSurface surface = eglGetCurrentSurface(EGL_DRAW);
	eglSurfaceAttrib(display, surface, EGL_SWAP_BEHAVIOR, noClearOnFlip ? EGL_BUFFER_PRESERVED : EGL_BUFFER_DESTROYED);
	EGLBoolean res = eglSwapBuffers (display, surface);
	// LOGD("eglSwap %d", (int)res);
	// checkGlError("eglSwapBuffers");
}

void BGJSGLView::resize(Isolate* isolate, int widthp, int heightp, bool resizeOnly) {
#ifdef DEBUG
	LOGI("Resize to %dx%d", widthp, heightp);
#endif	
	context2d->resize(widthp, heightp, resizeOnly);
	this->width = widthp;
	this->height = heightp;

	int count = _cbResize.size();
#ifdef DEBUG
	LOGI("Sending resize event to %i subscribers", count);
#endif
	if (count > 0) {
		Isolate::Scope isolateScope(isolate);
		TryCatch trycatch;
		HandleScope scope (isolate);
		Handle<Value> args[0];
		for (std::vector<Persistent<Object, v8::CopyablePersistentTraits<v8::Object> >*>::size_type i = 0; i < count; i++) {
        	Persistent<Object, v8::CopyablePersistentTraits<v8::Object> >* cb = _cbResize[i];
        	Local<Object> callback = (*reinterpret_cast<Local<Object>*>(cb));
        	LOGD("resize callback call");

			Handle<Value> result = callback->CallAsFunction(callback, 0, args);

			if (result.IsEmpty()) {
				BGJSContext::ReportException(&trycatch);
			}
		}
	}
}

void BGJSGLView::close() {

	// Invalidate all refresh requests
	for (int i = 0; i < MAX_FRAME_REQUESTS; i++) {
		if (_frameRequests[i].valid && _frameRequests[i].view != NULL) {
			_frameRequests[i].valid = false;
			BGJS_CLEAR_PERSISTENT(_frameRequests[i].callback);
			_frameRequests[i].view = NULL;
		}
	}

	LOGD("BGJSGLView close");

	call(Isolate::GetCurrent(), _cbClose);
}

void BGJSGLView::requestRefresh() {
	JNIEnv* env = JNU_GetEnv();
	if (env == NULL) {
		LOGE("Cannot execute AJAX request with no envCache");
		return;
	}
	jclass clazz = env->GetObjectClass(_javaGlView);
	this->_javaRedrawMid = env->GetMethodID(clazz, "requestRender", "()V");
	env->CallVoidMethod(_javaGlView, _javaRedrawMid);

#ifdef DEBUG
	LOGD("Requested refresh");
#endif
}

int BGJSGLView::requestAnimationFrameForView(Isolate* isolate, Handle<Object> cb, Handle<Object> thisObj, int id) {
	HandleScope scope(isolate);
	// make sure there is still room in the buffer
#ifdef DEBUG
	LOGD("requestAnimation %d %d", _firstFrameRequest, _nextFrameRequest);
#endif

	if (_nextFrameRequest >= MAX_FRAME_REQUESTS) {
		return -1;
	}

	// schedule request

	AnimationFrameRequest *request = &_frameRequests[_nextFrameRequest];
	BGJS_RESET_PERSISTENT(isolate, request->callback, cb);
	request->view = this;
	request->valid = true;
	BGJS_RESET_PERSISTENT(isolate, request->thisObj, thisObj);
	request->requestId = id;
	_nextFrameRequest = (_nextFrameRequest + 1) % MAX_FRAME_REQUESTS;

#ifdef DEBUG
	LOGD("requestAnimation new id %d", request->requestId);
#endif

	requestRefresh();

	return request->requestId;
}
