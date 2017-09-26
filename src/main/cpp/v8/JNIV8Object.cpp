//
// Created by Martin Kleinhans on 18.08.17.
//

#include "JNIV8Object.h"
#include "JNIV8Wrapper.h"
#include "../bgjs/BGJSV8Engine.h"

#define LOG_TAG "JNIV8Object"

using namespace v8;

BGJS_JNIV8OBJECT_LINK(JNIV8Object, "ag/boersego/bgjs/JNIV8Object");

JNIV8Object::JNIV8Object(jobject obj, JNIClassInfo *info) : JNIObject(obj, info) {
    _externalMemory = 0;
}

JNIV8Object::~JNIV8Object() {
    if(!_jsObject.IsEmpty()) {
        // adjust external memory counter if required
        if (_jsObject.IsWeak()) {
            _bgjsEngine->getIsolate()->AdjustAmountOfExternalAllocatedMemory(_externalMemory);
        }
        _jsObject.Reset();
    }
}

void JNIV8Object::weakPersistentCallback(const WeakCallbackInfo<void>& data) {
    auto jniV8Object = reinterpret_cast<JNIV8Object*>(data.GetParameter());

    // the js object is no longer being used => release the strong reference to the java object
    jniV8Object->releaseJObject();

    // "resurrect" the JS object, because we might need it later in some native or java function
    // IF we do, we have to make a strong reference to the java object again and also register this callback for
    // the provided JS object reference!
    jniV8Object->_jsObject.ClearWeak();

    // we are only holding the object because java/native is still alive, v8 can not gc it anymore
    // => adjust external memory counter
    jniV8Object->_bgjsEngine->getIsolate()->AdjustAmountOfExternalAllocatedMemory(-jniV8Object->_externalMemory);
}

void JNIV8Object::makeWeak() {
    // wrapper type objects are not directly linked to the lifecycle of the js object
    // they can be destroyed / gced from java at any time, and there can exist multiple
    if(_v8ClassInfo->container->type == JNIV8ObjectType::kWrapper || _jsObject.IsWeak()) return;
    _jsObject.SetWeak((void*)this, JNIV8Object::weakPersistentCallback, WeakCallbackType::kFinalizer);
    // create a strong reference to the java object as long as the JS object is referenced from somewhere
    retainJObject();

    // object can be gc'd by v8 => adjust external memory counter
    _bgjsEngine->getIsolate()->AdjustAmountOfExternalAllocatedMemory(_externalMemory);
}

void JNIV8Object::linkJSObject(v8::Handle<v8::Object> jsObject) {
    Isolate* isolate = _bgjsEngine->getIsolate();

    // store reference to native object in JS object
    if(_v8ClassInfo->container->type != JNIV8ObjectType::kWrapper) {
        jsObject->SetInternalField(0, External::New(isolate, (void *) this));
    }

    // store reference in persistent
    _jsObject.Reset(isolate, jsObject);
}

void JNIV8Object::adjustJSExternalMemory(int64_t change) {
    _externalMemory += change;
    // external memory only counts if js object
    // - already exists
    // - is still referenced from JS
    if(!_jsObject.IsEmpty() && _jsObject.IsWeak()) {
        _bgjsEngine->getIsolate()->AdjustAmountOfExternalAllocatedMemory(change);
    }
}

v8::Local<v8::Object> JNIV8Object::getJSObject() {
    Isolate* isolate = _bgjsEngine->getIsolate();
    Isolate::Scope scope(isolate);
    EscapableHandleScope handleScope(isolate);
    Context::Scope ctxScope(_bgjsEngine->getContext());

    Local<Object> localRef;

    // if there is no js object yet, create it now!
    if(_jsObject.IsEmpty()) {
        localRef = _v8ClassInfo->newInstance();
        linkJSObject(localRef);
    } else {
        localRef = Local<Object>::New(isolate, _jsObject);
    }

    // make the persistent reference weak, to get notified when the returned reference is no longer used
    makeWeak();

    return handleScope.Escape(localRef);
}

void JNIV8Object::setJSObject(BGJSV8Engine *engine, V8ClassInfo *cls,
                              Handle<Object> jsObject) {

    Isolate *isolate = engine->getIsolate();
    Isolate::Scope isolateScope(isolate);
    HandleScope scope(isolate);

    _bgjsEngine = engine;
    _v8ClassInfo = cls;

    if (!jsObject.IsEmpty()) {
        linkJSObject(jsObject);
        // a js object was provided, so we can asume the reference is already being used somewhere
        // => make the persistent reference weak, to get notified when it is no longer used
        makeWeak();
    }
}

