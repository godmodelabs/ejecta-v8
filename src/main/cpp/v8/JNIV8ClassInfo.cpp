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

static bool startsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

Local<Name> JNIV8ClassInfo::_makeName(std::string name) {
    Isolate* isolate = engine->getIsolate();
    EscapableHandleScope handleScope(isolate);
    Local<Name> nameRef;

    if(startsWith(name, "string:")) {
        nameRef = String::NewFromUtf8(isolate, name.c_str() + 7,
                                      v8::NewStringType::kNormal).ToLocalChecked().As<Name>();
    } else if(startsWith(name, "symbol:")) {
        if(name == "symbol:ITERATOR") {
            nameRef = v8::Symbol::GetIterator(isolate);
        } else if(name == "symbol:ASYNC_ITERATOR") {
            nameRef = v8::Symbol::GetAsyncIterator(isolate);
        } else if(name == "symbol:MATCH") {
            nameRef = v8::Symbol::GetMatch(isolate);
        } else if(name == "symbol:REPLACE") {
            nameRef = v8::Symbol::GetReplace(isolate);
        } else if(name == "symbol:SEARCH") {
            nameRef = v8::Symbol::GetSearch(isolate);
        } else if(name == "symbol:SPLIT") {
            nameRef = v8::Symbol::GetSplit(isolate);
        } else if(name == "symbol:HAS_INSTANCE") {
            nameRef = v8::Symbol::GetHasInstance(isolate);
        } else if(name == "symbol:IS_CONCAT_SPREADABLE") {
            nameRef = v8::Symbol::GetIsConcatSpreadable(isolate);
        } else if(name == "symbol:UNSCOPABLES") {
            nameRef = v8::Symbol::GetUnscopables(isolate);
        } else if(name == "symbol:TO_PRIMITIVE") {
            nameRef = v8::Symbol::GetToPrimitive(isolate);
        } else if(name == "symbol:TO_STRING_TAG") {
            nameRef = v8::Symbol::GetToStringTag(isolate);
        }
    }

    JNI_ASSERTF(!nameRef.IsEmpty(), "Encountered invalid property name '%s'", name.c_str());

    return handleScope.Escape(nameRef);
}

std::string JNIV8ClassInfo::_makeSymbolString(EJNIV8ObjectSymbolType symbol) {
    switch (symbol) {
        case kIterator:
            return "symbol:ITERATOR";
        case kAsyncIterator:
            return "symbol:ASYNC_ITERATOR";
        case kMatch:
            return "symbol:MATCH";
        case kReplace:
            return "symbol:REPLACE";
        case kSearch:
            return "symbol:SEARCH";
        case kSplit:
            return "symbol:SPLIT";
        case kHasInstance:
            return "symbol:HAS_INSTANCE";
        case kIsConcatSpreadable:
            return "symbol:IS_CONCAT_SPREADABLE";
        case kUnscopables:
            return "symbol:UNSCOPABLES";
        case kToPrimitive:
            return "symbol:TO_PRIMITIVE";
        case kToStringTag:
            return "symbol:TO_STRING_TAG";
    }
}

decltype(JNIV8ClassInfo::_jniObject) JNIV8ClassInfo::_jniObject = {0};

/**
 * cache JNI class references
 */
void JNIV8ClassInfo::initJNICache() {
    JNIEnv *env = JNIWrapper::getEnvironment();
    _jniObject.clazz = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Object"));
}

