//
// Created by Martin Kleinhans on 21.08.17.
//

#include "../bgjs/BGJSV8Engine.h"
using namespace v8;

#include "JNIV8Wrapper.h"
#include "JNIV8Array.h"
#include "JNIV8GenericObject.h"
#include "JNIV8Function.h"

#include <string>
#include <algorithm>

std::map<std::string, V8ClassInfoContainer*> JNIV8Wrapper::_objmap;

//const char* JNIV8Wrapper::_v8PrivateKey = "JNIV8WrapperPrivate";

void JNIV8Wrapper::init() {
    JNIWrapper::registerObject<JNIV8Object>(JNIObjectType::kAbstract);
    JNIV8Wrapper::registerObject<JNIV8Array>(JNIV8ObjectType::kWrapper);
    JNIV8Wrapper::registerObject<JNIV8GenericObject>(JNIV8ObjectType::kWrapper);
    JNIV8Wrapper::registerObject<JNIV8Function>(JNIV8ObjectType::kWrapper);
}

void JNIV8Wrapper::v8ConstructorCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Local<v8::External> ext = args.Data().As<v8::External>();
    V8ClassInfo* info = static_cast<V8ClassInfo*>(ext->Value());

    v8::Isolate* isolate = info->engine->getIsolate();
    HandleScope scope(isolate);

    // create temporary persistent for the js object and then call the constructor
    v8::Persistent<Object>* jsObj = new v8::Persistent<v8::Object>(isolate, args.This());
    auto ptr = info->container->creator(info, jsObj);

    if(info->constructorCallback) {
        (ptr.get()->*(info->constructorCallback))(args);
    }
}

