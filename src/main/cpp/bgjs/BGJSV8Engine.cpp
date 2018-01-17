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

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include "mallocdebug.h"
#include <assert.h>
#include <sstream>

#include "BGJSGLView.h"

#define LOG_TAG	"BGJSV8Engine-jni"

using namespace v8;

BGJS_JNI_LINK(BGJSV8Engine, "ag/boersego/bgjs/V8Engine")

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

	BGJSV8Engine *ctx = BGJSV8Engine::GetInstance(args.GetIsolate());

	ctx->log(LOG_INFO, args);
}

static void TraceCallback(const v8::FunctionCallbackInfo<Value>& args) {
    if (args.Length() < 1) {
        return;
    }

    BGJSV8Engine *ctx = BGJSV8Engine::GetInstance(args.GetIsolate());

    ctx->trace(args);
}

static void DebugCallback(const v8::FunctionCallbackInfo<Value>& args) {
    args.GetReturnValue().SetUndefined();
	if (args.Length() < 1) {
		return;
	}

	BGJSV8Engine *ctx = BGJSV8Engine::GetInstance(args.GetIsolate());
	ctx->log(LOG_DEBUG, args);
}

static void InfoCallback(const v8::FunctionCallbackInfo<Value>& args) {
    args.GetReturnValue().SetUndefined();
	if (args.Length() < 1) {
		return;
	}

	BGJSV8Engine *ctx = BGJSV8Engine::GetInstance(args.GetIsolate());

	ctx->log(LOG_INFO, args);
}

static void ErrorCallback(const v8::FunctionCallbackInfo<Value>& args) {
    args.GetReturnValue().SetUndefined();
	if (args.Length() < 1) {
		return;
	}

	BGJSV8Engine *ctx = BGJSV8Engine::GetInstance(args.GetIsolate());

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

	BGJSV8Engine *engine = BGJSV8Engine::GetInstance(isolate);

	MaybeLocal<Value> result = engine->require(
            JNIV8Marshalling::v8string2string(args[0]->ToString()));
    if(!result.IsEmpty()) {
        args.GetReturnValue().Set(scope.Escape(result.ToLocalChecked()));
    }
}

//-----------------------------------------------------------
// V8Engine
//-----------------------------------------------------------

decltype(BGJSV8Engine::_jniV8Module) BGJSV8Engine::_jniV8Module = {0};
decltype(BGJSV8Engine::_jniV8Exception) BGJSV8Engine::_jniV8Exception = {0};
decltype(BGJSV8Engine::_jniV8JSException) BGJSV8Engine::_jniV8JSException = {0};
decltype(BGJSV8Engine::_jniStackTraceElement) BGJSV8Engine::_jniStackTraceElement = {0};
decltype(BGJSV8Engine::_jniV8Engine) BGJSV8Engine::_jniV8Engine = {0};

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

/**
 * returns the engine instance for the specified isolate
 */
BGJSV8Engine* BGJSV8Engine::GetInstance(v8::Isolate *isolate) {
    return reinterpret_cast<BGJSV8Engine*>(isolate->GetCurrentContext()->GetAlignedPointerFromEmbedderData(EBGJSV8EngineEmbedderData::kContext));
}

