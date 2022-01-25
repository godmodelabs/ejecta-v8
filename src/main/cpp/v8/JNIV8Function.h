//
// Created by Martin Kleinhans on 26.09.17.
//

#ifndef ANDROID_TRADINGLIB_SAMPLE_JNIV8FUNCTION_H
#define ANDROID_TRADINGLIB_SAMPLE_JNIV8FUNCTION_H

#include "JNIV8Wrapper.h"

class JNIV8Function : public JNIScope<JNIV8Function, JNIV8Object> {
public:
    JNIV8Function(jobject obj, JNIClassInfo *info) : JNIScope(obj, info) {};

    static bool isWrappableV8Object(v8::Local<v8::Object> object);
    static void initializeJNIBindings(JNIClassInfo *info, bool isReload);

    static jobject jniCreate(JNIEnv *env, jobject obj, jobject engineObj, jobject handler);
    static jobject jniCallAsV8Function(JNIEnv *env, jobject obj, jboolean asConstructor, jint flags, jint type, jclass returnType, jobject receiver, jstring callContext, jobjectArray arguments);

    /**
     * cache JNI class references
     */
    static void initJNICache();
private:
    static struct {
        jclass clazz;
    } _jniObject;
    static v8::MaybeLocal<v8::Function> getJNIV8FunctionBaseFunction();
    static v8::MaybeLocal<v8::Function> createJavaBackedFunction(JNILocalRef<BGJSV8Engine> engine, jobject handler);
    static void v8FunctionCallback(const v8::FunctionCallbackInfo<v8::Value>& args);
};

BGJS_JNI_LINK_DEF(JNIV8Function)

#endif //ANDROID_TRADINGLIB_SAMPLE_JNIV8FUNCTION_H
