#ifndef __BGJSGLVIEW_H
#define __BGJSGLVIEW_H	1

#include "BGJSCanvasContext.h"
#include "BGJSV8Engine.h"
#include "os-android.h"

/**
 * BGJSGLView
 * Wrapper class around native windows that expose OpenGL operations
 *
 * Copyright 2014 Kevin Read <me@kevin-read.com> and BörseGo AG (https://github.com/godmodelabs/ejecta-v8/)
 * Licensed under the MIT license.
 */

class BGJSGLView : public JNIScope<BGJSGLView, JNIV8Object> {
public:
	BGJSGLView(jobject obj, JNIClassInfo *info) : JNIScope(obj, info) {};

    void setViewData(float pixelRatio, bool doNoClearOnFlip, int width, int heigh);

	static void initializeJNIBindings(JNIClassInfo *info, bool isReload);
	static void initializeV8Bindings(JNIV8ClassInfo *info);

	static jobject jniCreate(JNIEnv *env, jobject obj, jobject engineOb);

    virtual ~BGJSGLView();
    virtual void prepareRedraw();
    virtual void endRedraw();

	void setTouchPosition(int x, int y);
	void swapBuffers();

	BGJSCanvasContext *context2d;

protected:
    bool noFlushOnRedraw = false;
    bool noClearOnFlip = false;
};

#endif
