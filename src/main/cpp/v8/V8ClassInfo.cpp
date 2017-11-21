//
// Created by Martin Kleinhans on 18.08.17.
//

#include "V8ClassInfo.h"
#include "../bgjs/BGJSV8Engine.h"
#include "JNIV8Object.h"
#include "JNIV8Wrapper.h"

#include <cassert>

using namespace v8;

decltype(V8ClassInfo::_jniObject) V8ClassInfo::_jniObject = {0};

/**
 * cache JNI class references
 */
void V8ClassInfo::initJNICache() {
    JNIEnv *env = JNIWrapper::getEnvironment();
    _jniObject.clazz = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Object"));
}

void V8ClassInfo::v8JavaAccessorGetterCallback(Local<String> property, const PropertyCallbackInfo<Value> &info) {
    HandleScope scope(info.GetIsolate());

    v8::Local<v8::External> ext;

    ext = info.Data().As<v8::External>();
    JNIV8ObjectJavaAccessorHolder* cb = static_cast<JNIV8ObjectJavaAccessorHolder*>(ext->Value());
    if (cb->javaGetterId) {

        JNIEnv *env = JNIWrapper::getEnvironment();
        if (!cb->isStatic) {
            ext = info.This()->GetInternalField(0).As<v8::External>();
            JNIV8Object *v8Object = reinterpret_cast<JNIV8Object *>(ext->Value());

            jobject jobj = v8Object->getJObject();

            info.GetReturnValue().Set(
                    JNIV8Wrapper::jobject2v8value(env->CallObjectMethod(jobj, cb->javaGetterId)));
        } else {
            info.GetReturnValue().Set(
                    JNIV8Wrapper::jobject2v8value(
                            env->CallStaticObjectMethod(cb->javaClass, cb->javaGetterId)));
        }
    }
}


void V8ClassInfo::v8JavaAccessorSetterCallback(Local<String> property, Local<Value> value, const PropertyCallbackInfo<void> &info) {
    HandleScope scope(info.GetIsolate());

    v8::Local<v8::External> ext;

    ext = info.Data().As<v8::External>();
    JNIV8ObjectJavaAccessorHolder* cb = static_cast<JNIV8ObjectJavaAccessorHolder*>(ext->Value());

    if (cb->javaSetterId) {
        JNIEnv *env = JNIWrapper::getEnvironment();
        if (!cb->isStatic) {
            ext = info.This()->GetInternalField(0).As<v8::External>();
            JNIV8Object *v8Object = reinterpret_cast<JNIV8Object *>(ext->Value());

            jobject jobj = v8Object->getJObject();

            env->CallVoidMethod(jobj, cb->javaSetterId, JNIV8Wrapper::v8value2jobject(value));
        } else {
            env->CallStaticVoidMethod(cb->javaClass, cb->javaSetterId,
                                      JNIV8Wrapper::v8value2jobject(value));
        }
    }
}

void V8ClassInfo::v8JavaMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
    HandleScope scope(args.GetIsolate());
    Isolate *isolate = args.GetIsolate();

    JNIEnv *env = JNIWrapper::getEnvironment();
    jobject jobj = nullptr;

    v8::Local<v8::External> ext;
    ext = args.Data().As<v8::External>();
    JNIV8ObjectJavaCallbackHolder* cb = static_cast<JNIV8ObjectJavaCallbackHolder*>(ext->Value());

    // we only check the "this" for non-static methods
    // otherwise "this" can be anything, we do not care..
    if(!cb->isStatic) {
        v8::Local<v8::Object> thisArg = args.This();
        if (!thisArg->InternalFieldCount()) {
            args.GetIsolate()->ThrowException(v8::Exception::TypeError(String::NewFromUtf8(isolate, "invalid invocation")));
            return;
        }
        v8::Local<v8::Value> internalField = thisArg->GetInternalField(0);
        if (!internalField->IsExternal()) {
            args.GetIsolate()->ThrowException(v8::Exception::TypeError(String::NewFromUtf8(isolate, "invalid invocation")));
            return;
        }

        // this is not really "safe".. but how could it be? another part of the program could store arbitrary stuff in internal fields
        ext = internalField.As<v8::External>();
        JNIV8Object *v8Object = reinterpret_cast<JNIV8Object *>(ext->Value());
        jobj = v8Object->getJObject();

        if (!env->IsInstanceOf(jobj, cb->javaClass)) {
            args.GetIsolate()->ThrowException(v8::Exception::TypeError(String::NewFromUtf8(isolate, "invalid invocation")));
            return;
        }
    }

    jobjectArray jargs = env->NewObjectArray(args.Length(), _jniObject.clazz, nullptr);
    for(int idx=0,n=args.Length(); idx<n; idx++) {
        env->SetObjectArrayElement(jargs, idx, JNIV8Wrapper::v8value2jobject(args[idx]));
    }

    jobject result;
    if(!cb->isStatic) {
        result = env->CallObjectMethod(jobj, cb->javaMethodId, jargs);
    } else {
        result = env->CallStaticObjectMethod(cb->javaClass, cb->javaMethodId, jargs);
    }

    args.GetReturnValue().Set(JNIV8Wrapper::jobject2v8value(result));
}

