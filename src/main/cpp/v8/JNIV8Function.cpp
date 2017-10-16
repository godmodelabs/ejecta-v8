//
// Created by Martin Kleinhans on 26.09.17.
//

#include "JNIV8Function.h"

BGJS_JNIV8OBJECT_LINK(JNIV8Function, "ag/boersego/bgjs/JNIV8Function");

/**
 * internal struct for storing information for wrapped java functions
 */
struct JNIV8FunctionCallbackHolder {
    v8::Persistent<v8::Function> persistent;
    jobject jFuncRef;
    jmethodID callbackMethodId;
};

void JNIV8FunctionWeakPersistentCallback(const v8::WeakCallbackInfo<void>& data) {
    JNIEnv *env = JNIWrapper::getEnvironment();

    JNIV8FunctionCallbackHolder *holder = reinterpret_cast<JNIV8FunctionCallbackHolder*>(data.GetParameter());
    env->DeleteGlobalRef(holder->jFuncRef);

    holder->persistent.Reset();
    delete holder;
}

void JNIV8FunctionCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::HandleScope scope(args.GetIsolate());

    v8::Local<v8::External> ext;

    ext = args[0].As<v8::External>();
    JNIEnv *env = JNIWrapper::getEnvironment();

    JNIV8FunctionCallbackHolder *holder = static_cast<JNIV8FunctionCallbackHolder*>(ext->Value());

    jobject receiver = JNIV8Wrapper::v8value2jobject(args.This());
    jobjectArray arguments = nullptr;
    jobject value;

    arguments = env->NewObjectArray(args.Length()-1, env->FindClass("java/lang/Object"), nullptr);
    for(int i=1,n=args.Length(); i<n; i++) {
        value = JNIV8Wrapper::v8value2jobject(args[i]);
        env->SetObjectArrayElement(arguments, i-1, value);
    }

    jobject result = env->CallObjectMethod(holder->jFuncRef, holder->callbackMethodId, receiver, arguments);
    args.GetReturnValue().Set(JNIV8Wrapper::jobject2v8value(result));
}

void JNIV8Function::initializeJNIBindings(JNIClassInfo *info, bool isReload) {
    info->registerNativeMethod("Create", "(JLag/boersego/bgjs/JNIV8Function$Handler;)Lag/boersego/bgjs/JNIV8Function;", (void*)JNIV8Function::jniCreate);
    info->registerNativeMethod("callAsV8FunctionWithReceiver", "(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;", (void*)JNIV8Function::jniCallAsV8FunctionWithReceiver);
}

void JNIV8Function::initializeV8Bindings(V8ClassInfo *info) {

}

jobject JNIV8Function::jniCallAsV8FunctionWithReceiver(JNIEnv *env, jobject obj, jobject receiver, jobjectArray arguments) {
    auto ptr = JNIWrapper::wrapObject<JNIV8Object>(obj);
    if(!ptr) {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"),
                      "Attempt to call method on disposed object");
        return nullptr;
    }

    v8::Isolate* isolate = ptr->getEngine()->getIsolate();
    v8::Locker l(isolate);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = ptr->getEngine()->getContext();
    v8::Context::Scope ctxScope(context);

    v8::TryCatch try_catch;

    jsize numArgs;
    v8::Local<v8::Value> *args;

    numArgs = env->GetArrayLength(arguments);
    if (numArgs) {
        args = (v8::Local<v8::Value>*)malloc(sizeof(v8::Local<v8::Value>)*numArgs);
        for(jsize i=0; i<numArgs; i++) {
            args[i] = JNIV8Wrapper::jobject2v8value(env->GetObjectArrayElement(arguments, i));
        }
    } else {
        args = nullptr;
    }

    v8::MaybeLocal<v8::Value> maybeLocal;
    v8::Local<v8::Value> resultRef;
    maybeLocal = ptr->getJSObject()->CallAsFunction(context, JNIV8Wrapper::jobject2v8value(receiver), numArgs, args);
    if (!maybeLocal.ToLocal<v8::Value>(&resultRef)) {
        BGJSV8Engine::ReportException(&try_catch);
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"),
                      "V8 Exception occured during function call");
        return nullptr;
    }

    if(args) {
        free(args);
    }
    return JNIV8Wrapper::v8value2jobject(v8::Undefined(isolate));
}

