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
#include <libplatform/libplatform.h>

#include "BGJSV8Engine.h"
#include "ClientAbstract.h"
#include "ClientAndroid.h"
#include "modules/AjaxModule.h"
#include "modules/BGJSGLModule.h"

#include "jniext.h"
#include "../jni/JNIWrapper.h"
#include "../v8/JNIV8Wrapper.h"
#include "../V8TestClass.h"

using namespace v8;

#include <unistd.h>
extern unsigned int __page_size = getpagesize();

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define  LOG_TAG    "ClientAndroid"
#define	DEBUG false
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

/* Persistent<Object>* __debugPersistentAllocObject(v8::Isolate* isolate, v8::Local<Object>* data, const char* file, int line, const char* func) {
	Persistent<Object>* pers = new Persistent<Object>(isolate, *data);
	
	return pers;
}

Persistent<Function>* __debugPersistentAllocFunction(v8::Isolate* isolate, v8::Local<Function>* data, const char* file, int line, const char* func) {
	Persistent<Function>* pers = new Persistent<Function>(isolate, *data);
	LOGD("BGJS_NEW_PERSISTENT_PTR %p file %s line %i func %s", pers, file, line, func);
	return pers;
} */

ClientAbstract::~ClientAbstract() {
}

ClientAndroid::~ClientAndroid() {
	// TODO: Delete global reference to assetManager
}

const char* ClientAndroid::loadFile(const char* path, unsigned int* length) {
	AAssetManager* mgr = AAssetManager_fromJava(JNU_GetEnv(), this->assetManager);
	AAsset* asset = AAssetManager_open(mgr, path, AASSET_MODE_UNKNOWN);

	if (!asset) {
		if (DEBUG) {
			LOGE("Cannot find file %s", path);
		}
		return NULL;
	}

	const size_t count = (unsigned int)AAsset_getLength(asset);
    if(length) {
        *length = count;
    }
	char *buf = (char*) malloc(count + 1), *ptr = buf;
	bzero(buf, count + 1);
	int bytes_read = 0;
    size_t bytes_to_read = count;

    while ((bytes_read = AAsset_read(asset, ptr, bytes_to_read)) > 0) {
		bytes_to_read -= bytes_read;
		ptr += bytes_read;
	}

	AAsset_close(asset);

	return buf;
}

ClientAndroid* _client = new ClientAndroid();

void ClientAndroid::on (const char* event, void* cbPtr, void *thisObjPtr) {
	JNIEnv* env = JNU_GetEnv();

	jclass clazz = env->FindClass("ag/boersego/bgjs/V8Engine");
	jmethodID mid = env->GetStaticMethodID(clazz, "onFromJS", "(Ljava/lang/String;JJ)V");
	assert(mid);
	assert(clazz);


	jstring eventStr = env->NewStringUTF(event);

	env->CallStaticVoidMethod(clazz, mid, eventStr, (jlong)cbPtr, (jlong)thisObjPtr);
}

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return -1;
    }

	JNIWrapper::init(vm);
	JNIV8Wrapper::registerObject<V8TestClass>();

    // Get jclass with env->FindClass.
    // Register methods with env->RegisterNatives.
    jclass clazz = env->FindClass("ag/boersego/android/conn/BGJSPushHelper");
	if (clazz == NULL) {
		LOGE("Cannot find class BGJSPushHelper!");
		env->ExceptionClear();
	} else {
		_client->bgjsPushHelper = (jclass)env->NewGlobalRef(clazz);
		jmethodID pushMethod = env->GetStaticMethodID(clazz,
			"registerPush", "(Ljava/lang/String;JJZ)I");
		if (pushMethod) {
			_client->bgjsPushSubscribeMethod = pushMethod;
		} else {
			LOGE("Cannot find static method BGJSPushHelper.registerPush!");
		}

		pushMethod = env->GetStaticMethodID(clazz, "unregisterPush", "(I)V");

		if (pushMethod) {
			_client->bgjsPushUnsubscribeMethod = pushMethod;
		} else {
			LOGE("Cannot find static method BGJSPushHelper.registerPush!");
		}
	}

	clazz = env->FindClass("ag/boersego/android/conn/BGJSWebPushHelper");
	if (clazz == NULL) {
		LOGE("Cannot find class BGJSWebPushHelper!");
		env->ExceptionClear();
	} else {
		_client->bgjsWebPushHelper = (jclass)env->NewGlobalRef(clazz);
		jmethodID pushMethod = env->GetStaticMethodID(clazz,
													  "registerPush", "(Ljava/lang/String;JJJJ)Lag/boersego/android/conn/BGJSWebPushHelper$JSWebPushSubscription;");
		if (pushMethod) {
			_client->bgjsWebPushSubscribeMethod = pushMethod;
		} else {
			LOGE("Cannot find static method BGJSWebPushHelper.registerPush!");
		}
	}

    clazz = env->FindClass("ag/boersego/android/conn/BGJSWebPushHelper$JSWebPushSubscription");
    if (clazz == NULL) {
        LOGE("Cannot find class BGJSWebPushHelper$JSWebPushSubscription!");
        env->ExceptionClear();
    } else {
        jmethodID unsubscribeMethod = env->GetMethodID(clazz,
                                                      "unbind", "()Z");
        if (unsubscribeMethod) {
            _client->bgjsWebPushSubUnsubscribeMethod = unsubscribeMethod;
        } else {
            LOGE("Cannot find static method BGJSWebPushHelper$JSWebPushSubscription.unbind!");
        }
    }

	clazz = env->FindClass("ag/boersego/chartingjs/ChartingV8Engine");

	if (clazz == NULL) {
		LOGE("Cannot find class ChartingV8Engine!");
		env->ExceptionClear();
	} else {
		_client->chartingV8Engine = (jclass)env->NewGlobalRef(clazz);
		_client->v8EnginegetIAPState = env->GetStaticMethodID(clazz, "getIAPState", "(Ljava/lang/String;)Z");
	}


    return JNI_VERSION_1_6;
}

