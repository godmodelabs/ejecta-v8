//
// Created by Martin Kleinhans on 12.02.19.
//

#ifndef ANDROID_TRADINGLIB_SAMPLE_JNIV8SYMBOL_H
#define ANDROID_TRADINGLIB_SAMPLE_JNIV8SYMBOL_H

#include "JNIV8Wrapper.h"

class JNIV8Symbol : public JNIScope<JNIV8Symbol, JNIV8Object> {
public:
    JNIV8Symbol(jobject obj, JNIClassInfo *info) : JNIScope(obj, info) {};

    static bool isWrappableV8Object(v8::Local<v8::Object> object);
    static void initializeJNIBindings(JNIClassInfo *info, bool isReload);

    static jobject jniCreate(JNIEnv *env, jobject obj, jobject engineObj, jstring description);
    static jobject jniFor(JNIEnv *env, jobject obj, jobject engineObj, jstring name);
    static jobject jniForEnum(JNIEnv *env, jobject obj, jobject engineObj, jstring symbol);

    /**
     * cache JNI class references
     */
    static void initJNICache();
};

BGJS_JNI_LINK_DEF(JNIV8Symbol)

#endif //ANDROID_TRADINGLIB_SAMPLE_JNIV8SYMBOL_H