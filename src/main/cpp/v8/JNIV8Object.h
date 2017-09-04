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

    v8::Local<v8::Object> getJSObject();

    static void weakPersistentCallback(const v8::WeakCallbackInfo<void>& data);
protected:
    void setJSObject(BGJSV8Engine *engine, V8ClassInfo *cls, v8::Handle<v8::Object> jsObject);
private:
    void makeWeak();
    void linkJSObject(v8::Handle<v8::Object> jsObject);

    V8ClassInfo *_v8ClassInfo;
    BGJSV8Engine *_bgjsEngine;
    v8::Persistent<v8::Object> _jsObject;
};


#endif //TRADINGLIB_SAMPLE_JNIV8Object_H
