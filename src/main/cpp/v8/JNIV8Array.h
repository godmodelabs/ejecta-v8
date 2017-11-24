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
    static jobject jniCreateWithLength(JNIEnv *env, jobject obj, jlong enginePtr, jint length);
    static jobject jniCreateWithArray(JNIEnv *env, jobject obj, jlong enginePtr, jobjectArray elements);

    /**
     * returns the length of the array
     */
    static jint jniGetV8Length(JNIEnv *env, jobject obj);

    /**
     * Returns all objects inside of the array
     */
    static jobjectArray jniGetV8Elements(JNIEnv *env, jobject obj);

    /**
     * Returns all objects from a specified range inside of the array
     */
    static jobjectArray jniGetV8ElementsInRange(JNIEnv *env, jobject obj, jint from, jint to);

    /**
     * Returns the object at the specified index
     * if index is out of bounds, returns JNIV8Undefined
     */
    static jobject jniGetV8Element(JNIEnv *env, jobject obj, jint index);

    /**
     * cache JNI class references
     */
    static void initJNICache();
private:
    static struct {
        jclass clazz;
    } _jniObject;
    jobjectArray v8ArrayToObjectArray(v8::Local<v8::Array> array, uint32_t from=1, uint32_t to=0);
};

BGJS_JNI_LINK_DEF(JNIV8Array)

#endif //ANDROID_TRADINGLIB_SAMPLE_JNIV8ARRAY_H
