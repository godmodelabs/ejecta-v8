#ifndef __BGJSGLVIEW_H
#define __BGJSGLVIEW_H	1

#include "BGJSCanvasContext.h"
#include "BGJSView.h"
#include "BGJSInfo.h"
#include "BGJSContext.h"
#include "os-detection.h"

/**
 * BGJSGLView
 * Wrapper class around native windows that expose OpenGL operations
 *
 * Copyright 2014 Kevin Read <me@kevin-read.com> and BörseGo AG (https://github.com/godmodelabs/ejecta-v8/)
 * Licensed under the MIT license.
 */

typedef struct __tagAnimationFrameRequest {
	BGJSGLView *view;
	v8::Persistent<v8::Object> callback;
	v8::Persistent<v8::Object> thisObj;
	bool valid;
	int requestId;
} AnimationFrameRequest;

class BGJSGLView : public BGJSView {
public:
	BGJSGLView(v8::Isolate* isolate, const BGJSContext *ctx, float pixelRatio, bool doNoClearOnFlip, int width, int height);
	virtual ~BGJSGLView();
	virtual void prepareRedraw();
	virtual void endRedraw();
	void endRedrawNoSwap();
	void setTouchPosition(int x, int y);
	void swapBuffers();
	void resize (v8::Isolate* isolate, int width, int height, bool resizeOnly);
	void close ();
	void requestRefresh();
	int requestAnimationFrameForView(v8::Isolate* isolate, v8::Handle<v8::Object> cb,
	    v8::Handle<v8::Object> thisObj, int id);
#ifdef ANDROID
	void setJavaGl(JNIEnv* env, jobject javaGlView);
#endif

	BGJSCanvasContext *context2d;

	AnimationFrameRequest _frameRequests[MAX_FRAME_REQUESTS];
	int _firstFrameRequest;
	int _nextFrameRequest;

protected:
    bool noFlushOnRedraw = false;
};

#endif
