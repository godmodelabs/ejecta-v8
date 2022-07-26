//
// Created by Dominik Seifert on 25.07.22
//

#include "JNIV8TypedArray.h"

#include <stdlib.h>

BGJS_JNI_LINK(JNIV8TypedArray, "ag/boersego/bgjs/JNIV8TypedArray");



bool JNIV8TypedArray::isWrappableV8Object(v8::Local<v8::Object> object) {
    return object->IsTypedArray() || (object->IsProxy() && object.As<v8::Proxy>()->GetTarget()->IsTypedArray());
}

void JNIV8TypedArray::initializeJNIBindings(JNIClassInfo *info, bool isReload) {
    info->registerNativeMethod("getV8Length", "()I", (void *) JNIV8TypedArray::jniGetV8Length);
}

/**
 * returns the length of the TypedArray
 */
jint JNIV8TypedArray::jniGetV8Length(JNIEnv *env, jobject obj) {
    JNIV8Object_PrepareJNICall(JNIV8TypedArray, v8::TypedArray, 0);
    return localRef->Length();
}

/**
 * cache JNI class references
 */
void JNIV8TypedArray::initJNICache() {
}