//
// Created by Martin Kleinhans on 26.09.17.
//

#include "JNIV8Function.h"

#include <stdlib.h>

BGJS_JNI_LINK(JNIV8Function, "ag/boersego/bgjs/JNIV8Function");

/**
 * internal struct for storing information for wrapped java functions
 */
struct JNIV8FunctionCallbackHolder {
    v8::Persistent<v8::Function> persistent;
    jobject jFuncRef;
    jmethodID callbackMethodId;
};

decltype(JNIV8Function::_jniObject) JNIV8Function::_jniObject = {0};

/**
 * cache JNI class references
 */
void JNIV8Function::initJNICache() {
    JNIEnv *env = JNIWrapper::getEnvironment();
    _jniObject.clazz = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Object"));
}

void JNIV8FunctionWeakPersistentCallback(const v8::WeakCallbackInfo<void>& data) {
    JNIEnv *env = JNIWrapper::getEnvironment();

    JNIV8FunctionCallbackHolder *holder = reinterpret_cast<JNIV8FunctionCallbackHolder*>(data.GetParameter());
    env->DeleteGlobalRef(holder->jFuncRef);

    holder->persistent.Reset();
    delete holder;
}

void JNIV8Function::v8FunctionCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::HandleScope scope(args.GetIsolate());

    v8::Local<v8::External> ext;

    int numArgs = args.Length();

    JNI_ASSERT(numArgs >= 1, "invalid invocation of JNIV8Function");

    ext = args[0].As<v8::External>();
    JNIEnv *env = JNIWrapper::getEnvironment();

    JNIV8FunctionCallbackHolder *holder = static_cast<JNIV8FunctionCallbackHolder*>(ext->Value());

    jobject receiver = JNIV8Marshalling::v8value2jobject(args.This());
    jobjectArray arguments = nullptr;
    jobject value;

    arguments = env->NewObjectArray(numArgs - 1, _jniObject.clazz, nullptr);
    for (int i = 1, n = numArgs; i < n; i++) {
        value = JNIV8Marshalling::v8value2jobject(args[i]);
        env->SetObjectArrayElement(arguments, i - 1, value);
        env->DeleteLocalRef(value);
    }

    jobject result = env->CallObjectMethod(holder->jFuncRef, holder->callbackMethodId, receiver, arguments);

    if(env->ExceptionCheck()) {
        BGJSV8Engine::GetInstance(args.GetIsolate())->forwardJNIExceptionToV8();
        return;
    }

    args.GetReturnValue().Set(JNIV8Marshalling::jobject2v8value(result));
}

bool JNIV8Function::isWrappableV8Object(v8::Local<v8::Object> object) {
    return object->IsFunction() || (object->IsProxy() && object.As<v8::Proxy>()->GetTarget()->IsFunction());
}

void JNIV8Function::initializeJNIBindings(JNIClassInfo *info, bool isReload) {
    info->registerNativeMethod("Create", "(Lag/boersego/bgjs/V8Engine;Lag/boersego/bgjs/JNIV8Function$Handler;)Lag/boersego/bgjs/JNIV8Function;", (void*)JNIV8Function::jniCreate);
    info->registerNativeMethod("_callAsV8Function", "(ZIILjava/lang/Class;Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;", (void*)JNIV8Function::jniCallAsV8Function);
}

