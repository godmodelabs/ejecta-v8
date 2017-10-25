#ifndef	__BGJSJAVAWRAPPER_H
#define	__BGJSJAVAWRAPPER_H		1

#include <v8.h>
#include <jni.h>
#include <string>
#include "BGJSV8Engine.h"

/**
 * BGJSJavaWrapper
 * Helper class to wrap v8 objects in Java objects
 *
 * Copyright 2014 Kevin Read <me@kevin-read.com> and BörseGo AG (https://github.com/godmodelabs/ejecta-v8/)
 * Licensed under the MIT license.
 */

class BGJSJavaWrapper {
	jclass _jStringClass;
	std::vector<v8::Persistent<v8::Object>*> _v8ObsPersisted;
	std::vector<v8::Persistent<v8::Function>*> _v8FuncsPersisted;
	v8::Persistent<v8::Object> _jsObject;
    bool needsClear;
public:

	BGJSJavaWrapper (const BGJSV8Engine* context, JNIEnv* env, jobject javaObject);

	bool coerceArgToString (JNIEnv* env, jobject &object, jclass& clazz, const char *keyName, const char *valAsString);

	void loC (JNIEnv* env);

	bool apply (v8::Local<v8::Object> &v8obj, jobject &object, jclass &clazz, const char* className, JNIEnv* env,
			char* errBuf, int errBufLen, char* containingObject, bool (*f)(const char*, const char*, int, char**));
	jobject copyObject (bool isArray, v8::Local<v8::Value> &arg, char **classNameUsed, JNIEnv* env, char* errBuf, int errBufLen, int argNum,
			char* containingObject, bool (*f)(const char*, const char*, int, char**));
	bool jsValToJavaVal (char spec, jvalue &jv, v8::Local<v8::Value> &arg, bool isArray, JNIEnv* env,
			std::string &javaSig, int argNum, char* errBuf, int errBufLen, bool isOptional, bool (*f)(const char*, const char*, int, char**));
	void jsToJava (const char* argsSpec, const char* javaMethodName,
			const char* returnType, const bool isStatic, const v8::FunctionCallbackInfo<v8::Value>& args, bool (*f)(const char*, const char*, int, char**));
	int getArgCount (const char* argsSpec, const int argsStrLen);
    void persist(v8::Isolate* isolate, v8::Local<v8::Object> object);
    void SetWeak(typename v8::WeakCallbackInfo<BGJSJavaWrapper>::Callback callback,
                                  v8::WeakCallbackType type);
    v8::Local<v8::Object> getLocalObject();
	~BGJSJavaWrapper();
	void cleanUp();
	const BGJSV8Engine* _context;
	jobject _javaObject;
};


#endif
