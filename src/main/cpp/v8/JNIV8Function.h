//
// Created by Martin Kleinhans on 26.09.17.
//

#ifndef ANDROID_TRADINGLIB_SAMPLE_JNIV8FUNCTION_H
#define ANDROID_TRADINGLIB_SAMPLE_JNIV8FUNCTION_H

#include "JNIV8Wrapper.h"

class JNIV8Function : public JNIScope<JNIV8Function, JNIV8Object> {
public:
    JNIV8Function(jobject obj, JNIClassInfo *info) : JNIScope(obj, info) {};

    static void initializeJNIBindings(JNIClassInfo *info, bool isReload);
    static void initializeV8Bindings(V8ClassInfo *info);

    static jobject jniCreate(JNIEnv *env, jobject obj, jlong enginePtr, jobject handler);
    static jobject jniCallAsV8FunctionWithReceiver(JNIEnv *env, jobject obj, jobject receiver, jobjectArray arguments);
};

BGJS_JNIV8OBJECT_DEF(JNIV8Function)

#endif //ANDROID_TRADINGLIB_SAMPLE_JNIV8FUNCTION_H
