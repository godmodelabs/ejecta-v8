/**
 * BGJSV8Engine
 * Manages a v8 context and exposes script load and execute functions
 *
 * Copyright 2014 Kevin Read <me@kevin-read.com> and BörseGo AG (https://github.com/godmodelabs/ejecta-v8/)
 * Licensed under the MIT license.
 */

#include <libplatform/libplatform.h>
#include "BGJSV8Engine.h"
#include "../jni/JNIWrapper.h"
#include "../v8/JNIV8Wrapper.h"
#include "../v8/JNIV8GenericObject.h"
#include "../v8/JNIV8Function.h"

#include "mallocdebug.h"
#include <assert.h>
#include <sstream>

#include "BGJSGLView.h"

#define LOG_TAG	"BGJSV8Engine-jni"

using namespace v8;

//-----------------------------------------------------------
// Utility functions
//-----------------------------------------------------------

std::vector<std::string> &split(const std::string &s, char delim,
								std::vector<std::string> &elems) {
	std::stringstream ss(s);
	std::string item;
	while (std::getline(ss, item, delim)) {
		elems.push_back(item);
	}
	return elems;
}

std::string normalize_path(std::string& path) {
	std::vector < std::string > pathParts;
	std::stringstream ss(path);
	std::string item;
	while (std::getline(ss, item, '/')) {
		pathParts.push_back(item);
	}
	std::string outPath;

	int length = pathParts.size();
	int i = 0;
	if (length > 0 && pathParts.at(0).compare("..") == 0) {
		i = 1;
	}
	for (; i < length - 1; i++) {
		std::string pathPart = pathParts.at(i + 1);
		if (pathPart.compare("..") != 0) {
			std::string nextSegment = pathParts.at(i);
			if (nextSegment.compare(".") == 0) {
				continue;
			}
			if (outPath.length() > 0) {
				outPath.append("/");
			}
			outPath.append(pathParts.at(i));
		} else {
			i++;
		}
	}
	outPath.append("/").append(pathParts.at(length - 1));
	return outPath;
}

std::string getPathName(std::string& path) {
	size_t found = path.find_last_of("/");

	if (found == string::npos) {
		return path;
	}
	return path.substr(0, found);
}

void find_and_replace(std::string& source, std::string const& find, std::string const& replace)
{
	for(std::string::size_type i = 0; (i = source.find(find, i)) != std::string::npos;)
	{
		source.replace(i, find.length(), replace);
		i += replace.length();
	}
}

//-----------------------------------------------------------
// V8 function callbacks
//-----------------------------------------------------------

static void LogCallback(const v8::FunctionCallbackInfo<Value>& args) {
	if (args.Length() < 1) {
		return;
	}

	BGJSV8Engine *ctx = BGJS_CURRENT_V8ENGINE(args.GetIsolate());

	ctx->log(LOG_INFO, args);
}

static void DebugCallback(const v8::FunctionCallbackInfo<Value>& args) {
    args.GetReturnValue().SetUndefined();
	if (args.Length() < 1) {
		return;
	}

	BGJSV8Engine *ctx = BGJS_CURRENT_V8ENGINE(args.GetIsolate());
	ctx->log(LOG_DEBUG, args);
}

static void InfoCallback(const v8::FunctionCallbackInfo<Value>& args) {
    args.GetReturnValue().SetUndefined();
	if (args.Length() < 1) {
		return;
	}

	BGJSV8Engine *ctx = BGJS_CURRENT_V8ENGINE(args.GetIsolate());

	ctx->log(LOG_INFO, args);
}

static void ErrorCallback(const v8::FunctionCallbackInfo<Value>& args) {
    args.GetReturnValue().SetUndefined();
	if (args.Length() < 1) {
		return;
	}

	BGJSV8Engine *ctx = BGJS_CURRENT_V8ENGINE(args.GetIsolate());

	ctx->log(LOG_ERROR, args);
}

static void RequireCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
	Isolate *isolate = args.GetIsolate();

	// argument must be exactly one string
	if (args.Length() < 1 || !args[0]->IsString()) {
		args.GetReturnValue().SetUndefined();
		return;
	}

	EscapableHandleScope scope(isolate);

	BGJSV8Engine *engine = BGJS_CURRENT_V8ENGINE(isolate);

	Local<Value> result = engine->require(BGJS_STRING_FROM_V8VALUE(args[0]));

	args.GetReturnValue().Set(scope.Escape(result));
}

//-----------------------------------------------------------
// V8Engine
//-----------------------------------------------------------

decltype(BGJSV8Engine::_jniV8Module) BGJSV8Engine::_jniV8Module;

/* static Handle<Value> AssertCallback(const Arguments& args) {
 if (args.Length() < 1)
 return v8::Undefined();

 BGJSV8Engine::log(LOG_ERROR, args);
 return v8::Undefined();
 } */

/* BGJSV8Engine& BGJSV8Engine::getInstance()
 {
 static BGJSV8Engine    instance; // Guaranteed to be destroyed.
 // Instantiated on first use.
 return instance;
 } */

/**
 * internal struct for storing information for wrapped java functions
 */
struct BGJSV8EngineJavaErrorHolder {
    v8::Persistent<v8::Object> persistent;
    jthrowable throwable;
};

void BGJSV8EngineJavaErrorHolderWeakPersistentCallback(const v8::WeakCallbackInfo<void>& data) {
    JNIEnv *env = JNIWrapper::getEnvironment();

    BGJSV8EngineJavaErrorHolder *holder = reinterpret_cast<BGJSV8EngineJavaErrorHolder*>(data.GetParameter());
    env->DeleteGlobalRef(holder->throwable);

    holder->persistent.Reset();
    delete holder;
}

