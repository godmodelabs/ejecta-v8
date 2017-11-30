//
// Created by Martin Kleinhans on 18.08.17.
//

#include "JNIV8ClassInfo.h"
#include "../bgjs/BGJSV8Engine.h"
#include "JNIV8Object.h"
#include "JNIV8Wrapper.h"

#include <cassert>
#include <stdlib.h>

using namespace v8;

#define ThrowV8RangeError(msg)\
v8::Isolate::GetCurrent()->ThrowException(v8::Exception::RangeError(\
String::NewFromUtf8(v8::Isolate::GetCurrent(), (msg).c_str()))\
);

#define ThrowV8TypeError(msg)\
v8::Isolate::GetCurrent()->ThrowException(v8::Exception::TypeError(\
String::NewFromUtf8(v8::Isolate::GetCurrent(), (msg).c_str()))\
);


decltype(JNIV8ClassInfo::_jniObject) JNIV8ClassInfo::_jniObject = {0};

/**
 * cache JNI class references
 */
void JNIV8ClassInfo::initJNICache() {
    JNIEnv *env = JNIWrapper::getEnvironment();
    _jniObject.clazz = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Object"));
}

void JNIV8ClassInfo::v8JavaAccessorGetterCallback(Local<String> property, const PropertyCallbackInfo<Value> &info) {
    Isolate *isolate = info.GetIsolate();
    HandleScope scope(isolate);

    v8::Local<v8::External> ext;

    ext = info.Data().As<v8::External>();
    JNIV8ObjectJavaAccessorHolder* cb = static_cast<JNIV8ObjectJavaAccessorHolder*>(ext->Value());
    if (cb->javaGetterId) {
        jobject jobj = nullptr;
        JNIEnv *env = JNIWrapper::getEnvironment();
        if (!cb->isStatic) {
            ext = info.This()->GetInternalField(0).As<v8::External>();
            JNIV8Object *v8Object = reinterpret_cast<JNIV8Object *>(ext->Value());

            jobj = v8Object->getJObject();
        }

        v8::Local<v8::Value> value;

        value = JNIV8Marshalling::callJavaMethod(env, cb->propertyType, cb->javaClass, cb->javaGetterId, jobj, nullptr);

        // java method could have thrown an exception; if so forward it to v8
        if(env->ExceptionCheck()) {
            BGJSV8Engine::GetInstance(isolate)->forwardJNIExceptionToV8();
            return;
        }

        info.GetReturnValue().Set(value);
    }
}


void JNIV8ClassInfo::v8JavaAccessorSetterCallback(Local<String> property, Local<Value> value, const PropertyCallbackInfo<void> &info) {
    Isolate *isolate = info.GetIsolate();
    HandleScope scope(isolate);

    v8::Local<v8::External> ext;

    ext = info.Data().As<v8::External>();
    JNIV8ObjectJavaAccessorHolder* cb = static_cast<JNIV8ObjectJavaAccessorHolder*>(ext->Value());

    if (cb->javaSetterId) {
        jobject jobj = nullptr;
        jvalue jval;
        memset(&jval, 0, sizeof(jvalue));
        JNIEnv *env = JNIWrapper::getEnvironment();
        if (!cb->isStatic) {
            ext = info.This()->GetInternalField(0).As<v8::External>();
            JNIV8Object *v8Object = reinterpret_cast<JNIV8Object *>(ext->Value());

            jobj = v8Object->getJObject();
        }

        JNIV8MarshallingError res;
        res = JNIV8Marshalling::convertV8ValueToJavaArgument(env, value, cb->propertyType, &jval);
        if(res != JNIV8MarshallingError::kOk) {
            switch(res) {
                default:
                case JNIV8MarshallingError::kWrongType:
                    ThrowV8TypeError("property '" + cb->propertyName + "' is not nullable");
                    break;
                case JNIV8MarshallingError::kUndefined:
                    ThrowV8TypeError("property '" + cb->propertyName + "' must not be undefined");
                    break;
                case JNIV8MarshallingError::kNotNullable:
                    ThrowV8TypeError("wrong type for property '" + cb->propertyName);
                    break;
                case JNIV8MarshallingError::kNoNaN:
                    ThrowV8TypeError("property '" + cb->propertyName + "' must not be NaN");
                    break;
                case JNIV8MarshallingError::kVoidNotNull:
                    ThrowV8TypeError("property '" + cb->propertyName + "' can only be null");
                    break;
                case JNIV8MarshallingError::kOutOfRange:
                    ThrowV8RangeError("assigned value '"+JNIV8Marshalling::v8value2string(value)+"' is out of range for property '" + cb->propertyName + "'");
                    break;
            }
            return;
        }

        if (jobj) {
            env->CallVoidMethodA(jobj, cb->javaSetterId, &jval);
        } else {
            env->CallStaticVoidMethodA(cb->javaClass, cb->javaSetterId, &jval);
        }

        // java method could have thrown an exception; if so forward it to v8
        if(env->ExceptionCheck()) {
            BGJSV8Engine::GetInstance(isolate)->forwardJNIExceptionToV8();
            return;
        }
    }
}

