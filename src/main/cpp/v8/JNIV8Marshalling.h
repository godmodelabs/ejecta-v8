//
// Created by Martin Kleinhans on 30.11.17.
//

#ifndef ANDROID_GUIDANTS_JNIV8CONVERSION_H
#define ANDROID_GUIDANTS_JNIV8CONVERSION_H

#include <v8.h>
#include <jni.h>
#include <string>

/**
 * structs for describing java method signatures and arguments
 * used by utility routines on the JNIV8Marshalling class
 */
enum class JNIV8MarshallingError {
    kOk,
    kNotNullable,
    kUndefined,
    kWrongType,
    kNoNaN,
    kVoidNotNull,
    kOutOfRange
};

enum class JNIV8JavaValueType {
    kObject,
    kBoolean,
    kByte,
    kCharacter,
    kShort,
    kInteger,
    kLong,
    kFloat,
    kDouble,
    kString,
    kVoid
};

struct JNIV8JavaValue {
    std::string type;
    bool isNullable, undefinedIsNull;
    jclass clazz;
    JNIV8JavaValueType valueType;

    JNIV8JavaValue(const std::string& type, bool isNullable = false, bool undefinedIsNull = false);
};

struct JNIV8JavaArgument : JNIV8JavaValue {
    JNIV8JavaArgument(const std::string& type, bool isNullable = false, bool undefinedIsNull = false);
};

struct JNIV8ObjectJavaSignatureInfo {
    jmethodID javaMethodId;
    std::vector<JNIV8JavaArgument>* arguments;
};

class JNIV8Marshalling {
public:
    /**
     * converts a v8 value to a java value based on the provided type information
     * if conversion was successful method will return kOk and target will contain a valid jvalue
     */
    static JNIV8MarshallingError convertV8ValueToJavaArgument(JNIEnv *env, v8::Local<v8::Value> v8Value, JNIV8JavaArgument arg, jvalue *target);

    /**
     * calls a java method with the provided arguments
     * if object is null, the method is assumed to be static
     */
    static v8::Local<v8::Value> callJavaMethod(JNIEnv *env, JNIV8JavaValue returnType, jclass clazz, jmethodID methodId, jobject object, jvalue *args);

    /**
     * convert a v8 value to an instance of Object
     */
    static jobject v8value2jobject(v8::Local<v8::Value> valueRef);

    /**
     * convert an instance of Object to a v8value
     */
    static v8::Local<v8::Value> jobject2v8value(jobject object);

    /**
     * convert a jstring to a v8::String
     */
    static v8::Local<v8::String> jstring2v8string(jstring string);

    /**
     * convert a v8::String to a jstring
     */
    static jstring v8string2jstring(v8::Local<v8::String> string);

    /**
     * convert a v8::Value to a std::string
     */
    static std::string v8value2string(v8::Local<v8::Value> value);

    /**
     * return an object representing undefined in java
     */
    static jobject undefinedInJava();

    /**
     * cache JNI class references
     */
    static void initJNICache();
private:
    static jobject _undefined;

    static struct {
        jclass clazz;
        jmethodID valueOfId;
    } _jniByte, _jniShort, _jniInteger, _jniLong, _jniFloat, _jniDouble;
    static struct {
        jclass clazz;
        jmethodID valueOfId;
        jmethodID booleanValueId;
    } _jniBoolean;
    static struct {
        jclass clazz;
        jmethodID valueOfId;
        jmethodID charValueId;
    } _jniCharacter;
    static struct {
        jclass clazz;
        jmethodID doubleValueId;
    } _jniNumber;
    static struct {
        jclass clazz;
    } _jniV8Object, _jniObject, _jniString;
};


#endif //ANDROID_GUIDANTS_JNIV8CONVERSION_H