bool BGJSV8Engine::forwardJNIExceptionToV8() {
    JNIEnv *env = JNIWrapper::getEnvironment();
    jthrowable e = env->ExceptionOccurred();
    if(!e) return false;
    env->ExceptionClear();

    HandleScope scope(_isolate);
    Local<Context> context = getContext();

    /*
     * `e` could be an instance of V8Exception containing a v8 error
     * but we do NOT unwrap it and use the existing error because then we would loose some additional java stack trace entries
     */

    // Init
    if (_makeJavaErrorFn.IsEmpty()) {
        Local<Function> makeJavaErrorFn_ =
                Local<Function>::Cast(
                        Script::Compile(
                                String::NewFromOneByte(Isolate::GetCurrent(),(const uint8_t*)
                                        "(function() {"
                                        "function makeJavaError(message) { return new JavaError(message); };"
                                        "function JavaError(message) {"
                                                "this.name = 'JavaError';"
                                                "this.message = message || 'An exception was thrown in Java';"
                                                "const _ = Error.prepareStackTrace;"
                                                "Error.prepareStackTrace = (_, stack) => stack;"
                                                "Error.captureStackTrace(this, makeJavaError);"
                                                "Error.prepareStackTrace = _;"
                                        "}"
                                        "JavaError.prototype = Object.create(Error.prototype);"
                                        "JavaError.prototype.constructor = JavaError;"
                                        "return makeJavaError;"
                                        "}())"
                                ),
                                String::NewFromOneByte(Isolate::GetCurrent(), (const uint8_t*)"binding:makeJavaError"))->Run());
        _makeJavaErrorFn.Reset(_isolate, makeJavaErrorFn_);
    }

    BGJSV8EngineJavaErrorHolder *holder = new BGJSV8EngineJavaErrorHolder();
    Handle<Value> args[] = {};

    Local<Function> makeJavaErrorFn = Local<Function>::New(_isolate, _makeJavaErrorFn);
    Local<Object> result = makeJavaErrorFn->Call(context->Global(), 0, args).As<Object>();

	auto privateKey = v8::Private::ForApi(_isolate, v8::String::NewFromUtf8(_isolate, "JavaErrorExternal"));
	result->SetPrivate(context, privateKey, External::New(_isolate, holder));

	holder->throwable = (jthrowable)env->NewGlobalRef(e);
    holder->persistent.Reset(_isolate, result);
    holder->persistent.SetWeak((void*)holder, BGJSV8EngineJavaErrorHolderWeakPersistentCallback, v8::WeakCallbackType::kParameter);

    _isolate->ThrowException(result);

    return true;
}

#define CALLSITE_STRING(M, V)\
maybeValue = callSite->Get(context, String::NewFromOneByte(_isolate, (uint8_t *) M));\
if(maybeValue.ToLocal(&value) && value->IsFunction()) {\
maybeValue = value.As<Function>()->Call(context, callSite, 0, nullptr);\
if(maybeValue.ToLocal(&value) && value->IsString()) {\
V = JNIV8Wrapper::v8string2jstring(value.As<String>());\
}\
}

bool BGJSV8Engine::forwardV8ExceptionToJNI(v8::TryCatch* try_catch) {
    // @TODO: cache classes
    // @TODO: different Exception classes? the one with the JS stacktrace should be V8JSException?
    // @TODO: replace usage of ReportException in JNIV8Object and other places with forward*
    // @TODO: remove ReportException completely?

    if(!try_catch->HasCaught()) return false;

    JNIEnv *env = JNIWrapper::getEnvironment();

    HandleScope scope(_isolate);
    Local<Context> context = getContext();

    jclass exceptionCls = env->FindClass("ag/boersego/bgjs/V8Exception");

    jobject causeException = nullptr;

    MaybeLocal<Value> maybeValue;
    Local<Value> value;

    // if the v8 error is a `JavaError` that means it already contains a java exception
    // => we unwrap the error and reuse that exception
    // if the original error was a v8 error we still have that contained in the java exception along with a full v8 stack trace steps
    // NOTE: if it was a java error, then we do lose the v8 stack trace however..
    // the only way to keep it would be to use yet another type of Java exception that contains both the original java exception and the first created JavaError
    Local<Value> exception = try_catch->Exception();
    Local<Object> exceptionObj;
    if(exception->IsObject()) {
        exceptionObj = exception.As<Object>();
        auto privateKey = v8::Private::ForApi(_isolate, v8::String::NewFromUtf8(_isolate,
                                                                                "JavaErrorExternal"));
        maybeValue = exceptionObj->GetPrivate(context, privateKey);
        if (maybeValue.ToLocal(&value) && value->IsExternal()) {
            BGJSV8EngineJavaErrorHolder *holder = static_cast<BGJSV8EngineJavaErrorHolder *>(value.As<External>()->Value());

            if(!env->IsInstanceOf(holder->throwable, exceptionCls)) {
                // if the wrapped java exception was not of type V8Exception
                // then we need to wrap it to preserve the v8 call stack
                causeException = env->NewLocalRef(holder->throwable);
            } else {
                // otherwise we can reuse the embedded V8Exception!
                env->Throw((jthrowable) env->NewLocalRef(holder->throwable));
                return true;
            }
        }
    }

    jobject exceptionAsObject = JNIV8Wrapper::v8value2jobject(exception);

    jmethodID initId = env->GetMethodID(exceptionCls, "<init>", "(Ljava/lang/String;Ljava/lang/Object;Ljava/lang/Throwable;)V");
    jmethodID setStackTraceID = env->GetMethodID(exceptionCls, "setStackTrace", "([Ljava/lang/StackTraceElement;)V");
    jclass stackTraceElementCls = env->FindClass("java/lang/StackTraceElement");
    jmethodID newStackTraceElementId = env->GetMethodID(stackTraceElementCls, "<init>",
                                                        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;I)V");

    // convert v8 stack trace to a java stack trace
    jobject v8Exception = env->NewObject(exceptionCls, initId, JNIV8Wrapper::v8string2jstring(exception->ToString()), exceptionAsObject, causeException);

    jobjectArray stackTrace = nullptr;
    if(exception->IsObject()) {
        // Init
        if (_getStackTraceFn.IsEmpty()) {
            Local<Function> getStackTraceFn_ =
                    Local<Function>::Cast(
                            Script::Compile(
                                    String::NewFromOneByte(Isolate::GetCurrent(),(const uint8_t*)
                                            "(function(e) {"
                                                    "const _ = Error.prepareStackTrace;"
                                                    "Error.prepareStackTrace = (_, stack) => stack;"
                                                    "const stack = e.stack;"
                                                    "Error.prepareStackTrace = _;"
                                                    "return stack;"
                                            "})"
                                    ),
                                    String::NewFromOneByte(Isolate::GetCurrent(), (const uint8_t*)"binding:getStackTrace"))->Run());
            _getStackTraceFn.Reset(_isolate, getStackTraceFn_);
        }
        Local<Function> getStackTraceFn = Local<Function>::New(_isolate, _getStackTraceFn);

        maybeValue = getStackTraceFn->Call(context, context->Global(), 1, &exception);
        if (maybeValue.ToLocal(&value) && value->IsArray()) {
            Local<Array> array = value.As<Array>();

            bool error = false;
            uint32_t size = array->Length();
            stackTrace = env->NewObjectArray(size, stackTraceElementCls, nullptr);

            jobject stackTraceElement;
            for(uint32_t i=0; i<size; i++) {
                maybeValue = array->Get(context, i);
                if(maybeValue.ToLocal(&value) && value->IsObject()) {
                    Local<Object> callSite = value.As<Object>();

                    jstring fileName = nullptr;
                    jstring methodName = nullptr;
                    jstring functionName = nullptr;
                    jstring typeName = nullptr;
                    jint lineNumber = 0;

                    CALLSITE_STRING("getFileName", fileName);
                    CALLSITE_STRING("getMethodName", methodName);
                    CALLSITE_STRING("getFunctionName", functionName);
                    CALLSITE_STRING("getTypeName", typeName);

                    maybeValue = callSite->Get(context, String::NewFromOneByte(_isolate, (uint8_t *) "getLineNumber"));
                    if(maybeValue.ToLocal(&value) && value->IsFunction()) {
                        maybeValue = value.As<Function>()->Call(context, callSite, 0, nullptr);
                        if(maybeValue.ToLocal(&value) && value->IsNumber()) {
                            lineNumber = (jint)value.As<Number>()->IntegerValue();
                        }
                    }

                    stackTraceElement = env->NewObject(stackTraceElementCls, newStackTraceElementId,
                                                       typeName ? typeName : JNIWrapper::string2jstring("function"),
                                                       !methodName && !functionName ? JNIWrapper::string2jstring("<anonymous>"): methodName ? methodName : functionName,
                                                       fileName ? fileName : JNIWrapper::string2jstring("<unknown>"),
                                                       lineNumber);
                    env->SetObjectArrayElement(stackTrace, i, stackTraceElement);
                } else {
                    error = true;
                    break;
                }
            }

            // apply trace to exception
            if(!error) {
                env->CallVoidMethod(v8Exception, setStackTraceID, stackTrace);
            }
        }
    }

    // throw final exception
    env->Throw((jthrowable)env->NewObject(exceptionCls, initId, JNIWrapper::string2jstring("An exception occured in JavaScript"), exceptionAsObject, v8Exception));

    return true;
}

