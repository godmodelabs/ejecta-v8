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
#include "modules/BGJSGLModule.h"
#include "v8-profiler.h"

#define LOG_TAG    "BGJSV8Engine-jni"

#define THROW_IF_NOT_STARTED()\
if(engine->_state != EState::kStarted) {\
jthrowable throwable = (jthrowable) env->NewObject(_jniV8Exception.clazz, _jniV8Exception.initId, JNIWrapper::string2jstring("Engine is not started"), nullptr);\
env->Throw(throwable);\
}

using namespace v8;

// We can only dump one Isolate's heap at a time
char *_nextProfileDumpPath = nullptr;

BGJS_JNI_LINK(BGJSV8Engine, "ag/boersego/bgjs/V8Engine")

jint JNI_OnLoad(JavaVM* vm, void* reserved)  {
    if(!JNIWrapper::isInitialized()) {
        JNIWrapper::init(vm);
        JNIV8Wrapper::init();

        JNIWrapper::registerObject<BGJSV8Engine>();
        JNIV8Wrapper::registerObject<BGJSGLView>();
    }

    return JNI_VERSION_1_6;
}

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

std::string normalize_path(std::string &path) {
    std::vector<std::string> pathParts;

    std::stringstream ss(path);
    std::string item;
    while (std::getline(ss, item, '/')) {
        pathParts.push_back(item);
    }

    for(size_t i = 0, n = pathParts.size(); i < n;) {
        std::string part = pathParts[i];
        // current directory "." can be ignored
        // empty directories beyond root can also be ignored
        // this also handles a trailing slash
        if(part == "." || (i && part.empty())) {
            pathParts.erase(pathParts.begin() + i);
            n--;
        } else if(part == "..") {
            auto rangeStart = pathParts.begin() + i;
            // do not go up beyond root
            // only go up if this is not the first element
            if(i > 0 && !pathParts[i-1].empty()) {
                pathParts.erase(rangeStart - 1, rangeStart + 1);
                i--;
                n -= 2;
                // in both of the above cases: part is a nop, just remove
            } else {
                pathParts.erase(rangeStart);
                n--;
            }
        } else {
            i++;
        }
    }

    std::string outPath;
    for(size_t i = 0, n = pathParts.size(); i < n; i++) {
        if(i) outPath += "/";
        outPath += pathParts[i];
    }

    return outPath;
}

std::string getPathName(std::string &path) {
    size_t found = path.find_last_of('/');

    if (found == string::npos) {
        return path;
    }
    return path.substr(0, found);
}

void find_and_replace(std::string &source, std::string const &find, std::string const &replace) {
    for (std::string::size_type i = 0; (i = source.find(find, i)) != std::string::npos;) {
        source.replace(i, find.length(), replace);
        i += replace.length();
    }
}

//-----------------------------------------------------------
// V8 function callbacks
//-----------------------------------------------------------

static void LogCallback(const v8::FunctionCallbackInfo<Value> &args) {
    if (args.Length() < 1) {
        return;
    }

    BGJSV8Engine *ctx = BGJSV8Engine::GetInstance(args.GetIsolate());

    ctx->log(LOG_INFO, args);
}

static void TraceCallback(const v8::FunctionCallbackInfo<Value> &args) {
    if (args.Length() < 1) {
        return;
    }

    BGJSV8Engine *ctx = BGJSV8Engine::GetInstance(args.GetIsolate());

    ctx->trace(args);
}

static void AssertCallback(const v8::FunctionCallbackInfo<Value> &args) {
    if (args.Length() < 1) {
        return;
    }

    BGJSV8Engine *ctx = BGJSV8Engine::GetInstance(args.GetIsolate());

    ctx->doAssert(args);
}

static void DebugCallback(const v8::FunctionCallbackInfo<Value> &args) {
    args.GetReturnValue().SetUndefined();
    if (args.Length() < 1) {
        return;
    }

    BGJSV8Engine *ctx = BGJSV8Engine::GetInstance(args.GetIsolate());
    ctx->log(LOG_DEBUG, args);
}

static void InfoCallback(const v8::FunctionCallbackInfo<Value> &args) {
    args.GetReturnValue().SetUndefined();
    if (args.Length() < 1) {
        return;
    }

    BGJSV8Engine *ctx = BGJSV8Engine::GetInstance(args.GetIsolate());

    ctx->log(LOG_INFO, args);
}

static void ErrorCallback(const v8::FunctionCallbackInfo<Value> &args) {
    args.GetReturnValue().SetUndefined();
    if (args.Length() < 1) {
        return;
    }

    BGJSV8Engine *ctx = BGJSV8Engine::GetInstance(args.GetIsolate());

    ctx->log(LOG_ERROR, args);
}

static void RequireCallback(const v8::FunctionCallbackInfo<v8::Value> &args) {
    Isolate *isolate = args.GetIsolate();

    // argument must be exactly one string
    if (args.Length() < 1 || !args[0]->IsString()) {
        args.GetReturnValue().SetUndefined();
        return;
    }

    EscapableHandleScope scope(isolate);

    BGJSV8Engine *engine = BGJSV8Engine::GetInstance(isolate);

    MaybeLocal<Value> result = engine->require(
            JNIV8Marshalling::v8string2string(args[0]->ToString(isolate->GetCurrentContext()).ToLocalChecked()));
    if (!result.IsEmpty()) {
        args.GetReturnValue().Set(scope.Escape(result.ToLocalChecked()));
    }
}

//-----------------------------------------------------------
// V8Engine
//-----------------------------------------------------------

decltype(BGJSV8Engine::_jniV8Module) BGJSV8Engine::_jniV8Module = {nullptr};
decltype(BGJSV8Engine::_jniV8Exception) BGJSV8Engine::_jniV8Exception = {nullptr};
decltype(BGJSV8Engine::_jniV8JSException) BGJSV8Engine::_jniV8JSException = {nullptr};
decltype(BGJSV8Engine::_jniStackTraceElement) BGJSV8Engine::_jniStackTraceElement = {nullptr};
decltype(BGJSV8Engine::_jniV8Engine) BGJSV8Engine::_jniV8Engine = {nullptr};

void BGJSV8Engine::RejectedPromiseHolderWeakPersistentCallback(const v8::WeakCallbackInfo<void> &data) {
    auto *holder = reinterpret_cast<RejectedPromiseHolder *>(data.GetParameter());
    holder->promise.Reset();
    holder->collected = true;
}

/**
 * internal struct for storing information for wrapped java functions
 */
struct BGJSV8EngineJavaErrorHolder {
    v8::Persistent<v8::Object> persistent;
    jthrowable throwable;

    BGJSV8EngineJavaErrorHolder() : throwable(nullptr) {}
};

void BGJSV8EngineJavaErrorHolderWeakPersistentCallback(const v8::WeakCallbackInfo<void> &data) {
    JNIEnv *env = JNIWrapper::getEnvironment();

    auto *holder = reinterpret_cast<BGJSV8EngineJavaErrorHolder *>(data.GetParameter());
    env->DeleteGlobalRef(holder->throwable);

    holder->persistent.Reset();
    delete holder;
}

/**
 * returns the engine instance for the specified isolate
 */
BGJSV8Engine *BGJSV8Engine::GetInstance(v8::Isolate *isolate) {
    return reinterpret_cast<BGJSV8Engine *>(isolate->GetCurrentContext()->GetAlignedPointerFromEmbedderData(
            EBGJSV8EngineEmbedderData::kContext));
}

bool BGJSV8Engine::forwardJNIExceptionToV8() const {
    JNIEnv *env = JNIWrapper::getEnvironment();
    jthrowable e = env->ExceptionOccurred();
    if (!e) return false;
    env->ExceptionClear();

    HandleScope scope(_isolate);
    Local<Context> context = getContext();

    /*
     * `e` could be an instance of V8Exception containing a v8 error
     * but we do NOT unwrap it and use the existing error because then we would loose some additional java stack trace entries
     * Example: Java (0) -> JS (1) -> Java (2) -> JS (3)
     * If an exception is raised on #3, then it will first be converted to a JNI exception on #2 (V8Exception containing a V8JSException as cause);
     * when it is passed to v8 again at #1 we COULD unwrap the original exception, but then we would lose the info that the code went through #2.
     *
     * however, if `e` is an instance of V8JSException we DO unwrap it.
     * These exceptions are only thrown in java with the purpose of raising a specific javascript exception
     * Example: Java (0) -> JS (1) -> Java (2)
     * The java code at #2 wants to raise a special kind of v8 exception (e.g. a RangeError, because a parameter was invalid).
     * To do that it throws a V8JSException containing the desired v8 object => We HAVE to unwrap here.
     */

    if(env->IsInstanceOf(e, _jniV8JSException.clazz)) {
        jobject v8Exception = env->CallObjectMethod(e, _jniV8JSException.getV8ExceptionId);
        _isolate->ThrowException(JNIV8Marshalling::jobject2v8value(v8Exception));
        return true;
    }

    auto *holder = new BGJSV8EngineJavaErrorHolder();
    Handle<Value> args[] = {};

    Local<Function> makeJavaErrorFn = Local<Function>::New(_isolate, _makeJavaErrorFn);
    Local<Object> result = makeJavaErrorFn->Call(context, context->Global(), 0, args).ToLocalChecked().As<Object>();

    auto privateKey = v8::Private::ForApi(_isolate, v8::String::NewFromUtf8(_isolate, "JavaErrorExternal").ToLocalChecked());
    result->SetPrivate(context, privateKey, External::New(_isolate, holder));

    holder->throwable = (jthrowable) env->NewGlobalRef(e);
    holder->persistent.Reset(_isolate, result);
    holder->persistent.SetWeak((void *) holder, BGJSV8EngineJavaErrorHolderWeakPersistentCallback,
                               v8::WeakCallbackType::kParameter);

    _isolate->ThrowException(result);

    return true;
}

#define CALLSITE_STRING(L, M, V)\
maybeValue = L->Get(context, String::NewFromOneByte(_isolate, (uint8_t *) M, NewStringType::kInternalized).ToLocalChecked());\
if(maybeValue.ToLocal(&value) && value->IsFunction()) {\
maybeValue = value.As<Function>()->Call(context, L, 0, nullptr);\
if(maybeValue.ToLocal(&value) && value->IsString()) {\
V = JNIV8Marshalling::v8string2jstring(value.As<String>());\
}\
}

