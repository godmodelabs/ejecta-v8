#ifndef __BGJSV8Engine_H
#define __BGJSV8Engine_H 1

#include <v8.h>
#include <jni.h>
#include <map>
#include <string>
#include <set>
#include <mallocdebug.h>
#include <stdlib.h>
#include <uv.h>

#include "os-android.h"

#include "../jni/jni.h"

/**
 * BGJSV8Engine
 * Manages a v8 context and exposes script load and execute functions
 *
 * Copyright 2014 Kevin Read <me@kevin-read.com> and BörseGo AG (https://github.com/godmodelabs/ejecta-v8/)
 * Licensed under the MIT license.
 */

// see V8LockerLogger for debugging locks:
//#define V8_LOCK_LOGGING 1

class BGJSGLView;

typedef  void (*requireHook) (class BGJSV8Engine* engine, v8::Handle<v8::Object> target);

typedef enum EBGJSV8EngineEmbedderData {
    kContext = 1,
    FIRST_UNUSED = 2
} EBGJSV8EngineEmbedderData;

class BGJSV8Engine : public JNIObject {
	friend class JNIWrapper;
public:
	enum class EState {
		kInitial,
		kStarting,
		kStarted,
		kStopping,
		kStopped
	};

	struct Options {
		jobject assetManager;
		const char *commonJSPath;
		int maxHeapSize;
	};

	BGJSV8Engine(jobject obj, JNIClassInfo *info);
	virtual ~BGJSV8Engine();

	/**
	 * returns the engine instance for the specified isolate
	 */
	static BGJSV8Engine* GetInstance(v8::Isolate* isolate);

    v8::MaybeLocal<v8::Value> require(std::string baseNameStr);
    uint8_t requestEmbedderDataIndex();
    bool registerModule(const char *name, requireHook f);
	bool registerJavaModule(jobject module);

	v8::Isolate* getIsolate() const;
	v8::Local<v8::Context> getContext() const;

	bool forwardJNIExceptionToV8() const;
	bool forwardV8ExceptionToJNI(v8::TryCatch* try_catch, bool throwOnMainThread = true) const;

	char* loadFile(const char* path, unsigned int* length = nullptr) const;

//	static void js_global_logMemory(const v8::FunctionCallbackInfo<v8::Value> &);
	static void js_process_nextTick(const v8::FunctionCallbackInfo<v8::Value> &);
	static void js_global_setTimeout (const v8::FunctionCallbackInfo<v8::Value>& info);
	static void js_global_clearTimeoutOrInterval (const v8::FunctionCallbackInfo<v8::Value>& info);
	static void js_global_setInterval (const v8::FunctionCallbackInfo<v8::Value>& info);

	v8::MaybeLocal<v8::Value> parseJSON(v8::Handle<v8::String> source) const;
	v8::MaybeLocal<v8::Value> stringifyJSON(v8::Handle<v8::Object> source, bool pretty = false) const;

    const char* enqueueMemoryDump(const char *basePath);

	/**
     * cache JNI class references
     */
    static void initJNICache();

    void trace(const v8::FunctionCallbackInfo<v8::Value> &info);
	void log(int level, const v8::FunctionCallbackInfo<v8::Value>& args);
	void doAssert(const v8::FunctionCallbackInfo<v8::Value> &info);

	void pause();
	void unpause();

    // @TODO: make private after moving java methods inside class
    void shutdown();

    void start(const Options* options);

    const EState getState() const;
private:
	struct RejectedPromiseHolder {
		v8::Persistent<v8::Promise> promise;
		v8::Persistent<v8::Value> value;
		bool handled, collected;
	};

	struct TaskHolder {
	    v8::Persistent<v8::Function> callback;
	};

	struct TimerHolder {
		uv_timer_t handle;
		bool scheduled, cleared, stopped, repeats, closed;
		uint64_t id, delay, repeat;
		v8::Persistent<v8::Function> callback;
		JNIRetainedRef<BGJSV8Engine> engine;
	};

	struct RunnableHolder {
		jobject runnable;
	};

	uint64_t createTimer(v8::Local<v8::Function> callback, uint64_t delay, uint64_t repeat);
	bool forwardV8ExceptionToJNI(std::string messagePrefix, v8::Local<v8::Value> exception, v8::Local<v8::Message> message, bool throwOnMainThread = false) const;

	// utility method to convert v8 values to readable strings for debugging
	const std::string toDebugString(v8::Handle<v8::Value> source) const;

	// called by JNIWrapper
	static void initializeJNIBindings(JNIClassInfo *info, bool isReload);

	static void JavaModuleRequireCallback(BGJSV8Engine *engine, v8::Handle<v8::Object> target);

    static void OnGCCompletedForDump(v8::Isolate* isolate, v8::GCType type,
                                     v8::GCCallbackFlags flags);