// Register
bool BGJSV8Engine::registerModule(const char* name, requireHook requireFn) {
	// v8::Locker l(Isolate::GetCurrent());
	// HandleScope scope(Isolate::GetCurrent());
	// module->initWithContext(this);
	_modules[name] = requireFn;
	// _modules.insert(std::pair<char const*, BGJSModule*>(module->getName(), module));

	return true;
}

void BGJSV8Engine::JavaModuleRequireCallback(BGJSV8Engine *engine, v8::Handle<v8::Object> target) {
	Isolate *isolate = engine->getIsolate();
	HandleScope scope(isolate);
	Local<Context> context = engine->getContext();

	MaybeLocal<Value> maybeLocal = target->Get(context, String::NewFromUtf8(isolate, "id"));
	if(maybeLocal.IsEmpty()) {
		return;
	}
	std::string moduleId = BGJS_STRING_FROM_V8VALUE(maybeLocal.ToLocalChecked().As<String>());

	jobject module = engine->_javaModules.at(moduleId);

	JNIEnv *env = JNIWrapper::getEnvironment();
	env->CallVoidMethod(module, _jniV8Module.requireId, engine->getJObject(), JNIV8Wrapper::wrapObject<JNIV8GenericObject>(target)->getJObject());
}

bool BGJSV8Engine::registerJavaModule(jobject module) {
	JNIEnv *env = JNIWrapper::getEnvironment();
	if(!_jniV8Module.clazz) {
		_jniV8Module.clazz = (jclass)env->NewGlobalRef(env->FindClass("ag/boersego/bgjs/JNIV8Module"));
		_jniV8Module.getNameId = env->GetMethodID(_jniV8Module.clazz, "getName", "()Ljava/lang/String;");
		_jniV8Module.requireId = env->GetMethodID(_jniV8Module.clazz, "Require", "(Lag/boersego/bgjs/V8Engine;Lag/boersego/bgjs/JNIV8GenericObject;)V");
	}

	std::string strModuleName = JNIWrapper::jstring2string((jstring)env->CallObjectMethod(module, _jniV8Module.getNameId));
	_javaModules[strModuleName] = env->NewGlobalRef(module);
	_modules["javaModule"] = (requireHook)&BGJSV8Engine::JavaModuleRequireCallback;

	return true;
}

uint8_t BGJSV8Engine::requestEmbedderDataIndex() {
    return _nextEmbedderDataIndex++;
}

//////////////////////////
// Require

Handle<Value> BGJSV8Engine::JsonParse(Handle<Object> recv,
		Handle<String> source) {
	EscapableHandleScope scope(_isolate);
	Handle<Value> args[] = { source };

	// Init
	if (_jsonParseFn.IsEmpty()) {
	    Local<Function> jsonParseMethod_ =
			Local<Function>::Cast(
					Script::Compile(
							String::NewFromOneByte(Isolate::GetCurrent(),
									(const uint8_t*)"(function(source) { return JSON.parse(source); })"),
							String::NewFromOneByte(Isolate::GetCurrent(), (const uint8_t*)"binding:JSONParse"))->Run());
	    _jsonParseFn.Reset(_isolate, jsonParseMethod_);
	}

    Local<Function> jsonParseFn = Local<Function>::New(_isolate, _jsonParseFn);
	Local<Value> result = jsonParseFn->Call(recv, 1, args);

	return scope.Escape(result);
}

Handle<Value> BGJSV8Engine::JsonStringify(Handle<Object> recv,
									  Handle<Object> source) {
	EscapableHandleScope scope(Isolate::GetCurrent());

	Handle<Value> args[] = { source };

	// Init
	if (_jsonStringifyFn.IsEmpty()) {
	    Local<Function> jsonStringifyMethod_ =
			Local<Function>::Cast(
					Script::Compile(
							String::NewFromOneByte(Isolate::GetCurrent(),
												   (const uint8_t*)"(function(source) { return JSON.stringify(source); })"),
							String::NewFromOneByte(Isolate::GetCurrent(), (const uint8_t*)"binding:JSONStringify"))->Run());
        _jsonStringifyFn.Reset(_isolate, jsonStringifyMethod_);
	}
    Local<Function> jsonStringifyFn = Local<Function>::New(_isolate, _jsonStringifyFn);
	Local<Value> result = jsonStringifyFn->Call(recv, 1, args);

	return scope.Escape(result);
}

