/*
 * v8Engine.cpp
 *
 *  Created on: Sep 3, 2012
 *      Author: kread@boerse-go.de
 */

#include <jni.h>
#include <android/log.h>

#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <assert.h>
#include <string>
#include <sstream>
 /* #include <fstream>
 #include <streambuf> */

// for native asset manager
#include <sys/types.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include "v8Engine.h"

#define  LOG_TAG    "v8Engine-jni"
#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG,LOG_TAG,__VA_ARGS__)
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

// #define V8_CANVAS	1

using namespace v8;

#ifdef V8_CANVAS
	void v8CanvasInit (Handle<ObjectTemplate> target);
#endif

Persistent<ObjectTemplate> globalOT;

// Extracts a C string from a V8 Utf8Value.
const char* ToCString(const v8::String::Utf8Value& value) {
	return *value ? *value : "<string conversion failed>";
}

static Handle<Value> LogCallback(const Arguments& args) {
	if (args.Length() < 1)
		return v8::Undefined();

	v8Engine::log(ANDROID_LOG_INFO, args);
	return v8::Undefined();
}

static Handle<Value> DebugCallback(const Arguments& args) {
	if (args.Length() < 1)
		return v8::Undefined();

	v8Engine::log(ANDROID_LOG_DEBUG, args);
	return v8::Undefined();
}

static Handle<Value> InfoCallback(const Arguments& args) {
	if (args.Length() < 1)
		return v8::Undefined();

	v8Engine::log(ANDROID_LOG_INFO, args);
	return v8::Undefined();
}

static Handle<Value> ErrorCallback(const Arguments& args) {
	if (args.Length() < 1)
		return v8::Undefined();

	v8Engine::log(ANDROID_LOG_ERROR, args);
	return v8::Undefined();
}

static Handle<Value> AssertCallback(const Arguments& args) {
	if (args.Length() < 1)
		return v8::Undefined();

	v8Engine::log(ANDROID_LOG_ERROR, args);
	return v8::Undefined();
}

Persistent<Function> cloneObjectMethod;

void CloneObject(Handle<Object> recv, Handle<Value> source,
		Handle<Value> target) {
	HandleScope scope;

	Handle<Value> args[] = { source, target };

	// Init
	if (cloneObjectMethod.IsEmpty()) {
		Local<Function> cloneObjectMethod_ =
				Local<Function>::Cast(
						Script::Compile(
								String::New(
										"(function(source, target) {\n\
           Object.getOwnPropertyNames(source).forEach(function(key) {\n\
           try {\n\
             var desc = Object.getOwnPropertyDescriptor(source, key);\n\
             if (desc.value === source) desc.value = target;\n\
             Object.defineProperty(target, key, desc);\n\
           } catch (e) {\n\
            // Catch sealed properties errors\n\
           }\n\
         });\n\
        })"),
								String::New("binding:script"))->Run());
		cloneObjectMethod = Persistent<Function>::New(cloneObjectMethod_);
	}

	cloneObjectMethod->Call(recv, 2, args);
}

Persistent<Function> jsonParseMethod;

Handle<Value> JsonParse(Handle<Object> recv, Handle<String> source) {
	HandleScope scope;

	Handle<Value> args[] = { source  };

	// Init
	if (jsonParseMethod.IsEmpty()) {
		Local<Function> jsonParseMethod_ =
				Local<Function>::Cast(
						Script::Compile(
								String::New(
										"(function(source) {\n console.log('jsonsource', source);\
           try {\n\
             var a = JSON.parse(source); console.log('jsonparse', a); return a;\
           } catch (e) {\n\
            // Catch sealed properties errors\n\
           }\n\
         });"),
								String::New("binding:script"))->Run());
		jsonParseMethod = Persistent<Function>::New(jsonParseMethod_);
	}

	Handle<Value> result = jsonParseMethod->Call(recv, 1, args);
	return scope.Close(result);
}


