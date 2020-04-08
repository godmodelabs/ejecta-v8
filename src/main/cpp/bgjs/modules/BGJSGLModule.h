#ifndef __BGJSGLMODULE_H
#define __BGJSGLMODULE_H	1

#include "../BGJSCanvasContext.h"

class BGJSV8Engine2dGL;

/**
 * BGJSGLModule
 * Canvas BGJS extension. This is the glue between Ejectas Canvas and OpenGL draw code, BGJSGLViews context handling and v8.
 *
 * Copyright 2014 Kevin Read <me@kevin-read.com> and BörseGo AG (https://github.com/godmodelabs/ejecta-v8/)
 * Licensed under the MIT license.
 */

class BGJSGLModule  {
public:
	static void doRequire (BGJSV8Engine* engine, v8::Handle<v8::Object> target);

	static void js_canvas_constructor(const v8::FunctionCallbackInfo<v8::Value>& args);
	static void js_canvas_getContext(const v8::FunctionCallbackInfo<v8::Value>& args);

	static v8::Persistent<v8::Function> g_classRefCanvasGL;
	static v8::Persistent<v8::Function> g_classRefContext2dGL;
};


#endif
