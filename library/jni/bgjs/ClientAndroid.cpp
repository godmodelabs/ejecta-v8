/*/*
 *
 *    Z   z                                  //////////////_               the
 *          Z   O               __\\\\@   //^^        _-    \///////    sleeping
 *       Z    z   o       _____((_     \-/ ____/ /   {   { \\       }     ant
 *                  o    0__________\\\---//____/----//__|-^\\\\\\\\     eater
 *
 * Project Myrmecophaga - the rabid anteater taking a powernap
 *
 * Author: Kevin Read
 *
 * Copyright 2012 Boerse Go AG
 */

#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <sstream>
#include <assert.h>

#include "mallocdebug.h"

#include "lodepng/lodepng.h"

#include <stdio.h>
#include <stdlib.h>
#include <v8.h>

#include "BGJSContext.h"
#include "ClientAbstract.h"
#include "ClientAndroid.h"
#include "modules/AjaxModule.h"
#include "modules/BGJSGLModule.h"

#include "jniext.h"

using namespace v8;

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define  LOG_TAG    "ClientAndroid"
#define	DEBUG	false
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)



ClientAbstract::~ClientAbstract() {
}

ClientAndroid::~ClientAndroid() {
	// TODO: Delete global reference to assetManager
}

jint ClientAndroid::throwNoSuchFieldError( JNIEnv *env, char *message ) {
	jclass		 exClass;
	const char		*className = "java/lang/NoSuchFieldError" ;

	if (!env) {
		env = JNU_GetEnv();
	}

	exClass = env->FindClass (className);
	if ( exClass == NULL ) {
		LOGE ("Cannot throw exception, class not found!");
	}

	return env->ThrowNew (exClass, message);
}

const char* ClientAndroid::loadFile(const char* path) {
	if (DEBUG) {
		LOGD("loadFile: %s", path);
	}
	AAssetManager* mgr = AAssetManager_fromJava(JNU_GetEnv(),
			this->assetManager);
	AAsset* asset = AAssetManager_open(mgr, path, AASSET_MODE_UNKNOWN);

	if (!asset) {
		if (DEBUG) {
			LOGE("Cannot find file %s", path);
		}
		return NULL;
	} /* else {
		LOGD("Opened path %s", path);
	} */

	const int count = AAsset_getLength(asset);
	char *buf = (char*) malloc(count + 1), *ptr = buf;
	bzero(buf, count + 1);
	int bytes_read = 0, bytes_to_read = count;
	std::stringstream str;

	while ((bytes_read = AAsset_read(asset, ptr, bytes_to_read)) > 0) {
		bytes_to_read -= bytes_read;
		ptr += bytes_read;
		/* str << "Read " << bytes_read << " bytes from total of " << count << ", "
				<< bytes_to_read << " left\n";
		LOGI(str.str().c_str()); */

	}
	AAsset_close(asset);
	return buf;
}

const char* ClientAndroid::loadFile(const char* path, unsigned int* length) {
	#if DEBUG
		LOGD("loadFile: %s, this %p, am %p", path, this, this->assetManager);
	#endif
	AAssetManager* mgr = AAssetManager_fromJava(JNU_GetEnv(),
			this->assetManager);
	AAsset* asset = AAssetManager_open(mgr, path, AASSET_MODE_UNKNOWN);

	if (!asset) {
		if (DEBUG) {
			LOGE("Cannot find file %s", path);
		}
		return NULL;
	} /* else {
		LOGD("Opened path %s", path);
	} */

	const int count = AAsset_getLength(asset);
	*length = count;
	char *buf = (char*) malloc(count + 1), *ptr = buf;
	bzero(buf, count + 1);
	int bytes_read = 0, bytes_to_read = count;
	std::stringstream str;

	while ((bytes_read = AAsset_read(asset, ptr, bytes_to_read)) > 0) {
		bytes_to_read -= bytes_read;
		ptr += bytes_read;
		/* str << "Read " << bytes_read << " bytes from total of " << count << ", "
				<< bytes_to_read << " left\n";
		LOGI(str.str().c_str()); */

	}
	AAsset_close(asset);
	return buf;
}

ClientAndroid* _client = new ClientAndroid();

static void* g_func_ptr;

typedef long unsigned int *_Unwind_Ptr;

/* Stubbed out in libdl and defined in the dynamic linker.
 * Same semantics as __gnu_Unwind_Find_exidx().
 */
/* extern "C" _Unwind_Ptr dl_unwind_find_exidx(_Unwind_Ptr pc, int *pcount);
extern "C" _Unwind_Ptr __gnu_Unwind_Find_exidx(_Unwind_Ptr pc, int *pcount) {
	return dl_unwind_find_exidx(pc, pcount);
} */

void ClientAndroid::on (const char* event, void* cbPtr, void *thisObjPtr) {
	JNIEnv* env = JNU_GetEnv();

	jclass clazz = env->FindClass("ag/boersego/bgjs/V8Engine");
	jmethodID mid = env->GetStaticMethodID(clazz, "onFromJS", "(Ljava/lang/String;JJ)V");
	assert(mid);
	assert(clazz);


	jstring eventStr = env->NewStringUTF(event);

	env->CallStaticVoidMethod(clazz, mid, eventStr, (jlong)cbPtr, (jlong)thisObjPtr);
}

