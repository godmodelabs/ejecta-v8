//
// Created by Martin Kleinhans on 21.04.17.
//

#include <assert.h>
#include <jni.h>

#include "JNIWrapper.h"
#include "JNIClassInfo.h"

JNIClassInfo::JNIClassInfo(JNIObjectType  type, jclass clazz, const std::string& canonicalName, ObjectInitializer i, ObjectConstructor c, JNIClassInfo *baseClassInfo) :
        type(type), canonicalName(canonicalName), initializer(i), constructor(c), baseClassInfo(baseClassInfo) {
    // cache class
    jniClassRef = (jclass)JNIWrapper::getEnvironment()->NewGlobalRef(clazz);

    // copy up field & methodMap from baseclass for faster lookup
    // can be overwritten by subclass
    if(baseClassInfo) {
        // we might have to register the base classes instance methods again on this class because they might have been overridden on the java side
        // if we kept using the baseclasses ids, we would always call the baseclasses implementation!
        // Assume A<-B<-C. If B overwrites method M from A, and we would be using the methodId from A, then C would bypass B's overridden method
        // this clearly is NOT the desired outcome and not even possible in Java itself; there B could only call B.M from its own M implementation
        for(auto &it : baseClassInfo->methodMap) {
            // use base class id if:
            // - method does not exist on the subclass it means it was not overridden
            // - method is static
            if(it.second.isStatic || !getMethodID(it.second.name, it.second.signature, it.second.isStatic)) {
                methodMap[it.first] = it.second;
                continue;
            }
            registerMethod(it.second.name, it.second.signature, it.first);
        }

        // copy up constructor from base class
        if(!constructor) {
            constructor = baseClassInfo->constructor;
        }
    }
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
    assert(it != methodMap.end());
    jmethodID methodId = getMethodID(methodName, signature, false);
    assert(methodId);
    methodMap[finalAlias] = {false, methodName, signature, methodId};
}

void JNIClassInfo::registerField(const std::string& fieldName,
                                 const std::string& signature,
                                 const std::string& alias
) {
    const std::string& finalAlias = alias.length() ? alias : fieldName;
    auto it = fieldMap.find(finalAlias);
    assert(it != fieldMap.end());
    jfieldID fieldId = getFieldID(fieldName, signature, false);
    assert(fieldId);
    fieldMap[finalAlias] = {false, fieldName, signature, fieldId};
}

void JNIClassInfo::registerStaticMethod(const std::string& methodName,
                                        const std::string& signature,
                                        const std::string& alias) {
    const std::string& finalAlias = alias.length() ? alias : methodName;
    auto it = methodMap.find(finalAlias);
    assert(it != methodMap.end());
    jmethodID methodId = getMethodID(methodName, signature, true);
    assert(methodId);
    methodMap[finalAlias] = {true, methodName, signature, methodId};
}

void JNIClassInfo::registerStaticField(const std::string& fieldName,
                                       const std::string& signature,
                                       const std::string& alias) {
    const std::string& finalAlias = alias.length() ? alias : fieldName;
    auto it = fieldMap.find(finalAlias);
    assert(it != fieldMap.end());
    jfieldID fieldId = getFieldID(fieldName, signature, true);
    assert(fieldId);
    fieldMap[finalAlias] = {true, fieldName, signature, fieldId};
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
    return fieldId;
}