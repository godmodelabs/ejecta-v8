//
// Created by Martin Kleinhans on 26.09.17.
//

#include "JNIV8Array.h"
#include "JNIV8Wrapper.h"
#include "../bgjs/BGJSV8Engine.h"

BGJS_JNI_LINK(JNIV8Array, "ag/boersego/bgjs/JNIV8Array");

decltype(JNIV8Array::_jniObject) JNIV8Array::_jniObject = {0};

/**
 * cache JNI class references
 */
void JNIV8Array::initJNICache() {
    JNIEnv *env = JNIWrapper::getEnvironment();
    _jniObject.clazz = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Object"));
}

bool JNIV8Array::isWrappableV8Object(v8::Local<v8::Object> object) {
    return object->IsArray();
}

void JNIV8Array::initializeJNIBindings(JNIClassInfo *info, bool isReload) {
    info->registerNativeMethod("Create", "(Lag/boersego/bgjs/V8Engine;)Lag/boersego/bgjs/JNIV8Array;", (void*)JNIV8Array::jniCreate);
    info->registerNativeMethod("CreateWithLength", "(Lag/boersego/bgjs/V8Engine;I)Lag/boersego/bgjs/JNIV8Array;", (void*)JNIV8Array::jniCreateWithLength);
    info->registerNativeMethod("CreateWithArray", "(Lag/boersego/bgjs/V8Engine;[Ljava/lang/Object;)Lag/boersego/bgjs/JNIV8Array;", (void*)JNIV8Array::jniCreateWithArray);
    info->registerNativeMethod("getV8Length", "()I", (void*)JNIV8Array::jniGetV8Length);
    info->registerNativeMethod("_getV8Elements", "(IILjava/lang/Class;II)[Ljava/lang/Object;", (void*)JNIV8Array::jniGetV8ElementsInRange);
    info->registerNativeMethod("_getV8Element", "(IILjava/lang/Class;I)Ljava/lang/Object;", (void*)JNIV8Array::jniGetV8Element);
}

/**
 * returns the length of the array
 */
jint JNIV8Array::jniGetV8Length(JNIEnv *env, jobject obj) {
    JNIV8Object_PrepareJNICall(JNIV8Array, v8::Array, 0);
    return localRef->Length();
}

/**
 * Returns all objects from a specified range inside of the array
 */
jobjectArray JNIV8Array::jniGetV8ElementsInRange(JNIEnv *env, jobject obj, jint flags, jint type, jclass returnType, jint from, jint to) {
    JNIV8Object_PrepareJNICall(JNIV8Array, v8::Array, nullptr);

    JNIV8JavaValue arg = JNIV8Marshalling::valueWithClass(type, returnType, (JNIV8MarshallingFlags)flags);

    uint32_t len = localRef->Length();
    uint32_t size;

    // clamp range
    if(to>=len) to = len - 1;
    if(from<0) from = 0;

    // now determine size of slice
    if(from < len && from <= to) {
        size = (uint32_t)((to-from)+1);
    } else {
        size = 0;
    }

    jobjectArray elements = env->NewObjectArray(size, _jniObject.clazz, nullptr);
    if(!size) return elements;

    // we are only using the .l member here and overwriting it every time => one memset is enough
    jvalue jval = {0};
    memset(&jval, 0, sizeof(jvalue));

    for(uint32_t i=(uint32_t)from; i<=to; i++) {
        v8::MaybeLocal<v8::Value> maybeValue = localRef->Get(context, i);
        v8::Local<v8::Value> value;
        if(maybeValue.IsEmpty()) {
            value = v8::Undefined(isolate);
        } else {
            value = maybeValue.ToLocalChecked();
        }

        JNIV8MarshallingError res = JNIV8Marshalling::convertV8ValueToJavaValue(env, value, arg, &jval);
        if(res != JNIV8MarshallingError::kOk) {
            switch(res) {
                default:
                case JNIV8MarshallingError::kWrongType:
                    ThrowV8TypeError("wrong type for value of element #" + std::to_string(i));
                    break;
                case JNIV8MarshallingError::kUndefined:
                    ThrowV8TypeError("value of element #" + std::to_string(i) + " must not be undefined");
                    break;
                case JNIV8MarshallingError::kNotNullable:
                    ThrowV8TypeError("value of element #" + std::to_string(i) + " is not nullable");
                    break;
                case JNIV8MarshallingError::kNoNaN:
                    ThrowV8TypeError("value of element #" + std::to_string(i) + " must not be NaN");
                    break;
                case JNIV8MarshallingError::kVoidNotNull:
                    ThrowV8TypeError("value of element #" + std::to_string(i) + " can only be null or undefined");
                    break;
                case JNIV8MarshallingError::kOutOfRange:
                    ThrowV8RangeError("value '"+
                                      JNIV8Marshalling::v8string2string(value->ToString())+"' is out of range for element element #" + std::to_string(i));
                    break;
            }
            return nullptr;
        }

        env->SetObjectArrayElement(elements, i-from, jval.l);
        env->DeleteLocalRef(jval.l);
    }

    return elements;
}

