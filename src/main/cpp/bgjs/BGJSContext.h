#ifndef __BGJSCONTEXT_H
#define __BGJSCONTEXT_H 1

#include <v8.h>

#include "os-detection.h"

#ifdef ANDROID
#include <jni.h>
#include <string.h>
#include <sys/types.h>
#endif


#include "BGJSInfo.h"
#include "BGJSModule.h"
// #include "BGJSGLView.h"
#include "ClientAbstract.h"
#include <map>
#include <string>
#include <set>

#include <mallocdebug.h>
/**
 * BGJSContext
 * Manages a v8 context and exposes script load and execute functions
 *
 * Copyright 2014 Kevin Read <me@kevin-read.com> and BörseGo AG (https://github.com/godmodelabs/ejecta-v8/)
 * Licensed under the MIT license.
 */

class BGJSGLView;

#define MAX_FRAME_REQUESTS 10
// #define INTERNAL_REQUIRE_CACHE

struct cmp_str
{
   bool operator()(char const *a, char const *b) const
   {
      return strcmp(a, b) < 0;
   }
};

const char* ToCString(const v8::String::Utf8Value& value);
void LogStackTrace(v8::Local<v8::StackTrace>& str);

struct WrapPersistentFunc {
	v8::Persistent<v8::Function> callbackFunc;
};

struct WrapPersistentObj {
	v8::Persistent<v8::Object> obj;
};

typedef  void (*requireHook) (v8::Isolate* isolate, v8::Handle<v8::Object> target);

class BGJSContext : public BGJSInfo {
public:
    // static BGJSContext& getInstance();
    BGJSContext(v8::Isolate* isolate);
	virtual ~BGJSContext();

	v8::Handle<v8::Value> callFunction(v8::Isolate* isolate, v8::Handle<v8::Object> recv, const char* name,
    		int argc, v8::Handle<v8::Value> argv[]) const;
	bool loadScript(const char* path);
	bool registerModule(const char *name, requireHook f);
	v8::Handle<v8::Value> executeJS(const uint8_t* src);

	v8::Persistent<v8::Script, v8::CopyablePersistentTraits<v8::Script> > load(const char* path);
	static void ReportException(v8::TryCatch* try_catch);
	static void log(int level, const v8::FunctionCallbackInfo<v8::Value>& args);
	int run();
	void setClient(ClientAbstract* client);
	ClientAbstract* getClient() const;
    v8::Isolate* getIsolate() const;
	void setLocale(const char* locale, const char* lang, const char* tz);
	void require(const v8::FunctionCallbackInfo<v8::Value>& args);
	void normalizePath(const v8::FunctionCallbackInfo<v8::Value>& info);
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
	v8::Persistent<v8::Script> _script;  // Reference to script object that was loaded
	static bool debug;
	char *_locale;	// de_DE
	char *_lang;	// de
	char *_tz;		// Europe/Berlin

private:
	// Private constructors for singleton
	BGJSContext(BGJSContext const&) {};              // Don't Implement
	void operator=(BGJSContext const&); // Don't implement

	const char* loadFile (const char* path);
	void CloneObject(v8::Handle<v8::Object> recv, v8::Handle<v8::Value> source,
			v8::Handle<v8::Value> target);
	std::string normalize_path(std::string& path);
	std::string getPathName(std::string& path);
	// Attributes
	v8::Persistent<v8::Function> cloneObjectMethod;	// clone
	std::map<std::string, requireHook> _modules;
    v8::Isolate* _isolate;

#ifdef INTERNAL_REQUIRE_CACHE
	std::map<std::string, v8::Value*> _requireCache;
#endif
	std::set<BGJSGLView*> _glViews;

	int _nextTimerId;
};


#endif
