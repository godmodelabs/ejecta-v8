//
// Created by Martin Kleinhans on 24.08.17.
//

#include "JNIV8Wrapper.h"
#include "JNIV8GenericObject.h"

BGJS_JNI_LINK(JNIV8GenericObject, "ag/boersego/bgjs/JNIV8GenericObject");

static bool JNIV8GenericObject::isWrappableV8Object(v8::Local<v8::Object> object) {
    return object->IsObject();
}

void JNIV8GenericObject::initializeJNIBindings(JNIClassInfo *info, bool isReload) {
    info->registerNativeMethod("Create", "(Lag/boersego/bgjs/V8Engine;)Lag/boersego/bgjs/JNIV8GenericObject;", (void*)JNIV8GenericObject::jniCreate);
}

jobject JNIV8GenericObject::jniCreate(JNIEnv *env, jobject obj, jobject engineObj) {
    auto engine = JNIWrapper::wrapObject<BGJSV8Engine>(engineObj);

    v8::Isolate* isolate = engine->getIsolate();
    v8::Locker l(isolate);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = engine->getContext();
    v8::Context::Scope ctxScope(context);

    v8::Local<v8::Object> objRef;

    objRef = v8::Object::New(isolate);

    return JNIV8Wrapper::wrapObject<JNIV8GenericObject>(objRef)->getJObject();
}