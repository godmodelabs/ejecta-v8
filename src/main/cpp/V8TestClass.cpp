//
// Created by Martin Kleinhans on 13.04.17.
//

#include "V8TestClass.h"
#include <assert.h>

#include "../bgjs/BGJSV8Engine.h"
#include <functional>

using namespace v8;

BGJS_JNIV8OBJECT_LINK(V8TestClass, "ag/boersego/bgjs/V8TestClass");

void _V8TestClass_test(JNIEnv *env, jobject object, jlong l, jfloat f, jdouble d, jstring s) {
    auto pInstance = JNIWrapper::wrapObject<V8TestClass>(object);
    pInstance->test();
}

jstring _V8TestClass_getName(JNIEnv *env, jobject object, jlong l, jfloat f, jdouble d, jstring s) {
    auto pInstance = JNIWrapper::wrapObject<V8TestClass>(object);
    return env->NewStringUTF(pInstance->_name.c_str());
}

void V8TestClass::initializeV8Bindings(V8ClassInfo *info) {
    info->registerConstructor((JNIV8ObjectConstructorCallback)&V8TestClass::initFromJS);
    info->registerMethod("methodName", (JNIV8ObjectMethodCallback)&V8TestClass::testMember);
    info->registerAccessor("propertyName", (JNIV8ObjectAccessorGetterCallback)&V8TestClass::getter,
                           (JNIV8ObjectAccessorSetterCallback)&V8TestClass::setter);
}

void V8TestClass::testMember(const std::string &methodName, const v8::FunctionCallbackInfo<v8::Value>& args) {
    HandleScope scope(args.GetIsolate());
    Local<Object> obj = (Local<Object>::Cast(args[0]));
    auto cls = JNIV8Wrapper::wrapObject<V8TestClass>(obj);
    cls->test();
}

void V8TestClass::getter(const std::string &propertyName, const v8::PropertyCallbackInfo<v8::Value> &info) {

}

void V8TestClass::setter(const std::string &propertyName, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void> &info) {

}

void V8TestClass::initFromJS(const v8::FunctionCallbackInfo<v8::Value>& args) {
}

void V8TestClass::initializeJNIBindings(JNIClassInfo *info, bool isReload) {
    info->registerNativeMethod("test", "(JFDLjava/lang/String;)V", (void*)_V8TestClass_test);
    info->registerNativeMethod("getName","()Ljava/lang/String;", (void*)_V8TestClass_getName);

    info->registerMethod("test3", "(JF)V");
    info->registerStaticMethod("test4", "(Lag/boersego/bgjs/V8TestClass;)V");
    info->registerField("testLong","J");
    info->registerStaticField("staticField","J");
}


void V8TestClass::test() {
    this->callJavaVoidMethod("test3", (jlong)17, (jfloat)13.5);

    /*
    auto pNewInstance = JNIWrapper::createObject<V8TestClass>("customInit",(jlong)1337);
    pNewInstance->callJavaVoidMethod("test3", (jlong)17, (jfloat)13.5);
    pNewInstance->setJavaLongField("testLong", (jlong)1337);
    pNewInstance->_name = "Hello";

    auto clazz = JNIWrapper::wrapClass<V8TestClass>();
    clazz->callJavaStaticVoidMethod("test4", pNewInstance->getJObject());
    clazz->setJavaStaticLongField("staticField", 1337);
    clazz->getJavaStaticLongField("staticField");
     */
}