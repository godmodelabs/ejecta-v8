//
// Created by Martin Kleinhans on 18.08.17.
//

#ifndef TRADINGLIB_SAMPLE_JNIV8Object_H
#define TRADINGLIB_SAMPLE_JNIV8Object_H

#include "V8ClassInfo.h"
#include <jni.h>
#include "../jni/JNIWrapper.h"

class BGJSV8Engine;
class JNIV8Object;

class JNIV8Object : public JNIObject {
    friend class JNIV8Wrapper;
public:
    JNIV8Object(jobject obj, JNIClassInfo *info) : JNIObject(obj, info) {};
    virtual ~JNIV8Object();

private:
    V8ClassInfo *_v8ClassInfo;
    BGJSV8Engine *_bgjsEngine;

    static jfieldID _nativeHandleFieldId;
};


#endif //TRADINGLIB_SAMPLE_JNIV8Object_H
