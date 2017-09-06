//
// Created by Martin Kleinhans on 21.08.17.
//

#include "../bgjs/BGJSV8Engine.h"
using namespace v8;

#include "JNIV8Wrapper.h"

std::map<std::string, V8ClassInfoContainer*> JNIV8Wrapper::_objmap;

void JNIV8Wrapper::init() {
    JNIWrapper::registerObject<JNIV8Object>(JNIObjectType::kAbstract);
}

void JNIV8Wrapper::reloadBindings() {
    // see comments in `initializeNativeJNIV8Object` for why this is needed
    JNIEnv *env = JNIWrapper::getEnvironment();
    jclass cls = env->FindClass("ag/boersego/bgjs/JNIObject");
}

void JNIV8Wrapper::v8ConstructorCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Local<v8::External> ext = args.Data().As<v8::External>();
    V8ClassInfo* info = static_cast<V8ClassInfo*>(ext->Value());

    v8::Isolate* isolate = info->engine->getIsolate();
    HandleScope scope(isolate);

    // Constructor!
    v8::Persistent<Object>* jsObj = new v8::Persistent<v8::Object>(isolate, args.This());
    auto ptr = info->container->creator(info, jsObj);
    if(info->constructorCallback) {
        (ptr.get()->*(info->constructorCallback))(args);
    }
}

V8ClassInfo* JNIV8Wrapper::_getV8ClassInfo(const std::string& canonicalName, BGJSV8Engine *engine) {
    // find class info container
    auto it = _objmap.find(canonicalName);
    if(it == _objmap.end()) {
        // @TODO: exception?
        return nullptr;
    }
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
    // @TODO: context scope?

    Local<External> data = External::New(isolate, (void*)v8ClassInfo);
    Handle<FunctionTemplate> ft = FunctionTemplate::New(isolate, v8ConstructorCallback, data);
    ft->SetClassName(String::NewFromUtf8(isolate, "V8TestClassName"));

    Local<ObjectTemplate> tpl = ft->InstanceTemplate();
    tpl->SetInternalFieldCount(1);

    // store
    v8ClassInfo->functionTemplate.Reset(isolate, ft);

    it->second->initializer(v8ClassInfo);

    // @TODO: now copy over methods + accessors from the base class?
    // @TODO: what if a constructor was registered on baseclass? => probably ok to ignore
    // @TODO: do we need the notion of "abstract" objects here as well? => probably YES

    return v8ClassInfo;
}

void JNIV8Wrapper::initializeNativeJNIV8Object(jobject obj, jlong enginePtr, jlong jsObjPtr) {
    auto v8Object = JNIWrapper::wrapObject<JNIV8Object>(obj);
    BGJSV8Engine *engine = reinterpret_cast<BGJSV8Engine*>(enginePtr);
    V8ClassInfo *classInfo = JNIV8Wrapper::_getV8ClassInfo(v8Object->getCanonicalName(), engine);

    v8::Persistent<Object>* persistentPtr;
    v8::Local<Object> jsObj;

    v8::Isolate* isolate = engine->getIsolate();
    v8::Locker l(isolate);
    Isolate::Scope isolateScope(isolate);
    HandleScope scope(isolate);

    Context::Scope ctxScope(engine->getContext());

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

void JNIV8Wrapper::_registerObject(const std::string& canonicalName, const std::string& baseCanonicalName, JNIV8ObjectInitializer i, JNIV8ObjectCreator c, size_t size) {
    // base class has to be registered if it is not JNIV8Object (which is only registered with JNIWrapper, because it provides no JS functionality on its own)
    if(baseCanonicalName != JNIWrapper::getCanonicalName<JNIV8Object>() &&
            _objmap.find(baseCanonicalName) == _objmap.end()) {
        return;
    }

    // class itself must not be registered yet
    if(_objmap.find(canonicalName) != _objmap.end()) {
        return;
    }

    V8ClassInfoContainer *info = new V8ClassInfoContainer(canonicalName, i, c, size);
    _objmap[canonicalName] = info;
}