    static void PromiseRejectionHandler(v8::PromiseRejectMessage message);
	static void UncaughtExceptionHandler(v8::Local<v8::Message> message, v8::Local<v8::Value> data);
    static void OnPromiseRejectionMicrotask(void* data);
    static void OnTaskMicrotask(void *data);

	static void SetCurrentThreadName(std::string name);
    static void StartLoopThread(void *arg);
	static void StopLoopThread(uv_async_t *handle);
	static void SuspendLoopThread(uv_async_t *handle);
	static void OnHandleClosed(uv_handle_t *handle);

    static void OnTimerTriggeredCallback(uv_timer_t * handle);
	static void OnTimerClosedCallback(uv_handle_t * handle);
	static void OnTimerEventCallback(uv_async_t * handle);
	static void RejectedPromiseHolderWeakPersistentCallback(const v8::WeakCallbackInfo<void> &data);
	static void OnJniRunnables(uv_async_t* handle);

	void createContext();

	// jni methods
    static void jniInitialize(JNIEnv * env, jobject v8Engine, jobject assetManager, jstring commonJSPath, jint maxHeapSize);
    static void jniPause(JNIEnv *env, jobject obj);
    static void jniUnpause(JNIEnv *env, jobject obj);
    static void jniShutdown(JNIEnv *env, jobject obj);
    static jstring jniDumpHeap(JNIEnv *env, jobject obj, jstring pathToSaveIn);
    static void jniEnqueueOnNextTick(JNIEnv *env, jobject obj, jobject runnable);
    static jobject jniParseJSON(JNIEnv *env, jobject obj, jstring json);
    static jobject jniRequire(JNIEnv *env, jobject obj, jstring file);
    static jlong jniLock(JNIEnv *env, jobject obj, jstring ownerName);
    static jobject jniGetGlobalObject(JNIEnv *env, jobject obj);
    static void jniUnlock(JNIEnv *env, jobject obj, jlong lockerPtr);
    static jobject jniRunScript(JNIEnv *env, jobject obj, jstring script, jstring name);
    static void jniRegisterModuleNative(JNIEnv *env, jobject obj, jobject module);
    static jobject jniGetConstructor(JNIEnv *env, jobject obj, jstring canonicalName);

	// jni class info caches
	static struct {
		jclass clazz;
		jmethodID getNameId;
		jmethodID requireId;
	} _jniV8Module;

	static struct {
		jclass clazz;
		jmethodID initId;
		jmethodID setStackTraceId;
		jmethodID getV8ExceptionId;
	} _jniV8JSException;

	static struct {
		jclass clazz;
		jmethodID initId;
	} _jniV8Exception;

	static struct {
		jclass clazz;
		jmethodID initId;
	} _jniStackTraceElement;

	static struct {
		jclass clazz;
		jmethodID onReadyId;
		jmethodID onThrowId;
		jmethodID onSuspendId;
		jmethodID onResumeId;
	} _jniV8Engine;


	EState _state;
	bool _isSuspended;

	uv_thread_t _uvThread;
	uv_loop_t _uvLoop;
	uv_mutex_t _uvMutex;
	uv_cond_t _uvCondSuspend;
	uv_async_t _uvEventScheduleTimers, _uvEventStop, _uvEventSuspend;

	uv_async_t _uvEventJniRunnables;
	std::vector<RunnableHolder*> _nextTickRunnables;
	uv_mutex_t _uvMutexRunnables;

	int _maxHeapSize;	// in MB

    uint8_t _nextEmbedderDataIndex;
	jobject _javaAssetManager;

	v8::Persistent<v8::Context> _context;

	// Attributes
	bool _didScheduleURPTask;
	std::vector<RejectedPromiseHolder*> _unhandledRejectedPromises;

	uint64_t _nextTimerId;
	std::vector<TimerHolder*> _timers;

	std::string _commonJSPath;
	std::map<std::string, jobject> _javaModules;
	std::map<std::string, requireHook> _modules;
    std::map<std::string, v8::Persistent<v8::Value>> _moduleCache;
    v8::Isolate* _isolate;

    v8::Persistent<v8::Function> _requireFn, _makeRequireFn;
	v8::Persistent<v8::Function> _jsonParseFn, _jsonStringifyFn;
	v8::Persistent<v8::Function> _debugDumpFn;
	v8::Persistent<v8::Function> _makeJavaErrorFn;
	v8::Persistent<v8::Function> _getStackTraceFn;

    v8::Local<v8::Function> makeRequireFunction(std::string pathName);
};

BGJS_JNI_LINK_DEF(BGJSV8Engine)

#ifdef V8_LOCK_LOGGING
#define LOG_TAG "V8LockerLogger"