jobject JNIV8Function::jniCallAsV8Function(JNIEnv *env, jobject obj, jboolean asConstructor, jint flags, jint type, jclass returnType, jobject receiver, jobjectArray arguments) {
    auto ptr = JNIWrapper::wrapObject<JNIV8Function>(obj);
    if(!ptr) {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"),
                      "Attempt to call method on disposed or invalid object");
        return nullptr;
    }

    JNIV8JavaValue arg = JNIV8Marshalling::valueWithClass(type, returnType, (JNIV8MarshallingFlags)flags);

    v8::Isolate* isolate = ptr->getEngine()->getIsolate();
    V8Locker l(isolate, __FUNCTION__);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = ptr->getEngine()->getContext();
    v8::Context::Scope ctxScope(context);
    v8::MicrotasksScope taskScope(isolate, v8::MicrotasksScope::kRunMicrotasks);

    v8::TryCatch try_catch(isolate);

    jsize numArgs;
    jobject tempObj;
    v8::Local<v8::Value> *args;

    numArgs = env->GetArrayLength(arguments);
    if (numArgs) {
        args = (v8::Local<v8::Value>*)malloc(sizeof(v8::Local<v8::Value>)*numArgs);
        for(jsize i=0; i<numArgs; i++) {
            tempObj = env->GetObjectArrayElement(arguments, i);
            args[i] = JNIV8Marshalling::jobject2v8value(tempObj);
            env->DeleteLocalRef(tempObj);
        }
    } else {
        args = nullptr;
    }

    v8::Local<v8::Value> resultRef;
    if(asConstructor) {
        v8::MaybeLocal<v8::Object> maybeLocal;
        maybeLocal = ptr->getJSObject().As<v8::Function>()->NewInstance(context, numArgs, args);
        if(args) {
            free(args);
        }
        if(env->ExceptionOccurred()) {
            return nullptr;
        }
        if (!maybeLocal.ToLocal<v8::Value>(&resultRef)) {
            ptr->getEngine()->forwardV8ExceptionToJNI(&try_catch);
            return nullptr;
        }
    } else {
        v8::MaybeLocal<v8::Value> maybeLocal;
        maybeLocal = ptr->getJSObject().As<v8::Function>()->Call(context,
                                                        JNIV8Marshalling::jobject2v8value(receiver),
                                                        numArgs, args);
        if(args) {
            free(args);
        }
        if(env->ExceptionOccurred()) {
            return nullptr;
        }
        if (!maybeLocal.ToLocal<v8::Value>(&resultRef)) {
            ptr->getEngine()->forwardV8ExceptionToJNI(&try_catch);
            return nullptr;
        }
    }

    jvalue jval;
    memset(&jval, 0, sizeof(jvalue));
    JNIV8MarshallingError res = JNIV8Marshalling::convertV8ValueToJavaValue(env, resultRef, arg, &jval);
    if(res != JNIV8MarshallingError::kOk) {
        std::string strMethodName = JNIV8Marshalling::v8string2string(ptr->getJSObject().As<v8::Function>()->GetDebugName()->ToString(isolate->GetCurrentContext()).ToLocalChecked());
        switch(res) {
            default:
            case JNIV8MarshallingError::kWrongType:
                ThrowV8TypeError("wrong type for return value of function'" + strMethodName + "'");
                break;
            case JNIV8MarshallingError::kUndefined:
                ThrowV8TypeError("return value of '" + strMethodName + "' must not be undefined");
                break;
            case JNIV8MarshallingError::kNotNullable:
                ThrowV8TypeError("return value of '" + strMethodName + "' is not nullable");
                break;
            case JNIV8MarshallingError::kNoNaN:
                ThrowV8TypeError("return value of '" + strMethodName + "' must not be NaN");
                break;
            case JNIV8MarshallingError::kVoidNotNull:
                ThrowV8TypeError("return value of '" + strMethodName + "' can only be null or undefined");
                break;
            case JNIV8MarshallingError::kOutOfRange:
                ThrowV8RangeError("return value '"+
                                  JNIV8Marshalling::v8string2string(resultRef->ToString(isolate->GetCurrentContext()).ToLocalChecked())+"' is out of range for method '" + strMethodName + "'");
                break;
        }
        return nullptr;
    }

    return jval.l;
}

