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

typedef  void (*requireHook) (v8::Handle<v8::Object> target);

class BGJSContext : public BGJSInfo {
public:
    // static BGJSContext& getInstance();
    BGJSContext();
	virtual ~BGJSContext();

	void callFunction(const char* name, v8::Arguments arguments);
	bool loadScript(const char* path);
	bool registerModule(const char *name, requireHook f);
	v8::Handle<v8::Value> executeJS(const char* src);

	v8::Persistent<v8::Script> load(const char* path);
	static void ReportException(v8::TryCatch* try_catch);
	static void log(int, const v8::Arguments& args);
	int run();
	void setClient(ClientAbstract* client);
	ClientAbstract* getClient();
	void setLocale(const char* locale, const char* lang, const char* tz);
	v8::Handle<v8::Value> require(const v8::Arguments& args);
	v8::Handle<v8::Value> callFunction(v8::Handle<v8::Object> recv, const char* name, int argc, v8::Handle<v8::Value> argv[]);
	static v8::Handle<v8::Value> js_global_requestAnimationFrame (const v8::Arguments& args);
	static v8::Handle<v8::Value> js_global_cancelAnimationFrame (const v8::Arguments& args);
	static v8::Handle<v8::Value> js_global_setTimeout (const v8::Arguments& args);
	static v8::Handle<v8::Value> js_global_clearTimeout (const v8::Arguments& args);
	static v8::Handle<v8::Value> js_global_setInterval (const v8::Arguments& args);
	static v8::Handle<v8::Value> js_global_clearInterval (const v8::Arguments& args);
	static v8::Handle<v8::Value> js_global_getLocale(v8::Local<v8::String> property,
			const v8::AccessorInfo &info);
	static void js_global_setLocale(v8::Local<v8::String> property, v8::Local<v8::Value> value,
			const v8::AccessorInfo &info);
	static v8::Handle<v8::Value> js_global_getLang(v8::Local<v8::String> property,
			const v8::AccessorInfo &info);
	static void js_global_setLang(v8::Local<v8::String> property, v8::Local<v8::Value> value,
			const v8::AccessorInfo &info);
	static v8::Handle<v8::Value> js_global_getTz(v8::Local<v8::String> property,
			const v8::AccessorInfo &info);
	static void js_global_setTz(v8::Local<v8::String> property, v8::Local<v8::Value> value,
			const v8::AccessorInfo &info);
	static v8::Handle<v8::Value> setTimeoutInt(const v8::Arguments& args, bool recurring);
	static void clearTimeoutInt(const v8::Arguments& args);
	void cancelAnimationFrame(int id);
	bool runAnimationRequests(BGJSGLView* view);
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
	// Attributes
	v8::Persistent<v8::Function> cloneObjectMethod;	// clone
	v8::Persistent<v8::Function> jsonParseMethod;   // json parser
	std::map<std::string, requireHook> _modules;

#ifdef INTERNAL_REQUIRE_CACHE
	std::map<std::string, v8::Value*> _requireCache;
#endif
	std::set<BGJSGLView*> _glViews;

	int _nextTimerId;

};


#endif
