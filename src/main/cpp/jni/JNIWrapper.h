//
// Created by Martin Kleinhans on 20.04.17.
//

#ifndef __JNIWRAPPER_H
#define __JNIWRAPPER_H

#import <vector>
#import <string>
#import <map>
#include <jni.h>

#include "JNIClassInfo.h"
#include "JNIClass.h"
#include "JNIObject.h"

class JNIWrapper {
public:
    static void init(JavaVM *vm);
    static JNIEnv* getEnvironment();

    /**
     * returns the canonical name of the java class associated with the specified native object
     */
    template <typename T> static
    const std::string getCanonicalName() {
        throw "JNIWrapper::getCanonicalName called for unregistered class";
    }

    /**
     * returns the java signature of the specified native object type
     */
    template<class ObjectType> static
    const std::string getSignature() {
        return "L" + JNIWrapper::getCanonicalName<ObjectType>() + ";";
    };

    /**
     * internal struct used for registering a class with the factory
     */
    template<class ObjectType> static
    void registerObject(bool persistent = true) {
        _registerObject(persistent, JNIWrapper::getCanonicalName<ObjectType>(), initialize<ObjectType>, instantiate<ObjectType>);
    };

    template<class ObjectType> static
    void registerDerivedObject(const std::string &canonicalName, bool persistent = true) {
        _registerObject(persistent, canonicalName, initialize<ObjectType>, instantiate<ObjectType>);
    };

    /**
     * creates a Java+Native Object tuple based on the specified object type
     * object needs to have been registered before with BGJS_REGISTER_OBJECT
     */
    template <typename ObjectType> static
    std::shared_ptr<ObjectType> createObject(const char *constructorAlias = nullptr, ...) {
        va_list args;
        va_start(args, constructorAlias);
        jobject obj = _createObject(JNIWrapper::getCanonicalName<ObjectType>(), constructorAlias, args);
        va_end(args);
        return JNIWrapper::wrapObject<ObjectType>(obj);
    }

    template <typename ObjectType> static
    std::shared_ptr<ObjectType> createObject(const char *constructorAlias, va_list args) {
        jobject obj = _createObject(JNIWrapper::getCanonicalName<ObjectType>(), constructorAlias, args);
        return JNIWrapper::wrapObject<ObjectType>(obj);
    }

    /**
     * wraps a java object
     */
    template <typename ObjectType> static
    std::shared_ptr<ObjectType> wrapObject(jobject object) {
        auto it = _objmap.find(JNIWrapper::getCanonicalName<ObjectType>());
        if (it == _objmap.end()){
            return nullptr;
        } else {
            JNIClassInfo *info = it->second;
            ObjectType *jniObject;
            JNIEnv* env = JNIWrapper::getEnvironment();
            if(info->persistent) {
                auto handleFieldId = info->fieldMap.at("nativeHandle");
                if(!handleFieldId) {
                    env->ExceptionClear();
                    return nullptr;
                }
                jlong handle = env->GetLongField(object, handleFieldId);
                jniObject = reinterpret_cast<ObjectType*>(handle);
            } else {
                jniObject = new ObjectType(object, info);
            }
            return std::shared_ptr<ObjectType>(jniObject,[=](ObjectType* cls) {
                if(!cls->isPersistent()) delete cls;
            });
        }
    };

    /**
     * wraps a java class
     */
    template <typename ObjectType> static
    std::shared_ptr<JNIClass> wrapClass() {
        return _wrapClass(JNIWrapper::getCanonicalName<ObjectType>());
    }

    /**
     * creates a native object based on the specified class name
     * internal utility method; should not be called manually!
     * instead you should use:
     * - createObject<NativeType>() if you want to create a new Java+Native object tuple
     */
    static JNIObject* initializeNativeObject(jobject object);

    /**
     * internal utility method; should not be called manually
     * used to refresh all references after classes where unloaded
     */
    static void reloadBindings();
private:
    // Factory method for creating objects
    static jobject _createObject(const std::string& canonicalName, const char* constructorAlias, va_list constructorArgs);
    static std::shared_ptr<JNIClass> _wrapClass(const std::string& canonicalName);

    static void _registerObject(bool persistent, const std::string& canonicalName, ObjectInitializer i, ObjectConstructor c);

    static JavaVM *_jniVM;
    static JNIEnv *_jniEnv;
    static jmethodID _jniCanonicalNameMethodID;

    static std::map<std::string, JNIClassInfo*> _objmap;

    template<class ObjectType>
    static JNIObject* instantiate(jobject obj, JNIClassInfo *info) {
        return new ObjectType(obj, info);
    }

    template<class ObjectType>
    static void initialize(JNIClassInfo *info, bool isReload) {
        ObjectType::initializeJNIBindings(info, isReload);
    }
};

/**
 * macro to generate constructors for objects derived from JNIObject
 */
#define BGJS_JNIOBJECT_CONSTRUCTOR(type) type(jobject obj, JNIClassInfo *info) : JNIObject(obj, info) {};

/**
 * macro to register a native class with JNI
 * specify the native class, and the full canonical name of the associated java class
 */
#define BGJS_JNIOBJECT_LINK(type, canonicalName) template<> const std::string JNIWrapper::getCanonicalName<type>() { return canonicalName; };
#define BGJS_JNIOBJECT_DEF(type) template<> const std::string JNIWrapper::getCanonicalName<type>();

#endif //__JNIWRAPPER_H
