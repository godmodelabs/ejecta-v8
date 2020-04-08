//
// Created by Martin Kleinhans on 12.02.19.
//

#include "JNIV8Symbol.h"

#include <stdlib.h>

BGJS_JNI_LINK(JNIV8Symbol, "ag/boersego/bgjs/JNIV8Symbol");

/**
 * cache JNI class references
 */
void JNIV8Symbol::initJNICache() {
}

bool JNIV8Symbol::isWrappableV8Object(v8::Local<v8::Object> object) {
    return object->IsSymbolObject() || (object->IsProxy() && object.As<v8::Proxy>()->GetTarget()->IsSymbolObject());
}

void JNIV8Symbol::initializeJNIBindings(JNIClassInfo *info, bool isReload) {
    info->registerNativeMethod("Create", "(Lag/boersego/bgjs/V8Engine;Ljava/lang/String;)Lag/boersego/bgjs/JNIV8Symbol;", (void*)JNIV8Symbol::jniCreate);
    info->registerNativeMethod("For", "(Lag/boersego/bgjs/V8Engine;Ljava/lang/String;)Lag/boersego/bgjs/JNIV8Symbol;", (void*)JNIV8Symbol::jniFor);
    info->registerNativeMethod("ForEnumString", "(Lag/boersego/bgjs/V8Engine;Ljava/lang/String;)Lag/boersego/bgjs/JNIV8Symbol;", (void*)JNIV8Symbol::jniForEnum);
}

jobject JNIV8Symbol::jniCreate(JNIEnv *env, jobject obj, jobject engineObj, jstring description) {
    auto engine = JNIWrapper::wrapObject<BGJSV8Engine>(engineObj);

    v8::Isolate* isolate = engine->getIsolate();
    v8::Locker l(isolate);
    v8::MicrotasksScope taskScope(isolate, v8::MicrotasksScope::kRunMicrotasks);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = engine->getContext();
    v8::Context::Scope ctxScope(context);

    v8::Local<v8::Symbol> symbolRef = v8::Symbol::New(isolate, JNIV8Marshalling::jstring2v8string(description));
    v8::Local<v8::SymbolObject> symbolObjRef = v8::SymbolObject::New(isolate, symbolRef).As<v8::SymbolObject>();
    return JNIV8Wrapper::wrapObject<JNIV8Symbol>(symbolObjRef)->getJObject();
}

jobject JNIV8Symbol::jniFor(JNIEnv *env, jobject obj, jobject engineObj, jstring name) {
    auto engine = JNIWrapper::wrapObject<BGJSV8Engine>(engineObj);

    v8::Isolate* isolate = engine->getIsolate();
    v8::Locker l(isolate);
    v8::MicrotasksScope taskScope(isolate, v8::MicrotasksScope::kRunMicrotasks);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = engine->getContext();
    v8::Context::Scope ctxScope(context);

    v8::Local<v8::Symbol> symbolRef = v8::Symbol::For(isolate, JNIV8Marshalling::jstring2v8string(name));
    v8::Local<v8::SymbolObject> symbolObjRef = v8::SymbolObject::New(isolate, symbolRef).As<v8::SymbolObject>();
    return JNIV8Wrapper::wrapObject<JNIV8Symbol>(symbolObjRef)->getJObject();
}

jobject JNIV8Symbol::jniForEnum(JNIEnv *env, jobject obj, jobject engineObj, jstring symbol) {
    auto engine = JNIWrapper::wrapObject<BGJSV8Engine>(engineObj);

    v8::Isolate* isolate = engine->getIsolate();
    v8::Locker l(isolate);
    v8::MicrotasksScope taskScope(isolate, v8::MicrotasksScope::kRunMicrotasks);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = engine->getContext();
    v8::Context::Scope ctxScope(context);

    v8::Local<v8::Symbol> symbolRef;
    std::string name = JNIWrapper::jstring2string(symbol);
    if(name == "ITERATOR") {
        symbolRef = v8::Symbol::GetIterator(isolate);
    } else if(name == "ASYNC_ITERATOR") {
        symbolRef = v8::Symbol::GetAsyncIterator(isolate);
    } else if(name == "MATCH") {
        symbolRef = v8::Symbol::GetMatch(isolate);
    } else if(name == "REPLACE") {
        symbolRef = v8::Symbol::GetReplace(isolate);
    } else if(name == "SEARCH") {
        symbolRef = v8::Symbol::GetSearch(isolate);
    } else if(name == "SPLIT") {
        symbolRef = v8::Symbol::GetSplit(isolate);
    } else if(name == "HAS_INSTANCE") {
        symbolRef = v8::Symbol::GetHasInstance(isolate);
    } else if(name == "IS_CONCAT_SPREADABLE") {
        symbolRef = v8::Symbol::GetIsConcatSpreadable(isolate);
    } else if(name == "UNSCOPABLES") {
        symbolRef = v8::Symbol::GetUnscopables(isolate);
    } else if(name == "TO_PRIMITIVE") {
        symbolRef = v8::Symbol::GetToPrimitive(isolate);
    } else if(name == "TO_STRING_TAG") {
        symbolRef = v8::Symbol::GetToStringTag(isolate);
    }

    v8::Local<v8::SymbolObject> symbolObjRef = v8::SymbolObject::New(isolate, symbolRef).As<v8::SymbolObject>();
    return JNIV8Wrapper::wrapObject<JNIV8Symbol>(symbolObjRef)->getJObject();
}