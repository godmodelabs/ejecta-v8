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
#include <assert.h>

class JNIWrapper {
public:
    static void init(JavaVM *vm);
    static JNIEnv* getEnvironment();
    static bool isInitialized();

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
    void registerObject(JNIObjectType type = JNIObjectType::kPersistent) {
        _registerObject(type, JNIWrapper::getCanonicalName<ObjectType>(), JNIWrapper::getCanonicalName<JNIObject>(),
                        initialize<ObjectType>, type == JNIObjectType::kPersistent ? instantiate<ObjectType> : nullptr);
    };

    template<class ObjectType, class BaseObjectType> static
    void registerObject(JNIObjectType type = JNIObjectType::kPersistent) {
        _registerObject(type, JNIWrapper::getCanonicalName<ObjectType>(), JNIWrapper::getCanonicalName<BaseObjectType>(),
                        initialize<ObjectType>, type == JNIObjectType::kPersistent ? instantiate<ObjectType> : nullptr);
    };

    template<class ObjectType> static
    void registerDerivedObject(const std::string &canonicalName, JNIObjectType type = JNIObjectType::kPersistent) {
        _registerObject(type, canonicalName, JNIWrapper::getCanonicalName<ObjectType>(), nullptr, nullptr);
    };

    /**
     * creates a Java+Native Object tuple based on the specified object type
     * prerequisites:
     * - object needs to have been registered before with JNIWrapper::registerObject<ObjectType>() and BGJS_JNIJNIV8Object_LINK
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
     * creates a Java+Native Object tuple based on the specified object type and canonicalName
     *
     * the canonicalName must identify a java class that extends a baseclass linked to the specified objectType
     *
     * prerequisites:
     * - base class must have been registered before with JNIWrapper::registerObject<ObjectType>() and BGJS_JNIJNIV8Object_LINK
     * - derived class must have been registed with JNIWrapper::registerDerivedObject<ObjectType>(canonicalName)
     */
    template <typename ObjectType> static
    std::shared_ptr<ObjectType> createDerivedObject(const std::string &canonicalName, const char *constructorAlias = nullptr, ...) {
        va_list args;
        va_start(args, constructorAlias);
        jobject obj = _createObject(canonicalName, constructorAlias, args);
        va_end(args);
        return JNIWrapper::wrapObject<ObjectType>(obj);
    }

    template <typename ObjectType> static
    std::shared_ptr<ObjectType> createDerivedObject(const std::string &canonicalName, const char *constructorAlias, va_list args) {
        jobject obj = _createObject(canonicalName, constructorAlias, args);
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
            JNIObject *jniObject;
            JNIEnv* env = JNIWrapper::getEnvironment();
            if(info->type == JNIObjectType::kPersistent || info->type == JNIObjectType::kAbstract) {
                auto handleFieldId = info->fieldMap.at("nativeHandle");
                if(!handleFieldId) {
                    env->ExceptionClear();
                    return nullptr;
                }
                jlong handle = env->GetLongField(object, handleFieldId);
                // If object type did not match, the field access might fail => check for java exceptions
                if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                    return nullptr;
                }
                jniObject = reinterpret_cast<JNIObject*>(handle);
                // now check if this object is an instance of `ObjectType`
                JNIClassInfo *info2 = jniObject->_jniClassInfo;
                while(info2 != info) {
                    info2 = info2->baseClassInfo;
                    if(!info2) {
                        return nullptr;
                    }
                }
            } else {
                jniObject = new ObjectType(object, info);
            }
            // obtained casted shared pointer will share the reference count and deallocator method
            // with the original!
            return std::static_pointer_cast<ObjectType>(jniObject->getSharedPtr());
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
    static void initializeNativeObject(jobject object);

    /**
     * internal utility method; should not be called manually
     * used to refresh all references after classes where unloaded
     */
    static void reloadBindings();
private:
    // Factory method for creating objects
    static jobject _createObject(const std::string& canonicalName, const char* constructorAlias, va_list constructorArgs);
    static std::shared_ptr<JNIClass> _wrapClass(const std::string& canonicalName);

    static void _registerObject(JNIObjectType type, const std::string& canonicalName, const std::string& baseCanonicalName, ObjectInitializer i, ObjectConstructor c);

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