void JNIV8ClassInfo::v8JavaMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
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
        v8::Local<v8::Value> internalField = thisArg->GetInternalField(0);
        // this is not really "safe".. but how could it be? another part of the program could store arbitrary stuff in internal fields
        ext = internalField.As<v8::External>();
        JNIV8Object *v8Object = reinterpret_cast<JNIV8Object *>(ext->Value());
        jobj = v8Object->getJObject();
    }

    // try to find a matching signature
    JNIV8ObjectJavaSignatureInfo *signature = nullptr;

    for (auto sig : cb->signatures) {
        if (!sig.arguments) {
            if (!signature) {
                signature = &sig;
            }
        } else if (sig.arguments->size() == args.Length()) {
            signature = &sig;
            break;
        }
    }

    if(!signature) {
        isolate->ThrowException(v8::Exception::TypeError(String::NewFromUtf8(isolate, ("invalid number of arguments (" + std::to_string(args.Length()) + ") supplied to " + cb->methodName).c_str())));
        return;
    }

    jvalue *jargs;
    size_t numJArgs;

    if(!signature->arguments) {
        // generic case: an array of objects!
        // nothing to validate here, this always works
        numJArgs = 1;
        jargs = (jvalue*)malloc(sizeof(jvalue)*numJArgs);
        memset(jargs, 0, sizeof(jvalue)*numJArgs);
        jobjectArray jArray = env->NewObjectArray(args.Length(), _jniObject.clazz, nullptr);
        for (int idx = 0, n = args.Length(); idx < n; idx++) {
            env->SetObjectArrayElement(jArray, idx, JNIV8Marshalling::v8value2jobject(args[idx]));
        }
        jargs[0].l = jArray;
    } else {
        // specific case
        // arguments might have to be of a certain type, so we need to validate!
        numJArgs = (size_t)args.Length();
        if(numJArgs) {
            jargs = (jvalue *) malloc(sizeof(jvalue) * numJArgs);
            memset(jargs, 0, sizeof(jvalue) * numJArgs);

            for(int idx = 0, n = args.Length(); idx < n; idx++) {
                JNIV8JavaArgument &arg = (*signature->arguments)[idx];
                v8::Local<v8::Value> value = args[idx];

                JNIV8MarshallingError res = JNIV8Marshalling::convertV8ValueToJavaArgument(env, value, arg, &(jargs[idx]));
                if(res != JNIV8MarshallingError::kOk) {
                    // conversion failed => simply clean up & throw an exception
                    free(jargs);
                    switch(res) {
                        default:
                        case JNIV8MarshallingError::kWrongType:
                            ThrowV8TypeError("argument #" + std::to_string(idx) + " of '" + cb->methodName + "' is not nullable");
                            break;
                        case JNIV8MarshallingError::kUndefined:
                            ThrowV8TypeError("argument #" + std::to_string(idx) + " of '" + cb->methodName + "' does not accept undefined");
                            break;
                        case JNIV8MarshallingError::kNotNullable:
                            ThrowV8TypeError("wrong type for argument #" + std::to_string(idx) + " of '" + cb->methodName + "'");
                            break;
                        case JNIV8MarshallingError::kNoNaN:
                            ThrowV8TypeError("argument #" + std::to_string(idx) + " of '" + cb->methodName + "' must not be NaN");
                            break;
                        case JNIV8MarshallingError::kVoidNotNull:
                            ThrowV8TypeError("argument #" + std::to_string(idx) + " of '" + cb->methodName + "' must be null");
                            break;
                        case JNIV8MarshallingError::kOutOfRange:
                            ThrowV8RangeError("value '"+JNIV8Marshalling::v8value2string(value)+"' is out of range for argument #" + std::to_string(idx) + " of '" + cb->methodName + "'");
                            break;
                    }
                    return;
                }
            }
        } else {
            jargs = nullptr;
        }
    }

    v8::Local<v8::Value> result;

    result = JNIV8Marshalling::callJavaMethod(env, cb->returnType, cb->javaClass, signature->javaMethodId, jobj, jargs);

    if(jargs) {
        free(jargs);
    }

    // java method could have thrown an exception; if so forward it to v8
    if(env->ExceptionCheck()) {
        BGJSV8Engine::GetInstance(isolate)->forwardJNIExceptionToV8();
        return;
    }

    args.GetReturnValue().Set(result);
}