v8::MaybeLocal<v8::Function> getJNIV8FunctionBaseFunction() {
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::EscapableHandleScope scope(isolate);
    v8::Local<v8::Context> context = isolate->GetCurrentContext();

    v8::MaybeLocal<v8::Function> maybeFuncRef;
    v8::MaybeLocal<v8::Value> maybeLocalRef;
    v8::Local<v8::Function> baseFuncRef, funcRef;
    v8::Local<v8::Value> localRef;

    // first we check if the function is already store in a private of the context
    auto privateKey = v8::Private::ForApi(isolate, v8::String::NewFromUtf8(isolate, "JNIV8FunctionWrapper"));
    auto privateValue = context->Global()->GetPrivate(context, privateKey);
    if (privateValue.ToLocal(&localRef) && localRef->IsFunction()) {
        return scope.Escape(localRef.As<v8::Function>());
    }

    // functions created from a function template stick around until the context is destroyed
    // only functions that are created via script are garbage collected like other objects
    // => we have to use a dynamically created "wrapper" function around a native function created via
    // function template
    maybeFuncRef = v8::FunctionTemplate::New(isolate, JNIV8FunctionCallback)->GetFunction(context);
    if (!maybeFuncRef.ToLocal(&baseFuncRef)) {
        return v8::MaybeLocal<v8::Function>();
    }

    // the usual: a function returning a function returning a function calling a function!
    // Idea: in the end we want a function that calls a native function with an External containing the java reference
    // first function: the native function is always the same, but we have to get it "into the script" somehow (x). This is what happens here
    // second function: this one introduces the External (x)
    // third function: the actual function for a specific JNIV8Function instance. Forwards all of its arguments to x (native function) prepended with y (external).
    funcRef = v8::Local<v8::Function>::Cast(
            v8::Script::Compile(
                    v8::String::NewFromOneByte(isolate, (const uint8_t*)"(function(x){return function(y){return function(...args){return x(y,...args);}}})"),
                    v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (const uint8_t*)"JNIV8Function::wrapper")
            )->Run()
    );

    maybeLocalRef = funcRef->Call(context, context->Global(), 1, (v8::Local<v8::Value>*)&baseFuncRef);
    if (!maybeLocalRef.ToLocal(&localRef) || !localRef->IsObject()) {
        return v8::MaybeLocal<v8::Function>();
    }
    funcRef = localRef.As<v8::Function>();

    // store in context globals private
    context->Global()->SetPrivate(isolate->GetCurrentContext(), privateKey, funcRef);

    return scope.Escape(funcRef);
}

jobject JNIV8Function::jniCreate(JNIEnv *env, jobject obj, jlong enginePtr, jobject handler) {
    BGJSV8Engine *engine = reinterpret_cast<BGJSV8Engine*>(enginePtr);

    v8::Isolate* isolate = engine->getIsolate();
    v8::Locker l(isolate);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = engine->getContext();
    v8::Context::Scope ctxScope(context);

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
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "Failed to retrieve JNIV8Function wrapper function");
        return nullptr;
    }

    // generate an instance specific function from the base function
    maybeLocalRef = funcRef->Call(context, context->Global(), 1, (v8::Local<v8::Value>*)&data);
    if (!maybeLocalRef.ToLocal(&localRef) || !localRef->IsObject()) {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "V8 failed to create function");
        return nullptr;
    }
    funcRef = v8::Local<v8::Function>::Cast(localRef);

    // we keep track of the function using a weak persistent; when it is gc'd we can release the java reference
    holder->persistent.Reset(isolate, funcRef);
    holder->persistent.SetWeak((void*)holder, JNIV8FunctionWeakPersistentCallback, v8::WeakCallbackType::kParameter);

    return JNIV8Wrapper::wrapObject<JNIV8Function>(funcRef)->getJObject();
}