Handle<Value> BGJSV8Engine::callFunction(Isolate* isolate, Handle<Object> recv, const char* name,
		int argc, Handle<Value> argv[]) const {
	v8::Locker l(isolate);
	EscapableHandleScope scope(isolate);
	TryCatch trycatch;

	Local<Function> fn = Handle<Function>::Cast(recv->Get(String::NewFromUtf8(isolate, name)));
			String::Utf8Value value(fn);
	
	Local<Value> result = fn->Call(recv, argc, argv);
	if (result.IsEmpty()) {
		LOGE("callFunction exception");
		BGJSV8Engine::ReportException(&trycatch);
		return v8::Undefined(isolate);
	}
	return scope.Escape(result);
}

v8::Local<v8::Function> BGJSV8Engine::makeRequireFunction(std::string pathName) {
    Local<Context> context = _isolate->GetCurrentContext();
    EscapableHandleScope handle_scope(_isolate);
    Local<Function> makeRequireFn, baseRequireFn;

    if(_makeRequireFn.IsEmpty()) {
        const char *szJSRequireCode =
                "(function(require, prefix) {"
                        "   return function(path) {"
                        "       return require(path.indexOf('./')===0?'./'+prefix+'/'+path.substr(2):path);"
                        "   };"
                        "})";

        makeRequireFn =
                Local<Function>::Cast(
                        Script::Compile(
                                String::NewFromOneByte(_isolate, (const uint8_t *) szJSRequireCode),
                                String::NewFromOneByte(_isolate, (const uint8_t *) "binding:makeRequireFn")
                        )->Run()
                );
        baseRequireFn = v8::FunctionTemplate::New(_isolate, RequireCallback)->GetFunction();
        _makeRequireFn.Reset(_isolate, makeRequireFn);
        _requireFn.Reset(_isolate, baseRequireFn);
    } else {
        makeRequireFn = Local<Function>::New(_isolate, _makeRequireFn);
        baseRequireFn = Local<Function>::New(_isolate, _requireFn);
    }

    Handle<Value> args[] = { baseRequireFn, String::NewFromUtf8(_isolate, pathName.c_str()) };
    Local<Value> result = makeRequireFn->Call(context->Global(), 2, args);
    return handle_scope.Escape(Local<Function>::Cast(result));
}

Local<Value> BGJSV8Engine::require(std::string baseNameStr){
    Local<Context> context = _isolate->GetCurrentContext();
    EscapableHandleScope handle_scope(_isolate);

	Local<Value> result;
    // Catch errors
    TryCatch try_catch;

    if(baseNameStr.find("./") == 0) {
        baseNameStr = baseNameStr.substr(2);
        find_and_replace(baseNameStr, std::string("/./"), std::string("/"));
        baseNameStr = normalize_path(baseNameStr);
    }
    bool isJson = false;

    // check cache first
    std::map<std::string, v8::Persistent<v8::Value>>::iterator it;
    it = _moduleCache.find(baseNameStr);
    if(it != _moduleCache.end()) {
        return handle_scope.Escape(Local<Value>::New(_isolate, it->second));
    }

    // Source of JS file if external code
    Handle<String> source;
    const char* buf = 0;

    // Check if this is an internal module
    requireHook module = _modules[baseNameStr];
	
    if (module) {
        Local<Object> exportsObj = Object::New(_isolate);
        Local<Object> moduleObj = Object::New(_isolate);
		moduleObj->Set(String::NewFromUtf8(_isolate, "id"), String::NewFromUtf8(_isolate, baseNameStr.c_str()));
		moduleObj->Set(String::NewFromUtf8(_isolate, "exports"), exportsObj);

        module(this, moduleObj);
        result = moduleObj->Get(String::NewFromUtf8(_isolate, "exports"));
        _moduleCache[baseNameStr].Reset(_isolate, result);
        return handle_scope.Escape(result);
    }
    std::string fileName, pathName;

    fileName = baseNameStr;
    buf = this->_client->loadFile(fileName.c_str());
    if (!buf) {
        // Check if this is a directory containing index.js or package.json
        fileName = baseNameStr + "/package.json";
        buf = this->_client->loadFile(fileName.c_str());

        if (!buf) {
            // It might be a directory with an index.js
            fileName = baseNameStr + "/index.js";
            buf = this->_client->loadFile(fileName.c_str());

            if (!buf) {
                // So it might just be a js file
                fileName = baseNameStr + ".js";
                buf = this->_client->loadFile(fileName.c_str());

                if (!buf) {
                    // No JS file, but maybe JSON?
                    fileName = baseNameStr + ".json";
                    buf = this->_client->loadFile(fileName.c_str());
					
                    if (buf) {
                        isJson = true;
                    }
                }
            }
        } else {
            // Parse the package.json
            // Create a string containing the JSON source
            source = String::NewFromUtf8(_isolate, buf);
            Handle<Object> res = BGJSV8Engine::JsonParse(
                    _isolate->GetEnteredContext()->Global(), source)->ToObject();
            Handle<String> mainStr = String::NewFromUtf8(_isolate, (const char *) "main");
            if (res->Has(mainStr)) {
                Handle<String> jsFileName = res->Get(mainStr)->ToString();
                String::Utf8Value jsFileNameC(jsFileName);

                fileName = baseNameStr + "/" + *jsFileNameC;
                if (buf) {
                    free((void *) buf);
                    buf = NULL;
                }

                // It might be a directory with an index.js
                buf = this->_client->loadFile(fileName.c_str());
            } else {
                LOGE("%s doesn't have a main object: %s", baseNameStr.c_str(), buf);
                if (buf) {
                    free((void *) buf);
                    buf = NULL;
                }
            }
        }
    } else if(baseNameStr.find(".json") == baseNameStr.length() - 5) {
        isJson = true;
    }

    if (!buf) {
        LOGE("Cannot find file %s", baseNameStr.c_str());
        //log(LOG_ERROR, args);
        return Undefined(_isolate);
    }

    if (isJson) {
        // Create a string containing the JSON source
        source = String::NewFromUtf8(_isolate, buf);
        Local<Value> res = BGJSV8Engine::JsonParse(
                _isolate->GetEnteredContext()->Global(), source);
        free((void*) buf);
        return handle_scope.Escape(res);
    }

    pathName = getPathName(fileName);

    // wrap source in anonymous function to set up an isolated scope
    const char *szSourcePrefix = "(function (exports, require, module, __filename, __dirname) {";
    const char *szSourcePostfix = "})";
    source = String::Concat(
            String::Concat(
                    String::NewFromUtf8(_isolate, szSourcePrefix),
                    String::NewFromUtf8(_isolate, buf)
            ),
            String::NewFromUtf8(_isolate, szSourcePostfix)
    );
    free((void*) buf);

    // compile script
    Handle<Script> scriptR = Script::Compile(source, String::NewFromUtf8(_isolate, baseNameStr.c_str()));

    // run script; this will effectively return a function if everything worked
    // if not, something went wrong
    if(!scriptR.IsEmpty()) {
        result = scriptR->Run();
    }

    // if we received a function, run it!
    if (!result.IsEmpty() && result->IsFunction()) {
        Local<Function> requireFn = makeRequireFunction(pathName);

        Local<Object> exportsObj = Object::New(_isolate);
        Local<Object> moduleObj = Object::New(_isolate);
		moduleObj->Set(String::NewFromUtf8(_isolate, "id"), String::NewFromUtf8(_isolate, fileName.c_str()));
		moduleObj->Set(String::NewFromUtf8(_isolate, "environment"), String::NewFromUtf8(_isolate, "BGJSContext"));
        moduleObj->Set(String::NewFromUtf8(_isolate, "exports"), exportsObj);

        Handle<Value> fnModuleInitializerArgs[] = {
                exportsObj,                                      // exports
                requireFn,                                       // require
                moduleObj,                                       // module
                String::NewFromUtf8(_isolate, fileName.c_str()), // __filename
                String::NewFromUtf8(_isolate, pathName.c_str())  // __dirname
        };
        Local<Function> fnModuleInitializer = Local<Function>::Cast(result);
        fnModuleInitializer->Call(context->Global(), 5, fnModuleInitializerArgs);

        if(!try_catch.HasCaught()) {
            result = moduleObj->Get(String::NewFromUtf8(_isolate, "exports"));
			_moduleCache[baseNameStr].Reset(_isolate, result);
            return handle_scope.Escape(result);
        }
    }

    // somewhere along the way an exception was raised.. process it here
    if(try_catch.HasCaught()) {
        BGJSV8Engine::ReportException(&try_catch);

        /* ReThrow doesn't re-throw TerminationExceptions; a null exception value
         * is re-thrown instead (see Isolate::PropagatePendingExceptionToExternalTryCatch())
         * so we re-propagate a TerminationException explicitly here if necessary. */
        if (!try_catch.CanContinue()) {
            v8::V8::TerminateExecution(_isolate);
        }
    }

    // this only happens when something went wrong (e.g. exception)
    return Undefined(_isolate);
}

