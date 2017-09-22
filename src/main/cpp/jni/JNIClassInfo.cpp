//
// Created by Martin Kleinhans on 21.04.17.
//

#include <assert.h>
#include <jni.h>

#include "JNIWrapper.h"
#include "JNIClassInfo.h"

JNIClassInfo::JNIClassInfo(size_t hashCode, JNIObjectType  type, jclass clazz, const std::string& canonicalName, ObjectInitializer i, ObjectConstructor c, JNIClassInfo *baseClassInfo) :
        hashCode(hashCode), type(type), canonicalName(canonicalName), initializer(i), constructor(c), baseClassInfo(baseClassInfo) {
    // cache class
    jniClassRef = (jclass)JNIWrapper::getEnvironment()->NewGlobalRef(clazz);
}

void JNIClassInfo::inherit() {
    // copy up field & methodMap from baseclass for faster lookup
    // can be overwritten by subclass
    if(baseClassInfo) {
        // In contrast to methods, Java constructors are NOT virtual
        // e.g. assume class B extends A; both have a constructor X taking the same parameters
        // we have to obtain a different methodID for X on B if we want to create instances of B!
        // => instead of requiring all subclasses to manually register constructors we just do this automatically!
        // also, we just drop constructors that are NOT implemented on a subclass (because they can never be used to create an instance of the subclass)
        for(auto &it : baseClassInfo->methodMap) {
            // use base class id if:
            // - method is static
            // - method is not a constructor
            // - method does not exist on the subclass (means it was not overridden)
            if(it.second.isStatic || it.second.name[0] != '<' || !getMethodID(it.second.name, it.second.signature, it.second.isStatic)) {
                // constructors that do not exist on subclasses are dropped, only methods are copied!
                if(it.second.name[0] != '<') {
                    methodMap[it.first] = it.second;
                }
                continue;
            }
            registerMethod(it.second.name, it.second.signature, it.first);
        }

        // fields are simply copied from base class
        fieldMap = baseClassInfo->fieldMap;

        // copy up constructor from base class
        if(!constructor) {
            constructor = baseClassInfo->constructor;
        }
    }
}

void JNIClassInfo::registerNativeMethod(const std::string &name, const std::string &signature, void* fnPtr) {
    for(auto &it : methods) {
        assert(strcmp(it.name, name.c_str()));
    }
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
    assert(it == methodMap.end());
    jmethodID methodId = getMethodID(methodName, signature, false);
    assert(methodId);
    methodMap[finalAlias] = {false, methodName, signature, methodId};
}

void JNIClassInfo::registerField(const std::string& fieldName,
                                 const std::string& signature,
                                 const std::string& alias
) {
    const std::string& finalAlias = alias.length() ? alias : fieldName;
    jfieldID fieldId = getFieldID(fieldName, signature, false);
    assert(fieldId);
    fieldMap[finalAlias] = {false, fieldName, signature, fieldId};
}

void JNIClassInfo::registerStaticMethod(const std::string& methodName,
                                        const std::string& signature,
                                        const std::string& alias) {
    const std::string& finalAlias = alias.length() ? alias : methodName;
    jmethodID methodId = getMethodID(methodName, signature, true);
    assert(methodId);
    methodMap[finalAlias] = {true, methodName, signature, methodId};
}

void JNIClassInfo::registerStaticField(const std::string& fieldName,
                                       const std::string& signature,
                                       const std::string& alias) {
    const std::string& finalAlias = alias.length() ? alias : fieldName;
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