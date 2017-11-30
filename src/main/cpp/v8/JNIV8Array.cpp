//
// Created by Martin Kleinhans on 26.09.17.
//

#include "JNIV8Array.h"
#include "JNIV8Wrapper.h"
#include "../bgjs/BGJSV8Engine.h"

BGJS_JNI_LINK(JNIV8Array, "ag/boersego/bgjs/JNIV8Array");

decltype(JNIV8Array::_jniObject) JNIV8Array::_jniObject = {0};

/**
 * cache JNI class references
 */
void JNIV8Array::initJNICache() {
    JNIEnv *env = JNIWrapper::getEnvironment();
    _jniObject.clazz = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Object"));
}

jobjectArray JNIV8Array::v8ArrayToObjectArray(v8::Local<v8::Array> array, uint32_t from, uint32_t to) {
    JNIEnv *env = JNIWrapper::getEnvironment();
    BGJSV8Engine *engine = getEngine();
    v8::Isolate* isolate = engine->getIsolate();
    v8::Local<v8::Context> context = engine->getContext();

    uint32_t len = array->Length();
    uint32_t size = len;
    if(from >= 0 && to >= 0 && from < len && to < len && from<=to) {
        size = (to-from)+1;
    } else {
        from = 0;
        to = len-1;
    }

    jobjectArray elements = env->NewObjectArray(size, _jniObject.clazz, nullptr);
    for(uint32_t i=from; i<=to; i++) {
        v8::MaybeLocal<v8::Value> maybeValue = array->Get(context, i);
        v8::Local<v8::Value> value;
        if(maybeValue.IsEmpty()) {
            value = v8::Undefined(isolate);
        } else {
            value = maybeValue.ToLocalChecked();
        }
        env->SetObjectArrayElement(elements, i-from, JNIV8Marshalling::v8value2jobject(value));
    }
    return elements;
}

void JNIV8Array::initializeJNIBindings(JNIClassInfo *info, bool isReload) {
    info->registerNativeMethod("Create", "(J)Lag/boersego/bgjs/JNIV8Array;", (void*)JNIV8Array::jniCreate);
    info->registerNativeMethod("Create", "(JI)Lag/boersego/bgjs/JNIV8Array;", (void*)JNIV8Array::jniCreateWithLength);
    info->registerNativeMethod("Create", "(J[Ljava/lang/Object;)Lag/boersego/bgjs/JNIV8Array;", (void*)JNIV8Array::jniCreateWithArray);
    info->registerNativeMethod("getV8Length", "()I", (void*)JNIV8Array::jniGetV8Length);
    info->registerNativeMethod("getV8Elements", "()[Ljava/lang/Object;", (void*)JNIV8Array::jniGetV8Elements);
    info->registerNativeMethod("getV8Elements", "(II)[Ljava/lang/Object;", (void*)JNIV8Array::jniGetV8ElementsInRange);
    info->registerNativeMethod("getV8Element", "(I)Ljava/lang/Object;", (void*)JNIV8Array::jniGetV8Element);
}

void JNIV8Array::initializeV8Bindings(JNIV8ClassInfo *info) {

}

/**
 * returns the length of the array
 */
jint JNIV8Array::jniGetV8Length(JNIEnv *env, jobject obj) {
    JNIV8Object_PrepareJNICall(JNIV8Array, v8::Array, 0);
    return localRef->Length();
}

/**
 * Returns all objects inside of the array
 */
jobjectArray JNIV8Array::jniGetV8Elements(JNIEnv *env, jobject obj) {
    JNIV8Object_PrepareJNICall(JNIV8Array, v8::Array, nullptr);
    return ptr->v8ArrayToObjectArray(localRef);
}

/**
 * Returns all objects from a specified range inside of the array
 */
jobjectArray JNIV8Array::jniGetV8ElementsInRange(JNIEnv *env, jobject obj, jint from, jint to) {
    JNIV8Object_PrepareJNICall(JNIV8Array, v8::Array, nullptr);
    return ptr->v8ArrayToObjectArray(localRef, (uint32_t)from, (uint32_t)to);
}

/**
 * Returns the object at the specified index
 * if index is out of bounds, returns JNIV8Undefined
 */
jobject JNIV8Array::jniGetV8Element(JNIEnv *env, jobject obj, jint index) {
    JNIV8Object_PrepareJNICall(JNIV8Array, v8::Array, JNIV8Marshalling::undefinedInJava());

    v8::MaybeLocal<v8::Value> maybeValue;

    maybeValue = localRef->Get(context, (uint32_t)index);
    if(maybeValue.IsEmpty()) return JNIV8Marshalling::undefinedInJava();

    return JNIV8Marshalling::v8value2jobject(maybeValue.ToLocalChecked());
}

jobject JNIV8Array::jniCreate(JNIEnv *env, jobject obj, jlong enginePtr) {
    return jniCreateWithLength(env, obj, enginePtr, 0);
}

jobject JNIV8Array::jniCreateWithLength(JNIEnv *env, jobject obj, jlong enginePtr, jint length) {
    BGJSV8Engine *engine = reinterpret_cast<BGJSV8Engine*>(enginePtr);

    v8::Isolate* isolate = engine->getIsolate();
    v8::Locker l(isolate);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope scope(isolate);
    v8::Context::Scope ctxScope(engine->getContext());

    v8::Local<v8::Object> objRef = v8::Array::New(isolate, length);

    return JNIV8Wrapper::wrapObject<JNIV8Array>(objRef)->getJObject();
}

jobject JNIV8Array::jniCreateWithArray(JNIEnv *env, jobject obj, jlong enginePtr, jobjectArray elements) {
    BGJSV8Engine *engine = reinterpret_cast<BGJSV8Engine*>(enginePtr);

    v8::Isolate* isolate = engine->getIsolate();
    v8::Locker l(isolate);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope scope(isolate);
    v8::Context::Scope ctxScope(engine->getContext());

    jsize numArgs = env->GetArrayLength(elements);
    v8::Local<v8::Object> objRef = v8::Array::New(isolate, numArgs);
    if (numArgs) {
        for(jsize i=0; i<numArgs; i++) {
            objRef->Set(i, JNIV8Marshalling::jobject2v8value(env->GetObjectArrayElement(elements, i)));
        }
    }

    return JNIV8Wrapper::wrapObject<JNIV8Array>(objRef)->getJObject();
}