void BGJSV8Engine::setClient(ClientAbstract* client) {
	this->_client = client;
}

ClientAbstract* BGJSV8Engine::getClient() const {
    return this->_client;
}

v8::Isolate* BGJSV8Engine::getIsolate() const {
    return this->_isolate;
}

v8::Local<v8::Context> BGJSV8Engine::getContext() const {
	EscapableHandleScope scope(_isolate);
	return scope.Escape(Local<Context>::New(_isolate, _context));
}

void BGJSV8Engine::cancelAnimationFrame(int id) {
	for (std::set<BGJSGLView*>::iterator it = _glViews.begin();
			it != _glViews.end(); ++it) {
		int i = 0;
		while (i < MAX_FRAME_REQUESTS) {
			AnimationFrameRequest *request = &((*it)->_frameRequests[i]);
			if (request->requestId == id && request->valid
					&& request->view != NULL) {
				request->valid = false;
				BGJS_CLEAR_PERSISTENT(request->callback);
				BGJS_CLEAR_PERSISTENT(request->thisObj);
				break;
			}
			i++;
		}
	}
}

void BGJSV8Engine::registerGLView(BGJSGLView* view) {
	_glViews.insert(view);
}

void BGJSV8Engine::unregisterGLView(BGJSGLView* view) {
	_glViews.erase(view);
}

bool BGJSV8Engine::runAnimationRequests(BGJSGLView* view) const  {
	v8::Locker l(_isolate);
    Isolate::Scope isolateScope(_isolate);
	HandleScope scope(_isolate);

	TryCatch trycatch;
	bool didDraw = false;

	AnimationFrameRequest *request;
	int index = view->_firstFrameRequest, nextIndex = view->_nextFrameRequest,
			startFrame = view->_firstFrameRequest;
	while (index != nextIndex) {
		request = &(view->_frameRequests[index]);

		if (request->valid) {
			didDraw = true;
			request->view->prepareRedraw();
			Handle<Value> args[0];

			Handle<Value> result = Local<Object>::New(_isolate, request->callback)->CallAsFunction(
					Local<Object>::New(_isolate, request->thisObj), 0, args);

			if (result.IsEmpty()) {
				LOGE("Exception occured while running runAnimationRequest cb");
				BGJSV8Engine::ReportException(&trycatch);
			}

			String::Utf8Value fnName(Local<Object>::New(_isolate, request->callback)->ToString());
			BGJS_CLEAR_PERSISTENT(request->callback);
			BGJS_CLEAR_PERSISTENT(request->thisObj);

			request->view->endRedraw();
			request->valid = false;
			request->view = NULL;
		}

		index = (index + 1) % MAX_FRAME_REQUESTS;
	}
	while (index != nextIndex)
		;

	view->_firstFrameRequest = nextIndex;

	// If we couldn't draw anything, request that we can the next time
	if (!didDraw) {
		view->call(view->_cbRedraw);
	}
	return didDraw;
}

void BGJSV8Engine::js_global_getLocale(Local<String> property,
		const v8::PropertyCallbackInfo<v8::Value>& info) {
	EscapableHandleScope scope(Isolate::GetCurrent());
	BGJSV8Engine *ctx = BGJS_CURRENT_V8ENGINE(info.GetIsolate());

	if (ctx->_locale) {
		info.GetReturnValue().Set(scope.Escape(String::NewFromUtf8(Isolate::GetCurrent(), ctx->_locale)));
	} else {
	    info.GetReturnValue().SetNull();
	}
}

void BGJSV8Engine::js_global_getLang(Local<String> property,
		const v8::PropertyCallbackInfo<v8::Value>& info) {
	EscapableHandleScope scope(Isolate::GetCurrent());
	BGJSV8Engine *ctx = BGJS_CURRENT_V8ENGINE(info.GetIsolate());

	if (ctx->_lang) {
		info.GetReturnValue().Set(scope.Escape(String::NewFromUtf8(Isolate::GetCurrent(), ctx->_lang)));
	} else {
		info.GetReturnValue().SetNull();
	}
}

