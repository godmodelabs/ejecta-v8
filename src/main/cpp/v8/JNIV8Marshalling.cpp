//
// Created by Martin Kleinhans on 30.11.17.
//

#include "JNIV8Marshalling.h"
#include <cmath>

#include "../jni/JNIWrapper.h"
#include "JNIV8Wrapper.h"

#include "JNIV8Function.h"
#include "JNIV8Array.h"
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

JNIV8JavaValue::JNIV8JavaValue(const std::string& type, bool isNullable, bool undefinedIsNull) :
        type(type), isNullable(isNullable), undefinedIsNull(undefinedIsNull) {
    valueType = getArgumentType(type);
    if(type.length() > 1) {
        JNIEnv *env = JNIWrapper::getEnvironment();
        clazz = (jclass)env->NewGlobalRef(env->FindClass(type.substr(1, type.length() - 2).c_str()));
    } else {
        clazz = nullptr;
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
    JNI_ASSERT(!isNullable || type.length() > 1, "primitive types must not be nullable")
    JNI_ASSERT(valueType != JNIV8JavaValueType::kVoid || (!clazz || isNullable), "Void property must nullable");
#endif
}

JNIV8JavaArgument::JNIV8JavaArgument(const std::string& type, bool isNullable, bool undefinedIsNull) :
        JNIV8JavaValue(type, isNullable, undefinedIsNull) {
    JNI_ASSERT(type[0] != 'V', "'void' is not a valid argument type");
}

#define InitBoxedType(type, mnemonic)\
_jni##type.clazz = (jclass)env->NewGlobalRef(env->FindClass("java/lang/"#type));\
_jni##type.valueOfId = env->GetStaticMethodID(_jni##type.clazz, "valueOf","("#mnemonic")Ljava/lang/"#type";");

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
jobject JNIV8Marshalling::_undefined = nullptr;

/**
 * cache JNI class references
 */
void JNIV8Marshalling::initJNICache() {
    JNIEnv *env = JNIWrapper::getEnvironment();

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
    _jniObject.clazz = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Object"));
    _jniString.clazz = (jclass)env->NewGlobalRef(env->FindClass("java/lang/String"));
}

/**
 * converts a v8 value to a java value based on the provided type information
 * if conversion was successful method will return kOk and target will contain a valid jvalue
 */
JNIV8MarshallingError JNIV8Marshalling::convertV8ValueToJavaArgument(JNIEnv *env, v8::Local<v8::Value> v8Value, JNIV8JavaArgument arg, jvalue *target) {
    double numberValue;
    // non-primitive types can handle null; and undefined if it is configured to be mapped to null!
    if(arg.clazz && (v8Value->IsNull() || (v8Value->IsUndefined() && arg.undefinedIsNull))) {
        target->l = nullptr;
        if (!arg.isNullable) {
            return JNIV8MarshallingError::kNotNullable;
        }
    } else {
        switch (arg.valueType) {
            case JNIV8JavaValueType::kObject:
                target->l = JNIV8Marshalling::v8value2jobject(v8Value);
                // validation: nullptr should only occur if something is wrong
                if (!target->l) {
                    // check nullability
                    if (!arg.isNullable) {
                        return JNIV8MarshallingError::kNotNullable;
                    }
                } else if (!env->IsInstanceOf(target->l, arg.clazz)) {
                    // validate objects type
                    if (v8Value->IsUndefined()) {
                        return JNIV8MarshallingError::kUndefined;
                    }
                    return JNIV8MarshallingError::kWrongType;
                }
                break;
            case JNIV8JavaValueType::kBoolean: {
                jboolean value = (jboolean) v8Value->ToBoolean()->BooleanValue();
                AutoboxArgument(Boolean, z);
                break;
            }
            case JNIV8JavaValueType::kByte: {
                numberValue = v8Value->NumberValue();
                if (std::isnan(numberValue)) return JNIV8MarshallingError::kNoNaN;
                jbyte value = (jbyte) numberValue;
                if (value != numberValue) return JNIV8MarshallingError::kOutOfRange;
                AutoboxArgument(Byte, b);
                break;
            }
            case JNIV8JavaValueType::kCharacter: {
                jchar value = (jchar) JNIV8Marshalling::v8string2string(v8Value->ToString())[0];
                AutoboxArgument(Character, c);
                break;
            }
            case JNIV8JavaValueType::kShort: {
                numberValue = v8Value->NumberValue();
                if (std::isnan(numberValue)) return JNIV8MarshallingError::kNoNaN;
                jshort value = (jshort) numberValue;
                if (value != numberValue) return JNIV8MarshallingError::kOutOfRange;
                AutoboxArgument(Short, s);
                break;
            }
            case JNIV8JavaValueType::kInteger: {
                numberValue = v8Value->NumberValue();
                if (std::isnan(numberValue)) return JNIV8MarshallingError::kNoNaN;
                jint value = (jint) numberValue;
                if (value != numberValue) return JNIV8MarshallingError::kOutOfRange;
                AutoboxArgument(Integer, i);
                break;
            }
            case JNIV8JavaValueType::kLong: {
                numberValue = v8Value->NumberValue();
                if (std::isnan(numberValue)) return JNIV8MarshallingError::kNoNaN;
                jlong value = (jlong) numberValue;
                if (value != numberValue) return JNIV8MarshallingError::kOutOfRange;
                AutoboxArgument(Long, j);
                break;
            }
            case JNIV8JavaValueType::kFloat: {
                numberValue = v8Value->NumberValue();
                jfloat value = (jfloat) numberValue;
                if (value != numberValue) return JNIV8MarshallingError::kOutOfRange;
                AutoboxArgument(Float, f);
                break;
            }
            case JNIV8JavaValueType::kDouble: {
                numberValue = v8Value->NumberValue();
                jdouble value = (jdouble) numberValue;
                AutoboxArgument(Double, d);
                break;
            }
            case JNIV8JavaValueType::kString: {
                target->l = JNIV8Marshalling::v8value2jobject(v8Value->ToString());
                break;
            }
            case JNIV8JavaValueType::kVoid: {
                if (!v8Value->IsNull()) {
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
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::EscapableHandleScope handleScope(isolate);
    v8::Local<v8::Value> result;

    switch(returnType.valueType) {
        case JNIV8JavaValueType::kString:
        case JNIV8JavaValueType::kObject: {
            if (object) {
                result = JNIV8Marshalling::jobject2v8value(
                        env->CallObjectMethodA(object, methodId, args));
            } else {
                result = JNIV8Marshalling::jobject2v8value(
                        env->CallStaticObjectMethodA(clazz, methodId, args));
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
    if(env->IsSameObject(string, NULL)) {
        return scope.Escape(v8::Null(isolate)->ToString());
    }

    len = env->GetStringLength(string);

    if(len > 0) {
        const jchar *chars = env->GetStringChars(string, nullptr);
        maybeLocal = v8::String::NewFromTwoByte(isolate, chars, v8::NewStringType::kNormal, len);
        env->ReleaseStringChars(string, chars);
    }

    // if string is empty or if conversion failed we return an empty string
    if(!len || maybeLocal.IsEmpty()) {
        maybeLocal = v8::String::NewFromOneByte(isolate, (uint8_t*)"");
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
    const jchar *chars = *v8::String::Value(string); // returns NULL if not a string
    JNIEnv *env = JNIWrapper::getEnvironment();
    return env->NewString(chars, string->Length()); // returns "" when called with NULL,0
}

/**
 * convert an instance of V8Value to a jobject
 */
jobject JNIV8Marshalling::v8value2jobject(v8::Local<v8::Value> valueRef) {
    JNIEnv *env = JNIWrapper::getEnvironment();

    if(valueRef->IsNull()) {
        return nullptr;
    } else if(valueRef->IsObject()) {
        if (valueRef->IsFunction()) {
            return JNIV8Wrapper::wrapObject<JNIV8Function>(valueRef->ToObject())->getJObject();
        } else if (valueRef->IsArray()) {
            return JNIV8Wrapper::wrapObject<JNIV8Array>(valueRef->ToObject())->getJObject();
        }
        auto ptr = JNIV8Wrapper::wrapObject<JNIV8Object>(valueRef->ToObject());
        if (ptr) {
            return ptr->getJObject();
        } else {
            return JNIV8Wrapper::wrapObject<JNIV8GenericObject>(valueRef->ToObject())->getJObject();
        }
    } else if(valueRef->IsNumber()) {
        return env->CallStaticObjectMethod(_jniDouble.clazz, _jniDouble.valueOfId, valueRef->NumberValue());
    } else if(valueRef->IsString()) {
        return JNIV8Marshalling::v8string2jstring(valueRef.As<v8::String>());
    } else if(valueRef->IsBoolean()) {
        return env->CallStaticObjectMethod(_jniBoolean.clazz, _jniBoolean.valueOfId, valueRef->BooleanValue());
    } else if(valueRef->IsUndefined()) {
        return _undefined;
    } else if(valueRef->IsSymbol()) {
        JNI_ASSERT(0, "Symbols are not supported");
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
    if(env->IsSameObject(object, NULL) || !object) {
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
    return (value.IsEmpty() ? std::string("") : std::string(*v8::String::Utf8Value(value->ToString())));
}