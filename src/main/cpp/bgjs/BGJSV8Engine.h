#ifndef __BGJSV8Engine_H
#define __BGJSV8Engine_H 1

#include <v8.h>
#include <jni.h>

#include "os-android.h"

#ifdef ANDROID
#include <jni.h>
#include <string.h>
#include <sys/types.h>
#endif


#include "BGJSModule.h"

#include <map>
#include <string>
#include <set>

#include <mallocdebug.h>
/**
 * BGJSV8Engine
 * Manages a v8 context and exposes script load and execute functions
 *
 * Copyright 2014 Kevin Read <me@kevin-read.com> and BörseGo AG (https://github.com/godmodelabs/ejecta-v8/)
 * Licensed under the MIT license.
 */

#define BGJS_CURRENT_V8ENGINE(isolate) \
	reinterpret_cast<BGJSV8Engine*>(isolate->GetCurrentContext()->GetAlignedPointerFromEmbedderData(EBGJSV8EngineEmbedderData::kContext))

#define BGJS_STRING_FROM_V8VALUE(value) \
	(value.IsEmpty() ? std::string("") : std::string(*v8::String::Utf8Value(value->ToString())))

class BGJSGLView;

#define MAX_FRAME_REQUESTS 10

struct WrapPersistentFunc {
	v8::Persistent<v8::Function> callbackFunc;
};

struct WrapPersistentObj {
	v8::Persistent<v8::Object> obj;
};

typedef  void (*requireHook) (class BGJSV8Engine* engine, v8::Handle<v8::Object> target);

typedef enum EBGJSV8EngineEmbedderData {
    kContext = 1,
    FIRST_UNUSED = 2
} EBGJSV8EngineEmbedderData;

class BGJSV8Engine {
public:
    BGJSV8Engine(jobject javaObject, jobject javaAssetManager);
	virtual ~BGJSV8Engine();

    v8::MaybeLocal<v8::Value> require(std::string baseNameStr);
    uint8_t requestEmbedderDataIndex();
    bool registerModule(const char *name, requireHook f);
	bool registerJavaModule(jobject module);

	v8::Isolate* getIsolate() const;
	v8::Local<v8::Context> getContext() const;

	v8::Handle<v8::Value> callFunction(v8::Isolate* isolate, v8::Handle<v8::Object> recv, const char* name,
    		int argc, v8::Handle<v8::Value> argv[]) const;

	bool forwardJNIExceptionToV8() const;
	bool forwardV8ExceptionToJNI(v8::TryCatch* try_catch) const;

	static void log(int level, const v8::FunctionCallbackInfo<v8::Value>& args);

	jobject getJObject() const;

	void setLocale(const char* locale, const char* lang, const char* tz, const char* deviceClass);
	void setDensity(float density);
	float getDensity() const;

	char* loadFile(const char* path, unsigned int* length = nullptr) const;

	static void js_global_requestAnimationFrame (const v8::FunctionCallbackInfo<v8::Value>&);
	static void js_global_cancelAnimationFrame (const v8::FunctionCallbackInfo<v8::Value>& args);
	static void js_global_setTimeout (const v8::FunctionCallbackInfo<v8::Value>& info);
	static void js_global_clearTimeout (const v8::FunctionCallbackInfo<v8::Value>& info);
	static void js_global_setInterval (const v8::FunctionCallbackInfo<v8::Value>& info);
	static void js_global_clearInterval (const v8::FunctionCallbackInfo<v8::Value>& info);
	static void js_global_getLocale(v8::Local<v8::String> property,
			const v8::PropertyCallbackInfo<v8::Value>& info);
	static void js_global_getLang(v8::Local<v8::String> property,
			const v8::PropertyCallbackInfo<v8::Value>& info);
	static void js_global_getTz(v8::Local<v8::String> property,
			const v8::PropertyCallbackInfo<v8::Value>& info);
	static void js_global_getDeviceClass(v8::Local<v8::String> property,
                                const v8::PropertyCallbackInfo<v8::Value>& info);
    static void setTimeoutInt(const v8::FunctionCallbackInfo<v8::Value>& info, bool recurring);
	static void clearTimeoutInt(const v8::FunctionCallbackInfo<v8::Value>& info);
	void cancelAnimationFrame(int id);
	bool runAnimationRequests(BGJSGLView* view);
	void registerGLView(BGJSGLView* view);
	void unregisterGLView(BGJSGLView* view);

	v8::Handle<v8::Value> parseJSON(v8::Handle<v8::String> source) const;
	v8::Handle<v8::Value> stringifyJSON(v8::Handle<v8::Object> source) const;

	void createContext();
	static bool debug;
	char *_locale;	// de_DE
	char *_lang;	// de
	char *_tz;		// Europe/Berlin
	char *_deviceClass;		// "phone"/"tablet"

    /**
     * cache JNI class references
     */
    static void initJNICache();
private:
	static void JavaModuleRequireCallback(BGJSV8Engine *engine, v8::Handle<v8::Object> target);
	static struct {
		jclass clazz;
		jmethodID getNameId;
		jmethodID requireId;
	} _jniV8Module;

	static struct {
		jclass clazz;
		jmethodID initId;
		jmethodID setStackTraceId;
	} _jniV8JSException;

	static struct {
		jclass clazz;
		jmethodID initId;
	} _jniV8Exception;

	static struct {
		jclass clazz;
		jmethodID initId;
	} _jniStackTraceElement;

	float _density;

	uint8_t _nextEmbedderDataIndex;
	jobject _javaObject, _javaAssetManager;

	v8::Persistent<v8::Context> _context;

	// Attributes
	std::map<std::string, jobject> _javaModules;
	std::map<std::string, requireHook> _modules;
    std::map<std::string, v8::Persistent<v8::Value>> _moduleCache;
    v8::Isolate* _isolate;

    v8::Persistent<v8::Function> _requireFn, _makeRequireFn;
	v8::Persistent<v8::Function> _jsonParseFn, _jsonStringifyFn;
	v8::Persistent<v8::Function> _makeJavaErrorFn;
	v8::Persistent<v8::Function> _getStackTraceFn;
    v8::Local<v8::Function> makeRequireFunction(std::string pathName);

	std::set<BGJSGLView*> _glViews;

	int _nextTimerId;
};


#endif
