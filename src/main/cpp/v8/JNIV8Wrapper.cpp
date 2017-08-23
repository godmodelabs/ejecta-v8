//
// Created by Martin Kleinhans on 21.08.17.
//

#include "../bgjs/BGJSV8Engine.h"
using namespace v8;

#include "JNIV8Wrapper.h"

jfieldID JNIV8Wrapper::_nativeHandleFieldId = 0;
std::map<std::string, V8ClassInfoContainer*> JNIV8Wrapper::_objmap;

void JNIV8Wrapper::reloadBindings() {
    // see comments in `initializeNativeV8Object` for why this is needed
    JNIEnv *env = JNIWrapper::getEnvironment();
    jclass cls = env->FindClass("ag/boersego/bgjs/JNIObject");
    _nativeHandleFieldId = env->GetFieldID(cls, "nativeHandle", "J");
}

void JNIV8Wrapper::v8ConstructorCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Local<v8::External> ext = args.Data().As<v8::External>();
    V8ClassInfo* info = static_cast<V8ClassInfo*>(ext->Value());

    v8::Isolate* isolate = info->engine->getIsolate();
    HandleScope scope(isolate);

    // Constructor!
    v8::Persistent<Object>* jsObj = new v8::Persistent<v8::Object>(isolate, args.This());
    auto ptr = info->container->creator(info, jsObj);
    ptr->initFromJS(args);

    // @TODO forward arguments?
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
    HandleScope scope(isolate);

    Local<External> data = External::New(isolate, (void*)v8ClassInfo);
    Handle<FunctionTemplate> ft = FunctionTemplate::New(isolate, v8ConstructorCallback, data);
    ft->SetClassName(String::NewFromUtf8(isolate, "V8TestClassName"));

    Local<ObjectTemplate> tpl = ft->InstanceTemplate();
    tpl->SetInternalFieldCount(1);

    // store
    v8ClassInfo->functionTemplate.Reset(isolate, ft);

    it->second->initializer(v8ClassInfo);

    return v8ClassInfo;
}

void JNIV8Wrapper::initializeNativeV8Object(jobject obj, jlong enginePtr, jlong jsObjPtr) {
    // this is a little dirty because it bypasses all the nice work done by JNIWrapper
    // but due to the fact that we do not know the type of this object we can not use wrapObject here
    // we can not map to V8Object, as it is not exposed by JNIWrapper because it is abstract..

    JNIEnv *env = JNIWrapper::getEnvironment();
    jlong handle = env->GetLongField(obj, _nativeHandleFieldId);

    V8Object *v8Object = reinterpret_cast<V8Object*>(handle);
    v8Object->_bgjsEngine = reinterpret_cast<BGJSV8Engine*>(enginePtr);
    v8Object->_v8ClassInfo = JNIV8Wrapper::_getV8ClassInfo(v8Object->getCanonicalName(), v8Object->_bgjsEngine);

    v8::Persistent<Object>* persistentPtr;
    v8::Local<Object> jsObj;

    v8::Isolate* isolate = v8Object->_bgjsEngine->getIsolate();

    // if an object was already supplied we just need to store it
    if(jsObjPtr) {
        persistentPtr = reinterpret_cast<v8::Persistent<Object>*>(jsObjPtr);
        jsObj = v8::Local<Object>::New(isolate, *persistentPtr);
        // @TODO store
    // otherwise we need to create one
    } else {
        // @TODO create object
        return;
    }

    // store reference to native object in JS object
    jsObj->SetInternalField(0, External::New(isolate, (void*)v8Object));
}

void JNIV8Wrapper::_registerObject(const std::string& canonicalName, V8ObjectInitializer i, V8ObjectCreator c) {
    if(_objmap.find(canonicalName) != _objmap.end()) {
        return;
    }

    V8ClassInfoContainer *info = new V8ClassInfoContainer(canonicalName, i, c);
    _objmap[canonicalName] = info;
}

//--------------------------------------------------------------------------------------------------
// Exports
//--------------------------------------------------------------------------------------------------
extern "C" {

JNIEXPORT void JNICALL Java_ag_boersego_bgjs_V8Object_initBinding(JNIEnv *env) {
    JNIV8Wrapper::reloadBindings();
}

JNIEXPORT void JNICALL Java_ag_boersego_bgjs_V8Object_initNativeV8Object(JNIEnv *env, jobject obj, jlong enginePtr, jlong jsObjPtr) {
    JNIV8Wrapper::initializeNativeV8Object(obj, enginePtr, jsObjPtr);
}

}