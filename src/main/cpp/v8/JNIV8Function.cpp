//
// Created by Martin Kleinhans on 26.09.17.
//

#include "JNIV8Function.h"

BGJS_JNIV8OBJECT_LINK(JNIV8Function, "ag/boersego/bgjs/JNIV8Function");

void JNIV8Function::initializeJNIBindings(JNIClassInfo *info, bool isReload) {
    info->registerNativeMethod("NewInstance", "(J)Lag/boersego/bgjs/JNIV8Function;", (void*)JNIV8Function::NewInstance);
    info->registerNativeMethod("callAsV8Function", "([Ljava/lang/Object;)Ljava/lang/Object;", (void*)JNIV8Function::jniCallAsV8Function);
}

void JNIV8Function::initializeV8Bindings(V8ClassInfo *info) {

}

jobject JNIV8Function::jniCallAsV8Function(JNIEnv *env, jobject obj, jobjectArray arguments) {
    auto ptr = JNIWrapper::wrapObject<JNIV8Object>(obj);

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
    maybeLocal = ptr->getJSObject()->CallAsFunction(context, v8::Null(isolate), numArgs, args);
    if (!maybeLocal.ToLocal<v8::Value>(&resultRef)) {
        // @TODO: v8 exception to java exception!!
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

jobject JNIV8Function::NewInstance(JNIEnv *env, jobject obj, jlong enginePtr) {
    BGJSV8Engine *engine = reinterpret_cast<BGJSV8Engine*>(enginePtr);

    v8::Isolate* isolate = engine->getIsolate();
    v8::Locker l(isolate);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope scope(isolate);
    v8::Context::Scope ctxScope(engine->getContext());

    v8::Local<v8::Function> objRef = v8::Local<v8::Function>::Cast(
            v8::Script::Compile(
                    v8::String::NewFromOneByte(isolate, (const uint8_t*)"(function() {console.log('hello');})"),
                    v8::String::NewFromOneByte(isolate, (const uint8_t*)"binding:script")
            )->Run()
    );

    return JNIV8Wrapper::wrapObject<JNIV8Function>(objRef)->getJObject();
}