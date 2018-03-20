//
// Created by Martin Kleinhans on 21.08.17.
//

#include "../bgjs/BGJSV8Engine.h"
using namespace v8;

#include "JNIV8Wrapper.h"
#include "JNIV8Array.h"
#include "JNIV8GenericObject.h"
#include "JNIV8Function.h"
#include "v8.h"

#include <string>
#include <algorithm>

std::map<std::string, JNIV8ClassInfoContainer*> JNIV8Wrapper::_objmap;

//const char* JNIV8Wrapper::_v8PrivateKey = "JNIV8WrapperPrivate";

decltype(JNIV8Wrapper::_jniObject) JNIV8Wrapper::_jniObject = {0};
decltype(JNIV8Wrapper::_jniV8FunctionInfo) JNIV8Wrapper::_jniV8FunctionInfo = {0};
decltype(JNIV8Wrapper::_jniV8AccessorInfo) JNIV8Wrapper::_jniV8AccessorInfo = {0};
decltype(JNIV8Wrapper::_jniV8FunctionArgumentInfo) JNIV8Wrapper::_jniV8FunctionArgumentInfo = {0};

pthread_mutex_t JNIV8Wrapper::_mutexEnv;

void JNIV8Wrapper::init() {
    pthread_mutexattr_t Attr;
    pthread_mutexattr_init(&Attr);
    pthread_mutexattr_settype(&Attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&_mutexEnv, &Attr);

    JNIWrapper::registerObject<JNIV8Object>(JNIObjectType::kAbstract);
    _registerObject(JNIV8ObjectType::kAbstract, JNIBase::getCanonicalName<JNIV8Object>(), "",
                    nullptr, createJavaClass<JNIV8Object>, sizeof(JNIV8Object));

    JNIV8Wrapper::registerObject<JNIV8Array>(JNIV8ObjectType::kWrapper);
    JNIV8Wrapper::registerObject<JNIV8GenericObject>(JNIV8ObjectType::kWrapper);
    JNIV8Wrapper::registerObject<JNIV8Function>(JNIV8ObjectType::kWrapper);

    JNIEnv *env = JNIWrapper::getEnvironment();

    _jniObject.clazz = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Object"));

    _jniV8FunctionInfo.clazz = (jclass)env->NewGlobalRef(env->FindClass("ag/boersego/v8annotations/generated/V8FunctionInfo"));
    _jniV8FunctionInfo.propertyId = env->GetFieldID(_jniV8FunctionInfo.clazz, "property", "Ljava/lang/String;");
    _jniV8FunctionInfo.methodId = env->GetFieldID(_jniV8FunctionInfo.clazz, "method", "Ljava/lang/String;");
    _jniV8FunctionInfo.isStaticId = env->GetFieldID(_jniV8FunctionInfo.clazz, "isStatic", "Z");
    _jniV8FunctionInfo.returnTypeId = env->GetFieldID(_jniV8FunctionInfo.clazz, "returnType", "Ljava/lang/String;");
    _jniV8FunctionInfo.argumentsId = env->GetFieldID(_jniV8FunctionInfo.clazz, "arguments", "[Lag/boersego/v8annotations/generated/V8FunctionInfo$V8FunctionArgumentInfo;");

    _jniV8FunctionArgumentInfo.clazz = (jclass)env->NewGlobalRef(env->FindClass("ag/boersego/v8annotations/generated/V8FunctionInfo$V8FunctionArgumentInfo"));
    _jniV8FunctionArgumentInfo.typeId = env->GetFieldID(_jniV8FunctionArgumentInfo.clazz, "type", "Ljava/lang/String;");
    _jniV8FunctionArgumentInfo.isNullableId = env->GetFieldID(_jniV8FunctionArgumentInfo.clazz, "isNullable", "Z");
    _jniV8FunctionArgumentInfo.undefinedIsNullId = env->GetFieldID(_jniV8FunctionArgumentInfo.clazz, "undefinedIsNull", "Z");

    _jniV8AccessorInfo.clazz = (jclass)env->NewGlobalRef(env->FindClass("ag/boersego/v8annotations/generated/V8AccessorInfo"));
    _jniV8AccessorInfo.propertyId = env->GetFieldID(_jniV8AccessorInfo.clazz, "property", "Ljava/lang/String;");
    _jniV8AccessorInfo.getterId = env->GetFieldID(_jniV8AccessorInfo.clazz, "getter", "Ljava/lang/String;");
    _jniV8AccessorInfo.setterId = env->GetFieldID(_jniV8AccessorInfo.clazz, "setter", "Ljava/lang/String;");
    _jniV8AccessorInfo.typeId = env->GetFieldID(_jniV8AccessorInfo.clazz, "type", "Ljava/lang/String;");
    _jniV8AccessorInfo.isStaticId = env->GetFieldID(_jniV8AccessorInfo.clazz, "isStatic", "Z");
    _jniV8AccessorInfo.isNullableId = env->GetFieldID(_jniV8AccessorInfo.clazz, "isNullable", "Z");
    _jniV8AccessorInfo.undefinedIsNullId = env->GetFieldID(_jniV8AccessorInfo.clazz, "undefinedIsNull", "Z");

    JNIV8Object::initJNICache();
    JNIV8Function::initJNICache();
    JNIV8Array::initJNICache();
    JNIV8ClassInfo::initJNICache();
    JNIV8Marshalling::initJNICache();
    BGJSV8Engine::initJNICache();
}