void JNIV8ClassInfo::v8AccessorGetterCallback(Local<String> property, const PropertyCallbackInfo<Value> &info) {
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


void JNIV8ClassInfo::v8AccessorSetterCallback(Local<String> property, Local<Value> value, const PropertyCallbackInfo<void> &info) {
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

void JNIV8ClassInfo::v8MethodCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
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

JNIV8ClassInfoContainer::JNIV8ClassInfoContainer(JNIV8ObjectType type, const std::string& canonicalName, JNIV8ObjectInitializer i,
                                           JNIV8ObjectCreator c, size_t s, JNIV8ClassInfoContainer *baseClassInfo) :
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

JNIV8ClassInfo::JNIV8ClassInfo(JNIV8ClassInfoContainer *container, BGJSV8Engine *engine) :
        container(container), engine(engine), constructorCallback(0), createFromJavaOnly(false) {
}

JNIV8ClassInfo::~JNIV8ClassInfo() {
    for(auto &it : callbackHolders) {
        delete it;
    }
    for(auto &it : accessorHolders) {
        delete it;
    }
    JNIEnv *env = JNIWrapper::getEnvironment();
    for(auto &it : javaAccessorHolders) {
        env->DeleteGlobalRef(it->javaClass);
        if(it->propertyType.clazz) {
            env->DeleteGlobalRef(it->propertyType.clazz);
        }
        delete it;
    }
    for(auto &it : javaCallbackHolders) {
        env->DeleteGlobalRef(it->javaClass);
        for(auto sig : it->signatures) {
            if(!sig.arguments) continue;
            for(auto arg : *sig.arguments) {
                if(arg.clazz) {
                    env->DeleteGlobalRef(arg.clazz);
                }
            }
            delete sig.arguments;
        }
        delete it;
    }
}

void JNIV8ClassInfo::setCreationPolicy(JNIV8ClassCreationPolicy policy) {
    createFromJavaOnly = policy == JNIV8ClassCreationPolicy::NATIVE_ONLY;
}

void JNIV8ClassInfo::registerConstructor(JNIV8ObjectConstructorCallback callback) {
    assert(container->type == JNIV8ObjectType::kPersistent);
    constructorCallback = callback;
}

void JNIV8ClassInfo::registerMethod(const std::string &methodName, JNIV8ObjectMethodCallback callback) {
    JNIV8ObjectCallbackHolder* holder = new JNIV8ObjectCallbackHolder();
    holder->isStatic = false;
    holder->callback.i = callback;
    holder->methodName = methodName;
    _registerMethod(holder);
}

void JNIV8ClassInfo::registerStaticMethod(const std::string& methodName, JNIV8ObjectStaticMethodCallback callback) {
    JNIV8ObjectCallbackHolder* holder = new JNIV8ObjectCallbackHolder();
    holder->isStatic = true;
    holder->callback.s = callback;
    holder->methodName = methodName;
    _registerMethod(holder);
}

void JNIV8ClassInfo::registerJavaMethod(const std::string& methodName, jmethodID methodId, const JNIV8JavaValue& returnType, std::vector<JNIV8JavaArgument> *arguments) {
    // check if this is an overload for a method that is already registered
    for(auto &it : javaCallbackHolders) {
        if(it->methodName == methodName) {
            // make sure that return types match!
            JNI_ASSERTF(returnType.type == it->returnType.type, "Overload for method '%s' of class '%s' has a different return type", methodName.c_str(), container->canonicalName.c_str());
            // register overload
            it->signatures.push_back({methodId, arguments});
            return;
        }
    }

    // otherwise register a new method
    JNIV8ObjectJavaCallbackHolder *holder = new JNIV8ObjectJavaCallbackHolder(returnType);
    holder->methodName = methodName;
    holder->isStatic = false;
    holder->signatures.push_back({methodId, arguments});
    _registerJavaMethod(holder);
}

void JNIV8ClassInfo::registerStaticJavaMethod(const std::string &methodName, jmethodID methodId, const JNIV8JavaValue& returnType, std::vector<JNIV8JavaArgument> *arguments) {
    // check if this is an overload for a method that is already registered
    for(auto &it : javaCallbackHolders) {
        if(it->methodName == methodName) {
            // make sure that return types match!
            JNI_ASSERTF(returnType.type == it->returnType.type, "Overload for static method '%s' of class '%s' has a different return type", methodName.c_str(), container->canonicalName.c_str());
            // register overload
            it->signatures.push_back({methodId, arguments});
            return;
        }
    }

    // otherwise register a new method
    JNIV8ObjectJavaCallbackHolder *holder = new JNIV8ObjectJavaCallbackHolder(returnType);
    holder->methodName = methodName;
    holder->isStatic = true;
    holder->signatures.push_back({methodId, arguments});
    _registerJavaMethod(holder);
}

void JNIV8ClassInfo::registerJavaAccessor(const std::string& propertyName, const JNIV8JavaArgument& propertyType, jmethodID getterId, jmethodID setterId) {
    JNIV8ObjectJavaAccessorHolder* holder = new JNIV8ObjectJavaAccessorHolder(propertyType);
    holder->propertyName = propertyName;
    holder->javaGetterId = getterId;
    holder->javaSetterId = setterId;
    holder->isStatic = false;
    _registerJavaAccessor(holder);
}

void JNIV8ClassInfo::registerStaticJavaAccessor(const std::string &propertyName, const JNIV8JavaArgument& propertyType, jmethodID getterId,
                                             jmethodID setterId) {
    JNIV8ObjectJavaAccessorHolder* holder = new JNIV8ObjectJavaAccessorHolder(propertyType);
    holder->propertyName = propertyName;
    holder->javaGetterId = getterId;
    holder->javaSetterId = setterId;
    holder->isStatic = true;
    holder->propertyType = propertyType;
    _registerJavaAccessor(holder);
}

void JNIV8ClassInfo::registerAccessor(const std::string& propertyName,
                      JNIV8ObjectAccessorGetterCallback getter,
                      JNIV8ObjectAccessorSetterCallback setter) {
    JNIV8ObjectAccessorHolder* holder = new JNIV8ObjectAccessorHolder();
    holder->propertyName = propertyName;
    holder->getterCallback.i = getter;
    holder->setterCallback.i = setter;
    holder->isStatic = false;
    _registerAccessor(holder);
}

void JNIV8ClassInfo::registerStaticAccessor(const std::string &propertyName,
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

void JNIV8ClassInfo::_registerJavaMethod(JNIV8ObjectJavaCallbackHolder *holder) {
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
                         FunctionTemplate::New(isolate, v8JavaMethodCallback, data, Signature::New(isolate, ft), 0, ConstructorBehavior::kThrow));
    }
}

void JNIV8ClassInfo::_registerJavaAccessor(JNIV8ObjectJavaAccessorHolder *holder) {
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

void JNIV8ClassInfo::_registerMethod(JNIV8ObjectCallbackHolder *holder) {
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
        instanceTpl->Set(String::NewFromUtf8(isolate, holder->methodName.c_str()), FunctionTemplate::New(isolate, v8MethodCallback, data, Signature::New(isolate, ft), 0, ConstructorBehavior::kThrow));
    }
}

void JNIV8ClassInfo::_registerAccessor(JNIV8ObjectAccessorHolder *holder) {
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

v8::Local<v8::Object> JNIV8ClassInfo::newInstance() const {
    assert(container->type == JNIV8ObjectType::kPersistent);

    Isolate* isolate = engine->getIsolate();
    Isolate::Scope scope(isolate);
    EscapableHandleScope handleScope(isolate);
    Context::Scope ctxScope(engine->getContext());

    Local<FunctionTemplate> ft = Local<FunctionTemplate>::New(isolate, functionTemplate);

    return handleScope.Escape(ft->InstanceTemplate()->NewInstance());
}

v8::Local<v8::Function> JNIV8ClassInfo::getConstructor() const {
    assert(container->type == JNIV8ObjectType::kPersistent);
    Isolate* isolate = engine->getIsolate();
    EscapableHandleScope scope(isolate);
    auto ft = Local<FunctionTemplate>::New(isolate, functionTemplate);
    return scope.Escape(ft->GetFunction());
}
