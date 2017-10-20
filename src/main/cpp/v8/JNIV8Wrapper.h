//
// Created by Martin Kleinhans on 21.08.17.
//

#ifndef __JNIV8WRAPPER_H
#define __JNIV8WRAPPER_H

#include "V8ClassInfo.h"
#include "../bgjs/BGJSV8Engine.h"

#include "../jni/jni.h"

#include "JNIV8Object.h"

class JNIV8Wrapper {
public:
    static void init();

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
     * register a Java+Native+js object tuple
     * E.g. if there is a Java class "MyObject" with native class "MyObjectNative" you would call
     * registerObject<MyObjectNative>()
     */
    template<class ObjectType> static
    void registerObject(JNIV8ObjectType type = JNIV8ObjectType::kPersistent) {
        JNIWrapper::registerObject<ObjectType, JNIV8Object>(type == JNIV8ObjectType::kAbstract ? JNIObjectType::kAbstract : JNIObjectType::kPersistent);
        _registerObject(type, JNIWrapper::getCanonicalName<ObjectType>(), JNIWrapper::getCanonicalName<JNIV8Object>(),
                        type == JNIV8ObjectType::kWrapper ? nullptr : initialize<ObjectType>, createJavaClass<ObjectType>, sizeof(ObjectType));
    };

    /**
     * register a Java+Native+js object tuple extending another Java+Native object tuple
     * E.g. if there is a Java class "MySubclass" with native class "MySubclassNative" that extends
     * "MyObject" with native class "MyObjectNative" you would call
     * registerObject<MySubclassNative, MyObjectNative>()
     */
    template<class ObjectType, class BaseObjectType> static
    void registerObject(JNIV8ObjectType type = JNIV8ObjectType::kPersistent) {
        JNIWrapper::registerObject<ObjectType, BaseObjectType>(type == JNIV8ObjectType::kAbstract ? JNIObjectType::kAbstract : JNIObjectType::kPersistent);
        _registerObject(type, JNIWrapper::getCanonicalName<ObjectType>(), JNIWrapper::getCanonicalName<BaseObjectType>(),
                        type == JNIV8ObjectType::kWrapper ? nullptr : initialize<ObjectType>, createJavaClass<ObjectType>, sizeof(ObjectType));
    };

