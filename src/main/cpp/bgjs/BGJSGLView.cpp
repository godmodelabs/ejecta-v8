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

#include "os-android.h"
#include "../jni/JNIWrapper.h"

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

BGJS_JNI_LINK(BGJSGLView, "ag/boersego/bgjs/BGJSGLView");

void BGJSGLView::initializeJNIBindings(JNIClassInfo *info, bool isReload) {
    info->registerNativeMethod("prepareRedraw", "()V", (void*)BGJSGLView::prepareRedraw);
    info->registerNativeMethod("endRedraw", "()V", (void*)BGJSGLView::endRedraw);
    info->registerNativeMethod("setTouchPosition", "(II)V", (void*)BGJSGLView::setTouchPosition);
    info->registerNativeMethod("setViewData", "(FZII)V", (void*)BGJSGLView::setViewData);
    info->registerNativeMethod("viewWasResized", "(II)V", (void*)BGJSGLView::viewWasResized);
    info->registerMethod("requestAnimationFrame", "(Lag/boersego/bgjs/JNIV8Function;)I");
    info->registerMethod("cancelAnimationFrame", "(I)V");
}

void BGJSGLView::initializeV8Bindings(JNIV8ClassInfo *info) {


}

void BGJSGLView::viewWasResized(JNIEnv *env, jobject objWrapped, int width, int height) {
    auto self = JNIWrapper::wrapObject<BGJSGLView>(objWrapped);

    self->_width = width;
    self->_height = height;
    self->context2d->resize(width, height);
}

void BGJSGLView::setViewData(JNIEnv *env, jobject objWrapped, float pixelRatio, bool doNoClearOnFlip, int width, int height) {
	auto self = JNIWrapper::wrapObject<BGJSGLView>(objWrapped);

    self->onSetViewData(pixelRatio, doNoClearOnFlip, width, height);
}

void BGJSGLView::onSetViewData(float pixelRatio, bool doNoClearOnFlip, int width, int height) {
    _width = width;
    _height = height;
    _pixelRatio = pixelRatio;
    noFlushOnRedraw = false;
    noClearOnFlip = doNoClearOnFlip;

    const char* eglVersion = eglQueryString(eglGetCurrentDisplay(), EGL_VERSION);
    LOGD("egl version %s", eglVersion);
    // bzero (_frameRequests, sizeof(_frameRequests));

    context2d = new BGJSCanvasContext(width, height);
    context2d->backingStoreRatio = pixelRatio;
#ifdef DEBUG
    LOGI("pixel Ratio %f", pixelRatio);
#endif
    context2d->create();
    context2d->resize(width, height);
}

BGJSGLView::~BGJSGLView() {
	if (context2d) {
		delete (context2d);
	}
}

int BGJSGLView::getWidth() {
    return _width;
}

int BGJSGLView::getHeight() {
    return _height;
}

void BGJSGLView::prepareRedraw(JNIEnv *env, jobject objWrapped) {
    auto self = JNIWrapper::wrapObject<BGJSGLView>(objWrapped);

    self->onPrepareRedraw();
}

void BGJSGLView::onPrepareRedraw() {
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

void BGJSGLView::endRedraw(JNIEnv *env, jobject objWrapped) {
    auto self = JNIWrapper::wrapObject<BGJSGLView>(objWrapped);

    self->onEndRedraw();
}

void BGJSGLView::onEndRedraw() {
    context2d->endRendering();
    if (!noFlushOnRedraw) {
        this->swapBuffers();
    }
}

void BGJSGLView::setTouchPosition(JNIEnv *env, jobject objWrapped, int x, int y) {
    auto self = JNIWrapper::wrapObject<BGJSGLView>(objWrapped);

    self->onSetTouchPosition(x, y);
}

void BGJSGLView::onSetTouchPosition(int x, int y) {
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


