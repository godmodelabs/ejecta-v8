//
// Created by Martin Kleinhans on 26.09.17.
//

#include "JNIV8Array.h"

BGJS_JNIV8OBJECT_LINK(JNIV8Array, "ag/boersego/bgjs/JNIV8Array");

void JNIV8Array::initializeJNIBindings(JNIClassInfo *info, bool isReload) {
    info->registerNativeMethod("NewInstance", "(J)Lag/boersego/bgjs/JNIV8Array;", (void*)JNIV8Array::NewInstance);
}

void JNIV8Array::initializeV8Bindings(V8ClassInfo *info) {

}

jobject JNIV8Array::NewInstance(JNIEnv *env, jobject obj, jlong enginePtr) {
    BGJSV8Engine *engine = reinterpret_cast<BGJSV8Engine*>(enginePtr);

    v8::Isolate* isolate = engine->getIsolate();
    v8::Locker l(isolate);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope scope(isolate);
    v8::Context::Scope ctxScope(engine->getContext());

    v8::Local<v8::Object> objRef = v8::Array::New(isolate);

    return JNIV8Wrapper::wrapObject<JNIV8Array>(objRef)->getJObject();
}