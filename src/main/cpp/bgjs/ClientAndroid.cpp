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

#include "BGJSContext.h"
#include "ClientAbstract.h"
#include "ClientAndroid.h"
#include "modules/AjaxModule.h"
#include "modules/BGJSGLModule.h"

#include "jniext.h"

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

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return -1;
    }

    // Get jclass with env->FindClass.
    // Register methods with env->RegisterNatives.
    jclass clazz = env->FindClass("ag/boersego/android/conn/BGJSPushHelper");
	if (clazz == NULL) {
		LOGE("Cannot find class BGJSPushHelper!");
		env->ExceptionClear();
	} else {
		_client->bgjsPushHelper = (jclass)env->NewGlobalRef(clazz);
		jmethodID pushMethod = env->GetStaticMethodID(clazz,
			"registerPush", "(Ljava/lang/String;JJ)I");
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

	/* #ifdef __LP64__
      const char kNativesFileName[] = "natives_blob_64.bin";
      const char kSnapshotFileName[] = "snapshot_blob_64.bin";
    #else
      const char kNativesFileName[] = "natives_blob_32.bin";
      const char kSnapshotFileName[] = "snapshot_blob_32.bin";
    #endif // __LP64__ */

	// v8::V8::InitializeExternalStartupData(argv[0]);
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
    // v8::Isolate::Scope isolate_scope(isolate);
    // LOGD("Initialized isolateScope");

	BGJSContext* ct = new BGJSContext(isolate);

	const char* localeStr = env->GetStringUTFChars(locale, NULL);
	const char* langStr = env->GetStringUTFChars(lang, NULL);
	const char* tzStr = env->GetStringUTFChars(timezone, NULL);
	ct->setLocale(localeStr, langStr, tzStr);
	env->ReleaseStringUTFChars(locale, localeStr);
	env->ReleaseStringUTFChars(lang, langStr);
	env->ReleaseStringUTFChars(timezone, tzStr);
	ct->setClient(_client);
	ct->createContext();
	LOGD("BGJS context created");
	AjaxModule::doRegister(isolate, ct);
	BGJSGLModule::doRegister(isolate, ct);
	LOGD("ClientAndroid init: doRegister done");

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
	BGJSContext* ct = (BGJSContext*) ctxPtr;
	v8::Locker l (ct->getIsolate());
	Isolate::Scope(ct->getIsolate());
	Context::Scope context_scope(*reinterpret_cast<Local<Context>*>(ct->_context));
	_client->envCache = env;
	ct->run(pathStr);
}

JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_timeoutCB(
		JNIEnv * env, jobject obj, jlong ctxPtr, jlong jsCbPtr, jlong thisPtr, jboolean cleanup, jboolean runCb) {
	BGJSContext* context = (BGJSContext*)ctxPtr;
    v8::Isolate* isolate = context->getIsolate();
    v8::Locker l (isolate);
	Isolate::Scope isolateScope(isolate);
	if (DEBUG) {
		LOGD("clientAndroid timeoutCB");
	}

	Context::Scope context_scope(*reinterpret_cast<Local<Context>*>(context->_context));

	HandleScope scope (isolate);
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
			BGJSContext::ReportException(&trycatch);
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
	BGJSContext* context = (BGJSContext*)ctxPtr;
    v8::Isolate* isolate = context->getIsolate();
    v8::Locker l (isolate);
	Isolate::Scope isolateScope(isolate);

	Context::Scope context_scope(*reinterpret_cast<Local<Context>*>(context->_context));

	HandleScope scope(isolate);
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
		BGJSContext::ReportException(&trycatch);
	}
	// TODO: Don't we need to clean up these persistents?
}


