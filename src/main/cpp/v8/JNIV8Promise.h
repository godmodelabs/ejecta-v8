//
// Created by Martin Kleinhans on 26.09.17.
//

#ifndef ANDROID_TRADINGLIB_SAMPLE_JNIV8PROMISE_H
#define ANDROID_TRADINGLIB_SAMPLE_JNIV8PROMISE_H

#include "JNIV8Wrapper.h"

class JNIV8Promise : public JNIScope<JNIV8Promise, JNIV8Object> {
public:
    JNIV8Promise(jobject obj, JNIClassInfo *info) : JNIScope(obj, info) {};

    static bool isWrappableV8Object(v8::Local<v8::Object> object);
    static void initializeJNIBindings(JNIClassInfo *info, bool isReload);

    static jobject jniCreateResolver(JNIEnv *env, jobject obj, jobject engineObj);

    /**
     * cache JNI class references
     */
    static void initJNICache();
};

BGJS_JNI_LINK_DEF(JNIV8Promise)

class JNIV8PromiseResolver : public JNIScope<JNIV8PromiseResolver, JNIV8Object> {
public:
    JNIV8PromiseResolver(jobject obj, JNIClassInfo *info) : JNIScope(obj, info) {};

    static bool isWrappableV8Object(v8::Local<v8::Object> object);
    static void initializeJNIBindings(JNIClassInfo *info, bool isReload);

    static jobject jniGetPromise(JNIEnv *env, jobject obj);
    static jboolean jniResolve(JNIEnv *env, jobject obj, jobject value);
    static jboolean jniReject(JNIEnv *env, jobject obj, jobject value);

    /**
     * cache JNI class references
     */
    static void initJNICache();
};

BGJS_JNI_LINK_DEF(JNIV8PromiseResolver)

#endif //ANDROID_TRADINGLIB_SAMPLE_JNIV8PROMISE_H