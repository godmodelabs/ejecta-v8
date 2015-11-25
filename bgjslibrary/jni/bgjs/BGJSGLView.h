#ifndef __BGJSGLVIEW_H
#define __BGJSGLVIEW_H	1

#include "BGJSCanvasContext.h"
#include "BGJSView.h"
#include "BGJSInfo.h"
#include "BGJSContext.h"
#include "os-detection.h"

typedef struct __tagAnimationFrameRequest {
	BGJSGLView *view;
	v8::Persistent<v8::Object> callback;
	v8::Persistent<v8::Object> thisObj;
	bool valid;
	int requestId;
} AnimationFrameRequest;

class BGJSGLView : public BGJSView {
public:
	BGJSGLView(BGJSContext *ctx, float pixelRatio);
	virtual ~BGJSGLView();
	virtual void prepareRedraw();
	virtual void endRedraw();
	void endRedrawNoSwap();
	void setTouchPosition(int x, int y);
	void swapBuffers();
	void resize (int width, int height, bool resizeOnly);
	void close ();
	void requestRefresh();
	int requestAnimationFrameForView(v8::Persistent<v8::Object> cb, v8::Persistent<v8::Object> thisObj, int id);
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
