//
// Created by Martin Kleinhans on 24.08.17.
//

#ifndef TRADINGLIB_SAMPLE_JNIV8GENERICOBJECT_H
#define TRADINGLIB_SAMPLE_JNIV8GENERICOBJECT_H

#include "JNIV8Object.h"

class JNIV8GenericObject : public JNIScope<JNIV8GenericObject, JNIV8Object> {
public:
    JNIV8GenericObject(jobject obj, JNIClassInfo *info) : JNIScope(obj, info) {};

    static void initializeJNIBindings(JNIClassInfo *info, bool isReload);
    static void initializeV8Bindings(V8ClassInfo *info);

    static jobject NewInstance(JNIEnv *env, jobject obj, jlong enginePtr);
};

BGJS_JNIV8OBJECT_DEF(JNIV8GenericObject)

#endif //TRADINGLIB_SAMPLE_JNIV8GENERICOBJECT_H