void JNIV8Wrapper::v8ConstructorCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Local<v8::External> ext = args.Data().As<v8::External>();
    JNIV8ClassInfo* info = static_cast<JNIV8ClassInfo*>(ext->Value());

    v8::Isolate* isolate = info->engine->getIsolate();
    HandleScope scope(isolate);

    // this method must be called as constructor
    if(!args.IsConstructCall()) {
        args.GetIsolate()->ThrowException(v8::Exception::TypeError(String::NewFromUtf8(isolate, "Please use the 'new' operator, this object constructor cannot be called as a function")));
        return;
    }

    // check if class can be created from JS
    if(info->createFromJavaOnly) {
        args.GetIsolate()->ThrowException(v8::Exception::TypeError(String::NewFromUtf8(isolate, "Illegal constructor")));
        return;
    }

    // check that `this` has expected type
    JNI_ASSERT(Local<FunctionTemplate>::New(isolate, info->functionTemplate)->HasInstance(args.This()), "created object has unexpected class");

    // convert arguments
    jobjectArray arguments = nullptr;
    jobject value;
    int numArgs = args.Length();

    JNIEnv *env = JNIWrapper::getEnvironment();

    arguments = env->NewObjectArray(numArgs, _jniObject.clazz, nullptr);
    for (int i = 0, n = numArgs; i < n; i++) {
        value = JNIV8Marshalling::v8value2jobject(args[i]);
        env->SetObjectArrayElement(arguments, i, value);
        env->DeleteLocalRef(value);
    }

    // create temporary persistent for the js object and then call the constructor
    v8::Persistent<Object>* jsObj = new v8::Persistent<v8::Object>(isolate, args.This());
    auto ptr = info->container->creator(info, jsObj, arguments);

    env->DeleteLocalRef(arguments);

    // also forward arguments to optional native constructor handler (if one was registered)
    if(info->constructorCallback) {
        (ptr.get()->*(info->constructorCallback))(args);
    }
}

