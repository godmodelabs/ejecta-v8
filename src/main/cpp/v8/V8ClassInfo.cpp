//
// Created by Martin Kleinhans on 18.08.17.
//

#include "V8ClassInfo.h"
#include "../bgjs/BGJSV8Engine.h"
#include "JNIV8Object.h"
#include "JNIV8Wrapper.h"

using namespace v8;

void v8JavaAccessorGetterCallback(Local<String> property, const PropertyCallbackInfo<Value> &info) {
    HandleScope scope(info.GetIsolate());

    v8::Local<v8::External> ext;

    ext = info.Data().As<v8::External>();
    JNIV8ObjectJavaAccessorHolder* cb = static_cast<JNIV8ObjectJavaAccessorHolder*>(ext->Value());

    ext = info.This()->GetInternalField(0).As<v8::External>();
    JNIV8Object* v8Object = reinterpret_cast<JNIV8Object*>(ext->Value());

    jobject jobj = v8Object->getJObject();
    JNIEnv *env = JNIWrapper::getEnvironment();

    info.GetReturnValue().Set(JNIV8Wrapper::jobject2v8value(env->CallObjectMethod(jobj, cb->javaGetterId)));
}


void v8JavaAccessorSetterCallback(Local<String> property, Local<Value> value, const PropertyCallbackInfo<void> &info) {
    HandleScope scope(info.GetIsolate());

    v8::Local<v8::External> ext;

    ext = info.Data().As<v8::External>();
    JNIV8ObjectJavaAccessorHolder* cb = static_cast<JNIV8ObjectJavaAccessorHolder*>(ext->Value());

    ext = info.This()->GetInternalField(0).As<v8::External>();
    JNIV8Object* v8Object = reinterpret_cast<JNIV8Object*>(ext->Value());

    jobject jobj = v8Object->getJObject();
    JNIEnv *env = JNIWrapper::getEnvironment();

    env->CallVoidMethod(jobj, cb->javaSetterId, JNIV8Wrapper::v8value2jobject(value));
}

void v8JavaMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
    HandleScope scope(args.GetIsolate());

    v8::Local<v8::External> ext;

    ext = args.Data().As<v8::External>();
    JNIV8ObjectJavaCallbackHolder* cb = static_cast<JNIV8ObjectJavaCallbackHolder*>(ext->Value());

    ext = args.This()->GetInternalField(0).As<v8::External>();
    JNIV8Object* v8Object = reinterpret_cast<JNIV8Object*>(ext->Value());

    jobject jobj = v8Object->getJObject();
    JNIEnv *env = JNIWrapper::getEnvironment();

    jobjectArray jargs = env->NewObjectArray(args.Length(), env->FindClass("java/lang/Object"), nullptr);
    for(int idx=0,n=args.Length(); idx<n; idx++) {
        env->SetObjectArrayElement(jargs, idx, JNIV8Wrapper::v8value2jobject(args[idx]));
    }

    jobject result = env->CallObjectMethod(jobj, cb->javaMethodId, jobj, jargs);

    args.GetReturnValue().Set(JNIV8Wrapper::jobject2v8value(result));
}

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

V8ClassInfoContainer::V8ClassInfoContainer(JNIV8ObjectType type, const std::string& canonicalName, JNIV8ObjectInitializer i,
                                           JNIV8ObjectCreator c, size_t s, V8ClassInfoContainer *baseClassInfo) :
        type(type), canonicalName(canonicalName), initializer(i), creator(c), size(s), baseClassInfo(baseClassInfo) {
    if(baseClassInfo) {
        if (!creator) {
            creator = baseClassInfo->creator;
        }
        if(!s) {
            size = baseClassInfo->size;
        }
    }
}

V8ClassInfo::V8ClassInfo(V8ClassInfoContainer *container, BGJSV8Engine *engine) :
        container(container), engine(engine), constructorCallback(0) {
}

V8ClassInfo::~V8ClassInfo() {
    for(auto &it : callbackHolders) {
        delete it;
    }
    for(auto &it : accessorHolders) {
        delete it;
    }
    for(auto &it : javaAccessorHolders) {
        delete it;
    }
    for(auto &it : javaCallbackHolders) {
        delete it;
    }
}

void V8ClassInfo::registerConstructor(JNIV8ObjectConstructorCallback callback) {
    assert(container->type == JNIV8ObjectType::kPersistent);
    constructorCallback = callback;
}