void BGJSV8Engine::js_global_getTz(Local<String> property,
		const v8::PropertyCallbackInfo<v8::Value>& info) {
	EscapableHandleScope scope(Isolate::GetCurrent());
	BGJSV8Engine *ctx = BGJS_CURRENT_V8ENGINE(info.GetIsolate());

	if (ctx->_tz) {
		info.GetReturnValue().Set(scope.Escape(String::NewFromUtf8(Isolate::GetCurrent(), ctx->_tz)));
	} else {
		info.GetReturnValue().SetNull();
	}
}

void BGJSV8Engine::js_global_getDeviceClass(Local<String> property,
                                           const v8::PropertyCallbackInfo<v8::Value>& info) {
    EscapableHandleScope scope(Isolate::GetCurrent());
    BGJSV8Engine *ctx = BGJS_CURRENT_V8ENGINE(info.GetIsolate());

    if (ctx->_deviceClass) {
        info.GetReturnValue().Set(scope.Escape(String::NewFromUtf8(Isolate::GetCurrent(), ctx->_deviceClass)));
    } else {
        info.GetReturnValue().SetNull();
    }
}

void BGJSV8Engine::js_global_requestAnimationFrame(
		const v8::FunctionCallbackInfo<v8::Value>& args) {
    BGJSV8Engine *ctx = BGJS_CURRENT_V8ENGINE(args.GetIsolate());
	v8::Locker l(args.GetIsolate());
	HandleScope scope(args.GetIsolate());


	if (args.Length() >= 2 && args[0]->IsFunction() && args[1]->IsObject()) {
	    Local<Object> localFunc = args[0]->ToObject();
		Handle<Object> objRef = args[1]->ToObject();
		BGJSGLView* view = static_cast<BGJSGLView *>(v8::External::Cast(*(objRef->GetInternalField(0)))->Value());
		if (localFunc->IsFunction()) {
			int id = view->requestAnimationFrameForView(localFunc, args.This(),
					(ctx->_nextTimerId)++);
			args.GetReturnValue().Set(id);
			return;
		} else {
			LOGI("requestAnimationFrame: Not a function");
		}
	} else {
	    LOGI("requestAnimationFrame: Wrong number or type of parameters (num %d, is function %d %d, is object %d %d, is null %d %d)",
	        args.Length(), args[0]->IsFunction(), args.Length() >= 2 ? args[1]->IsFunction() : false,
	        args[0]->IsObject(), args.Length() >= 2 ? args[1]->IsObject() : false,
	        args[0]->IsNull(), args.Length() >= 2 ? args[1]->IsNull() : false);
        ctx->getIsolate()->ThrowException(
				v8::Exception::ReferenceError(
					v8::String::NewFromUtf8(ctx->getIsolate(), "requestAnimationFrame: Wrong number or type of parameters")));
		return;
	}
	args.GetReturnValue().Set(-1);
}

void BGJSV8Engine::js_global_cancelAnimationFrame(
		const v8::FunctionCallbackInfo<v8::Value>& args) {
    BGJSV8Engine *ctx = BGJS_CURRENT_V8ENGINE(args.GetIsolate());
	v8::Locker l(ctx->getIsolate());
    HandleScope scope(ctx->getIsolate());
	if (args.Length() >= 1 && args[0]->IsNumber()) {

		int id = (int) (Local<Number>::Cast(args[0])->Value());
		ctx->cancelAnimationFrame(id);
	}

	args.GetReturnValue().SetUndefined();
}

void BGJSV8Engine::js_global_setTimeout(const v8::FunctionCallbackInfo<v8::Value>& args) {
	setTimeoutInt(args, false);
}

void BGJSV8Engine::js_global_setInterval(const v8::FunctionCallbackInfo<v8::Value>& args) {
	setTimeoutInt(args, true);
}

void BGJSV8Engine::setTimeoutInt(const v8::FunctionCallbackInfo<v8::Value>& args,
		bool recurring) {
    BGJSV8Engine *ctx = BGJS_CURRENT_V8ENGINE(args.GetIsolate());
	v8::Locker l(args.GetIsolate());
	HandleScope scope(args.GetIsolate());


	if (args.Length() == 2 && args[0]->IsFunction() && args[1]->IsNumber()) {
		Local<v8::Function> callback = Local<Function>::Cast(args[0]);

		WrapPersistentFunc* ws = new WrapPersistentFunc();
		BGJS_RESET_PERSISTENT(ctx->getIsolate(), ws->callbackFunc, callback);
		WrapPersistentObj* wo = new WrapPersistentObj();
		BGJS_RESET_PERSISTENT(ctx->getIsolate(), wo->obj, args.This());

		jlong timeout = (jlong)(Local<Number>::Cast(args[1])->Value());

		ClientAndroid* client = (ClientAndroid*) (BGJS_CURRENT_V8ENGINE(args.GetIsolate())->_client);
		JNIEnv* env = JNU_GetEnv();
		if (env == NULL) {
			LOGE("Cannot execute setTimeout with no envCache");
			args.GetReturnValue().SetUndefined();
			return;
		}

		jclass clazz = env->FindClass("ag/boersego/bgjs/V8Engine");
		jmethodID pushMethod = env->GetStaticMethodID(clazz, "setTimeout",
				"(JJJZ)I");
		assert(pushMethod);
		assert(clazz);
		int subId = env->CallStaticIntMethod(clazz, pushMethod, (jlong) ws,
				(jlong) wo, timeout, (jboolean) recurring);
        args.GetReturnValue().Set(subId);
	} else {
        ctx->getIsolate()->ThrowException(
				v8::Exception::ReferenceError(
						v8::String::NewFromUtf8(ctx->getIsolate(), "Wrong number of parameters")));
	}
	return;
}

void BGJSV8Engine::js_global_clearInterval(const v8::FunctionCallbackInfo<v8::Value>& args) {
	clearTimeoutInt(args);
}

void BGJSV8Engine::js_global_clearTimeout(const v8::FunctionCallbackInfo<v8::Value>& args) {
	clearTimeoutInt(args);
}

