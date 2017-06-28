#ifndef __BGJSVIEW_H
#define __BGJSVIEW_H	1

#include "EJCanvasContext.h"
#include "EJTexture.h"
#include "BGJSV8Engine.h"
#include "BGJSClass.h"
#include <v8.h>
#include <jni.h>
#include "GLcompat.h"

/**
 * BGJSView
 * A v8 wrapper around a native window
 *
 * Copyright 2014 Kevin Read <me@kevin-read.com> and BörseGo AG (https://github.com/godmodelabs/ejecta-v8/)
 *
 */

class BGJSView : public BGJSClass {
public:
	BGJSView(BGJSV8Engine* engine, float pixelRatio, bool doNoClearOnFlip);
	virtual ~BGJSView();
	v8::Handle<v8::Value> startJS(const char* fnName, const char* configJson, v8::Handle<v8::Value> uiObj, long configId, bool hasIntradayQuotes);
	static void js_view_on(const v8::FunctionCallbackInfo<v8::Value>& args);
	void sendEvent(v8::Handle<v8::Object> eventObjRef);
	void call(std::vector<v8::Persistent<v8::Object, v8::CopyablePersistentTraits<v8::Object> >*> &list);

	v8::Persistent<v8::Object> _jsObj;
	v8::Persistent<v8::Object> _bridgeObj;	// TODO: This is returned by startJS
	std::vector<v8::Persistent<v8::Object, v8::CopyablePersistentTraits<v8::Object> >*> _cbResize;
	std::vector<v8::Persistent<v8::Object, v8::CopyablePersistentTraits<v8::Object> >*> _cbEvent;
	std::vector<v8::Persistent<v8::Object, v8::CopyablePersistentTraits<v8::Object> >*> _cbClose;
	std::vector<v8::Persistent<v8::Object, v8::CopyablePersistentTraits<v8::Object> >*> _cbRedraw;
	int _contentObj = -1;	// This is the object returned by the JS init code

#ifdef ANDROID
	jobject _javaGlView;
	jmethodID _javaRedrawMid;
#endif

    BGJSV8Engine *_engine;
    v8::Persistent<v8::ObjectTemplate> jsViewOT;
	int width, height;
	bool opened;
	float pixelRatio;
	bool noClearOnFlip;

};

#endif
