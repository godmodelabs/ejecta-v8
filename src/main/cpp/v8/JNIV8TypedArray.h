//
// Created by Dominik Seifert on 25.07.22
//

#ifndef ANDROID_TRADINGLIB_SAMPLE_JNIV8TYPEDARRAY_H
#define ANDROID_TRADINGLIB_SAMPLE_JNIV8TYPEDARRAY_H

#include "JNIV8Wrapper.h"

class JNIV8TypedArray : public JNIScope<JNIV8TypedArray, JNIV8Object> {
public:
    JNIV8TypedArray(jobject obj, JNIClassInfo *info) : JNIScope(obj, info) {};

    static bool isWrappableV8Object(v8::Local<v8::Object> object);
    static void initializeJNIBindings(JNIClassInfo *info, bool isReload);

    /**
    * returns the length of the TypedArray
    */
    static jint jniGetV8Length(JNIEnv *env, jobject obj);

    /**
     * cache JNI class references
     */
    static void initJNICache();
};

BGJS_JNI_LINK_DEF(JNIV8TypedArray)

#endif //ANDROID_TRADINGLIB_SAMPLE_JNIV8TYPEDARRAY_H