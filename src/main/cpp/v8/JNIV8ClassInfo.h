//
// Created by Martin Kleinhans on 18.08.17.
//

#ifndef TRADINGLIB_SAMPLE_V8CLASSINFO_H
#define TRADINGLIB_SAMPLE_V8CLASSINFO_H

#include "../bgjs/BGJSV8Engine.h"
#include "JNIV8Marshalling.h"

class JNIV8ClassInfo;
class JNIV8ClassInfoContainer;
class JNIV8Object;

// these declarations use JNIV8Object, but the type is valid for methods on all subclasses, see:
// http://www.open-std.org/jtc1/sc22/WG21/docs/wp/html/nov97-2/expr.html#expr.static.cast
// there is no implicit conversion however, so methods of derived classes have to be casted manually
typedef void(JNIV8Object::*JNIV8ObjectConstructorCallback)(const v8::FunctionCallbackInfo<v8::Value>& args);
typedef void(JNIV8Object::*JNIV8ObjectMethodCallback)(const std::string &methodName, const v8::FunctionCallbackInfo<v8::Value>& args);
typedef void(*JNIV8ObjectStaticMethodCallback)(const std::string &methodName, const v8::FunctionCallbackInfo<v8::Value>& args);
typedef void(JNIV8Object::*JNIV8ObjectAccessorGetterCallback)(const std::string &propertyName, const v8::PropertyCallbackInfo<v8::Value> &info);
typedef void(JNIV8Object::*JNIV8ObjectAccessorSetterCallback)(const std::string &propertyName, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void> &info);
typedef void(*JNIV8ObjectStaticAccessorGetterCallback)(const std::string &propertyName, const v8::PropertyCallbackInfo<v8::Value> &info);
typedef void(*JNIV8ObjectStaticAccessorSetterCallback)(const std::string &propertyName, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void> &info);

/**
 * internal struct for storing information for property accessor bound to java methods
 */
struct JNIV8ObjectJavaAccessorHolder {
    std::string propertyName;
    jmethodID javaGetterId;
    jmethodID javaSetterId;
    jclass javaClass;
    JNIV8JavaArgument propertyType;
    bool isStatic;

    JNIV8ObjectJavaAccessorHolder(JNIV8JavaArgument propertyType) : propertyType(propertyType) {};
};

/**
 * internal struct for storing information for functions bound to java methods
 */
struct JNIV8ObjectJavaCallbackHolder {
    std::string methodName;
    JNIV8JavaValue returnType;
    std::vector<JNIV8ObjectJavaSignatureInfo> signatures;
    jclass javaClass;
    bool isStatic;

    JNIV8ObjectJavaCallbackHolder(JNIV8JavaValue returnType) : returnType(returnType) {};
};

/**
 * internal struct for storing member function pointers for property accessors
 */
struct JNIV8ObjectAccessorHolder {
    union {
        JNIV8ObjectAccessorGetterCallback i;
        JNIV8ObjectStaticAccessorGetterCallback s;
    } getterCallback;
    union {
        JNIV8ObjectAccessorSetterCallback i;
        JNIV8ObjectStaticAccessorSetterCallback s;
    } setterCallback;
    std::string propertyName;
    bool isStatic;
};

/**
 * internal struct for storing member function pointers for methods
 */
struct JNIV8ObjectCallbackHolder {
    union {
        JNIV8ObjectMethodCallback i;
        JNIV8ObjectStaticMethodCallback s;
    } callback;
    bool isStatic;
    std::string methodName;
};

enum class JNIV8ObjectType {
    /**
     * Java + native + v8 class are permanently linked together
     * native class and v8 class can store persistent state, all three have the same lifetime
     * wrapping the same java object or v8 object will always yield the exact same native instance!
     */
    kPersistent,
    /**
     * like kPersistent, but these objects can not be constructed
     */
    kAbstract,
    /**
     * Tuple of Java+native class acts as a wrapper for an existing v8 object
     * this can be used to write utility methods for working with existing JavaScript classes, e.g. you could wrap Object, or Array
     * wrapping the same javascript object will always yield a new instance of a Java + native tuple!
     */
    kWrapper
};

enum class JNIV8ClassCreationPolicy {
    DEFAULT = 0,
    NATIVE_ONLY = 1
};

struct JNIV8ClassInfo {
    friend class JNIV8Object;
    friend class JNIV8Wrapper;
public:
    BGJSV8Engine* getEngine() const {
        return engine;
    };

