//
// Created by Martin Kleinhans on 18.08.17.
//

#include "V8ClassInfo.h"
#include "../bgjs/BGJSV8Engine.h"
#include "JNIV8Object.h"

using namespace v8;

void v8AccessorGetterCallback(Local<String> property, const PropertyCallbackInfo<Value> &info) {
    HandleScope scope(info.GetIsolate());

    v8::Local<v8::External> ext;

    ext = info.Data().As<v8::External>();
    JNIV8ObjectAccessorHolder* cb = static_cast<JNIV8ObjectAccessorHolder*>(ext->Value());

    ext = info.This()->GetInternalField(0).As<v8::External>();
    JNIV8Object* v8Object = reinterpret_cast<JNIV8Object*>(ext->Value());

    (v8Object->*(cb->getterCallback))(cb->propertyName, info);
}


void v8AccessorSetterCallback(Local<String> property, Local<Value> value, const PropertyCallbackInfo<void> &info) {
    HandleScope scope(info.GetIsolate());

    v8::Local<v8::External> ext;

    ext = info.Data().As<v8::External>();
    JNIV8ObjectAccessorHolder* cb = static_cast<JNIV8ObjectAccessorHolder*>(ext->Value());

    ext = info.This()->GetInternalField(0).As<v8::External>();
    JNIV8Object* v8Object = reinterpret_cast<JNIV8Object*>(ext->Value());

    (v8Object->*(cb->setterCallback))(cb->propertyName, value, info);
}

void v8MethodCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
    HandleScope scope(args.GetIsolate());

    v8::Local<v8::External> ext;

    ext = args.Data().As<v8::External>();
    JNIV8ObjectCallbackHolder* cb = static_cast<JNIV8ObjectCallbackHolder*>(ext->Value());

    ext = args.This()->GetInternalField(0).As<v8::External>();
    JNIV8Object* v8Object = reinterpret_cast<JNIV8Object*>(ext->Value());

    (v8Object->*(cb->callback))(cb->methodName, args);
}

V8ClassInfoContainer::V8ClassInfoContainer(const std::string& canonicalName, JNIV8ObjectInitializer i,
                                           JNIV8ObjectCreator c, size_t size) : canonicalName(canonicalName), initializer(i), creator(c), size(size) {
}

V8ClassInfo::V8ClassInfo(V8ClassInfoContainer *container, BGJSV8Engine *engine) :
        container(container), engine(engine), constructorCallback(0) {
}

void V8ClassInfo::registerConstructor(JNIV8ObjectConstructorCallback callback) {
    constructorCallback = callback;
}

void V8ClassInfo::registerMethod(const std::string &methodName, JNIV8ObjectMethodCallback callback) {
    Isolate* isolate = engine->getIsolate();
    HandleScope scope(isolate);

    Local<FunctionTemplate> ft = Local<FunctionTemplate>::New(isolate, functionTemplate);
    Local<ObjectTemplate> instanceTpl = ft->InstanceTemplate();

    JNIV8ObjectCallbackHolder* holder = new JNIV8ObjectCallbackHolder(methodName, callback);
    Local<External> data = External::New(isolate, (void*)holder);

    instanceTpl->Set(String::NewFromUtf8(isolate, methodName.c_str()), FunctionTemplate::New(isolate, v8MethodCallback, data));
}

void V8ClassInfo::registerAccessor(const std::string& propertyName,
                      JNIV8ObjectAccessorGetterCallback getter,
                      JNIV8ObjectAccessorSetterCallback setter,
                      v8::PropertyAttribute settings) {
    Isolate* isolate = engine->getIsolate();
    HandleScope scope(isolate);

    Local<FunctionTemplate> ft = Local<FunctionTemplate>::New(isolate, functionTemplate);
    Local<ObjectTemplate> instanceTpl = ft->InstanceTemplate();

    JNIV8ObjectAccessorHolder* holder = new JNIV8ObjectAccessorHolder(propertyName, getter, setter);
    Local<External> data = External::New(isolate, (void*)holder);

    AccessorSetterCallback finalSetter = 0;
    if(setter) {
        finalSetter = v8AccessorSetterCallback;
    }
    instanceTpl->SetAccessor(String::NewFromUtf8(isolate, propertyName.c_str()),
                             v8AccessorGetterCallback, finalSetter,
                             data, DEFAULT, settings);
}

v8::Local<v8::FunctionTemplate> V8ClassInfo::getFunctionTemplate() const {
    Isolate* isolate = engine->getIsolate();
    EscapableHandleScope scope(isolate);
    return scope.Escape(Local<FunctionTemplate>::New(isolate, functionTemplate));
}

v8::Local<v8::Function> V8ClassInfo::getConstructor() const {
    Isolate* isolate = engine->getIsolate();
    EscapableHandleScope scope(isolate);
    auto ft = Local<FunctionTemplate>::New(isolate, functionTemplate);
    return scope.Escape(ft->GetFunction());
}

