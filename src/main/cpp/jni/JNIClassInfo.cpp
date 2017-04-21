//
// Created by Martin Kleinhans on 21.04.17.
//

#include <assert.h>
#include <jni.h>

#include "JNIWrapper.h"
#include "JNIClassInfo.h"

JNIClassInfo::JNIClassInfo(bool persistent, const std::string& canonicalName, ObjectInitializer i, ObjectConstructor c) :
        persistent(persistent), canonicalName(canonicalName), initializer(i), constructor(c) {
    jniClassRef = nullptr;
}

void JNIClassInfo::registerNativeMethod(const std::string &name, const std::string &signature, void* fnPtr) {
    methods.push_back({strdup(name.c_str()), strdup(signature.c_str()), fnPtr}); // freed after calling registerNatives
}

void JNIClassInfo::registerConstructor(const std::string& signature, const std::string& alias) {
    registerMethod("<init>", signature, alias);
}

void JNIClassInfo::registerMethod(const std::string& methodName,
                                  const std::string& signature,
                                  const std::string& alias) {
    const std::string& finalAlias = alias.length() ? alias : methodName;
    auto it = methodMap.find(finalAlias);
    if(it != methodMap.end()) {
        return;
    }
    methodMap[finalAlias] = getMethodID(methodName, signature, false);
}

void JNIClassInfo::registerField(const std::string& fieldName,
                                 const std::string& signature) {
    auto it = fieldMap.find(fieldName);
    if(it != fieldMap.end()) {
        return;
    }
    fieldMap[fieldName] = getFieldID(fieldName, signature, false);
}

void JNIClassInfo::registerStaticMethod(const std::string& methodName,
                                        const std::string& signature,
                                        const std::string& alias) {
    const std::string& finalAlias = alias.length() ? alias : methodName;
    auto it = methodMap.find(finalAlias);
    if(it != methodMap.end()) {
        return;
    }
    methodMap[finalAlias] = getMethodID(methodName, signature, true);
}

void JNIClassInfo::registerStaticField(const std::string& fieldName,
                                       const std::string& signature) {
    auto it = fieldMap.find(fieldName);
    if(it != fieldMap.end()) {
        return;
    }
    fieldMap[fieldName] = getFieldID(fieldName, signature, true);
}

jmethodID JNIClassInfo::getMethodID(const std::string& methodName,
                                    const std::string& signature,
                                    bool isStatic) {
    JNIEnv* env = JNIWrapper::getEnvironment();
    if(!jniClassRef) return nullptr;

    jmethodID methodId;
    if(isStatic) {
        methodId = env->GetStaticMethodID(jniClassRef, methodName.c_str(), signature.c_str());
    } else {
        methodId = env->GetMethodID(jniClassRef, methodName.c_str(), signature.c_str());
    }
    if(!methodId) {
        env->ExceptionClear();
        return nullptr;
    }
    return methodId;
}

jfieldID JNIClassInfo::getFieldID(const std::string& fieldName,
                                  const std::string& signature,
                                  bool isStatic) {
    JNIEnv* env = JNIWrapper::getEnvironment();
    if(!jniClassRef) return nullptr;

    jfieldID fieldId;
    if(isStatic) {
        fieldId = env->GetStaticFieldID(jniClassRef, fieldName.c_str(), signature.c_str());
    } else {
        fieldId = env->GetFieldID(jniClassRef, fieldName.c_str(), signature.c_str());
    }
    if(!fieldId) {
        env->ExceptionClear();
        return nullptr;
    }

    fieldMap[fieldName] = fieldId;
    return fieldId;
}