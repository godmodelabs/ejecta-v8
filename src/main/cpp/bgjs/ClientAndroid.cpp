/**
 * ClientAndroid
 * The bridge between v8 and Android.
 *
 * Copyright 2014 Kevin Read <me@kevin-read.com> and BörseGo AG (https://github.com/godmodelabs/ejecta-v8/)
 *
 */

#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <sstream>
#include <assert.h>

#include "mallocdebug.h"

#include "lodepng.h"

#include <stdio.h>
#include <stdlib.h>
#include <v8.h>

#include "BGJSV8Engine.h"
#include "modules/BGJSGLModule.h"
#include "BGJSGLView.h"

#include "jniext.h"
#include "../jni/JNIWrapper.h"
#include "../v8/JNIV8Wrapper.h"

using namespace v8;


#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define  LOG_TAG    "ClientAndroid"
#define	DEBUG false
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)


jint JNI_OnLoad(JavaVM* vm, void* reserved)  {
    if(!JNIWrapper::isInitialized()) {
        JNIWrapper::init(vm);
		JNIV8Wrapper::init();

		JNIWrapper::registerObject<BGJSV8Engine>();
        JNIV8Wrapper::registerObject<BGJSGLView>();
    }

    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_initialize(
		JNIEnv * env, jobject obj, jobject assetManager, jobject v8Engine, jstring locale, jstring lang,
        jstring timezone, jfloat density, jstring deviceClass, jboolean debug) {

	auto ct = JNIV8Wrapper::wrapObject<BGJSV8Engine>(v8Engine);
	ct->setAssetManager(assetManager);

	const char* localeStr = env->GetStringUTFChars(locale, NULL);
	const char* langStr = env->GetStringUTFChars(lang, NULL);
	const char* tzStr = env->GetStringUTFChars(timezone, NULL);
	const char* deviceClassStr = env->GetStringUTFChars(deviceClass, NULL);
	ct->setLocale(localeStr, langStr, tzStr, deviceClassStr);
	ct->setDensity(density);
	ct->setDebug(debug);
	env->ReleaseStringUTFChars(locale, localeStr);
	env->ReleaseStringUTFChars(lang, langStr);
	env->ReleaseStringUTFChars(timezone, tzStr);
    env->ReleaseStringUTFChars(deviceClass, deviceClassStr);
	ct->createContext();
	LOGD("BGJS context created");

	ct->registerModule("canvas", BGJSGLModule::doRequire);
	LOGD("ClientAndroid init: registerModule done");
}

JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_timeoutCB(
		JNIEnv * env, jobject obj, jobject engine, jlong jsCbPtr, jlong thisPtr, jboolean cleanup, jboolean runCb) {
	auto context = JNIWrapper::wrapObject<BGJSV8Engine>(engine);
    v8::Isolate* isolate = context->getIsolate();
    v8::Locker l (isolate);
	Isolate::Scope isolateScope(isolate);
	if (DEBUG) {
		LOGD("clientAndroid timeoutCB");
	}
	HandleScope scope (isolate);
	Context::Scope context_scope(context->getContext());

	TryCatch trycatch;

	// Persistent<Function>* callbackPers = (Persistent<Function>*) jsCbPtr;
	WrapPersistentObj* wo = (WrapPersistentObj*)thisPtr;
	Local<Object> thisObj = (*reinterpret_cast<Local<Object>*>(&wo->obj));
	WrapPersistentFunc* ws = (WrapPersistentFunc*)jsCbPtr;
	Local<Function> callbackP = Local<Function>::New(isolate, *reinterpret_cast<Local<Function>*>(&ws->callbackFunc));


	if (runCb) {
		int argcount = 0;
		Handle<Value> argarray[] = { };

		Handle<Value> result = callbackP->Call(thisObj, argcount, argarray);
		if (result.IsEmpty()) {
			context->forwardV8ExceptionToJNI(&trycatch);
		}
		if (DEBUG) {
			LOGI("timeoutCb finished");
		}
	}

	if (cleanup) {
		if (DEBUG) {
			LOGI("timeoutCb cleaning up");
		}
		BGJS_CLEAR_PERSISTENT(wo->obj)
		delete(wo);
		BGJS_CLEAR_PERSISTENT(ws->callbackFunc)
		delete(ws);
	}
}

JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_runCBBoolean (JNIEnv * env, jobject obj, jobject engine, jlong cbPtr, jlong thisPtr, jboolean b) {
	auto context = JNIWrapper::wrapObject<BGJSV8Engine>(engine);
    v8::Isolate* isolate = context->getIsolate();
    v8::Locker l (isolate);
	Isolate::Scope isolateScope(isolate);

	HandleScope scope(isolate);
	Context::Scope context_scope(context->getContext());

	TryCatch trycatch;
	Persistent<Function>* fnPersist = static_cast<Persistent<Function>*>((void*)cbPtr);
	Persistent<Object>* thisObjPersist = static_cast<Persistent<Object>*>((void*)thisPtr);
	Local<Function> fn = (*reinterpret_cast<Local<Function>*>(fnPersist));
	Local<Object> thisObj = (*reinterpret_cast<Local<Object>*>(thisObjPersist));

	int argcount = 1;
	Handle<Value> argarray[] = { Boolean::New(isolate, b ? true : false)};

	Handle<Value> result = fn->Call(thisObj, argcount, argarray);
	if (result.IsEmpty()) {
		context->forwardV8ExceptionToJNI(&trycatch);
	}
	// TODO: Don't we need to clean up these persistents?
    // => No, because they are pointers to a persistent that is stored elsewhere?!
}