JNIV8ClassInfo* JNIV8Wrapper::_getV8ClassInfo(const std::string& canonicalName, BGJSV8Engine *engine) {
    pthread_mutex_lock(&_mutexEnv);

    // find class info container
    auto it = _objmap.find(canonicalName);
    JNI_ASSERTF(it != _objmap.end(), "Attempt to retrieve class info for unregistered class: %s", canonicalName.c_str());

    // check if class info object already exists for this engine!
    for(auto &it2 : it->second->classInfos) {
        if(it2->engine == engine) {
            pthread_mutex_unlock(&_mutexEnv);
            return it2;
        }
    }
    // if it was not found we have to create it now & link it with the container
    auto v8ClassInfo = new JNIV8ClassInfo(it->second, engine);
    it->second->classInfos.push_back(v8ClassInfo);

    // initialize class info: template with constructor and general setup created here
    // individual methods and accessors handled by static method on subclass
    v8::Isolate* isolate = engine->getIsolate();
    HandleScope scope(isolate);

    // v8 class name: canonical name with underscores instead of slashes
    // e.g. ag/boersego/bgjs/Test becomes ag_boersego_bgjs_Test
    std::string strV8ClassName = canonicalName;
    std::replace(strV8ClassName.begin(), strV8ClassName.end(), '/', '_');

    Local<External> data = External::New(isolate, (void*)v8ClassInfo);
    Handle<FunctionTemplate> ft = FunctionTemplate::New(isolate, v8ConstructorCallback, data);
    ft->SetClassName(String::NewFromUtf8(isolate, strV8ClassName.c_str()));

    // inherit from baseclass
    if(v8ClassInfo->container->baseClassInfo) {
        JNIV8ClassInfo *baseInfo = nullptr;
        // base classinfo might not have been initialized yet => do so now!
        _getV8ClassInfo(v8ClassInfo->container->baseClassInfo->canonicalName, engine);
        for(auto &it2 : v8ClassInfo->container->baseClassInfo->classInfos) {
            if(it2->engine == engine) {
                baseInfo = it2;
                break;
            }
        }
        JNI_ASSERT(baseInfo, "Failed to retrieve baseclass info");
        Local<FunctionTemplate> baseFT = Local<FunctionTemplate>::New(isolate, baseInfo->functionTemplate);
        ft->Inherit(baseFT);
    }

    Local<ObjectTemplate> tpl = ft->InstanceTemplate();
    tpl->SetInternalFieldCount(1);

    // store
    v8ClassInfo->functionTemplate.Reset(isolate, ft);

    // if this is a pure java class it might not have an initializer
    if(it->second->initializer) {
        it->second->initializer(v8ClassInfo);
    }

    // but it might have bindings on java that need to be processed
    // binding classes + methods do not need to be cached here, because they are only used once per Engine upon initialization!
    JNIEnv *env = JNIWrapper::getEnvironment();
    jclass clsObject = it->second->clsObject;
    jclass clsBinding = it->second->clsBinding;
    if(clsBinding && clsObject) {
        jfieldID createFromJavaOnlyId = env->GetStaticFieldID(clsBinding, "createFromJavaOnly", "Z");
        v8ClassInfo->createFromJavaOnly = env->GetStaticBooleanField(clsBinding, createFromJavaOnlyId);

        jmethodID getFunctionsMethodId = env->GetStaticMethodID(clsBinding, "getV8Functions",
                                                                "()[Lag/boersego/v8annotations/generated/V8FunctionInfo;");
        jmethodID getAccessorsMethodId = env->GetStaticMethodID(clsBinding, "getV8Accessors",
                                                                "()[Lag/boersego/v8annotations/generated/V8AccessorInfo;");

        // first register functions
        jobjectArray functionInfos = (jobjectArray) env->CallStaticObjectMethod(clsBinding,
                                                                                getFunctionsMethodId);
        for(jsize idx=0,n=env->GetArrayLength(functionInfos);idx<n;idx++) {
            jobject functionInfo = env->GetObjectArrayElement(functionInfos, idx);
            const std::string strFunctionName = JNIWrapper::jstring2string((jstring)env->GetObjectField(functionInfo, _jniV8FunctionInfo.propertyId));
            const std::string strMethodName = JNIWrapper::jstring2string((jstring)env->GetObjectField(functionInfo, _jniV8FunctionInfo.methodId));
            const std::string strReturnType = JNIWrapper::jstring2string((jstring)env->GetObjectField(functionInfo, _jniV8FunctionInfo.returnTypeId));
            jobjectArray argumentInfos = (jobjectArray)env->GetObjectField(functionInfo, _jniV8FunctionInfo.argumentsId);
            std::vector<JNIV8JavaValue>* arguments = nullptr;

            // return type information
            const JNIV8JavaValue returnType = JNIV8Marshalling::persistentValueWithTypeSignature(strReturnType);

            // build signature string and collect argument information
            std::string strSignature = "(";
            if(env->IsSameObject(argumentInfos, nullptr)) {
                strSignature = "([Ljava/lang/Object;)" + strReturnType;
            } else {
                arguments = new std::vector<JNIV8JavaValue>();
                jsize numArguments = env->GetArrayLength(argumentInfos);
                // collect arguments
                // ownership of malloc'ed memory is implicitly transferred to JNIV8ClassInfo!
                if(numArguments>0) {
                    for(jsize argIdx=0; argIdx<numArguments; argIdx++) {
                        const jobject argumentInfo = env->GetObjectArrayElement(argumentInfos, argIdx);
                        const std::string strArgumentType = JNIWrapper::jstring2string((jstring)env->GetObjectField(argumentInfo, _jniV8FunctionArgumentInfo.typeId));
                        JNIV8MarshallingFlags flags = JNIV8MarshallingFlags::kDefault;
                        if(!(bool)env->GetBooleanField(argumentInfo, _jniV8FunctionArgumentInfo.isNullableId)) flags = JNIV8MarshallingFlags::kNonNull;
                        if((bool)env->GetBooleanField(argumentInfo, _jniV8FunctionArgumentInfo.undefinedIsNullId)) flags = (JNIV8MarshallingFlags)(flags|JNIV8MarshallingFlags::kUndefinedIsNull);
                        JNIV8JavaValue argument = JNIV8Marshalling::persistentArgumentWithTypeSignature(strArgumentType, flags);
                        arguments->push_back(argument);
                        strSignature += strArgumentType;
                    }
                }
                strSignature += ")" + strReturnType;
            }

            // finally register the method
            jmethodID javaMethodId;
            if(env->GetBooleanField(functionInfo, _jniV8FunctionInfo.isStaticId)) {
                javaMethodId = env->GetStaticMethodID(clsObject, strMethodName.c_str(),
                                                strSignature.c_str());
                v8ClassInfo->registerStaticJavaMethod(strFunctionName, javaMethodId,
                                                      returnType,
                                                      arguments);
            } else {
                javaMethodId = env->GetMethodID(clsObject, strMethodName.c_str(),
                                                strSignature.c_str());
                v8ClassInfo->registerJavaMethod(strFunctionName, javaMethodId,
                                                returnType,
                                                arguments);
            }
        }

        // now register all property accessors
        jobjectArray accessorInfos = (jobjectArray) env->CallStaticObjectMethod(clsBinding,
                                                                                getAccessorsMethodId);
        for(jsize idx=0,n=env->GetArrayLength(accessorInfos);idx<n;idx++) {
            jobject accessorInfo = env->GetObjectArrayElement(accessorInfos, idx);
            const std::string strPropertyType = JNIWrapper::jstring2string((jstring)env->GetObjectField(accessorInfo, _jniV8AccessorInfo.typeId));
            JNIV8MarshallingFlags flags = JNIV8MarshallingFlags::kDefault;
            if(!(bool)env->GetBooleanField(accessorInfo, _jniV8AccessorInfo.isNullableId)) flags = JNIV8MarshallingFlags::kNonNull;
            if((bool)env->GetBooleanField(accessorInfo, _jniV8AccessorInfo.undefinedIsNullId)) flags = (JNIV8MarshallingFlags)(flags|JNIV8MarshallingFlags::kUndefinedIsNull);
            const JNIV8JavaValue property = JNIV8Marshalling::persistentArgumentWithTypeSignature(strPropertyType, flags);

            const std::string strPropertyName = JNIWrapper::jstring2string((jstring)env->GetObjectField(accessorInfo, _jniV8AccessorInfo.propertyId));
            const std::string strGetterName = JNIWrapper::jstring2string((jstring)env->GetObjectField(accessorInfo, _jniV8AccessorInfo.getterId));
            const std::string strSetterName = JNIWrapper::jstring2string((jstring)env->GetObjectField(accessorInfo, _jniV8AccessorInfo.setterId));
            jmethodID javaGetterId = NULL, javaSetterId = NULL;
            if(env->GetBooleanField(accessorInfo, _jniV8AccessorInfo.isStaticId)) {
                if (!strGetterName.empty()) { javaGetterId = env->GetStaticMethodID(clsObject, strGetterName.c_str(), ("()" + strPropertyType).c_str()); }
                if (!strSetterName.empty()) { javaSetterId = env->GetStaticMethodID(clsObject, strSetterName.c_str(), ("(" + strPropertyType + ")V").c_str()); }
                v8ClassInfo->registerStaticJavaAccessor(strPropertyName, property, javaGetterId, javaSetterId);
            } else {
                if (!strGetterName.empty()) { javaGetterId = env->GetMethodID(clsObject, strGetterName.c_str(), ("()" + strPropertyType).c_str()); }
                if (!strSetterName.empty()) { javaSetterId = env->GetMethodID(clsObject, strSetterName.c_str(), ("(" + strPropertyType + ")V").c_str()); }
                v8ClassInfo->registerJavaAccessor(strPropertyName, property, javaGetterId, javaSetterId);
            }
        }
    }

    // make sure that constructors exist!
#ifdef ENABLE_JNI_ASSERT
    jmethodID constructorId;
    if(!v8ClassInfo->createFromJavaOnly) {
        // if creation from javascript is allowed, we need the constructor!
        constructorId = env->GetMethodID(it->second->clsObject, "<init>",
                                         "(Lag/boersego/bgjs/V8Engine;J[Ljava/lang/Object;)V");
        JNI_ASSERTF(constructorId,
                   "Constructor '(V8Engine, long, Object[])' does not exist on registered class '%s'", canonicalName.c_str());
    }
#endif

    pthread_mutex_unlock(&_mutexEnv);

    return v8ClassInfo;
}