static std::string v8LockOwnerName = "";
static uv_thread_t v8LockOwnerThread = {};
static std::string v8LockOwnerThreadName = "";
static uv_mutex_t v8LockOwnerMutex = {};
static auto __unused__ = uv_mutex_init(&v8LockOwnerMutex);

class V8LockerLogger {
    bool isTopLevel = true; // The v8::Locker is a recursive lock (can lock more than once in a given thread)
    std::string ownerName;
    std::string waitingForName;
    uv_thread_t waitingForThread;
    std::string waitingForThreadName;
    uint64_t startTime;
    const uint64_t logThresholdMs = 100; // log only if waiting/locking for more than this (ms)

public:
    V8LockerLogger(const char* ownerName) : ownerName(ownerName) {
        uv_mutex_lock(&v8LockOwnerMutex);
        waitingForName = v8LockOwnerName;
        waitingForThread = v8LockOwnerThread;
        waitingForThreadName = v8LockOwnerThreadName;
        uv_mutex_unlock(&v8LockOwnerMutex);

        //LOG(LOG_DEBUG, "V8Locker: %s: Waiting for Lock", this->ownerName.c_str());

        startTime = uv_hrtime();
    }

    void gotLock() {
        uint64_t waitingTime = (uv_hrtime() - startTime) / 1000000;

        uv_mutex_lock(&v8LockOwnerMutex);
        if (v8LockOwnerName.length() == 0) {
            v8LockOwnerName = ownerName;
            v8LockOwnerThread = uv_thread_self();
            v8LockOwnerThreadName = getCurrentThreadName();
        } else {
            isTopLevel = false;
        }
        uv_mutex_unlock(&v8LockOwnerMutex);

        //LOG(LOG_DEBUG, "V8Locker: %s: Got Lock", ownerName.c_str());

        if (waitingTime > logThresholdMs) {
            uv_thread_t currentThread = uv_thread_self();
            LOG(LOG_ERROR, "V8Locker: %s (Thread %lu - %s) hat %llums auf %s (Thread %lu - %s) gewartet",
                ownerName.c_str(), currentThread, getCurrentThreadName().c_str(), waitingTime,
                waitingForName.length() > 0 ? waitingForName.c_str() : "(unbekannt)", waitingForThread, waitingForThreadName.c_str());
        }

        startTime = uv_hrtime();
    }

    void beforeUnlock() {
        uint64_t lockingTime = (uv_hrtime() - startTime) / 1000000;
        if (lockingTime > logThresholdMs) {
            uv_thread_t currentThread = uv_thread_self();
            LOG(LOG_ERROR, "V8Locker: %s (%s, Thread %lu - %s) hat %llums gelockt", ownerName.c_str(), isTopLevel ? "TopLevel" : "SubLock",
                currentThread, getCurrentThreadName().c_str(), lockingTime);
        }

        //LOG(LOG_DEBUG, "V8Locker: %s: Unlocking now", ownerName.c_str());

        if (isTopLevel) {
            uv_mutex_lock(&v8LockOwnerMutex);
            v8LockOwnerName = "";
            v8LockOwnerThread = {};
            v8LockOwnerThreadName = "";
            uv_mutex_unlock(&v8LockOwnerMutex);
        }
    }

    ~V8LockerLogger() {
        //LOG(LOG_DEBUG, "V8Locker: %s: Unlocked", ownerName.c_str());
    }

    std::string getCurrentThreadName() {
        JNIEnv* env = JNIWrapper::getEnvironment();

        jclass threadClass = env->FindClass("java/lang/Thread");
        jmethodID currentThreadMid = env->GetStaticMethodID(threadClass, "currentThread", "()Ljava/lang/Thread;");
        jobject currentThread = env->CallStaticObjectMethod(threadClass, currentThreadMid);
        jclass currentThreadClass = env->GetObjectClass(currentThread);
        jmethodID getNameMid = env->GetMethodID(currentThreadClass, "getName", "()Ljava/lang/String;");
        jstring name = (jstring) env->CallObjectMethod(currentThread, getNameMid);

        return JNIWrapper::jstring2string(name);
    }
};

class V8Unlocker {
    V8LockerLogger& logger;

public:
    V8Unlocker(V8LockerLogger& logger) : logger(logger) {}
    ~V8Unlocker() {
        logger.beforeUnlock();
    }
};

class V8Locker : public V8LockerLogger, public v8::Locker, public V8Unlocker {
public:
    V8Locker(v8::Isolate* isolate, const char* ownerName) : V8LockerLogger(ownerName), v8::Locker(isolate), V8Unlocker((V8LockerLogger&) *this) {
        gotLock();
    }
};

#undef LOG_TAG
#else // V8_LOCK_LOGGING

class V8Locker : public v8::Locker {
public:
    V8Locker(v8::Isolate* isolate, const char* ownerName) : v8::Locker(isolate) {}
};

#endif // V8_LOCK_LOGGING

#endif
