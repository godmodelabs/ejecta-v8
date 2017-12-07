//
// Created by Martin Kleinhans on 20.04.17.
//

#ifndef __JNIWRAPPER_H
#define __JNIWRAPPER_H

#import <vector>
#import <string>
#import <map>
#include <jni.h>
#include "jni_assert.h"
#include <unistd.h>
#include <pthread.h>

#include "JNIClassInfo.h"
#include "JNIClass.h"
#include "JNIObject.h"

class JNIWrapper {
public:
    static void init(JavaVM *vm);
    static JNIEnv* getEnvironment();
    static bool isInitialized();

    /**
     * checks if the objects referenced java clas is an instance of the provided classname
     */
    static bool isObjectInstanceOf(JNIObject *obj, const std::string &canonicalName);
    template<class ObjectType> static bool isObjectInstanceOf(JNIObject *obj) {
        return isObjectInstanceOf(obj, JNIBase::getCanonicalName<ObjectType>());
    }

    /**
     * returns the java signature of the specified native object type
     */
    template<class ObjectType> static
    const std::string getSignature() {
        return "L" + JNIBase::getCanonicalName<ObjectType>() + ";";
    };

    /**
     * register a Java+Native object tuple
     * E.g. if there is a Java class "MyObject" with native class "MyObjectNative" you would call
     * registerObject<MyObjectNative>()
     */
    template<class ObjectType> static
    void registerObject(JNIObjectType type = JNIObjectType::kPersistent) {
        _registerObject(typeid(ObjectType).hash_code(), type, JNIBase::getCanonicalName<ObjectType>(), JNIBase::getCanonicalName<JNIObject>(),
                        initialize<ObjectType>, type != JNIObjectType::kTemporary ? instantiate<ObjectType> : nullptr);
    };

    /**
     * register a Java+Native object tuple extending another Java+Native object tuple
     * E.g. if there is a Java class "MySubclass" with native class "MySubclassNative" that extends
     * "MyObject" with native class "MyObjectNative" you would call
     * registerObject<MySubclassNative, MyObjectNative>()
     */
    template<class ObjectType, class BaseObjectType> static
    void registerObject(JNIObjectType type = JNIObjectType::kPersistent) {
        _registerObject(typeid(ObjectType).hash_code(), type, JNIBase::getCanonicalName<ObjectType>(), JNIBase::getCanonicalName<BaseObjectType>(),
                        initialize<ObjectType>, type != JNIObjectType::kTemporary ? instantiate<ObjectType> : nullptr);
    };

    /**
     * register a pure Java class extending a Java+Native tuple
     * E.g. if there is a pure Java class "MyJavaSubclass" that extends the java class "MyObject" with native class "MyObjectNative" you would call
     * registerJavaObject<MyObjectNative>("com/example/MyJavaSubclass")
     */
    template<class ObjectType> static
    void registerJavaObject(const std::string &canonicalName, JNIObjectType type = JNIObjectType::kPersistent) {
        _registerObject(typeid(void).hash_code(), type, canonicalName, JNIBase::getCanonicalName<ObjectType>(), nullptr, nullptr);
    };
    /**
     * this overload is primarily used for registering java classes directly from java where the template version above can not be used
     * if possible use the template method instead.
     * The base class MUST have been registered as a Java+Native tuple previously!
     */
    static
    void registerJavaObject(const std::string &canonicalName, const std::string &baseCanonicalName, JNIObjectType type = JNIObjectType::kPersistent) {
        _registerObject(typeid(void).hash_code(), type, canonicalName, baseCanonicalName, nullptr, nullptr);
    };

    /**
     * creates a Java+Native Object tuple based on the specified object type
     * prerequisites:
     * - object needs to have been registered before with JNIWrapper::registerObject<ObjectType>() (or another overload) and BGJS_JNIJNIV8Object_LINK
     */
    template <typename ObjectType> static
    std::shared_ptr<ObjectType> createObject(const char *constructorAlias = nullptr, ...) {
        va_list args;
        va_start(args, constructorAlias);
        jobject obj = _createObject(JNIBase::getCanonicalName<ObjectType>(), constructorAlias, args);
        va_end(args);
        return JNIWrapper::wrapObject<ObjectType>(obj);
    }

    template <typename ObjectType> static
    std::shared_ptr<ObjectType> createObject(const char *constructorAlias, va_list args) {
        jobject obj = _createObject(JNIBase::getCanonicalName<ObjectType>(), constructorAlias, args);
        return JNIWrapper::wrapObject<ObjectType>(obj);
    }

    /**
     * creates a Java+Native Object tuple based on the specified object type and canonicalName
     *
     * the canonicalName must identify a java class that extends a baseclass linked to the specified objectType
     *
     * prerequisites:
     * - base class must have been registered before with JNIWrapper::registerObject<ObjectType>() (or another overload) and BGJS_JNIJNIV8Object_LINK
     * - derived class must have been registed with JNIWrapper::registerJavaObject<ObjectType>(canonicalName) (or another overload)
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
     * NOTE: returns nullptr for invalid/unknown objects!
     */
    template <typename ObjectType> static
    std::shared_ptr<ObjectType> wrapObject(jobject object) {
        auto it = _objmap.find(JNIBase::getCanonicalName<ObjectType>());
        if (!object || it == _objmap.end()){
            return nullptr;
        } else {
            JNIClassInfo *info = it->second;
            JNIObject *jniObject;
            JNIEnv* env = JNIWrapper::getEnvironment();
            if(info->type == JNIObjectType::kPersistent || info->type == JNIObjectType::kAbstract) {
                jlong handle = env->GetLongField(object, _jniNativeHandleFieldID);
                // If object type did not match, the field access might fail => check for java exceptions
                if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                    return nullptr;
                }
                // If handle was already disposed => cancel
                if(!handle) {
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
        return _wrapClass(JNIBase::getCanonicalName<ObjectType>());
    }


    /**
     * convert a jstring to a std::string
     */
    static std::string jstring2string(jstring string);
    /**
     * convert a std::string to a jstring
     */
    static jstring string2jstring(const std::string& string);

    /**
     * creates a native object based on the specified class name
     * internal utility method; should not be called manually!
     * instead you should use:
     * - createObject<NativeType>() if you want to create a new Java+Native object tuple
     */
    static void initializeNativeObject(jobject object, jstring canonicalName);
private:
    // Factory method for creating objects
    static jobject _createObject(const std::string& canonicalName, const char* constructorAlias, va_list constructorArgs);
    static std::shared_ptr<JNIClass> _wrapClass(const std::string& canonicalName);

    static void _registerObject(size_t hashCode, JNIObjectType type, const std::string& canonicalName, const std::string& baseCanonicalName, ObjectInitializer i, ObjectConstructor c);

    static pthread_mutex_t _mutexEnv;
    static JavaVM *_jniVM;
    static JNIEnv *_jniEnv;
    static pid_t _jniThreadId;
    static jfieldID _jniNativeHandleFieldID;

    static jclass _jniStringClass;
    static jmethodID _jniStringGetBytes;
    static jmethodID _jniStringInit;
    static jstring _jniCharsetName;

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

#endif //__JNIWRAPPER_H