void JNIV8Wrapper::initializeNativeJNIV8Object(jobject obj, jobject engineObj, jlong jsObjPtr) {
    auto v8Object = JNIWrapper::wrapObject<JNIV8Object>(obj);
    auto engine = JNIWrapper::wrapObject<BGJSV8Engine>(engineObj);
    JNI_ASSERT(engine, "Invalid engine parameter");
    JNI_ASSERT(v8Object, "Invalid object; object must extend JNIV8Object and be registered on JNIV8Wrapper");

    v8::Isolate* isolate = engine->getIsolate();
    v8::Locker l(isolate);
    Isolate::Scope isolateScope(isolate);
    HandleScope scope(isolate);
    Context::Scope ctxScope(engine->getContext());

    v8::Persistent<Object>* persistentPtr;
    v8::Local<Object> jsObj;

    JNIV8ClassInfo *classInfo = JNIV8Wrapper::_getV8ClassInfo(v8Object->getCanonicalName(), engine.get());

    // if an object was already supplied we just need to extract it and store it
    if(jsObjPtr) {
        persistentPtr = reinterpret_cast<v8::Persistent<Object>*>(jsObjPtr);
        jsObj = v8::Local<Object>::New(isolate, *persistentPtr);
        // clear and delete persistent
        persistentPtr->Reset();
        delete persistentPtr;
    }

    // adjust external memory size: size of native object + 64bit pointer to native object stored on java
    v8Object->adjustJSExternalMemory(classInfo->container->size + 8);

    // associate js object with native c++ object
    v8Object->setJSObject(engine.get(), classInfo, jsObj);
}

