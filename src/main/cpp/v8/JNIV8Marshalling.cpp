//
// Created by Martin Kleinhans on 30.11.17.
//

#include "JNIV8Marshalling.h"
#include <cmath>

#include "../jni/JNIWrapper.h"
#include "JNIV8Wrapper.h"

#include "JNIV8Function.h"
#include "JNIV8Promise.h"
#include "JNIV8Array.h"
#include "JNIV8ArrayBuffer.h"
#include "JNIV8Symbol.h"
#include "JNIV8GenericObject.h"

JNIV8JavaValueType getArgumentType(const std::string& type) {
    if(type == "Z" || type == "Ljava/lang/Boolean;") {
        return JNIV8JavaValueType::kBoolean;
    } else if(type == "B" || type == "Ljava/lang/Byte;") {
        return JNIV8JavaValueType::kByte;
    } else if(type == "C" || type == "Ljava/lang/Character;") {
        return JNIV8JavaValueType::kCharacter;
    } else if(type == "S" || type == "Ljava/lang/Short;") {
        return JNIV8JavaValueType::kShort;
    } else if(type == "I" || type == "Ljava/lang/Integer;") {
        return JNIV8JavaValueType::kInteger;
    } else if(type == "J" || type == "Ljava/lang/Long;") {
        return JNIV8JavaValueType::kLong;
    } else if(type == "F" || type == "Ljava/lang/Float;") {
        return JNIV8JavaValueType::kFloat;
    } else if(type == "D" || type == "Ljava/lang/Double;") {
        return JNIV8JavaValueType::kDouble;
    } else if(type == "V" || type == "Ljava/lang/Void;") {
        return JNIV8JavaValueType::kVoid;
    } else if(type == "Ljava/lang/String;") {
        return JNIV8JavaValueType::kString;
    }
    return JNIV8JavaValueType::kObject;
}

JNIV8JavaValue JNIV8Marshalling::valueWithType(JNIV8JavaValueType type, bool boxed, JNIV8MarshallingFlags flags) {
    switch(type) {
        case JNIV8JavaValueType::kBoolean: {
            return JNIV8JavaValue(type, boxed ? _jniBoolean.clazz : nullptr, flags);
        }
        case JNIV8JavaValueType::kByte: {
            return JNIV8JavaValue(type, boxed ? _jniByte.clazz : nullptr, flags);
        }
        case JNIV8JavaValueType::kCharacter: {
            return JNIV8JavaValue(type, boxed ? _jniCharacter.clazz : nullptr, flags);
        }
        case JNIV8JavaValueType::kShort: {
            return JNIV8JavaValue(type, boxed ? _jniShort.clazz : nullptr, flags);
        }
        case JNIV8JavaValueType::kInteger: {
            return JNIV8JavaValue(type, boxed ? _jniInteger.clazz : nullptr, flags);
        }
        case JNIV8JavaValueType::kLong: {
            return JNIV8JavaValue(type, boxed ? _jniLong.clazz : nullptr, flags);
        }
        case JNIV8JavaValueType::kFloat: {
            return JNIV8JavaValue(type, boxed ? _jniFloat.clazz : nullptr, flags);
        }
        case JNIV8JavaValueType::kDouble: {
            return JNIV8JavaValue(type, boxed ? _jniDouble.clazz : nullptr, flags);
        }
        case JNIV8JavaValueType::kVoid: {
            return JNIV8JavaValue(type, nullptr, flags);
        }
        case JNIV8JavaValueType::kString: {
            return JNIV8JavaValue(type, _jniString.clazz, flags);
        }
        case JNIV8JavaValueType::kObject: {
            return JNIV8JavaValue(type, _jniObject.clazz, flags);
        }
    }
}

JNIV8JavaValue JNIV8Marshalling::valueWithClass(jint type, jclass clazz, JNIV8MarshallingFlags flags) {
    auto it = _typeMap.find(type);
    if(it == _typeMap.end()) {
        return JNIV8JavaValue(JNIV8JavaValueType::kObject, clazz, flags);
    }
    return JNIV8Marshalling::valueWithType((*it).second, clazz ? true : false, flags);
}

