//
// Created by Martin Kleinhans on 08.02.19.
//

#include "JNIV8Promise.h"

#include <stdlib.h>

BGJS_JNI_LINK(JNIV8Promise, "ag/boersego/bgjs/JNIV8Promise");
BGJS_JNI_LINK(JNIV8PromiseResolver, "ag/boersego/bgjs/JNIV8Promise$Resolver");

/**
 * cache JNI class references
 */
void JNIV8Promise::initJNICache() {
}

bool JNIV8Promise::isWrappableV8Object(v8::Local<v8::Object> object) {
    return object->IsPromise();
}

void JNIV8Promise::initializeJNIBindings(JNIClassInfo *info, bool isReload) {
    info->registerNativeMethod("CreateResolver", "(Lag/boersego/bgjs/V8Engine;)Lag/boersego/bgjs/JNIV8Promise$Resolver;", (void*)JNIV8Promise::jniCreateResolver);
}

jobject JNIV8Promise::jniCreateResolver(JNIEnv *env, jobject obj, jobject engineObj) {
    auto engine = JNIWrapper::wrapObject<BGJSV8Engine>(engineObj);

    v8::Isolate* isolate = engine->getIsolate();
    v8::Locker l(isolate);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = engine->getContext();
    v8::Context::Scope ctxScope(context);

    v8::Local<v8::Promise::Resolver> resolverRef;
    auto maybeResolver = v8::Promise::Resolver::New(context);
    if (!maybeResolver.ToLocal(&resolverRef)) {
        return nullptr;
    }
    return JNIV8Wrapper::wrapObject<JNIV8PromiseResolver>(resolverRef)->getJObject();
}

/**
 * cache JNI class references
 */
void JNIV8PromiseResolver::initJNICache() {
}

bool JNIV8PromiseResolver::isWrappableV8Object(v8::Local<v8::Object> object) {
    // @TODO: how to identify if object is of type v8::Promise::Resolver ??!
    /*v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    return object->InstanceOf(context, v8::Promise::Resolver::???);
     */
    return true;
}

void JNIV8PromiseResolver::initializeJNIBindings(JNIClassInfo *info, bool isReload) {
    info->registerNativeMethod("getPromise", "()Lag/boersego/bgjs/JNIV8Promise;", (void*)JNIV8PromiseResolver::jniGetPromise);
    info->registerNativeMethod("resolve", "(Ljava/lang/Object;)Z", (void*)JNIV8PromiseResolver::jniResolve);
    info->registerNativeMethod("reject", "(Ljava/lang/Object;)Z", (void*)JNIV8PromiseResolver::jniReject);
}

jobject JNIV8PromiseResolver::jniGetPromise(JNIEnv *env, jobject obj) {
    JNIV8Object_PrepareJNICall(JNIV8PromiseResolver, v8::Promise::Resolver, nullptr);

    return JNIV8Wrapper::wrapObject<JNIV8Promise>(localRef->GetPromise())->getJObject();
}

jboolean JNIV8PromiseResolver::jniResolve(JNIEnv *env, jobject obj, jobject value) {
    JNIV8Object_PrepareJNICall(JNIV8PromiseResolver, v8::Promise::Resolver, false);

    auto result = localRef->Resolve(context, JNIV8Marshalling::jobject2v8value(value));
    if(env->ExceptionCheck()) return (jboolean) false;
    if(result.IsNothing()) {
        ptr->getEngine()->forwardV8ExceptionToJNI(&try_catch);
        return (jboolean)false;
    }

    return (jboolean)result.FromJust();
}

jboolean JNIV8PromiseResolver::jniReject(JNIEnv *env, jobject obj, jobject value) {
    JNIV8Object_PrepareJNICall(JNIV8PromiseResolver, v8::Promise::Resolver, false);

    auto result = localRef->Reject(context, JNIV8Marshalling::jobject2v8value(value));
    if(env->ExceptionCheck()) return (jboolean) false;
    if(result.IsNothing()) {
        ptr->getEngine()->forwardV8ExceptionToJNI(&try_catch);
        return (jboolean)false;
    }

    return (jboolean)result.FromJust();
}