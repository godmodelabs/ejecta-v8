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
decltype(V8ClassInfo::_jniString) V8ClassInfo::_jniString = {0};

/**
 * cache JNI class references
 */
void V8ClassInfo::initJNICache() {
    JNIEnv *env = JNIWrapper::getEnvironment();
    _jniObject.clazz = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Object"));
    _jniString.clazz = (jclass)env->NewGlobalRef(env->FindClass("java/lang/String"));
}

bool V8ClassInfo::_convertArgument(JNIEnv *env, v8::Local<v8::Value> v8Value, JNIV8ObjectJavaArgument arg, jvalue *target) {
    if(arg.clazz) {
        // special case: undefined is treated as null, because we can't differentiate in java/kotlin
        if(v8Value->IsUndefined() && arg.undefinedIsNull) {
            target->l = nullptr;
            // special case: strings are objects, but they also need coercion!
        } else if(env->IsSameObject(arg.clazz, _jniString.clazz)) {
            target->l = JNIV8Wrapper::v8value2jobject(v8Value->ToString());
        } else {
            target->l = JNIV8Wrapper::v8value2jobject(v8Value);
        }
        if(env->IsSameObject(target->l, nullptr)) {
            if(!arg.isNullable) {
                v8::Isolate::GetCurrent()->ThrowException(v8::Exception::TypeError(
                        String::NewFromUtf8(v8::Isolate::GetCurrent(), "argument is not nullable")));
                return false;
            }
        } else if(!env->IsInstanceOf(target->l, arg.clazz)) {
            v8::Isolate::GetCurrent()->ThrowException(v8::Exception::TypeError(String::NewFromUtf8(v8::Isolate::GetCurrent(), "invalid argument type")));
            return false;
        }
    } else {
        switch (arg.type[0]) {
            case 'Z': // boolean
                target->z = (jboolean) v8Value->ToBoolean()->BooleanValue();
                break;
            case 'B': // byte
                target->b = (jbyte) v8Value->ToNumber()->Int32Value();
                break;
            case 'C': // char
                target->c = (jchar) BGJS_STRING_FROM_V8VALUE(v8Value)[0];
                break;
            case 'S': // short
                target->s = (jshort) v8Value->ToNumber()->Int32Value();
                break;
            case 'I': // integer
                target->i = (jint) v8Value->ToNumber()->Int32Value();
                break;
            case 'J': // long
                target->j = (jlong) v8Value->ToNumber()->Int32Value();
                break;
            case 'F': // float
                target->f = (jfloat) v8Value->ToNumber()->Value();
                break;
            case 'D': // double
                target->d = (jdouble) v8Value->ToNumber()->Value();
                break;
            default:
                v8::Isolate::GetCurrent()->ThrowException(v8::Exception::TypeError(
                        String::NewFromUtf8(v8::Isolate::GetCurrent(),
                                            "invalid argument type")));
                return false;
        }
    }
    return true;
}

