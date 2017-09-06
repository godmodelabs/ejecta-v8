//
// Created by Martin Kleinhans on 18.08.17.
//

#ifndef TRADINGLIB_SAMPLE_JNIV8Object_H
#define TRADINGLIB_SAMPLE_JNIV8Object_H

#include "V8ClassInfo.h"
#include <jni.h>
#include "../jni/JNIWrapper.h"

class BGJSV8Engine;
class JNIV8Object;

/**
 * Base class for all native classes associated with a java object and a js object
 * constructor should never be called manually; if you want to create a new instance
 * use JNIV8Wrapper::createObject<Type>(env) instead
 */
class JNIV8Object : public JNIObject {
    friend class JNIV8Wrapper;
    friend class JNIWrapper;
public:
    JNIV8Object(jobject obj, JNIClassInfo *info);
    virtual ~JNIV8Object();

    /**
     * returns the referenced js object
     */
    v8::Local<v8::Object> getJSObject();

protected:
    /**
     * can be used to tell the javascript engine about the amount of memory used by the
     * c++ or java part of the object to improve garbage collection
     */
    void adjustJSExternalMemory(int64_t change);

private:
    // private methods
    void makeWeak();
    void linkJSObject(v8::Handle<v8::Object> jsObject);

    // initialization; called from JNIV8Wrapper
    void setJSObject(BGJSV8Engine *engine, V8ClassInfo *cls, v8::Handle<v8::Object> jsObject);

    // jni callbacks
    static void initializeJNIBindings(JNIClassInfo *info, bool isReload);
    static void jniAdjustJSExternalMemory(JNIEnv *env, jobject obj, jlong change);

    // v8 callbacks
    static void weakPersistentCallback(const v8::WeakCallbackInfo<void>& data);

    // private properties
    int64_t _externalMemory;
    V8ClassInfo *_v8ClassInfo;
    BGJSV8Engine *_bgjsEngine;
    v8::Persistent<v8::Object> _jsObject;
};


#endif //TRADINGLIB_SAMPLE_JNIV8Object_H