    /**
     * register a pure Java class extending a Java+Native+js tuple
     * E.g. if there is a pure Java class "MyJavaSubclass" that extends the java class "MyObject" with native class "MyObjectNative" you would call
     * registerJavaObject<MyObjectNative>("com/example/MyJavaSubclass")
     */
    template<class ObjectType> static
    void registerJavaObject(const std::string &canonicalName, JNIV8ObjectType type = JNIV8ObjectType::kPersistent) {
        JNIWrapper::registerJavaObject<ObjectType>(canonicalName, type == JNIV8ObjectType::kAbstract ? JNIObjectType::kAbstract : JNIObjectType::kPersistent);
        _registerObject(type, canonicalName, JNIWrapper::getCanonicalName<ObjectType>(), nullptr, nullptr, 0);
    };
    /**
     * this overload is primarily used for registering java classes directly from java where the template version above can not be used
     * if possible use the template method instead.
     * The base class MUST have been registered as a Java+Native tuple previously!
     */
    static
    void registerJavaObject(const std::string &canonicalName, const std::string &baseCanonicalName, JNIV8ObjectType type = JNIV8ObjectType::kPersistent) {
        JNIWrapper::registerJavaObject(canonicalName, baseCanonicalName, type == JNIV8ObjectType::kAbstract ? JNIObjectType::kAbstract : JNIObjectType::kPersistent);
        _registerObject(type, canonicalName, baseCanonicalName, nullptr, nullptr, 0);
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

    template <typename ObjectType> static
    std::shared_ptr<ObjectType> createDerivedObject(const std::string &canonicalName, const char *constructorAlias = nullptr, ...) {
        va_list args;
        va_start(args, constructorAlias);
        std::shared_ptr<ObjectType> ptr = JNIWrapper::createDerivedObject<ObjectType>(canonicalName, constructorAlias, args);
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
     * wraps a js object backed by a V8 enabled java object
     */
    template <typename ObjectType> static
    std::shared_ptr<ObjectType> wrapObject(v8::Local<v8::Object> object) {
        auto it = _objmap.find(JNIWrapper::getCanonicalName<ObjectType>());
        if (it == _objmap.end()){
            return nullptr;
        }

        JNIV8Object* ptr;
        v8::Local<v8::External> ext;

        v8::Isolate* isolate = v8::Isolate::GetCurrent();
        // because this method takes a local, we can be sure that the correct v8 scopes are active
        // we still need a handle scope however...
        v8::HandleScope scope(isolate);

        V8ClassInfoContainer *info = it->second;

        if(info->type == JNIV8ObjectType::kWrapper) {
            /*
            // does the object have a native object stored in a private key?
            auto privateKey = v8::Private::ForApi(isolate, v8::String::NewFromUtf8(isolate, _v8PrivateKey));
            auto privateValue = object->GetPrivate(isolate->GetCurrentContext(), privateKey);
            if (!privateValue.IsEmpty()) {
                ext = privateValue.ToLocalChecked().As<v8::External>();
            } else {
                // the private key was empty so we have to create a new object
                v8::Persistent<v8::Object>* persistent = new v8::Persistent<v8::Object>(isolate, object);
                auto sharedPtr = info->creator(_getV8ClassInfo(JNIWrapper::getCanonicalName<ObjectType>(), BGJS_CURRENT_V8ENGINE(isolate)), persistent);

                // store in private
                object->SetPrivate(isolate->GetCurrentContext(), privateKey, v8::External::New(isolate, sharedPtr.get()));

                return std::static_pointer_cast<ObjectType>(sharedPtr);
            }
             */
            // a new wrapper object is created every time!
            v8::Persistent<v8::Object>* persistent = new v8::Persistent<v8::Object>(isolate, object);
            auto sharedPtr = info->creator(_getV8ClassInfo(JNIWrapper::getCanonicalName<ObjectType>(), BGJS_CURRENT_V8ENGINE(isolate)), persistent);
            return std::static_pointer_cast<ObjectType>(sharedPtr);
        } else {
            if (object->InternalFieldCount() >= 1) {
                // does the object have internal fields? if so use it!
                ext = object->GetInternalField(0).As<v8::External>();
            } else {
                return nullptr;
            }
        }

        ptr = reinterpret_cast<JNIV8Object*>(ext->Value());
        if(!JNIWrapper::isObjectInstanceOf<ObjectType>(ptr)) {
            return nullptr;
        }
        return std::static_pointer_cast<ObjectType>(ptr->getSharedPtr());
    };

    /**
     * retrieves the V8ClassInfo for a native class in the specified engine
     */
    template <typename ObjectType> static
    v8::Local<v8::Function> getJSConstructor(BGJSV8Engine *engine) {
        return _getV8ClassInfo(JNIWrapper::getCanonicalName<ObjectType>(), engine)->getConstructor();
    }
    static v8::Local<v8::Function> getJSConstructor(BGJSV8Engine *engine, const std::string &canonicalName) {
        return _getV8ClassInfo(canonicalName, engine)->getConstructor();
    }

    /**
     * convert a v8 value to an instance of Object
     */
    static jobject v8value2jobject(v8::Local<v8::Value> valueRef);

    /**
     * convert an instance of Object to a v8value
     */
    static v8::Local<v8::Value> jobject2v8value(jobject object);

    /**
     * convert a jstring to a std::string
     */
    static v8::Local<v8::String> jstring2v8string(jstring string);
    /**
     * convert a std::string to a jstring
     */
    static jstring v8string2jstring(v8::Local<v8::String> string);

    /**
     * return an object representing undefined in java
     */
    static jobject undefinedInJava();

    /**
     * internal utility method; should not be called manually!
     * instead you should use:
     * - createObject<NativeType>() if you want to create a new V8 enabled Java+Native object tuple
     */
    static void initializeNativeJNIV8Object(jobject obj, jlong enginePtr, jlong jsObjPtr);

    /**
     * internal constructor callback for objects created from javascript
     */
    static void v8ConstructorCallback(const v8::FunctionCallbackInfo<v8::Value>& args);

    /**
     * internal helper function called by V8Engine on destruction
     */
    static void cleanupV8Engine(BGJSV8Engine *engine);
private:
    //static const char* _v8PrivateKey;

    static void _registerObject(JNIV8ObjectType type, const std::string& canonicalName, const std::string& baseCanonicalName, JNIV8ObjectInitializer i, JNIV8ObjectCreator c, size_t size);
    static V8ClassInfo* _getV8ClassInfo(const std::string& canonicalName, BGJSV8Engine *engine);

    static std::map<std::string, V8ClassInfoContainer*> _objmap;

    static jobject _undefined;

    template<class ObjectType>
    static void initialize(V8ClassInfo *info) {
        ObjectType::initializeV8Bindings(info);
    }

    template<class ObjectType>
    static std::shared_ptr<JNIV8Object> createJavaClass(V8ClassInfo *info, v8::Persistent<v8::Object> *jsObj) {
        std::shared_ptr<ObjectType> ptr = JNIV8Wrapper::createDerivedObject<ObjectType>(info->container->canonicalName, "<JNIV8ObjectInit>", info->engine->getJObject(), (jlong)(void*)jsObj);
        return ptr;
    }
    
    // cache of classes + ids
    static struct {
        jclass clazz;
        jfieldID propertyId;
        jfieldID methodId;
        jfieldID isStaticId;
    } _jniV8FunctionInfo;
    static struct {
        jclass clazz;
        jfieldID propertyId;
        jfieldID getterId;
        jfieldID setterId;
        jfieldID isStaticId;
    } _jniV8AccessorInfo;
    static struct {
        jclass clazz;
        jmethodID valueOfId;
    } _jniDouble;
    static struct {
        jclass clazz;
        jmethodID valueOfId;
        jmethodID booleanValueId;
    } _jniBoolean;
    static struct {
        jclass clazz;
    } _jniString;
    static struct {
        jclass clazz;
        jmethodID charValueId;
    } _jniCharacter;
    static struct {
        jclass clazz;
        jmethodID doubleValueId;
    } _jniNumber;
    static struct {
        jclass clazz;
    } _jniV8Object;
};

template <> std::shared_ptr<JNIV8Object> JNIV8Wrapper::wrapObject<JNIV8Object>(v8::Local<v8::Object> object);

/**
 * macro to register native class with JNI
 * specify the native class, and the full canonical name of the associated v8 enabled java class
 */
#define BGJS_JNIV8OBJECT_LINK(type, canonicalName) BGJS_JNIOBJECT_LINK(type, canonicalName)
#define BGJS_JNIV8OBJECT_DEF(type) template<> const std::string JNIWrapper::getCanonicalName<type>();

#endif //__JNIV8WRAPPER_H