JNIV8JavaValue JNIV8Marshalling::persistentValueWithTypeSignature(const std::string &type, JNIV8MarshallingFlags flags) {
    JNIV8JavaValueType valueType = getArgumentType(type);
    jclass clazz;
    if (type.length() > 1) {
        JNIEnv *env = JNIWrapper::getEnvironment();

        if(valueType != JNIV8JavaValueType::kObject || type == "Ljava/lang/Object;") {
            return JNIV8Marshalling::valueWithType(valueType, true, flags);
        }

        clazz = (jclass) env->NewGlobalRef(
                env->FindClass(type.substr(1, type.length() - 2).c_str()));
    } else {
        clazz = nullptr;
    }

    // primitives are never nullable
    if(type.length() == 1) {
        flags = (JNIV8MarshallingFlags)(flags|JNIV8MarshallingFlags::kNonNull);
    }

#ifdef ENABLE_JNI_ASSERT
    static jclass _jniObjectClazz = nullptr, _jniJNIV8ObjectClazz = nullptr;
    JNIEnv *env = JNIWrapper::getEnvironment();
    if(!_jniObjectClazz) {
        _jniObjectClazz = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Object"));
        _jniJNIV8ObjectClazz = (jclass)env->NewGlobalRef(env->FindClass("ag/boersego/bgjs/JNIV8Object"));
    }
    JNI_ASSERT(type[0] != '[', "array types are not supported");
    JNI_ASSERTF((type.length()>1 && clazz) || std::string("ZBCSIJFDV").find(type) != std::string::npos, "invalid argument type '%s'", type.c_str());
    JNI_ASSERTF(valueType != JNIV8JavaValueType::kObject || (env->IsSameObject(clazz, _jniObjectClazz) || env->IsAssignableFrom(clazz, _jniJNIV8ObjectClazz)), "invalid argument type '%s'", type.c_str())
    JNI_ASSERT((flags & JNIV8MarshallingFlags::kNonNull) || type.length() > 1, "primitive types must not be nullable")
    JNI_ASSERT(!(flags & JNIV8MarshallingFlags::kUndefinedIsNull) || type.length() > 1, "primitive types can not treat undefined as null")
    JNI_ASSERT(valueType != JNIV8JavaValueType::kVoid || (!clazz || !(flags & JNIV8MarshallingFlags::kNonNull)), "Void property must nullable");
#endif

    return JNIV8JavaValue(valueType, clazz, flags);
}

JNIV8JavaValue JNIV8Marshalling::persistentArgumentWithTypeSignature(const std::string &type, JNIV8MarshallingFlags flags) {
    JNI_ASSERT(type[0] != 'V', "'void' is not a valid argument type");
    return persistentValueWithTypeSignature(type, flags);
}

JNIV8JavaValue::JNIV8JavaValue(JNIV8JavaValueType type, jclass clazz, JNIV8MarshallingFlags flags) :
            valueType(type), clazz(clazz), flags(flags) {
}

