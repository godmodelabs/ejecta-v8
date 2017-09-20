//
// Created by Martin Kleinhans on 13.04.17.
//

#ifndef TRADINGLIB_SAMPLE_V8TESTCLASS_H
#define TRADINGLIB_SAMPLE_V8TESTCLASS_H

#include <jni.h>
#include "../v8/JNIV8Wrapper.h"

class V8TestClass : public JNIScope<V8TestClass, JNIV8Object> {
public:
    V8TestClass(jobject obj, JNIClassInfo *info) : JNIScope(obj, info) {};

    static void initializeJNIBindings(JNIClassInfo *info, bool isReload);
    static void initializeV8Bindings(V8ClassInfo *info);
    void test();

    void testMember(const std::string &methodName, const v8::FunctionCallbackInfo<v8::Value>& args);

    void initFromJS(const v8::FunctionCallbackInfo<v8::Value>& args);

    void getter(const std::string &propertyName, const v8::PropertyCallbackInfo<v8::Value> &info);
    void setter(const std::string &propertyName, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void> &info);
    std::string _name;
};

BGJS_JNIV8OBJECT_DEF(V8TestClass)

#endif //TRADINGLIB_SAMPLE_V8TESTCLASS_H