void JNIV8ClassInfo::v8JavaAccessorGetterCallback(Local<Name> property, const PropertyCallbackInfo<Value> &info) {
    JNIEnv *env = JNIWrapper::getEnvironment();
    JNILocalFrame localFrame(env, 1);

    Isolate *isolate = info.GetIsolate();
    HandleScope scope(isolate);

    v8::Local<v8::External> ext;

    ext = info.Data().As<v8::External>();
    auto cb = static_cast<JNIV8ObjectJavaAccessorHolder*>(ext->Value());
    if (cb->javaGetterId) {
        jobject jobj = nullptr;

        if (!cb->isStatic) {
            ext = info.This()->GetInternalField(0).As<v8::External>();
            auto *v8Object = reinterpret_cast<JNIV8Object *>(ext->Value());

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


void JNIV8ClassInfo::v8JavaAccessorSetterCallback(Local<Name> property, Local<Value> value, const PropertyCallbackInfo<void> &info) {
    JNIEnv *env = JNIWrapper::getEnvironment();
    JNILocalFrame localFrame(env, 1);

    Isolate *isolate = info.GetIsolate();
    HandleScope scope(isolate);

    v8::Local<v8::External> ext;

    ext = info.Data().As<v8::External>();
    auto * cb = static_cast<JNIV8ObjectJavaAccessorHolder*>(ext->Value());

    if (cb->javaSetterId) {
        jobject jobj = nullptr;
        jvalue jval;
        memset(&jval, 0, sizeof(jvalue));

        if (!cb->isStatic) {
            ext = info.This()->GetInternalField(0).As<v8::External>();
            auto *v8Object = reinterpret_cast<JNIV8Object *>(ext->Value());

            jobj = v8Object->getJObject();
        }

        JNIV8MarshallingError res;
        res = JNIV8Marshalling::convertV8ValueToJavaValue(env, value, cb->propertyType, &jval);
        if(res != JNIV8MarshallingError::kOk) {
            switch(res) {
                default:
                case JNIV8MarshallingError::kWrongType:
                    ThrowV8TypeError("wrong type for property '" + cb->propertyName);
                    break;
                case JNIV8MarshallingError::kUndefined:
                    ThrowV8TypeError("property '" + cb->propertyName + "' must not be undefined");
                    break;
                case JNIV8MarshallingError::kNotNullable:
                    ThrowV8TypeError("property '" + cb->propertyName + "' is not nullable");
                    break;
                case JNIV8MarshallingError::kNoNaN:
                    ThrowV8TypeError("property '" + cb->propertyName + "' must not be NaN");
                    break;
                case JNIV8MarshallingError::kVoidNotNull:
                    ThrowV8TypeError("property '" + cb->propertyName + "' can only be null or undefined");
                    break;
                case JNIV8MarshallingError::kOutOfRange:
                    ThrowV8RangeError("assigned value '"+
                                      JNIV8Marshalling::v8string2string(value->ToString(isolate->GetCurrentContext()).ToLocalChecked())+"' is out of range for property '" + cb->propertyName + "'");
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
    JNIEnv *env = JNIWrapper::getEnvironment();
    JNILocalFrame localFrame(env);

    HandleScope scope(args.GetIsolate());
    Isolate *isolate = args.GetIsolate();

    jobject jobj = nullptr;

    v8::Local<v8::External> ext;
    ext = args.Data().As<v8::External>();
    auto * cb = static_cast<JNIV8ObjectJavaCallbackHolder*>(ext->Value());

    // we only check the "this" for non-static methods
    // otherwise "this" can be anything, we do not care..
    if(!cb->isStatic) {
        v8::Local<v8::Object> thisArg = args.This();
        v8::Local<v8::Value> internalField = thisArg->GetInternalField(0);
        // this is not really "safe".. but how could it be? another part of the program could store arbitrary stuff in internal fields
        ext = internalField.As<v8::External>();
        auto *v8Object = reinterpret_cast<JNIV8Object *>(ext->Value());
        jobj = v8Object->getJObject();
    }

    // try to find a matching signature
    JNIV8ObjectJavaSignatureInfo *signature = nullptr;

    for (auto& sig : cb->signatures) {
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
        isolate->ThrowException(v8::Exception::TypeError(String::NewFromUtf8(isolate, ("invalid number of arguments (" + std::to_string(args.Length()) + ") supplied to " + cb->methodName).c_str()).ToLocalChecked()));
        return;
    }

    jvalue *jargs;
    jobject obj;
    size_t numJArgs;

    if(!signature->arguments) {
        // generic case: an array of objects!
        // nothing to validate here, this always works
        numJArgs = 1;
        jargs = (jvalue*)malloc(sizeof(jvalue)*numJArgs);
        memset(jargs, 0, sizeof(jvalue)*numJArgs);
        jobjectArray jArray = env->NewObjectArray(args.Length(), _jniObject.clazz, nullptr);
        for (int idx = 0, n = args.Length(); idx < n; idx++) {
            obj = JNIV8Marshalling::v8value2jobject(args[idx]);
            env->SetObjectArrayElement(jArray, idx, obj);
            env->DeleteLocalRef(obj);
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
                JNIV8JavaValue &arg = (*signature->arguments)[idx];
                v8::Local<v8::Value> value = args[idx];

                JNIV8MarshallingError res = JNIV8Marshalling::convertV8ValueToJavaValue(env, value, arg, &(jargs[idx]));
                if(res != JNIV8MarshallingError::kOk) {
                    // conversion failed => simply clean up & throw an exception
                    free(jargs);
                    switch(res) {
                        default:
                        case JNIV8MarshallingError::kWrongType:
                            ThrowV8TypeError("wrong type for argument #" + std::to_string(idx) + " of '" + cb->methodName + "'");
                            break;
                        case JNIV8MarshallingError::kUndefined:
                            ThrowV8TypeError("argument #" + std::to_string(idx) + " of '" + cb->methodName + "' does not accept undefined");
                            break;
                        case JNIV8MarshallingError::kNotNullable:
                            ThrowV8TypeError("argument #" + std::to_string(idx) + " of '" + cb->methodName + "' is not nullable");
                            break;
                        case JNIV8MarshallingError::kNoNaN:
                            ThrowV8TypeError("argument #" + std::to_string(idx) + " of '" + cb->methodName + "' must not be NaN");
                            break;
                        case JNIV8MarshallingError::kVoidNotNull:
                            ThrowV8TypeError("argument #" + std::to_string(idx) + " of '" + cb->methodName + "' must be null or undefined");
                            break;
                        case JNIV8MarshallingError::kOutOfRange:
                            ThrowV8RangeError("value '"+
                                              JNIV8Marshalling::v8string2string(value->ToString(isolate->GetCurrentContext()).ToLocalChecked())+"' is out of range for argument #" + std::to_string(idx) + " of '" + cb->methodName + "'");
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

void JNIV8ClassInfo::v8AccessorGetterCallback(Local<Name> property, const PropertyCallbackInfo<Value> &info) {
    JNIEnv *env = JNIWrapper::getEnvironment();
    JNILocalFrame localFrame(env);

    HandleScope scope(info.GetIsolate());

    v8::Local<v8::External> ext;

    ext = info.Data().As<v8::External>();
    auto * cb = static_cast<JNIV8ObjectAccessorHolder*>(ext->Value());

    if(cb->isStatic) {
        (cb->getterCallback.s)(cb->propertyName, info);
    } else {
        ext = info.This()->GetInternalField(0).As<v8::External>();
        auto *v8Object = reinterpret_cast<JNIV8Object *>(ext->Value());

        (v8Object->*(cb->getterCallback.i))(cb->propertyName, info);
    }
}


void JNIV8ClassInfo::v8AccessorSetterCallback(Local<Name> property, Local<Value> value, const PropertyCallbackInfo<void> &info) {
    JNIEnv *env = JNIWrapper::getEnvironment();
    JNILocalFrame localFrame(env);

    HandleScope scope(info.GetIsolate());

    v8::Local<v8::External> ext;

    ext = info.Data().As<v8::External>();
    auto * cb = static_cast<JNIV8ObjectAccessorHolder*>(ext->Value());

    if(cb->isStatic) {
        (cb->setterCallback.s)(cb->propertyName, value, info);
    } else {
        ext = info.This()->GetInternalField(0).As<v8::External>();
        auto *v8Object = reinterpret_cast<JNIV8Object *>(ext->Value());

        (v8Object->*(cb->setterCallback.i))(cb->propertyName, value, info);
    }
}

void JNIV8ClassInfo::v8MethodCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
    JNIEnv *env = JNIWrapper::getEnvironment();
    JNILocalFrame localFrame(env);

    HandleScope scope(args.GetIsolate());

    v8::Local<v8::External> ext;

    ext = args.Data().As<v8::External>();
    auto * cb = static_cast<JNIV8ObjectCallbackHolder*>(ext->Value());

    if(cb->isStatic) {
        // we do NOT check how this function was invoked.. if a this was supplied, we just ignore it!
        (cb->callback.s)(cb->methodName, args);
    } else {
        ext = args.This()->GetInternalField(0).As<v8::External>();
        auto *v8Object = reinterpret_cast<JNIV8Object *>(ext->Value());

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

    // if creation from native/javascript is allowed, we need the special constructor!
    bool createFromJavaOnly = false;
    if(clsBinding) {
        jfieldID createFromJavaOnlyId = env->GetStaticFieldID(clsBinding, "createFromJavaOnly", "Z");
        createFromJavaOnly = env->GetStaticBooleanField(clsBinding, createFromJavaOnlyId);
    }
    if(!createFromJavaOnly) {
        jmethodID constructorId = env->GetMethodID(clsObject, "<init>",
                                         "(Lag/boersego/bgjs/V8Engine;J[Ljava/lang/Object;)V");
        JNI_ASSERTF(constructorId,
                    "Constructor '(V8Engine, long, Object[])' does not exist on registered class '%s';\n"
                    "Possible cause: non-static nested classes are not supported!",
                    "Possible cause: missing class creating policy!",
                    canonicalName.c_str());
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
    auto * holder = new JNIV8ObjectCallbackHolder();
    holder->isStatic = false;
    holder->callback.i = callback;
    holder->methodName = "string:" + methodName;
    _registerMethod(holder);
}

void JNIV8ClassInfo::registerStaticMethod(const std::string& methodName, JNIV8ObjectStaticMethodCallback callback) {
    auto * holder = new JNIV8ObjectCallbackHolder();
    holder->isStatic = true;
    holder->callback.s = callback;
    holder->methodName = "string:" + methodName;
    _registerMethod(holder);
}

void JNIV8ClassInfo::registerMethod(const EJNIV8ObjectSymbolType symbol, JNIV8ObjectMethodCallback callback) {
    auto * holder = new JNIV8ObjectCallbackHolder();
    holder->isStatic = false;
    holder->callback.i = callback;
    holder->methodName = _makeSymbolString(symbol);
    _registerMethod(holder);
}

void JNIV8ClassInfo::registerStaticMethod(const EJNIV8ObjectSymbolType symbol, JNIV8ObjectStaticMethodCallback callback) {
    auto * holder = new JNIV8ObjectCallbackHolder();
    holder->isStatic = true;
    holder->callback.s = callback;
    holder->methodName = _makeSymbolString(symbol);
    _registerMethod(holder);
}

void JNIV8ClassInfo::registerJavaMethod(const std::string& methodName, jmethodID methodId, const JNIV8JavaValue& returnType, std::vector<JNIV8JavaValue> *arguments) {
    // check if this is an overload for a method that is already registered
    for(auto &it : javaCallbackHolders) {
        if(it->methodName == methodName) {
            // make sure that return types match!
            JNI_ASSERTF(returnType.valueType == it->returnType.valueType && JNIWrapper::getEnvironment()->IsSameObject(returnType.clazz, it->returnType.clazz),
                        "Overload for method '%s' of class '%s' has a different return type", methodName.c_str(), container->canonicalName.c_str());
            // register overload
            it->signatures.push_back({methodId, arguments});
            return;
        }
    }

    // otherwise register a new method
    auto *holder = new JNIV8ObjectJavaCallbackHolder(returnType);
    holder->methodName = methodName;
    holder->isStatic = false;
    holder->signatures.push_back({methodId, arguments});
    _registerJavaMethod(holder);
}

void JNIV8ClassInfo::registerStaticJavaMethod(const std::string &methodName, jmethodID methodId, const JNIV8JavaValue& returnType, std::vector<JNIV8JavaValue> *arguments) {
    // check if this is an overload for a method that is already registered
    for(auto &it : javaCallbackHolders) {
        if(it->methodName == methodName) {
            // make sure that return types match!
            JNI_ASSERTF(returnType.valueType == it->returnType.valueType && JNIWrapper::getEnvironment()->IsSameObject(returnType.clazz, it->returnType.clazz),
                        "Overload for method '%s' of class '%s' has a different return type", methodName.c_str(), container->canonicalName.c_str());
            // register overload
            it->signatures.push_back({methodId, arguments});
            return;
        }
    }

    // otherwise register a new method
    auto *holder = new JNIV8ObjectJavaCallbackHolder(returnType);
    holder->methodName = methodName;
    holder->isStatic = true;
    holder->signatures.push_back({methodId, arguments});
    _registerJavaMethod(holder);
}

void JNIV8ClassInfo::registerJavaAccessor(const std::string& propertyName, const JNIV8JavaValue& propertyType, jmethodID getterId, jmethodID setterId) {
    JNIV8ObjectJavaAccessorHolder* holder = new JNIV8ObjectJavaAccessorHolder(propertyType);
    holder->propertyName = propertyName;
    holder->javaGetterId = getterId;
    holder->javaSetterId = setterId;
    holder->isStatic = false;
    _registerJavaAccessor(holder);
}

void JNIV8ClassInfo::registerStaticJavaAccessor(const std::string &propertyName, const JNIV8JavaValue& propertyType, jmethodID getterId,
                                             jmethodID setterId) {
    auto * holder = new JNIV8ObjectJavaAccessorHolder(propertyType);
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
    auto * holder = new JNIV8ObjectAccessorHolder();
    holder->propertyName = "string:" + propertyName;
    holder->getterCallback.i = getter;
    holder->setterCallback.i = setter;
    holder->isStatic = false;
    _registerAccessor(holder);
}

void JNIV8ClassInfo::registerStaticAccessor(const std::string &propertyName,
                                         JNIV8ObjectStaticAccessorGetterCallback getter,
                                         JNIV8ObjectStaticAccessorSetterCallback setter) {
    auto * holder = new JNIV8ObjectAccessorHolder();
    holder->propertyName = "string:" + propertyName;
    holder->getterCallback.s = getter;
    holder->setterCallback.s = setter;
    holder->isStatic = true;
    accessorHolders.push_back(holder);
    _registerAccessor(holder);
}

void JNIV8ClassInfo::registerAccessor(const EJNIV8ObjectSymbolType symbol,
                                      JNIV8ObjectAccessorGetterCallback getter,
                                      JNIV8ObjectAccessorSetterCallback setter) {
    auto * holder = new JNIV8ObjectAccessorHolder();
    holder->propertyName = _makeSymbolString(symbol);
    holder->getterCallback.i = getter;
    holder->setterCallback.i = setter;
    holder->isStatic = false;
    _registerAccessor(holder);
}

void JNIV8ClassInfo::registerStaticAccessor(const EJNIV8ObjectSymbolType symbol,
                                            JNIV8ObjectStaticAccessorGetterCallback getter,
                                            JNIV8ObjectStaticAccessorSetterCallback setter) {
    auto * holder = new JNIV8ObjectAccessorHolder();
    holder->propertyName = _makeSymbolString(symbol);
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
    holder->javaClass = (jclass)env->NewGlobalRef(env->FindClass(container->canonicalName.c_str()));
    javaCallbackHolders.push_back(holder);

    Local<External> data = External::New(isolate, (void*)holder);

    Local<Name> nameRef = _makeName(holder->methodName);

    if(holder->isStatic) {
        Local<Function> f = ft->GetFunction(isolate->GetCurrentContext()).ToLocalChecked();
        f->Set(isolate->GetCurrentContext(), nameRef, FunctionTemplate::New(isolate, v8JavaMethodCallback, data, Local<Signature>(), 0, ConstructorBehavior::kThrow)->GetFunction(isolate->GetCurrentContext()).ToLocalChecked());
    } else {
        // ofc functions belong on the prototype, and not on the actual instance for performance/memory reasons
        // but interestingly enough, we MUST store them there because they simply are not "copied" from the InstanceTemplate when using inherit later
        // but other properties and accessors are copied without problems.
        // Thought: it is not allowed to store actual Functions on these templates - only FunctionTemplates
        // functions can only exist in a context once and probaly can not be duplicated/copied in the same way as scalars and accessors, so there IS a difference.
        // maybe when doing inherit the function template is instanced, and then inherit copies over properties to its own instance template which can not be done for instanced functions..
        Local<ObjectTemplate> instanceTpl = ft->PrototypeTemplate();
        instanceTpl->Set(nameRef,
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

    AccessorNameSetterCallback finalSetter = 0;
    v8::PropertyAttribute settings = v8::PropertyAttribute::None;
    if(holder->javaSetterId) {
        finalSetter = v8JavaAccessorSetterCallback;
    } else {
        settings = v8::PropertyAttribute::ReadOnly;
    }

    Local<Name> nameRef = _makeName(holder->propertyName);

    if(holder->isStatic) {
        Local<Function> f = ft->GetFunction(isolate->GetCurrentContext()).ToLocalChecked();
        f->SetAccessor(engine->getContext(), nameRef,
                       v8JavaAccessorGetterCallback, finalSetter,
                       data, DEFAULT, settings);
    } else {
        Local<ObjectTemplate> instanceTpl = ft->InstanceTemplate();
        instanceTpl->SetAccessor(nameRef,
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

    Local<Name> nameRef = _makeName(holder->methodName);

    if(holder->isStatic) {
        Local<Function> f = ft->GetFunction(isolate->GetCurrentContext()).ToLocalChecked();
        f->Set(isolate->GetCurrentContext(), nameRef,
               FunctionTemplate::New(isolate, v8MethodCallback, data, Local<Signature>(), 0, ConstructorBehavior::kThrow)->GetFunction(isolate->GetCurrentContext()).ToLocalChecked());
    } else {
        // ofc functions belong on the prototype, and not on the actual instance for performance/memory reasons
        // but interestingly enough, we MUST store them there because they simply are not "copied" from the InstanceTemplate when using inherit later
        // but other properties and accessors are copied without problems.
        // Thought: it is not allowed to store actual Functions on these templates - only FunctionTemplates
        // functions can only exist in a context once and probaly can not be duplicated/copied in the same way as scalars and accessors, so there IS a difference.
        // maybe when doing inherit the function template is instanced, and then inherit copies over properties to its own instance template which can not be done for instanced functions..
        Local<ObjectTemplate> instanceTpl = ft->PrototypeTemplate();
        instanceTpl->Set(nameRef, FunctionTemplate::New(isolate, v8MethodCallback, data, Signature::New(isolate, ft), 0, ConstructorBehavior::kThrow));
    }
}

void JNIV8ClassInfo::_registerAccessor(JNIV8ObjectAccessorHolder *holder) {
    Isolate* isolate = engine->getIsolate();
    HandleScope scope(isolate);

    Local<FunctionTemplate> ft = Local<FunctionTemplate>::New(isolate, functionTemplate);

    accessorHolders.push_back(holder);
    Local<External> data = External::New(isolate, (void*)holder);

    AccessorNameSetterCallback finalSetter = 0;
    v8::PropertyAttribute settings = v8::PropertyAttribute::None;
    if(holder->setterCallback.i || holder->setterCallback.s) {
        finalSetter = v8AccessorSetterCallback;
    } else {
        settings = v8::PropertyAttribute::ReadOnly;
    }

    Local<Name> nameRef = _makeName(holder->propertyName);

    if(holder->isStatic) {
        Local<Function> f = ft->GetFunction(isolate->GetCurrentContext()).ToLocalChecked();
        f->SetAccessor(engine->getContext(), nameRef,
                       v8AccessorGetterCallback, finalSetter,
                       data, DEFAULT, settings);
    } else {
        Local<ObjectTemplate> instanceTpl = ft->InstanceTemplate();
        instanceTpl->SetAccessor(nameRef,
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

    return handleScope.Escape(ft->InstanceTemplate()->NewInstance(isolate->GetCurrentContext()).ToLocalChecked());
}

v8::Local<v8::Function> JNIV8ClassInfo::getConstructor() const {
    assert(container->type == JNIV8ObjectType::kPersistent);
    Isolate* isolate = engine->getIsolate();
    EscapableHandleScope scope(isolate);
    auto ft = Local<FunctionTemplate>::New(isolate, functionTemplate);
    return scope.Escape(ft->GetFunction(isolate->GetCurrentContext()).ToLocalChecked());
}
