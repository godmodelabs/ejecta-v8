//
// Created by Martin Kleinhans on 26.09.17.
//

#include "JNIV8Function.h"

BGJS_JNIV8OBJECT_LINK(JNIV8Function, "ag/boersego/bgjs/JNIV8Function");

void JNIV8Function::initializeJNIBindings(JNIClassInfo *info, bool isReload) {
    info->registerNativeMethod("NewInstance", "(J)Lag/boersego/bgjs/JNIV8Function;", (void*)JNIV8Function::NewInstance);
}

void JNIV8Function::initializeV8Bindings(V8ClassInfo *info) {

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