Handle<Value> v8Engine::ajax(const Arguments& args) {
	if (args.Length() < 1) {
		LOGE("Not enough parameters for ajax");
		return v8::Undefined();
	}

	HandleScope scope;

	Local<v8::Object> options = args[0]->ToObject();

	// Get callback
	Local<Function> callback = Local<Function>::Cast(options->Get(String::New("success")));
	Persistent<Function> callbackPers = Persistent<Function>::New(callback);

	jstring dataStr, urlStr;

	// Get url
	Local<String> url = Local<String>::Cast(options->Get(String::New("url")));
	String::AsciiValue urlLocal(url);
	urlStr = this->envCache->NewStringUTF(*urlLocal);

	// Persist the this object
	Persistent<Object> thisObj = Persistent<Object>::New(args.This());

	// Get data
	Local<Value> data = options->Get(String::New("data"));
	if (data != v8::Undefined()) {
		String::AsciiValue dataLocal(data);
		dataStr = this->envCache->NewStringUTF(*dataLocal);
	} else {
		dataStr = 0;
	}

	/* if (!data->IsString()) {
		LOGE("data parameter needs to be a string");
		return v8::Undefined();
	} */

	jclass clazz = this->envCache->FindClass("de/boersego/charting/V8Engine");
	jmethodID ajaxMethod = this->envCache->GetStaticMethodID(clazz, "doAjaxRequest", "(Ljava/lang/String;IILjava/lang/String;)V");
	assert(ajaxMethod);
	assert(clazz);
	/* std::stringstream str;
	str << "Clazz " << clazz << ", " << urlStr << ", " << dataStr << ", cbPers" << (jint)&callbackPers;
	LOGI(str.str().c_str()); */
	this->envCache->CallStaticVoidMethod(clazz, ajaxMethod, urlStr, (jint)&callbackPers, (jint)&thisObj, dataStr); // (jint)&callbackPers

	return v8::Undefined();
}

bool v8Engine::ajaxSuccess(JNIEnv * env, jobject obj,
		jstring dataStr, jint responseCode, jint jsCbPtr, jint thisPtr) {
	const char *nativeString = env->GetStringUTFChars(dataStr, 0);

	HandleScope scope;

	Persistent<Function>* callbackPers = (Persistent<Function>*)jsCbPtr;
	Persistent<Object>* thisObj = (Persistent<Object>*)thisPtr;

	Handle<Value> resultObj = JsonParse(*thisObj, String::New(nativeString));

	int argcount = 1;
	Handle<Value> argarray[] = { resultObj };

	Handle<Value> result = (*callbackPers)->Call(*thisObj, argcount, argarray);
	env->ReleaseStringUTFChars(dataStr, nativeString);
	(*callbackPers).Dispose();
	(*thisObj).Dispose();

	return true;
}


static Handle<Value> AjaxCallback(const Arguments& args) {
	return ((v8Engine::getInstance()).ajax(args));
}

