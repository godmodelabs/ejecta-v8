//
// Created by Martin Kleinhans on 18.04.17.
//

#include <assert.h>

#include "JNIObject.h"
#include "JNIWrapper.h"

JNIObject::JNIObject(jobject obj,JNIClassInfo *info) : JNIClass(info) {

    JNIEnv* env = JNIWrapper::getEnvironment();
    _jniObject = env->NewGlobalRef(obj);

    // store pointer to native instance in "nativeHandle" field
    if(info->persistent) {
        setLongField("nativeHandle", reinterpret_cast<jlong>(this));
    }
}

JNIObject::~JNIObject() {
    if(_jniClassInfo->persistent) {
        setLongField("nativeHandle", reinterpret_cast<jlong>(nullptr));
    }
    JNIWrapper::getEnvironment()->DeleteGlobalRef(_jniObject);
    _jniObject = nullptr;
}

const jobject JNIObject::getJObject() const {
    return _jniObject;
}

//--------------------------------------------------------------------------------------------------
// Fields Getter
//--------------------------------------------------------------------------------------------------
#define GETTER(TypeName, JNITypeName) \
JNITypeName JNIObject::get##TypeName##Field(const std::string& fieldName) {\
    JNIEnv* env = JNIWrapper::getEnvironment(); \
    const auto fieldId = _jniClassInfo->fieldMap.at(fieldName); \
    return env->Get##TypeName##Field(_jniObject, fieldId); \
}

GETTER(Long, jlong)
GETTER(Boolean, jboolean)
GETTER(Byte, jbyte)
GETTER(Char, jchar)
GETTER(Double, jdouble)
GETTER(Float, jfloat)
GETTER(Int, jint)
GETTER(Short, jshort)
GETTER(Object, jobject)

//--------------------------------------------------------------------------------------------------
// Fields Setter
//--------------------------------------------------------------------------------------------------
#define SETTER(TypeName, JNITypeName) \
void JNIObject::set##TypeName##Field(const std::string& fieldName, JNITypeName value) {\
    JNIEnv* env = JNIWrapper::getEnvironment(); \
    const auto fieldId = _jniClassInfo->fieldMap.at(fieldName); \
    return env->Set##TypeName##Field(_jniObject, fieldId, value); \
}

SETTER(Long, jlong)
SETTER(Boolean, jboolean)
SETTER(Byte, jbyte)
SETTER(Char, jchar)
SETTER(Double, jdouble)
SETTER(Float, jfloat)
SETTER(Int, jint)
SETTER(Short, jshort)
SETTER(Object, jobject)

//--------------------------------------------------------------------------------------------------
// Methods
//--------------------------------------------------------------------------------------------------
#define METHOD(TypeName, JNITypeName) \
JNITypeName JNIObject::call##TypeName##Method(const char* name, ...) {\
    JNIEnv* env = JNIWrapper::getEnvironment();\
    const auto methodId = _jniClassInfo->methodMap.at(name);\
    va_list args;\
    JNITypeName res;\
    va_start(args, name);\
    res = env->Call##TypeName##MethodV(_jniObject, methodId, args);\
    va_end(args);\
    return res;\
}

void JNIObject::callVoidMethod(const char* name, ...) {
    JNIEnv* env = JNIWrapper::getEnvironment();
    const auto methodId = _jniClassInfo->methodMap.at(name);
    va_list args;
    va_start(args, name);\
    env->CallVoidMethodV(_jniObject, methodId, args);
    va_end(args);
}

METHOD(Long, jlong)
METHOD(Boolean, jboolean)
METHOD(Byte, jbyte)
METHOD(Char, jchar)
METHOD(Double, jdouble)
METHOD(Float, jfloat)
METHOD(Int, jint)
METHOD(Short, jshort)
METHOD(Object, jobject)

//--------------------------------------------------------------------------------------------------
// Exports
//--------------------------------------------------------------------------------------------------
extern "C" {
    JNIEXPORT void JNICALL Java_ag_boersego_bgjs_JNIObject_initBinding(JNIEnv *env) {
        JNIWrapper::reloadBindings();
    }

    JNIEXPORT void JNICALL Java_ag_boersego_bgjs_JNIObject_initNative(JNIEnv *env, jobject obj) {
        JNIWrapper::initializeNativeObject(obj);
    }

    JNIEXPORT void JNICALL Java_ag_boersego_bgjs_JNIObject_disposeNative(JNIEnv *env, jobject obj, jlong nativeHandle) {
        delete reinterpret_cast<JNIObject*>(nativeHandle);
    }
}