void BGJSV8Engine::clearTimeoutInt(const v8::FunctionCallbackInfo<v8::Value>& args) {
    BGJSV8Engine *ctx = BGJS_CURRENT_V8ENGINE(args.GetIsolate());
	v8::Locker l(ctx->getIsolate());
    HandleScope scope(ctx->getIsolate());

	args.GetReturnValue().SetUndefined();

	if (args.Length() == 1) {

		int id = (int) ((Local<Integer>::Cast(args[0]))->Value());

		if (id == 0) {
			return;
		}

		ClientAndroid* client = (ClientAndroid*) (BGJS_CURRENT_V8ENGINE(args.GetIsolate())->_client);
		JNIEnv* env = JNU_GetEnv();
		if (env == NULL) {
			LOGE("Cannot execute setTimeout with no envCache");
			return;
		}

		jclass clazz = env->FindClass("ag/boersego/bgjs/V8Engine");
		jmethodID pushMethod = env->GetStaticMethodID(clazz, "removeTimeout",
				"(I)V");
		assert(pushMethod);
		assert(clazz);
		env->CallStaticVoidMethod(clazz, pushMethod, (jint) id);
	} else {
        ctx->getIsolate()->ThrowException(
    				v8::Exception::ReferenceError(
    						v8::String::NewFromUtf8(ctx->getIsolate(), "Wrong arguments for clearTimeout")));
		LOGE("Wrong arguments for clearTimeout");
	}
}

BGJSV8Engine::BGJSV8Engine(jobject javaObject) {
	_client = NULL;
	_nextTimerId = 1;
	_locale = NULL;
    _nextEmbedderDataIndex = EBGJSV8EngineEmbedderData::FIRST_UNUSED;

    JNIEnv *env = JNIWrapper::getEnvironment();
    _javaObject = env->NewWeakGlobalRef(javaObject);

    _isolate = NULL;
}

jobject BGJSV8Engine::getJObject() const {
	return _javaObject;
}

void BGJSV8Engine::createContext() {
	static bool isPlatformInitialized = false;

	if(!isPlatformInitialized) {
		isPlatformInitialized = true;
		LOGI("Creating default platform");
		v8::Platform *platform = v8::platform::CreateDefaultPlatform();
		LOGD("Created default platform %p", platform);
		v8::V8::InitializePlatform(platform);
		LOGD("Initialized platform");
		v8::V8::Initialize();
		LOGD("Initialized v8");
	}

	v8::Isolate::CreateParams create_params;
	create_params.array_buffer_allocator =
			v8::ArrayBuffer::Allocator::NewDefaultAllocator();

	_isolate = v8::Isolate::New(create_params);

	v8::Locker l(_isolate);
	Isolate::Scope isolate_scope(_isolate);
	HandleScope scope(_isolate);

	// Create global object template
	v8::Local<v8::ObjectTemplate> globalObjTpl = v8::ObjectTemplate::New();

	// Add methods to console function
	v8::Local<v8::FunctionTemplate> console = v8::FunctionTemplate::New(_isolate);
	console->Set(String::NewFromUtf8(_isolate, "log"),
				 v8::FunctionTemplate::New(_isolate, LogCallback));
	console->Set(String::NewFromUtf8(_isolate, "debug"),
				 v8::FunctionTemplate::New(_isolate, DebugCallback));
	console->Set(String::NewFromUtf8(_isolate, "info"),
				 v8::FunctionTemplate::New(_isolate, InfoCallback));
	console->Set(String::NewFromUtf8(_isolate, "error"),
				 v8::FunctionTemplate::New(_isolate, ErrorCallback));
	console->Set(String::NewFromUtf8(_isolate, "warn"),
				 v8::FunctionTemplate::New(_isolate, ErrorCallback));
	// console->Set("assert", v8::FunctionTemplate::New(AssertCallback)); // TODO

	globalObjTpl->Set(v8::String::NewFromUtf8(_isolate, "console"), console);

	// environment variables
	globalObjTpl->SetAccessor(String::NewFromUtf8(_isolate, "_locale"),
                              BGJSV8Engine::js_global_getLocale, 0, Local<Value>(), AccessControl::DEFAULT, PropertyAttribute::ReadOnly);
	globalObjTpl->SetAccessor(String::NewFromUtf8(_isolate, "_lang"),
                              BGJSV8Engine::js_global_getLang, 0, Local<Value>(), AccessControl::DEFAULT, PropertyAttribute::ReadOnly);
	globalObjTpl->SetAccessor(String::NewFromUtf8(_isolate, "_tz"),
                              BGJSV8Engine::js_global_getTz, 0, Local<Value>(), AccessControl::DEFAULT, PropertyAttribute::ReadOnly);
    globalObjTpl->SetAccessor(String::NewFromUtf8(_isolate, "_deviceClass"),
                              BGJSV8Engine::js_global_getDeviceClass, 0, Local<Value>(), AccessControl::DEFAULT, PropertyAttribute::ReadOnly);

	// global functions
	globalObjTpl->Set(String::NewFromUtf8(_isolate, "requestAnimationFrame"),
				v8::FunctionTemplate::New(_isolate,
										  BGJSV8Engine::js_global_requestAnimationFrame));
	globalObjTpl->Set(String::NewFromUtf8(_isolate, "cancelAnimationFrame"),
				v8::FunctionTemplate::New(_isolate,
										  BGJSV8Engine::js_global_cancelAnimationFrame));
	globalObjTpl->Set(String::NewFromUtf8(_isolate, "setTimeout"),
				v8::FunctionTemplate::New(_isolate, BGJSV8Engine::js_global_setTimeout));
	globalObjTpl->Set(String::NewFromUtf8(_isolate, "setInterval"),
				v8::FunctionTemplate::New(_isolate, BGJSV8Engine::js_global_setInterval));
	globalObjTpl->Set(String::NewFromUtf8(_isolate, "clearTimeout"),
				v8::FunctionTemplate::New(_isolate, BGJSV8Engine::js_global_clearTimeout));
	globalObjTpl->Set(String::NewFromUtf8(_isolate, "clearInterval"),
				v8::FunctionTemplate::New(_isolate, BGJSV8Engine::js_global_clearInterval));

	// Create a new context.
    Local<Context> context = v8::Context::New(_isolate, NULL, globalObjTpl);
	context->SetAlignedPointerInEmbedderData(EBGJSV8EngineEmbedderData::kContext, this);

	_context.Reset(_isolate, context);
}