bool BGJSV8Engine::forwardJNIExceptionToV8() const {
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

#define CALLSITE_STRING(L, M, V)\
maybeValue = L->Get(context, String::NewFromOneByte(_isolate, (uint8_t *) M));\
if(maybeValue.ToLocal(&value) && value->IsFunction()) {\
maybeValue = value.As<Function>()->Call(context, L, 0, nullptr);\
if(maybeValue.ToLocal(&value) && value->IsString()) {\
V = JNIV8Marshalling::v8string2jstring(value.As<String>());\
}\
}

bool BGJSV8Engine::forwardV8ExceptionToJNI(v8::TryCatch* try_catch) const {
    if(!try_catch->HasCaught()) {
        return false;
    }

    JNIEnv *env = JNIWrapper::getEnvironment();

    HandleScope scope(_isolate);
    Local<Context> context = getContext();

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

            if(!env->IsInstanceOf(holder->throwable, _jniV8Exception.clazz)) {
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

    jobject exceptionAsObject = JNIV8Marshalling::v8value2jobject(exception);

    // convert v8 stack trace to a java stack trace
    jobject v8JSException;
    jstring exceptionMessage = nullptr;

    jobjectArray stackTrace = nullptr;
    bool error = false;
    if(exception->IsObject()) {
        // retrieve message (toString contains typename, we don't want that..)
        std::string strExceptionMessage;
        maybeValue = exception.As<Object>()->Get(context, String::NewFromOneByte(_isolate, (uint8_t *) "message"));
        if(maybeValue.ToLocal(&value) && value->IsString()) {
            strExceptionMessage = JNIV8Marshalling::v8string2string(
                    maybeValue.ToLocalChecked()->ToString());
        }

        // retrieve error name (e.g. "SyntaxError")
        std::string strErrorName;
        maybeValue = exception.As<Object>()->Get(context, String::NewFromOneByte(_isolate, (uint8_t *) "name"));
        if(maybeValue.ToLocal(&value) && value->IsString()) {
            strErrorName = JNIV8Marshalling::v8string2string(
                    maybeValue.ToLocalChecked()->ToString());
        }

        // the stack trace for syntax errors does not contain the location of the actual error
        // and neither does the message
        // so we have to append that manually
        // for errors thrown from native code it might not be available though
        if(strErrorName == "SyntaxError") {
            int lineNumber = -1;
            Maybe<int> maybeLineNumber = try_catch->Message()->GetLineNumber(context);
            if(!maybeLineNumber.IsNothing()) {
                lineNumber = maybeLineNumber.ToChecked();
            }
            Local<Value> jsScriptResourceName = try_catch->Message()->GetScriptResourceName();
            if(jsScriptResourceName->IsString()) {
                strExceptionMessage =
                        JNIV8Marshalling::v8string2string(jsScriptResourceName.As<String>()) +
                                      (lineNumber > 0 ? ":" + std::to_string(lineNumber) : "") +
                                      " - " + strExceptionMessage;
            }
        }

        exceptionMessage = JNIWrapper::string2jstring("[" + strErrorName + "] " + strExceptionMessage);

        Local<Function> getStackTraceFn = Local<Function>::New(_isolate, _getStackTraceFn);

        maybeValue = getStackTraceFn->Call(context, context->Global(), 1, &exception);
        if (maybeValue.ToLocal(&value) && value->IsArray()) {
            Local<Array> array = value.As<Array>();

            uint32_t size = array->Length();
            stackTrace = env->NewObjectArray(size, _jniStackTraceElement.clazz, nullptr);

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

                    CALLSITE_STRING(callSite, "getFileName", fileName);
                    CALLSITE_STRING(callSite, "getMethodName", methodName);
                    CALLSITE_STRING(callSite, "getFunctionName", functionName);
                    CALLSITE_STRING(callSite, "getTypeName", typeName);

                    maybeValue = callSite->Get(context, String::NewFromOneByte(_isolate, (uint8_t *) "getLineNumber"));
                    if(maybeValue.ToLocal(&value) && value->IsFunction()) {
                        maybeValue = value.As<Function>()->Call(context, callSite, 0, nullptr);
                        if(maybeValue.ToLocal(&value) && value->IsNumber()) {
                            lineNumber = (jint)value.As<Number>()->IntegerValue();
                        }
                    }

                    stackTraceElement = env->NewObject(_jniStackTraceElement.clazz, _jniStackTraceElement.initId,
                                                       typeName ? typeName : JNIWrapper::string2jstring("<unknown>"),
                                                       !methodName && !functionName ? JNIWrapper::string2jstring("<anonymous>"): methodName ? methodName : functionName,
                                                       fileName, // fileName can be zero => maps to "Unknown Source" or "Native Method" (Depending on line numer)
                                                       fileName ? (lineNumber >= 1 ? lineNumber : -1) : -2); // -1 is unknown, -2 means native
                    env->SetObjectArrayElement(stackTrace, i, stackTraceElement);
                } else {
                    error = true;
                    break;
                }
            }
        }
    }

    // if no stack trace was provided by v8, or if there was an error converting it, we still have to show something
    if(error || !stackTrace) {
        int lineNumber = -1;
        Maybe<int> maybeLineNumber = try_catch->Message()->GetLineNumber(context);
        if(!maybeLineNumber.IsNothing()) {
            lineNumber = maybeLineNumber.ToChecked();
        }

        // script resource might not be set if the exception came from native code
        jstring fileName;
        Local<Value> jsScriptResourceName = try_catch->Message()->GetScriptResourceName();
        if(jsScriptResourceName->IsString()) {
            fileName = JNIV8Marshalling::v8string2jstring(jsScriptResourceName.As<String>());
        } else {
            fileName = nullptr;
        }

        // dummy trace entry
        stackTrace = env->NewObjectArray(1, _jniStackTraceElement.clazz,
                                         env->NewObject(_jniStackTraceElement.clazz, _jniStackTraceElement.initId,
                                                        JNIWrapper::string2jstring("<unknown>"),
                                                        JNIWrapper::string2jstring("<unknown>"),
                                                        fileName,
                                                        fileName ? (lineNumber >= 1 ? lineNumber : -1) : -2));
    }

    // if exception was not an Error object, or if .message is not set for some reason => use toString()
    if(!exceptionMessage) {
        exceptionMessage = JNIV8Marshalling::v8string2jstring(exception->ToString());
    }

    // apply trace to js exception
    v8JSException = env->NewObject(_jniV8JSException.clazz, _jniV8JSException.initId, exceptionMessage, exceptionAsObject, causeException);
    env->CallVoidMethod(v8JSException, _jniV8JSException.setStackTraceId, stackTrace);

    // throw final exception
    env->Throw((jthrowable)env->NewObject(_jniV8Exception.clazz, _jniV8Exception.initId, JNIWrapper::string2jstring("An exception was thrown in JavaScript"), v8JSException));

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
	std::string moduleId = JNIV8Marshalling::v8string2string(
            maybeLocal.ToLocalChecked()->ToString());

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
	_modules[strModuleName] = (requireHook)&BGJSV8Engine::JavaModuleRequireCallback;

	return true;
}

