//
// Created by Martin Kleinhans on 30.11.17.
//

#ifndef ANDROID_GUIDANTS_JNIV8CONVERSION_H
#define ANDROID_GUIDANTS_JNIV8CONVERSION_H

#include <v8.h>
#include <jni.h>
#include <string>
#include <unordered_map>

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
    kObject = 0,
    kBoolean = 1,
    kByte = 2,
    kCharacter = 3,
    kShort = 4,
    kInteger = 5,
    kLong = 6,
    kFloat = 7,
    kDouble = 8,
    kString = 9,
    kVoid = 10
};

enum JNIV8MarshallingFlags {
    kDefault = 0,
    /*
     * throw exception instead of returning null
     * has no effect if CoerceNull is set or requested type can not handle null (e.g. double)
     * combined with UndefinedIsNull will also make undefined throw
     */
    kNonNull = 1,
    /*
     * treat undefined as null
     * has no effect if requested type can not handle null (e.g. double)
     */
    kUndefinedIsNull = 1<<1,
    /*
     * throw exception when encountering values of different type
     * instead of coercing them
     * null is allowed if requested type can handle it (e.g. Double, but not double)!
     */
    kStrict = 1<<2,
    /*
     * coerce null even if requested type can handle it (e.g. Double)
     */
    kCoerceNull = 1<<3,
    /*
     * discard result value; can only be used with type Void
     */
    kDiscard = 1<<4
};

struct JNIV8JavaValue {
    JNIV8MarshallingFlags flags;
    jclass clazz;
    JNIV8JavaValueType valueType;

    JNIV8JavaValue(JNIV8JavaValueType type, jclass clazz, JNIV8MarshallingFlags flags = JNIV8MarshallingFlags::kDefault);
};

struct JNIV8ObjectJavaSignatureInfo {
    jmethodID javaMethodId;
    std::vector<JNIV8JavaValue>* arguments;
};

class JNIV8Marshalling {
public:
    /**
     * create a JNIV8JavaValue struct for the specified type
     */
    static JNIV8JavaValue valueWithType(JNIV8JavaValueType type, bool boxed = false, JNIV8MarshallingFlags flags = JNIV8MarshallingFlags::kDefault);
    static JNIV8JavaValue valueWithClass(jint type, jclass clazz, JNIV8MarshallingFlags flags = JNIV8MarshallingFlags::kDefault);

    /**
     * create a JNIV8JavaValue struct for the specified canonical name
     *
     * WARNING: returned struct will hold a global ref for custom classes
     * Releasing this ref is the responsibility of the caller.
     * for classes inside of java.lang the ref will be local and must not be released!
     */
    static JNIV8JavaValue persistentValueWithTypeSignature(const std::string &type, JNIV8MarshallingFlags flags = JNIV8MarshallingFlags::kDefault);
    static JNIV8JavaValue persistentArgumentWithTypeSignature(const std::string &type, JNIV8MarshallingFlags flags = JNIV8MarshallingFlags::kDefault);

    /**
     * converts a v8 value to a java value based on the provided type information
     * if conversion was successful method will return kOk and target will contain a valid jvalue
     */
    static JNIV8MarshallingError convertV8ValueToJavaValue(JNIEnv *env, v8::Local<v8::Value> v8Value, JNIV8JavaValue arg, jvalue *target);

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
    static std::string v8string2string(v8::Local<v8::Value> value);

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
    static std::unordered_map<int, JNIV8JavaValueType> _typeMap;

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
    } _jniObject, _jniV8Object, _jniString, _jniVoid;
};


#endif //ANDROID_GUIDANTS_JNIV8CONVERSION_H