    /**
     * objects can be configured so that they can not be created from java script
     * even if the prototype is exported
     * invoking the constructor with new will throw an exception
     */
    void setCreationPolicy(JNIV8ClassCreationPolicy policy);

    /**
     * if multiple classes in the objects hierarchy registered a constructor, it is only called for the final class
     * implementors have to manually call methods baseclasses if needed
     */
    void registerConstructor(JNIV8ObjectConstructorCallback callback);

    void registerMethod(const std::string& methodName, JNIV8ObjectMethodCallback callback);
    void registerStaticMethod(const std::string& methodName, JNIV8ObjectStaticMethodCallback callback);
    void registerAccessor(const std::string& propertyName, JNIV8ObjectAccessorGetterCallback getter, JNIV8ObjectAccessorSetterCallback setter = 0);
    void registerStaticAccessor(const std::string& propertyName, JNIV8ObjectStaticAccessorGetterCallback getter, JNIV8ObjectStaticAccessorSetterCallback setter = 0);

    v8::Local<v8::Object> newInstance() const;
    v8::Local<v8::Function> getConstructor() const;

    /**
     * cache JNI class references
     */
    static void initJNICache();
private:
    JNIV8ClassInfo(JNIV8ClassInfoContainer *container, BGJSV8Engine *engine);
    ~JNIV8ClassInfo();

    void registerJavaMethod(const std::string& methodName, jmethodID methodId, const JNIV8JavaValue& returnType, std::vector<JNIV8JavaArgument> *arguments);
    void registerStaticJavaMethod(const std::string& methodName, jmethodID methodId, const JNIV8JavaValue& returnType, std::vector<JNIV8JavaArgument> *arguments);
    void registerJavaAccessor(const std::string& propertyName, const JNIV8JavaArgument& propertyType, jmethodID getterId, jmethodID setterId);
    void registerStaticJavaAccessor(const std::string& propertyName, const JNIV8JavaArgument& propertyType, jmethodID getterId, jmethodID setterId);

    void _registerJavaMethod(JNIV8ObjectJavaCallbackHolder *holder);
    void _registerJavaAccessor(JNIV8ObjectJavaAccessorHolder *holder);
    void _registerMethod(JNIV8ObjectCallbackHolder *holder);
    void _registerAccessor(JNIV8ObjectAccessorHolder *holder);

    std::vector<JNIV8ObjectJavaCallbackHolder*> javaCallbackHolders;
    std::vector<JNIV8ObjectJavaAccessorHolder*> javaAccessorHolders;
    std::vector<JNIV8ObjectCallbackHolder*> callbackHolders;
    std::vector<JNIV8ObjectAccessorHolder*> accessorHolders;

    BGJSV8Engine *engine;
    JNIV8ClassInfoContainer *container;
    v8::Persistent<v8::FunctionTemplate> functionTemplate;
    JNIV8ObjectConstructorCallback constructorCallback;
    bool createFromJavaOnly;

    static struct {
        jclass clazz;
    } _jniObject;

    static void v8JavaAccessorGetterCallback(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value> &info);
    static void v8JavaAccessorSetterCallback(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void> &info);
    static void v8JavaMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void v8AccessorGetterCallback(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value> &info);
    static void v8AccessorSetterCallback(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void> &info);
    static void v8MethodCallback(const v8::FunctionCallbackInfo<v8::Value>& args);

};

// internal helper methods for creating and initializing objects
typedef void(*JNIV8ObjectInitializer)(JNIV8ClassInfo *info);
typedef std::shared_ptr<JNIV8Object>(*JNIV8ObjectCreator)(JNIV8ClassInfo *info, v8::Persistent<v8::Object> *jsObj, jobjectArray arguments);

/**
 * internal container object for managing all class info instances (one for each v8 engine) of an object
 */
struct JNIV8ClassInfoContainer {
    friend class JNIV8Object;
    friend class JNIV8Wrapper;
    friend class JNIV8ClassInfo;
private:
    JNIV8ClassInfoContainer(JNIV8ObjectType type, const std::string& canonicalName, JNIV8ObjectInitializer i, JNIV8ObjectCreator c, size_t size, JNIV8ClassInfoContainer *baseClassInfo);

    JNIV8ObjectType type;
    JNIV8ClassInfoContainer *baseClassInfo;
    size_t size;
    std::string canonicalName;
    JNIV8ObjectInitializer initializer;
    JNIV8ObjectCreator creator;
    std::vector<JNIV8ClassInfo*> classInfos;

    jclass clsObject, clsBinding;
};

#endif //TRADINGLIB_SAMPLE_V8CLASSINFO_H
