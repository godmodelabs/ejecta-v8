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
#include "JNIScope.h"
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
        // @TODO: wrong syntax. throw a real exception object here? or maybe something like an assert that turns up in the log
        throw "JNIWrapper::getCanonicalName called for unregistered class";
    }

    /**
     * checks if the objects referenced java clas is an instance of the provided classname
     */
    static bool isObjectInstanceOf(JNIObject *obj, const std::string &canonicalName);
    template<class ObjectType> static bool isObjectInstanceOf(JNIObject *obj) {
        return isObjectInstanceOf(obj, getCanonicalName<ObjectType>());
    }

    /**
     * returns the java signature of the specified native object type
     */
    template<class ObjectType> static
    const std::string getSignature() {
        return "L" + JNIWrapper::getCanonicalName<ObjectType>() + ";";
    };

    /**
     * register a Java+Native object tuple
     * E.g. if there is a Java class "MyObject" with native class "MyObjectNative" you would call
     * registerObject<MyObjectNative>()
     */
    template<class ObjectType> static
    void registerObject(JNIObjectType type = JNIObjectType::kPersistent) {
        _registerObject(typeid(ObjectType).hash_code(), type, JNIWrapper::getCanonicalName<ObjectType>(), JNIWrapper::getCanonicalName<JNIObject>(),
                        initialize<ObjectType>, type == JNIObjectType::kPersistent ? instantiate<ObjectType> : nullptr);
    };

    /**
     * register a Java+Native object tuple extending another Java+Native object tuple
     * E.g. if there is a Java class "MySubclass" with native class "MySubclassNative" that extends
     * "MyObject" with native class "MyObjectNative" you would call
     * registerObject<MySubclassNative, MyObjectNative>()
     */
    template<class ObjectType, class BaseObjectType> static
    void registerObject(JNIObjectType type = JNIObjectType::kPersistent) {
        _registerObject(typeid(ObjectType).hash_code(), type, JNIWrapper::getCanonicalName<ObjectType>(), JNIWrapper::getCanonicalName<BaseObjectType>(),
                        initialize<ObjectType>, type == JNIObjectType::kPersistent ? instantiate<ObjectType> : nullptr);
    };

    /**
     * register a Java+Native object tuple extending a pure Java class that in turn extends a Java+Native tuple
     * E.g. if there is a Java class "MySubclass" with native class "MySubclassNative" that extends
     * the java class "MyJavaSubclass" which in turn extends the java class "MyObject" with native class "MyObjectNative" you would call
     * registerObject<MySubclassNative>("com/example/MyJavaSubclass")
     * Note: The native object MUST extend the next native object up the inheritance chain! In the example above this would be "MyObjectNative"
     */
    template<class ObjectType> static
    void registerObject(const std::string &baseCanonicalName, JNIObjectType type = JNIObjectType::kPersistent) {
        _registerObject(typeid(ObjectType).hash_code(), type, JNIWrapper::getCanonicalName<ObjectType>(), baseCanonicalName,
                        initialize<ObjectType>, type == JNIObjectType::kPersistent ? instantiate<ObjectType> : nullptr);
    };

    /**
     * register a pure Java class extending a Java+Native tuple
     * E.g. if there is a pure Java class "MyJavaSubclass" that extends the java class "MyObject" with native class "MyObjectNative" you would call
     * registerJavaObject<MyObjectNative>("com/example/MyJavaSubclass")
     */
    template<class ObjectType> static
    void registerJavaObject(const std::string &canonicalName, JNIObjectType type = JNIObjectType::kPersistent) {
        _registerObject(typeid(void).hash_code(), type, canonicalName, JNIWrapper::getCanonicalName<ObjectType>(), nullptr, nullptr);
    };

    /**
     * register a pure Java class extending another pure Java class that (not necessarily directly) extends a Java+native tuple
     * E.g. if there is a pure Java class "MyOtherJavaSubclass" that extends the pure java class "MyJavaSubclass" you would call
     * registerJavaObject("com/example/MyOtherJavaSubclass","com/example/MyJavaSubclass")
     * Note: somwhere up the inheritance chain "MyJavaSubclass" needs to extend a native class in this example!
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
                auto handleField = info->fieldMap.find("nativeHandle");
                if(handleField == info->fieldMap.end()) {
                    env->ExceptionClear();
                    return nullptr;
                }
                jlong handle = env->GetLongField(object, handleField->second.id);
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

    static void _registerObject(size_t hashCode, JNIObjectType type, const std::string& canonicalName, const std::string& baseCanonicalName, ObjectInitializer i, ObjectConstructor c);

    static void _reloadBinding(std::map<std::string, bool>& alreadyConverted, JNIClassInfo *info);

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
 * macro to register a native class with JNI
 * specify the native class, and the full canonical name of the associated java class
 */
#define BGJS_JNIOBJECT_LINK(type, canonicalName) template<> const std::string JNIWrapper::getCanonicalName<type>() { return canonicalName; };
#define BGJS_JNIOBJECT_DEF(type) template<> const std::string JNIWrapper::getCanonicalName<type>();

#endif //__JNIWRAPPER_H