void V8ClassInfo::registerMethod(const std::string &methodName, JNIV8ObjectMethodCallback callback) {
    Isolate* isolate = engine->getIsolate();
    HandleScope scope(isolate);

    Local<FunctionTemplate> ft = Local<FunctionTemplate>::New(isolate, functionTemplate);
    // ofc functions belong on the prototype, and not on the actual instance for performance/memory reasons
    // but interestingly enough, we MUST store them there because they simply are not "copied" from the InstanceTemplate when using inherit later
    // but other properties and accessors are copied without problems.
    // Thought: it is not allowed to store actual Functions on these templates - only FunctionTemplates
    // functions can only exist in a context once and probaly can not be duplicated/copied in the same way as scalars and accessors, so there IS a difference.
    // maybe when doing inherit the function template is instanced, and then inherit copies over properties to its own instance template which can not be done for instanced functions..
    Local<ObjectTemplate> instanceTpl = ft->PrototypeTemplate();

    JNIV8ObjectCallbackHolder* holder = new JNIV8ObjectCallbackHolder(methodName, callback);
    callbackHolders.push_back(holder);
    Local<External> data = External::New(isolate, (void*)holder);

    instanceTpl->Set(String::NewFromUtf8(isolate, methodName.c_str()), FunctionTemplate::New(isolate, v8MethodCallback, data));
}

void V8ClassInfo::registerJavaMethod(const std::string& methodName, jmethodID methodId) {
    Isolate* isolate = engine->getIsolate();
    HandleScope scope(isolate);

    Local<FunctionTemplate> ft = Local<FunctionTemplate>::New(isolate, functionTemplate);
    // ofc functions belong on the prototype, and not on the actual instance for performance/memory reasons
    // but interestingly enough, we MUST store them there because they simply are not "copied" from the InstanceTemplate when using inherit later
    // but other properties and accessors are copied without problems.
    // Thought: it is not allowed to store actual Functions on these templates - only FunctionTemplates
    // functions can only exist in a context once and probaly can not be duplicated/copied in the same way as scalars and accessors, so there IS a difference.
    // maybe when doing inherit the function template is instanced, and then inherit copies over properties to its own instance template which can not be done for instanced functions..
    Local<ObjectTemplate> instanceTpl = ft->PrototypeTemplate();

    JNIV8ObjectJavaCallbackHolder* holder = new JNIV8ObjectJavaCallbackHolder(methodName, methodId);
    javaCallbackHolders.push_back(holder);

    Local<External> data = External::New(isolate, (void*)holder);

    instanceTpl->Set(String::NewFromUtf8(isolate, methodName.c_str()), FunctionTemplate::New(isolate, v8JavaMethodCallback, data));
}

void V8ClassInfo::registerJavaAccessor(const std::string& propertyName, jmethodID getterId, jmethodID setterId, v8::PropertyAttribute settings) {
    Isolate* isolate = engine->getIsolate();
    HandleScope scope(isolate);

    Local<FunctionTemplate> ft = Local<FunctionTemplate>::New(isolate, functionTemplate);
    Local<ObjectTemplate> instanceTpl = ft->InstanceTemplate();

    JNIV8ObjectJavaAccessorHolder* holder = new JNIV8ObjectJavaAccessorHolder(propertyName, getterId, setterId);
    javaAccessorHolders.push_back(holder);

    Local<External> data = External::New(isolate, (void*)holder);

    AccessorSetterCallback finalSetter = 0;
    if(setterId) {
        finalSetter = v8JavaAccessorSetterCallback;
    } else if(!(settings & v8::PropertyAttribute::ReadOnly)) {
        settings = (v8::PropertyAttribute)(settings | v8::PropertyAttribute::ReadOnly);
    }
    instanceTpl->SetAccessor(String::NewFromUtf8(isolate, propertyName.c_str()),
                             v8JavaAccessorGetterCallback, finalSetter,
                             data, DEFAULT, settings);
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
    accessorHolders.push_back(holder);
    Local<External> data = External::New(isolate, (void*)holder);

    AccessorSetterCallback finalSetter = 0;
    if(setter) {
        finalSetter = v8AccessorSetterCallback;
    } else if(!(settings & v8::PropertyAttribute::ReadOnly)) {
        settings = (v8::PropertyAttribute)(settings | v8::PropertyAttribute::ReadOnly);
    }
    instanceTpl->SetAccessor(String::NewFromUtf8(isolate, propertyName.c_str()),
                             v8AccessorGetterCallback, finalSetter,
                             data, DEFAULT, settings);
}

v8::Local<v8::Object> V8ClassInfo::newInstance() const {
    assert(container->type == JNIV8ObjectType::kPersistent);

    Isolate* isolate = engine->getIsolate();
    Isolate::Scope scope(isolate);
    EscapableHandleScope handleScope(isolate);
    Context::Scope ctxScope(engine->getContext());

    Local<FunctionTemplate> ft = Local<FunctionTemplate>::New(isolate, functionTemplate);

    return handleScope.Escape(ft->InstanceTemplate()->NewInstance());
}

v8::Local<v8::Function> V8ClassInfo::getConstructor() const {
    assert(container->type == JNIV8ObjectType::kPersistent);
    Isolate* isolate = engine->getIsolate();
    EscapableHandleScope scope(isolate);
    auto ft = Local<FunctionTemplate>::New(isolate, functionTemplate);
    return scope.Escape(ft->GetFunction());
}
