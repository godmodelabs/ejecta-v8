//
// Created by Martin Kleinhans on 21.08.17.
//

#ifndef __JNIV8WRAPPER_H
#define __JNIV8WRAPPER_H

#include "JNIV8ClassInfo.h"
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
        return JNIBase::getCanonicalName<ObjectType>();
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
        _registerObject(type, JNIBase::getCanonicalName<ObjectType>(), JNIBase::getCanonicalName<JNIV8Object>(),
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
        _registerObject(type, JNIBase::getCanonicalName<ObjectType>(), JNIBase::getCanonicalName<BaseObjectType>(),
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
        _registerObject(type, canonicalName, JNIBase::getCanonicalName<ObjectType>(), nullptr, nullptr, 0);
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
    JNILocalRef<ObjectType> createObject(const char *constructorAlias = nullptr, ...) {
        va_list args;
        va_start(args, constructorAlias);
        JNILocalRef<ObjectType> ptr = JNIWrapper::createObject<ObjectType>(constructorAlias, args);
        va_end(args);
        return ptr;
    }

    template <typename ObjectType> static
    JNILocalRef<ObjectType> createDerivedObject(const std::string &canonicalName, const char *constructorAlias = nullptr, ...) {
        va_list args;
        va_start(args, constructorAlias);
        JNILocalRef<ObjectType> ptr = JNIWrapper::createDerivedObject<ObjectType>(canonicalName, constructorAlias, args);
        va_end(args);
        return ptr;
    }

    /**
     * wraps a V8 enabled java object
     */
    template <typename ObjectType> static
    JNILocalRef<ObjectType> wrapObject(jobject object) {
        return JNIWrapper::wrapObject<ObjectType>(object);
    };

    /**
     * wraps a js object backed by a V8 enabled java object
     */
    template <typename ObjectType> static
    JNILocalRef<ObjectType> wrapObject(v8::Local<v8::Object> object) {
        auto it = _objmap.find(JNIBase::getCanonicalName<ObjectType>());
        if (it == _objmap.end()){
            return nullptr;
        }

        JNIV8Object* ptr;
        v8::Local<v8::External> ext;

        v8::Isolate* isolate = v8::Isolate::GetCurrent();
        // because this method takes a local, we can be sure that the correct v8 scopes are active
        // we still need a handle scope however...
        v8::HandleScope scope(isolate);

        JNIV8ClassInfoContainer *info = it->second;

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
                auto sharedPtr = info->creator(_getV8ClassInfo(JNIBase::getCanonicalName<ObjectType>(), BGJSV8Engine::GetInstance(isolate)), persistent);

                // store in private
                object->SetPrivate(isolate->GetCurrentContext(), privateKey, v8::External::New(isolate, sharedPtr.get()));

                return std::static_pointer_cast<ObjectType>(sharedPtr);
            }
             */
            // a new wrapper object is created every time!
            v8::Persistent<v8::Object>* persistent = new v8::Persistent<v8::Object>(isolate, object);
            JNIEnv *env = JNIWrapper::getEnvironment();
            jobjectArray arguments = env->NewObjectArray(0, _jniObject.clazz, nullptr);
            // __android_log_print(ANDROID_LOG_WARN, "JNIV8Wrapper", "Creating %s", JNIBase::getCanonicalName<ObjectType>().c_str());
            auto localRef = JNILocalRef<ObjectType>::Cast(info->creator(_getV8ClassInfo(JNIBase::getCanonicalName<ObjectType>(), BGJSV8Engine::GetInstance(isolate)), persistent, arguments));
            env->DeleteLocalRef(arguments);
            return localRef;
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
        return JNILocalRef<ObjectType>(reinterpret_cast<ObjectType*>(ptr));
    };

    /**
     * retrieves the JNIV8ClassInfo for a native class in the specified engine
     */
    template <typename ObjectType> static
    v8::Local<v8::Function> getJSConstructor(BGJSV8Engine *engine) {
        return _getV8ClassInfo(JNIBase::getCanonicalName<ObjectType>(), engine)->getConstructor();
    }
    static v8::Local<v8::Function> getJSConstructor(BGJSV8Engine *engine, const std::string &canonicalName) {
        return _getV8ClassInfo(canonicalName, engine)->getConstructor();
    }

    /**
     * internal utility method; should not be called manually!
     * instead you should use:
     * - createObject<NativeType>() if you want to create a new V8 enabled Java+Native object tuple
     */
    static void initializeNativeJNIV8Object(jobject obj, jobject engineObj, jlong jsObjPtr);

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
    static JNIV8ClassInfo* _getV8ClassInfo(const std::string& canonicalName, BGJSV8Engine *engine);

    static std::map<std::string, JNIV8ClassInfoContainer*> _objmap;

    static pthread_mutex_t _mutexEnv;

    template<class ObjectType>
    static void initialize(JNIV8ClassInfo *info) {
        ObjectType::initializeV8Bindings(info);
    }

    template<class ObjectType>
    static JNILocalRef<JNIV8Object> createJavaClass(JNIV8ClassInfo *info, v8::Persistent<v8::Object> *jsObj, jobjectArray arguments) {
        jobject engineObj = info->engine->getJObject();
        JNILocalRef<JNIV8Object> ref = JNILocalRef<JNIV8Object>::Cast(JNIV8Wrapper::createDerivedObject<ObjectType>(info->container->canonicalName, "<JNIV8ObjectInit>", engineObj, (jlong)(void*)jsObj, arguments));
        JNIWrapper::getEnvironment()->DeleteLocalRef(engineObj);
        return ref;
    }
    
    // cache of classes + ids
    static struct {
        jclass clazz;
    } _jniObject;
    static struct {
        jclass clazz;
        jfieldID propertyId;
        jfieldID methodId;
        jfieldID isStaticId;
        jfieldID returnTypeId;
        jfieldID argumentsId;
    } _jniV8FunctionInfo;
    static struct {
        jclass clazz;
        jfieldID typeId;
        jfieldID isNullableId;
        jfieldID undefinedIsNullId;
    } _jniV8FunctionArgumentInfo;
    static struct {
        jclass clazz;
        jfieldID propertyId;
        jfieldID getterId;
        jfieldID setterId;
        jfieldID isStaticId;
        jfieldID isNullableId;
        jfieldID undefinedIsNullId;
        jfieldID typeId;
    } _jniV8AccessorInfo;
};

template <> JNILocalRef<JNIV8Object> JNIV8Wrapper::wrapObject<JNIV8Object>(v8::Local<v8::Object> object);

#endif //__JNIV8WRAPPER_H