void BGJSV8Engine::ReportException(v8::TryCatch* try_catch) {
    if(!try_catch->HasCaught()) return;
	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);
	Local<Context> context = isolate->GetCurrentContext();

	const std::string exception_string = BGJS_STRING_FROM_V8VALUE(try_catch->Exception());
	v8::Handle<v8::Message> message = try_catch->Message();

	if (message.IsEmpty()) {
		// V8 didn't provide any extra information about this error; just
		// print the exception.
		LOGE("Exception: %s", exception_string.c_str());
	} else {
		// Print (filename):(line number): (message).
		const std::string filename_string = BGJS_STRING_FROM_V8VALUE(message->GetScriptResourceName());
		int linenum = message->GetLineNumber();
        int colnum = message->GetStartColumn();
		LOGE("Exception: %s:%i:%i %s\n", filename_string.c_str(), linenum, colnum, exception_string.c_str());
		// Print line of source code.
		const std::string sourceline_string = BGJS_STRING_FROM_V8VALUE(message->GetSourceLine());
		LOGE("%s\n", sourceline_string.c_str());
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
		LOGE("%s", str.str().c_str());

		MaybeLocal<Value> stack_trace = try_catch->StackTrace(context);
		if (!stack_trace.IsEmpty()) {
			LOGE("%s", BGJS_STRING_FROM_V8VALUE(try_catch->StackTrace()).c_str());
		}
	}
}

void BGJSV8Engine::log(int debugLevel, const v8::FunctionCallbackInfo<v8::Value>& args) {
	v8::Locker locker(args.GetIsolate());
	HandleScope scope(args.GetIsolate());

	std::stringstream str;
	int l = args.Length();
	for (int i = 0; i < l; i++) {
		str << " " << BGJS_STRING_FROM_V8VALUE(args[i]).c_str();
	}

	LOG(debugLevel, "%s", str.str().c_str());
}

int BGJSV8Engine::run(const char *path) {
    Locker locker(_isolate);
    Isolate::Scope isolate_scope(_isolate);

	HandleScope scope(_isolate);

    v8::TryCatch try_catch;

    // create context
    Local<Context> context = Local<Context>::New(_isolate, _context);

    Context::Scope context_scope(context);

    // register global object for all required modules
    context->Global()->Set(String::NewFromUtf8(_isolate, "global"), context->Global());

    // if an initial module was specified load it immediately
    if(path) {
	    Local<Value> result = require(path);

        if (result.IsEmpty()) {
            ReportException(&try_catch);
            return false;
        }
    }

	return 1;
}

void BGJSV8Engine::setLocale(const char* locale, const char* lang,
                             const char* tz, const char* deviceClass) {
    _locale = strdup(locale);
	_lang = strdup(lang);
	_tz = strdup(tz);
    _deviceClass = strdup(deviceClass);
}

BGJSV8Engine::~BGJSV8Engine() {
	LOGI("Cleaning up");

    JNIEnv* env = JNIWrapper::getEnvironment();
    env->DeleteWeakGlobalRef(_javaObject);

	// clear persistent references
	_context.Reset();
    _requireFn.Reset();
    _makeRequireFn.Reset();
    _jsonParseFn.Reset();
    _jsonStringifyFn.Reset();
    _makeJavaErrorFn.Reset();
    _getStackTraceFn.Reset();

	if (_locale) {
		free(_locale);
	}
    this->_isolate->Exit();

	for(auto &it : _javaModules) {
		env->DeleteGlobalRef(it.second);
	}

	JNIV8Wrapper::cleanupV8Engine(this);
}

extern "C" {
JNIEXPORT jobject JNICALL
Java_ag_boersego_bgjs_V8Engine_parseJSON(JNIEnv *env, jobject obj, jlong enginePtr,
                                              jstring json) {
    BGJSV8Engine *engine = reinterpret_cast<BGJSV8Engine *>(enginePtr);

    v8::Isolate* isolate = engine->getIsolate();
    v8::Locker l(isolate);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = engine->getContext();
    v8::Context::Scope ctxScope(context);

    v8::TryCatch try_catch;
    v8::Local<v8::Value> value = engine->JsonParse(context->Global(), JNIV8Wrapper::jstring2v8string(json));
    if(value.IsEmpty()) {
        engine->forwardV8ExceptionToJNI(&try_catch);
        return nullptr;
    }
    return JNIV8Wrapper::v8value2jobject(value);
}

JNIEXPORT jobject JNICALL
Java_ag_boersego_bgjs_V8Engine_runScript(JNIEnv *env, jobject obj, jlong enginePtr,
                                              jstring script) {
    BGJSV8Engine *engine = reinterpret_cast<BGJSV8Engine *>(enginePtr);

    v8::Isolate* isolate = engine->getIsolate();
    v8::Locker l(isolate);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = engine->getContext();
    v8::Context::Scope ctxScope(context);

    v8::TryCatch try_catch;

    v8::MaybeLocal<v8::Value> value =
    Script::Compile(
            JNIV8Wrapper::jstring2v8string(script),
            String::NewFromOneByte(isolate, (const uint8_t*)"binding:script")
    )->Run(context);

    if(value.IsEmpty()) {
        engine->forwardV8ExceptionToJNI(&try_catch);
        return nullptr;
    }
    return JNIV8Wrapper::v8value2jobject(value.ToLocalChecked());
}

JNIEXPORT void JNICALL
Java_ag_boersego_bgjs_V8Engine_registerModule(JNIEnv *env, jobject obj, jlong enginePtr,
											  jobject module) {
	BGJSV8Engine *engine = reinterpret_cast<BGJSV8Engine *>(enginePtr);
	engine->registerJavaModule(module);
}

JNIEXPORT jobject JNICALL
Java_ag_boersego_bgjs_V8Engine_getConstructor(JNIEnv *env, jobject obj, jlong enginePtr,
											  jstring canonicalName) {
	BGJSV8Engine *engine = reinterpret_cast<BGJSV8Engine *>(enginePtr);

	v8::Isolate* isolate = engine->getIsolate();
	v8::Locker l(isolate);
	v8::Isolate::Scope isolateScope(isolate);
	v8::HandleScope scope(isolate);
	v8::Local<v8::Context> context = engine->getContext();
	v8::Context::Scope ctxScope(context);

    std::string strCanonicalName = JNIWrapper::jstring2string(canonicalName);
    std::replace(strCanonicalName.begin(), strCanonicalName.end(), '.', '/');

	return JNIV8Wrapper::wrapObject<JNIV8Function>(JNIV8Wrapper::getJSConstructor(engine, strCanonicalName))->getJObject();
}
}