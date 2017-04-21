//
// Created by Martin Kleinhans on 13.04.17.
//

#include "V8TestClass.h"
#include <assert.h>

BGJS_JNIOBJECT_LINK(V8TestClass, "ag/boersego/bgjs/V8TestClass");

void _V8TestClass_test(JNIEnv *env, jobject object, jlong l, jfloat f, jdouble d, jstring s) {
    auto pInstance = JNIWrapper::wrapObject<V8TestClass>(object);
    pInstance->test();
}

jstring _V8TestClass_getName(JNIEnv *env, jobject object, jlong l, jfloat f, jdouble d, jstring s) {
    auto pInstance = JNIWrapper::wrapObject<V8TestClass>(object);
    jstring str;
    return env->NewStringUTF(pInstance->_name.c_str());
}

void V8TestClass::initializeJNIBindings(JNIClassInfo *info, bool isReload) {
    info->registerNativeMethod("test", "(JFDLjava/lang/String;)V", (void*)_V8TestClass_test);
    info->registerNativeMethod("getName","()Ljava/lang/String;", (void*)_V8TestClass_getName);

    info->registerConstructor("(J)V","customInit");
    info->registerMethod("test3", "(JF)V");
    info->registerStaticMethod("test4", "(Lag/boersego/bgjs/V8TestClass;)V");
    info->registerField("testLong","J");
    info->registerStaticField("staticField","J");
}

void V8TestClass::test() {

    auto pNewInstance = JNIWrapper::createObject<V8TestClass>("customInit",(jlong)1337);
    pNewInstance->callVoidMethod("test3", (jlong)17, (jfloat)13.5);
    pNewInstance->setLongField("testLong", (jlong)1337);
    pNewInstance->_name = "Hello";

    auto clazz = JNIWrapper::wrapClass<V8TestClass>();
    clazz->callStaticVoidMethod("test4", pNewInstance->getJObject());
    clazz->setStaticLongField("staticField", 1337);
    clazz->getStaticLongField("staticField");
}