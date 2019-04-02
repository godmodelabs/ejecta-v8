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
#include "BGJSModule.h"

#include "../jni/jni.h"

/**
 * BGJSV8Engine
 * Manages a v8 context and exposes script load and execute functions
 *
 * Copyright 2014 Kevin Read <me@kevin-read.com> and BörseGo AG (https://github.com/godmodelabs/ejecta-v8/)
 * Licensed under the MIT license.
 */

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
		const char *locale, *lang, *timezone, *deviceClass;
		int maxHeapSize;
		bool isStoreBuild, debug;
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
	bool forwardV8ExceptionToJNI(v8::TryCatch* try_catch) const;

	char* loadFile(const char* path, unsigned int* length = nullptr) const;

	static void js_global_requestAnimationFrame (const v8::FunctionCallbackInfo<v8::Value>&);
    static void js_process_nextTick (const v8::FunctionCallbackInfo<v8::Value>&);
	static void js_global_cancelAnimationFrame (const v8::FunctionCallbackInfo<v8::Value>& args);
	static void js_global_setTimeout (const v8::FunctionCallbackInfo<v8::Value>& info);
	static void js_global_clearTimeoutOrInterval (const v8::FunctionCallbackInfo<v8::Value>& info);
	static void js_global_setInterval (const v8::FunctionCallbackInfo<v8::Value>& info);
	static void js_global_getLocale(v8::Local<v8::String> property,
			const v8::PropertyCallbackInfo<v8::Value>& info);
	static void js_global_getLang(v8::Local<v8::String> property,
			const v8::PropertyCallbackInfo<v8::Value>& info);
	static void js_global_getTz(v8::Local<v8::String> property,
			const v8::PropertyCallbackInfo<v8::Value>& info);
	static void js_global_getDeviceClass(v8::Local<v8::String> property,
                                const v8::PropertyCallbackInfo<v8::Value>& info);

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

	struct TimerHolder {
		uv_timer_t handle;
		bool scheduled, cleared, stopped, repeats;
		uint64_t id, delay, repeat;
		v8::Persistent<v8::Function> callback;
		JNIRetainedRef<BGJSV8Engine> engine;
	};

	uint64_t createTimer(v8::Local<v8::Function> callback, uint64_t delay, uint64_t repeat);
	bool forwardV8ExceptionToJNI(std::string messagePrefix, v8::Local<v8::Value> exception, v8::Local<v8::Message> message, jobject causeException = nullptr, bool throwOnMainThread = false) const;
	bool forwardV8ExceptionToJNI(std::string messagePrefix, v8::TryCatch* try_catch, bool throwOnMainThread) const;

	// utility method to convert v8 values to readable strings for debugging
	const std::string toDebugString(v8::Handle<v8::Value> source) const;

	// called by JNIWrapper
	static void initializeJNIBindings(JNIClassInfo *info, bool isReload);

	static void JavaModuleRequireCallback(BGJSV8Engine *engine, v8::Handle<v8::Object> target);

    static void OnGCCompletedForDump(v8::Isolate* isolate, v8::GCType type,
                                     v8::GCCallbackFlags flags);

    static void PromiseRejectionHandler(v8::PromiseRejectMessage message);

    static void OnPromiseRejectionMicrotask(void* data);

    static void StartLoopThread(void *arg);
	static void StopLoopThread(uv_async_t *handle);
	static void SuspendLoopThread(uv_async_t *handle);
	static void OnHandleClosed(uv_handle_t *handle);

    static void OnTimerTriggeredCallback(uv_timer_t * handle);
	static void OnTimerClosedCallback(uv_handle_t * handle);
	static void OnTimerEventCallback(uv_async_t * handle);
	static void RejectedPromiseHolderWeakPersistentCallback(const v8::WeakCallbackInfo<void> &data);

	void createContext();

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
	bool _isSuspended, _debug;

	uv_thread_t _uvThread;
	uv_loop_t _uvLoop;
	uv_mutex_t _uvMutex;
	uv_cond_t _uvCondSuspend;
	uv_async_t _uvEventScheduleTimers, _uvEventStop, _uvEventSuspend;

	char *_locale;		// de_DE
	char *_lang;		// de
	char *_tz;			// Europe/Berlin
	char *_deviceClass; // "phone"/"tablet"
	int _maxHeapSize;	// in MB
    bool _isStoreBuild;

    uint8_t _nextEmbedderDataIndex;
	jobject _javaAssetManager;

	v8::Persistent<v8::Context> _context;

	// Attributes
	bool _didScheduleURPTask;
	std::vector<RejectedPromiseHolder*> _unhandledRejectedPromises;

	uint64_t _nextTimerId;
	std::vector<TimerHolder*> _timers;

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

#endif
