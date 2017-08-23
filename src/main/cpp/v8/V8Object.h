//
// Created by Martin Kleinhans on 18.08.17.
//

#ifndef TRADINGLIB_SAMPLE_V8OBJECT_H
#define TRADINGLIB_SAMPLE_V8OBJECT_H

#include "V8ClassInfo.h"
#include <jni.h>
#include "../jni/JNIWrapper.h"

class BGJSV8Engine;
class V8Object;

class V8Object : public JNIObject {
    friend class JNIV8Wrapper;
public:
    V8Object(jobject obj, JNIClassInfo *info) : JNIObject(obj, info) {};
    virtual ~V8Object();

protected:
    virtual void initFromJS(const v8::FunctionCallbackInfo<v8::Value>& args);

private:
    V8ClassInfo *_v8ClassInfo;
    BGJSV8Engine *_bgjsEngine;

    static jfieldID _nativeHandleFieldId;
};


#endif //TRADINGLIB_SAMPLE_V8OBJECT_H