#define InitBoxedType(type, mnemonic)\
_jni##type.clazz = (jclass)env->NewGlobalRef(env->FindClass("java/lang/"#type));\
_jni##type.valueOfId = env->GetStaticMethodID(_jni##type.clazz, "valueOf","("#mnemonic")Ljava/lang/"#type";");\
_typeMap[env->CallIntMethod(_jni##type.clazz, hashCodeId)] = JNIV8JavaValueType::k##type;

#define AutoboxArgument(cls, key)\
if(arg.clazz) {\
target->l = env->CallStaticObjectMethod(_jni##cls.clazz, _jni##cls.valueOfId, value);\
} else {\
target->key = value;\
}

decltype(JNIV8Marshalling::_jniBoolean) JNIV8Marshalling::_jniBoolean = {0};
decltype(JNIV8Marshalling::_jniByte) JNIV8Marshalling::_jniByte = {0};
decltype(JNIV8Marshalling::_jniCharacter) JNIV8Marshalling::_jniCharacter = {0};
decltype(JNIV8Marshalling::_jniShort) JNIV8Marshalling::_jniShort = {0};
decltype(JNIV8Marshalling::_jniInteger) JNIV8Marshalling::_jniInteger = {0};
decltype(JNIV8Marshalling::_jniLong) JNIV8Marshalling::_jniLong = {0};
decltype(JNIV8Marshalling::_jniFloat) JNIV8Marshalling::_jniFloat = {0};
decltype(JNIV8Marshalling::_jniDouble) JNIV8Marshalling::_jniDouble = {0};
decltype(JNIV8Marshalling::_jniNumber) JNIV8Marshalling::_jniNumber = {0};
decltype(JNIV8Marshalling::_jniV8Object) JNIV8Marshalling::_jniV8Object = {0};
decltype(JNIV8Marshalling::_jniObject) JNIV8Marshalling::_jniObject = {0};
decltype(JNIV8Marshalling::_jniString) JNIV8Marshalling::_jniString = {0};
decltype(JNIV8Marshalling::_jniVoid) JNIV8Marshalling::_jniVoid = {0};
jobject JNIV8Marshalling::_undefined = nullptr;
std::unordered_map<int, JNIV8JavaValueType> JNIV8Marshalling::_typeMap;

/**
 * register an alias for a primitive type
 */
void JNIV8Marshalling::registerAliasForPrimitive(jint aliasType, jint primitiveType) {
    _typeMap[aliasType] = _typeMap[primitiveType];
}

/**
 * cache JNI class references
 */
void JNIV8Marshalling::initJNICache() {
    JNIEnv *env = JNIWrapper::getEnvironment();

    _jniObject.clazz = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Object"));
    jmethodID hashCodeId = env->GetMethodID(_jniObject.clazz, "hashCode", "()I");

    // all classes that support auto-boxing; we need the jclass and the valueOf method for boxing
    InitBoxedType(Boolean, Z);
    InitBoxedType(Byte, B);
    InitBoxedType(Character, C);
    InitBoxedType(Short, S);
    InitBoxedType(Integer, I);
    InitBoxedType(Long, J);
    InitBoxedType(Float, F);
    InitBoxedType(Double, D);

    // additional methods & classes required for some conversion routines
    jclass clsUndefined = env->FindClass("ag/boersego/bgjs/JNIV8Undefined");
    jmethodID getInstanceId = env->GetStaticMethodID(clsUndefined, "GetInstance", "()Lag/boersego/bgjs/JNIV8Undefined;");
    _undefined = env->NewGlobalRef(env->CallStaticObjectMethod(clsUndefined, getInstanceId));

    _jniBoolean.booleanValueId = env->GetMethodID(_jniBoolean.clazz, "booleanValue","()Z");
    _jniCharacter.charValueId = env->GetMethodID(_jniCharacter.clazz, "charValue","()C");
    _jniNumber.clazz = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Number"));
    _jniNumber.doubleValueId = env->GetMethodID(_jniNumber.clazz, "doubleValue","()D");
    _jniV8Object.clazz = (jclass)env->NewGlobalRef(env->FindClass("ag/boersego/bgjs/JNIV8Object"));
    _jniString.clazz = (jclass)env->NewGlobalRef(env->FindClass("java/lang/String"));
    _typeMap[env->CallIntMethod(_jniString.clazz, hashCodeId)] = JNIV8JavaValueType::kString;
    _jniVoid.clazz = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Void"));
    _typeMap[env->CallIntMethod(_jniVoid.clazz, hashCodeId)] = JNIV8JavaValueType::kVoid;
}

/**
 * converts a v8 value to a java value based on the provided type information
 * if conversion was successful method will return kOk and target will contain a valid jvalue
 */
JNIV8MarshallingError JNIV8Marshalling::convertV8ValueToJavaValue(JNIEnv *env, v8::Local<v8::Value> v8Value, JNIV8JavaValue arg, jvalue *target) {
    double numberValue;
    // non-primitive types can handle null; and undefined if it is configured to be mapped to null!
    if(!(arg.flags & JNIV8MarshallingFlags::kCoerceNull) && arg.clazz &&
            (v8Value->IsNull() || (v8Value->IsUndefined() && (arg.flags & JNIV8MarshallingFlags::kUndefinedIsNull)))) {
        target->l = nullptr;
        if ((arg.flags & JNIV8MarshallingFlags::kNonNull)) {
            return JNIV8MarshallingError::kNotNullable;
        }
    } else {
        v8::Isolate* isolate = v8::Isolate::GetCurrent();
        switch (arg.valueType) {
            case JNIV8JavaValueType::kObject:
                target->l = JNIV8Marshalling::v8value2jobject(v8Value);
                if (!target->l) {
                    // check nullability
                    if ((arg.flags & JNIV8MarshallingFlags::kNonNull)) {
                        return JNIV8MarshallingError::kNotNullable;
                    }
                } else if (arg.clazz && !env->IsInstanceOf(target->l, arg.clazz)) {
                    // validate objects type
                    if (v8Value->IsUndefined()) {
                        return JNIV8MarshallingError::kUndefined;
                    }
                    return JNIV8MarshallingError::kWrongType;
                }
                break;
            case JNIV8JavaValueType::kBoolean: {
                if((arg.flags & JNIV8MarshallingFlags::kStrict) && !v8Value->IsBoolean())
                    return JNIV8MarshallingError::kWrongType;
                jboolean value = (jboolean) v8Value->ToBoolean(isolate)->BooleanValue(isolate);
                AutoboxArgument(Boolean, z);
                break;
            }
            case JNIV8JavaValueType::kByte: {
                if((arg.flags & JNIV8MarshallingFlags::kStrict) && !v8Value->IsNumber())
                    return JNIV8MarshallingError::kWrongType;
                numberValue = v8Value->NumberValue(isolate->GetCurrentContext()).FromMaybe(FP_NAN);
                if (std::isnan(numberValue)) return JNIV8MarshallingError::kNoNaN;
                jbyte value = (jbyte) numberValue;
                if (value != numberValue) return JNIV8MarshallingError::kOutOfRange;
                AutoboxArgument(Byte, b);
                break;
            }
            case JNIV8JavaValueType::kCharacter: {
                if((arg.flags & JNIV8MarshallingFlags::kStrict) && !v8Value->IsString())
                    return JNIV8MarshallingError::kWrongType;
                jchar value = (jchar) JNIV8Marshalling::v8string2string(v8Value->ToString(isolate))[0];
                AutoboxArgument(Character, c);
                break;
            }
            case JNIV8JavaValueType::kShort: {
                if((arg.flags & JNIV8MarshallingFlags::kStrict) && !v8Value->IsNumber())
                    return JNIV8MarshallingError::kWrongType;
                numberValue = v8Value->NumberValue(isolate->GetCurrentContext()).FromMaybe(FP_NAN);
                if (std::isnan(numberValue)) return JNIV8MarshallingError::kNoNaN;
                jshort value = (jshort) numberValue;
                if (value != numberValue) return JNIV8MarshallingError::kOutOfRange;
                AutoboxArgument(Short, s);
                break;
            }
            case JNIV8JavaValueType::kInteger: {
                if((arg.flags & JNIV8MarshallingFlags::kStrict) && !v8Value->IsNumber())
                    return JNIV8MarshallingError::kWrongType;
                numberValue = v8Value->NumberValue(isolate->GetCurrentContext()).FromMaybe(FP_NAN);
                if (std::isnan(numberValue)) return JNIV8MarshallingError::kNoNaN;
                jint value = (jint) numberValue;
                if (value != numberValue) return JNIV8MarshallingError::kOutOfRange;
                AutoboxArgument(Integer, i);
                break;
            }
            case JNIV8JavaValueType::kLong: {
                if((arg.flags & JNIV8MarshallingFlags::kStrict) && !v8Value->IsNumber())
                    return JNIV8MarshallingError::kWrongType;
                numberValue = v8Value->NumberValue(isolate->GetCurrentContext()).FromMaybe(FP_NAN);
                if (std::isnan(numberValue)) return JNIV8MarshallingError::kNoNaN;
                jlong value = (jlong) numberValue;
                if (value != numberValue) return JNIV8MarshallingError::kOutOfRange;
                AutoboxArgument(Long, j);
                break;
            }
            case JNIV8JavaValueType::kFloat: {
                if((arg.flags & JNIV8MarshallingFlags::kStrict) && !v8Value->IsNumber())
                    return JNIV8MarshallingError::kWrongType;
                numberValue = v8Value->NumberValue(isolate->GetCurrentContext()).FromMaybe(FP_NAN);
                jfloat value = (jfloat) numberValue;
                // Comparing a double and float can lead to unexpected false negatives. We accept that some precision
                // can be lost here and do not do the kOutOfRange check.
                AutoboxArgument(Float, f);
                break;
            }
            case JNIV8JavaValueType::kDouble: {
                if((arg.flags & JNIV8MarshallingFlags::kStrict) && !v8Value->IsNumber())
                    return JNIV8MarshallingError::kWrongType;
                numberValue = v8Value->NumberValue(isolate->GetCurrentContext()).FromMaybe(FP_NAN);
                jdouble value = (jdouble) numberValue;
                AutoboxArgument(Double, d);
                break;
            }
            case JNIV8JavaValueType::kString: {
                if((arg.flags & JNIV8MarshallingFlags::kStrict) && !v8Value->IsString())
                    return JNIV8MarshallingError::kWrongType;
                target->l = JNIV8Marshalling::v8value2jobject(v8Value->ToString(isolate));
                break;
            }
            case JNIV8JavaValueType::kVoid: {
                if ((arg.flags & JNIV8MarshallingFlags::kStrict) && !v8Value->IsNull() && !v8Value->IsUndefined()) {
                    return JNIV8MarshallingError::kVoidNotNull;
                }
                target->l = nullptr;
            }
        }
    }

    return JNIV8MarshallingError::kOk;
}

/**
 * calls a java method with the provided arguments
 * if object is null, the method is assumed to be static
 */
v8::Local<v8::Value> JNIV8Marshalling::callJavaMethod(JNIEnv *env, JNIV8JavaValue returnType, jclass clazz, jmethodID methodId, jobject object, jvalue *args) {
    JNILocalFrame localFrame(env, 1);
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::EscapableHandleScope handleScope(isolate);
    v8::Local<v8::Value> result;

    // Do not expect boxed return types as primitives
    JNIV8JavaValueType valueType = returnType.valueType;
    if (returnType.clazz) {
        valueType = JNIV8JavaValueType::kObject;
    }

    switch(valueType) {
        case JNIV8JavaValueType::kString:
        case JNIV8JavaValueType::kObject: {
            jobject jresult;
            if (object) {
                jresult = env->CallObjectMethodA(object, methodId, args);
            } else {
                jresult = env->CallStaticObjectMethodA(clazz, methodId, args);
            }
            if(!env->ExceptionCheck()) {
                result = JNIV8Marshalling::jobject2v8value(jresult);
            } else {
                result = v8::Undefined(isolate);
            }
            break;
        }
        case JNIV8JavaValueType::kBoolean: {
            result = v8::Boolean::New(isolate,
                                      object ? env->CallBooleanMethodA(object, methodId, args) :
                                      env->CallStaticBooleanMethodA(clazz, methodId, args));
            break;
        }
        case JNIV8JavaValueType::kByte: {
            result = v8::Number::New(isolate,
                                     object ? env->CallByteMethodA(object, methodId, args) :
                                     env->CallStaticByteMethodA(clazz, methodId, args));
            break;
        }
        case JNIV8JavaValueType::kCharacter: {
            v8::MaybeLocal<v8::String> maybeLocal;
            jchar value = object ? env->CallCharMethodA(object, methodId, args) :
                          env->CallStaticCharMethodA(clazz, methodId, args);
            maybeLocal = v8::String::NewFromTwoByte(isolate, &value, v8::NewStringType::kNormal, 1);
            if(maybeLocal.IsEmpty()) {
                result = v8::Undefined(isolate);
            } else {
                result = maybeLocal.ToLocalChecked();
            }
            break;
        }
        case JNIV8JavaValueType::kShort: {
            result = v8::Number::New(isolate,
                                     object ? env->CallShortMethodA(object, methodId, args) :
                                     env->CallStaticShortMethodA(clazz,
                                                                 methodId, args));
            break;
        }
        case JNIV8JavaValueType::kInteger: {
            result = v8::Number::New(isolate,
                                     object ? env->CallIntMethodA(object, methodId, args) :
                                     env->CallStaticIntMethodA(clazz, methodId, args));
            break;
        }
        case JNIV8JavaValueType::kLong: {
            result = v8::Number::New(isolate,
                                     object ? env->CallLongMethodA(object, methodId, args) :
                                     env->CallStaticLongMethodA(clazz, methodId, args));
            break;
        }
        case JNIV8JavaValueType::kFloat: {
            result = v8::Number::New(isolate,
                                     object ? env->CallFloatMethodA(object, methodId, args) :
                                     env->CallStaticFloatMethodA(clazz,
                                                                 methodId, args));
            break;
        }
        case JNIV8JavaValueType::kDouble: {
            result = v8::Number::New(isolate,
                                     object ? env->CallDoubleMethodA(object, methodId, args) :
                                     env->CallStaticDoubleMethodA(clazz,
                                                                  methodId, args));
            break;
        }
        case JNIV8JavaValueType::kVoid: {
            if (object) {
                env->CallVoidMethodA(object, methodId, args);
            } else {
                env->CallStaticVoidMethodA(clazz, methodId, args);
            }
            result = v8::Undefined(isolate);
            break;
        }
    }

    return handleScope.Escape(result);
}


/**
 * convert a jstring to a std::string
 */
v8::Local<v8::String> JNIV8Marshalling::jstring2v8string(jstring string) {
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    // because this method returns a local, we can assume that the correct v8 scopes are active around it already
    // we still need a handle scope however...
    v8::EscapableHandleScope scope(isolate);
    JNIEnv *env = JNIWrapper::getEnvironment();

    v8::MaybeLocal<v8::String> maybeLocal;
    jsize len;

    // string pointers can also be null
    if(env->IsSameObject(string, nullptr)) {
        return scope.Escape(v8::Null(isolate)->ToString(isolate));
    }

    len = env->GetStringLength(string);

    if(len > 0) {
        const jchar *chars = env->GetStringChars(string, nullptr);
        maybeLocal = v8::String::NewFromTwoByte(isolate, chars, v8::NewStringType::kNormal, len);
        env->ReleaseStringChars(string, chars);
    }

    // if string is empty or if conversion failed we return an empty string
    if(!len || maybeLocal.IsEmpty()) {
        maybeLocal = v8::String::NewFromOneByte(isolate, (uint8_t*)"", v8::NewStringType::kInternalized);
        if(maybeLocal.IsEmpty()) {
            return scope.Escape(v8::Undefined(isolate).As<v8::String>());
        }
    }

    return scope.Escape(maybeLocal.ToLocalChecked());
}

/**
 * convert a v8 string to a jstring
 */
jstring JNIV8Marshalling::v8string2jstring(v8::Local<v8::String> string) {
    JNIEnv *env = JNIWrapper::getEnvironment();
    return env->NewString(*v8::String::Value(v8::Isolate::GetCurrent(), string), string->Length()); // returns "" when called with NULL,0
}

/**
 * convert an instance of V8Value to a jobject
 */
jobject JNIV8Marshalling::v8value2jobject(v8::Local<v8::Value> valueRef) {
    JNIEnv *env = JNIWrapper::getEnvironment();
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope scope(isolate);

    if(valueRef->IsNull()) {
        return nullptr;
    } else if(valueRef->IsObject()) {
        v8::Local<v8::Object> objectRef = valueRef.As<v8::Object>();

        // if the object is a proxy we wrap it based on the type of the target
        if (valueRef->IsProxy()) {
            valueRef = objectRef.As<v8::Proxy>()->GetTarget();
        }

        if (valueRef->IsFunction()) {
            return JNIV8Wrapper::wrapObject<JNIV8Function>(objectRef)->getJObject();
        } else if (valueRef->IsArray()) {
            return JNIV8Wrapper::wrapObject<JNIV8Array>(objectRef)->getJObject();
        } else if(valueRef->IsPromise()) {
            return JNIV8Wrapper::wrapObject<JNIV8Promise>(objectRef)->getJObject();
        } else if(valueRef->IsArrayBuffer()) {
            return JNIV8Wrapper::wrapObject<JNIV8ArrayBuffer>(objectRef)->getJObject();
        }
        auto ptr = JNIV8Wrapper::wrapObject<JNIV8Object>(objectRef);
        if (ptr) {
            return ptr->getJObject();
        } else {
            return JNIV8Wrapper::wrapObject<JNIV8GenericObject>(objectRef)->getJObject();
        }
    } else if(valueRef->IsNumber()) {
        return env->CallStaticObjectMethod(_jniDouble.clazz, _jniDouble.valueOfId, valueRef->NumberValue(isolate->GetCurrentContext()).FromJust());
    } else if(valueRef->IsString()) {
        return JNIV8Marshalling::v8string2jstring(valueRef.As<v8::String>());
    } else if(valueRef->IsBoolean()) {
        return env->CallStaticObjectMethod(_jniBoolean.clazz, _jniBoolean.valueOfId, valueRef->BooleanValue(isolate));
    } else if(valueRef->IsUndefined()) {
        return env->NewLocalRef(_undefined);
    } else if(valueRef->IsSymbol()) {
        v8::Local<v8::SymbolObject> symbolObjRef = v8::SymbolObject::New(isolate, valueRef.As<v8::Symbol>()).As<v8::SymbolObject>();
        return JNIV8Wrapper::wrapObject<JNIV8Symbol>(symbolObjRef)->getJObject();
    } else {
        JNI_ASSERT(0, "Encountered unexpected v8 type");
    }
    return nullptr;
}

/**
 * convert an instance of Object to a v8value
 */
v8::Local<v8::Value> JNIV8Marshalling::jobject2v8value(jobject object) {
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    // because this method returns a local, we can assume that the correct v8 scopes are active around it already
    // we still need a handle scope however...
    v8::EscapableHandleScope scope(isolate);

    v8::Local<v8::Value> resultRef;

    JNIEnv *env = JNIWrapper::getEnvironment();

    // jobject referencing "null" can actually be non-null..
    if(env->IsSameObject(object, nullptr) || !object) {
        resultRef = v8::Null(isolate);
    } else if(env->IsInstanceOf(object, _jniString.clazz)) {
        resultRef = JNIV8Marshalling::jstring2v8string((jstring)object);
    } else if(env->IsInstanceOf(object, _jniCharacter.clazz)) {
        jchar c = env->CallCharMethod(object, _jniCharacter.charValueId);
        v8::MaybeLocal<v8::String> maybeLocal = v8::String::NewFromTwoByte(isolate, &c, v8::NewStringType::kNormal, 1);
        if(!maybeLocal.IsEmpty()) {
            resultRef = maybeLocal.ToLocalChecked();
        }
    } else if(env->IsInstanceOf(object, _jniNumber.clazz)) {
        jdouble n = env->CallDoubleMethod(object, _jniNumber.doubleValueId);
        resultRef = v8::Number::New(isolate, n);
    } else if(env->IsInstanceOf(object, _jniBoolean.clazz)) {
        jboolean b = env->CallBooleanMethod(object, _jniBoolean.booleanValueId);
        resultRef = v8::Boolean::New(isolate, b);
    } else if(env->IsInstanceOf(object, _jniV8Object.clazz)) {
        resultRef = JNIV8Wrapper::wrapObject<JNIV8Object>(object)->getJSObject();
        // unwrap symbols
        if(resultRef->IsSymbolObject()) {
            resultRef = resultRef.As<v8::SymbolObject>()->ValueOf();
        }
    }
    if(resultRef.IsEmpty()) {
        resultRef = v8::Undefined(isolate);
    }
    return scope.Escape(resultRef);
}

/**
 * return an object representing undefined in java
 */
jobject JNIV8Marshalling::undefinedInJava() {
    return _undefined;
}

/**
 * convert a v8::String to a std::string
 */
std::string JNIV8Marshalling::v8string2string(v8::Local<v8::Value> value) {
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    return (value.IsEmpty() ? std::string("") : std::string(*v8::String::Utf8Value(isolate, value->ToString(isolate))));
}