V8ClassInfo* JNIV8Wrapper::_getV8ClassInfo(const std::string& canonicalName, BGJSV8Engine *engine) {
    // find class info container
    auto it = _objmap.find(canonicalName);
    JNI_ASSERT(it != _objmap.end(), "Attempt to retrieve class info for unregistered class");

    // check if class info object already exists for this engine!
    for(auto &it2 : it->second->classInfos) {
        if(it2->engine == engine) {
            return it2;
        }
    }
    // if it was not found we have to create it now & link it with the container
    auto v8ClassInfo = new V8ClassInfo(it->second, engine);
    it->second->classInfos.push_back(v8ClassInfo);

    // initialize class info: template with constructor and general setup created here
    // individual methods and accessors handled by static method on subclass
    v8::Isolate* isolate = engine->getIsolate();
    v8::Locker l(isolate);
    Isolate::Scope isolateScope(isolate);
    HandleScope scope(isolate);
    Context::Scope ctxScope(engine->getContext());

    // v8 class name: canonical name with underscores instead of slashes
    // e.g. ag/boersego/bgjs/Test becomes ag_boersego_bgjs_Test
    std::string strV8ClassName = canonicalName;
    std::replace(strV8ClassName.begin(), strV8ClassName.end(), '/', '_');

    Local<External> data = External::New(isolate, (void*)v8ClassInfo);
    Handle<FunctionTemplate> ft = FunctionTemplate::New(isolate, v8ConstructorCallback, data);
    ft->SetClassName(String::NewFromUtf8(isolate, strV8ClassName.c_str()));

    // inherit from baseclass
    if(v8ClassInfo->container->baseClassInfo) {
        V8ClassInfo *baseInfo = nullptr;
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

    return v8ClassInfo;
}

void JNIV8Wrapper::initializeNativeJNIV8Object(jobject obj, jlong enginePtr, jlong jsObjPtr) {
    // @TODO: make sure that internal field or private field is not already used!!
    auto v8Object = JNIWrapper::wrapObject<JNIV8Object>(obj);
    BGJSV8Engine *engine = reinterpret_cast<BGJSV8Engine*>(enginePtr);
    V8ClassInfo *classInfo = JNIV8Wrapper::_getV8ClassInfo(v8Object->getCanonicalName(), engine);

    v8::Isolate* isolate = engine->getIsolate();
    v8::Locker l(isolate);
    Isolate::Scope isolateScope(isolate);
    HandleScope scope(isolate);
    Context::Scope ctxScope(engine->getContext());

    v8::Persistent<Object>* persistentPtr;
    v8::Local<Object> jsObj;

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
    v8Object->setJSObject(engine, classInfo, jsObj);
}

void JNIV8Wrapper::_registerObject(JNIV8ObjectType type, const std::string& canonicalName, const std::string& baseCanonicalName, JNIV8ObjectInitializer i, JNIV8ObjectCreator c, size_t size) {
    // canonicalName may be already registered
    // (e.g. when called from JNI_OnLoad; when using multiple linked libraries it is called once for each library)
    if (_objmap.find(canonicalName) != _objmap.end()) {
        return;
    }

    // base class has to be registered if it is not JNIV8Object (which is only registered with JNIWrapper, because it provides no JS functionality on its own)
    V8ClassInfoContainer *baseInfo = nullptr;
    if (baseCanonicalName != JNIWrapper::getCanonicalName<JNIV8Object>()) {
        auto it = _objmap.find(baseCanonicalName);
        if (it == _objmap.end()) {
            return;
        }
        baseInfo = it->second;
    // pure java objects can only directly extend JNIV8Object if they are abstract!
    } else if(!i && type != JNIV8ObjectType::kAbstract) {
        // @TODO: this should be possible...
        return;
    }

    if(baseInfo) {
        if (type == JNIV8ObjectType::kWrapper) {
            // wrapper classes can only extend other wrapper classes (or JNIV8Object directly)
            V8ClassInfoContainer *baseInfo2 = baseInfo;
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

    V8ClassInfoContainer *info = new V8ClassInfoContainer(type, canonicalName, i, c, size, baseInfo);
    _objmap[canonicalName] = info;
}

/**
 * convert a jstring to a std::string
 */
v8::Local<v8::String> JNIV8Wrapper::jstring2v8string(jstring string) {
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    // because this method returns a local, we can assume that the correct v8 scopes are active around it already
    // we still need a handle scope however...
    v8::EscapableHandleScope scope(isolate);
    return scope.Escape(v8::String::NewFromUtf8(isolate, JNIWrapper::jstring2string(string).c_str()));
}

/**
 * convert a std::string to a jstring
 */
jstring JNIV8Wrapper::v8string2jstring(v8::Local<v8::String> string) {
    return JNIWrapper::string2jstring(BGJS_STRING_FROM_V8VALUE(string));
}

/**
 * convert an instance of V8Value to a jobject
 */
jobject JNIV8Wrapper::v8value2jobject(Local<Value> valueRef) {
    JNIEnv *env = JNIWrapper::getEnvironment();

    // @TODO: cache
    jclass clsUndefined = env->FindClass("ag/boersego/bgjs/JNIV8Undefined");
    jclass clsDouble = env->FindClass("java/lang/Double");
    jclass clsBool = env->FindClass("java/lang/Boolean");
    jmethodID getterDouble = env->GetStaticMethodID(clsDouble, "valueOf","(D)Ljava/lang/Double;");
    jmethodID getterBool = env->GetStaticMethodID(clsBool, "valueOf","(Z)Ljava/lang/Boolean;");
    jmethodID getterUndefined = env->GetStaticMethodID(clsUndefined, "GetInstance","()Lag/boersego/bgjs/JNIV8Undefined;");

    if(valueRef->IsUndefined()) {
        return env->CallStaticObjectMethod(clsUndefined, getterUndefined);
    } else if(valueRef->IsNumber()) {
        return env->CallStaticObjectMethod(clsDouble, getterDouble, valueRef->NumberValue());
    } else if(valueRef->IsString()) {
        return JNIWrapper::string2jstring(BGJS_STRING_FROM_V8VALUE(valueRef));
    } else if(valueRef->IsBoolean()) {
        return env->CallStaticObjectMethod(clsBool, getterBool, valueRef->BooleanValue());
    } else if(valueRef->IsSymbol()) {
        JNI_ASSERT(0, "Symbols are not supported"); // return env->NewObject(clsJNIV8Value, constructor, 4, nullptr);
    } else if(valueRef->IsNull()) {
        return nullptr;
    } else if(valueRef->IsFunction()){
        return JNIV8Wrapper::wrapObject<JNIV8Function>(valueRef->ToObject())->getJObject();
    } else if(valueRef->IsArray()){
        return JNIV8Wrapper::wrapObject<JNIV8Array>(valueRef->ToObject())->getJObject();
    } else if(valueRef->IsObject()) {
        auto ptr = JNIV8Wrapper::wrapObject<JNIV8Object>(valueRef->ToObject());
        if(ptr) {
            return ptr->getJObject();
        } else {
            return JNIV8Wrapper::wrapObject<JNIV8GenericObject>(valueRef->ToObject())->getJObject();
        }
    } else {
        JNI_ASSERT(0, "Encountered unexpected v8 type");
    }
    return nullptr;
}

/**
 * convert an instance of Object to a v8value
 */
v8::Local<v8::Value> JNIV8Wrapper::jobject2v8value(jobject object) {
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    // because this method returns a local, we can assume that the correct v8 scopes are active around it already
    // we still need a handle scope however...
    v8::EscapableHandleScope scope(isolate);

    v8::Local<v8::Value> resultRef;

    JNIEnv *env = JNIWrapper::getEnvironment();

    // @TODO: cache!
    jclass clsBoolean = env->FindClass("java/lang/Boolean");
    jmethodID mBooleanValue = env->GetMethodID(clsBoolean, "booleanValue","()Z");
    jclass clsString = env->FindClass("java/lang/String");
    jclass clsChar = env->FindClass("java/lang/Character");
    jmethodID mCharValue = env->GetMethodID(clsChar, "charValue","()C");
    jclass clsNumber = env->FindClass("java/lang/Number");
    jmethodID mDoubleValue = env->GetMethodID(clsNumber, "doubleValue","()D");
    jclass clsObj = env->FindClass("ag/boersego/bgjs/JNIV8Object");

    // jobject referencing "null" can actually be non-null..
    if(env->IsSameObject(object, NULL) || !object) {
        resultRef = v8::Null(isolate);
    } else if(env->IsInstanceOf(object, clsString)) {
        resultRef = JNIV8Wrapper::jstring2v8string((jstring)object);
    } else if(env->IsInstanceOf(object, clsChar)) {
        jchar c = env->CallCharMethod(object, mCharValue);
        v8::MaybeLocal<v8::String> maybeLocal = v8::String::NewFromTwoByte(isolate, &c, NewStringType::kNormal, 1);
        if(!maybeLocal.IsEmpty()) {
            resultRef = maybeLocal.ToLocalChecked();
        }
    } else if(env->IsInstanceOf(object, clsNumber)) {
        jdouble n = env->CallDoubleMethod(object, mDoubleValue);
        resultRef = v8::Number::New(isolate, n);
    } else if(env->IsInstanceOf(object, clsBoolean)) {
        jboolean b = env->CallBooleanMethod(object, mBooleanValue);
        resultRef = v8::Boolean::New(isolate, b);
    } else if(env->IsInstanceOf(object, clsObj)) {
        resultRef = JNIV8Wrapper::wrapObject<JNIV8Object>(object)->getJSObject();
    }
    if(resultRef.IsEmpty()) {
        resultRef = v8::Undefined(isolate);
    }
    return scope.Escape(resultRef);
}

// persistent classes can also be accessed as JNIV8Object directly!
template <> std::shared_ptr<JNIV8Object> JNIV8Wrapper::wrapObject<JNIV8Object>(v8::Local<v8::Object> object) {
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
    return std::static_pointer_cast<JNIV8Object>(reinterpret_cast<JNIV8Object*>(ext->Value())->getSharedPtr());
};