//
// Created by Martin Kleinhans on 13.04.17.
//

#ifndef TRADINGLIB_SAMPLE_V8TESTCLASS_H
#define TRADINGLIB_SAMPLE_V8TESTCLASS_H

#include <jni.h>
#include "jni/JNIWrapper.h"

class V8TestClass : public JNIObject {
public:
    BGJS_JNIOBJECT_CONSTRUCTOR(V8TestClass);
    static void initializeJNIBindings(JNIClassInfo *info, bool isReload);
    void test();

    std::string _name;
};


#endif //TRADINGLIB_SAMPLE_V8TESTCLASS_H
