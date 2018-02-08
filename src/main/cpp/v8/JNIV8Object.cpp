//
// Created by Martin Kleinhans on 18.08.17.
//

#include "JNIV8Object.h"
#include "JNIV8Wrapper.h"
#include "../bgjs/BGJSV8Engine.h"

#include <stdlib.h>

#define LOG_TAG "JNIV8Object"

using namespace v8;

BGJS_JNI_LINK(JNIV8Object, "ag/boersego/bgjs/JNIV8Object");

decltype(JNIV8Object::_jniObject) JNIV8Object::_jniObject = {0};
decltype(JNIV8Object::_jniString) JNIV8Object::_jniString = {0};
decltype(JNIV8Object::_jniHashMap) JNIV8Object::_jniHashMap = {0};
decltype(JNIV8Object::_jniMap) JNIV8Object::_jniMap = {0};
decltype(JNIV8Object::_jniMapEntry) JNIV8Object::_jniMapEntry = {0};
decltype(JNIV8Object::_jniIterator) JNIV8Object::_jniIterator = {0};
decltype(JNIV8Object::_jniSet) JNIV8Object::_jniSet = {0};

/**
 * cache JNI class references
 */
void JNIV8Object::initJNICache() {
    JNIEnv *env = JNIWrapper::getEnvironment();

    _jniString.clazz = (jclass)env->NewGlobalRef(env->FindClass("java/lang/String"));

    _jniObject.clazz = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Object"));

    _jniHashMap.clazz = (jclass)env->NewGlobalRef(env->FindClass("java/util/HashMap"));
    _jniHashMap.initId = env->GetMethodID(_jniHashMap.clazz, "<init>", "()V");
    _jniHashMap.putId = env->GetMethodID(_jniHashMap.clazz, "put",
                                         "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");

    _jniMap.clazz = (jclass)env->NewGlobalRef(env->FindClass("java/util/Map"));
    _jniMap.entrySetId = env->GetMethodID(_jniMap.clazz, "entrySet", "()Ljava/util/Set;");

    _jniMapEntry.clazz = (jclass)env->NewGlobalRef(env->FindClass("java/util/Map$Entry"));
    _jniMapEntry.getKeyId = env->GetMethodID(_jniMapEntry.clazz, "getKey", "()Ljava/lang/Object;");
    _jniMapEntry.getValueId = env->GetMethodID(_jniMapEntry.clazz, "getValue", "()Ljava/lang/Object;");

    _jniIterator.clazz = (jclass)env->NewGlobalRef(env->FindClass("java/util/Iterator"));
    _jniIterator.hasNextId = env->GetMethodID(_jniIterator.clazz, "hasNext", "()Z");
    _jniIterator.nextId = env->GetMethodID(_jniIterator.clazz, "next", "()Ljava/lang/Object;");

    _jniSet.clazz = (jclass)env->NewGlobalRef(env->FindClass("java/util/Set"));
    _jniSet.iteratorId = env->GetMethodID(_jniSet.clazz, "iterator", "()Ljava/util/Iterator;");
}

JNIV8Object::JNIV8Object(jobject obj, JNIClassInfo *info) : JNIObject(obj, info) {
    _externalMemory = 0;
    // __android_log_print(ANDROID_LOG_INFO, "JNIV8Object", "created v8 object: %s", getCanonicalName().c_str());
}

JNIV8Object::~JNIV8Object() {
    // __android_log_print(ANDROID_LOG_INFO, "JNIV8Object", "deleted v8 object: %s", getCanonicalName().c_str());
    if(!_jsObject.IsEmpty()) {
        // adjust external memory counter if required
        JNI_ASSERT(!_jsObject.IsWeak(), "JNIV8Object deleted while still referenced by JavaScript");
        _jsObject.Reset();
    }
}