bool BGJSV8Engine::forwardV8ExceptionToJNI(v8::TryCatch *try_catch, bool throwOnMainThread) const {
    return forwardV8ExceptionToJNI("", try_catch->Exception(), try_catch->Message(), throwOnMainThread);
}

bool BGJSV8Engine::forwardV8ExceptionToJNI(std::string messagePrefix, v8::Local<v8::Value> exception, v8::Local<Message> message, bool throwOnMainThread) const {
    JNIEnv *env = JNIWrapper::getEnvironment();

    HandleScope scope(_isolate);
    Local<Context> context = getContext();

    MaybeLocal<Value> maybeValue;
    Local<Value> value;

    jobject causeException = nullptr;
    // if the v8 error is a `JavaError` that means it already contains a java exception
    // => we unwrap the error and reuse that exception
    // if the original error was a v8 error we still have that contained in the java exception along with a full v8 stack trace steps
    // NOTE: if it was a java error, then we do lose the v8 stack trace however..
    // the only way to keep it would be to use yet another type of Java exception that contains both the original java exception and the first created JavaError
    Local<Object> exceptionObj;
    if (exception->IsObject()) {
        exceptionObj = exception.As<Object>();
        auto privateKey = v8::Private::ForApi(_isolate, v8::String::NewFromUtf8(_isolate,
                                                                                "JavaErrorExternal").ToLocalChecked());
        maybeValue = exceptionObj->GetPrivate(context, privateKey);
        if (maybeValue.ToLocal(&value) && value->IsExternal()) {
            BGJSV8EngineJavaErrorHolder *holder = static_cast<BGJSV8EngineJavaErrorHolder *>(value.As<External>()->Value());

            if (!env->IsInstanceOf(holder->throwable, _jniV8Exception.clazz)) {
                // if the wrapped java exception was not of type V8Exception
                // then we need to wrap it to preserve the v8 call stack
                causeException = env->NewLocalRef(holder->throwable);
            } else {
                if(throwOnMainThread) {
                    env->CallVoidMethod(getJObject(), _jniV8Engine.onThrowId, holder->throwable);
                } else {
                    // otherwise we can reuse the embedded V8Exception!
                    env->Throw((jthrowable) env->NewLocalRef(holder->throwable));
                }
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
    if (exception->IsObject()) {
        // retrieve message (toString contains typename, we don't want that..)
        std::string strExceptionMessage;
        maybeValue = exception.As<Object>()->Get(context, String::NewFromOneByte(_isolate, (uint8_t *) "message",
                                                                                 NewStringType::kInternalized).ToLocalChecked());
        if (maybeValue.ToLocal(&value) && value->IsString()) {
            strExceptionMessage = JNIV8Marshalling::v8string2string(
                    maybeValue.ToLocalChecked()->ToString(_isolate->GetCurrentContext()).ToLocalChecked());
        }

        // retrieve error name (e.g. "SyntaxError")
        std::string strErrorName;
        maybeValue = exception.As<Object>()->Get(context, String::NewFromOneByte(_isolate, (uint8_t *) "name",
                                                                                 NewStringType::kInternalized).ToLocalChecked());
        if (maybeValue.ToLocal(&value) && value->IsString()) {
            strErrorName = JNIV8Marshalling::v8string2string(
                    maybeValue.ToLocalChecked()->ToString(_isolate->GetCurrentContext()).ToLocalChecked());
        }

        // the stack trace for syntax errors does not contain the location of the actual error
        // and neither does the message
        // so we have to append that manually
        // for errors thrown from native code it might not be available though
        if (strErrorName == "SyntaxError") {
            int lineNumber = -1;
            Maybe<int> maybeLineNumber = message->GetLineNumber(context);
            if (!maybeLineNumber.IsNothing()) {
                lineNumber = maybeLineNumber.ToChecked();
            }
            Local<Value> jsScriptResourceName = message->GetScriptResourceName();
            if (jsScriptResourceName->IsString()) {
                strExceptionMessage =
                        JNIV8Marshalling::v8string2string(jsScriptResourceName.As<String>()) +
                        (lineNumber > 0 ? ":" + std::to_string(lineNumber) : "") +
                        " - " + strExceptionMessage;
            }
        }

        exceptionMessage = JNIWrapper::string2jstring(messagePrefix + "[" + strErrorName + "] " + strExceptionMessage);

        Local<Function> getStackTraceFn = Local<Function>::New(_isolate, _getStackTraceFn);

        maybeValue = getStackTraceFn->Call(context, context->Global(), 1, &exception);
        if (maybeValue.ToLocal(&value) && value->IsArray()) {
            Local<Array> array = value.As<Array>();

            uint32_t size = array->Length();
            stackTrace = env->NewObjectArray(size, _jniStackTraceElement.clazz, nullptr);

            jobject stackTraceElement;
            for (uint32_t i = 0; i < size; i++) {
                maybeValue = array->Get(context, i);
                if (maybeValue.ToLocal(&value) && value->IsObject()) {
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

                    maybeValue = callSite->Get(context, String::NewFromOneByte(_isolate, (uint8_t *) "getLineNumber",
                                                                               NewStringType::kInternalized).ToLocalChecked());
                    if (maybeValue.ToLocal(&value) && value->IsFunction()) {
                        maybeValue = value.As<Function>()->Call(context, callSite, 0, nullptr);
                        if (maybeValue.ToLocal(&value) && value->IsNumber()) {
                            lineNumber = (jint) value.As<Number>()->IntegerValue(context).FromJust();
                        }
                    }

                    stackTraceElement = env->NewObject(_jniStackTraceElement.clazz, _jniStackTraceElement.initId,
                                                       typeName ? typeName : JNIWrapper::string2jstring("<unknown>"),
                                                       !methodName && !functionName ? JNIWrapper::string2jstring(
                                                               "<anonymous>") : methodName ? methodName : functionName,
                                                       fileName, // fileName can be zero => maps to "Unknown Source" or "Native Method" (Depending on line numer)
                                                       fileName ? (lineNumber >= 1 ? lineNumber : -1)
                                                                : -2); // -1 is unknown, -2 means native
                    env->SetObjectArrayElement(stackTrace, i, stackTraceElement);
                } else {
                    error = true;
                    break;
                }
            }
        }
    }

    // if no stack trace was provided by v8, or if there was an error converting it, we still have to show something
    if (error || !stackTrace) {
        int lineNumber = -1;
        Maybe<int> maybeLineNumber = message->GetLineNumber(context);
        if (!maybeLineNumber.IsNothing()) {
            lineNumber = maybeLineNumber.ToChecked();
        }

        // script resource might not be set if the exception came from native code
        jstring fileName;
        Local<Value> jsScriptResourceName = message->GetScriptResourceName();
        if (jsScriptResourceName->IsString()) {
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
    if (!exceptionMessage) {
        exceptionMessage = JNIWrapper::string2jstring(messagePrefix + JNIV8Marshalling::v8string2string(exception->ToString(_isolate->GetCurrentContext()).ToLocalChecked()));
    }

    // apply trace to js exception
    v8JSException = env->NewObject(_jniV8JSException.clazz, _jniV8JSException.initId, exceptionMessage,
                                   exceptionAsObject, causeException);
    env->CallVoidMethod(v8JSException, _jniV8JSException.setStackTraceId, stackTrace);

    // throw final exception
    jthrowable throwable = (jthrowable) env->NewObject(_jniV8Exception.clazz, _jniV8Exception.initId,
                                                       JNIWrapper::string2jstring("An exception was thrown in JavaScript"),
                                                       v8JSException);
    if(throwOnMainThread) {
        env->CallVoidMethod(getJObject(), _jniV8Engine.onThrowId, throwable);
    } else {
        env->Throw(throwable);
    }

    return true;
}

// Register
bool BGJSV8Engine::registerModule(const char *name, requireHook requireFn) {
    _modules[name] = requireFn;
    return true;
}

void BGJSV8Engine::JavaModuleRequireCallback(BGJSV8Engine *engine, v8::Handle<v8::Object> target) {
    JNIEnv *env = JNIWrapper::getEnvironment();
    JNILocalFrame localFrame(env, 2);

    Isolate *isolate = engine->getIsolate();
    HandleScope scope(isolate);
    Local<Context> context = engine->getContext();

    MaybeLocal<Value> maybeLocal = target->Get(context, String::NewFromUtf8(isolate, "id").ToLocalChecked());
    if (maybeLocal.IsEmpty()) {
        return;
    }
    std::string moduleId = JNIV8Marshalling::v8string2string(maybeLocal.ToLocalChecked()->ToString(isolate->GetCurrentContext()).ToLocalChecked());

    jobject module = engine->_javaModules.at(moduleId);

    env->CallVoidMethod(module, _jniV8Module.requireId, engine->getJObject(),
                        JNIV8Wrapper::wrapObject<JNIV8GenericObject>(target)->getJObject());
}

bool BGJSV8Engine::registerJavaModule(jobject module) {
    JNIEnv *env = JNIWrapper::getEnvironment();
    if (!_jniV8Module.clazz) {
        _jniV8Module.clazz = (jclass) env->NewGlobalRef(env->FindClass("ag/boersego/bgjs/JNIV8Module"));
        _jniV8Module.getNameId = env->GetMethodID(_jniV8Module.clazz, "getName", "()Ljava/lang/String;");
        _jniV8Module.requireId = env->GetMethodID(_jniV8Module.clazz, "Require",
                                                  "(Lag/boersego/bgjs/V8Engine;Lag/boersego/bgjs/JNIV8GenericObject;)V");
    }

    std::string strModuleName = JNIWrapper::jstring2string(
            (jstring) env->CallObjectMethod(module, _jniV8Module.getNameId));
    _javaModules[strModuleName] = env->NewGlobalRef(module);
    _modules[strModuleName] = (requireHook) &BGJSV8Engine::JavaModuleRequireCallback;

    return true;
}

uint8_t BGJSV8Engine::requestEmbedderDataIndex() {
    return _nextEmbedderDataIndex++;
}

//////////////////////////
// Require

MaybeLocal<Value> BGJSV8Engine::parseJSON(Handle<String> source) const {
    EscapableHandleScope scope(_isolate);
    Local<Context> context = getContext();
    Handle<Value> args[] = {source};

    Local<Function> jsonParseFn = Local<Function>::New(_isolate, _jsonParseFn);
    MaybeLocal<Value> result = jsonParseFn->Call(context, context->Global(), 1, args);
    if (result.IsEmpty()) {
        return result;
    }
    return scope.Escape(result.ToLocalChecked());
}

// utility method to convert v8 values to readable strings for debugging
const std::string BGJSV8Engine::toDebugString(Handle<Value> source) const {
    Handle<Value> stringValue;
    // errors
    if(source->IsNativeError()) {
        auto messageRef = v8::Exception::CreateMessage(Isolate::GetCurrent(), source);
        stringValue = messageRef->Get();
    // other objects + primitives
    } else {
        Local<Function> debugDumpFn = Local<Function>::New(_isolate, _debugDumpFn);
        Handle<Value> args[] = {source};
        Local<Context> context = Isolate::GetCurrent()->GetCurrentContext();
        MaybeLocal<Value> result = debugDumpFn->Call(context, context->Global(), 1, args);
        if (!result.IsEmpty()) {
            stringValue = result.ToLocalChecked();
        }
    }

    if (stringValue.IsEmpty()) {
        MaybeLocal<String> maybeValue = source->ToString(Isolate::GetCurrent()->GetCurrentContext());
        if (!maybeValue.IsEmpty()) {
            stringValue = maybeValue.ToLocalChecked();
        }
    }

    return JNIV8Marshalling::v8string2string(stringValue);
}

MaybeLocal<Value> BGJSV8Engine::stringifyJSON(Handle<Object> source, bool pretty) const {
    EscapableHandleScope scope(Isolate::GetCurrent());
    Local<Context> context = getContext();

    Handle<Value> format;
    if (pretty) {
        format = Number::New(Isolate::GetCurrent(), 4);
    } else {
        format = Null(Isolate::GetCurrent());
    }
    Handle<Value> args[] = {source, format};

    Local<Function> jsonStringifyFn = Local<Function>::New(_isolate, _jsonStringifyFn);
    MaybeLocal<Value> result = jsonStringifyFn->Call(context, context->Global(), 2, args);
    if (result.IsEmpty()) {
        return result;
    }
    return scope.Escape(result.ToLocalChecked());
}

v8::Local<v8::Function> BGJSV8Engine::makeRequireFunction(std::string pathName) {
    Local<Context> context = _isolate->GetCurrentContext();
    EscapableHandleScope handle_scope(_isolate);
    Local<Function> makeRequireFn, baseRequireFn;

    if (_makeRequireFn.IsEmpty()) {
        const char *szJSRequireCode =
                "(function(internalRequire, prefix) {"
                        "   return function require(path) {"
                        "       const l = path.indexOf('./');"
                        "       return internalRequire(l>=0&&l<=1?'./'+prefix+'/'+path.substr(!l?2:0):path);"
                        "   };"
                        "})";

        ScriptOrigin origin = ScriptOrigin(String::NewFromOneByte(_isolate, (const uint8_t *) "binding:makeRequireFn",
                                                                  NewStringType::kInternalized).ToLocalChecked());
        makeRequireFn =
                Local<Function>::Cast(
                        Script::Compile(
                                context,
                                String::NewFromOneByte(_isolate, (const uint8_t *) szJSRequireCode,
                                                       NewStringType::kNormal).ToLocalChecked(),
                                &origin
                        ).ToLocalChecked()->Run(context).ToLocalChecked()
                );
        baseRequireFn = v8::FunctionTemplate::New(_isolate, RequireCallback)->GetFunction(_isolate->GetCurrentContext()).ToLocalChecked();
        _makeRequireFn.Reset(_isolate, makeRequireFn);
        _requireFn.Reset(_isolate, baseRequireFn);
    } else {
        makeRequireFn = Local<Function>::New(_isolate, _makeRequireFn);
        baseRequireFn = Local<Function>::New(_isolate, _requireFn);
    }

    Handle<Value> args[] = {baseRequireFn, String::NewFromUtf8(_isolate, pathName.c_str()).ToLocalChecked()};
    Local<Value> result = makeRequireFn->Call(context, context->Global(), 2, args).ToLocalChecked();
    return handle_scope.Escape(Local<Function>::Cast(result));
}

#define _CHECK_AND_RETURN_REQUIRE_CACHE(fileName) std::map<std::string, v8::Persistent<v8::Value>>::iterator it; \
it = _moduleCache.find(fileName); \
if(it != _moduleCache.end()) { \
    return handle_scope.Escape(Local<Value>::New(_isolate, it->second)); \
}

MaybeLocal<Value> BGJSV8Engine::require(std::string baseNameStr) {
    Local<Context> context = _isolate->GetCurrentContext();
    EscapableHandleScope handle_scope(_isolate);

    Local<Value> result;
    LOG(LOG_DEBUG, "require: %s", baseNameStr.c_str());

    // prefix "/" is basically identical to "./"
    // both reference the root of the assets folder
    if(baseNameStr.find('/') == 0) {
        baseNameStr = "./" + baseNameStr.substr(1);
    }

    // if the name starts with "./" it's a path to a js module
    // prefix "../" is handled by js require and also arrives here as "./../"
    // path needs to be normalized
    bool isRelativePath = (baseNameStr.find("./") == 0);
    if (isRelativePath) {
        baseNameStr = baseNameStr.substr(2);
        baseNameStr = normalize_path(baseNameStr);
    }
    LOG(LOG_DEBUG, "require: %s", baseNameStr.c_str());

    // check cache first
    _CHECK_AND_RETURN_REQUIRE_CACHE(baseNameStr)

    if(!isRelativePath) {
        // Check if this is an internal native module
        requireHook module = _modules[baseNameStr];

        if (module) {
            Local<Object> exportsObj = Object::New(_isolate);
            Local<Object> moduleObj = Object::New(_isolate);
            moduleObj->Set(context, String::NewFromUtf8(_isolate, "id").ToLocalChecked(),
                           String::NewFromUtf8(_isolate, baseNameStr.c_str()).ToLocalChecked());
            moduleObj->Set(context, String::NewFromUtf8(_isolate, "exports").ToLocalChecked(), exportsObj);

            module(this, moduleObj);
            result = moduleObj->Get(context, String::NewFromUtf8(_isolate, "exports").ToLocalChecked()).ToLocalChecked();
            _moduleCache[baseNameStr].Reset(_isolate, result);
            return handle_scope.Escape(result);
        } else {
            baseNameStr = _commonJSPath + baseNameStr;
        }
    }

    // Source of JS file if external code
    Handle<String> source;
    const char *buf = nullptr;
    bool isJson = false;

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
            _CHECK_AND_RETURN_REQUIRE_CACHE(fileName)
            buf = loadFile(fileName.c_str());

            if (!buf) {
                // So it might just be a js file
                fileName = baseNameStr + ".js";
                _CHECK_AND_RETURN_REQUIRE_CACHE(fileName)
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
            source = String::NewFromUtf8(_isolate, buf).ToLocalChecked();
            Handle<Value> res;
            MaybeLocal<Value> maybeRes = parseJSON(source);
            Handle<String> mainStr = String::NewFromUtf8(_isolate, (const char *) "main").ToLocalChecked();
            const bool hasMain = (maybeRes.ToLocal(&res) && res->IsObject()) ? res.As<Object>()->Has(context, mainStr).FromMaybe(false) : false;
            if (hasMain) {
                Handle<String> jsFileName = res.As<Object>()->Get(context, mainStr).ToLocalChecked()->ToString(context).ToLocalChecked();
                String::Utf8Value jsFileNameC(_isolate, jsFileName);

                fileName = baseNameStr + "/" + *jsFileNameC;
                if (buf) {
                    free((void *) buf);
                }

                // It might be a directory with an index.js
                _CHECK_AND_RETURN_REQUIRE_CACHE(fileName)
                buf = loadFile(fileName.c_str());
            } else {
                LOGE("%s doesn't have a main object: %s", baseNameStr.c_str(), buf);
                if (buf) {
                    free((void *) buf);
                    buf = nullptr;
                }
            }
        }
    } else if (baseNameStr.find(".json") == baseNameStr.length() - 5) {
        isJson = true;
    }

    MaybeLocal<Value> maybeLocal;

    if (!buf) {
        _isolate->ThrowException(v8::Exception::Error(
                String::NewFromUtf8(_isolate, ("Cannot find module '" + baseNameStr + "'").c_str()).ToLocalChecked()));
        return maybeLocal;
    }

    if (isJson) {
        // Create a string containing the JSON source
        source = String::NewFromUtf8(_isolate, buf).ToLocalChecked();
        MaybeLocal<Value> res = parseJSON(source);
        free((void *) buf);
        if (res.IsEmpty()) return res;
        return handle_scope.Escape(res.ToLocalChecked());
    }

    pathName = getPathName(fileName);

    // wrap source in anonymous function to set up an isolated scope
    const char *szSourcePrefix = "(function (exports, require, module, __filename, __dirname) {";
    const char *szSourcePostfix = "\n})";
    source = String::Concat(_isolate,
            String::Concat(_isolate,
                    String::NewFromUtf8(_isolate, szSourcePrefix).ToLocalChecked(),
                    String::NewFromUtf8(_isolate, buf).ToLocalChecked()
            ),
            String::NewFromUtf8(_isolate, szSourcePostfix).ToLocalChecked()
    );
    free((void *) buf);

    // Create script origin
    v8::ScriptOrigin* origin = new ScriptOrigin(String::NewFromOneByte(Isolate::GetCurrent(),
                                            (const uint8_t *) baseNameStr.c_str(),
                                            NewStringType::kInternalized).ToLocalChecked());
    // compile script
    MaybeLocal<Script> scriptR = Script::Compile(context, source, origin);

    // run script; this will effectively return a function if everything worked
    // if not, something went wrong
    if (!scriptR.IsEmpty()) {
        result = scriptR.ToLocalChecked()->Run(context).ToLocalChecked();
    }

    // if we received a function, run it!
    if (!result.IsEmpty() && result->IsFunction()) {
        Local<Function> requireFn = makeRequireFunction(pathName);

        Local<Object> exportsObj = Object::New(_isolate);
        Local<Object> moduleObj = Object::New(_isolate);
        moduleObj->Set(context, String::NewFromUtf8(_isolate, "id").ToLocalChecked(), String::NewFromUtf8(_isolate, fileName.c_str()).ToLocalChecked());
        moduleObj->Set(context, String::NewFromUtf8(_isolate, "exports").ToLocalChecked(), exportsObj);

        Handle<Value> fnModuleInitializerArgs[] = {
                exportsObj,                                                       // exports
                requireFn,                                                        // require
                moduleObj,                                                        // module
                String::NewFromUtf8(_isolate, fileName.c_str()).ToLocalChecked(), // __filename
                String::NewFromUtf8(_isolate, pathName.c_str()).ToLocalChecked()  // __dirname
        };
        Local<Function> fnModuleInitializer = Local<Function>::Cast(result);
        maybeLocal = fnModuleInitializer->Call(context, context->Global(), 5, fnModuleInitializerArgs);

        if (!maybeLocal.IsEmpty()) {
            result = moduleObj->Get(context, String::NewFromUtf8(_isolate, "exports").ToLocalChecked()).ToLocalChecked();
            _moduleCache[fileName].Reset(_isolate, result);

            return handle_scope.Escape(result);
        }

    }

    // this only happens when something went wrong (e.g. exception)
    return maybeLocal;
}

v8::Isolate *BGJSV8Engine::getIsolate() const {
    if (_isolate == nullptr) {
        JNIEnv *env = JNIWrapper::getEnvironment();
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"),
                      "V8Engine has not been initialized yet");
    }
    return _isolate;
}

v8::Local<v8::Context> BGJSV8Engine::getContext() const {
    EscapableHandleScope scope(_isolate);
    return scope.Escape(Local<Context>::New(_isolate, _context));
}

void BGJSV8Engine::js_process_nextTick(const v8::FunctionCallbackInfo<v8::Value> &args) {
    BGJSV8Engine *ctx = BGJSV8Engine::GetInstance(args.GetIsolate());
    if (args.Length() >= 1 && args[0]->IsFunction()) {
        TaskHolder *holder = new TaskHolder();
        holder->callback.Reset(args.GetIsolate(), args[0].As<v8::Function>());
        args.GetIsolate()->EnqueueMicrotask(&BGJSV8Engine::OnTaskMicrotask, (void*)holder);
    } else {
        ctx->getIsolate()->ThrowException(
                v8::Exception::ReferenceError(
                        v8::String::NewFromUtf8(ctx->getIsolate(), "callback must be a function").ToLocalChecked()));
    }
}

void BGJSV8Engine::js_global_setTimeout(const v8::FunctionCallbackInfo<v8::Value> &args) {
    BGJSV8Engine *ctx = BGJSV8Engine::GetInstance(args.GetIsolate());
    HandleScope scope(args.GetIsolate());

    if (args.Length() == 2 && args[0]->IsFunction()) {
        double numberValue = args[1]->NumberValue(args.GetIsolate()->GetCurrentContext()).FromMaybe(0.0f);
        if (std::isnan(numberValue)) numberValue = 0.0f;

        Local<v8::Function> funcRef = Local<Function>::Cast(args[0]);
        uint64_t id = ctx->createTimer(funcRef, (uint64_t)numberValue, 0);

        args.GetReturnValue().Set(v8::Number::New(args.GetIsolate(), (double)id));
    } else {
        ctx->getIsolate()->ThrowException(
                v8::Exception::ReferenceError(
                v8::String::NewFromUtf8(ctx->getIsolate(), "Wrong number of parameters").ToLocalChecked()));
    }
}

void BGJSV8Engine::js_global_setInterval(const v8::FunctionCallbackInfo<v8::Value> &args) {
    BGJSV8Engine *ctx = BGJSV8Engine::GetInstance(args.GetIsolate());
    HandleScope scope(args.GetIsolate());

    if (args.Length() == 2 && args[0]->IsFunction()) {
        double numberValue = args[1]->NumberValue(args.GetIsolate()->GetCurrentContext()).FromMaybe(0.0f);
        if (std::isnan(numberValue)) numberValue = 0.0f;

        Local<v8::Function> funcRef = Local<Function>::Cast(args[0]);
        uint64_t id = ctx->createTimer(funcRef, (uint64_t)numberValue, (uint64_t)numberValue);

        args.GetReturnValue().Set(v8::Number::New(args.GetIsolate(), (double)id));
    } else {
        ctx->getIsolate()->ThrowException(
                v8::Exception::ReferenceError(
                        v8::String::NewFromUtf8(ctx->getIsolate(), "Wrong number of parameters").ToLocalChecked()));
    }
}

uint64_t BGJSV8Engine::createTimer(v8::Local<v8::Function> callback, uint64_t delay, uint64_t repeat) {
    auto *holder = new TimerHolder();
    holder->callback.Reset(_isolate, callback);
    holder->engine = this;
    holder->id = _nextTimerId++;
    holder->repeats = repeat > 0;
    holder->delay = delay;
    holder->repeat = repeat;

    uv_async_send(&_uvEventScheduleTimers);

    _timers.push_back(holder);

    return holder->id;
}

/**
 * used to schedule or stop timers
 * timers are created inside of V8 from arbitrary threads, but
 * the libuv calls all need to happen inside of the libuv thread
 * so this async event is triggered by the thread creating/stopping a timer,
 * and the actual work is then done here
 * the v8 locker is simply used for synchronization, no actual v8 calls are made here
 */
void BGJSV8Engine::OnTimerEventCallback(uv_async_t * handle) {
    auto *engine = (BGJSV8Engine*)handle->data;

    v8::Isolate *isolate = engine->getIsolate();
    v8::Locker l(isolate);

    for (size_t i = 0; i < engine->_timers.size(); ++i) {
        auto holder = engine->_timers.at(i);
        if(!holder->scheduled) {
            uv_timer_init(&engine->_uvLoop, &holder->handle);
            holder->handle.data = holder;
            uv_timer_start(&holder->handle, &BGJSV8Engine::OnTimerTriggeredCallback, holder->delay, holder->repeat);
            holder->scheduled = true;
        } else if(holder->cleared && !holder->stopped) {
            holder->stopped = true;
            uv_timer_stop(&holder->handle);
            uv_close((uv_handle_t*)&holder->handle, &BGJSV8Engine::OnTimerClosedCallback);
        }
    }
}

/**
 * called by libuv when a timer was triggered
 */
void BGJSV8Engine::OnTimerTriggeredCallback(uv_timer_t * handle) {
    auto *holder = (TimerHolder*)handle->data;

    v8::Isolate *isolate = holder->engine->getIsolate();
    v8::Locker l(isolate);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = holder->engine->getContext();
    v8::Context::Scope ctxScope(context);
    v8::MicrotasksScope taskScope(isolate, v8::MicrotasksScope::kRunMicrotasks);

    v8::TryCatch try_catch(isolate);

    // timer might have been stopped while waiting for the locker
    if(holder->cleared) return;

    v8::Local<v8::Function> funcRef = v8::Local<v8::Function>::New(isolate, holder->callback);
    v8::MaybeLocal<v8::Value> maybeValueRef = funcRef->Call(context, context->Global(), 0, nullptr);

    if(!holder->repeats) {
        uv_close((uv_handle_t *) handle, &BGJSV8Engine::OnTimerClosedCallback);
    }

    if(maybeValueRef.IsEmpty()) {
        holder->engine->forwardV8ExceptionToJNI(&try_catch, true);
    }
}

/**
 * called by libuv when a timer was closed
 * resource cleanup happens here
 */
void BGJSV8Engine::OnTimerClosedCallback(uv_handle_t * handle) {
    auto *holder = (TimerHolder*)handle->data;
    BGJSV8Engine *engine = holder->engine.get();

    v8::Locker l(engine->getIsolate());

    holder->callback.Reset();
    holder->engine.reset();
    delete holder;

    auto it = std::find(engine->_timers.begin(), engine->_timers.end(), holder);
    if(it != engine->_timers.end()) {
        engine->_timers.erase(it);
    }
}

void BGJSV8Engine::js_global_clearTimeoutOrInterval(const v8::FunctionCallbackInfo<v8::Value> &args) {
    BGJSV8Engine *engine = BGJSV8Engine::GetInstance(args.GetIsolate());

    if (args.Length() == 1) {
        double numberValue = args[0]->NumberValue(args.GetIsolate()->GetCurrentContext()).FromMaybe(FP_NAN);
        if (std::isnan(numberValue)) {
            return;
        }
        auto id = (uint64_t)numberValue;
        for (size_t i = 0; i < engine->_timers.size(); ++i) {
            auto holder = engine->_timers.at(i);
            if (holder->id == id) {
                if(holder->cleared) return;
                holder->cleared = true;
                uv_async_send(&engine->_uvEventScheduleTimers);
                break;
            }
        }
    } else {
        engine->getIsolate()->ThrowException(
                v8::Exception::ReferenceError(
                        v8::String::NewFromUtf8(engine->getIsolate(), "Wrong number of parameters").ToLocalChecked()));
    }
}

/**
 * cache JNI class references
 */
void BGJSV8Engine::initJNICache() {
    JNIEnv *env = JNIWrapper::getEnvironment();

    _jniV8JSException.clazz = (jclass) env->NewGlobalRef(env->FindClass("ag/boersego/bgjs/V8JSException"));
    _jniV8JSException.getV8ExceptionId = env->GetMethodID(_jniV8JSException.clazz, "getV8Exception", "()Ljava/lang/Object;");
    _jniV8JSException.initId = env->GetMethodID(_jniV8JSException.clazz, "<init>",
                                                "(Ljava/lang/String;Ljava/lang/Object;Ljava/lang/Throwable;)V");
    _jniV8JSException.setStackTraceId = env->GetMethodID(_jniV8JSException.clazz, "setStackTrace",
                                                         "([Ljava/lang/StackTraceElement;)V");

    _jniV8Exception.clazz = (jclass) env->NewGlobalRef(env->FindClass("ag/boersego/bgjs/V8Exception"));
    _jniV8Exception.initId = env->GetMethodID(_jniV8Exception.clazz, "<init>",
                                              "(Ljava/lang/String;Ljava/lang/Throwable;)V");

    _jniStackTraceElement.clazz = (jclass) env->NewGlobalRef(env->FindClass("java/lang/StackTraceElement"));
    _jniStackTraceElement.initId = env->GetMethodID(_jniStackTraceElement.clazz, "<init>",
                                                    "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;I)V");
    _jniV8Engine.clazz = (jclass) env->NewGlobalRef(env->FindClass("ag/boersego/bgjs/V8Engine"));
    _jniV8Engine.onReadyId = env->GetMethodID(_jniV8Engine.clazz, "onReady", "()V");
    _jniV8Engine.onThrowId = env->GetMethodID(_jniV8Engine.clazz, "onThrow", "(Ljava/lang/RuntimeException;)V");
    _jniV8Engine.onSuspendId = env->GetMethodID(_jniV8Engine.clazz, "onSuspend", "()V");
    _jniV8Engine.onResumeId = env->GetMethodID(_jniV8Engine.clazz, "onResume", "()V");
}

BGJSV8Engine::BGJSV8Engine(jobject obj, JNIClassInfo *info) : JNIObject(obj, info) {
    _nextTimerId = 1;
    _nextEmbedderDataIndex = EBGJSV8EngineEmbedderData::FIRST_UNUSED;
    _javaAssetManager = nullptr;
    _isolate = nullptr;
    _isSuspended = false;
    _state = EState::kInitial;

    // create uv loop, async events, mutexes & conditions
    // these are required for synchronization and dispatching events before the engine is actually started
    uv_loop_init(&_uvLoop);
    _uvLoop.data = this;

    uv_async_init(&_uvLoop, &_uvEventStop, &BGJSV8Engine::StopLoopThread);
    _uvEventStop.data = this;

    uv_async_init(&_uvLoop, &_uvEventScheduleTimers, &BGJSV8Engine::OnTimerEventCallback);
    _uvEventScheduleTimers.data = this;

    uv_async_init(&_uvLoop, &_uvEventSuspend, &BGJSV8Engine::SuspendLoopThread);
    _uvEventSuspend.data = this;

    uv_mutex_init(&_uvMutex);
    uv_cond_init(&_uvCondSuspend);

}

const BGJSV8Engine::EState BGJSV8Engine::getState() const {
    return _state;
}

void BGJSV8Engine::initializeJNIBindings(JNIClassInfo *info, bool isReload) {
    info->registerNativeMethod("initialize", "(Landroid/content/res/AssetManager;Ljava/lang/String;I)V", (void*)BGJSV8Engine::jniInitialize);
    info->registerNativeMethod("pause", "()V", (void*)BGJSV8Engine::jniPause);
    info->registerNativeMethod("unpause", "()V", (void*)BGJSV8Engine::jniUnpause);
    info->registerNativeMethod("shutdown", "()V", (void*)BGJSV8Engine::jniShutdown);
    info->registerNativeMethod("dumpHeap", "(Ljava/lang/String;)Ljava/lang/String;", (void*)BGJSV8Engine::jniDumpHeap);
    info->registerNativeMethod("enqueueOnNextTick", "(Lag/boersego/bgjs/JNIV8Function;)V", (void*)BGJSV8Engine::jniEnqueueOnNextTick);
    info->registerNativeMethod("parseJSON", "(Ljava/lang/String;)Ljava/lang/Object;", (void*)BGJSV8Engine::jniParseJSON);
    info->registerNativeMethod("require", "(Ljava/lang/String;)Ljava/lang/Object;", (void*)BGJSV8Engine::jniRequire);
    info->registerNativeMethod("lock", "()J", (void*)BGJSV8Engine::jniLock);
    info->registerNativeMethod("unlock", "(J)V", (void*)BGJSV8Engine::jniUnlock);
    info->registerNativeMethod("getGlobalObject", "()Lag/boersego/bgjs/JNIV8GenericObject;", (void*)BGJSV8Engine::jniGetGlobalObject);
    info->registerNativeMethod("runScript", "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/Object;", (void*)BGJSV8Engine::jniRunScript);
    info->registerNativeMethod("registerModuleNative", "(Lag/boersego/bgjs/JNIV8Module;)V", (void*)BGJSV8Engine::jniRegisterModuleNative);
    info->registerNativeMethod("getConstructor", "(Ljava/lang/String;)Lag/boersego/bgjs/JNIV8Function;", (void*)BGJSV8Engine::jniGetConstructor);
}

void BGJSV8Engine::createContext() {
    static std::unique_ptr<v8::Platform> defaultPlatform;

    if (!defaultPlatform) {
        defaultPlatform = v8::platform::NewDefaultPlatform();
        LOGI("Creating default platform");
        LOGD("Created default platform %p", defaultPlatform.get());
        v8::V8::InitializePlatform(defaultPlatform.get());
        LOGD("Initialized platform");
        v8::V8::Initialize();
        std::string flags = "--expose_gc --max_old_space_size=";
        flags = flags + std::to_string(_maxHeapSize);
        v8::V8::SetFlagsFromString(flags.c_str(), (int) flags.length());
        LOGD("Initialized v8: %s", v8::V8::GetVersion());
    }

    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator =
            v8::ArrayBuffer::Allocator::NewDefaultAllocator();

    _isolate = v8::Isolate::New(create_params);
    _isolate->SetMicrotasksPolicy(v8::MicrotasksPolicy::kScoped);

    v8::Locker l(_isolate);
    Isolate::Scope isolate_scope(_isolate);
    HandleScope scope(_isolate);

    // Create global object template
    v8::Local<v8::ObjectTemplate> globalObjTpl = v8::ObjectTemplate::New(_isolate);

    // Add methods to console function
    v8::Local<v8::FunctionTemplate> console = v8::FunctionTemplate::New(_isolate);
    console->Set(String::NewFromUtf8(_isolate, "log").ToLocalChecked(),
                 v8::FunctionTemplate::New(_isolate, LogCallback, Local<Value>(), Local<Signature>(), 0,
                                           ConstructorBehavior::kThrow));
    console->Set(String::NewFromUtf8(_isolate, "debug").ToLocalChecked(),
                 v8::FunctionTemplate::New(_isolate, DebugCallback, Local<Value>(), Local<Signature>(), 0,
                                           ConstructorBehavior::kThrow));
    console->Set(String::NewFromUtf8(_isolate, "info").ToLocalChecked(),
                 v8::FunctionTemplate::New(_isolate, InfoCallback, Local<Value>(), Local<Signature>(), 0,
                                           ConstructorBehavior::kThrow));
    console->Set(String::NewFromUtf8(_isolate, "error").ToLocalChecked(),
                 v8::FunctionTemplate::New(_isolate, ErrorCallback, Local<Value>(), Local<Signature>(), 0,
                                           ConstructorBehavior::kThrow));
    console->Set(String::NewFromUtf8(_isolate, "warn").ToLocalChecked(),
                 v8::FunctionTemplate::New(_isolate, ErrorCallback, Local<Value>(), Local<Signature>(), 0,
                                           ConstructorBehavior::kThrow));
    console->Set(String::NewFromUtf8(_isolate, "assert").ToLocalChecked(),
                 v8::FunctionTemplate::New(_isolate, AssertCallback, Local<Value>(), Local<Signature>(), 0,
                                           ConstructorBehavior::kThrow));
    console->Set(String::NewFromUtf8(_isolate, "trace").ToLocalChecked(),
                 v8::FunctionTemplate::New(_isolate, TraceCallback, Local<Value>(), Local<Signature>(), 0,
                                           ConstructorBehavior::kThrow));

    globalObjTpl->Set(v8::String::NewFromUtf8(_isolate, "console").ToLocalChecked(), console);

    // Add methods to process function
    v8::Local<v8::FunctionTemplate> process = v8::FunctionTemplate::New(_isolate);
    process->Set(String::NewFromUtf8(_isolate, "nextTick").ToLocalChecked(),
                 v8::FunctionTemplate::New(_isolate, BGJSV8Engine::js_process_nextTick, Local<Value>(),
                                           Local<Signature>(), 0, ConstructorBehavior::kThrow));
    globalObjTpl->Set(v8::String::NewFromUtf8(_isolate, "process").ToLocalChecked(), process);

    // global functions
    globalObjTpl->Set(String::NewFromUtf8(_isolate, "setTimeout").ToLocalChecked(),
                      v8::FunctionTemplate::New(_isolate, BGJSV8Engine::js_global_setTimeout, Local<Value>(),
                                                Local<Signature>(), 0, ConstructorBehavior::kThrow));
    globalObjTpl->Set(String::NewFromUtf8(_isolate, "setInterval").ToLocalChecked(),
                      v8::FunctionTemplate::New(_isolate, BGJSV8Engine::js_global_setInterval, Local<Value>(),
                                                Local<Signature>(), 0, ConstructorBehavior::kThrow));
    globalObjTpl->Set(String::NewFromUtf8(_isolate, "clearTimeout").ToLocalChecked(),
                      v8::FunctionTemplate::New(_isolate, BGJSV8Engine::js_global_clearTimeoutOrInterval, Local<Value>(),
                                                Local<Signature>(), 0, ConstructorBehavior::kThrow));
    globalObjTpl->Set(String::NewFromUtf8(_isolate, "clearInterval").ToLocalChecked(),
                      v8::FunctionTemplate::New(_isolate, BGJSV8Engine::js_global_clearTimeoutOrInterval, Local<Value>(),
                                                Local<Signature>(), 0, ConstructorBehavior::kThrow));

    // Create a new context.
    Local<Context> context = v8::Context::New(_isolate, nullptr, globalObjTpl);
    context->SetAlignedPointerInEmbedderData(EBGJSV8EngineEmbedderData::kContext, this);

    // register global object for all required modules
    v8::Context::Scope ctxScope(context);
    context->Global()->Set(context, String::NewFromUtf8(_isolate, "global").ToLocalChecked(), context->Global());

    _context.Reset(_isolate, context);

    //----------------------------------------
    // create bindings
    // we create as much as possible here all at once, so methods can be const
    // and we also save some checks on each execution..
    //----------------------------------------

    // init error creation binding
    {
        ScriptOrigin makeJavaErrorOrigin = ScriptOrigin(
                String::NewFromOneByte(Isolate::GetCurrent(), (const uint8_t *) "binding:makeJavaError",
                                       NewStringType::kInternalized).ToLocalChecked());
        Local<Function> makeJavaErrorFn_ =
                Local<Function>::Cast(
                        Script::Compile(
                                context,
                                String::NewFromOneByte(Isolate::GetCurrent(), (const uint8_t *)
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
                                                "}())", NewStringType::kInternalized).ToLocalChecked(),
                                &makeJavaErrorOrigin).ToLocalChecked()->Run(context).ToLocalChecked());
        _makeJavaErrorFn.Reset(_isolate, makeJavaErrorFn_);
    }

    // Init stack retrieval utility function
    {
        Local<Function> getStackTraceFn_ =
                Local<Function>::Cast(
                        Script::Compile(
                                context,
                                String::NewFromOneByte(Isolate::GetCurrent(), (const uint8_t *)
                                        "(function(e) {"
                                                "const _ = Error.prepareStackTrace;"
                                                "Error.prepareStackTrace = (_, stack) => stack;"
                                                "const stack = e.stack;"
                                                "Error.prepareStackTrace = _;"
                                                "return stack;"
                                                "})", NewStringType::kInternalized).ToLocalChecked(),
                                new ScriptOrigin(String::NewFromOneByte(Isolate::GetCurrent(),
                                                                        (const uint8_t *) "binding:getStackTrace",
                                                                        NewStringType::kInternalized).ToLocalChecked())).ToLocalChecked()->Run(context).ToLocalChecked());
        _getStackTraceFn.Reset(_isolate, getStackTraceFn_);
    }

    // Init json parse binding
    {
        Local<Function> jsonParseMethod_ =
                Local<Function>::Cast(
                        Script::Compile(
                                context,
                                String::NewFromOneByte(Isolate::GetCurrent(),
                                                       (const uint8_t *) "(function parseJSON(source) { return JSON.parse(source); })",
                                                       NewStringType::kInternalized).ToLocalChecked(),
                                new ScriptOrigin(String::NewFromOneByte(Isolate::GetCurrent(),
                                                                        (const uint8_t *) "binding:parseJSON",
                                                                        NewStringType::kInternalized).ToLocalChecked())).ToLocalChecked()->Run(context).ToLocalChecked());
        _jsonParseFn.Reset(_isolate, jsonParseMethod_);
    }

    // Init json stringify binding
    {
        Local<Function> jsonStringifyMethod_ =
                Local<Function>::Cast(
                        Script::Compile(
                                context,
                                String::NewFromOneByte(Isolate::GetCurrent(),
                                                       (const uint8_t *) "(function stringifyJSON(source, space) { return JSON.stringify(source, null, space); })",
                                                       NewStringType::kInternalized).ToLocalChecked(),
                                new ScriptOrigin(String::NewFromOneByte(Isolate::GetCurrent(),
                                                                        (const uint8_t *) "binding:stringifyJSON",
                                                                        NewStringType::kInternalized).ToLocalChecked())).ToLocalChecked()->Run(context).ToLocalChecked());
        _jsonStringifyFn.Reset(_isolate, jsonStringifyMethod_);
    }

    // Init debug dump binding
    {
        Local<Function> debugDumpMethod =
                Local<Function>::Cast(
                        Script::Compile(
                                context,
                                String::NewFromOneByte(Isolate::GetCurrent(),
                                                       (const uint8_t *) "(function debugDump(a,b,c){b||(b=5),c||(c=0);const d=typeof a;if(!a||\"string\"==d)return\"'\"+a+\"'\";if(\"boolean\"==d||\"number\"==d)return a;if(\"symbol\"==d)"
                                                                         "return a.toString();if(\"function\"==d){const b=a.toString();return 100<b.length?b.substr(0,100)+\"\\n    ... \"+(b.length-100)+\" more chars ...\\n}\":b}if(c>b)"
                                                                         "return\"...\";let e=\"\";Object.getPrototypeOf(a)!==Object.prototype&&a.constructor.name&&(e=a.constructor.name+\" \");const f=[],g=\"  \".repeat(c+1);"
                                                                         "for(let d in a)f.push(g+d+\": \"+debugDump(a[d],b,c+1));return f.length?e+\"{\\n\"+f.join(\",\\n\")+\"\\n\"+\"  \".repeat(c)+\"}\":e+\"{}\"})",
                                                       NewStringType::kInternalized).ToLocalChecked(),
                                new ScriptOrigin(String::NewFromOneByte(Isolate::GetCurrent(),
                                                                        (const uint8_t *) "binding:debugDump",
                                                                        NewStringType::kInternalized).ToLocalChecked())).ToLocalChecked()->Run(context).ToLocalChecked());
        _debugDumpFn.Reset(_isolate, debugDumpMethod);
    }

    // Init unhandled promise rejection handler
    _isolate->SetPromiseRejectCallback(&BGJSV8Engine::PromiseRejectionHandler);
    _didScheduleURPTask = false;

    // uncaught exception handler
    // should not be necessary because trycatch is used everywhere where jni is calling into v8
    _isolate->SetCaptureStackTraceForUncaughtExceptions(true);
    _isolate->AddMessageListener(&BGJSV8Engine::UncaughtExceptionHandler);
}

void BGJSV8Engine::start(const Options* options) {
    JNI_ASSERT(_state == EState::kInitial, "BGJSV8Engine::start must only be called once after creation");
    _state = EState::kStarting;

    // apply options
    JNIEnv *env = JNIWrapper::getEnvironment();
    _javaAssetManager = env->NewGlobalRef(options->assetManager);
    _maxHeapSize = options->maxHeapSize;
    _commonJSPath = options->commonJSPath;

    // create dedicated looper thread
    uv_thread_create(&_uvThread, &BGJSV8Engine::StartLoopThread, this);
}

void BGJSV8Engine::shutdown() {
    uv_async_send(&_uvEventStop);
}

void BGJSV8Engine::pause() {
    // if engine is not started yet, the event will still be scheduled
    // once the eventloop is started, it will immediately suspend
    uv_mutex_lock(&_uvMutex);
    if(!_isSuspended) {
        _isSuspended = true;
        uv_async_send(&_uvEventSuspend);
    }
    uv_mutex_unlock(&_uvMutex);
}

void BGJSV8Engine::unpause() {
    uv_mutex_lock(&_uvMutex);
    if(_isSuspended) {
        _isSuspended = false;
        // if the eventloop is currently suspended => wake up using signal
        uv_cond_signal(&_uvCondSuspend);
    }
    uv_mutex_unlock(&_uvMutex);
}

void BGJSV8Engine::StartLoopThread(void *arg) {
    LOG(LOG_INFO, "BGJSV8Engine: EventLoop started");

    // the event loop retains the engine for as long as it is running
    JNIRetainedRef<BGJSV8Engine> engine = (BGJSV8Engine*)arg;

    LOGD("BGJSV8Engine: creating context...");

    engine->createContext();

    LOGD("BGJSV8Engine: creating context [OK]");

    LOGD("BGJSV8Engine: transitioning to ready state...");

    engine->registerModule("canvas", BGJSGLModule::doRequire);

    engine->_state = EState::kStarted;

    JNIEnv* env = JNIWrapper::getEnvironment();
    env->CallVoidMethod(engine->getJObject(), engine->_jniV8Engine.onReadyId);
    if(env->ExceptionCheck()) {
        LOGD("BGJSV8Engine: transitioning to ready state [FAILED]");
        jthrowable e = env->ExceptionOccurred();
        env->ExceptionClear();
        env->CallVoidMethod(engine->getJObject(), _jniV8Engine.onThrowId, e);
        return;
    }

    LOGD("BGJSV8Engine: transitioning to ready state [OK]");

    uv_run(&engine->_uvLoop, UV_RUN_DEFAULT);

    engine->_state = EState::kStopped;
    LOG(LOG_INFO, "BGJSV8Engine: EventLoop ended");
}

void BGJSV8Engine::StopLoopThread(uv_async_t *handle) {
    auto *engine = (BGJSV8Engine*)handle->data;
    engine->_state = EState::kStopping;
    uv_stop(&engine->_uvLoop);
}

void BGJSV8Engine::SuspendLoopThread(uv_async_t *handle) {
    auto *engine = (BGJSV8Engine*)handle->data;
    bool waiting = false;
    JNIEnv* env = JNIWrapper::getEnvironment();

    uv_mutex_lock(&engine->_uvMutex);
    while(engine->_isSuspended) {
        // due to spurious wake ups, or suspend/resume in quick succession we might end up here multiple times
        // if event loop has not been waiting before => run OnSuspend logic
        if(!waiting) {
            waiting = true;
            uv_mutex_unlock(&engine->_uvMutex);

            // because `pause`/`unpause` could be called from code executed by the OnSuspend handler
            // we have to run this WITHOUT holding the mutex
            env->CallVoidMethod(engine->getJObject(), _jniV8Engine.onSuspendId);
            if(env->ExceptionCheck()) {
                jthrowable e = env->ExceptionOccurred();
                env->ExceptionClear();
                env->CallVoidMethod(engine->getJObject(), _jniV8Engine.onThrowId, e);
                return;
            }

            uv_mutex_lock(&engine->_uvMutex);
            LOG(LOG_INFO, "BGJSV8Engine: EventLoop suspended");
            continue; // to check if still waiting, because mutex was released temporarily
        }
        uv_cond_wait(&engine->_uvCondSuspend, &engine->_uvMutex);
    }
    uv_mutex_unlock(&engine->_uvMutex);

    // if suspend/resume are triggered in quick succession this method might have been called after the engine was already resumed again
    // if the event loop was actually suspended => run OnResume logic
    if(waiting) {
        env->CallVoidMethod(engine->getJObject(), _jniV8Engine.onResumeId);
        if(env->ExceptionCheck()) {
            jthrowable e = env->ExceptionOccurred();
            env->ExceptionClear();
            env->CallVoidMethod(engine->getJObject(), _jniV8Engine.onThrowId, e);
            return;
        }
        LOG(LOG_INFO, "BGJSV8Engine: EventLoop resumed");
    }
}

void BGJSV8Engine::OnHandleClosed(uv_handle_t *handle) {
}

void BGJSV8Engine::OnTaskMicrotask(void *data) {
    TaskHolder *holder = (TaskHolder*)data;
    v8::Isolate *isolate = Isolate::GetCurrent();
    v8::Locker l(isolate);
    BGJSV8Engine* engine = BGJSV8Engine::GetInstance(isolate);
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = engine->getContext();
    v8::Context::Scope ctxScope(context);

    v8::TryCatch try_catch(isolate);

    v8::Local<v8::Function> funcRef = v8::Local<v8::Function>::New(isolate, holder->callback);
    funcRef->Call(context, context->Global(), 0, nullptr);

    if (try_catch.HasCaught()) {
        engine->forwardV8ExceptionToJNI(&try_catch, true);
    }

    delete holder;
}

void BGJSV8Engine::OnPromiseRejectionMicrotask(void *data) {
    v8::Isolate *isolate = Isolate::GetCurrent();
    v8::Locker l(isolate);
    BGJSV8Engine* engine = BGJSV8Engine::GetInstance(isolate);
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = engine->getContext();
    v8::Context::Scope ctxScope(context);

    v8::Local<v8::Symbol> symbolRef;

    // loop through queue containing promises with unhandled rejections
    for (size_t i = 0; i < engine->_unhandledRejectedPromises.size(); ++i) {
        auto holder = engine->_unhandledRejectedPromises.at(i);
        if (!holder->handled && !holder->collected) {

            // check if the promise was wrapped by a JNIV8Promise Object and returned to the JVM
            // if that happened, this microtask will run BEFORE the JVM code has the chance to register a handler
            // so we must NOT throw an exception here in that case
            // @TODO it would probably be a good idea to "postpone" the check instead of completely skipping it - but how? there seems to be no reliable way that does not rely on "magic timeouts"
            if(symbolRef.IsEmpty()) {
                symbolRef = v8::Symbol::ForApi(
                        isolate,
                        v8::String::NewFromOneByte(isolate, (const uint8_t*)"JNIV8Promise", v8::NewStringType::kInternalized).ToLocalChecked()
                );
            }

            bool wasWrapped = false;
            v8::Local<v8::Value> valueRef;
            if (v8::Local<v8::Promise>::New(isolate, holder->promise)->Get(context, symbolRef).ToLocal(&valueRef)) {
                wasWrapped = Local<v8::Boolean>::Cast(valueRef)->Value();
            };

            if (!wasWrapped) {
                auto exception = v8::Local<v8::Value>::New(isolate, holder->value);

                // exceptions detected here are thrown directly to the main thread
                // due to asynchronous nature of the promises, these exceptions are often not related to the current call stack
                // and might end up "all over the place" if we were to throw them directly on the current thread
                // also, the event looper thread could end up here during initialization or when triggering timeouts;
                // and it is NOT set up to log or otherwise handle exceptions! they would simply "disappear" if thrown there..
                LOG(LOG_ERROR, "Unhandled rejected promise: %s", engine->toDebugString(exception).c_str());
            }
        }
        holder->value.Reset();
        holder->promise.Reset();
        delete holder;
    }
    engine->_unhandledRejectedPromises.clear();
    engine->_didScheduleURPTask = false;
}

void BGJSV8Engine::PromiseRejectionHandler(v8::PromiseRejectMessage message) {
    v8::Isolate *isolate = Isolate::GetCurrent();
    BGJSV8Engine *engine = BGJSV8Engine::GetInstance(isolate);

    // See https://gist.github.com/domenic/9b40029f59f29b822f3b#promise-error-handling-hooks-rough-spec-algorithm
    // "Host environment algorithm"
    // Chromium Implementation: https://chromium.googlesource.com/chromium/blink/+/master/Source/bindings/core/v8/RejectedPromises.cpp
    if(message.GetEvent() == kPromiseRejectWithNoHandler) {
        // add promise to list
        auto holder = new RejectedPromiseHolder();
        holder->promise.Reset(isolate, message.GetPromise());
        holder->promise.SetWeak((void *) holder, RejectedPromiseHolderWeakPersistentCallback,
                                   v8::WeakCallbackType::kParameter);
        holder->value.Reset(isolate, message.GetValue());
        holder->handled = false;
        holder->collected = false;
        engine->_unhandledRejectedPromises.push_back(holder);
        // using the isolate pointer here is safe, because microtasks are executed
        // immediately when the javascript call stack depth reaches zero
        // so the isolate CAN NOT be destroyed before
        if(!engine->_didScheduleURPTask) {
            engine->_didScheduleURPTask = true;
            isolate->EnqueueMicrotask(&BGJSV8Engine::OnPromiseRejectionMicrotask, nullptr);
        }
    } else if(message.GetEvent() == kPromiseHandlerAddedAfterReject) {
        // check if promise is listed; if yes, flag as handled
        // Then look it up in the reported errors.
        for (auto holder : engine->_unhandledRejectedPromises) {
            auto promise = message.GetPromise();
            if(promise == v8::Local<v8::Promise>::New(isolate, holder->promise)) {
                holder->handled = true;
                break;
            }
        }
    }
}

void BGJSV8Engine::UncaughtExceptionHandler(v8::Local<v8::Message> message, v8::Local<v8::Value> data) {
    v8::Isolate *isolate = Isolate::GetCurrent();
    v8::Locker l(isolate);
    BGJSV8Engine* engine = BGJSV8Engine::GetInstance(isolate);
    engine->forwardV8ExceptionToJNI("Uncaught exception: ", data, message, true);
}

void BGJSV8Engine::log(int debugLevel, const v8::FunctionCallbackInfo<v8::Value> &args) {
    v8::Locker locker(args.GetIsolate());
    HandleScope scope(args.GetIsolate());

    std::stringstream str;
    int l = args.Length();
    for (int i = 0; i < l; i++) {
        str << " " << toDebugString(args[i]);
    }

    LOG(debugLevel, "%s", str.str().c_str());
}

char *BGJSV8Engine::loadFile(const char *path, unsigned int *length) const {
    JNIEnv *env = JNIWrapper::getEnvironment();
    AAssetManager *mgr = AAssetManager_fromJava(env, _javaAssetManager);
    AAsset *asset = AAssetManager_open(mgr, path, AASSET_MODE_UNKNOWN);

    if (!asset) {
        return nullptr;
    }

    const size_t count = (unsigned int) AAsset_getLength(asset);
    if (length) {
        *length = (unsigned int) count;
    }
    char *buf = (char *) malloc(count + 1), *ptr = buf;
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

    uv_loop_close(&_uvLoop);
    uv_close((uv_handle_t*)&_uvEventScheduleTimers, &BGJSV8Engine::OnHandleClosed);
    uv_close((uv_handle_t*)&_uvEventStop, &BGJSV8Engine::OnHandleClosed);

    JNIEnv *env = JNIWrapper::getEnvironment();
    env->DeleteGlobalRef(_javaAssetManager);

    // clear persistent references
    _context.Reset();
    _requireFn.Reset();
    _makeRequireFn.Reset();
    _jsonParseFn.Reset();
    _jsonStringifyFn.Reset();
    _makeJavaErrorFn.Reset();
    _getStackTraceFn.Reset();

    _isolate->Exit();

    for (auto &it : _javaModules) {
        env->DeleteGlobalRef(it.second);
    }

    JNIV8Wrapper::cleanupV8Engine(this);
}

void BGJSV8Engine::trace(const FunctionCallbackInfo<Value> &args) {
    v8::Locker locker(args.GetIsolate());
    HandleScope scope(args.GetIsolate());

    std::stringstream str;
    int l = args.Length();
    for (int i = 0; i < l; i++) {
        str << " " << toDebugString(args[i]);
    }
    str << "\n";

    Local<StackTrace> stackTrace = StackTrace::CurrentStackTrace(args.GetIsolate(), 15);
    l = stackTrace->GetFrameCount();
    for (int i = 0; i < l; i++) {
        const Local<StackFrame> &frame = stackTrace->GetFrame(args.GetIsolate(), i);
        str << "    " << JNIV8Marshalling::v8string2string(frame->GetScriptName()) << " ("
            << JNIV8Marshalling::v8string2string(frame->GetFunctionName()) << ":" << frame->GetLineNumber() << ")\n";
    }

    LOG(LOG_INFO, "%s", str.str().c_str());
}

/**
 * Check an assertion. A failed assertion is logged but does not throw an Exception. This is the same behaviour that browsers
 * have, and not how assertions behave in node.
 * @param args
 */
void BGJSV8Engine::doAssert(const FunctionCallbackInfo<Value> &args) {
    v8::Isolate *isolate = args.GetIsolate();
    if (args.Length() < 1) {
        isolate->ThrowException(
                v8::Exception::ReferenceError(
                        v8::String::NewFromUtf8(isolate, "assert: Needs at least one parameter").ToLocalChecked()));
        return;
    }

    v8::Locker locker(isolate);
    HandleScope scope(isolate);

    Local<Boolean> assertion = args[0]->ToBoolean(isolate);

    if (!assertion->Value()) {
        if (args.Length() > 1) {
            const std::string assertionMessage = toDebugString(args[1]);
            LOG(LOG_ERROR, "Assertion failed: %s", assertionMessage.c_str());
        } else {
            LOG(LOG_ERROR, "Assertion failed");
        }

        std::stringstream str;

        Local<StackTrace> stackTrace = StackTrace::CurrentStackTrace(args.GetIsolate(), 15);
        int l = stackTrace->GetFrameCount();
        for (int i = 0; i < l; i++) {
            const Local<StackFrame> &frame = stackTrace->GetFrame(isolate, i);
            str << "    " << JNIV8Marshalling::v8string2string(frame->GetScriptName()) << " ("
                << JNIV8Marshalling::v8string2string(frame->GetFunctionName()) << ":" << frame->GetLineNumber()
                << ")\n";
        }

        LOG(LOG_ERROR, "%s", str.str().c_str());

    }
}

/////////////// Heap dump functions
// These are based on https://github.com/bnoordhuis/node-heapdump

class FileOutputStream : public OutputStream {
public:
    FileOutputStream(FILE *stream) : stream_(stream) {}

    virtual int GetChunkSize() {
        return 65536;  // big chunks == faster
    }

    virtual void EndOfStream() {}

    virtual WriteResult WriteAsciiChunk(char *data, int size) {
        const size_t len = static_cast<size_t>(size);
        size_t off = 0;

        while (off < len && !feof(stream_) && !ferror(stream_))
            off += fwrite(data + off, 1, len - off, stream_);

        return off == len ? kContinue : kAbort;
    }

private:
    FILE *stream_;
};

inline bool WriteSnapshotHelper(Isolate *isolate, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (fp == nullptr) return false;

    const HeapSnapshot *const snap = isolate->GetHeapProfiler()->TakeHeapSnapshot();
    FileOutputStream stream(fp);
    snap->Serialize(&stream, HeapSnapshot::kJSON);
    fclose(fp);
    // Work around a deficiency in the API.  The HeapSnapshot object is const
    // but we cannot call HeapProfiler::DeleteAllHeapSnapshots() because that
    // invalidates _all_ snapshots, including those created by other tools.
    const_cast<HeapSnapshot *>(snap)->Delete();
    return true;
}

void BGJSV8Engine::OnGCCompletedForDump(Isolate *isolate, GCType type,
                                        GCCallbackFlags flags) {


    if (_nextProfileDumpPath == nullptr) {
        return;
    }
    const char *dumpPath = _nextProfileDumpPath;
    _nextProfileDumpPath = nullptr;
    LOGI("GC completed, now dumping to %s", dumpPath);

    WriteSnapshotHelper(isolate, dumpPath);
    free((void *) dumpPath);
    isolate->RemoveGCEpilogueCallback((const v8::Isolate::GCCallback) &(BGJSV8Engine::OnGCCompletedForDump));

    LOGI("heap dump to %s done", dumpPath);
}

const char *BGJSV8Engine::enqueueMemoryDump(const char *basePath) {
    v8::Isolate *isolate = getIsolate();
    v8::Locker l(isolate);

    if (_nextProfileDumpPath != nullptr) {
        return nullptr;
    }

    char *filename = static_cast<char *>(malloc(512));
    std::time_t result = std::time(nullptr);
    snprintf(filename, 512, "%s/heapdump-%lu.heapsnapshot", basePath, result);
    LOGI("Enqueueing heap dump to %s", filename);

    _nextProfileDumpPath = filename;

    isolate->AddGCEpilogueCallback((v8::Isolate::GCCallback) &OnGCCompletedForDump);
    isolate->RequestGarbageCollectionForTesting(Isolate::GarbageCollectionType::kFullGarbageCollection);

    return filename;
}

void BGJSV8Engine::jniInitialize(
        JNIEnv * env, jobject v8Engine, jobject assetManager, jstring commonJSPath, jint maxHeapSize) {

    auto ct = JNIV8Wrapper::wrapObject<BGJSV8Engine>(v8Engine);

    BGJSV8Engine::Options options = {0};
    options.assetManager = assetManager;
    options.commonJSPath = env->GetStringUTFChars(commonJSPath, nullptr);
    options.maxHeapSize = maxHeapSize;

    ct->start(&options);

    env->ReleaseStringUTFChars(commonJSPath, options.commonJSPath);
}


void BGJSV8Engine::jniPause(JNIEnv *env, jobject obj) {
    auto engine = JNIWrapper::wrapObject<BGJSV8Engine>(obj);
    engine->pause();
}

void BGJSV8Engine::jniUnpause(JNIEnv *env, jobject obj) {
    auto engine = JNIWrapper::wrapObject<BGJSV8Engine>(obj);
    engine->unpause();
}

void BGJSV8Engine::jniShutdown(JNIEnv *env, jobject obj) {
    auto engine = JNIWrapper::wrapObject<BGJSV8Engine>(obj);
    engine->shutdown();
}

jstring BGJSV8Engine::jniDumpHeap(JNIEnv *env, jobject obj, jstring pathToSaveIn) {
    auto engine = JNIWrapper::wrapObject<BGJSV8Engine>(obj);
    THROW_IF_NOT_STARTED();

    const char *path = env->GetStringUTFChars(pathToSaveIn, 0);
    const char *outPath = engine->enqueueMemoryDump(path);

    env->ReleaseStringUTFChars(pathToSaveIn, path);

    if (outPath != nullptr) {
        return env->NewStringUTF(outPath);
    } else {
        return nullptr;
    }
}

void BGJSV8Engine::jniEnqueueOnNextTick(JNIEnv *env, jobject obj, jobject function) {
    auto engine = JNIWrapper::wrapObject<BGJSV8Engine>(obj);
    THROW_IF_NOT_STARTED();

    v8::Isolate *isolate = engine->getIsolate();
    v8::Locker l(isolate);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = engine->getContext();
    v8::Context::Scope ctxScope(context);
    v8::MicrotasksScope taskScope(isolate, v8::MicrotasksScope::kRunMicrotasks);

    auto funcRef = JNIV8Wrapper::wrapObject<JNIV8Function>(function)->getJSObject().As<v8::Function>();

    TaskHolder *holder = new TaskHolder();
    holder->callback.Reset(isolate, funcRef);
    isolate->EnqueueMicrotask(&BGJSV8Engine::OnTaskMicrotask, (void*)holder);
}

jobject BGJSV8Engine::jniParseJSON(JNIEnv *env, jobject obj, jstring json) {
    auto engine = JNIWrapper::wrapObject<BGJSV8Engine>(obj);
    THROW_IF_NOT_STARTED();

    v8::Isolate *isolate = engine->getIsolate();
    v8::Locker l(isolate);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = engine->getContext();
    v8::Context::Scope ctxScope(context);
    v8::MicrotasksScope taskScope(isolate, v8::MicrotasksScope::kRunMicrotasks);

    v8::TryCatch try_catch(isolate);
    v8::MaybeLocal<v8::Value> value = engine->parseJSON(JNIV8Marshalling::jstring2v8string(json));
    if (&value == nullptr || value.IsEmpty()) {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "jniParseJSON failed");
        return nullptr;
    }

    return JNIV8Marshalling::v8value2jobject(value.ToLocalChecked());
}

jobject BGJSV8Engine::jniRequire(JNIEnv *env, jobject obj, jstring file) {
    auto engine = JNIWrapper::wrapObject<BGJSV8Engine>(obj);
    THROW_IF_NOT_STARTED();

    v8::Isolate *isolate = engine->getIsolate();
    v8::Locker l(isolate);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = engine->getContext();
    v8::Context::Scope ctxScope(context);
    v8::MicrotasksScope taskScope(isolate, v8::MicrotasksScope::kRunMicrotasks);

    v8::TryCatch try_catch(isolate);

    v8::MaybeLocal<v8::Value> value = engine->require(JNIWrapper::jstring2string(file));
    if (value.IsEmpty()) {
        engine->forwardV8ExceptionToJNI(&try_catch);
        return nullptr;
    }
    return JNIV8Marshalling::v8value2jobject(value.ToLocalChecked());
}

jlong BGJSV8Engine::jniLock(JNIEnv *env, jobject obj) {
    auto engine = JNIWrapper::wrapObject<BGJSV8Engine>(obj);
    THROW_IF_NOT_STARTED();

    v8::Isolate *isolate = engine->getIsolate();
    auto *locker = new Locker(isolate);

    return (jlong) locker;
}

jobject BGJSV8Engine::jniGetGlobalObject(JNIEnv *env, jobject obj) {
    auto engine = JNIWrapper::wrapObject<BGJSV8Engine>(obj);
    THROW_IF_NOT_STARTED();

    v8::Isolate *isolate = engine->getIsolate();
    v8::Locker l(isolate);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = engine->getContext();
    v8::Context::Scope ctxScope(context);
    v8::MicrotasksScope taskScope(isolate, v8::MicrotasksScope::kRunMicrotasks);

    return JNIV8Marshalling::v8value2jobject(context->Global());
}

void BGJSV8Engine::jniUnlock(JNIEnv *env, jobject obj, jlong lockerPtr) {
    auto *locker = reinterpret_cast<Locker *>(lockerPtr);
    delete (locker);
}

jobject BGJSV8Engine::jniRunScript(JNIEnv *env, jobject obj, jstring script, jstring name) {
    auto engine = JNIWrapper::wrapObject<BGJSV8Engine>(obj);
    THROW_IF_NOT_STARTED();

    v8::Isolate *isolate = engine->getIsolate();
    v8::Locker l(isolate);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = engine->getContext();
    v8::Context::Scope ctxScope(context);
    v8::MicrotasksScope taskScope(isolate, v8::MicrotasksScope::kRunMicrotasks);

    v8::TryCatch try_catch(isolate);

    ScriptOrigin origin = ScriptOrigin(
            String::NewFromOneByte(isolate, (const uint8_t *) ("script:" + JNIWrapper::jstring2string(name)).c_str(),
                                   NewStringType::kNormal).ToLocalChecked());
    v8::MaybeLocal<v8::Value> value =
            Script::Compile(
                    context,
                    JNIV8Marshalling::jstring2v8string(script),
                    &origin
            ).ToLocalChecked()->Run(context);

    if (value.IsEmpty()) {
        engine->forwardV8ExceptionToJNI(&try_catch);
        return nullptr;
    }
    return JNIV8Marshalling::v8value2jobject(value.ToLocalChecked());
}

void BGJSV8Engine::jniRegisterModuleNative(JNIEnv *env, jobject obj, jobject module) {
    auto engine = JNIWrapper::wrapObject<BGJSV8Engine>(obj);
    engine->registerJavaModule(module);
}

jobject BGJSV8Engine::jniGetConstructor(JNIEnv *env, jobject obj, jstring canonicalName) {
    auto engine = JNIWrapper::wrapObject<BGJSV8Engine>(obj);
    THROW_IF_NOT_STARTED();

    v8::Isolate *isolate = engine->getIsolate();
    v8::Locker l(isolate);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = engine->getContext();
    v8::Context::Scope ctxScope(context);

    std::string strCanonicalName = JNIWrapper::jstring2string(canonicalName);
    std::replace(strCanonicalName.begin(), strCanonicalName.end(), '.', '/');

    return JNIV8Wrapper::wrapObject<JNIV8Function>(
            JNIV8Wrapper::getJSConstructor(engine.get(), strCanonicalName))->getJObject();
}