JNIEnv* JNU_GetEnv() {
	JNIEnv* env;
	_client->cachedJVM->GetEnv((void**) &env, JNI_VERSION_1_6);
	return env;
}

JNIEXPORT jlong JNICALL Java_ag_boersego_bgjs_ClientAndroid_initialize(
		JNIEnv * env, jobject obj, jobject assetManager, jobject v8Engine, jstring locale, jstring lang,
        jstring timezone, jfloat density, jstring deviceClass) {

	#if DEBUG
		LOGD("bgjs initialize, AM %p this %p", assetManager, _client);
	#endif
	_client->assetManager = env->NewGlobalRef(assetManager);
	_client->envCache = env;
	env->GetJavaVM(&(_client->cachedJVM));

	_client->v8Engine = v8Engine;

	LOGI("Creating default platform");
    v8::Platform* platform = v8::platform::CreateDefaultPlatform();
    LOGD("Created default platform %p", platform);
    v8::V8::InitializePlatform(platform);
    LOGD("Initialized platform");
    v8::V8::Initialize();
    LOGD("Initialized v8");
    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator =
          v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    LOGI("Initialized createParams");
    v8::Isolate* isolate = v8::Isolate::New(create_params);
    v8::Locker l (isolate);
	v8::Isolate::Scope isolateScope(isolate);
    LOGD("Initialized Isolate %p", isolate);

	BGJSV8Engine* ct = new BGJSV8Engine(isolate, v8Engine);

	const char* localeStr = env->GetStringUTFChars(locale, NULL);
	const char* langStr = env->GetStringUTFChars(lang, NULL);
	const char* tzStr = env->GetStringUTFChars(timezone, NULL);
	const char* deviceClassStr = env->GetStringUTFChars(deviceClass, NULL);
	ct->setLocale(localeStr, langStr, tzStr, deviceClassStr);
	env->ReleaseStringUTFChars(locale, localeStr);
	env->ReleaseStringUTFChars(lang, langStr);
	env->ReleaseStringUTFChars(timezone, tzStr);
    env->ReleaseStringUTFChars(deviceClass, deviceClassStr);
	ct->setClient(_client);
	ct->createContext();
	LOGD("BGJS context created");

	ct->registerModule("ajax", AjaxModule::doRequire);
	ct->registerModule("canvas", BGJSGLModule::doRequire);
	LOGD("ClientAndroid init: registerModule done");

	return (jlong) ct;
}

JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_run(JNIEnv * env,
		jobject obj, jlong ctxPtr, jstring path) {
	if (DEBUG) {
		LOGD("clientAndroid run");
	}
    const char* pathStr = env->GetStringUTFChars(path, 0);
	BGJSV8Engine* ct = (BGJSV8Engine*) ctxPtr;
	_client->envCache = env;
	ct->run(pathStr);
}

JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_timeoutCB(
		JNIEnv * env, jobject obj, jlong ctxPtr, jlong jsCbPtr, jlong thisPtr, jboolean cleanup, jboolean runCb) {
	BGJSV8Engine* context = (BGJSV8Engine*)ctxPtr;
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
			BGJSV8Engine::ReportException(&trycatch);
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

JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_runCBBoolean (JNIEnv * env, jobject obj, jlong ctxPtr, jlong cbPtr, jlong thisPtr, jboolean b) {
	BGJSV8Engine* context = (BGJSV8Engine*)ctxPtr;
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

	LOGD("runOn %llu %llu", cbPtr, thisPtr);

	int argcount = 1;
	Handle<Value> argarray[] = { Boolean::New(isolate, b ? true : false)};

	Handle<Value> result = fn->Call(thisObj, argcount, argarray);
	if (result.IsEmpty()) {
		BGJSV8Engine::ReportException(&trycatch);
	}
	// TODO: Don't we need to clean up these persistents?
    // => No, because they are pointers to a persistent that is stored elsewhere?!
}