v8::Local<v8::Value> V8ClassInfo::_callJavaMethod(JNIEnv *env, JNIV8ObjectJavaArgument returnType, jclass clazz, jmethodID methodId, jobject object, jvalue *args) {
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::EscapableHandleScope handleScope(isolate);

    v8::Local<v8::Value> result;

    if(returnType.clazz) {
        if(object) {
            result = JNIV8Wrapper::jobject2v8value(env->CallObjectMethod(object, methodId, args));
        } else {
            result = JNIV8Wrapper::jobject2v8value(env->CallStaticObjectMethod(clazz, methodId, args));
        }
    } else {
        switch (returnType.type[0]) {
            case 'Z': // boolean
                result = v8::Boolean::New(isolate,
                                          object ? env->CallBooleanMethod(object, methodId, args) :
                                          env->CallStaticBooleanMethod(clazz,
                                                                       methodId, args));
                break;
            case 'B': // byte
                result = v8::Number::New(isolate,
                                         object ? env->CallByteMethod(object, methodId, args) :
                                         env->CallStaticByteMethod(clazz, methodId, args));
                break;
            case 'C': // char
                result = v8::Number::New(isolate,
                                         object ? env->CallCharMethod(object, methodId, args) :
                                         env->CallStaticCharMethod(clazz, methodId, args));
                break;
            case 'S': // short
                result = v8::Number::New(isolate,
                                         object ? env->CallShortMethod(object, methodId, args) :
                                         env->CallStaticShortMethod(clazz,
                                                                    methodId, args));
                break;
            case 'I': // integer
                result = v8::Number::New(isolate,
                                         object ? env->CallIntMethod(object, methodId, args) :
                                         env->CallStaticIntMethod(clazz, methodId, args));
                break;
            case 'J': // long
                result = v8::Number::New(isolate,
                                         object ? env->CallLongMethod(object, methodId, args) :
                                         env->CallStaticLongMethod(clazz, methodId, args));
                break;
            case 'F': // float
                result = v8::Number::New(isolate,
                                         object ? env->CallFloatMethod(object, methodId, args) :
                                         env->CallStaticFloatMethod(clazz,
                                                                    methodId, args));
                break;
            case 'D': // double
                result = v8::Number::New(isolate,
                                         object ? env->CallDoubleMethod(object, methodId, args) :
                                         env->CallStaticDoubleMethod(clazz,
                                                                     methodId, args));
                break;
            case 'V': // void
                if(object) {
                    env->CallVoidMethod(object, methodId, args);
                } else {
                    env->CallStaticVoidMethod(clazz, methodId, args);
                }
                result = v8::Undefined(isolate);
                break;
            default:
                isolate->ThrowException(v8::Exception::TypeError(
                        String::NewFromUtf8(isolate, "return type unknown")));
                return v8::Undefined(isolate);
        }
    }

    return handleScope.Escape(result);
}

void V8ClassInfo::v8JavaAccessorGetterCallback(Local<String> property, const PropertyCallbackInfo<Value> &info) {
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

        value = _callJavaMethod(env, cb->propertyType, cb->javaClass, cb->javaGetterId, jobj, nullptr);

        // java method could have thrown an exception; if so forward it to v8
        if(env->ExceptionCheck()) {
            BGJS_CURRENT_V8ENGINE(isolate)->forwardJNIExceptionToV8();
            return;
        }

        info.GetReturnValue().Set(value);
    }
}


