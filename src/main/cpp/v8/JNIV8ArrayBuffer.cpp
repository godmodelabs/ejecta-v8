//
// Created by Martin Kleinhans on 08.02.19
//

#include "JNIV8ArrayBuffer.h"

#include <stdlib.h>

BGJS_JNI_LINK(JNIV8ArrayBuffer, "ag/boersego/bgjs/JNIV8ArrayBuffer");

/**
 * cache JNI class references
 */
void JNIV8ArrayBuffer::initJNICache() {
}

bool JNIV8ArrayBuffer::isWrappableV8Object(v8::Local<v8::Object> object) {
    return object->IsArrayBuffer();
}

void JNIV8ArrayBuffer::initializeJNIBindings(JNIClassInfo *info, bool isReload) {
}