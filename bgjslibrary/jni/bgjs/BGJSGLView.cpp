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

#include <v8.h>

#include "os-detection.h"

#undef DEBUG
// #define DEBUG 1
// #define DEBUG_GL 0
#undef DEBUG_GL
#define LOG_TAG "BGJSGLView"

using namespace v8;

static void checkGlError(const char* op) {
#ifdef DEBUG_GL
	for (GLint error = glGetError(); error; error = glGetError()) {
		LOGI("after %s() glError (0x%x)\n", op, error);
	}
#endif
}

BGJSGLView::BGJSGLView(BGJSContext *ctx, float pixelRatio) :
		BGJSView(ctx, pixelRatio) {

	_firstFrameRequest = 0;
	_nextFrameRequest = 0;
	noFlushOnRedraw = false;
	bzero (_frameRequests, sizeof(_frameRequests));

	context2d = new BGJSCanvasContext(500, 500);
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
}

void BGJSGLView::resize(int widthp, int heightp, bool resizeOnly) {
	context2d->resize(widthp, heightp, resizeOnly);
	this->width = widthp;
	this->height = heightp;

	int count = _cbResize.size();
#ifdef DEBUG
	LOGI("Sending resize event to %i subscribers", count);
#endif
	if (count > 0) {
		TryCatch trycatch;
		HandleScope scope;
		Handle<Value> args[0];
		for (std::vector<Persistent<Object> >::size_type i = 0; i < count;
				i++) {
			Handle<Value> result = _cbResize[i]->CallAsFunction(_jsObj, 0, args);

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
			_frameRequests[i].callback.Dispose();
			_frameRequests[i].view = NULL;
		}
	}

	call(_cbClose);
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

int BGJSGLView::requestAnimationFrameForView(Persistent<Object> cb, Persistent<Object> thisObj,
		int id) {
	// make sure there is still room in the buffer
#ifdef DEBUG
	LOGD("requestAnimation %d %d", _firstFrameRequest, _nextFrameRequest);
#endif

	if (_nextFrameRequest >= MAX_FRAME_REQUESTS) {
		return -1;
	}

	// schedule request

	AnimationFrameRequest *request = &_frameRequests[_nextFrameRequest];
	request->callback = cb;
	request->view = this;
	request->valid = true;
	request->thisObj = thisObj;
	request->requestId = id;
	_nextFrameRequest = (_nextFrameRequest + 1) % MAX_FRAME_REQUESTS;

#ifdef DEBUG
	LOGD("requestAnimation new id %d", request->requestId);
#endif

	requestRefresh();

	return request->requestId;
}
