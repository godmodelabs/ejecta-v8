#include "BGJSModule.h"
#include "BGJSContext.h"
#include "BGJSJavaWrapper.h"

#include <assert.h>

using namespace v8;

const BGJSContext* BGJSModule::_bgjscontext = NULL;

/**
 * BGJSModule
 * Base class for an ejecta-v8 extension.
 *
 * Copyright 2014 Kevin Read <me@kevin-read.com> and BörseGo AG (https://github.com/godmodelabs/ejecta-v8/)
 * Licensed under the MIT license.
 */

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "BGJSModule"



BGJSModule::BGJSModule(const char* modName) {
	this->name = std::string (modName);
}

std::string BGJSModule::getName() const {
	return name;
}

void BGJSModule::doRegister (v8::Isolate* isolate, const BGJSContext* context) {
	BGJSModule::_bgjscontext = context;
	BGJSModule::_context = context->_context;
	// BGJSModule::_global.Set(isolate, context->_global.Get(isolate));
}

BGJSModule::~BGJSModule() {
}

void BGJSModule::javaToJsField (v8::Isolate* isolate, const char* fieldName,
        const char fieldType, JNIEnv *env, jobject &jobj, v8::Handle<v8::Object> &jsObj) {
	jclass clazz = env->GetObjectClass(jobj);
	switch (fieldType) {
		case 's': {
			jfieldID fieldId = env->GetFieldID(clazz, fieldName, "Ljava/lang/String;");
			if (fieldId) {
				jstring javaString = (jstring)env->GetObjectField(jobj, fieldId);
				if (javaString) {
					const char *nativeString = env->GetStringUTFChars(javaString, 0);
					jsObj->Set(String::NewFromUtf8(isolate, fieldName), String::NewFromUtf8(isolate, nativeString));
					env->ReleaseStringUTFChars(javaString, nativeString);
				} else {
					LOGD("Couldn't retrieve java string for field %s", fieldName);
				}
			} else {
				LOGD("Couldn't find fieldId for string field %s", fieldName);
			}
			break;
		}
		case 'I': {
			jfieldID fieldId = env->GetFieldID(clazz, fieldName, "I");
			if (fieldId) {
				jint data = env->GetIntField(jobj, fieldId);
				if (env->ExceptionCheck()) {
					LOGD("Couldn't get int field for field %s", fieldName);
				}
				jsObj->Set(String::NewFromUtf8(isolate, fieldName), Integer::New(isolate, (int)data));
			} else {
				LOGD("Couldn't find fieldId for int field %s", fieldName);
			}

			break;
		}
	}
}

extern "C" {
	JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_cleanupNativeFnPtr (JNIEnv * env, jobject obj, jlong ctxPtr, jlong nativePtr);
	JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_cleanupPersistentFunction (JNIEnv * env, jobject obj, jlong ctxPtr, jlong nativePtr);
}

JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_cleanupNativeFnPtr (JNIEnv * env, jobject obj, jlong ctxPtr, jlong nativePtr) {
	if (nativePtr) {
		BGJSContext* context = (BGJSContext*)ctxPtr;
	    v8::Isolate* isolate = context->getIsolate();
	    v8::Locker l (isolate);
		Isolate::Scope isolateScope(isolate);
		BGJSJavaWrapper* wrapper = (BGJSJavaWrapper*)nativePtr;
		// wrapper->cleanUp(env);
		delete (wrapper);
	}
}

JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_cleanupPersistentFunction (JNIEnv * env, jobject obj, jlong ctxPtr, jlong nativePtr) {
	if (nativePtr) {
		Persistent<Function, v8::CopyablePersistentTraits<v8::Function> > func = *(((Persistent<Function, v8::CopyablePersistentTraits<v8::Function> >*)nativePtr));
		BGJS_CLEAR_PERSISTENT(func);
	}
}

