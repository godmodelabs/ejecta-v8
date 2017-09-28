//
// Created by Martin Kleinhans on 26.09.17.
//

#ifndef ANDROID_TRADINGLIB_SAMPLE_JNIV8ARRAY_H
#define ANDROID_TRADINGLIB_SAMPLE_JNIV8ARRAY_H

#include "JNIV8Wrapper.h"

class JNIV8Array : public JNIScope<JNIV8Array, JNIV8Object> {
public:
    JNIV8Array(jobject obj, JNIClassInfo *info) : JNIScope(obj, info) {};

    static void initializeJNIBindings(JNIClassInfo *info, bool isReload);
    static void initializeV8Bindings(V8ClassInfo *info);

    static jobject jniCreate(JNIEnv *env, jobject obj, jlong enginePtr);
};

BGJS_JNIV8OBJECT_DEF(JNIV8Array)

#endif //ANDROID_TRADINGLIB_SAMPLE_JNIV8ARRAY_H
