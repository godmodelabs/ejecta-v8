//
// Created by Martin Kleinhans on 08.02.19.
//

#ifndef ANDROID_TRADINGLIB_SAMPLE_JNIV8ARRAYBUFFER_H
#define ANDROID_TRADINGLIB_SAMPLE_JNIV8ARRAYBUFFER_H

#include "JNIV8Wrapper.h"

class JNIV8ArrayBuffer : public JNIScope<JNIV8ArrayBuffer, JNIV8Object> {
public:
    JNIV8ArrayBuffer(jobject obj, JNIClassInfo *info) : JNIScope(obj, info) {};

    static bool isWrappableV8Object(v8::Local<v8::Object> object);
    static void initializeJNIBindings(JNIClassInfo *info, bool isReload);

    /**
     * cache JNI class references
     */
    static void initJNICache();
};

BGJS_JNI_LINK_DEF(JNIV8ArrayBuffer)

#endif //ANDROID_TRADINGLIB_SAMPLE_JNIV8ARRAYBUFFER_H