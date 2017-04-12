#ifndef __BGJSV8Engine_H
#define __BGJSV8Engine_H 1

#include <v8.h>

#include "os-android.h"

#ifdef ANDROID
#include <jni.h>
#include <string.h>
#include <sys/types.h>
#endif


#include "BGJSModule.h"

#include "ClientAbstract.h"
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

#define BGJS_CURRENT_V8ENGINE() \
	reinterpret_cast<BGJSV8Engine*>(Isolate::GetCurrent()->GetCurrentContext()->GetAlignedPointerFromEmbedderData(EBGJSV8EngineEmbedderData::kContext))

#define BGJS_STRING_FROM_V8VALUE(value) \
	(value.IsEmpty() ? std::string("") : std::string(*v8::String::Utf8Value(value->ToString())))

#define BGJS_CHAR_FROM_V8VALUE(value) \
	(value.IsEmpty() ? "" : *v8::String::Utf8Value(value->ToString()))

class BGJSGLView;

#define MAX_FRAME_REQUESTS 10

struct WrapPersistentFunc {
	v8::Persistent<v8::Function> callbackFunc;
};

struct WrapPersistentObj {
	v8::Persistent<v8::Object> obj;
};

typedef  void (*requireHook) (v8::Isolate* isolate, v8::Handle<v8::Object> target);

typedef enum EBGJSV8EngineEmbedderData {
    kContext = 1
} EBGJSV8EngineEmbedderData;

class BGJSV8Engine {
public:
    // static BGJSV8Engine& getInstance();
    BGJSV8Engine(v8::Isolate* isolate);
	virtual ~BGJSV8Engine();

    v8::Local<v8::Value> require(std::string baseNameStr);

	ClientAbstract* getClient() const;
	v8::Isolate* getIsolate() const;
	v8::Local<v8::Context> getContext() const;

	v8::Handle<v8::Value> callFunction(v8::Isolate* isolate, v8::Handle<v8::Object> recv, const char* name,
    		int argc, v8::Handle<v8::Value> argv[]) const;
	bool registerModule(const char *name, requireHook f);

	static void ReportException(v8::TryCatch* try_catch);
	static void log(int level, const v8::FunctionCallbackInfo<v8::Value>& args);
    int run(const char *path = NULL);
	void setClient(ClientAbstract* client);

	void setLocale(const char* locale, const char* lang, const char* tz);

	static void js_global_requestAnimationFrame (const v8::FunctionCallbackInfo<v8::Value>&);
	static void js_global_cancelAnimationFrame (const v8::FunctionCallbackInfo<v8::Value>& args);
	static void js_global_setTimeout (const v8::FunctionCallbackInfo<v8::Value>& info);
	static void js_global_clearTimeout (const v8::FunctionCallbackInfo<v8::Value>& info);
	static void js_global_setInterval (const v8::FunctionCallbackInfo<v8::Value>& info);
	static void js_global_clearInterval (const v8::FunctionCallbackInfo<v8::Value>& info);
	static void js_global_getLocale(v8::Local<v8::String> property,
			const v8::PropertyCallbackInfo<v8::Value>& info);
	static void js_global_setLocale(v8::Local<v8::String> property, v8::Local<v8::Value> value,
			const v8::PropertyCallbackInfo<void>& info);
	static void js_global_getLang(v8::Local<v8::String> property,
			const v8::PropertyCallbackInfo<v8::Value>& info);
	static void js_global_setLang(v8::Local<v8::String> property, v8::Local<v8::Value> value,
			const v8::PropertyCallbackInfo<void>& info);
	static void js_global_getTz(v8::Local<v8::String> property,
			const v8::PropertyCallbackInfo<v8::Value>& info);
	static void js_global_setTz(v8::Local<v8::String> property, v8::Local<v8::Value> value,
			const v8::PropertyCallbackInfo<void>& info);
	static void setTimeoutInt(const v8::FunctionCallbackInfo<v8::Value>& info, bool recurring);
	static void clearTimeoutInt(const v8::FunctionCallbackInfo<v8::Value>& info);
	void cancelAnimationFrame(int id);
	bool runAnimationRequests(BGJSGLView* view) const;
	void registerGLView(BGJSGLView* view);
	void unregisterGLView(BGJSGLView* view);

	v8::Handle<v8::Value> JsonParse(v8::Handle<v8::Object> recv, v8::Handle<v8::String> source);

	void createContext();
	ClientAbstract *_client;
	static bool debug;
	char *_locale;	// de_DE
	char *_lang;	// de
	char *_tz;		// Europe/Berlin

private:
	// Private constructors for singleton
	BGJSV8Engine(BGJSV8Engine const&) {}; // Don't Implement
	void operator=(BGJSV8Engine const&); // Don't implement

	v8::Persistent<v8::Context> _context;

	// Attributes
	std::map<std::string, requireHook> _modules;
    v8::Isolate* _isolate;

    v8::Persistent<v8::Function> _requireFn;
    v8::Local<v8::Function> makeRequireFunction(std::string pathName);

	std::set<BGJSGLView*> _glViews;

	int _nextTimerId;
};


#endif
