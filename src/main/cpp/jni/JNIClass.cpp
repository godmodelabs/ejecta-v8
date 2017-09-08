//
// Created by Martin Kleinhans on 21.04.17.
//

#include <assert.h>

#include "JNIClass.h"
#include "JNIWrapper.h"

JNIClass::JNIClass(JNIClassInfo *info) {
    _jniClassInfo = info;
}

JNIClass::~JNIClass() {
}

bool JNIClass::isPersistent() const {
    return _jniClassInfo->type == JNIObjectType::kPersistent;
}

const std::string& JNIClass::getCanonicalName() const {
    return _jniClassInfo->canonicalName;
}

const jclass JNIClass::getJClass() const {
    return _jniClassInfo->jniClassRef;
}

const std::string JNIClass::getSignature() const {
    return "L" + _jniClassInfo->canonicalName + ";";
}

//--------------------------------------------------------------------------------------------------
// Static Fields Getter
//--------------------------------------------------------------------------------------------------
#define GETTER_STATIC(TypeName, JNITypeName) \
JNITypeName JNIClass::getJavaStatic##TypeName##Field(const std::string& fieldName) {\
    JNIEnv* env = JNIWrapper::getEnvironment(); \
    auto it = _jniClassInfo->fieldMap.find(fieldName); \
    assert(it != _jniClassInfo->fieldMap.end());\
    assert(it->second.isStatic);\
    return env->GetStatic##TypeName##Field(_jniClassInfo->jniClassRef, it->second.it); \
}

GETTER_STATIC(Long, jlong)
GETTER_STATIC(Boolean, jboolean)
GETTER_STATIC(Byte, jbyte)
GETTER_STATIC(Char, jchar)
GETTER_STATIC(Double, jdouble)
GETTER_STATIC(Float, jfloat)
GETTER_STATIC(Int, jint)
GETTER_STATIC(Short, jshort)
GETTER_STATIC(Object, jobject)

//--------------------------------------------------------------------------------------------------
// Static Fields Setter
//--------------------------------------------------------------------------------------------------
#define SETTER_STATIC(TypeName, JNITypeName) \
void JNIClass::setJavaStatic##TypeName##Field(const std::string& fieldName, JNITypeName value) {\
    JNIEnv* env = JNIWrapper::getEnvironment(); \
    auto it = _jniClassInfo->fieldMap.find(fieldName); \
    assert(it != _jniClassInfo->fieldMap.end());\
    assert(it->second.isStatic);\
    return env->SetStatic##TypeName##Field(_jniClassInfo->jniClassRef, it->second.id, value); \
}

SETTER_STATIC(Long, jlong)
SETTER_STATIC(Boolean, jboolean)
SETTER_STATIC(Byte, jbyte)
SETTER_STATIC(Char, jchar)
SETTER_STATIC(Double, jdouble)
SETTER_STATIC(Float, jfloat)
SETTER_STATIC(Int, jint)
SETTER_STATIC(Short, jshort)
SETTER_STATIC(Object, jobject)

//--------------------------------------------------------------------------------------------------
// Static Methods
//--------------------------------------------------------------------------------------------------
#define METHOD_STATIC(TypeName, JNITypeName) \
JNITypeName JNIClass::callJavaStatic##TypeName##Method(const char* name, ...) {\
    JNIEnv* env = JNIWrapper::getEnvironment();\
    auto it = _jniClassInfo->methodMap.find(name); \
    assert(it != _jniClassInfo->methodMap.end());\
    assert(it->second.isStatic);\
    va_list args;\
    JNITypeName res;\
    va_start(args, name);\
    res = env->CallStatic##TypeName##MethodV(_jniClassInfo->jniClassRef, it->second.id, args);\
    va_end(args);\
    return res;\
}

void JNIClass::callJavaStaticVoidMethod(const char* name, ...) {
    JNIEnv* env = JNIWrapper::getEnvironment();
    auto it = _jniClassInfo->methodMap.find(name);
    assert(it != _jniClassInfo->methodMap.end());
    assert(it->second.isStatic);
    va_list args;
    va_start(args, name);
    env->CallStaticVoidMethodV(_jniClassInfo->jniClassRef, it->second.id, args);
    va_end(args);
}

METHOD_STATIC(Long, jlong)
METHOD_STATIC(Boolean, jboolean)
METHOD_STATIC(Byte, jbyte)
METHOD_STATIC(Char, jchar)
METHOD_STATIC(Double, jdouble)
METHOD_STATIC(Float, jfloat)
METHOD_STATIC(Int, jint)
METHOD_STATIC(Short, jshort)
METHOD_STATIC(Object, jobject)