void JNIV8Wrapper::_registerObject(JNIV8ObjectType type, const std::string& canonicalName, const std::string& baseCanonicalName, JNIV8ObjectInitializer i, JNIV8ObjectCreator c, size_t size) {
    // canonicalName may be already registered
    // (e.g. when called from JNI_OnLoad; when using multiple linked libraries it is called once for each library)
    auto it = _objmap.find(canonicalName);
    if (it != _objmap.end()) {
        JNI_ASSERTF(!i && !it->second->initializer, "Class %s registered both from native and java", canonicalName.c_str());
        return;
    }

    // base class has to be registered if it is not JNIV8Object (which is only registered with JNIWrapper, because it provides no JS functionality on its own)
    JNIV8ClassInfoContainer *baseInfo = nullptr;
    if (!baseCanonicalName.empty()) {
        it = _objmap.find(baseCanonicalName);
        if (it == _objmap.end()) {
            return;
        }
        baseInfo = it->second;
    } else if(canonicalName != JNIBase::getCanonicalName<JNIV8Object>()) {
        // an empty base class is only allowed here for internally registering JNIObject itself
        JNI_ASSERT(0, "Attempt to register an object without super class");
        return;
    }

    if(baseInfo) {
        if (type == JNIV8ObjectType::kWrapper) {
            // wrapper classes can only extend other wrapper classes (or JNIV8Object directly)
            JNIV8ClassInfoContainer *baseInfo2 = baseInfo;
            do {
                if (baseInfo2->type != JNIV8ObjectType::kWrapper && baseInfo2->baseClassInfo) {
                    return;
                }
                baseInfo2 = baseInfo->baseClassInfo;
            } while (baseInfo2);
        } else if (baseInfo->type == JNIV8ObjectType::kWrapper &&
                   baseInfo->type != type) {
            // wrapper classes can only be extended by other wrapper classes!
            return;
        }
    }

    JNIV8ClassInfoContainer *info = new JNIV8ClassInfoContainer(type, canonicalName, i, c, size, baseInfo);
    _objmap[canonicalName] = info;
}

// persistent classes can also be accessed as JNIV8Object directly!
template<> JNILocalRef<JNIV8Object> JNIV8Wrapper::wrapObject<JNIV8Object>(
        v8::Local<v8::Object> object) {
    v8::Local<v8::External> ext;
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    // because this method takes a local, we can be sure that the correct v8 scopes are active around it already
    // we still need a handle scope however...
    v8::HandleScope scope(isolate);

    if (object->InternalFieldCount() >= 1) {
        // does the object have internal fields? if so use it!
        ext = object->GetInternalField(0).As<v8::External>();
    } else {
        return nullptr;
    }
    return JNILocalRef<JNIV8Object>(reinterpret_cast<JNIV8Object*>(ext->Value()));
};

/**
 * internal helper function called by V8Engine on destruction
 */
void JNIV8Wrapper::cleanupV8Engine(BGJSV8Engine *engine) {
    pthread_mutex_lock(&_mutexEnv);
    for(auto it : _objmap) {
        for (auto it2 = it.second->classInfos.begin(); it2 != it.second->classInfos.end(); ++it2) {
            if((*it2)->engine == engine) {
                delete *it2;
                it.second->classInfos.erase(it2);
                break;
            }
        }
    }
    pthread_mutex_unlock(&_mutexEnv);
}