Handle<Value> v8Engine::require(const Arguments& args) {
	if (args.Length() < 1)
		return v8::Undefined();

	HandleScope scope;

	struct stat fileStat;

	Local<String> filename = args[0]->ToString();
	Local<String> extension = String::New(".js");
	Local<String> fullname = v8::String::Concat(filename, extension);

	String::AsciiValue name(fullname);

	/* if (stat(*name, &fileStat)) {
		LOGE("Cannot find file");
		log(ANDROID_LOG_ERROR, args);
		return v8::Undefined();
	} */

	Local<Object> sandbox = args[1]->ToObject();
	Persistent<Context> context = Context::New(NULL, globalOT);

	Local<Array> keys;
	// Enter the context
	context->Enter();

	// Copy everything from the passed in sandbox (either the persistent
	// context for runInContext(), or the sandbox arg to runInNewContext()).
	CloneObject(args.This(), sandbox, context->Global()->GetPrototype());

	// Catch errors
	TryCatch try_catch;

	Handle<Value> result;
	Handle<Script> script;

	AAssetManager* mgr = AAssetManager_fromJava(this->envCache, this->assetManager);
	AAsset* asset = AAssetManager_open(mgr, *name, AASSET_MODE_UNKNOWN);

	if (!asset) {
		LOGE("Cannot find file");
		log(ANDROID_LOG_ERROR, args);
		return v8::Undefined();
	}

	const int count = AAsset_getLength(asset);
	char buf[count + 1], *ptr = buf;
	bzero(buf, count+1);
	int bytes_read = 0, bytes_to_read = count;
	std::stringstream str;

	while ((bytes_read = AAsset_read(asset, ptr, bytes_to_read)) > 0) {
		bytes_to_read -= bytes_read;
		ptr += bytes_read;
		str << "Read " << bytes_read << " bytes from total of " << count << ", " << bytes_to_read << " left\n";
		LOGI(str.str().c_str());
		// str.str("");
		// LOGI(ptr);

	}
	AAsset_close(asset);


	// Create a string containing the JavaScript source code.
	Handle<String> source = String::New(buf);

	// well, here WrappedScript::New would suffice in all cases, but maybe
	// Compile has a little better performance where possible
	Handle<Script>scriptR = Script::Compile(source, filename);
	if (scriptR.IsEmpty()) {
		// FIXME UGLY HACK TO DISPLAY SYNTAX ERRORS.
		v8Engine::ReportException(&try_catch);

		// Hack because I can't get a proper stacktrace on SyntaxError
		return try_catch.ReThrow();
	}

	result = scriptR->Run();
	if (result.IsEmpty()) {
		context->DetachGlobal();
		context->Exit();
		context.Dispose();
		/* ReThrow doesn't re-throw TerminationExceptions; a null exception value
		 * is re-thrown instead (see Isolate::PropagatePendingExceptionToExternalTryCatch())
		 * so we re-propagate a TerminationException explicitly here if necesary. */
		if (try_catch.CanContinue())
			return try_catch.ReThrow();
		v8::V8::TerminateExecution();
		return v8::Undefined();
	}

	// success! copy changes back onto the sandbox object.
	CloneObject(args.This(), context->Global()->GetPrototype(), sandbox);
	//Handle<Value> logArgs[] = {sandbox};
	// LogCallback(&logArgs);
	// Clean up, clean up, everybody everywhere!
	context->DetachGlobal();
	context->Exit();
	context.Dispose();

	return scope.Close(result);
}

static Handle<Value> RequireCallback(const Arguments& args) {

	return ((v8Engine::getInstance()).require(args));
}

/* static JSValueRef globalGetContext(JSContextRef ctx, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception) {
 // create event object
 JSObjectRef contextObjRef = JSObjectMake(ctx, NULL, NULL);

 // set width property
 JSStringRef stringRef = JSStringCreateWithUTF8CString("width");
 JSObjectSetProperty(ctx, contextObjRef, stringRef, JSValueMakeNumber(ctx, __context.bounds.size.width), kJSPropertyAttributeNone, nil);
 JSStringRelease(stringRef);

 // set height property
 stringRef = JSStringCreateWithUTF8CString("height");
 JSObjectSetProperty(ctx, contextObjRef, stringRef, JSValueMakeNumber(ctx, __context.bounds.size.height), kJSPropertyAttributeNone, nil);
 JSStringRelease(stringRef);

 // set height property
 stringRef = JSStringCreateWithUTF8CString("context2d");
 JSObjectSetProperty(ctx, contextObjRef, stringRef, __context->objContext2d, kJSPropertyAttributeNone, nil);
 JSStringRelease(stringRef);

 // ajax request
 stringRef = JSStringCreateWithUTF8CString("ajax");
 JSObjectRef ajaxRef = JSObjectMakeFunctionWithCallback(ctx, stringRef, ajaxRequest);
 JSObjectSetProperty(ctx, contextObjRef, stringRef, ajaxRef, kJSPropertyAttributeNone, nil);
 JSStringRelease(stringRef);

 // ajax request
 stringRef = JSStringCreateWithUTF8CString("requestRedraw");
 JSObjectRef redrawRef = JSObjectMakeFunctionWithCallback(ctx, stringRef, redrawRequest);
 JSObjectSetProperty(ctx, contextObjRef, stringRef, redrawRef, kJSPropertyAttributeNone, nil);
 JSStringRelease(stringRef);

 // subscribe to push
 stringRef = JSStringCreateWithUTF8CString("push");
 JSObjectRef pushRef = JSObjectMakeFunctionWithCallback(ctx, stringRef, pushSubscribe);
 JSObjectSetProperty(ctx, contextObjRef, stringRef, pushRef, kJSPropertyAttributeNone, nil);
 JSStringRelease(stringRef);

 // subscribe to push
 stringRef = JSStringCreateWithUTF8CString("pushUnsubscribe");
 pushRef = JSObjectMakeFunctionWithCallback(ctx, stringRef, pushUnsubscribe);
 JSObjectSetProperty(ctx, contextObjRef, stringRef, pushRef, kJSPropertyAttributeNone, nil);
 JSStringRelease(stringRef);

 return contextObjRef;
 } */

