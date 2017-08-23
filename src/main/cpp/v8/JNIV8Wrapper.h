//
// Created by Martin Kleinhans on 21.08.17.
//

#ifndef __JNIV8WRAPPER_H
#define __JNIV8WRAPPER_H

#include "V8ClassInfo.h"
#include "../bgjs/BGJSV8Engine.h"

#include "../jni/JNIWrapper.h"

#include "V8Object.h"

class JNIV8Wrapper {
public:
    /**
     * returns the canonical name of the v8 enabled java class associated with the specified native object
     */
    template <typename ObjectType> static
    const std::string getCanonicalName() {
        return JNIWrapper::getCanonicalName<ObjectType>();
    }

    /**
     * returns the java signature of the specified native object type
     */
    template<class ObjectType> static
    const std::string getSignature() {
        return JNIWrapper::getSignature<ObjectType>();
    };

    /**
     * internal struct used for registering a class with the factory
     */
    template<class ObjectType> static
    void registerObject() {
        JNIWrapper::registerObject<ObjectType>(true);
        _registerObject(JNIWrapper::getCanonicalName<ObjectType>(), initialize<ObjectType>, createJavaClass<ObjectType>);
    };

    /**
     * creates a V8 enabled Java+Native Object tuple based on the specified object type
     * object needs to have been registered before with BGJS_REGISTER_OBJECT
     */
    template <typename ObjectType> static
    std::shared_ptr<ObjectType> createObject(const char *constructorAlias = nullptr, ...) {
        va_list args;
        va_start(args, constructorAlias);
        std::shared_ptr<ObjectType> ptr = JNIWrapper::createObject<ObjectType>(constructorAlias, args);
        va_end(args);
        return ptr;
    }

    /**
     * wraps a V8 enabled java object
     */
    template <typename ObjectType> static
    std::shared_ptr<ObjectType> wrapObject(jobject object) {
        return JNIWrapper::wrapObject<ObjectType>(object);
    };

    /**
     * retrieves the V8ClassInfo for a native class in the specified engine
     */
    template <typename ObjectType> static
    V8ClassInfo* getV8ClassInfo(BGJSV8Engine *engine) {
        return _getV8ClassInfo(JNIWrapper::getCanonicalName<ObjectType>(), engine);
    }

    /**
     * internal utility method; should not be called manually!
     * instead you should use:
     * - createObject<NativeType>() if you want to create a new V8 enabled Java+Native object tuple
     */
    static void initializeNativeV8Object(jobject obj, jlong enginePtr, jlong jsObjPtr);

    /**
     * internal utility method; should not be called manually
     * used to refresh all references after classes where unloaded
     */
    static void reloadBindings();

    /**
     * internal constructor callback for objects created from javascript
     */
    static void v8ConstructorCallback(const v8::FunctionCallbackInfo<v8::Value>& args);

private:
    static void _registerObject(const std::string& canonicalName, V8ObjectInitializer i, V8ObjectCreator c);
    static V8ClassInfo* _getV8ClassInfo(const std::string& canonicalName, BGJSV8Engine *engine);

    static std::map<std::string, V8ClassInfoContainer*> _objmap;

    static jfieldID _nativeHandleFieldId;

    template<class ObjectType>
    static void initialize(V8ClassInfo *info) {
        ObjectType::initializeV8Bindings(info);
    }

    template<class ObjectType>
    static std::shared_ptr<V8Object> createJavaClass(V8ClassInfo *info, v8::Persistent<v8::Object> *jsObj) {
        std::shared_ptr<ObjectType> ptr = JNIV8Wrapper::createObject<ObjectType>("<V8ObjectInit>", info->engine->getJObject(), (jlong)(void*)jsObj);
        return ptr;
    }
};

/**
 * macro to generate constructors for objects derived from JNIObject
 */
#define BGJS_JNIV8OBJECT_CONSTRUCTOR(type) type(jobject obj, JNIClassInfo *info) : V8Object(obj, info) {};

/**
 * macro to register native class with JNI
 * specify the native class, and the full canonical name of the associated v8 enabled java class
 */
#define BGJS_JNIV8OBJECT_LINK(type, canonicalName) BGJS_JNIOBJECT_LINK(type, canonicalName)


#endif //__JNIV8WRAPPER_H