void V8ClassInfo::v8JavaAccessorSetterCallback(Local<String> property, Local<Value> value, const PropertyCallbackInfo<void> &info) {
    Isolate *isolate = info.GetIsolate();
    HandleScope scope(isolate);

    v8::Local<v8::External> ext;

    ext = info.Data().As<v8::External>();
    JNIV8ObjectJavaAccessorHolder* cb = static_cast<JNIV8ObjectJavaAccessorHolder*>(ext->Value());

    if (cb->javaSetterId) {
        jobject jobj = nullptr;
        jvalue jvalue = {0};
        JNIEnv *env = JNIWrapper::getEnvironment();
        if (!cb->isStatic) {
            ext = info.This()->GetInternalField(0).As<v8::External>();
            JNIV8Object *v8Object = reinterpret_cast<JNIV8Object *>(ext->Value());

            jobj = v8Object->getJObject();
        }

        if(!_convertArgument(env, value, cb->propertyType, &jvalue)) {
            // conversion failed => a v8 exception was thrown, simply clean up & return
            return;
        }

        if (jobj) {
            env->CallVoidMethod(jobj, cb->javaSetterId, jvalue);
        } else {
            env->CallStaticVoidMethod(cb->javaClass, cb->javaSetterId, jvalue);
        }

        // java method could have thrown an exception; if so forward it to v8
        if(env->ExceptionCheck()) {
            BGJS_CURRENT_V8ENGINE(isolate)->forwardJNIExceptionToV8();
            return;
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
            isolate->ThrowException(v8::Exception::TypeError(String::NewFromUtf8(isolate, "invalid invocation")));
            return;
        }
        v8::Local<v8::Value> internalField = thisArg->GetInternalField(0);
        if (!internalField->IsExternal()) {
            isolate->ThrowException(v8::Exception::TypeError(String::NewFromUtf8(isolate, "invalid invocation")));
            return;
        }

        // this is not really "safe".. but how could it be? another part of the program could store arbitrary stuff in internal fields
        ext = internalField.As<v8::External>();
        JNIV8Object *v8Object = reinterpret_cast<JNIV8Object *>(ext->Value());
        jobj = v8Object->getJObject();

        if (!env->IsInstanceOf(jobj, cb->javaClass)) {
            isolate->ThrowException(v8::Exception::TypeError(String::NewFromUtf8(isolate, "invalid invocation")));
            return;
        }
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
        isolate->ThrowException(v8::Exception::TypeError(String::NewFromUtf8(isolate, "invalid arguments")));
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
            env->SetObjectArrayElement(jArray, idx, JNIV8Wrapper::v8value2jobject(args[idx]));
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
                JNIV8ObjectJavaArgument &arg = (*signature->arguments)[idx];
                v8::Local<v8::Value> value = args[idx];

                if(!_convertArgument(env, value, arg, &jargs[idx])) {
                    // conversion failed => a v8 exception was thrown, simply clean up & return
                    free(jargs);
                    return;
                }
            }
        } else {
            jargs = nullptr;
        }
    }

    v8::Local<v8::Value> result;

    result = _callJavaMethod(env, cb->returnType, cb->javaClass, signature->javaMethodId, jobj, jargs);

    if(jargs) {
        free(jargs);
    }

    // java method could have thrown an exception; if so forward it to v8
    if(env->ExceptionCheck()) {
        BGJS_CURRENT_V8ENGINE(isolate)->forwardJNIExceptionToV8();
        return;
    }

    args.GetReturnValue().Set(result);
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

void V8ClassInfo::registerJavaMethod(const std::string& methodName, jmethodID methodId, const JNIV8ObjectJavaArgument& returnType, std::vector<JNIV8ObjectJavaArgument> *arguments) {
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
    JNIV8ObjectJavaCallbackHolder* holder = new JNIV8ObjectJavaCallbackHolder();
    holder->methodName = methodName;
    holder->isStatic = false;
    holder->returnType = returnType;
    holder->signatures.push_back({methodId, arguments});

    _registerJavaMethod(holder);
}

void V8ClassInfo::registerStaticJavaMethod(const std::string &methodName, jmethodID methodId, const JNIV8ObjectJavaArgument& returnType, std::vector<JNIV8ObjectJavaArgument> *arguments) {
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
    JNIV8ObjectJavaCallbackHolder* holder = new JNIV8ObjectJavaCallbackHolder();
    holder->methodName = methodName;
    holder->isStatic = true;
    holder->returnType = returnType;
    holder->signatures.push_back({methodId, arguments});
    _registerJavaMethod(holder);
}

void V8ClassInfo::registerJavaAccessor(const std::string& propertyName, const JNIV8ObjectJavaArgument& propertyType, jmethodID getterId, jmethodID setterId) {
    JNIV8ObjectJavaAccessorHolder* holder = new JNIV8ObjectJavaAccessorHolder();
    holder->propertyName = propertyName;
    holder->javaGetterId = getterId;
    holder->javaSetterId = setterId;
    holder->isStatic = false;
    holder->propertyType = propertyType;
    _registerJavaAccessor(holder);
}

void V8ClassInfo::registerStaticJavaAccessor(const std::string &propertyName, const JNIV8ObjectJavaArgument& propertyType, jmethodID getterId,
                                             jmethodID setterId) {
    JNIV8ObjectJavaAccessorHolder* holder = new JNIV8ObjectJavaAccessorHolder();
    holder->propertyName = propertyName;
    holder->javaGetterId = getterId;
    holder->javaSetterId = setterId;
    holder->isStatic = true;
    holder->propertyType = propertyType;
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