/**
 * Get the width of the underlying view
 */
Handle<Value> getWidth(Local<String> property, const AccessorInfo& info) {
	return Integer::New(100);
}

void setWidth(Local<String> property, Local<Value> value,
		const AccessorInfo& info) {
}

v8Engine::v8Engine() {
	// Create a stack-allocated handle scope.
	HandleScope handle_scope;

	// Create a template for the global object where we set the
	// built-in global functions.
	Handle<ObjectTemplate> global = ObjectTemplate::New();

	#ifdef V8_CANVAS
		v8CanvasInit(global);
	#endif

	// Add methods to console function
	v8::Local<v8::FunctionTemplate> console = v8::FunctionTemplate::New();
	console->Set("log", FunctionTemplate::New(LogCallback));
	console->Set("debug", FunctionTemplate::New(DebugCallback));
	console->Set("info", FunctionTemplate::New(InfoCallback));
	console->Set("error", FunctionTemplate::New(ErrorCallback));
	console->Set("assert", FunctionTemplate::New(AssertCallback)); // TODO
	// Set the created FunctionTemplate as console in global object
	global->Set(String::New("console"), console);
	// Set the int_require function in the global object
	global->Set("int_require", FunctionTemplate::New(RequireCallback));

	// Create the template for the __BGJSContext object
	v8::Local<v8::FunctionTemplate> bgjs = v8::FunctionTemplate::New();
	bgjs->Set("ajax", FunctionTemplate::New(AjaxCallback));
	// bgjs->SetAccessor(String::New("width"), getWidth, setWidth);
	global->Set(String::New("bgjs"), bgjs);

	// Create a new context.
	this->context = Context::New(NULL, global);

	// Also, persist the global object template so we can add stuff here later when calling require
	globalOT = Persistent<ObjectTemplate>::New(global);
}

bool v8Engine::registerAssetManager(JNIEnv * env, jobject obj,
		jobject assetManager) {

	this->assetManager = env->NewGlobalRef(assetManager);
	return true;
}

int v8Engine::load(JNIEnv * env, jobject obj, jstring path) {
	// Create a stack-allocated handle scope.
	HandleScope handle_scope;
	v8::TryCatch try_catch;

	Context::Scope context_scope(context);

	const char* pathStr = env->GetStringUTFChars(path, 0);

	AAssetManager* mgr = AAssetManager_fromJava(env, assetManager);
	AAsset* asset = AAssetManager_open(mgr, pathStr, AASSET_MODE_UNKNOWN);

	const int count = AAsset_getLength(asset);
	char buf[count + 1];
	bzero(buf, count+1);

	while (AAsset_read(asset, buf, count) > 0) {
		LOGI(buf);
	}
	AAsset_close(asset);

	// Create a string containing the JavaScript source code.
	Handle<String> source = String::New(buf);

	// Compile the source code.
	Local<Script> scr = Script::Compile(source);
	script = Persistent<Script>::New(scr);

	if (script.IsEmpty()) {
		LOGE("Error compiling script %s\n", buf);
		// Print errors that happened during compilation.
		v8Engine::ReportException(&try_catch);
		return false;
	}

	env->ReleaseStringUTFChars(path, pathStr);

	validScriptLoaded = true;

	LOGI("Compiled\n");
	return true;
}

int v8Engine::tick(int ms) {
	// Create a stack-allocated handle scope.

	if (!mockTimerTick->IsFunction() || !mockTimer->IsObject()) {
		LOGE("Cannot tick");
		return false;
	}
	HandleScope handle_scope;
	v8::TryCatch try_catch;

	Context::Scope context_scope(context);
	// Invoke the Mocktimer.tick function, giving the global object as 'this'
	// and one argument, the number of milliseconds that have passed.
	const int argc = 1;
	v8::Handle<Integer> msv8 = Integer::New(ms);
	Handle<Value> argv[argc] = { msv8 };
	Handle<Value> result = mockTimerTick->Call(mockTimer, argc, argv);

	if (result.IsEmpty()) {
		// Print errors that happened during execution.
		ReportException(&try_catch);
		return false;
	}
	return true;
}

