//
// Created by Martin Kleinhans on 18.08.17.
//

#ifndef TRADINGLIB_SAMPLE_JNIV8Object_H
#define TRADINGLIB_SAMPLE_JNIV8Object_H

#include "JNIV8ClassInfo.h"
#include <jni.h>
#include "../jni/jni.h"

class BGJSV8Engine;
class JNIV8Object;

#define JNIV8Object_PrepareJNICall(T, L, R)\
auto ptr = JNIWrapper::wrapObject<T>(obj);\
if(!ptr){env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "Attempt to call method on disposed object"); return R;}\
BGJSV8Engine *engine = ptr->getEngine();\
v8::Isolate* isolate = engine->getIsolate();\
v8::Locker l(isolate);\
v8::Isolate::Scope isolateScope(isolate);\
v8::HandleScope scope(isolate);\
v8::Local<v8::Context> context = engine->getContext();\
v8::Context::Scope ctxScope(context);\
v8::TryCatch try_catch;\
v8::Local<L> localRef = v8::Local<v8::Object>::New(isolate, ptr->getJSObject()).As<L>();

#define ThrowV8RangeError(msg)\
v8::Isolate::GetCurrent()->ThrowException(v8::Exception::RangeError(\
v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), (msg).c_str()))\
);

#define ThrowV8TypeError(msg)\
v8::Isolate::GetCurrent()->ThrowException(v8::Exception::TypeError(\
v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), (msg).c_str()))\
);

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

    /**
     * returns the referenced engine
     */
    BGJSV8Engine* getEngine() const;

    /**
     * cache JNI class references
     */
    static void initJNICache();
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
    void setJSObject(BGJSV8Engine *engine, JNIV8ClassInfo *cls, v8::Handle<v8::Object> jsObject);

    static void initializeJNIBindings(JNIClassInfo *info, bool isReload);
    static void initializeV8Bindings(JNIV8ClassInfo *info);

    // jni callbacks
    static void jniAdjustJSExternalMemory(JNIEnv *env, jobject obj, jlong change);
    static jobject jniGetV8FieldWithReturnType(JNIEnv *env, jobject obj, jstring name, jint flags, jint type, jclass returnType);
    static void jniSetV8Field(JNIEnv *env, jobject obj, jstring name, jobject value);
    static void jniSetV8Fields(JNIEnv *env, jobject obj, jobject map);
    static jobject jniCallV8MethodWithReturnType(JNIEnv *env, jobject obj, jstring name, jint flags, jint type, jclass returnType, jobjectArray arguments);
    static jboolean jniHasV8Field(JNIEnv *env, jobject obj, jstring name, jboolean ownOnly);
    static jobjectArray jniGetV8Keys(JNIEnv *env, jobject obj, jboolean ownOnly);
    static jobject jniGetV8Fields(JNIEnv *env, jobject obj, jboolean ownOnly, jint flags, jint type, jclass returnType);
    static jdouble jniToNumber(JNIEnv *env, jobject obj);
    static jstring jniToString(JNIEnv *env, jobject obj);
    static jstring jniToJSON(JNIEnv *env, jobject obj);
    static void jniRegisterV8Class(JNIEnv *env, jobject obj, jstring derivedClass, jstring baseClass);

    // v8 callbacks
    static void weakPersistentCallback(const v8::WeakCallbackInfo<void>& data);

    // cached classes + ids
    static struct {
        jclass clazz;
    } _jniString;
    static struct {
        jclass clazz;
    } _jniObject;
    static struct {
        jclass clazz;
        jmethodID iteratorId;
    } _jniSet;
    static struct {
        jclass clazz;
        jmethodID hasNextId;
        jmethodID nextId;
    } _jniIterator;
    static struct {
        jclass clazz;
        jmethodID getKeyId;
        jmethodID getValueId;
    } _jniMapEntry;
    static struct {
        jclass clazz;
        jmethodID entrySetId;
    } _jniMap;
    static struct {
        jclass clazz;
        jmethodID initId;
        jmethodID putId;
    } _jniHashMap;
    // private properties
    int64_t _externalMemory;
    JNIV8ClassInfo *_v8ClassInfo;
    BGJSV8Engine *_bgjsEngine;
    v8::Persistent<v8::Object> _jsObject;
};

BGJS_JNI_LINK_DEF(JNIV8Object)

#endif //TRADINGLIB_SAMPLE_JNIV8Object_H
