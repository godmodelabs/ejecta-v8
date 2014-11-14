#ifndef __BGJSGLMODULE_H
#define __BGJSGLMODULE_H	1

#include "../BGJSCanvasContext.h"
#include "../BGJSModule.h"


class BGJSGLModule : public BGJSModule {
	bool initialize();
	~BGJSGLModule();
	BGJSCanvasContext *_canvasContext;

public:
	BGJSGLModule();
	v8::Handle<v8::Value> initWithContext(BGJSContext* context);
	static void doRequire (v8::Handle<v8::Object> target);
	static v8::Handle<v8::Value> create(const v8::Arguments& args);
	static v8::Handle<v8::Value> js_canvas_constructor(const v8::Arguments& args);
	static v8::Handle<v8::Value> js_canvas_getContext(const v8::Arguments& args);
	static void js_context_destruct (v8::Persistent<v8::Value> value, void *data);
	// static v8::Handle<v8::Value> js_context_beginPath(const v8::Arguments& args);
	// static Handle<Value> BGJSCanvasGL:js_context_beginPath(const Arguments& args)

	static v8::Persistent<v8::Function> g_classRefCanvasGL;
	static v8::Persistent<v8::Function> g_classRefContext2dGL;
};


#endif