void v8Engine::ReportException(v8::TryCatch* try_catch) {
	v8::HandleScope handle_scope;
	v8::String::Utf8Value exception(try_catch->Exception());
	const char* exception_string = ToCString(exception);
	v8::Handle<v8::Message> message = try_catch->Message();

	if (message.IsEmpty()) {
		// V8 didn't provide any extra information about this error; just
		// print the exception.
		LOGD(exception_string);
	} else {
		// Print (filename):(line number): (message).
		v8::String::Utf8Value filename(message->GetScriptResourceName());
		const char* filename_string = ToCString(filename);
		int linenum = message->GetLineNumber();
		LOGD("%s:%i: %s\n", filename_string, linenum, exception_string);
		// Print line of source code.
		v8::String::Utf8Value sourceline(message->GetSourceLine());
		const char* sourceline_string = ToCString(sourceline);
		LOGD("%s\n", sourceline_string);
		std::stringstream str;
		// Print wavy underline (GetUnderline is deprecated).
		int start = message->GetStartColumn();
		for (int i = 0; i < start; i++) {
			str << " ";
		}
		int end = message->GetEndColumn();
		for (int i = start; i < end; i++) {
			str << "^";
		}
		LOGD(str.str().c_str());
		v8::String::Utf8Value stack_trace(try_catch->StackTrace());
		if (stack_trace.length() > 0) {
			const char* stack_trace_string = ToCString(stack_trace);
			LOGD(stack_trace_string);
		}
	}
}

void v8Engine::log(int debugLevel, const Arguments& args) {
	HandleScope scope;

	std::stringstream str;
	int l = args.Length();
	for (int i = 0; i < l; i++) {
		String::Utf8Value value(args[i]);
		str << " " << ToCString(value);
	}

	__android_log_print(debugLevel, LOG_TAG, str.str().c_str());
}

int v8Engine::run(JNIEnv * env, jobject obj) {
	if (!validScriptLoaded) {
		LOGE("run called when no valid script loaded");
	}
	// Create a stack-allocated handle scope.
	HandleScope handle_scope;
	v8::TryCatch try_catch;
	Context::Scope context_scope(context);

	this->envCache = env;

	// Run the script to get the result.
	Handle<Value> result = script->Run();

	this->envCache = NULL;

	if (result.IsEmpty()) {
		// Print errors that happened during execution.
		ReportException(&try_catch);
		return false;
	}

	// The script compiled and ran correctly.  Now we fetch out the
	// Process function from the global object.
	Handle<String> timer_name = String::New("window");

	Handle<Value> timer_val = context->Global()->Get(timer_name);

	// If there is no Process function, or if it is not a function,
	// bail out
	if (!timer_val->IsObject()) {
		LOGE("Cannot find mocktimer window object\n");
		return false;
	}

	// It is a function; cast it to a Function
	Handle<Object> timer_fun = Handle<Object>::Cast(timer_val);
	mockTimer = Persistent<Object>::New(timer_fun);
	Handle<Value> tick_val = timer_fun->Get(String::New("tick"));
	if (!tick_val->IsFunction()) {
		LOGE("Cannot find mocktimer.tick function\n");
		return false;
	}
	Handle<Function> tick_fun = Handle<Function>::Cast(tick_val);

	// Store the function in a Persistent handle, since we also want
	// that to remain after this call returns
	mockTimerTick = Persistent<Function>::New(tick_fun);

	/* volatile bool bGo = 0;

	while(!bGo) {
	  sleep(1);
	} */

	return 0;
}

v8Engine::~v8Engine() {
	LOGI("Cleaning up");
	if (!mockTimerTick.IsEmpty()) {
		mockTimerTick.Dispose();
	}
	if (!mockTimer.IsEmpty()) {
		mockTimer.Dispose();
	}
	if (!global.IsEmpty()) {
		global.Dispose();
	}
	if (!script.IsEmpty()) {
		script.Dispose();
	}
	if (!cloneObjectMethod.IsEmpty()) {
		cloneObjectMethod.Dispose();
	}
	if (!jsonParseMethod.IsEmpty()) {
		jsonParseMethod.Dispose();
	}

	// Dispose the persistent context.
	context.Dispose();
}

