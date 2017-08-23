//
// Created by Martin Kleinhans on 18.08.17.
//

#include "V8ClassInfo.h"
#include "../bgjs/BGJSV8Engine.h"
#include "V8Object.h"

using namespace v8;

void v8MethodCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
    HandleScope scope(args.GetIsolate());

    v8::Local<v8::External> ext;

    ext = args.Data().As<v8::External>();
    V8ObjectCallbackHolder* cb = static_cast<V8ObjectCallbackHolder*>(ext->Value());

    ext = args.This()->GetInternalField(0).As<v8::External>();
    V8Object* v8Object = reinterpret_cast<V8Object*>(ext->Value());

    (v8Object->*(cb->callback))(args);
}

V8ClassInfoContainer::V8ClassInfoContainer(const std::string& canonicalName, V8ObjectInitializer i,
                                           V8ObjectCreator c) : canonicalName(canonicalName), initializer(i), creator(c) {
}

V8ClassInfo::V8ClassInfo(V8ClassInfoContainer *container, BGJSV8Engine *engine) :
        container(container), engine(engine) {
}

void V8ClassInfo::registerMethod(const std::string &methodName, V8ObjectMethodCallback callback) {
    Isolate* isolate = engine->getIsolate();
    HandleScope scope(isolate);

    Local<FunctionTemplate> ft = Local<FunctionTemplate>::New(isolate, functionTemplate);
    Local<ObjectTemplate> instanceTpl = ft->InstanceTemplate();

    V8ObjectCallbackHolder* holder = new V8ObjectCallbackHolder(callback);
    Local<External> data = External::New(isolate, (void*)holder);

    instanceTpl->Set(String::NewFromUtf8(isolate, methodName.c_str()), FunctionTemplate::New(isolate, v8MethodCallback, data));
}

v8::Local<v8::FunctionTemplate> V8ClassInfo::getFunctionTemplate() const {
    Isolate* isolate = engine->getIsolate();
    EscapableHandleScope scope(isolate);
    return scope.Escape(Local<FunctionTemplate>::New(isolate, functionTemplate));
}