void V8ClassInfo::v8AccessorGetterCallback(Local<String> property, const PropertyCallbackInfo<Value> &info) {
    HandleScope scope(info.GetIsolate());

    v8::Local<v8::External> ext;

    ext = info.Data().As<v8::External>();
    JNIV8ObjectAccessorHolder* cb = static_cast<JNIV8ObjectAccessorHolder*>(ext->Value());

    if(cb->isStatic) {
        (cb->getterCallback.s)(cb->propertyName, info);
    } else {
        ext = info.This()->GetInternalField(0).As<v8::External>();
        JNIV8Object *v8Object = reinterpret_cast<JNIV8Object *>(ext->Value());

        (v8Object->*(cb->getterCallback.i))(cb->propertyName, info);
    }
}


void V8ClassInfo::v8AccessorSetterCallback(Local<String> property, Local<Value> value, const PropertyCallbackInfo<void> &info) {
    HandleScope scope(info.GetIsolate());

    v8::Local<v8::External> ext;

    ext = info.Data().As<v8::External>();
    JNIV8ObjectAccessorHolder* cb = static_cast<JNIV8ObjectAccessorHolder*>(ext->Value());

    if(cb->isStatic) {
        (cb->setterCallback.s)(cb->propertyName, value, info);
    } else {
        ext = info.This()->GetInternalField(0).As<v8::External>();
        JNIV8Object *v8Object = reinterpret_cast<JNIV8Object *>(ext->Value());

        (v8Object->*(cb->setterCallback.i))(cb->propertyName, value, info);
    }
}

void V8ClassInfo::v8MethodCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
    HandleScope scope(args.GetIsolate());

    v8::Local<v8::External> ext;

    ext = args.Data().As<v8::External>();
    JNIV8ObjectCallbackHolder* cb = static_cast<JNIV8ObjectCallbackHolder*>(ext->Value());

    if(cb->isStatic) {
        // we do NOT check how this function was invoked.. if a this was supplied, we just ignore it!
        (cb->callback.s)(cb->methodName, args);
    } else {
        ext = args.This()->GetInternalField(0).As<v8::External>();
        JNIV8Object *v8Object = reinterpret_cast<JNIV8Object *>(ext->Value());

        (v8Object->*(cb->callback.i))(cb->methodName, args);
    }
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

    JNIEnv *env = JNIWrapper::getEnvironment();
    jclass clazz;

    clazz = env->FindClass(canonicalName.c_str());
    JNI_ASSERT(clazz != nullptr, "Failed to retrieve java class");
    clsObject = (jclass)env->NewGlobalRef(clazz);

    // binding is optional; if it does not exist we have to clear the pending exception
    clazz = env->FindClass((canonicalName+"$V8Binding").c_str());
    if(clazz) {
        clsBinding = (jclass)env->NewGlobalRef(clazz);
    } else {
        clsBinding = nullptr;
        env->ExceptionClear();
    }
}

V8ClassInfo::V8ClassInfo(V8ClassInfoContainer *container, BGJSV8Engine *engine) :
        container(container), engine(engine), constructorCallback(0), createFromJavaOnly(false) {
}

V8ClassInfo::~V8ClassInfo() {
    for(auto &it : callbackHolders) {
        delete it;
    }
    for(auto &it : accessorHolders) {
        delete it;
    }
    JNIEnv *env = JNIWrapper::getEnvironment();
    for(auto &it : javaAccessorHolders) {
        env->DeleteGlobalRef(it->javaClass);
        delete it;
    }
    for(auto &it : javaCallbackHolders) {
        env->DeleteGlobalRef(it->javaClass);
        delete it;
    }
}

void V8ClassInfo::setCreationPolicy(JNIV8ClassCreationPolicy policy) {
    createFromJavaOnly = policy == JNIV8ClassCreationPolicy::NATIVE_ONLY;
}

void V8ClassInfo::registerConstructor(JNIV8ObjectConstructorCallback callback) {
    assert(container->type == JNIV8ObjectType::kPersistent);
    constructorCallback = callback;
}

void V8ClassInfo::registerMethod(const std::string &methodName, JNIV8ObjectMethodCallback callback) {
    JNIV8ObjectCallbackHolder* holder = new JNIV8ObjectCallbackHolder();
    holder->isStatic = false;
    holder->callback.i = callback;
    holder->methodName = methodName;
    _registerMethod(holder);
}