/**
 * Returns the object at the specified index
 * if index is out of bounds, returns JNIV8Undefined
 */
jobject JNIV8Array::jniGetV8Element(JNIEnv *env, jobject obj, jint flags, jint type, jclass returnType, jint index) {
    JNIV8Object_PrepareJNICall(JNIV8Array, v8::Array, JNIV8Marshalling::undefinedInJava());

    JNIV8JavaValue arg = JNIV8Marshalling::valueWithClass(type, returnType, (JNIV8MarshallingFlags)flags);

    v8::MaybeLocal<v8::Value> maybeValue;

    maybeValue = localRef->Get(context, (uint32_t)index);
    if(maybeValue.IsEmpty()) return JNIV8Marshalling::undefinedInJava();

    v8::Local<v8::Value> value = maybeValue.ToLocalChecked();

    jvalue jval = {0};
    memset(&jval, 0, sizeof(jvalue));
    JNIV8MarshallingError res = JNIV8Marshalling::convertV8ValueToJavaValue(env, value, arg, &jval);
    if(res != JNIV8MarshallingError::kOk) {
        switch(res) {
            default:
            case JNIV8MarshallingError::kWrongType:
                ThrowV8TypeError("wrong type for value of element #" + std::to_string(index));
                break;
            case JNIV8MarshallingError::kUndefined:
                ThrowV8TypeError("value of element #" + std::to_string(index) + " must not be undefined");
                break;
            case JNIV8MarshallingError::kNotNullable:
                ThrowV8TypeError("value of element #" + std::to_string(index) + " is not nullable");
                break;
            case JNIV8MarshallingError::kNoNaN:
                ThrowV8TypeError("value of element #" + std::to_string(index) + " must not be NaN");
                break;
            case JNIV8MarshallingError::kVoidNotNull:
                ThrowV8TypeError("value of element #" + std::to_string(index) + " can only be null or undefined");
                break;
            case JNIV8MarshallingError::kOutOfRange:
                ThrowV8RangeError("value '"+
                                  JNIV8Marshalling::v8string2string(value->ToString())+"' is out of range for element element #" + std::to_string(index));
                break;
        }
        return nullptr;
    }

    return jval.l;
}

jobject JNIV8Array::jniCreate(JNIEnv *env, jobject obj, jobject engineObj) {
    return jniCreateWithLength(env, obj, engineObj, 0);
}

jobject JNIV8Array::jniCreateWithLength(JNIEnv *env, jobject obj, jobject engineObj, jint length) {
    auto engine = JNIWrapper::wrapObject<BGJSV8Engine>(engineObj);

    v8::Isolate* isolate = engine->getIsolate();
    v8::Locker l(isolate);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope scope(isolate);
    v8::Context::Scope ctxScope(engine->getContext());

    v8::Local<v8::Object> objRef = v8::Array::New(isolate, length);

    return JNIV8Wrapper::wrapObject<JNIV8Array>(objRef)->getJObject();
}

jobject JNIV8Array::jniCreateWithArray(JNIEnv *env, jobject obj, jobject engineObj, jobjectArray elements) {
    auto engine = JNIWrapper::wrapObject<BGJSV8Engine>(engineObj);

    v8::Isolate* isolate = engine->getIsolate();
    v8::Locker l(isolate);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope scope(isolate);
    v8::Context::Scope ctxScope(engine->getContext());

    jsize numArgs = env->GetArrayLength(elements);
    v8::Local<v8::Object> objRef = v8::Array::New(isolate, numArgs);
    if (numArgs) {
        jobject obj;
        for(jsize i=0; i<numArgs; i++) {
            obj = env->GetObjectArrayElement(elements, i);
            objRef->Set(i, JNIV8Marshalling::jobject2v8value(obj));
            env->DeleteLocalRef(obj);
        }
    }

    return JNIV8Wrapper::wrapObject<JNIV8Array>(objRef)->getJObject();
}
