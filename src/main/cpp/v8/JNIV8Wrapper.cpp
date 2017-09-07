//
// Created by Martin Kleinhans on 21.08.17.
//

#include "../bgjs/BGJSV8Engine.h"
using namespace v8;

#include "JNIV8Wrapper.h"

std::map<std::string, V8ClassInfoContainer*> JNIV8Wrapper::_objmap;

const char* JNIV8Wrapper::_v8PrivateKey = "JNIV8WrapperPrivate";

void JNIV8Wrapper::init() {
    JNIWrapper::registerObject<JNIV8Object>(JNIObjectType::kAbstract);
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
    assert(it != _objmap.end());

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

    Local<External> data = External::New(isolate, (void*)v8ClassInfo);
    Handle<FunctionTemplate> ft = FunctionTemplate::New(isolate, v8ConstructorCallback, data);
    ft->SetClassName(String::NewFromUtf8(isolate, "V8TestClassName")); // @TODO: classname!

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
        assert(baseInfo);
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
    if(_objmap.find(canonicalName) != _objmap.end()) {
        return;
    }

    // base class has to be registered if it is not JNIV8Object (which is only registered with JNIWrapper, because it provides no JS functionality on its own)
    V8ClassInfoContainer *baseInfo = nullptr;
    if(baseCanonicalName != JNIWrapper::getCanonicalName<JNIV8Object>()) {
        auto it = _objmap.find(baseCanonicalName);
        if(it == _objmap.end()) {
            return;
        }
        baseInfo = it->second;

        // pure java objects can only directly extend JNIObject if they are abstract!
        if(JNIWrapper::getCanonicalName<JNIV8Object>() == baseCanonicalName && !i && type != JNIV8ObjectType::kAbstract) {
            return;
        }
    }

    if(baseInfo) {
        if (type == JNIV8ObjectType::kWrapper) {
            // wrapper classes can only extend other wrapper classes (or JNIV8Object directly)
            // @TODO: what about JNIV8BridgedObject; if it includes calling v8 functions, it might be interesting as a wrapper?!
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