uint8_t BGJSV8Engine::requestEmbedderDataIndex() {
    return _nextEmbedderDataIndex++;
}

//////////////////////////
// Require

Handle<Value> BGJSV8Engine::parseJSON(Handle<String> source) const {
	EscapableHandleScope scope(_isolate);
	Handle<Value> args[] = { source };

    Local<Function> jsonParseFn = Local<Function>::New(_isolate, _jsonParseFn);
	Local<Value> result = jsonParseFn->Call(getContext()->Global(), 1, args);

	return scope.Escape(result);
}

Handle<Value> BGJSV8Engine::stringifyJSON(Handle<Object> source) const {
	EscapableHandleScope scope(Isolate::GetCurrent());

	Handle<Value> args[] = { source };

    Local<Function> jsonStringifyFn = Local<Function>::New(_isolate, _jsonStringifyFn);
	Local<Value> result = jsonStringifyFn->Call(getContext()->Global(), 1, args);

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
	return scope.Escape(result);
}

v8::Local<v8::Function> BGJSV8Engine::makeRequireFunction(std::string pathName) {
    Local<Context> context = _isolate->GetCurrentContext();
    EscapableHandleScope handle_scope(_isolate);
    Local<Function> makeRequireFn, baseRequireFn;

    if(_makeRequireFn.IsEmpty()) {
        const char *szJSRequireCode =
                "(function(internalRequire, prefix) {"
                        "   return function require(path) {"
                        "       return internalRequire(path.indexOf('./')===0?'./'+prefix+'/'+path.substr(2):path);"
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

MaybeLocal<Value> BGJSV8Engine::require(std::string baseNameStr){
    Local<Context> context = _isolate->GetCurrentContext();
    EscapableHandleScope handle_scope(_isolate);

	Local<Value> result;

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
        moduleObj->Set(String::NewFromUtf8(_isolate, "environment"), String::NewFromUtf8(_isolate, "BGJSContext"));
		moduleObj->Set(String::NewFromUtf8(_isolate, "exports"), exportsObj);
        moduleObj->Set(String::NewFromUtf8(_isolate, "debug"), Boolean::New(_isolate, _debug));

        module(this, moduleObj);
        result = moduleObj->Get(String::NewFromUtf8(_isolate, "exports"));
        _moduleCache[baseNameStr].Reset(_isolate, result);
        return handle_scope.Escape(result);
    }
    std::string fileName, pathName;

    fileName = baseNameStr;
    buf = loadFile(fileName.c_str());
    if (!buf) {
        // Check if this is a directory containing index.js or package.json
        fileName = baseNameStr + "/package.json";
        buf = loadFile(fileName.c_str());

        if (!buf) {
            // It might be a directory with an index.js
            fileName = baseNameStr + "/index.js";
            buf = loadFile(fileName.c_str());

            if (!buf) {
                // So it might just be a js file
                fileName = baseNameStr + ".js";
                buf = loadFile(fileName.c_str());

                if (!buf) {
                    // No JS file, but maybe JSON?
                    fileName = baseNameStr + ".json";
                    buf = loadFile(fileName.c_str());
					
                    if (buf) {
                        isJson = true;
                    }
                }
            }
        } else {
            // Parse the package.json
            // Create a string containing the JSON source
            source = String::NewFromUtf8(_isolate, buf);
            Handle<Object> res = BGJSV8Engine::parseJSON(source)->ToObject();
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
                buf = loadFile(fileName.c_str());
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

    MaybeLocal<Value> maybeLocal;

    if (!buf) {
        _isolate->ThrowException(v8::Exception::Error(String::NewFromUtf8(_isolate, (const char*)("Cannot find module '"+baseNameStr+"'").c_str())));
        return maybeLocal;
    }

    if (isJson) {
        // Create a string containing the JSON source
        source = String::NewFromUtf8(_isolate, buf);
        Local<Value> res = BGJSV8Engine::parseJSON(source);
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
        moduleObj->Set(String::NewFromUtf8(_isolate, "debug"), Boolean::New(_isolate, _debug));

        Handle<Value> fnModuleInitializerArgs[] = {
                exportsObj,                                      // exports
                requireFn,                                       // require
                moduleObj,                                       // module
                String::NewFromUtf8(_isolate, fileName.c_str()), // __filename
                String::NewFromUtf8(_isolate, pathName.c_str())  // __dirname
        };
        Local<Function> fnModuleInitializer = Local<Function>::Cast(result);
        maybeLocal = fnModuleInitializer->Call(context, context->Global(), 5, fnModuleInitializerArgs);

        if(!maybeLocal.IsEmpty()) {
            result = moduleObj->Get(String::NewFromUtf8(_isolate, "exports"));
			_moduleCache[baseNameStr].Reset(_isolate, result);
            return handle_scope.Escape(result);
        }
    }

    // this only happens when something went wrong (e.g. exception)
    return maybeLocal;
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

bool BGJSV8Engine::runAnimationRequests(BGJSGLView* view)  {
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
                forwardV8ExceptionToJNI(&trycatch);
                return false;
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
	BGJSV8Engine *ctx = BGJSV8Engine::GetInstance(info.GetIsolate());

	if (ctx->_locale) {
		info.GetReturnValue().Set(scope.Escape(String::NewFromUtf8(Isolate::GetCurrent(), ctx->_locale)));
	} else {
	    info.GetReturnValue().SetNull();
	}
}

void BGJSV8Engine::js_global_getLang(Local<String> property,
		const v8::PropertyCallbackInfo<v8::Value>& info) {
	EscapableHandleScope scope(Isolate::GetCurrent());
	BGJSV8Engine *ctx = BGJSV8Engine::GetInstance(info.GetIsolate());

	if (ctx->_lang) {
		info.GetReturnValue().Set(scope.Escape(String::NewFromUtf8(Isolate::GetCurrent(), ctx->_lang)));
	} else {
		info.GetReturnValue().SetNull();
	}
}

void BGJSV8Engine::js_global_getTz(Local<String> property,
		const v8::PropertyCallbackInfo<v8::Value>& info) {
	EscapableHandleScope scope(Isolate::GetCurrent());
	BGJSV8Engine *ctx = BGJSV8Engine::GetInstance(info.GetIsolate());

	if (ctx->_tz) {
		info.GetReturnValue().Set(scope.Escape(String::NewFromUtf8(Isolate::GetCurrent(), ctx->_tz)));
	} else {
		info.GetReturnValue().SetNull();
	}
}

void BGJSV8Engine::js_global_getDeviceClass(Local<String> property,
                                           const v8::PropertyCallbackInfo<v8::Value>& info) {
    EscapableHandleScope scope(Isolate::GetCurrent());
    BGJSV8Engine *ctx = BGJSV8Engine::GetInstance(info.GetIsolate());

    if (ctx->_deviceClass) {
        info.GetReturnValue().Set(scope.Escape(String::NewFromUtf8(Isolate::GetCurrent(), ctx->_deviceClass)));
    } else {
        info.GetReturnValue().SetNull();
    }
}

void BGJSV8Engine::js_global_requestAnimationFrame(
		const v8::FunctionCallbackInfo<v8::Value>& args) {
    BGJSV8Engine *ctx = BGJSV8Engine::GetInstance(args.GetIsolate());
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

void BGJSV8Engine::js_process_nextTick(const v8::FunctionCallbackInfo<v8::Value>& args) {
    BGJSV8Engine *ctx = BGJSV8Engine::GetInstance(args.GetIsolate());

    if (args.Length() >= 1 && args[0]->IsFunction()) {
        ctx->enqueueNextTick(args);
    }
}

void BGJSV8Engine::js_global_cancelAnimationFrame(
		const v8::FunctionCallbackInfo<v8::Value>& args) {
    BGJSV8Engine *ctx = BGJSV8Engine::GetInstance(args.GetIsolate());
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
    BGJSV8Engine *ctx = BGJSV8Engine::GetInstance(args.GetIsolate());
	v8::Locker l(args.GetIsolate());
	HandleScope scope(args.GetIsolate());


	if (args.Length() == 2 && args[0]->IsFunction() && args[1]->IsNumber()) {
		Local<v8::Function> callback = Local<Function>::Cast(args[0]);

		WrapPersistentFunc* ws = new WrapPersistentFunc();
		BGJS_RESET_PERSISTENT(ctx->getIsolate(), ws->callbackFunc, callback);
		WrapPersistentObj* wo = new WrapPersistentObj();
		BGJS_RESET_PERSISTENT(ctx->getIsolate(), wo->obj, args.This());

		jlong timeout = (jlong)(Local<Number>::Cast(args[1])->Value());

		JNIEnv* env = JNIWrapper::getEnvironment();
		if (env == NULL) {
			LOGE("Cannot execute setTimeout with no envCache");
			args.GetReturnValue().SetUndefined();
			return;
		}

		assert(ctx->_jniV8Engine.setTimeoutId);
		assert(ctx->_jniV8Engine.clazz);
		int subId = env->CallStaticIntMethod(ctx->_jniV8Engine.clazz, ctx->_jniV8Engine.setTimeoutId, (jlong) ws,
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
    BGJSV8Engine *ctx = BGJSV8Engine::GetInstance(args.GetIsolate());
	v8::Locker l(ctx->getIsolate());
    HandleScope scope(ctx->getIsolate());

	args.GetReturnValue().SetUndefined();

	if (args.Length() == 1) {

		int id = (int) ((Local<Integer>::Cast(args[0]))->Value());

		if (id == 0) {
			return;
		}

		JNIEnv* env = JNIWrapper::getEnvironment();
		if (env == NULL) {
			LOGE("Cannot execute setTimeout with no envCache");
			return;
		}

		env->CallStaticVoidMethod(ctx->_jniV8Engine.clazz, ctx->_jniV8Engine.removeTimeoutId, (jint) id);
	} else {
        ctx->getIsolate()->ThrowException(
    				v8::Exception::ReferenceError(
    						v8::String::NewFromUtf8(ctx->getIsolate(), "Wrong arguments for clearTimeout")));
		LOGE("Wrong arguments for clearTimeout");
	}
}

/**
 * cache JNI class references
 */
void BGJSV8Engine::initJNICache() {
    JNIEnv *env = JNIWrapper::getEnvironment();

    _jniV8JSException.clazz = (jclass)env->NewGlobalRef(env->FindClass("ag/boersego/bgjs/V8JSException"));
    _jniV8JSException.initId = env->GetMethodID(_jniV8JSException.clazz, "<init>",
                                                "(Ljava/lang/String;Ljava/lang/Object;Ljava/lang/Throwable;)V");
    _jniV8JSException.setStackTraceId = env->GetMethodID(_jniV8JSException.clazz, "setStackTrace",
                                                         "([Ljava/lang/StackTraceElement;)V");

    _jniV8Exception.clazz = (jclass)env->NewGlobalRef(env->FindClass("ag/boersego/bgjs/V8Exception"));
    _jniV8Exception.initId = env->GetMethodID(_jniV8Exception.clazz, "<init>",
                                              "(Ljava/lang/String;Ljava/lang/Throwable;)V");

    _jniStackTraceElement.clazz = (jclass)env->NewGlobalRef(env->FindClass("java/lang/StackTraceElement"));
    _jniStackTraceElement.initId = env->GetMethodID(_jniStackTraceElement.clazz, "<init>",
                                                    "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;I)V");
    _jniV8Engine.clazz = (jclass)env->NewGlobalRef(env->FindClass("ag/boersego/bgjs/V8Engine"));
    _jniV8Engine.enqueueOnNextTick = env->GetMethodID(_jniV8Engine.clazz, "enqueueOnNextTick",
                                            "(Lag/boersego/bgjs/JNIV8Function;)Z");
    _jniV8Engine.setTimeoutId = env->GetStaticMethodID(_jniV8Engine.clazz, "setTimeout",
                                                       "(JJJZ)I");
    _jniV8Engine.removeTimeoutId = env->GetStaticMethodID(_jniV8Engine.clazz, "removeTimeout",
                                                          "(I)V");
}

BGJSV8Engine::BGJSV8Engine(jobject obj, JNIClassInfo *info) : JNIObject(obj, info) {
	_nextTimerId = 1;
	_locale = NULL;
    _nextEmbedderDataIndex = EBGJSV8EngineEmbedderData::FIRST_UNUSED;
    _javaAssetManager = nullptr;
    _isolate = NULL;
}

void BGJSV8Engine::initializeJNIBindings(JNIClassInfo *info, bool isReload) {

}

void BGJSV8Engine::setAssetManager(jobject jAssetManager) {
    JNIEnv *env = JNIWrapper::getEnvironment();
    _javaAssetManager = env->NewGlobalRef(jAssetManager);
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
				 v8::FunctionTemplate::New(_isolate, LogCallback, Local<Value>(), Local<Signature>(), 0, ConstructorBehavior::kThrow));
	console->Set(String::NewFromUtf8(_isolate, "debug"),
				 v8::FunctionTemplate::New(_isolate, DebugCallback, Local<Value>(), Local<Signature>(), 0, ConstructorBehavior::kThrow));
	console->Set(String::NewFromUtf8(_isolate, "info"),
				 v8::FunctionTemplate::New(_isolate, InfoCallback, Local<Value>(), Local<Signature>(), 0, ConstructorBehavior::kThrow));
	console->Set(String::NewFromUtf8(_isolate, "error"),
				 v8::FunctionTemplate::New(_isolate, ErrorCallback, Local<Value>(), Local<Signature>(), 0, ConstructorBehavior::kThrow));
	console->Set(String::NewFromUtf8(_isolate, "warn"),
				 v8::FunctionTemplate::New(_isolate, ErrorCallback, Local<Value>(), Local<Signature>(), 0, ConstructorBehavior::kThrow));
    console->Set(String::NewFromUtf8(_isolate, "trace"),
                 v8::FunctionTemplate::New(_isolate, TraceCallback, Local<Value>(), Local<Signature>(), 0, ConstructorBehavior::kThrow));
	// console->Set("assert", v8::FunctionTemplate::New(AssertCallback)); // TODO

	globalObjTpl->Set(v8::String::NewFromUtf8(_isolate, "console"), console);

    // Add methods to process function
    v8::Local<v8::FunctionTemplate> process = v8::FunctionTemplate::New(_isolate);
    process->Set(String::NewFromUtf8(_isolate, "nextTick"),
                 v8::FunctionTemplate::New(_isolate, BGJSV8Engine::js_process_nextTick, Local<Value>(), Local<Signature>(), 0, ConstructorBehavior::kThrow));
    globalObjTpl->Set(v8::String::NewFromUtf8(_isolate, "process"), process);

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
										  BGJSV8Engine::js_global_requestAnimationFrame, Local<Value>(), Local<Signature>(), 0, ConstructorBehavior::kThrow));
	globalObjTpl->Set(String::NewFromUtf8(_isolate, "cancelAnimationFrame"),
				v8::FunctionTemplate::New(_isolate,
										  BGJSV8Engine::js_global_cancelAnimationFrame, Local<Value>(), Local<Signature>(), 0, ConstructorBehavior::kThrow));
	globalObjTpl->Set(String::NewFromUtf8(_isolate, "setTimeout"),
				v8::FunctionTemplate::New(_isolate, BGJSV8Engine::js_global_setTimeout, Local<Value>(), Local<Signature>(), 0, ConstructorBehavior::kThrow));
	globalObjTpl->Set(String::NewFromUtf8(_isolate, "setInterval"),
				v8::FunctionTemplate::New(_isolate, BGJSV8Engine::js_global_setInterval, Local<Value>(), Local<Signature>(), 0, ConstructorBehavior::kThrow));
	globalObjTpl->Set(String::NewFromUtf8(_isolate, "clearTimeout"),
				v8::FunctionTemplate::New(_isolate, BGJSV8Engine::js_global_clearTimeout, Local<Value>(), Local<Signature>(), 0, ConstructorBehavior::kThrow));
	globalObjTpl->Set(String::NewFromUtf8(_isolate, "clearInterval"),
				v8::FunctionTemplate::New(_isolate, BGJSV8Engine::js_global_clearInterval, Local<Value>(), Local<Signature>(), 0, ConstructorBehavior::kThrow));

	// Create a new context.
    Local<Context> context = v8::Context::New(_isolate, NULL, globalObjTpl);
	context->SetAlignedPointerInEmbedderData(EBGJSV8EngineEmbedderData::kContext, this);

    // register global object for all required modules
    v8::Context::Scope ctxScope(context);
    context->Global()->Set(String::NewFromUtf8(_isolate, "global"), context->Global());

	_context.Reset(_isolate, context);

    //----------------------------------------
    // create bindings
    // we create as much as possible here all at once, so methods can be const
    // and we also save some checks on each execution..
    //----------------------------------------

    // init error creation binding
    {
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

    // Init stack retrieval utility function
    {
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

    // Init json parse binding
    {
        Local<Function> jsonParseMethod_ =
                Local<Function>::Cast(
                        Script::Compile(
                                String::NewFromOneByte(Isolate::GetCurrent(),
                                                       (const uint8_t*)"(function parseJSON(source) { return JSON.parse(source); })"),
                                String::NewFromOneByte(Isolate::GetCurrent(), (const uint8_t*)"binding:parseJSON"))->Run());
        _jsonParseFn.Reset(_isolate, jsonParseMethod_);
    }

    // Init json stringify binding
    {
        Local<Function> jsonStringifyMethod_ =
                Local<Function>::Cast(
                        Script::Compile(
                                String::NewFromOneByte(Isolate::GetCurrent(),
                                                       (const uint8_t*)"(function stringifyJSON(source) { return JSON.stringify(source); })"),
                                String::NewFromOneByte(Isolate::GetCurrent(), (const uint8_t*)"binding:stringifyJSON"))->Run());
        _jsonStringifyFn.Reset(_isolate, jsonStringifyMethod_);
    }
}

void BGJSV8Engine::log(int debugLevel, const v8::FunctionCallbackInfo<v8::Value>& args) {
	v8::Locker locker(args.GetIsolate());
	HandleScope scope(args.GetIsolate());

	std::stringstream str;
	int l = args.Length();
	for (int i = 0; i < l; i++) {
		str << " " << JNIV8Marshalling::v8string2string(args[i]->ToString());
	}

	LOG(debugLevel, "%s", str.str().c_str());
}

void BGJSV8Engine::setLocale(const char* locale, const char* lang,
                             const char* tz, const char* deviceClass) {
    _locale = strdup(locale);
	_lang = strdup(lang);
	_tz = strdup(tz);
    _deviceClass = strdup(deviceClass);
}

void BGJSV8Engine::setDensity(float density) {
    _density = density;
}
float BGJSV8Engine::getDensity() const {
    return _density;
}

void BGJSV8Engine::setDebug(bool debug) {
    _debug = debug;
}

char* BGJSV8Engine::loadFile(const char* path, unsigned int* length) const {
    JNIEnv* env = JNIWrapper::getEnvironment();
    AAssetManager* mgr = AAssetManager_fromJava(env, _javaAssetManager);
    AAsset* asset = AAssetManager_open(mgr, path, AASSET_MODE_UNKNOWN);

    if (!asset) {
        return nullptr;
    }

    const size_t count = (unsigned int)AAsset_getLength(asset);
    if(length) {
        *length = (unsigned int)count;
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

BGJSV8Engine::~BGJSV8Engine() {
	LOGI("Cleaning up");

    JNIEnv* env = JNIWrapper::getEnvironment();
    env->DeleteGlobalRef(_javaAssetManager);

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

void BGJSV8Engine::enqueueNextTick(const v8::FunctionCallbackInfo<v8::Value>& args) {
    HandleScope scope(args.GetIsolate());
    const auto wrappedFunction = JNIV8Wrapper::wrapObject<JNIV8Function>(args[0]->ToObject());
    JNIEnv* env = JNIWrapper::getEnvironment();
    env->CallBooleanMethod(getJObject(), _jniV8Engine.enqueueOnNextTick, wrappedFunction.get()->getJObject());
}

void BGJSV8Engine::trace(const FunctionCallbackInfo<Value> &args) {
    v8::Locker locker(args.GetIsolate());
    HandleScope scope(args.GetIsolate());

    std::stringstream str;
    str << " " << JNIV8Marshalling::v8string2string(args[0]->ToString()) << "\n";

    Local<StackTrace> stackTrace = StackTrace::CurrentStackTrace(args.GetIsolate(), 15);
    int l = stackTrace->GetFrameCount();
    for (int i = 0; i < l; i++) {
        const Local<StackFrame> &frame = stackTrace->GetFrame(i);
        str << "    " << JNIV8Marshalling::v8string2string(frame->GetScriptName()) << " (" << JNIV8Marshalling::v8string2string(frame->GetFunctionName()) << ":" << frame->GetLineNumber() << ")\n";
    }

    LOG(LOG_INFO, "%s", str.str().c_str());
}

extern "C" {

JNIEXPORT jobject JNICALL
Java_ag_boersego_bgjs_V8Engine_parseJSON(JNIEnv *env, jobject obj, jstring json) {
    auto engine = JNIWrapper::wrapObject<BGJSV8Engine>(obj);

    v8::Isolate* isolate = engine->getIsolate();
    v8::Locker l(isolate);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = engine->getContext();
    v8::Context::Scope ctxScope(context);

    v8::TryCatch try_catch;
    v8::Local<v8::Value> value = engine->parseJSON(JNIV8Marshalling::jstring2v8string(json));
    if(value.IsEmpty()) {
        engine->forwardV8ExceptionToJNI(&try_catch);
        return nullptr;
    }
    return JNIV8Marshalling::v8value2jobject(value);
}

JNIEXPORT jobject JNICALL
Java_ag_boersego_bgjs_V8Engine_require(JNIEnv *env, jobject obj, jstring file) {
    auto engine = JNIWrapper::wrapObject<BGJSV8Engine>(obj);

    v8::Isolate* isolate = engine->getIsolate();
    v8::Locker l(isolate);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = engine->getContext();
    v8::Context::Scope ctxScope(context);

    v8::TryCatch try_catch;

    v8::MaybeLocal<v8::Value> value = engine->require(JNIWrapper::jstring2string(file));

    if(value.IsEmpty()) {
        engine->forwardV8ExceptionToJNI(&try_catch);
        return nullptr;
    }
    return JNIV8Marshalling::v8value2jobject(value.ToLocalChecked());
}

JNIEXPORT jlong JNICALL
Java_ag_boersego_bgjs_V8Engine_lock(JNIEnv *env, jobject obj) {
    auto engine = JNIWrapper::wrapObject<BGJSV8Engine>(obj);

    v8::Isolate *isolate = engine->getIsolate();
    v8::Locker *locker = new Locker(isolate);

    return (jlong)locker;
}

JNIEXPORT jobject JNICALL
Java_ag_boersego_bgjs_V8Engine_getGlobalObject(JNIEnv *env, jobject obj) {
    auto engine = JNIWrapper::wrapObject<BGJSV8Engine>(obj);

    v8::Isolate* isolate = engine->getIsolate();
    v8::Locker l(isolate);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = engine->getContext();
    v8::Context::Scope ctxScope(context);

    return JNIV8Marshalling::v8value2jobject(context->Global());
}

JNIEXPORT void JNICALL
Java_ag_boersego_bgjs_V8Engine_unlock(JNIEnv *env, jobject obj, jlong lockerPtr) {
    v8::Locker *locker = reinterpret_cast<Locker *>(lockerPtr);
    delete(locker);
}

JNIEXPORT jobject JNICALL
Java_ag_boersego_bgjs_V8Engine_runScript(JNIEnv *env, jobject obj, jstring script, jstring name) {
    auto engine = JNIWrapper::wrapObject<BGJSV8Engine>(obj);

    v8::Isolate* isolate = engine->getIsolate();
    v8::Locker l(isolate);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = engine->getContext();
    v8::Context::Scope ctxScope(context);

    v8::TryCatch try_catch;

    v8::MaybeLocal<v8::Value> value =
    Script::Compile(
            JNIV8Marshalling::jstring2v8string(script),
            String::NewFromOneByte(isolate, (const uint8_t*)("script:"+JNIWrapper::jstring2string(name)).c_str())
    )->Run(context);

    if(value.IsEmpty()) {
        engine->forwardV8ExceptionToJNI(&try_catch);
        return nullptr;
    }
    return JNIV8Marshalling::v8value2jobject(value.ToLocalChecked());
}

JNIEXPORT void JNICALL
Java_ag_boersego_bgjs_V8Engine_registerModule(JNIEnv *env, jobject obj, jobject module) {
    auto engine = JNIWrapper::wrapObject<BGJSV8Engine>(obj);
    engine->registerJavaModule(module);
}

JNIEXPORT jobject JNICALL
Java_ag_boersego_bgjs_V8Engine_getConstructor(JNIEnv *env, jobject obj, jstring canonicalName) {
    auto engine = JNIWrapper::wrapObject<BGJSV8Engine>(obj);

	v8::Isolate* isolate = engine->getIsolate();
	v8::Locker l(isolate);
	v8::Isolate::Scope isolateScope(isolate);
	v8::HandleScope scope(isolate);
	v8::Local<v8::Context> context = engine->getContext();
	v8::Context::Scope ctxScope(context);

    std::string strCanonicalName = JNIWrapper::jstring2string(canonicalName);
    std::replace(strCanonicalName.begin(), strCanonicalName.end(), '.', '/');

	return JNIV8Wrapper::wrapObject<JNIV8Function>(JNIV8Wrapper::getJSConstructor(engine.get(), strCanonicalName))->getJObject();
}
}