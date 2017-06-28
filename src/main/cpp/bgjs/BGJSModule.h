#ifndef __BGJSMODULE_H
#define __BGJSMODULE_H  1

#include "BGJSClass.h"
#include <string>

/**
 * BGJSModule
 * Base class for an ejecta-v8 extension.
 *
 * Copyright 2014 Kevin Read <me@kevin-read.com> and BörseGo AG (https://github.com/godmodelabs/ejecta-v8/)
 * Licensed under the MIT license.
 */

class BGJSV8Engine;

class BGJSModule : public BGJSClass {
	// attributes
	std::string name;

protected:
	// methods
public:
	// members
	BGJSModule(const char* name);
	std::string getName() const;
	virtual ~BGJSModule() = 0;

	static void javaToJsField (v8::Isolate* isolate, const char* fieldName, const char fieldType,
	        JNIEnv *env, jobject &jobj, v8::Handle<v8::Object> &jsObj);

};

#endif
