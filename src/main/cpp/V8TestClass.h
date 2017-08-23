//
// Created by Martin Kleinhans on 13.04.17.
//

#ifndef TRADINGLIB_SAMPLE_V8TESTCLASS_H
#define TRADINGLIB_SAMPLE_V8TESTCLASS_H

#include <jni.h>
#include "../v8/JNIV8Wrapper.h"

class V8TestClass : public V8Object {
public:
    BGJS_JNIV8OBJECT_CONSTRUCTOR(V8TestClass);
    static void initializeJNIBindings(JNIClassInfo *info, bool isReload);
    static void initializeV8Bindings(V8ClassInfo *info);
    void test();

    void testMember(const v8::FunctionCallbackInfo<v8::Value>& args);

    void initFromJS(const v8::FunctionCallbackInfo<v8::Value>& args);

    std::string _name;
};

BGJS_JNIOBJECT_DEF(V8TestClass)

#endif //TRADINGLIB_SAMPLE_V8TESTCLASS_H