void JNIV8Object::weakPersistentCallback(const WeakCallbackInfo<void>& data) {
    // never use the raw pointer directly; this way we are retaining the object until this method finishes!
    auto jniV8Object = reinterpret_cast<JNIV8Object*>(data.GetParameter());

    // "resurrect" the JS object, because we might need it later in some native or java function
    // IF we do, we have to make a strong reference to the java object again and also register this callback for
    // the provided JS object reference!
    jniV8Object->_jsObject.ClearWeak();

    // we are only holding the object because java/native is still alive, v8 can not gc it anymore
    // => adjust external memory counter
    jniV8Object->_bgjsEngine->getIsolate()->AdjustAmountOfExternalAllocatedMemory(-jniV8Object->_externalMemory);

    // finally: the js object is no longer being used => release the strong reference to the java object
    // NOTE: object might be deleted by another thread after calling this
    jniV8Object->releaseJObject();
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
        JNI_ASSERT(jsObject->GetInternalField(0)->IsUndefined(), "failed to link js object");
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

void JNIV8Object::setJSObject(BGJSV8Engine *engine, JNIV8ClassInfo *cls,
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

void JNIV8Object::initializeV8Bindings(JNIV8ClassInfo *info) {

}

void JNIV8Object::initializeJNIBindings(JNIClassInfo *info, bool isReload) {
    info->registerConstructor("(Lag/boersego/bgjs/V8Engine;J[Ljava/lang/Object;)V","<JNIV8ObjectInit>");
    info->registerConstructor("(Lag/boersego/bgjs/V8Engine;)V","<JNIV8ObjectInit#2>");

    info->registerNativeMethod("adjustJSExternalMemory", "(J)V", (void*)JNIV8Object::jniAdjustJSExternalMemory);
    info->registerNativeMethod("_applyV8Method", "(Ljava/lang/String;IILjava/lang/Class;[Ljava/lang/Object;)Ljava/lang/Object;", (void*)JNIV8Object::jniCallV8MethodWithReturnType);
    info->registerNativeMethod("_getV8Field", "(Ljava/lang/String;IILjava/lang/Class;)Ljava/lang/Object;", (void*)JNIV8Object::jniGetV8FieldWithReturnType);
    info->registerNativeMethod("setV8Field", "(Ljava/lang/String;Ljava/lang/Object;)V", (void*)JNIV8Object::jniSetV8Field);
    info->registerNativeMethod("setV8Fields", "(Ljava/util/Map;)V", (void*)JNIV8Object::jniSetV8Fields);

    info->registerNativeMethod("hasV8Field", "(Ljava/lang/String;Z)Z", (void*)JNIV8Object::jniHasV8Field);
    info->registerNativeMethod("getV8Keys", "(Z)[Ljava/lang/String;", (void*)JNIV8Object::jniGetV8Keys);
    info->registerNativeMethod("getV8Fields", "(ZIILjava/lang/Class;)Ljava/util/Map;", (void*)JNIV8Object::jniGetV8Fields);

    info->registerNativeMethod("toNumber", "()D", (void*)JNIV8Object::jniToNumber);
    info->registerNativeMethod("toString", "()Ljava/lang/String;", (void*)JNIV8Object::jniToString);
    info->registerNativeMethod("toJSON", "()Ljava/lang/String;", (void*)JNIV8Object::jniToJSON);

    info->registerNativeMethod("RegisterV8Class", "(Ljava/lang/String;Ljava/lang/String;)V", (void*)JNIV8Object::jniRegisterV8Class);
}

void JNIV8Object::jniAdjustJSExternalMemory(JNIEnv *env, jobject obj, jlong change) {
    auto ptr = JNIWrapper::wrapObject<JNIV8Object>(obj);
    ptr->adjustJSExternalMemory(change);
}

jobject JNIV8Object::jniGetV8FieldWithReturnType(JNIEnv *env, jobject obj, jstring name, jint flags, jint type, jclass returnType) {
    JNIV8Object_PrepareJNICall(JNIV8Object, Object, nullptr);

    JNIV8JavaValue arg = JNIV8Marshalling::valueWithClass(type, returnType, (JNIV8MarshallingFlags)flags);

    MaybeLocal<Value> valueRef = localRef->Get(context, JNIV8Marshalling::jstring2v8string(name));
    if(valueRef.IsEmpty()) {
        ptr->getEngine()->forwardV8ExceptionToJNI(&try_catch);
        return nullptr;
    }

    jvalue jval;
    memset(&jval, 0, sizeof(jvalue));
    JNIV8MarshallingError res = JNIV8Marshalling::convertV8ValueToJavaValue(env, valueRef.ToLocalChecked(), arg, &jval);
    if(res != JNIV8MarshallingError::kOk) {
        std::string strFieldName = JNIWrapper::jstring2string(name);
        switch(res) {
            default:
            case JNIV8MarshallingError::kWrongType:
                ThrowV8TypeError("wrong type for field '" + strFieldName + "'");
                break;
            case JNIV8MarshallingError::kUndefined:
                ThrowV8TypeError("field '" + strFieldName + "' must not be undefined");
                break;
            case JNIV8MarshallingError::kNotNullable:
                ThrowV8TypeError("field '" + strFieldName + "' is not nullable");
                break;
            case JNIV8MarshallingError::kNoNaN:
                ThrowV8TypeError("field '" + strFieldName + "' must not be NaN");
                break;
            case JNIV8MarshallingError::kVoidNotNull:
                ThrowV8TypeError("field '" + strFieldName + "' can only be null or undefined");
                break;
            case JNIV8MarshallingError::kOutOfRange:
                ThrowV8RangeError("assigned value '"+
                                  JNIV8Marshalling::v8string2string(valueRef.ToLocalChecked()->ToString())+"' is out of range for field '" + strFieldName + "'");
                break;
        }
        return nullptr;
    }

    return jval.l;
}

void JNIV8Object::jniSetV8Field(JNIEnv *env, jobject obj, jstring name, jobject value) {
    JNIV8Object_PrepareJNICall(JNIV8Object, Object, void());

    Maybe<bool> res = localRef->Set(context, JNIV8Marshalling::jstring2v8string(name), JNIV8Marshalling::jobject2v8value(value));
    if(res.IsNothing()) {
        ptr->getEngine()->forwardV8ExceptionToJNI(&try_catch);
    }
}

void JNIV8Object::jniSetV8Fields(JNIEnv *env, jobject obj, jobject map) {
    JNIV8Object_PrepareJNICall(JNIV8Object, Object, void());

    jobject set = env->CallObjectMethod(map, _jniMap.entrySetId);
    if (set == nullptr) {
        return;
    }

    jobject iter = env->CallObjectMethod(set, _jniSet.iteratorId);
    if (iter == nullptr) {
        return;
    }

    while (env->CallBooleanMethod(iter, _jniIterator.hasNextId)) {
        jobject entry = env->CallObjectMethod(iter, _jniIterator.nextId);
        jstring key = (jstring) env->CallObjectMethod(entry, _jniMapEntry.getKeyId);
        jobject value = env->CallObjectMethod(entry, _jniMapEntry.getValueId);

        Maybe<bool> res = localRef->Set(context, JNIV8Marshalling::jstring2v8string(key), JNIV8Marshalling::jobject2v8value(value));
        if(res.IsNothing()) {
            ptr->getEngine()->forwardV8ExceptionToJNI(&try_catch);
            break;
        }

        env->DeleteLocalRef(entry);
        env->DeleteLocalRef(key);
        env->DeleteLocalRef(value);
    }
}

jobject JNIV8Object::jniCallV8MethodWithReturnType(JNIEnv *env, jobject obj, jstring name, jint flags, jint type, jclass returnType, jobjectArray arguments) {
    JNIV8Object_PrepareJNICall(JNIV8Object, Object, nullptr);

    JNIV8JavaValue arg = JNIV8Marshalling::valueWithClass(type, returnType, (JNIV8MarshallingFlags)flags);

    MaybeLocal<Value> maybeLocal;
    Local<Value> funcRef;
    maybeLocal = localRef->Get(context, JNIV8Marshalling::jstring2v8string(name));
    if (!maybeLocal.ToLocal<Value>(&funcRef)) {
        ptr->getEngine()->forwardV8ExceptionToJNI(&try_catch);
        return nullptr;
    }
    if(!funcRef->IsFunction()) {
        ptr = nullptr; // release shared_ptr before throwing an exception!
        env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"),
                      "Called v8 field is not a function");
        return nullptr;
    }

    jsize numArgs;
    Local<Value> *args;
    jobject tempObj;

    numArgs = env->GetArrayLength(arguments);
    if (numArgs) {
        args = (Local<Value>*)malloc(sizeof(Local<Value>)*numArgs);
        for(jsize i=0; i<numArgs; i++) {
            tempObj = env->GetObjectArrayElement(arguments, i);
            args[i] = JNIV8Marshalling::jobject2v8value(tempObj);
            env->DeleteLocalRef(tempObj);
        }
    } else {
        args = nullptr;
    }

    Local<Value> resultRef;
    maybeLocal = Local<Object>::Cast(funcRef)->CallAsFunction(context, localRef, numArgs, args);
    if (!maybeLocal.ToLocal<Value>(&resultRef)) {
        ptr->getEngine()->forwardV8ExceptionToJNI(&try_catch);
        return nullptr;
    }

    if(args) {
        free(args);
    }

    jvalue jval;
    memset(&jval, 0, sizeof(jvalue));
    JNIV8MarshallingError res = JNIV8Marshalling::convertV8ValueToJavaValue(env, resultRef, arg, &jval);
    if(res != JNIV8MarshallingError::kOk) {
        std::string strMethodName = JNIWrapper::jstring2string(name);
        switch(res) {
            default:
            case JNIV8MarshallingError::kWrongType:
                ThrowV8TypeError("wrong type for return value of '" + strMethodName + "'");
                break;
            case JNIV8MarshallingError::kUndefined:
                ThrowV8TypeError("return value of '" + strMethodName + "' must not be undefined");
                break;
            case JNIV8MarshallingError::kNotNullable:
                ThrowV8TypeError("return value of '" + strMethodName + "' is not nullable");
                break;
            case JNIV8MarshallingError::kNoNaN:
                ThrowV8TypeError("return value of '" + strMethodName + "' must not be NaN");
                break;
            case JNIV8MarshallingError::kVoidNotNull:
                ThrowV8TypeError("return value of '" + strMethodName + "' can only be null or undefined");
                break;
            case JNIV8MarshallingError::kOutOfRange:
                ThrowV8RangeError("return value '"+
                                  JNIV8Marshalling::v8string2string(resultRef->ToString())+"' is out of range for method '" + strMethodName + "'");
                break;
        }
        return nullptr;
    }

    return jval.l;
}

jboolean JNIV8Object::jniHasV8Field(JNIEnv *env, jobject obj, jstring name, jboolean ownOnly) {
    JNIV8Object_PrepareJNICall(JNIV8Object, Object, false);

    Local<String> keyRef = JNIV8Marshalling::jstring2v8string(name);
    Maybe<bool> res = ownOnly ? localRef->HasOwnProperty(context, keyRef) : localRef->Has(context, keyRef);
    if(res.IsNothing()) {
        ptr->getEngine()->forwardV8ExceptionToJNI(&try_catch);
        return (jboolean)false;
    }
    return (jboolean)res.ToChecked();
}

jobjectArray JNIV8Object::jniGetV8Keys(JNIEnv *env, jobject obj, jboolean ownOnly) {
    JNIV8Object_PrepareJNICall(JNIV8Object, Object, nullptr);

    MaybeLocal<Array> maybeArrayRef = ownOnly ? localRef->GetOwnPropertyNames(context) : localRef->GetPropertyNames();
    if(maybeArrayRef.IsEmpty()) {
        ptr->getEngine()->forwardV8ExceptionToJNI(&try_catch);
        return nullptr;
    }

    Local<Array> arrayRef = maybeArrayRef.ToLocalChecked();
    Local<Value> valueRef;

    jobjectArray result = nullptr;
    jstring string;

    for(uint32_t i=0,n=arrayRef->Length(); i<n; i++) {
        MaybeLocal<Value> maybeValueRef = arrayRef->Get(context, i);
        if(!maybeValueRef.ToLocal<Value>(&valueRef)) {
            ptr->getEngine()->forwardV8ExceptionToJNI(&try_catch);
            return nullptr;
        }
        string = JNIV8Marshalling::v8string2jstring(Local<String>::Cast(valueRef));
        if(!result) {
            result = env->NewObjectArray(n, _jniString.clazz, string);
        } else {
            env->SetObjectArrayElement(result, i, string);
        }
        env->DeleteLocalRef(string);
    }

    return result;
}

jobject JNIV8Object::jniGetV8Fields(JNIEnv *env, jobject obj, jboolean ownOnly, jint flags, jint type, jclass returnType) {
    JNIV8Object_PrepareJNICall(JNIV8Object, Object, nullptr);

    JNIV8JavaValue arg = JNIV8Marshalling::valueWithClass(type, returnType, (JNIV8MarshallingFlags)flags);

    MaybeLocal<Array> maybeArrayRef = ownOnly ? localRef->GetOwnPropertyNames(context) : localRef->GetPropertyNames(context);
    if(maybeArrayRef.IsEmpty()) {
        ptr->getEngine()->forwardV8ExceptionToJNI(&try_catch);
        return nullptr;
    }

    Local<Array> arrayRef = maybeArrayRef.ToLocalChecked();
    Local<Value> valueRef;
    Local<String> keyRef;

    jobject strObj;
    // we are only using the .l member of the jvalue; so one memset is enough!
    jvalue jval = {0};
    memset(&jval, 0, sizeof(jvalue));

    jobject result = env->NewObject(_jniHashMap.clazz, _jniHashMap.initId);
    for(uint32_t i=0,n=arrayRef->Length(); i<n; i++) {
        MaybeLocal<Value> maybeValueRef = arrayRef->Get(context, i);
        if(!maybeValueRef.ToLocal<Value>(&valueRef)) {
            ptr->getEngine()->forwardV8ExceptionToJNI(&try_catch);
            return nullptr;
        }
        keyRef = valueRef->ToString();

        maybeValueRef = localRef->Get(context, keyRef);
        if(!maybeValueRef.ToLocal<Value>(&valueRef)) {
            ptr->getEngine()->forwardV8ExceptionToJNI(&try_catch);
            return nullptr;
        }

        JNIV8MarshallingError res = JNIV8Marshalling::convertV8ValueToJavaValue(env, valueRef, arg, &jval);
        if(res != JNIV8MarshallingError::kOk) {
            std::string strPropertyName = JNIV8Marshalling::v8string2string(keyRef);
            switch(res) {
                default:
                case JNIV8MarshallingError::kWrongType:
                    ThrowV8TypeError("wrong type for value of '" + strPropertyName + "'");
                    break;
                case JNIV8MarshallingError::kUndefined:
                    ThrowV8TypeError("value of '" + strPropertyName + "' must not be undefined");
                    break;
                case JNIV8MarshallingError::kNotNullable:
                    ThrowV8TypeError("value of '" + strPropertyName + "' is not nullable");
                    break;
                case JNIV8MarshallingError::kNoNaN:
                    ThrowV8TypeError("value of '" + strPropertyName + "' must not be NaN");
                    break;
                case JNIV8MarshallingError::kVoidNotNull:
                    ThrowV8TypeError("value of '" + strPropertyName + "' can only be null or undefined");
                    break;
                case JNIV8MarshallingError::kOutOfRange:
                    ThrowV8RangeError("value '"+
                                      JNIV8Marshalling::v8string2string(valueRef->ToString())+"' is out of range for property '" + strPropertyName + "'");
                    break;
            }
            return nullptr;
        }

        strObj = JNIV8Marshalling::v8string2jstring(keyRef);
        env->CallObjectMethod(result,
                              _jniHashMap.putId,
                              strObj,
                              jval.l
        );
        env->DeleteLocalRef(jval.l);
        env->DeleteLocalRef(strObj);
    }

    return result;
}

jdouble JNIV8Object::jniToNumber(JNIEnv *env, jobject obj) {
    JNIV8Object_PrepareJNICall(JNIV8Object, Object, 0);
    v8::Maybe<double> numberValue = localRef->NumberValue(context);
    if(numberValue.IsNothing()) {
        engine->forwardV8ExceptionToJNI(&try_catch);
        return 0;
    }
    return numberValue.ToChecked();
}

jstring JNIV8Object::jniToJSON(JNIEnv *env, jobject obj) {
    JNIV8Object_PrepareJNICall(JNIV8Object, Object, nullptr);
    v8::Local<v8::Value> stringValue = engine->stringifyJSON(localRef);
    if(stringValue.IsEmpty()) {
        engine->forwardV8ExceptionToJNI(&try_catch);
        return nullptr;
    }
    return JNIV8Marshalling::v8string2jstring(stringValue.As<v8::String>());
}

jstring JNIV8Object::jniToString(JNIEnv *env, jobject obj) {
    JNIV8Object_PrepareJNICall(JNIV8Object, Object, nullptr);
    MaybeLocal<String> maybeLocal = localRef->ToString(context);
    if(maybeLocal.IsEmpty()) {
        engine->forwardV8ExceptionToJNI(&try_catch);
        return nullptr;
    }
    return JNIV8Marshalling::v8string2jstring(maybeLocal.ToLocalChecked());
}

void JNIV8Object::jniRegisterV8Class(JNIEnv *env, jobject obj, jstring derivedClass, jstring baseClass) {
    std::string strDerivedClass = JNIWrapper::jstring2string(derivedClass);
    std::replace(strDerivedClass.begin(), strDerivedClass.end(), '.', '/');

    std::string strBaseClass = JNIWrapper::jstring2string(baseClass);
    std::replace(strBaseClass.begin(), strBaseClass.end(), '.', '/');

    JNIV8Wrapper::registerJavaObject(strDerivedClass, strBaseClass);
}

//--------------------------------------------------------------------------------------------------
// Exports
//--------------------------------------------------------------------------------------------------
extern "C" {

JNIEXPORT void JNICALL
Java_ag_boersego_bgjs_JNIV8Object_initNativeJNIV8Object(JNIEnv *env, jobject obj, jstring canonicalName, jobject engine,
                                                        jlong jsObjPtr) {
    JNIWrapper::initializeNativeObject(obj, canonicalName);
    JNIV8Wrapper::initializeNativeJNIV8Object(obj, engine, jsObjPtr);
}

}