v8::MaybeLocal<v8::Function> JNIV8Function::getJNIV8FunctionBaseFunction() {
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::EscapableHandleScope scope(isolate);
    v8::Local<v8::Context> context = isolate->GetCurrentContext();

    v8::MaybeLocal<v8::Function> maybeFuncRef;
    v8::MaybeLocal<v8::Value> maybeLocalRef, maybeLocalScriptRef;
    v8::Local<v8::Function> baseFuncRef, funcRef;
    v8::Local<v8::Value> localRef;

    // first we check if the function is already store in a private of the context
    auto privateKey = v8::Private::ForApi(isolate, v8::String::NewFromUtf8(isolate, "JNIV8FunctionWrapper").ToLocalChecked());
    auto privateValue = context->Global()->GetPrivate(context, privateKey);
    if (privateValue.ToLocal(&localRef) && localRef->IsFunction()) {
        return scope.Escape(localRef.As<v8::Function>());
    }

    // functions created from a function template stick around until the context is destroyed
    // only functions that are created via script are garbage collected like other objects
    // => we have to use a dynamically created "wrapper" function around a native function created via
    // function template
    maybeFuncRef = v8::FunctionTemplate::New(isolate, v8FunctionCallback)->GetFunction(context);
    if (!maybeFuncRef.ToLocal(&baseFuncRef)) {
        return v8::MaybeLocal<v8::Function>();
    }

    // the usual: a function returning a function returning a function calling a function!
    // Idea: in the end we want a function that calls a native function with an External containing the java reference
    // first function: the native function is always the same, but we have to get it "into the script" somehow (x). This is what happens here
    // second function: this one introduces the External (y)
    // third function: the actual function for a specific JNIV8Function instance. Forwards all of its arguments to x (native function) prepended with y (external).
    v8::ScriptOrigin origin = v8::ScriptOrigin(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (const uint8_t*)"binding:JNIV8Function", v8::NewStringType::kInternalized).ToLocalChecked());
    maybeLocalScriptRef =
            v8::Script::Compile(
                    context,
                    v8::String::NewFromOneByte(isolate, (const uint8_t*)"(function(x){return function(y){return function(...args){return x(y,...args);}}})", v8::NewStringType::kInternalized).ToLocalChecked(),
                    &origin)
            .ToLocalChecked()->Run(context);

    if (maybeLocalScriptRef.IsEmpty()) {
        return v8::MaybeLocal<v8::Function>();
    }

    funcRef = maybeLocalScriptRef.ToLocalChecked().As<v8::Function>();

    maybeLocalRef = funcRef->Call(context, context->Global(), 1, (v8::Local<v8::Value>*)&baseFuncRef);
    if (!maybeLocalRef.ToLocal(&localRef) || !localRef->IsFunction()) {
        return v8::MaybeLocal<v8::Function>();
    }
    funcRef = localRef.As<v8::Function>();

    // store in context globals private
    context->Global()->SetPrivate(isolate->GetCurrentContext(), privateKey, funcRef);

    return scope.Escape(funcRef);
}

jobject JNIV8Function::jniCreate(JNIEnv *env, jobject obj, jobject engineObj, jobject handler) {
    auto engine = JNIWrapper::wrapObject<BGJSV8Engine>(engineObj);

    v8::Isolate* isolate = engine->getIsolate();
    V8Locker l(isolate, __FUNCTION__);
    v8::MicrotasksScope taskScope(isolate, v8::MicrotasksScope::kRunMicrotasks);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = engine->getContext();
    v8::Context::Scope ctxScope(context);

    v8::Local<v8::Function> funcRef;
    auto maybeFuncRef = createJavaBackedFunction(engine, handler);
    if (!maybeFuncRef.ToLocal(&funcRef)) {
        // exception will already have been thrown at this point
        return nullptr;
    }

    return JNIV8Wrapper::wrapObject<JNIV8Function>(funcRef)->getJObject();
}

v8::MaybeLocal<v8::Function> JNIV8Function::createJavaBackedFunction(JNILocalRef<BGJSV8Engine> engine, jobject handler) {
    JNIEnv *env = JNIWrapper::getEnvironment();

    v8::Isolate* isolate = engine->getIsolate();
    v8::Local<v8::Context> context = engine->getContext();
    v8::EscapableHandleScope scope(isolate);

    v8::MaybeLocal<v8::Value> maybeLocalRef;
    v8::Local<v8::Value> localRef;
    v8::Local<v8::Function> funcRef;
    v8::MaybeLocal<v8::Function> maybeFuncRef;

    // java reference is stored in the functions data parameter to be retrieved when called
    JNIV8FunctionCallbackHolder *holder = new JNIV8FunctionCallbackHolder();
    holder->jFuncRef = env->NewGlobalRef(handler);
    holder->callbackMethodId = env->GetMethodID(env->GetObjectClass(handler), "Callback", "(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;");

    v8::Local<v8::External> data = v8::External::New(isolate, (void*)holder);

    maybeFuncRef = getJNIV8FunctionBaseFunction();
    if (!maybeFuncRef.ToLocal(&funcRef)) {
        delete holder;
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "Failed to retrieve JNIV8Function wrapper function");
        return v8::MaybeLocal<v8::Function>();
    }

    // generate an instance specific function from the base function
    maybeLocalRef = funcRef->Call(context, context->Global(), 1, (v8::Local<v8::Value>*)&data);
    if (!maybeLocalRef.ToLocal(&localRef) || !localRef->IsFunction()) {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "V8 failed to create function");
        return v8::MaybeLocal<v8::Function>();
    }
    funcRef = v8::Local<v8::Function>::Cast(localRef);

    // we keep track of the function using a weak persistent; when it is gc'd we can release the java reference
    holder->persistent.Reset(isolate, funcRef);
    holder->persistent.SetWeak((void*)holder, JNIV8FunctionWeakPersistentCallback, v8::WeakCallbackType::kParameter);

    return scope.Escape(funcRef);
}