void V8ClassInfo::registerStaticMethod(const std::string& methodName, JNIV8ObjectStaticMethodCallback callback) {
    JNIV8ObjectCallbackHolder* holder = new JNIV8ObjectCallbackHolder();
    holder->isStatic = true;
    holder->callback.s = callback;
    holder->methodName = methodName;
    _registerMethod(holder);
}

void V8ClassInfo::registerJavaMethod(const std::string& methodName, jmethodID methodId) {
    JNIV8ObjectJavaCallbackHolder* holder = new JNIV8ObjectJavaCallbackHolder();
    holder->methodName = methodName;
    holder->javaMethodId = methodId;
    holder->isStatic = false;
    _registerJavaMethod(holder);
}

void V8ClassInfo::registerStaticJavaMethod(const std::string &methodName, jmethodID methodId) {
    JNIV8ObjectJavaCallbackHolder* holder = new JNIV8ObjectJavaCallbackHolder();
    holder->methodName = methodName;
    holder->javaMethodId = methodId;
    holder->isStatic = true;
    _registerJavaMethod(holder);
}

void V8ClassInfo::registerJavaAccessor(const std::string& propertyName, jmethodID getterId, jmethodID setterId) {
    JNIV8ObjectJavaAccessorHolder* holder = new JNIV8ObjectJavaAccessorHolder();
    holder->propertyName = propertyName;
    holder->javaGetterId = getterId;
    holder->javaSetterId = setterId;
    holder->isStatic = false;
    _registerJavaAccessor(holder);
}

void V8ClassInfo::registerStaticJavaAccessor(const std::string &propertyName, jmethodID getterId,
                                             jmethodID setterId) {
    JNIV8ObjectJavaAccessorHolder* holder = new JNIV8ObjectJavaAccessorHolder();
    holder->propertyName = propertyName;
    holder->javaGetterId = getterId;
    holder->javaSetterId = setterId;
    holder->isStatic = true;
    _registerJavaAccessor(holder);
}

void V8ClassInfo::registerAccessor(const std::string& propertyName,
                      JNIV8ObjectAccessorGetterCallback getter,
                      JNIV8ObjectAccessorSetterCallback setter) {
    JNIV8ObjectAccessorHolder* holder = new JNIV8ObjectAccessorHolder();
    holder->propertyName = propertyName;
    holder->getterCallback.i = getter;
    holder->setterCallback.i = setter;
    holder->isStatic = false;
    _registerAccessor(holder);
}

void V8ClassInfo::registerStaticAccessor(const std::string &propertyName,
                                         JNIV8ObjectStaticAccessorGetterCallback getter,
                                         JNIV8ObjectStaticAccessorSetterCallback setter) {
    JNIV8ObjectAccessorHolder* holder = new JNIV8ObjectAccessorHolder();
    holder->propertyName = propertyName;
    holder->getterCallback.s = getter;
    holder->setterCallback.s = setter;
    holder->isStatic = true;
    accessorHolders.push_back(holder);
    _registerAccessor(holder);
}

void V8ClassInfo::_registerJavaMethod(JNIV8ObjectJavaCallbackHolder *holder) {
    Isolate* isolate = engine->getIsolate();
    HandleScope scope(isolate);

    Local<FunctionTemplate> ft = Local<FunctionTemplate>::New(isolate, functionTemplate);

    JNIEnv *env = JNIWrapper::getEnvironment();
    holder->javaClass = (jclass)env->NewGlobalRef(env->FindClass(container->canonicalName.c_str()));;
    javaCallbackHolders.push_back(holder);

    Local<External> data = External::New(isolate, (void*)holder);

    if(holder->isStatic) {
        Local<Function> f = ft->GetFunction();
        f->Set(String::NewFromUtf8(isolate, holder->methodName.c_str()), FunctionTemplate::New(isolate, v8JavaMethodCallback, data, Local<Signature>(), 0, ConstructorBehavior::kThrow)->GetFunction());
    } else {
        // ofc functions belong on the prototype, and not on the actual instance for performance/memory reasons
        // but interestingly enough, we MUST store them there because they simply are not "copied" from the InstanceTemplate when using inherit later
        // but other properties and accessors are copied without problems.
        // Thought: it is not allowed to store actual Functions on these templates - only FunctionTemplates
        // functions can only exist in a context once and probaly can not be duplicated/copied in the same way as scalars and accessors, so there IS a difference.
        // maybe when doing inherit the function template is instanced, and then inherit copies over properties to its own instance template which can not be done for instanced functions..
        Local<ObjectTemplate> instanceTpl = ft->PrototypeTemplate();
        instanceTpl->Set(String::NewFromUtf8(isolate, holder->methodName.c_str()),
                         FunctionTemplate::New(isolate, v8JavaMethodCallback, data, Local<Signature>(), 0, ConstructorBehavior::kThrow));
    }
}