BGJSV8Engine* JNIV8Object::getEngine() const {
    return _bgjsEngine;
}

void JNIV8Object::initializeJNIBindings(JNIClassInfo *info, bool isReload) {
    info->registerConstructor("(Lag/boersego/bgjs/V8Engine;J)V","<JNIV8ObjectInit>");
    info->registerConstructor("(Lag/boersego/bgjs/V8Engine;)V","<JNIV8ObjectInit#2>");

    info->registerNativeMethod("adjustJSExternalMemory", "(J)V", (void*)JNIV8Object::jniAdjustJSExternalMemory);
    info->registerNativeMethod("callV8Method", "(Ljava/lang/String;[Ljava/lang/Object;)Lag/boersego/bgjs/JNIV8Value;", (void*)JNIV8Object::jniCallV8Method);
    info->registerNativeMethod("getV8Field", "(Ljava/lang/String;)Lag/boersego/bgjs/JNIV8Value;", (void*)JNIV8Object::jniGetV8Field);
    info->registerNativeMethod("setV8Field", "(Ljava/lang/String;Ljava/lang/Object;)V", (void*)JNIV8Object::jniSetV8Field);
}

void JNIV8Object::jniAdjustJSExternalMemory(JNIEnv *env, jobject obj, jlong change) {
    auto ptr = JNIWrapper::wrapObject<JNIV8Object>(obj);
    ptr->adjustJSExternalMemory(change);
}

jobject JNIV8Object::jniGetV8Field(JNIEnv *env, jobject obj, jobject name) {
    auto ptr = JNIWrapper::wrapObject<JNIV8Object>(obj);
    BGJSV8Engine *engine = ptr->_bgjsEngine;

    v8::Isolate* isolate = engine->getIsolate();
    v8::Locker l(isolate);
    Isolate::Scope isolateScope(isolate);
    HandleScope scope(isolate);
    Context::Scope ctxScope(engine->getContext());

    // if JSObject does not exist yet, create it!
    if(ptr->_jsObject.IsEmpty()) {
        ptr->getJSObject();
    }

    Local<Object> localRef = Local<Object>::New(isolate, ptr->_jsObject);

    std::string fieldName = JNIWrapper::jstring2string((jstring)name);
    Local<Value> valueRef = localRef->Get(String::NewFromUtf8(isolate, JNIWrapper::jstring2string((jstring)name).c_str()));

    return JNIV8Wrapper::v8value2jobject(valueRef);
}

void JNIV8Object::jniSetV8Field(JNIEnv *env, jobject obj, jobject name, jobject value) {
    auto ptr = JNIWrapper::wrapObject<JNIV8Object>(obj);
    BGJSV8Engine *engine = ptr->_bgjsEngine;

    v8::Isolate* isolate = engine->getIsolate();
    v8::Locker l(isolate);
    Isolate::Scope isolateScope(isolate);
    HandleScope scope(isolate);
    Context::Scope ctxScope(engine->getContext());

    // @TODO set property here
    // arguments => v8 values
}

jobject JNIV8Object::jniCallV8Method(JNIEnv *env, jobject obj, jobject name, jobject arguments) {
    auto ptr = JNIWrapper::wrapObject<JNIV8Object>(obj);
    BGJSV8Engine *engine = ptr->_bgjsEngine;

    v8::Isolate* isolate = engine->getIsolate();
    v8::Locker l(isolate);
    Isolate::Scope isolateScope(isolate);
    HandleScope scope(isolate);
    Context::Scope ctxScope(engine->getContext());

    // @TODO: call method here
    // arguments => v8 values

    return JNIV8Wrapper::v8value2jobject(v8::Undefined(isolate));
}

//--------------------------------------------------------------------------------------------------
// Exports
//--------------------------------------------------------------------------------------------------
extern "C" {

JNIEXPORT void JNICALL
Java_ag_boersego_bgjs_JNIV8Object_initNativeJNIV8Object(JNIEnv *env, jobject obj, jlong enginePtr,
                                                        jlong jsObjPtr) {
    JNIV8Wrapper::initializeNativeJNIV8Object(obj, enginePtr, jsObjPtr);
}

}