/*
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *jvm, void *reserved) {

	// when i throw exception, linker maybe can't find __gnu_Unwind_Find_exidx(lazy binding issue??)
	// so I force to bind this symbol at shared object load time
	// g_func_ptr = (void*) __gnu_Unwind_Find_exidx;

	JNIEnv *env;
	_client->cachedJVM = jvm;
	if (jvm->GetEnv((void**) &env, JNI_VERSION_1_6)) {
		LOGE("Could not get JNIEnv*");
		return JNI_ERR;
	}
	return JNI_VERSION_1_6;
} */

JNIEnv* JNU_GetEnv() {
	JNIEnv* env;
	_client->cachedJVM->GetEnv((void**) &env, JNI_VERSION_1_6);
	return env;
}

JNIEXPORT jlong JNICALL Java_ag_boersego_bgjs_ClientAndroid_initialize(
		JNIEnv * env, jobject obj, jobject assetManager, jobject v8Engine, jstring locale, jstring lang, jstring timezone, jfloat density) {

	#if DEBUG
		LOGD("bgjs initialize, AM %p this %p", assetManager, _client);
	#endif
	_client->assetManager = env->NewGlobalRef(assetManager);
	_client->envCache = env;
	env->GetJavaVM(&(_client->cachedJVM));

	_client->v8Engine = v8Engine;

	BGJSContext* ct = new BGJSContext();

	const char* localeStr = env->GetStringUTFChars(locale, NULL);
	const char* langStr = env->GetStringUTFChars(lang, NULL);
	const char* tzStr = env->GetStringUTFChars(timezone, NULL);
	ct->setLocale(localeStr, langStr, tzStr);
	env->ReleaseStringUTFChars(locale, localeStr);
	env->ReleaseStringUTFChars(lang, langStr);
	env->ReleaseStringUTFChars(timezone, tzStr);
	ct->setClient(_client);
	ct->createContext();
	AjaxModule::doRegister(ct);
	BGJSGLModule::doRegister(ct);

	ct->registerModule("ajax", AjaxModule::doRequire);
	ct->registerModule("canvas", BGJSGLModule::doRequire);

	return (jlong) ct;
}

JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_load(JNIEnv * env,
		jobject obj, jlong ctxPtr, jstring path) {
	BGJSContext* ct = (BGJSContext*) ctxPtr;
	const char* pathStr = env->GetStringUTFChars(path, 0);
	Persistent<Script> res = ct->load(pathStr);
	if (!res.IsEmpty()) {
		ct->_script = res;
	}
	env->ReleaseStringUTFChars(path, pathStr);
}

JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_run(JNIEnv * env,
		jobject obj, jlong ctxPtr) {
	BGJSContext* ct = (BGJSContext*) ctxPtr;
	_client->envCache = env;
	ct->run();
}

JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_timeoutCB(
		JNIEnv * env, jobject obj, jlong ctxPtr, jlong jsCbPtr, jlong thisPtr, jboolean cleanup, jboolean runCb) {
	BGJSContext* context = (BGJSContext*)ctxPtr;

	v8::Locker l;
	Context::Scope context_scope(context->_context);

	HandleScope scope;
	TryCatch trycatch;

	// Persistent<Function>* callbackPers = (Persistent<Function>*) jsCbPtr;
	WrapPersistentObj* wo = (WrapPersistentObj*)thisPtr;
	Persistent<Object> thisObj = wo->obj;
	WrapPersistentFunc* ws = (WrapPersistentFunc*)jsCbPtr;
	Persistent<Function> callbackP = ws->callbackFunc;

	if (DEBUG) {
		LOGI("timeoutCb called, cbPtr is %llu, thisObj %llu, ctxPtr %llu", jsCbPtr, thisPtr, ctxPtr);
	}

	if (runCb) {
		int argcount = 0;
		Handle<Value> argarray[] = { };

		Handle<Value> result = callbackP->Call(thisObj, argcount, argarray);
		if (result.IsEmpty()) {
			BGJSContext::ReportException(&trycatch);
		}
	}

	if (cleanup) {
		if (DEBUG) {
			LOGI("timeoutCb cleaning up");
		}
		thisObj.Dispose();
		delete(wo);
		callbackP.Dispose();
		delete(ws);
	}
}

JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_runCBBoolean (JNIEnv * env, jobject obj, jlong ctxPtr, jlong cbPtr, jlong thisPtr, jboolean b) {
	BGJSContext* context = (BGJSContext*)ctxPtr;

	v8::Locker l;
	Context::Scope context_scope(context->_context);

	HandleScope scope;
	TryCatch trycatch;
	Persistent<Function> fn = static_cast<Function*>((Function*)cbPtr);
	Persistent<Object> thisObj = static_cast<Object*>((void*)thisPtr);

	LOGD("runOn %llu %llu", cbPtr, thisPtr);

	int argcount = 1;
	Handle<Value> argarray[] = { Boolean::New(b ? true : false)};

	Handle<Value> result = fn->Call(thisObj, argcount, argarray);
	if (result.IsEmpty()) {
		BGJSContext::ReportException(&trycatch);
	}
}


