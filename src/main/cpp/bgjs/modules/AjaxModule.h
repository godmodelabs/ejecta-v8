#ifndef __AJAXMODULE_H
#define __AJAXMODULE_H	1

#include "../BGJSModule.h"

/**
 * AjaxModule
 * AJAX BGJS extension
 *
 * Copyright 2014 Kevin Read <me@kevin-read.com> and BörseGo AG (https://github.com/godmodelabs/ejecta-v8/)
 * Licensed under the MIT license.
 */

class AjaxModule : public BGJSModule {
	bool initialize();
	~AjaxModule();

public:
	AjaxModule ();
	v8::Local<v8::Value> initWithContext(v8::Isolate* isolate, const BGJSContext* context);
	static void doRequire (v8::Isolate* isolate, v8::Handle<v8::Object> target);
	static void ajax(const v8::FunctionCallbackInfo<v8::Value>& args);
};


#endif