void V8ClassInfo::_registerJavaAccessor(JNIV8ObjectJavaAccessorHolder *holder) {
    Isolate* isolate = engine->getIsolate();
    HandleScope scope(isolate);

    Local<FunctionTemplate> ft = Local<FunctionTemplate>::New(isolate, functionTemplate);

    JNIEnv *env = JNIWrapper::getEnvironment();
    holder->javaClass = (jclass)env->NewGlobalRef(env->FindClass(container->canonicalName.c_str()));
    javaAccessorHolders.push_back(holder);

    Local<External> data = External::New(isolate, (void*)holder);

    AccessorSetterCallback finalSetter = 0;
    v8::PropertyAttribute settings = v8::PropertyAttribute::None;
    if(holder->javaSetterId) {
        finalSetter = v8JavaAccessorSetterCallback;
    } else {
        settings = v8::PropertyAttribute::ReadOnly;
    }

    if(holder->isStatic) {
        Local<Function> f = ft->GetFunction();
        f->SetAccessor(String::NewFromUtf8(isolate, holder->propertyName.c_str()),
                       v8JavaAccessorGetterCallback, finalSetter,
                       data, DEFAULT, settings);
    } else {
        Local<ObjectTemplate> instanceTpl = ft->InstanceTemplate();
        instanceTpl->SetAccessor(String::NewFromUtf8(isolate, holder->propertyName.c_str()),
                                 v8JavaAccessorGetterCallback, finalSetter,
                                 data, DEFAULT, settings);
    }
}

void V8ClassInfo::_registerMethod(JNIV8ObjectCallbackHolder *holder) {
    Isolate* isolate = engine->getIsolate();
    HandleScope scope(isolate);

    Local<FunctionTemplate> ft = Local<FunctionTemplate>::New(isolate, functionTemplate);

    callbackHolders.push_back(holder);
    Local<External> data = External::New(isolate, (void*)holder);

    if(holder->isStatic) {
        Local<Function> f = ft->GetFunction();
        f->Set(String::NewFromUtf8(isolate, holder->methodName.c_str()),
               FunctionTemplate::New(isolate, v8MethodCallback, data, Local<Signature>(), 0, ConstructorBehavior::kThrow)->GetFunction());
    } else {
        // ofc functions belong on the prototype, and not on the actual instance for performance/memory reasons
        // but interestingly enough, we MUST store them there because they simply are not "copied" from the InstanceTemplate when using inherit later
        // but other properties and accessors are copied without problems.
        // Thought: it is not allowed to store actual Functions on these templates - only FunctionTemplates
        // functions can only exist in a context once and probaly can not be duplicated/copied in the same way as scalars and accessors, so there IS a difference.
        // maybe when doing inherit the function template is instanced, and then inherit copies over properties to its own instance template which can not be done for instanced functions..
        Local<ObjectTemplate> instanceTpl = ft->PrototypeTemplate();
        instanceTpl->Set(String::NewFromUtf8(isolate, holder->methodName.c_str()), FunctionTemplate::New(isolate, v8MethodCallback, data, Local<Signature>(), 0, ConstructorBehavior::kThrow));
    }
}

void V8ClassInfo::_registerAccessor(JNIV8ObjectAccessorHolder *holder) {
    Isolate* isolate = engine->getIsolate();
    HandleScope scope(isolate);

    Local<FunctionTemplate> ft = Local<FunctionTemplate>::New(isolate, functionTemplate);

    accessorHolders.push_back(holder);
    Local<External> data = External::New(isolate, (void*)holder);

    AccessorSetterCallback finalSetter = 0;
    v8::PropertyAttribute settings = v8::PropertyAttribute::None;
    if(holder->setterCallback.i || holder->setterCallback.s) {
        finalSetter = v8AccessorSetterCallback;
    } else {
        settings = v8::PropertyAttribute::ReadOnly;
    }

    if(holder->isStatic) {
        Local<Function> f = ft->GetFunction();
        f->SetAccessor(String::NewFromUtf8(isolate, holder->propertyName.c_str()),
                       v8AccessorGetterCallback, finalSetter,
                       data, DEFAULT, settings);
    } else {
        Local<ObjectTemplate> instanceTpl = ft->InstanceTemplate();
        instanceTpl->SetAccessor(String::NewFromUtf8(isolate, holder->propertyName.c_str()),
                                 v8AccessorGetterCallback, finalSetter,
                                 data, DEFAULT, settings);
    }
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
